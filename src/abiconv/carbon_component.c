/*
 * carbon_component.c — a generic reimplementation of the classic Mac OS
 * Component Manager (Open[A]DefaultComponent / OpenComponent / CloseComponent /
 * FindNextComponent / CountComponents / GetComponentInfo / ...).
 *
 * WHY: the Component Manager was GUTTED from modern macOS along with QuickTime.
 * Its symbols still live in CoreServices/CarbonCore's .tbd, so abigen generated
 * ABI shims that call the dead native entry points -> null deref (iPhoto's
 * OpenADefaultComponent('grip','JPEG',&ci) to open the JPEG GraphicsImporter).
 *
 * This is a real, extensible registry + dispatch, NOT a one-call patch:
 *   - backends register a (type, subType, manufacturer) descriptor + open/close
 *     hooks via cm_register_backend (subType/manuf 0 = wildcard);
 *   - Open* finds a matching backend, allocates a ComponentInstance (a real
 *     low-4GB pointer handed straight back to the i386 caller — the libabiconv
 *     heap is < 4GB, see malloc_shim.c), and runs the backend's open hook;
 *   - the "component methods" (e.g. GraphicsImportDraw) are plain shims that
 *     recover their per-instance storage with cm_inst_from_i386 + cm_inst_storage
 *     and operate on it directly, so we never need the arcane ComponentCallNow
 *     selector-dispatch ABI.
 *
 * UNIVERSAL: any i386 app using the Component Manager (QuickTime importers/
 * exporters/codecs, the Sound Manager, etc.) is served; the still-image
 * GraphicsImporter backend is registered by quicktime_image.c.
 *
 * Reached via the ___OpenADefaultComponent / ___CloseComponent / ... trampolines
 * in maptable_tramp.asm; the CoreServices originals are excluded from abigen via
 * custom.syms so these overrides win.
 */

#include "carbon_shim.h"
#include <string.h>
#include <os/lock.h>

/* coreaudio_au_shim.c: AudioUnit component types ('au..') -> AudioComponent. */
extern int au_find_next(uint32_t prev, const uint32_t *cd, uint32_t *out);
extern int au_open(uint32_t comp, uint32_t *inst);
extern int au_close(uint32_t inst, uint32_t *result);
static uint32_t au_open_default(uint32_t type, uint32_t sub, int *handled)
{
   const uint32_t cd[5] = { type, sub, 0, 0, 0 };
   uint32_t comp = 0, inst = 0;
   *handled = au_find_next(0, cd, &comp);
   if (*handled && comp) { au_open(comp, &inst); }
   return inst;
}

extern void *malloc(size_t);
extern void  free(void *);

/* ---- registry ----------------------------------------------------------- */

#define CM_MAX_COMPONENTS 64

typedef struct cm_component {
   int          in_use;
   cm_ostype    type, subtype, manuf;
   cm_open_fn   open;
   cm_close_fn  close;
   const char  *name;
} cm_component;

/* A live ComponentInstance. The pointer to this struct IS the i386 handle. */
#define CM_INST_MAGIC 0x434d4953u       /* 'CMIS' */
struct cm_instance {
   uint32_t            magic;
   cm_component       *comp;
   cm_ostype           open_subtype;    /* subtype actually requested at open */
   void               *storage;         /* backend per-instance state */
   struct cm_instance *next_live;
};

static cm_component   g_components[CM_MAX_COMPONENTS];
static int            g_ncomponents;
static struct cm_instance *g_live;       /* singly-linked live-instance set */
static os_unfair_lock g_lock = OS_UNFAIR_LOCK_INIT;

static cm_component *find_first(cm_ostype t, cm_ostype s, cm_ostype m);

/* Symmetric wildcard match: a 0 on EITHER side means "any". */
static int field_match(cm_ostype want, cm_ostype have)
{
   return want == 0 || have == 0 || want == have;
}
static int comp_matches(const cm_component *c,
                        cm_ostype type, cm_ostype subtype, cm_ostype manuf)
{
   return c->in_use && field_match(type, c->type) &&
          field_match(subtype, c->subtype) && field_match(manuf, c->manuf);
}

void cm_register_backend(cm_ostype type, cm_ostype subtype, cm_ostype manuf,
                         cm_open_fn open, cm_close_fn close, const char *name)
{
   os_unfair_lock_lock(&g_lock);
   if (g_ncomponents < CM_MAX_COMPONENTS) {
      cm_component *c = &g_components[g_ncomponents++];
      c->in_use  = 1;
      c->type    = type;
      c->subtype = subtype;
      c->manuf   = manuf;
      c->open    = open;
      c->close   = close;
      c->name    = name;
   }
   os_unfair_lock_unlock(&g_lock);
}

/* ---- instance lifecycle ------------------------------------------------- */

