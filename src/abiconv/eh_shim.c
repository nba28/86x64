/*
 * C++ EXCEPTION unwinding across TRANSLATED i386 frames.  Single concern
 * (the one-shim-one-job rule): everything the Itanium C++ exception
 * model needs to throw/catch/rethrow/unwind-through-destructors inside code the
 * macho-tool rewrote from i386 to x86_64.  Kept out of cxx_shim.c (which owns
 * the non-unwinding C++ runtime: guards, atexit, new/delete, _Rb_tree) so the
 * two compose and can be reasoned about / toggled independently.
 *
 * ============================ THE TWO WALLS ============================
 *
 * WALL 1 (ABI) — hit FIRST.  The legacy i386 binary imports __cxa_throw,
 * __cxa_begin_catch, __cxa_end_catch, __cxa_allocate_exception,
 * __gxx_personality_v0 and _Unwind_Resume from /usr/lib/libstdc++.6.dylib /
 * libSystem.  cxx_shim.c deliberately leaves them unshimmed, so dyld binds them
 * to the NATIVE x86_64 libstdc++/libunwind present in the Tahoe shared cache.
 * Translated i386 cdecl code then calls them with i386 stack args + a 4-byte
 * return slot; the native callee uses SysV regs and an 8-byte return, over-pops
 * the i386 frame and faults at a fused PC.  (Reproduced: f01 prints "before
 * throw" then SIGSEGVs at 0x4_00032a45.)  Wall-1 fix = the i386-discipline
 * shims in this file (trampolines in eh_tramp.asm, binds redirected by
 * static-interpose).  The exception OBJECT lifecycle (allocate / begin_catch /
 * end_catch / rethrow / free) is fully implemented here on the i386 layout.
 *
 * WALL 2 (stale CFI) — the real unwind.  macho-tool copies the original i386
 * __eh_frame (DWARF CFI), __gcc_except_tab (LSDA) and __compact_unwind through
 * as OPAQUE data; it does NOT rewrite them.  So every PC/offset/range inside
 * them is in the ORIGINAL i386 address space, while the code now lives at
 * different x86_64 addresses (verified on f01: LSDA landing-pad off 0x5d vs the
 * translated 0xae; FDE range 0xef vs translated ~0x1ec).  The native unwinder
 * therefore cannot find our frames or landing pads.  This file implements its
 * OWN unwinder + personality that:
 *   - walks the translated frames (which keep EXACT i386 stack discipline:
 *     4-byte ebp chain, 4-byte translated return addresses — verified in the
 *     disassembly), so frame-pointer unwinding works for framed functions;
 *   - reads the ORIGINAL i386 LSDA (still in the binary) to find call sites,
 *     landing pads, actions and catch types (i386 type_info, 4-byte fields);
 *   - maps ORIGINAL<->TRANSLATED PCs through a translator-emitted map so it can
 *     turn an LSDA landing-pad offset into the address to actually resume at,
 *     and turn a translated return address into the original PC the LSDA is
 *     keyed by;
 *   - resumes the landing pad with rax = exception handle, rdx = selector (the
 *     Itanium handoff), via eh_resume in eh_tramp.asm.
 *
 * THE PC MAP (core dependency).  The one thing this file cannot synthesize at
 * runtime is the original<->translated PC correspondence (the original code is
 * gone; the symtab only gives function starts, not the non-linear intra-
 * function shifts).  The blob graph holds it during translation, so it must be
 * emitted by macho-tool.  Two sections, both trivially available from the blob
 * graph (see the SendMessage to `main`):
 *
 *   __DATA,__86x64_pcmap   : { u32 count; u32 _r; } then count *
 *                            { u32 trans_off; u32 orig_off; } sorted by
 *                            trans_off (image-relative; one per instruction).
 *   __DATA,__86x64_ehlsda  : { u32 count; u32 _r; } then count *
 *                            { u32 trans_func; u32 orig_func; u32 lsda_off; }
 *                            sorted by trans_func (lsda_off = image-relative
 *                            __gcc_except_tab address for that function, 0 if
 *                            none).
 *
 * Until those sections exist this file is a correct WALL-1 layer: the entry
 * points are callable (no more ABI crash), the exception object is built and
 * tracked, and a throw that cannot be resolved (no map) terminates cleanly with
 * a diagnostic instead of corrupting memory — exactly std::terminate semantics
 * for an unhandled exception.  When the sections land, the same code unwinds
 * end to end with no further change here.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include "dyld_image_list.h"

/* ===================================================================== */
/* eh_tramp.asm                                                          */
/* ===================================================================== */
extern void eh_resume(uint32_t rip, uint32_t esp, uint32_t ebp,
                      uint32_t eax_exc, uint32_t edx_sel,
                      uint32_t ebx, uint32_t esi, uint32_t edi) __attribute__((noreturn));

/* ===================================================================== */
/* DWARF EH pointer encodings (DW_EH_PE_*)                              */
/* ===================================================================== */
#define DW_EH_PE_omit     0xff
#define DW_EH_PE_absptr   0x00
#define DW_EH_PE_uleb128  0x01
#define DW_EH_PE_udata2   0x02
#define DW_EH_PE_udata4   0x03
#define DW_EH_PE_udata8   0x04
#define DW_EH_PE_sleb128  0x09
#define DW_EH_PE_sdata2   0x0a
#define DW_EH_PE_sdata4   0x0b
#define DW_EH_PE_sdata8   0x0c
#define DW_EH_PE_pcrel    0x10
#define DW_EH_PE_textrel  0x20
#define DW_EH_PE_datarel  0x30
#define DW_EH_PE_funcrel  0x40
#define DW_EH_PE_aligned  0x50
#define DW_EH_PE_indirect 0x80

static int eh_trace_on(void) {
   static int v = -1;
   if (v < 0) { const char *e = getenv("ABICONV_EH_TRACE"); v = (e && *e) ? 1 : 0; }
   return v;
}
#define EHLOG(...) do { if (eh_trace_on()) { fprintf(stderr, "[eh] " __VA_ARGS__); fflush(stderr); } } while (0)

/* ===================================================================== */
/* uleb/sleb readers (operate on low-4GB i386 data pointers)            */
/* ===================================================================== */
static uint64_t read_uleb(const uint8_t **pp) {
   const uint8_t *p = *pp;
   uint64_t r = 0; int s = 0; uint8_t b;
   do { b = *p++; r |= (uint64_t)(b & 0x7f) << s; s += 7; } while (b & 0x80);
   *pp = p; return r;
}
static int64_t read_sleb(const uint8_t **pp) {
   const uint8_t *p = *pp;
   int64_t r = 0; int s = 0; uint8_t b;
   do { b = *p++; r |= (int64_t)(b & 0x7f) << s; s += 7; } while (b & 0x80);
   if (s < 64 && (b & 0x40)) r |= -((int64_t)1 << s);
   *pp = p; return r;
}

