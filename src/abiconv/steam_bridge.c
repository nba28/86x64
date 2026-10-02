// steam_bridge.c — translated i386 Steamworks game code <-> the NATIVE x86_64
// libsteam_api + steamclient (talking to the real, running Steam client).
//
// WHY: an i386 Steam game's own libsteam_api cannot reach a modern Steam: the
// steamclient.dylib it would load is 64-bit only. But Steamworks games ship a
// UNIVERSAL libsteam_api (Portal 2: i386 + x86_64 + arm64), and its x86_64
// slice runs fine inside our Rosetta process and connects to an arm64 Steam
// over Steam's own IPC (measured: SteamAPI_InitSafe -> your real SteamID). So
// the bridge sits between the translated GAME modules and that native slice:
//
//   C surface   the ~25 SteamAPI_* / SteamInternal_* / SteamGameServer_* entry
//               points the game imports: MTSHIM shims (steam_tramp.asm) that
//               call the native slice.
//   interfaces  every native interface pointer (high memory: an i386 register
//               cannot even hold it) is handed to the game as a low-4GB PROXY
//               { vptr, magic, native, iface }. Its vtable holds i386-callable
//               slot stubs (steam_tramp.asm: slot k -> sb_entry -> sb_dispatch);
//               sb_dispatch finds the proxy, and the generated marshaller
//               (steam_gen.inc, src/86x64/steamgen.py) calls slot k of the
//               native object's own vtable with exactly typed arguments.
//   callbacks   the game's i386 CCallbackBase objects get native stand-ins whose
//               Run(...) copies the payload low and calls the i386 object.
//
// The native slice is the thin x86_64 copy the deploy step places beside
// libabiconv as `libsteam_api.native.dylib` (knob ABICONV_STEAM_API overrides).
// Missing => loud GAP and SteamAPI_Init* returns false (the game's own "Steam
// is not running" path), never a crash.
#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc/malloc.h>

#include "gap.h"

#define KNOB(name) ({ static const char *_kv; static int _ki;                \
                      if (__builtin_expect(!_ki, 0)) { _kv = getenv(name);   \
                         __atomic_store_n(&_ki, 1, __ATOMIC_RELEASE); }      \
                      _kv; })

extern uint64_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                                 const uint32_t *words, uint64_t lowstack_top);

static int sb_trace(void) { return KNOB("ABICONV_STEAM_TRACE") != NULL; }
#define TR(...) do { if (sb_trace()) { fprintf(stderr, "[steam] " __VA_ARGS__); } } while (0)

/* ---- marshaller plumbing used by steam_gen.inc ---------------------------- */
enum { SB_VOID, SB_INT, SB_FLT, SB_SRET };
struct sb_ret {            /* layout shared with steam_tramp.asm: v@0 f@8 kind@16 */
   uint64_t v;
   double   f;
   uint32_t kind;
   uint32_t argbase;       /* index of the first param in the i386 args */
};
struct sb_method { void (*fn)(const uint32_t *, struct sb_ret *, void *); int sret; const char *name; };
struct sb_iface  { const char *version; int n; const struct sb_method *m; };

static uint64_t sb_u64(const uint32_t *p) { return (uint64_t)p[0] | ((uint64_t)p[1] << 32); }
static float    sb_f32(const uint32_t *p) { float f; memcpy(&f, p, 4); return f; }
static double   sb_f64(const uint32_t *p) { double d; memcpy(&d, p, 8); return d; }
static void     sb_gap(struct sb_ret *r, const char *name) __attribute__((unused));
static void     sb_gap(struct sb_ret *r, const char *name) {
   x64_gap_hit("steam", name, 0, 0);    /* every hit: each is a missing feature */
   r->v = 0; r->kind = SB_INT;
}
static uint64_t sb_lowstr(const char *s);
static uint64_t sb_lowptr(void *p, const char *who);
static uint64_t sb_wrap(void *native, const char *version);

#include "steam_gen.inc"