static uint32_t open_component(cm_component *c, cm_ostype subtype)
{
   struct cm_instance *inst = (struct cm_instance *)malloc(sizeof(*inst));
   if (!inst)
      return 0;
   inst->magic        = CM_INST_MAGIC;
   inst->comp         = c;
   inst->open_subtype = subtype;
   inst->storage      = NULL;

   if (c->open) {
      cm_result r = c->open(inst);
      if (r != cmNoErr) {
         free(inst);
         return 0;
      }
   }

   os_unfair_lock_lock(&g_lock);
   inst->next_live = g_live;
   g_live = inst;
   os_unfair_lock_unlock(&g_lock);
   return to_i386(inst);
}

uint32_t cm_open_default(cm_ostype type, cm_ostype subtype)
{
   cm_component *c = find_first(type, subtype, 0);
   return c ? open_component(c, subtype) : 0;
}

cm_instance *cm_inst_from_i386(uint32_t h)
{
   if (!h)
      return NULL;
   /* Validate against the live set so a stale/garbage handle never gets
    * dereferenced (we only ever touch instances we created). */
   struct cm_instance *want = (struct cm_instance *)i386_ptr(h);
   struct cm_instance *found = NULL;
   os_unfair_lock_lock(&g_lock);
   for (struct cm_instance *p = g_live; p; p = p->next_live)
      if (p == want && p->magic == CM_INST_MAGIC) { found = p; break; }
   os_unfair_lock_unlock(&g_lock);
   return found;
}

void     *cm_inst_storage(cm_instance *inst)            { return inst->storage; }
void      cm_inst_set_storage(cm_instance *inst, void *s) { inst->storage = s; }
cm_ostype cm_inst_subtype(cm_instance *inst)            { return inst->open_subtype; }

static uint32_t close_instance(uint32_t h)
{
   struct cm_instance *inst = cm_inst_from_i386(h);
   if (!inst)
      return cmNoErr;                    /* tolerate double/!ours close */
   os_unfair_lock_lock(&g_lock);
   for (struct cm_instance **pp = &g_live; *pp; pp = &(*pp)->next_live)
      if (*pp == inst) { *pp = inst->next_live; break; }
   os_unfair_lock_unlock(&g_lock);

   if (inst->comp && inst->comp->close)
      inst->comp->close(inst);
   inst->magic = 0;
   free(inst);
   return cmNoErr;
}

/* ---- i386-cdecl entry points -------------------------------------------- */
/* The Component handed to i386 is the (low-4GB) address of a g_components[]
 * entry; that round-trips through the 4-byte slot and back to a real pointer. */

static cm_component *find_first(cm_ostype t, cm_ostype s, cm_ostype m)
{
   for (int i = 0; i < g_ncomponents; i++)
      if (comp_matches(&g_components[i], t, s, m))
         return &g_components[i];
   return NULL;
}

/* ComponentInstance OpenDefaultComponent(OSType type, OSType subType); */
uint32_t shim_OpenDefaultComponent(uint32_t *a)
{
   int au; uint32_t ai = au_open_default(a[0], a[1], &au);
   if (au) { return ai; }
   cm_component *c = find_first(a[0], a[1], 0);
   return c ? open_component(c, a[1]) : 0;
}

/* OSErr OpenADefaultComponent(OSType type, OSType subType, ComponentInstance *ci); */
uint32_t shim_OpenADefaultComponent(uint32_t *a)
{
   int au; uint32_t ai = au_open_default(a[0], a[1], &au);
   if (au) { put_u32(a[2], ai); return ai ? cmNoErr : cmCantOpenErr; }
   cm_component *c = find_first(a[0], a[1], 0);
   uint32_t inst = c ? open_component(c, a[1]) : 0;
   put_u32(a[2], inst);
   return inst ? cmNoErr : cmCantOpenErr;
}

/* ComponentInstance OpenComponent(Component aComponent); */
uint32_t shim_OpenComponent(uint32_t *a)
{
   uint32_t ai;
   if (au_open(a[0], &ai)) { return ai; }
   cm_component *c = (cm_component *)i386_ptr(a[0]);
   return c ? open_component(c, c->subtype) : 0;
}

/* OSErr OpenAComponent(Component aComponent, ComponentInstance *ci); */
uint32_t shim_OpenAComponent(uint32_t *a)
{
   uint32_t ai;
   if (au_open(a[0], &ai)) { put_u32(a[1], ai); return ai ? cmNoErr : cmCantOpenErr; }
   cm_component *c = (cm_component *)i386_ptr(a[0]);
   uint32_t inst = c ? open_component(c, c->subtype) : 0;
   put_u32(a[1], inst);
   return inst ? cmNoErr : cmCantOpenErr;
}

/* ComponentResult CloseComponent(ComponentInstance ci); */
uint32_t shim_CloseComponent(uint32_t *a)
{
   uint32_t r;
   if (au_close(a[0], &r)) { return r; }
   return close_instance(a[0]);
}