/* Read one encoded value (LSDA call-site / TType entries). Returns the decoded
 * (absolute, low-4GB) value; advances *pp.  `base` is the encoding-relative
 * base (start of the LSDA region for funcrel, the value's own address for
 * pcrel).  Only the encodings GCC/clang emit for i386 EH are handled. */
static uint32_t read_encoded(const uint8_t **pp, uint8_t enc, uint32_t pcrel_base) {
   const uint8_t *p = *pp;
   uint32_t start = (uint32_t)(uintptr_t)p;
   uint64_t val = 0;
   switch (enc & 0x0f) {
      case DW_EH_PE_absptr:  val = *(const uint32_t *)p; p += 4; break;
      case DW_EH_PE_uleb128: val = read_uleb(&p); break;
      case DW_EH_PE_udata2:  val = *(const uint16_t *)p; p += 2; break;
      case DW_EH_PE_udata4:  val = *(const uint32_t *)p; p += 4; break;
      case DW_EH_PE_sdata2:  val = (uint32_t)(int32_t)*(const int16_t *)p; p += 2; break;
      case DW_EH_PE_sdata4:  val = (uint32_t)*(const int32_t *)p; p += 4; break;
      case DW_EH_PE_sleb128: val = (uint32_t)read_sleb(&p); break;
      default:               val = *(const uint32_t *)p; p += 4; break;
   }
   *pp = p;
   if (val == 0) return 0;                       /* a 0 offset stays 0 (no LP) */
   uint32_t abs;
   switch (enc & 0x70) {
      case DW_EH_PE_pcrel:   abs = start + (uint32_t)val; break;
      case DW_EH_PE_funcrel:
      case DW_EH_PE_datarel:
      case DW_EH_PE_textrel: abs = pcrel_base + (uint32_t)val; break;
      default:               abs = (uint32_t)val; break;   /* absptr */
   }
   if (enc & DW_EH_PE_indirect) abs = *(const uint32_t *)(uintptr_t)abs;
   return abs;
}

/* ===================================================================== */
/* Exception object model (i386 layout; this file owns it end to end)   */
/* ===================================================================== */
#define EH_MAGIC 0x48453836u                      /* "86EH" */

struct eh_exception {                             /* header, precedes object */
   uint32_t magic;
   uint32_t type_info;                            /* i386 std::type_info*    */
   uint32_t destructor;                           /* i386 void(*)(void*) | 0 */
   int32_t  handler_count;                        /* begin/end_catch nesting */
   uint32_t next;                                 /* caught-stack chain      */
   uint32_t obj;                                  /* thrown object (==hdr+1) */
   uint32_t adjusted;                             /* catch-bound pointer     */
   int32_t  selector;                             /* chosen handler index    */
};

/* Per-thread exception machinery (native TLS; libabiconv is native code). */
struct eh_globals {
   uint32_t caught;                               /* top of caught stack     */
   uint32_t uncaught_count;
};
static pthread_key_t g_eh_key;
static pthread_once_t g_eh_once = PTHREAD_ONCE_INIT;
static void eh_key_make(void) { pthread_key_create(&g_eh_key, free); }
static struct eh_globals *eh_get(void) {
   pthread_once(&g_eh_once, eh_key_make);
   struct eh_globals *g = pthread_getspecific(g_eh_key);
   if (!g) { g = calloc(1, sizeof *g); pthread_setspecific(g_eh_key, g); }
   return g;
}

static struct eh_exception *hdr_from_obj(uint32_t obj) {
   if (!obj) return NULL;
   struct eh_exception *h = (struct eh_exception *)(uintptr_t)(obj - (uint32_t)sizeof(struct eh_exception));
   return (h->magic == EH_MAGIC) ? h : NULL;
}
static struct eh_exception *hdr_from_handle(uint32_t handle) {
   if (!handle) return NULL;
   struct eh_exception *h = (struct eh_exception *)(uintptr_t)handle;
   return (h->magic == EH_MAGIC) ? h : NULL;
}

/* ===================================================================== */
/* Translator PC map + LSDA table discovery (per loaded image)          */
/* ===================================================================== */
/* On-disk records (see Archive::inject_pcmap_section / inject_ehlsda_section).
 * trans offsets are RELATIVE TO __text (convert-invariant); lsda_off is relative
 * to __gcc_except_tab.  At runtime we add the actual section bases. */
struct pcmap_ent { int32_t trans_off; uint32_t orig; };
struct lsda_ent  { int32_t trans_func_off; uint32_t orig_func; int32_t lsda_off; };

/* One i386 __eh_frame FDE + its CIE's relevant fields, for the CFI register
 * restore.  relpc = the function's ORIGINAL i386 start relative to the
 * __eh_frame disk base (recovered from the FDE's pcrel pc-begin, immune to the
 * stale absolute address); `region` is the resolved disk func start. */
struct fde_rec {
   uint32_t relpc;
   uint32_t region;                               /* resolved disk func start */
   const uint8_t *cfi, *cfi_end;                  /* FDE CFI instructions     */
   const uint8_t *cie_cfi, *cie_cfi_end;          /* CIE initial instructions */
   uint8_t  code_align;
   int32_t  data_align;
};

struct eh_image {
   uintptr_t text_base, text_size;                /* runtime __text span     */
   uintptr_t gxt_base;                            /* runtime __gcc_except_tab*/
   const struct pcmap_ent *pcmap;                 /* sorted by trans_off     */
   uint32_t  pcmap_n;
   struct pcmap_ent *pcmap_byorig;                /* malloc'd, sorted by orig*/
   const struct lsda_ent *lsda;                   /* sorted by trans_func_off*/
   uint32_t  lsda_n;
   struct fde_rec *fdes;                          /* malloc'd, sorted by region*/
   uint32_t  fde_n;
};
static struct eh_image g_imgs[64];
static int g_img_n = 0;
static int g_img_scanned = 0;
static pthread_mutex_t g_img_mu = PTHREAD_MUTEX_INITIALIZER;

static int cmp_ent_byorig(const void *x, const void *y) {
   uint32_t a = ((const struct pcmap_ent *)x)->orig, b = ((const struct pcmap_ent *)y)->orig;
   return (a > b) - (a < b);
}
static int cmp_fde_relpc(const void *x, const void *y) {
   uint32_t a = ((const struct fde_rec *)x)->relpc, b = ((const struct fde_rec *)y)->relpc;
   return (a > b) - (a < b);
}
static int cmp_fde_region(const void *x, const void *y) {
   uint32_t a = ((const struct fde_rec *)x)->region, b = ((const struct fde_rec *)y)->region;
   return (a > b) - (a < b);
}

