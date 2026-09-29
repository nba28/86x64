/*
 * import_repair.c — eager repair of never-bound indirect-pointer slots in
 * TRANSLATED images (the half-wired-artifact family; Civ IV s29).
 *
 * A macho-tool-translated dylib relies on two cooperating binding layers:
 * route-C's synthesized LC_DYLD_INFO (dyld eagerly binds the unshimmed
 * native imports — dyld4 has no runtime lazy binding) and the
 * static-interpose wiring that renames shim-covered imports to libabiconv
 * `__X` marshalling shims and adds the libabiconv dependency. Artifacts
 * from before that second layer exist in deployed bundles (Civ IV's
 * QuickTime.framework was the s29 instance): the shim-claimed lazy slots
 * have NO bind entries at all, the stub_helper's dyld_stub_binder GOT slot
 * has no bind (stays 0), the per-stub lazy-info push-immediates are stale
 * relative to the synthesized blob, and there is no libabiconv dependency
 * to resolve against. Consequences at runtime:
 *
 *   - first call through any unbound lazy slot: stub -> stub_helper ->
 *     jmp *dyld_stub_binder-slot == jmp *0 -> rip=0. (Civ IV s29
 *     deterministic variant: QuickTime internalGetPerThreadStorage's first
 *     _pthread_once, crashlog rip=0 with [rsp]=&__nl_symbol_ptr[1] and
 *     [rsp+8]=stale lazy offset.)
 *   - unbound non-lazy GOT slots read as NULL/garbage data or fn pointers
 *     and feed marshalling shims garbage (the s29 `movl (%r12),%r14d`
 *     variant inside an abigen deep-copy preamble).
 *
 * Repair (structural, universal — triggers on Mach-O shape, never a name):
 * for every loaded MH_DYLIB/MH_BUNDLE whose __TEXT sits at macho-tool's
 * translated layout base (0x10000000; no native dylib is laid out there —
 * natives are 0-based PIE), find indirect-pointer slots that provably
 * missed binding:
 *
 *   - S_LAZY_SYMBOL_POINTERS slots still pointing into the image's own
 *     __stub_helper: dyld4 binds every lazy entry eagerly at load (missing
 *     symbols become NULL), so a post-load stub_helper value can only mean
 *     "no bind entry exists for this slot".
 *   - S_NON_LAZY_SYMBOL_POINTERS slots containing 0 whose indirect symbol
 *     is a genuine undefined import (covers both never-emitted binds and
 *     weak-import binds dyld NULLed for since-removed symbols).
 *
 * Each is resolved exactly as static-interpose would have wired it:
 *   1. libabiconv's `__`+name export first — the i386->x86_64 marshalling
 *      shim for functions, the low-4GB shadow for data symbols. This is the
 *      identical binding a current-pipeline translation would carry.
 *   2. else the native symbol (the same no-shim-native-bind semantics the
 *      pipeline itself uses for uncovered imports).
 *   3. else: lazy slots are left on their stub_helper entry, but the
 *      image's dyld_stub_binder GOT slot is pointed at a loud abort
 *      trampoline — a residual call dies with a message instead of rip=0.
 *      (It must NOT be pointed at libabiconv's __dyld_stub_binder: these
 *      artifacts' stub push-immediates are stale, so a runtime lazy bind
 *      would resolve the WRONG entry and silently mis-bind.)
 *
 * Idempotent: repaired slots no longer satisfy the unbound predicates.
 * Runs from the add-image callback (objc_slide.c) before the image's
 * translated initializers execute; inert for every native image (layout
 * base gate) and for complete current-pipeline translations (no slot
 * satisfies the predicates).
 */

#include <dlfcn.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gap.h"

/* macho-tool's M64 layout base for translated output (Archive::Build). */
#define IR_TRANSLATED_TEXT_BASE 0x10000000ULL