/* Component FindNextComponent(Component prev, ComponentDescription *looking);
 * ComponentDescription (i386) = 5 x UInt32: type,subType,manuf,flags,flagsMask. */
uint32_t shim_FindNextComponent(uint32_t *a)
{
   uint32_t prev = a[0];
   uint32_t *cd  = (uint32_t *)i386_ptr(a[1]);
   uint32_t au;
   if (au_find_next(prev, cd, &au)) { return au; }
   cm_ostype t = cd ? cd[0] : 0, s = cd ? cd[1] : 0, m = cd ? cd[2] : 0;

   /* Resume after `prev` (a g_components[] address) if given. */
   int start = 0;
   if (prev) {
      cm_component *pc = (cm_component *)i386_ptr(prev);
      start = (int)(pc - g_components) + 1;
      if (start < 0 || start > g_ncomponents) start = g_ncomponents;
   }
   for (int i = start; i < g_ncomponents; i++)
      if (comp_matches(&g_components[i], t, s, m))
         return to_i386(&g_components[i]);
   return 0;
}

/* long CountComponents(ComponentDescription *looking); */
uint32_t shim_CountComponents(uint32_t *a)
{
   uint32_t *cd = (uint32_t *)i386_ptr(a[0]);
   cm_ostype t = cd ? cd[0] : 0, s = cd ? cd[1] : 0, m = cd ? cd[2] : 0;
   uint32_t n = 0;
   for (int i = 0; i < g_ncomponents; i++)
      if (comp_matches(&g_components[i], t, s, m))
         n++;
   return n;
}

/* ComponentResult GetComponentInfo(Component c, ComponentDescription *cd,
 *                                  Handle name, Handle info, Handle icon); */
uint32_t shim_GetComponentInfo(uint32_t *a)
{
   cm_component *c = (cm_component *)i386_ptr(a[0]);
   uint32_t *cd = (uint32_t *)i386_ptr(a[1]);
   if (!c)
      return cmParamErr;
   if (cd) {
      cd[0] = c->type;  cd[1] = c->subtype; cd[2] = c->manuf;
      cd[3] = 0;        cd[4] = 0;
   }
   /* name handle (a[2]): fill with a Pascal string if requested. */
   if (a[2] && c->name) {
      size_t n = strlen(c->name);
      if (n > 255) n = 255;
      void *blk = cm_handle_block(a[2]);
      if (blk && cm_handle_size(a[2]) >= n + 1) {
         unsigned char *p = (unsigned char *)blk;
         p[0] = (unsigned char)n;
         memcpy(p + 1, c->name, n);
      }
   }
   return cmNoErr;
}

/* ---- standard-capability PRESENCE backends -----------------------------
 * Legacy QuickTime apps commonly gate startup on the EXISTENCE of standard
 * component families — e.g. Civ IV, after its Gestalt('qtim') version check,
 * does FindNextComponent for a sound decompressor ('sdec'/'.mp3'), a JPEG
 * graphics importer ('grip'/'JPEG') and a JPEG graphics exporter ('grex'), and
 * shows a "requires QuickTime" alert + exits if ANY is absent. The importer is
 * backed for real by quicktime_image.c (ImageIO); the other two families have
 * no real backend yet, so the probe returns 0 and the app quits.
 *
 * Register PRESENCE backends for those families so the existence probe (and
 * CountComponents) reports the standard QuickTime capability set as available.
 * These carry no open/close hook: open_component() then hands back a bare
 * instance, which is correct for the probe-only path (the app tests the
 * FindNextComponent result and never operates on it here). If an app later
 * actually OPENS one to decode/encode media, that call is a separate concern —
 * a real ImageIO exporter / AudioToolbox decoder would slot in via
 * cm_register_backend exactly like the 'grip' importer, replacing this stub.
 *
 * Universal: triggers on the standard component TYPE (a QuickTime capability
 * the modern OS no longer registers), never on an app name. */
__attribute__((constructor))
static void cm_register_presence_backends(void)
{
   cm_register_backend(kSoundDecompressorType, 0, 0, NULL, NULL,
                       "QuickTime sound decompressor (presence)");
   cm_register_backend(kGraphicsExporterType, 0, 0, NULL, NULL,
                       "QuickTime graphics exporter (presence)");
}

/* Component ResolveComponentAlias(Component c); — no aliasing, identity. */
uint32_t shim_ResolveComponentAlias(uint32_t *a) { return a[0]; }

/* OSErr UnregisterComponent(Component c); */
uint32_t shim_UnregisterComponent(uint32_t *a)
{
   cm_component *c = (cm_component *)i386_ptr(a[0]);
   os_unfair_lock_lock(&g_lock);
   if (c >= g_components && c < g_components + CM_MAX_COMPONENTS)
      c->in_use = 0;
   os_unfair_lock_unlock(&g_lock);
   return cmNoErr;
}