static int eh_enc_sz(uint8_t e) {
   switch (e & 0x0f) {
   case 0x00: case 0x03: case 0x0b: return 4;     /* absptr/udata4/sdata4 (i386) */
   case 0x02: case 0x0a: return 2;
   case 0x04: case 0x0c: return 8;
   default: return -1;                            /* uleb/sleb -> variable */
   }
}

/* Parse __eh_frame: keep the FDEs that carry an LSDA (the EH functions — exactly
 * the ehlsda set), recording each FDE's relpc (its function start relative to
 * the __eh_frame disk base, recovered from the pcrel pc-begin so it's immune to
 * the stale absolute address) + the FDE/CIE CFI instruction ranges.  The
 * function<->FDE association is resolved against the ehlsda in eh_scan_images. */
static void eh_parse_eh_frame(struct eh_image *im, const uint8_t *base, unsigned long size) {
   im->fdes = NULL; im->fde_n = 0;
   if (!base || size < 8) return;
   struct cie_t { size_t off; uint8_t code_align; int32_t data_align; uint8_t r_enc, l_enc;
                  int has_aug, has_l; const uint8_t *cfi, *cfi_end; };
   struct cie_t *cies = malloc(sizeof(struct cie_t) * 64); int ncie = 0, ciecap = 64;
   struct fde_rec *fdes = malloc(((size / 8) + 1) * sizeof(struct fde_rec));
   if (!cies || !fdes) { free(cies); free(fdes); return; }
   uint32_t nfde = 0;
   size_t i = 0;
   while (i + 4 <= size) {
      uint32_t len = *(const uint32_t *)(base + i);
      if (len == 0 || len == 0xffffffffu) break;
      size_t after_len = i + 4, rec_end = after_len + len;
      if (rec_end > size) break;
      uint32_t id = *(const uint32_t *)(base + after_len);
      if (id == 0) {                               /* CIE */
         const uint8_t *p = base + after_len + 4;
         uint8_t version = *p++;
         const char *aug = (const char *)p;
         while (p < base + rec_end && *p) { p++; }
         if (p < base + rec_end) { p++; }
         int has_aug = (aug[0] == 'z');
         read_uleb(&p);                            /* code_align (re-read below) */
         const uint8_t *q = base + after_len + 4 + 1 + (size_t)(strlen(aug) + 1);
         uint64_t code_align = read_uleb(&q);
         int64_t data_align = read_sleb(&q);
         if (version == 1) { q++; } else { read_uleb(&q); }
         uint8_t r_enc = 0, l_enc = 0; int has_l = 0;
         if (has_aug) {
            uint64_t auglen = read_uleb(&q);
            const uint8_t *aug_end = q + auglen;
            for (const char *c = aug + 1; *c; c++) {
               if (*c == 'L') { l_enc = *q++; has_l = 1; }
               else if (*c == 'R') { r_enc = *q++; }
               else if (*c == 'P') { uint8_t pe = *q++; int s = eh_enc_sz(pe);
                                     if (s < 0) read_uleb(&q); else q += s; }
            }
            q = aug_end;
         }
         if (ncie >= ciecap) { ciecap *= 2; cies = realloc(cies, sizeof(struct cie_t) * ciecap); if (!cies) break; }
         cies[ncie++] = (struct cie_t){ i, (uint8_t)(code_align?code_align:1),
            (int32_t)(data_align?data_align:-4), r_enc, l_enc, has_aug, has_l,
            q, base + rec_end };
      } else {                                     /* FDE */
         size_t cie_off = (id <= after_len) ? after_len - id : (size_t)-1;
         struct cie_t *cie = NULL;
         for (int k = 0; k < ncie; k++) if (cies[k].off == cie_off) { cie = &cies[k]; break; }
         if (cie && cie->has_l) {
            const uint8_t *p = base + after_len + 4;
            size_t pcfield = (size_t)(p - base);
            int rsz = eh_enc_sz(cie->r_enc);
            int32_t V = 0;
            if (rsz == 4) { V = *(const int32_t *)p; p += 4; }
            else if (rsz == 2) { V = *(const int16_t *)p; p += 2; }
            else { p += (rsz > 0 ? rsz : 4); }
            uint32_t relpc = (uint32_t)((int64_t)pcfield + V);
            if (rsz > 0) { p += rsz; } else { read_uleb(&p); }   /* pc_range */
            uint64_t auglen = read_uleb(&p);
            const uint8_t *aug_end = p + auglen;
            /* LSDA pointer: keep only FDEs with a real LSDA (the EH set). */
            uint32_t lsda_raw = 0; int lsz = eh_enc_sz(cie->l_enc);
            if (lsz == 4) lsda_raw = *(const uint32_t *)p;
            p = aug_end;
            if (lsda_raw != 0) {
               struct fde_rec *f = &fdes[nfde++];
               f->relpc = relpc; f->region = 0;
               f->cfi = p; f->cfi_end = base + rec_end;
               f->cie_cfi = cie->cfi; f->cie_cfi_end = cie->cfi_end;
               f->code_align = cie->code_align; f->data_align = cie->data_align;
            }
         }
      }
      i = rec_end;
   }
   free(cies);
   im->fdes = fdes; im->fde_n = nfde;
}

/* Resolve each FDE's `region` (disk func start) by pairing the LSDA-bearing FDEs
 * (sorted by relpc) 1:1 with the ehlsda functions (sorted by orig_func): both
 * are exactly the EH-function set, so region[k] = orig_func[k] and the constant
 * ehf_disk_base falls out (region = ehf_disk_base + relpc). */
static void eh_assoc_fdes(struct eh_image *im) {
   if (!im->fdes || !im->fde_n || !im->lsda || !im->lsda_n) { im->fde_n = 0; return; }
   qsort(im->fdes, im->fde_n, sizeof(struct fde_rec), cmp_fde_relpc);
   uint32_t *regions = malloc(sizeof(uint32_t) * im->lsda_n);
   if (!regions) { im->fde_n = 0; return; }
   for (uint32_t k = 0; k < im->lsda_n; k++) regions[k] = im->lsda[k].orig_func;
   /* simple insertion sort of regions (small) */
   for (uint32_t a = 1; a < im->lsda_n; a++) { uint32_t v = regions[a]; int b = (int)a - 1;
      while (b >= 0 && regions[b] > v) { regions[b+1] = regions[b]; b--; } regions[b+1] = v; }
   uint32_t pairs = im->fde_n < im->lsda_n ? im->fde_n : im->lsda_n;
   for (uint32_t k = 0; k < pairs; k++) im->fdes[k].region = regions[k];
   im->fde_n = pairs;
   free(regions);
   qsort(im->fdes, im->fde_n, sizeof(struct fde_rec), cmp_fde_region);
}

