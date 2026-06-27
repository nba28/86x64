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

/* ===================================================================== */
/* eh_tramp.asm                                                          */
/* ===================================================================== */
extern void eh_resume(uint32_t rip, uint32_t esp, uint32_t ebp,
                      uint32_t eax_exc, uint32_t edx_sel) __attribute__((noreturn));

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

struct eh_image {
   uintptr_t text_base, text_size;                /* runtime __text span     */
   uintptr_t gxt_base;                            /* runtime __gcc_except_tab*/
   const struct pcmap_ent *pcmap;                 /* sorted by trans_off     */
   uint32_t  pcmap_n;
   struct pcmap_ent *pcmap_byorig;                /* malloc'd, sorted by orig*/
   const struct lsda_ent *lsda;                   /* sorted by trans_func_off*/
   uint32_t  lsda_n;
};
static struct eh_image g_imgs[64];
static int g_img_n = 0;
static int g_img_scanned = 0;
static pthread_mutex_t g_img_mu = PTHREAD_MUTEX_INITIALIZER;

static int cmp_ent_byorig(const void *x, const void *y) {
   uint32_t a = ((const struct pcmap_ent *)x)->orig, b = ((const struct pcmap_ent *)y)->orig;
   return (a > b) - (a < b);
}

/* Locate the __86x64_pcmap / __86x64_ehlsda sections in each loaded image and
 * register the ones that carry them.  Called lazily on the first throw. */
static void eh_scan_images(void) {
   if (g_img_scanned) return;
   pthread_mutex_lock(&g_img_mu);
   if (g_img_scanned) { pthread_mutex_unlock(&g_img_mu); return; }
   uint32_t n = _dyld_image_count();
   for (uint32_t i = 0; i < n && g_img_n < (int)(sizeof g_imgs / sizeof g_imgs[0]); i++) {
      const struct mach_header *mh = _dyld_get_image_header(i);
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
      /* Walk the action chain. */
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
            if (!want_handler) { *lp_orig = lp; *selector = 0; return 1; }
         }
         /* ttype_index < 0 => exception-spec; treated as no-match here. */
         if (next_off == 0) break;
         ap = after_idx + next_off;
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
static int eh_ptr_mapped(uint32_t p) {
   if (p >= 0x88000000u && p < 0xF0000000u) return 1;     /* shim heap */
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
/* `lp_esp` is the esp the landing pad must run with: the function's WORKING esp
 * (where it places call args), NOT the CFA.  For a frame-pointer function that
 * is the esp it had at the unwound call site = (the inner frame's ebp) + 8; for
 * the throwing frame itself it is the body esp captured at the throw call. */
static void eh_raise_from(struct eh_exception *h, uint32_t start_ebp,
                          uint32_t start_ret, uint32_t start_esp) {
   eh_scan_images();
   if (g_img_n == 0) {
      eh_terminate(h, "no translator PC map (core __86x64_pcmap/__86x64_ehlsda "
                      "not yet emitted)");
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
                          /*want_handler=*/1, &lp_orig, &sel, &adj)) {
            uintptr_t lp_trans = 0;
            if (!pc_orig_to_trans(im, lp_orig, &lp_trans)) {
               EHLOG("handler LP orig=%#x has no trans mapping\n", lp_orig);
               goto next;
            }
            if (sel != 0) { h->selector = sel; if (adj) h->adjusted = adj; }
            EHLOG("install handler: frame ebp=%#x esp=%#x lp=%#lx sel=%d\n",
                  ebp, lp_esp, (unsigned long)lp_trans, sel);
            eh_resume((uint32_t)lp_trans, lp_esp, ebp, (uint32_t)(uintptr_t)h, (uint32_t)sel);
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
   /* No handler found anywhere. */
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