static int ir_verbose(void) {
   static int v = -1;
   if (v < 0) {
      v = getenv("ABICONV_OBJC_SLIDE_VERBOSE") != NULL ||
          getenv("VERBOSE") != NULL;
   }
   return v;
}

/* Landing for a residual stub_helper trip (an unresolved lazy import that
 * was actually called). Reached via `jmp *slot`, i386-style frame above —
 * we never return, so the odd stack shape is irrelevant. */
static void ir_stub_binder_abort(void) {
   fprintf(stderr,
           "abiconv import_repair: translated image called a lazy import "
           "that could not be repaired (see the load-time "
           "ABICONV_OBJC_SLIDE_VERBOSE listing); aborting instead of "
           "jmp *0\n");
   fflush(stderr);
   abort();
}

/* dlopen handle of THIS libabiconv copy (shim/export lookups). Any copy
 * exports the same shim surface; our own is certainly constructed. */
static void *ir_self_handle(void) {
   static void *h = NULL;
   if (h != NULL) { return h; }
   Dl_info info;
   if (!dladdr((void *)&ir_self_handle, &info) || info.dli_fname == NULL) {
      return NULL;
   }
   h = dlopen(info.dli_fname, RTLD_NOLOAD | RTLD_LAZY);
   return h;
}

/* Resolve an undefined import `name` (nlist spelling, leading '_') to the
 * address static-interpose would have bound: the libabiconv `__`+name
 * marshalling shim / data shadow first, else the native symbol. 0 = none. */
static uint64_t ir_resolve(const char *name) {
   if (name == NULL || name[0] == '\0') { return 0; }
   char buf[512];
   size_t len = strlen(name);
   if (len + 2 <= sizeof(buf)) {
      /* shim export symbol is "__"+name; dlsym() prepends the C '_', so the
       * query string is "_"+name (e.g. _pthread_once -> ___pthread_once). */
      buf[0] = '_';
      memcpy(buf + 1, name, len + 1);
      void *self = ir_self_handle();
      if (self != NULL) {
         void *s = dlsym(self, buf);
         if (s != NULL) { return (uint64_t)(uintptr_t)s; }
      }
   }
   /* native fallback — the pipeline's own semantics for shimless imports */
   const char *dl = (name[0] == '_') ? name + 1 : name;
   void *s = dlsym(RTLD_DEFAULT, dl);
   return (uint64_t)(uintptr_t)s;
}

/* libabiconv `__`+name marshalling shim ONLY (no native fallback). Used for the
 * weak-NULL lazy repair: a shim entry handles the i386 4-byte-return frame, but
 * a NATIVE function would over-pop it (s28 ABI), so the weak-NULL path must
 * never fall back to native — it leaves the slot NULL instead (preserving weak-
 * optional semantics for genuinely absent symbols). 0 = no shim. */
static uint64_t ir_resolve_shim_only(const char *name) {
   if (name == NULL || name[0] == '\0') { return 0; }
   char buf[512];
   size_t len = strlen(name);
   if (len + 2 > sizeof(buf)) { return 0; }
   buf[0] = '_';
   memcpy(buf + 1, name, len + 1);
   void *self = ir_self_handle();
   if (self == NULL) { return 0; }
   void *s = dlsym(self, buf);
   return (uint64_t)(uintptr_t)s;
}

void _86x64_import_repair(const struct mach_header_64 *mh64, intptr_t slide,
                          const char *imgname);