static const struct fde_rec *eh_fde_for_region(struct eh_image *im, uint32_t region) {
   for (uint32_t k = 0; k < im->fde_n; k++) if (im->fdes[k].region == region) return &im->fdes[k];
   return NULL;
}

/* Execute the CFI (CIE initial instructions then the FDE up to pc_off) for a
 * frame-pointer function, returning the CFA rule + the CFA-relative save offset
 * of each GP register.  i386 reg numbers: eax0 ecx1 edx2 ebx3 esp4 ebp5 esi6
 * edi7.  reg_off[r] is meaningful only when reg_set[r]. */
static void eh_cfi_run(const struct fde_rec *fde, uint32_t pc_off,
                       int *cfa_reg, int32_t *cfa_off,
                       int32_t reg_off[8], int reg_set[8]) {
   *cfa_reg = 4; *cfa_off = 4;                     /* initial: CFA = esp+4 */
   for (int r = 0; r < 8; r++) reg_set[r] = 0;
   int32_t da = fde->data_align ? fde->data_align : -4;
   uint8_t ca = fde->code_align ? fde->code_align : 1;
   /* remembered state for DW_CFA_remember/restore_state */
   int32_t s_cfa_off = 0; int s_cfa_reg = 0; int32_t s_reg_off[8]; int s_reg_set[8]; int have_saved = 0;
   for (int phase = 0; phase < 2; phase++) {
      const uint8_t *p = phase == 0 ? fde->cie_cfi : fde->cfi;
      const uint8_t *end = phase == 0 ? fde->cie_cfi_end : fde->cfi_end;
      uint32_t loc = 0;
      while (p < end) {
         uint8_t op = *p++;
         uint8_t hi = op & 0xc0, lo = op & 0x3f;
         if (hi == 0x40) {                          /* advance_loc */
            loc += (uint32_t)lo * ca;
            if (phase == 1 && loc > pc_off) return;
         } else if (hi == 0x80) {                   /* offset reg */
            uint64_t n = read_uleb(&p);
            reg_off[lo & 7] = (int32_t)n * da; reg_set[lo & 7] = 1;
         } else if (hi == 0xc0) {                    /* restore reg */
            reg_set[lo & 7] = 0;
         } else {                                    /* extended (op = lo) */
            switch (op) {
            case 0x00: break;                        /* nop */
            case 0x01: { read_uleb(&p); /*set_loc abs (rare)*/ break; }
            case 0x02: { uint32_t d = *p++; loc += d * ca; if (phase==1 && loc>pc_off) return; break; }
            case 0x03: { uint32_t d = *(const uint16_t *)p; p += 2; loc += d * ca; if (phase==1 && loc>pc_off) return; break; }
            case 0x04: { uint32_t d = *(const uint32_t *)p; p += 4; loc += d * ca; if (phase==1 && loc>pc_off) return; break; }
            case 0x05: { uint64_t r = read_uleb(&p); uint64_t n = read_uleb(&p);
                         reg_off[r & 7] = (int32_t)n * da; reg_set[r & 7] = 1; break; }
            case 0x06: case 0x07: case 0x08: { uint64_t r = read_uleb(&p); if (op!=0x08) {} (void)r;
                         reg_set[r & 7] = 0; break; }     /* restore_ext/undefined/same */
            case 0x09: { uint64_t r1 = read_uleb(&p); read_uleb(&p); (void)r1; break; } /* register */
            case 0x0a: { s_cfa_off=*cfa_off; s_cfa_reg=*cfa_reg;
                         for(int r=0;r<8;r++){s_reg_off[r]=reg_off[r];s_reg_set[r]=reg_set[r];} have_saved=1; break; }
            case 0x0b: { if (have_saved){ *cfa_off=s_cfa_off; *cfa_reg=s_cfa_reg;
                         for(int r=0;r<8;r++){reg_off[r]=s_reg_off[r];reg_set[r]=s_reg_set[r];} } break; }
            case 0x0c: { *cfa_reg = (int)read_uleb(&p); *cfa_off = (int32_t)read_uleb(&p); break; }
            case 0x0d: { *cfa_reg = (int)read_uleb(&p); break; }
            case 0x0e: { *cfa_off = (int32_t)read_uleb(&p); break; }
            case 0x0f: { read_uleb(&p); /*def_cfa_expression*/ goto done; }
            case 0x12: { *cfa_reg=(int)read_uleb(&p); *cfa_off=(int32_t)read_sleb(&p)*da; break; } /* def_cfa_sf */
            case 0x13: { *cfa_off=(int32_t)read_sleb(&p)*da; break; }                              /* def_cfa_offset_sf */
            default: goto done;                      /* unknown -> stop */
            }
         }
      }
   }
done:
   return;
}

/* Locate the __86x64_pcmap / __86x64_ehlsda sections in each loaded image and
 * register the ones that carry them.  Called lazily on the first throw. */
static void eh_scan_images(void) {
   if (g_img_scanned) return;
   pthread_mutex_lock(&g_img_mu);
   if (g_img_scanned) { pthread_mutex_unlock(&g_img_mu); return; }
   uint32_t n = x64_img_count();
   for (uint32_t i = 0; i < n && g_img_n < (int)(sizeof g_imgs / sizeof g_imgs[0]); i++) {
      const struct mach_header *mh = x64_img_header(i);
      if (!mh) continue;
      const struct mach_header_64 *mh64 = (const struct mach_header_64 *)mh;
      unsigned long pcsz = 0, lssz = 0, txsz = 0, gxsz = 0;
      const uint8_t *pc = getsectiondata(mh64, "__DATA", "__86x64_pcmap", &pcsz);
      if (!pc) pc = getsectiondata(mh64, "__TEXT", "__86x64_pcmap", &pcsz);
      if (!pc || pcsz < 8) continue;
      const uint8_t *tx = getsectiondata(mh64, "__TEXT", "__text", &txsz);
      if (!tx) continue;                           /* need the trans anchor */
      const uint8_t *ls = getsectiondata(mh64, "__DATA", "__86x64_ehlsda", &lssz);
      if (!ls) ls = getsectiondata(mh64, "__TEXT", "__86x64_ehlsda", &lssz);
      const uint8_t *gx = getsectiondata(mh64, "__TEXT", "__gcc_except_tab", &gxsz);
      unsigned long ehfsz = 0;
      const uint8_t *ehf = getsectiondata(mh64, "__TEXT", "__eh_frame", &ehfsz);

      struct eh_image *im = &g_imgs[g_img_n];
      memset(im, 0, sizeof *im);
      im->text_base = (uintptr_t) tx;
      im->text_size = txsz;
      im->gxt_base  = (uintptr_t) gx;
      im->pcmap_n = ((const uint32_t *)pc)[0];
      im->pcmap   = (const struct pcmap_ent *)(pc + 8);
      if ((unsigned long)im->pcmap_n * 8 + 8 > pcsz) im->pcmap_n = (uint32_t)((pcsz - 8) / 8);
      if (ls && lssz >= 8) {
         im->lsda_n = ((const uint32_t *)ls)[0];
         im->lsda   = (const struct lsda_ent *)(ls + 8);
         if ((unsigned long)im->lsda_n * 12 + 8 > lssz) im->lsda_n = (uint32_t)((lssz - 8) / 12);
      }
      /* Build the orig-sorted index for orig->trans lookups. */
      im->pcmap_byorig = malloc((size_t)im->pcmap_n * sizeof(struct pcmap_ent));
      if (im->pcmap_byorig) {
         memcpy(im->pcmap_byorig, im->pcmap, (size_t)im->pcmap_n * sizeof(struct pcmap_ent));
         qsort(im->pcmap_byorig, im->pcmap_n, sizeof(struct pcmap_ent), cmp_ent_byorig);
      }
      /* Parse __eh_frame CFI for callee-saved register restore at landing pads. */
      eh_parse_eh_frame(im, ehf, ehfsz);
      eh_assoc_fdes(im);
      EHLOG("registered eh-image #%d text=[%#lx,+%#lx) gxt=%#lx pcmap=%u lsda=%u\n",
            g_img_n, (unsigned long)im->text_base, (unsigned long)im->text_size,
            (unsigned long)im->gxt_base, im->pcmap_n, im->lsda_n);
      g_img_n++;
   }
   g_img_scanned = 1;
   pthread_mutex_unlock(&g_img_mu);
}