/* ---- the native slice ------------------------------------------------------ */
static void *g_lib;
static void *sym(const char *name) {
   if (!g_lib) {
      const char *path = KNOB("ABICONV_STEAM_API");
      char buf[1024];
      if (!path) {
         Dl_info di;
         if (!dladdr((void *)sym, &di) || !di.dli_fname) return NULL;
         const char *slash = strrchr(di.dli_fname, '/');
         snprintf(buf, sizeof buf, "%.*s/libsteam_api.native.dylib",
                  (int)(slash ? slash - di.dli_fname : 0), di.dli_fname);
         path = buf;
      }
      g_lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
      if (!g_lib) {
         x64_gap_hit("steam", "libsteam_api.native.dylib", dlerror(), 0);
         return NULL;
      }
      TR("native libsteam_api: %s\n", path);
   }
   void *s = dlsym(g_lib, name);
   if (!s) x64_gap_hit("steam", name, "native libsteam_api lacks it", 0);
   return s;
}
#define NATIVE(ret, name, ...) \
   static ret (*n_##name)(__VA_ARGS__); \
   if (!n_##name) n_##name = (ret (*)(__VA_ARGS__))sym(#name);

/* ---- low-4GB proxies for native interfaces -------------------------------- */
#define SB_MAGIC 0x5354424du  /* 'STBM' */
struct sb_proxy { uint32_t vptr; uint32_t magic; uint64_t native; uint32_t iface; };

extern const uint64_t sb_slot_addrs[];     /* steam_tramp.asm: i386-callable slot stubs */
extern const uint32_t sb_slot_count;

static pthread_mutex_t g_lk = PTHREAD_MUTEX_INITIALIZER;
static struct { void *native; uint32_t iface; struct sb_proxy *proxy; } g_proxies[512];
static int g_nproxies;
static uint32_t *g_vtbls[sizeof SB_IFACES / sizeof SB_IFACES[0]];

static int sb_find_iface(const char *version) {
   for (size_t i = 0; i < sizeof SB_IFACES / sizeof SB_IFACES[0]; i++)
      if (strcmp(SB_IFACES[i].version, version) == 0) return (int)i;
   return -1;
}

/* low (libabiconv malloc == the i386 heap) i386 vtable for an interface version */
static uint32_t *sb_vtbl(int iface) {
   if (!g_vtbls[iface]) {
      int n = SB_IFACES[iface].n;
      if ((uint32_t)n > sb_slot_count) {           /* steam_tramp.asm SB_SLOTS too small */
         x64_gap_hit("steam", SB_IFACES[iface].version, "more methods than slot stubs", 0);
         n = (int)sb_slot_count;
      }
      uint32_t *t = malloc(sizeof(uint32_t) * (size_t)n);
      for (int k = 0; k < n; k++) t[k] = (uint32_t)sb_slot_addrs[k];
      g_vtbls[iface] = t;
   }
   return g_vtbls[iface];
}

static uint64_t sb_wrap(void *native, const char *version) {
   if (!native) return 0;
   if (!version) { x64_gap_hit("steam", "interface without version", 0, 0); return 0; }
   int iface = sb_find_iface(version);
   if (iface < 0) {
      x64_gap_hit("steam", version, "interface version has no bridge table", 0);
      return 0;
   }
   pthread_mutex_lock(&g_lk);
   for (int i = 0; i < g_nproxies; i++) {
      if (g_proxies[i].native == native && g_proxies[i].iface == (uint32_t)iface) {
         struct sb_proxy *p = g_proxies[i].proxy;
         pthread_mutex_unlock(&g_lk);
         return (uint64_t)(uintptr_t)p;
      }
   }
   struct sb_proxy *p = NULL;
   if (g_nproxies < (int)(sizeof g_proxies / sizeof g_proxies[0])) {
      p = malloc(sizeof *p);                    /* low 4GB */
      p->vptr = (uint32_t)(uintptr_t)sb_vtbl(iface);
      p->magic = SB_MAGIC;
      p->native = (uint64_t)(uintptr_t)native;
      p->iface = (uint32_t)iface;
      g_proxies[g_nproxies].native = native;
      g_proxies[g_nproxies].iface = (uint32_t)iface;
      g_proxies[g_nproxies].proxy = p;
      g_nproxies++;
   }
   pthread_mutex_unlock(&g_lk);
   TR("wrap %s native=%p -> proxy %p\n", version, native, (void *)p);
   return (uint64_t)(uintptr_t)p;
}

static struct sb_proxy *sb_is_proxy(uint32_t a) {
   if (a < 0x1000) return NULL;
   pthread_mutex_lock(&g_lk);
   struct sb_proxy *hit = NULL;
   for (int i = 0; i < g_nproxies; i++)
      if ((uint32_t)(uintptr_t)g_proxies[i].proxy == a) { hit = g_proxies[i].proxy; break; }
   pthread_mutex_unlock(&g_lk);
   return hit;
}

/* A string the native side returned lives in steamclient's (high) memory: the
 * game gets a stable low copy, one per distinct string. */
static uint64_t sb_lowstr(const char *s) {
   if (!s) return 0;
   if ((uintptr_t)s < 0x100000000ULL) return (uint64_t)(uintptr_t)s;
   static struct { char *low; } tab[4096];
   static int n;
   pthread_mutex_lock(&g_lk);
   for (int i = 0; i < n; i++)
      if (strcmp(tab[i].low, s) == 0) { char *r = tab[i].low; pthread_mutex_unlock(&g_lk); return (uint64_t)(uintptr_t)r; }
   /* NOT strdup: libSystem's strdup allocates from the NATIVE heap (above 4GB
    * under Rosetta), and the truncated pointer is unmapped (Portal 2
    * matchmaking strncpy of GetFriendPersonaName). libabiconv malloc is the
    * low i386 heap. Kill M64_NO_STEAM_LOWSTR_HEAP; guard steam-bridge. */
   const bool old_strdup = KNOB("M64_NO_STEAM_LOWSTR_HEAP") != NULL;
   size_t len = strlen(s) + 1;
   char *low = old_strdup ? strdup(s) : malloc(len);
   if (!old_strdup) memcpy(low, s, len);
   if (n < 4096) tab[n++].low = low;
   pthread_mutex_unlock(&g_lk);
   return (uint64_t)(uintptr_t)low;
}

static uint64_t sb_lowptr(void *p, const char *who) {
   if ((uintptr_t)p < 0x100000000ULL) return (uint64_t)(uintptr_t)p;
   x64_gap_hit("steam", who, "returned a high native pointer", 0);
   return 0;
}

/* steam_tramp.asm sb_entry: args = &i386 arg slots, slot = vtable index */
void sb_dispatch(const uint32_t *args, uint32_t slot, struct sb_ret *r) {
   r->v = 0; r->f = 0; r->kind = SB_INT;
   struct sb_proxy *p = sb_is_proxy(args[0]);
   r->argbase = 1;
   if (!p) { p = sb_is_proxy(args[1]); r->argbase = 2; }   /* struct return: args[0] = sret */
   if (!p) { x64_gap_hit("steam", "call on an unknown proxy", 0, args[-1]); return; }
   const struct sb_iface *ifc = &SB_IFACES[p->iface];
   if ((int)slot >= ifc->n) { x64_gap_hit("steam", ifc->version, "slot past the table", args[-1]); return; }
   const struct sb_method *m = &ifc->m[slot];
   if (m->sret != (r->argbase == 2)) {
      x64_gap_hit("steam", m->name, "struct-return shape disagrees with the call", args[-1]);
      return;
   }
   TR("%s\n", m->name);
   m->fn(args, r, (void *)(uintptr_t)p->native);
}

/* ---- the C surface (MTSHIM shims, steam_tramp.asm) ------------------------ */
static uint32_t g_init_gen;   /* bumped per successful init/shutdown: ContextInit refresh */

uint32_t shim_SteamAPI_InitSafe(uint32_t *a) {
   (void)a;
   NATIVE(bool, SteamAPI_InitSafe, void);
   bool ok = n_SteamAPI_InitSafe ? n_SteamAPI_InitSafe() : false;
   if (ok) g_init_gen++;
   TR("SteamAPI_InitSafe -> %d\n", ok);
   return ok;
}
uint32_t shim_SteamAPI_Init(uint32_t *a) { return shim_SteamAPI_InitSafe(a); }
uint32_t shim_SteamAPI_Shutdown(uint32_t *a) {
   (void)a;
   NATIVE(void, SteamAPI_Shutdown, void);
   if (n_SteamAPI_Shutdown) n_SteamAPI_Shutdown();
   g_init_gen++;
   return 0;
}
uint32_t shim_SteamAPI_RestartAppIfNecessary(uint32_t *a) {
   NATIVE(bool, SteamAPI_RestartAppIfNecessary, uint32_t);
   return n_SteamAPI_RestartAppIfNecessary ? n_SteamAPI_RestartAppIfNecessary(a[0]) : 0;
}
uint32_t shim_SteamAPI_RunCallbacks(uint32_t *a) {
   (void)a;
   NATIVE(void, SteamAPI_RunCallbacks, void);
   if (n_SteamAPI_RunCallbacks) n_SteamAPI_RunCallbacks();
   return 0;
}
uint32_t shim_SteamAPI_GetHSteamPipe(uint32_t *a) {
   (void)a; NATIVE(int32_t, SteamAPI_GetHSteamPipe, void);
   return n_SteamAPI_GetHSteamPipe ? (uint32_t)n_SteamAPI_GetHSteamPipe() : 0;
}
uint32_t shim_SteamAPI_GetHSteamUser(uint32_t *a) {
   (void)a; NATIVE(int32_t, SteamAPI_GetHSteamUser, void);
   return n_SteamAPI_GetHSteamUser ? (uint32_t)n_SteamAPI_GetHSteamUser() : 0;
}
uint32_t shim_SteamAPI_SetMiniDumpComment(uint32_t *a) {
   NATIVE(void, SteamAPI_SetMiniDumpComment, const char *);
   if (n_SteamAPI_SetMiniDumpComment) n_SteamAPI_SetMiniDumpComment((const char *)(uintptr_t)a[0]);
   return 0;
}
uint32_t shim_SteamAPI_SetTryCatchCallbacks(uint32_t *a) {
   NATIVE(void, SteamAPI_SetTryCatchCallbacks, bool);
   if (n_SteamAPI_SetTryCatchCallbacks) n_SteamAPI_SetTryCatchCallbacks(a[0] & 0xff);
   return 0;
}
/* Steam's breakpad would install a NATIVE crash handler that walks an i386
 * stack it cannot read and calls an i386 pre-minidump callback natively: we
 * keep our own fault reporter instead. A deliberate no-op, reported once. */
uint32_t shim_SteamAPI_UseBreakpadCrashHandler(uint32_t *a) { GAP_STUB(a); return 0; }

uint32_t shim_SteamClient(uint32_t *a) {
   (void)a; NATIVE(void *, SteamClient, void);
   return n_SteamClient ? (uint32_t)sb_wrap(n_SteamClient(), "SteamClient020") : 0;
}
uint32_t shim_SteamInternal_CreateInterface(uint32_t *a) {
   NATIVE(void *, SteamInternal_CreateInterface, const char *);
   const char *ver = (const char *)(uintptr_t)a[0];
   return n_SteamInternal_CreateInterface ? (uint32_t)sb_wrap(n_SteamInternal_CreateInterface(ver), ver) : 0;
}
uint32_t shim_SteamInternal_FindOrCreateUserInterface(uint32_t *a) {
   NATIVE(void *, SteamInternal_FindOrCreateUserInterface, int32_t, const char *);
   const char *ver = (const char *)(uintptr_t)a[1];
   void *n = n_SteamInternal_FindOrCreateUserInterface
      ? n_SteamInternal_FindOrCreateUserInterface((int32_t)a[0], ver) : NULL;
   return (uint32_t)sb_wrap(n, ver);
}
uint32_t shim_SteamInternal_FindOrCreateGameServerInterface(uint32_t *a) {
   NATIVE(void *, SteamInternal_FindOrCreateGameServerInterface, int32_t, const char *);
   const char *ver = (const char *)(uintptr_t)a[1];
   void *n = n_SteamInternal_FindOrCreateGameServerInterface
      ? n_SteamInternal_FindOrCreateGameServerInterface((int32_t)a[0], ver) : NULL;
   return (uint32_t)sb_wrap(n, ver);
}
/* SteamInternal_GameServer_Init(uint32 unIP, uint16 usLegacySteamPort,
 *   uint16 usGamePort, uint16 usQueryPort, EServerMode, const char *pchVersion) */
uint32_t shim_SteamInternal_GameServer_Init(uint32_t *a) {
   NATIVE(bool, SteamInternal_GameServer_Init, uint32_t, uint16_t, uint16_t, uint16_t, int32_t, const char *);
   bool ok = n_SteamInternal_GameServer_Init
      ? n_SteamInternal_GameServer_Init(a[0], (uint16_t)a[1], (uint16_t)a[2], (uint16_t)a[3],
                                        (int32_t)a[4], (const char *)(uintptr_t)a[5]) : false;
   if (ok) g_init_gen++;
   TR("SteamInternal_GameServer_Init -> %d\n", ok);
   return ok;
}
uint32_t shim_SteamGameServer_GetHSteamPipe(uint32_t *a) {
   (void)a; NATIVE(int32_t, SteamGameServer_GetHSteamPipe, void);
   return n_SteamGameServer_GetHSteamPipe ? (uint32_t)n_SteamGameServer_GetHSteamPipe() : 0;
}
uint32_t shim_SteamGameServer_GetHSteamUser(uint32_t *a) {
   (void)a; NATIVE(int32_t, SteamGameServer_GetHSteamUser, void);
   return n_SteamGameServer_GetHSteamUser ? (uint32_t)n_SteamGameServer_GetHSteamUser() : 0;
}
uint32_t shim_SteamGameServer_GetIPCCallCount(uint32_t *a) {
   (void)a; NATIVE(uint32_t, SteamGameServer_GetIPCCallCount, void);
   return n_SteamGameServer_GetIPCCallCount ? n_SteamGameServer_GetIPCCallCount() : 0;
}
uint32_t shim_SteamGameServer_RunCallbacks(uint32_t *a) {
   (void)a; NATIVE(void, SteamGameServer_RunCallbacks, void);
   if (n_SteamGameServer_RunCallbacks) n_SteamGameServer_RunCallbacks();
   return 0;
}
uint32_t shim_SteamGameServer_Shutdown(uint32_t *a) {
   (void)a; NATIVE(void, SteamGameServer_Shutdown, void);
   if (n_SteamGameServer_Shutdown) n_SteamGameServer_Shutdown();
   g_init_gen++;
   return 0;
}

/* SteamInternal_ContextInit(CSteamAPIContextInitData *): i386 layout
 * { void (*pFn)(void *ctx) @0; uintp counter @4; CSteamAPIContext ctx @8 }.
 * The native implementation would call the i386 pFn natively; do its job here:
 * (re)fill the context through the i386 pFn whenever Steam was (re)inited. */
static uint64_t sb_low_stack_top(void) {
   volatile char probe[16];
   uint64_t here = (uint64_t)(uintptr_t)probe;
   if (here < 0x100000000ULL) return (here - 64) & ~0xfULL;   /* already on the i386 stack */
   static __thread char *own;
   if (!own) own = malloc(512 * 1024);                        /* low 4GB */
   return ((uint64_t)(uintptr_t)own + 512 * 1024) & ~0xfULL;
}
uint32_t shim_SteamInternal_ContextInit(uint32_t *a) {
   uint32_t data = a[0];
   uint32_t *d = (uint32_t *)(uintptr_t)data;
   if (d[1] != g_init_gen) {
      uint32_t words[1] = { data + 8 };
      _86x64_call_i386(d[0], 1, words, sb_low_stack_top());
      d[1] = g_init_gen;
   }
   return data + 8;
}

/* ---- callbacks: native CCallbackBase stand-ins for i386 objects ------------
 * i386 CCallbackBase: { vptr @0; uint8 m_nCallbackFlags @4; int m_iCallback @8 },
 * vtable { Run(void*), Run(void*, bool, SteamAPICall_t), GetCallbackSizeBytes() }.
 * Native layout: vptr @0 (8), flags @8, iCallback @12 — sb_cb mirrors that and
 * remembers the i386 object. */
struct sb_cb { const void *vtbl; uint8_t flags; uint8_t pad[3]; int32_t icb; uint32_t obj; };

static uint32_t i386_vfn(uint32_t obj, int k) {
   uint32_t vt = *(uint32_t *)(uintptr_t)obj;
   return ((uint32_t *)(uintptr_t)vt)[k];
}
static int32_t cb_size_i386(uint32_t obj) {
   uint32_t w[1] = { obj };
   return (int32_t)_86x64_call_i386(i386_vfn(obj, 2), 1, w, sb_low_stack_top());
}
static uint32_t cb_lowcopy(uint32_t obj, const void *param) {
   int32_t n = cb_size_i386(obj);
   if (n <= 0 || n > (1 << 20)) n = 0;
   void *low = malloc(n ? (size_t)n : 1);
   if (n && param) memcpy(low, param, (size_t)n);
   return (uint32_t)(uintptr_t)low;
}
static void cb_run(struct sb_cb *self, void *param) {
   uint32_t low = cb_lowcopy(self->obj, param);
   uint32_t w[2] = { self->obj, low };
   _86x64_call_i386(i386_vfn(self->obj, 0), 2, w, sb_low_stack_top());
   free((void *)(uintptr_t)low);
}
static void cb_run2(struct sb_cb *self, void *param, bool io_failure, uint64_t call) {
   uint32_t low = cb_lowcopy(self->obj, param);
   uint32_t w[5] = { self->obj, low, io_failure, (uint32_t)call, (uint32_t)(call >> 32) };
   _86x64_call_i386(i386_vfn(self->obj, 1), 5, w, sb_low_stack_top());
   free((void *)(uintptr_t)low);
}
static int cb_size(struct sb_cb *self) { return cb_size_i386(self->obj); }
static const void *const g_cb_vtbl[3] = { (void *)cb_run, (void *)cb_run2, (void *)cb_size };

static struct { uint32_t obj; struct sb_cb *cb; } g_cbs[1024];
static int g_ncbs;

static struct sb_cb *cb_for(uint32_t obj, int create) {
   pthread_mutex_lock(&g_lk);
   for (int i = 0; i < g_ncbs; i++)
      if (g_cbs[i].obj == obj) { struct sb_cb *c = g_cbs[i].cb; pthread_mutex_unlock(&g_lk); return c; }
   struct sb_cb *c = NULL;
   if (create && g_ncbs < 1024) {
      c = malloc_zone_calloc(malloc_default_zone(), 1, sizeof *c);   /* native side owns it */
      c->vtbl = g_cb_vtbl;
      c->obj = obj;
      g_cbs[g_ncbs].obj = obj; g_cbs[g_ncbs].cb = c; g_ncbs++;
   }
   pthread_mutex_unlock(&g_lk);
   return c;
}
static void cb_mirror(struct sb_cb *c) {          /* native-written state -> i386 object */
   uint8_t *o = (uint8_t *)(uintptr_t)c->obj;
   o[4] = c->flags;
   *(int32_t *)(o + 8) = c->icb;
}
uint32_t shim_SteamAPI_RegisterCallback(uint32_t *a) {
   NATIVE(void, SteamAPI_RegisterCallback, void *, int32_t);
   struct sb_cb *c = cb_for(a[0], 1);
   if (!c || !n_SteamAPI_RegisterCallback) return 0;
   c->flags = *(uint8_t *)(uintptr_t)(a[0] + 4);   /* e.g. the game-server flag */
   c->icb = (int32_t)a[1];
   n_SteamAPI_RegisterCallback(c, (int32_t)a[1]);
   cb_mirror(c);
   TR("RegisterCallback obj=0x%x id=%d\n", a[0], (int32_t)a[1]);
   return 0;
}
uint32_t shim_SteamAPI_UnregisterCallback(uint32_t *a) {
   NATIVE(void, SteamAPI_UnregisterCallback, void *);
   struct sb_cb *c = cb_for(a[0], 0);
   if (c && n_SteamAPI_UnregisterCallback) { n_SteamAPI_UnregisterCallback(c); cb_mirror(c); }
   return 0;
}
uint32_t shim_SteamAPI_RegisterCallResult(uint32_t *a) {
   NATIVE(void, SteamAPI_RegisterCallResult, void *, uint64_t);
   struct sb_cb *c = cb_for(a[0], 1);
   if (!c || !n_SteamAPI_RegisterCallResult) return 0;
   c->flags = *(uint8_t *)(uintptr_t)(a[0] + 4);
   c->icb = *(int32_t *)(uintptr_t)(a[0] + 8);
   n_SteamAPI_RegisterCallResult(c, sb_u64(&a[1]));
   cb_mirror(c);
   return 0;
}
uint32_t shim_SteamAPI_UnregisterCallResult(uint32_t *a) {
   NATIVE(void, SteamAPI_UnregisterCallResult, void *, uint64_t);
   struct sb_cb *c = cb_for(a[0], 0);
   if (c && n_SteamAPI_UnregisterCallResult) { n_SteamAPI_UnregisterCallResult(c, sb_u64(&a[1])); cb_mirror(c); }
   return 0;
}