void _86x64_import_repair(const struct mach_header_64 *mh64, intptr_t slide,
                          const char *imgname) {
   if (mh64->magic != MH_MAGIC_64) { return; }
   if (mh64->filetype != MH_DYLIB && mh64->filetype != MH_BUNDLE) { return; }

   const struct segment_command_64 *linkedit = NULL;
   const struct symtab_command *symtab = NULL;
   const struct dysymtab_command *dysymtab = NULL;
   uint64_t text_base = 0;
   int have_text = 0;
   uint64_t helper_lo = 0, helper_hi = 0;   /* live __stub_helper range */

   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg =
            (const struct segment_command_64 *)p;
         if (strcmp(seg->segname, "__TEXT") == 0) {
            text_base = seg->vmaddr;
            have_text = 1;
            const struct section_64 *sect =
               (const struct section_64 *)(seg + 1);
            for (uint32_t k = 0; k < seg->nsects; k++, sect++) {
               if (strncmp(sect->sectname, "__stub_helper", 16) == 0) {
                  helper_lo = sect->addr + (uint64_t)slide;
                  helper_hi = helper_lo + sect->size;
               }
            }
         } else if (strcmp(seg->segname, "__LINKEDIT") == 0) {
            linkedit = seg;
         }
      } else if (lc->cmd == LC_SYMTAB) {
         symtab = (const struct symtab_command *)p;
      } else if (lc->cmd == LC_DYSYMTAB) {
         dysymtab = (const struct dysymtab_command *)p;
      }
      p += lc->cmdsize;
   }

   if (!have_text || text_base != IR_TRANSLATED_TEXT_BASE) { return; }
   if (linkedit == NULL || symtab == NULL || dysymtab == NULL ||
       dysymtab->nindirectsyms == 0 || symtab->nsyms == 0) {
      return;
   }

   const uint8_t *lebase = (const uint8_t *)(uintptr_t)
      (linkedit->vmaddr + (uint64_t)slide - linkedit->fileoff);
   const struct nlist_64 *syms =
      (const struct nlist_64 *)(lebase + symtab->symoff);
   const char *strs = (const char *)(lebase + symtab->stroff);
   const uint32_t *ind =
      (const uint32_t *)(lebase + dysymtab->indirectsymoff);

   uint32_t n_unbound = 0, n_shim = 0, n_native = 0, n_missing = 0;

   p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      p += lc->cmdsize;
      if (lc->cmd != LC_SEGMENT_64) { continue; }
      const struct segment_command_64 *seg =
         (const struct segment_command_64 *)lc;
      const struct section_64 *sect = (const struct section_64 *)(seg + 1);
      for (uint32_t si = 0; si < seg->nsects; si++, sect++) {
         const uint32_t stype = sect->flags & SECTION_TYPE;
         const int lazy = (stype == S_LAZY_SYMBOL_POINTERS);
         if (!lazy && stype != S_NON_LAZY_SYMBOL_POINTERS) { continue; }
         if (lazy && helper_hi == 0) { continue; }  /* can't classify */
         uint64_t *slots = (uint64_t *)(uintptr_t)(sect->addr + slide);
         const uint64_t n = sect->size / 8;
         for (uint64_t k = 0; k < n; k++) {
            const uint64_t iidx = (uint64_t)sect->reserved1 + k;
            if (iidx >= dysymtab->nindirectsyms) { break; }
            const uint32_t isym = ind[iidx];
            if (isym & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
               continue;
            }
            if (isym >= symtab->nsyms) { continue; }
            const struct nlist_64 *nl = &syms[isym];
            if ((nl->n_type & N_TYPE) != N_UNDF) { continue; }
            if (nl->n_un.n_strx == 0 || nl->n_un.n_strx >= symtab->strsize) {
               continue;
            }
            const char *name = strs + nl->n_un.n_strx;
            const uint64_t v = slots[k];
            if (lazy) {
               if (v >= helper_lo && v < helper_hi) {
                  /* still points into __stub_helper: never bound (half-wired
                   * artifact) -> full repair below (shim or native). */
               } else if (v == 0) {
                  /* WEAK bind that dyld resolved to NULL — a removed symbol
                   * (e.g. translated QuickTime's __InitHLTB from HIToolbox,
                   * removed from modern macOS). Redirect to a libabiconv `__`+
                   * name marshalling shim ONLY when one exists: the shim's
                   * MTSHIM trampoline handles the i386 4-byte-return frame,
                   * whereas a NATIVE function would 8-byte over-pop it (s28 ABI
                   * family). No shim -> leave the legit weak-optional NULL. This
                   * closes the gap where a properly-wired route-C image weak-
                   * NULLs a removed import that libabiconv actually covers. */
                  const uint64_t ws = ir_resolve_shim_only(name);
                  if (ws != 0) { slots[k] = ws; n_unbound++; n_shim++; }
                  else { x64_gap_arm_missing_slot(name, &slots[k]); }
                  continue;
               } else {
                  /* GENUINELY BOUND — but to WHAT? A translated image's calls
                   * use the i386 4-byte-return cdecl frame; a slot dyld bound to
                   * a NATIVE function makes the callee's 8-byte `ret` over-pop
                   * that frame (the s28 ABI family; QuickTime's header-gated
                   * QuickDraw/Resource-Mgr/Nav present imports that static-
                   * interpose could not rewrite because libabiconv had no shim
                   * for them yet — e.g. _RMOpenResourceFileRef from OpenWarhol-
                   * ForkMapped). If a libabiconv marshalling shim EXISTS for the
                   * symbol, redirect the slot to it: the shim enters the i386
                   * frame correctly and forwards to native with ABI conversion.
                   * Skip if the slot already points into libabiconv (already
                   * marshalled by static-interpose) or no shim exists (leave the
                   * native bind; a genuinely-called one surfaces + gets a shim,
                   * same iterate loop). This makes import_repair a runtime
                   * marshalling safety net independent of static-interpose. */
                  Dl_info cur;
                  if (dladdr((void *)(uintptr_t)v, &cur) &&
                      cur.dli_fname != NULL &&
                      strstr(cur.dli_fname, "libabiconv") != NULL) {
                     continue;   /* already a libabiconv shim */
                  }
                  const uint64_t sh = ir_resolve_shim_only(name);
                  if (sh != 0 && sh != v) { slots[k] = sh; n_unbound++; n_shim++; }
                  continue;
               }
            } else {
               if (v != 0) { continue; }                          /* bound */
               if (strcmp(name, "dyld_stub_binder") == 0) {
                  /* Never wire this to libabiconv's runtime lazy binder:
                   * the artifact's stub push-immediates are stale and would
                   * mis-bind. A loud abort beats jmp *0. */
                  slots[k] = (uint64_t)(uintptr_t)&ir_stub_binder_abort;
                  n_unbound++;
                  continue;
               }
            }
            n_unbound++;
            const uint64_t t = ir_resolve(name);
            if (t != 0) {
               /* shim addresses live in libabiconv; count by provenance */
               Dl_info where;
               slots[k] = t;
               if (dladdr((void *)(uintptr_t)t, &where) &&
                   where.dli_fname != NULL &&
                   strstr(where.dli_fname, "libabiconv") != NULL) {
                  n_shim++;
               } else {
                  n_native++;
               }
            } else {
               n_missing++;
               /* a CALL slot only: a NULL data pointer is a weak-import test */
               if (strncmp(sect->sectname, "__jt_ptrs", 16) == 0) {
                  x64_gap_arm_missing_slot(name, &slots[k]);
               }
               if (ir_verbose()) {
                  fprintf(stderr,
                          "abiconv import_repair: %s: no shim or native "
                          "definition for %s (%s slot %llu)\n",
                          imgname ? imgname : "?", name,
                          lazy ? "lazy" : "non-lazy",
                          (unsigned long long)k);
               }
            }
         }
      }
   }

   if (n_unbound > 0 && ir_verbose()) {
      fprintf(stderr,
              "abiconv import_repair: %s: %u never-bound import slot(s): "
              "%u -> libabiconv shims, %u -> native, %u unresolved\n",
              imgname ? imgname : "?", n_unbound, n_shim, n_native,
              n_missing);
      fflush(stderr);
   }
}