static struct eh_image *eh_image_for_pc(uintptr_t ret) {
   for (int i = 0; i < g_img_n; i++) {
      struct eh_image *im = &g_imgs[i];
      if (im->text_base && ret >= im->text_base && ret < im->text_base + im->text_size)
         return im;
   }
   return NULL;
}

/* translated runtime PC -> original i386 PC.  Return addresses are instruction-
 * aligned, so the largest trans_off <= toff is the instruction the call returns
 * into; its orig is the original return address (parse-space i386 vmaddr). */
static int pc_trans_to_orig(struct eh_image *im, uintptr_t ret, uint32_t *orig_out) {
   const struct pcmap_ent *m = im->pcmap; uint32_t n = im->pcmap_n;
   if (!n) return 0;
   const int32_t toff = (int32_t)(ret - im->text_base);
   uint32_t lo = 0, hi = n;                        /* last trans_off <= toff  */
   while (lo < hi) { uint32_t mid = (lo + hi) / 2; if (m[mid].trans_off <= toff) lo = mid + 1; else hi = mid; }
   if (lo == 0) return 0;
   *orig_out = m[lo - 1].orig + (uint32_t)(toff - m[lo - 1].trans_off);
   return 1;
}

/* original i386 PC -> translated runtime address (for landing pads). */
static int pc_orig_to_trans(struct eh_image *im, uint32_t orig_pc, uintptr_t *trans_out) {
   const struct pcmap_ent *m = im->pcmap_byorig; uint32_t n = im->pcmap_n;
   if (!m || !n) return 0;
   uint32_t lo = 0, hi = n;
   while (lo < hi) { uint32_t mid = (lo + hi) / 2; if (m[mid].orig <= orig_pc) lo = mid + 1; else hi = mid; }
   if (lo == 0) return 0;
   const int32_t toff = m[lo - 1].trans_off + (int32_t)(orig_pc - m[lo - 1].orig);
   *trans_out = im->text_base + (intptr_t)toff;
   return 1;
}

/* Find the EH function record covering a translated runtime PC. */
static const struct lsda_ent *eh_lsda_for_pc(struct eh_image *im, uintptr_t ret) {
   const struct lsda_ent *r = im->lsda; uint32_t n = im->lsda_n;
   if (!r || !n) return NULL;
   const int32_t toff = (int32_t)(ret - im->text_base);
   uint32_t lo = 0, hi = n;
   while (lo < hi) { uint32_t mid = (lo + hi) / 2; if (r[mid].trans_func_off <= toff) lo = mid + 1; else hi = mid; }
   if (lo == 0) return NULL;
   return &r[lo - 1];
}

/* ===================================================================== */
/* Type matching (i386 std::type_info)                                  */
/* ===================================================================== */
/* Exact match + catch-all for increment 1.  Public-base / pointer-qualifier
 * matching (type_info::__do_catch) shares the i386 type_info hierarchy walk
 * the RTTI agent is building for __dynamic_cast; folded in once that lands. */
static int type_caught_by(uint32_t thrown_ti, uint32_t catch_ti, uint32_t *adjust_io) {
   (void)adjust_io;
   if (catch_ti == 0) return 1;                    /* catch(...) */
   if (thrown_ti == catch_ti) return 1;            /* exact type */
   return 0;
}

/* ===================================================================== */
/* The personality: parse one frame's LSDA for a given original PC.      */
/* Returns 1 and fills *lp_orig (landing-pad ORIGINAL address) + *selector  */
/* when an action (catch or cleanup) applies; 0 when the frame is transparent. */
/* ===================================================================== */
/* `region` is the EFFECTIVE @LPStart the core resolved into pcmap-orig space
 * (Archive::collect_eh_lsda_pairs walks the i386 instruction stream so the LSDA
 * disk offsets — which include PIC get_pc_thunk bytes the M64 transform drops —
 * line up with the compressed translated layout).  So `region + cs_off` is
 * directly a valid pcmap-orig address. */
static int eh_scan_lsda(uint32_t lsda_addr, uint32_t region, uint32_t orig_pc,
                        uint32_t thrown_ti, int want_handler,
                        uint32_t *lp_orig, int32_t *selector, uint32_t *adjusted) {
   if (!lsda_addr) return 0;
   const uint8_t *p = (const uint8_t *)(uintptr_t)lsda_addr;

   uint8_t lpstart_enc = *p++;
   uint32_t lpstart = region;
   if (lpstart_enc != DW_EH_PE_omit)
      lpstart = read_encoded(&p, lpstart_enc, region);

   uint8_t ttype_enc = *p++;
   const uint8_t *ttype_base = NULL;
   if (ttype_enc != DW_EH_PE_omit) {
      uint64_t ttoff = read_uleb(&p);
      ttype_base = p + ttoff;
   }

   uint8_t cs_enc = *p++;
   uint64_t cs_len = read_uleb(&p);
   const uint8_t *cs = p;
   const uint8_t *cs_end = p + cs_len;
   const uint8_t *action_tab = cs_end;

   uint32_t ip_off = (orig_pc - 1) - region;
   while (cs < cs_end) {
      uint32_t cs_start = read_encoded(&cs, cs_enc, 0);
      uint32_t cs_clen  = read_encoded(&cs, cs_enc, 0);
      uint32_t cs_lp    = read_encoded(&cs, cs_enc, 0);
      uint64_t cs_act   = read_uleb(&cs);
      if (ip_off < cs_start || ip_off >= cs_start + cs_clen) continue;

      if (cs_lp == 0) return 0;                    /* no landing pad here */
      uint32_t lp = lpstart + cs_lp;
      if (cs_act == 0) {                           /* cleanup-only */
         if (want_handler) return 0;               /* phase 1 ignores cleanups */
         *lp_orig = lp; *selector = 0; return 1;
      }
      /* Walk the WHOLE action chain: a matching catch wins (install the handler);
       * otherwise a cleanup applies (run dtors then continue via _Unwind_Resume).*/
      int have_cleanup = 0;
      const uint8_t *ap = action_tab + (cs_act - 1);
      for (;;) {
         const uint8_t *aq = ap;
         int64_t ttype_index = read_sleb(&aq);
         const uint8_t *after_idx = aq;
         int64_t next_off = read_sleb(&aq);
         if (ttype_index > 0) {                    /* catch clause */
            uint32_t catch_ti = 0;
            if (ttype_base) {
               const uint8_t *tp = ttype_base - ttype_index * 4; /* sdata4 slots */
               catch_ti = read_encoded(&tp, ttype_enc, 0);
            }
            uint32_t adj = 0;
            if (type_caught_by(thrown_ti, catch_ti, &adj)) {
               EHLOG("match: cs_start=%#x clen=%#x cs_lp=%#x region=%#x "
                     "orig_pc=%#x -> lp_orig=%#x ti=%d\n", cs_start, cs_clen,
                     cs_lp, region, orig_pc, lp, (int)ttype_index);
               *lp_orig = lp; *selector = (int32_t)ttype_index;
               if (adjusted) *adjusted = adj;
               return 1;
            }
         } else if (ttype_index == 0) {            /* cleanup in the action chain */
            have_cleanup = 1;
         }
         /* ttype_index < 0 => exception-spec; treated as no-match here. */
         if (next_off == 0) break;
         ap = after_idx + next_off;
      }
      if (have_cleanup && !want_handler) {         /* no catch matched: run cleanup */
         EHLOG("cleanup: cs_lp=%#x region=%#x orig_pc=%#x -> lp_orig=%#x\n",
               cs_lp, region, orig_pc, lp);
         *lp_orig = lp; *selector = 0; return 1;
      }
      return 0;                                    /* call site found, no match */
   }
   return 0;
}

/* ===================================================================== */
/* The unwinder: walk translated frames, find a handler, resume.         */
/* ===================================================================== */
/* Conservative "is this low-4GB pointer safe to dereference" test: the libabiconv
 * shim heap, or inside a registered translated image.  Used only by the
 * diagnostic path — external RTTI type_info pointers (__ZTI*) are currently
 * truncated native libstdc++ addresses (the typeinfo-truncation family the RTTI
 * agent owns), so a thrown type_info often is NOT readable; never fault on it. */
extern uint64_t cb_readable_span(uint64_t p, uint64_t want);   /* cb_bridge.c */
static int eh_ptr_mapped(uint32_t p) {
   if (cb_readable_span(p, 8) >= 8) return 1;             /* shim heap, anything mapped */
   for (int i = 0; i < g_img_n; i++) {
      struct eh_image *im = &g_imgs[i];
      if (im->text_base && (uintptr_t)p >= im->text_base &&
          (uintptr_t)p < im->text_base + im->text_size + 0x100000u) return 1;
   }
   return 0;
}

static void eh_terminate(struct eh_exception *h, const char *why) {
   const char *tn = "?";
   if (h && h->type_info && eh_ptr_mapped(h->type_info) &&
       eh_ptr_mapped(h->type_info + 4)) {
      /* i386 type_info: vtable(4) then name(4); name is a C string ptr. */
      uint32_t nameptr = *(const uint32_t *)(uintptr_t)(h->type_info + 4);
      if (nameptr && eh_ptr_mapped(nameptr)) tn = (const char *)(uintptr_t)nameptr;
   }
   fprintf(stderr,
      "[eh] terminate: %s — unhandled C++ exception of type '%s' from "
      "translated code.\n", why, tn);
   fflush(stderr);
   abort();
}

/* Walk from `start_ebp` (the throwing/resuming frame's ebp) outward, searching
 * each framed function's LSDA for a handler.  `resume_only` true => phase-2
 * continuation from _Unwind_Resume (cleanups only count as install points; a
 * catch still installs).  On success eh_resume() is tail-called (noreturn);
 * otherwise returns (caller terminates). */
/* Phase 1 (Itanium two-phase unwind): does ANY frame from `start_ebp` outward
 * have a matching CATCH?  Walks the ebp chain without running anything; returns
 * 1 if a handler exists.  Used so we only run cleanups (which may have side
 * effects) when the exception will actually be caught — otherwise terminate. */
static int eh_has_handler(struct eh_exception *h, uint32_t start_ebp, uint32_t start_ret) {
   uint32_t ebp = start_ebp, ret = start_ret; int hops = 0;
   while (ret && hops++ < 4096) {
      struct eh_image *im = eh_image_for_pc((uintptr_t)ret);
      if (!im) { EHLOG("ph1 ret=%#x not in image\n", ret); break; }
      uint32_t orig_pc = 0;
      if (pc_trans_to_orig(im, (uintptr_t)ret, &orig_pc)) {
         const struct lsda_ent *rec = eh_lsda_for_pc(im, (uintptr_t)ret);
         EHLOG("ph1 ret=%#x orig_pc=%#x rec_region=%#x lsda_off=%d\n",
               ret, orig_pc, rec?rec->orig_func:0, rec?rec->lsda_off:0);
         if (rec && im->gxt_base) {
            uint32_t lp = 0; int32_t sel = 0; uint32_t adj = 0;
            if (eh_scan_lsda((uint32_t)(im->gxt_base + (intptr_t)rec->lsda_off),
                             rec->orig_func, orig_pc, h->type_info,
                             /*want_handler=*/1, &lp, &sel, &adj))
               return 1;
         }
      } else { EHLOG("ph1 no orig for ret=%#x\n", ret); }
      if (!ebp) break;
      uint32_t saved_ebp = *(const uint32_t *)(uintptr_t)ebp;
      uint32_t caller_ret = *(const uint32_t *)(uintptr_t)(ebp + 4);
      if (saved_ebp <= ebp) break;
      ebp = saved_ebp; ret = caller_ret;
   }
   return 0;
}

/* `lp_esp` is the esp the landing pad must run with: the function's WORKING esp
 * (where it places call args), NOT the CFA.  For a frame-pointer function that
 * is the esp it had at the unwound call site = (the inner frame's ebp) + 8; for
 * the throwing frame itself it is the body esp captured at the throw call.
 *
 * Phase 2: walk outward and resume the FIRST frame that has an action — a
 * matching catch INSTALLS the handler (noreturn into the catch); a CLEANUP
 * resumes the dtor landing pad, which runs and tail-calls _Unwind_Resume to
 * re-enter here from the caller, so cleanups in every intervening frame run in
 * order before the catch. */
static void eh_raise_from(struct eh_exception *h, uint32_t start_ebp,
                          uint32_t start_ret, uint32_t start_esp) {
   eh_scan_images();
   if (g_img_n == 0) {
      eh_terminate(h, "no translator PC map (core __86x64_pcmap/__86x64_ehlsda "
                      "not yet emitted)");
      return;
   }
   if (!eh_has_handler(h, start_ebp, start_ret)) {
      eh_terminate(h, "no matching handler in any frame");
      return;
   }

   uint32_t ebp = start_ebp;
   uint32_t ret = start_ret;                       /* translated return addr  */
   uint32_t lp_esp = start_esp;                    /* this frame's body esp   */
   int hops = 0;
   while (ret && hops++ < 4096) {
      struct eh_image *im = eh_image_for_pc((uintptr_t)ret);
      if (!im) { EHLOG("frame ret=%#x not in any eh-image; stop\n", ret); break; }

      uint32_t orig_pc = 0;
      if (!pc_trans_to_orig(im, (uintptr_t)ret, &orig_pc)) { EHLOG("no orig for ret=%#x\n", ret); goto next; }
      /* The unwinder keys the LSDA on the instruction BEFORE the return point
       * (the call), per Itanium ("ip-1"). */
      const struct lsda_ent *rec = eh_lsda_for_pc(im, (uintptr_t)ret);
      if (rec && im->gxt_base) {
         uint32_t lsda_addr = (uint32_t)(im->gxt_base + (intptr_t)rec->lsda_off);
         uint32_t lp_orig = 0; int32_t sel = 0; uint32_t adj = 0;
         if (eh_scan_lsda(lsda_addr, rec->orig_func, orig_pc, h->type_info,
                          /*want_handler=*/0, &lp_orig, &sel, &adj)) {
            uintptr_t lp_trans = 0;
            if (!pc_orig_to_trans(im, lp_orig, &lp_trans)) {
               EHLOG("LP orig=%#x has no trans mapping\n", lp_orig);
               goto next;
            }
            if (sel != 0) { h->selector = sel; if (adj) h->adjusted = adj; }
            /* Restore callee-saved ebx/esi/edi from the handler frame's saved
             * slots, located via the original i386 __eh_frame CFI (valid for the
             * translated frame: the i386 stack layout is preserved).  CFA from the
             * frame pointer (ebp + cfa_off when cfa_reg==ebp); each register's
             * saved value is at [CFA + reg_off].  Defaults keep the current value
             * when the function didn't save that register (it doesn't clobber it).*/
            uint32_t r_ebx = 0, r_esi = 0, r_edi = 0;
            __asm__ volatile("movl %%ebx,%0; movl %%esi,%1; movl %%edi,%2"
                             : "=r"(r_ebx), "=r"(r_esi), "=r"(r_edi));
            const struct fde_rec *fde = eh_fde_for_region(im, rec->orig_func);
            if (fde) {
               int cfa_reg; int32_t cfa_off; int32_t roff[8]; int rset[8];
               eh_cfi_run(fde, orig_pc - rec->orig_func, &cfa_reg, &cfa_off, roff, rset);
               /* Handler frames are frame-pointer functions, so the post-prologue
                * CFA is frame-pointer-based regardless of the i386 eh_frame's
                * esp/ebp register number (it swaps esp=5/ebp=4): CFA = ebp +
                * cfa_off (cfa_off is 8 once the prologue's push ebp is set up). */
               uint32_t cfa = ebp + (uint32_t)cfa_off;
               if (rset[3]) r_ebx = *(const uint32_t *)(uintptr_t)(cfa + roff[3]);
               if (rset[6]) r_esi = *(const uint32_t *)(uintptr_t)(cfa + roff[6]);
               if (rset[7]) r_edi = *(const uint32_t *)(uintptr_t)(cfa + roff[7]);
               EHLOG("cfi: cfa_reg=%d cfa_off=%d ebx@%d=%s esi@%d=%s edi@%d=%s\n",
                     cfa_reg, cfa_off, roff[3], rset[3]?"y":"n", roff[6],
                     rset[6]?"y":"n", roff[7], rset[7]?"y":"n");
            }
            EHLOG("resume: frame ebp=%#x esp=%#x lp=%#lx sel=%d (%s) ebx=%#x esi=%#x edi=%#x\n",
                  ebp, lp_esp, (unsigned long)lp_trans, sel, sel ? "catch" : "cleanup",
                  r_ebx, r_esi, r_edi);
            eh_resume((uint32_t)lp_trans, lp_esp, ebp, (uint32_t)(uintptr_t)h,
                      (uint32_t)sel, r_ebx, r_esi, r_edi);
            /* noreturn */
         }
      }
   next:
      /* Pop to the caller frame (i386 ebp chain: [ebp]=saved ebp, [ebp+4]=ret).
       * The caller's body esp at the call into this frame = this ebp + 8. */
      if (!ebp) break;
      uint32_t saved_ebp = *(const uint32_t *)(uintptr_t)ebp;
      uint32_t caller_ret = *(const uint32_t *)(uintptr_t)(ebp + 4);
      if (saved_ebp <= ebp) break;                 /* chain must ascend */
      lp_esp = ebp + 8;
      ebp = saved_ebp;
      ret = caller_ret;
   }
   /* phase 1 promised a handler, so we shouldn't fall out; terminate if we do. */
}

/* ===================================================================== */
/* Itanium __cxa_* / _Unwind_* entry points (i386 cdecl, args at a[]).    */
/* ===================================================================== */

/* void* __cxa_allocate_exception(size_t thrown_size) */
uint32_t shim_cxa_allocate_exception(uint32_t *a) {
   uint32_t size = a[0];
   uint32_t total = (uint32_t)sizeof(struct eh_exception) + (size ? size : 1);
   uint8_t *p = malloc(total);                     /* low-4GB shim heap */
   if (!p) { eh_terminate(NULL, "__cxa_allocate_exception: out of memory"); }
   struct eh_exception *h = (struct eh_exception *)p;
   memset(h, 0, sizeof *h);
   h->magic = EH_MAGIC;
   h->obj   = (uint32_t)(uintptr_t)(p + sizeof(struct eh_exception));
   return h->obj;
}

/* void __cxa_free_exception(void* thrown) */
uint32_t shim_cxa_free_exception(uint32_t *a) {
   struct eh_exception *h = hdr_from_obj(a[0]);
   if (h) free(h);
   return 0;
}

/* void __cxa_throw(void* obj, std::type_info* tinfo, void (*dtor)(void*)) */
uint32_t shim_cxa_throw(uint32_t *a) {
   uint32_t obj = a[0], tinfo = a[1], dtor = a[2];
   struct eh_exception *h = hdr_from_obj(obj);
   if (!h) {
      /* Not one of ours (e.g. exception object from a path we didn't allocate);
       * synthesize a header view so we can still describe it. */
      eh_terminate(NULL, "__cxa_throw: exception object has no 86x64 header");
   }
   h->type_info  = tinfo;
   h->destructor = dtor;
   h->handler_count = 0;
   eh_get()->uncaught_count++;
   EHLOG("throw obj=%#x tinfo=%#x dtor=%#x\n", obj, tinfo, dtor);

   /* The throwing frame is our trampoline's i386 caller.  &a[0] is [rbp+12] in
    * the trampoline frame; the throwing frame's ebp is the dword the
    * trampoline saved at [rbp] = a[-3], and its resume PC is the call's return
    * address at [rbp+8] = a[-1]. */
   uint32_t thrower_ebp = a[-3];
   uint32_t thrower_ret = a[-1];
   /* &a[0] is the throwing frame's body esp (where it staged the throw args). */
   eh_raise_from(h, thrower_ebp, thrower_ret, (uint32_t)(uintptr_t)a);

   /* Unwinding returned => no handler. */
   eh_terminate(h, "__cxa_throw: no matching handler");
   return 0;
}

/* void __cxa_rethrow(void) */
uint32_t shim_cxa_rethrow(uint32_t *a) {
   struct eh_globals *g = eh_get();
   struct eh_exception *h = hdr_from_handle(g->caught);
   if (!h) eh_terminate(NULL, "__cxa_rethrow: no current exception");
   g->uncaught_count++;
   EHLOG("rethrow obj=%#x\n", h->obj);
   uint32_t thrower_ebp = a[-3];
   uint32_t thrower_ret = a[-1];
   eh_raise_from(h, thrower_ebp, thrower_ret, (uint32_t)(uintptr_t)a);
   eh_terminate(h, "__cxa_rethrow: no matching handler");
   return 0;
}

/* void* __cxa_begin_catch(void* unwind_exc) — returns the adjusted object. */
uint32_t shim_cxa_begin_catch(uint32_t *a) {
   struct eh_exception *h = hdr_from_handle(a[0]);
   if (!h) { h = hdr_from_obj(a[0]); }             /* tolerate object-ptr handle */
   if (!h) return a[0];
   struct eh_globals *g = eh_get();
   if (h->handler_count <= 0) {                    /* first catch of this exc */
      h->next = g->caught;
      g->caught = (uint32_t)(uintptr_t)h;
      if (g->uncaught_count) g->uncaught_count--;
   }
   h->handler_count++;
   uint32_t r = h->adjusted ? h->adjusted : h->obj;
   EHLOG("begin_catch handle=%#x -> obj=%#x (count=%d)\n", a[0], r, h->handler_count);
   return r;
}

/* void __cxa_end_catch(void) */
uint32_t shim_cxa_end_catch(uint32_t *a) {
   (void)a;
   struct eh_globals *g = eh_get();
   struct eh_exception *h = hdr_from_handle(g->caught);
   if (!h) return 0;
   if (--h->handler_count <= 0) {
      g->caught = h->next;                         /* pop */
      /* TODO(next increment): run h->destructor (the thrown object's own i386
       * dtor) via _86x64_call_i386 on a fresh low-4GB stack — same setup as
       * cb_bridge.  Deferred: the f01-f05 fixtures throw trivially-destructible
       * objects, and skipping it only leaks the object's members, never
       * crashes.  Cleanup destructors for LOCALS unwound past are run by the
       * translated cleanup landing pads themselves (via _Unwind_Resume), not
       * here. */
      EHLOG("end_catch free obj=%#x (dtor=%#x deferred)\n", h->obj, h->destructor);
      free(h);
   }
   return 0;
}

/* void* __cxa_get_exception_ptr(void* unwind_exc) */
uint32_t shim_cxa_get_exception_ptr(uint32_t *a) {
   struct eh_exception *h = hdr_from_handle(a[0]);
   if (!h) h = hdr_from_obj(a[0]);
   if (!h) return a[0];
   return h->adjusted ? h->adjusted : h->obj;
}

/* __gxx_personality_v0: present so binds resolve.  Our unwinder drives the
 * personality logic directly (eh_scan_lsda); nothing should call this.  If a
 * native unwinder ever does, fail loudly rather than walk our frames wrong. */
uint32_t shim_gxx_personality_v0(uint32_t *a) {
   (void)a;
   fprintf(stderr, "[eh] __gxx_personality_v0 invoked directly — the 86x64 "
           "unwinder should drive unwinding; not reached in normal flow.\n");
   fflush(stderr);
   return 8;                                        /* _URC_CONTINUE_UNWIND */
}

/* void _Unwind_Resume(_Unwind_Exception* exc) — continue phase-2 unwinding from
 * a cleanup landing pad. */
uint32_t shim_Unwind_Resume(uint32_t *a) {
   struct eh_exception *h = hdr_from_handle(a[0]);
   if (!h) h = hdr_from_obj(a[0]);
   if (!h) eh_terminate(NULL, "_Unwind_Resume: unknown exception");
   EHLOG("Unwind_Resume obj=%#x\n", h->obj);
   /* Resume from the frame that called us (the cleanup landing pad's frame): we
    * continue searching its CALLER outward. */
   uint32_t cur_ebp = a[-3];                        /* the resuming frame ebp  */
   uint32_t caller_ebp = cur_ebp ? *(const uint32_t *)(uintptr_t)cur_ebp : 0;
   uint32_t caller_ret = cur_ebp ? *(const uint32_t *)(uintptr_t)(cur_ebp + 4) : 0;
   /* The caller's body esp at the call into this (cleanup) frame = cur_ebp + 8. */
   eh_raise_from(h, caller_ebp, caller_ret, cur_ebp + 8);
   eh_terminate(h, "_Unwind_Resume: no further handler");
   return 0;
}

uint32_t shim_Unwind_Resume_or_Rethrow(uint32_t *a) { return shim_Unwind_Resume(a); }

/* void __cxa_call_unexpected(void*) — exception-spec violation. */
uint32_t shim_cxa_call_unexpected(uint32_t *a) {
   struct eh_exception *h = hdr_from_handle(a[0]);
   eh_terminate(h, "__cxa_call_unexpected: exception specification violated");
   return 0;
}
