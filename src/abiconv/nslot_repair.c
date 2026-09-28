/* nslot_repair.c — load-time repair of abigen's capture-proof native-call
 * slots. ONE JOB: guarantee the invariant that a marshalling shim's inner
 * `call native_target(S)` NEVER lands in another marshalling shim.
 *
 * THE BUG THIS CLOSES (Civ IV "XML Load Error" root cause): abigen exports
 * every shim as "__"+S, so when the consider set holds both a public libc
 * function (_tolower) and its internal underscore twin (___tolower = C
 * __tolower, called directly by old ctype.h inlines), the twin shim
 * _____tolower's inner `call ___tolower` binds at LINK time to the SIBLING
 * SHIM ___tolower (the shim for public tolower) instead of libSystem. The
 * sibling shim then re-runs its i386 arg marshalling on a NATIVE 8-byte call
 * frame ([rbp+0xc] = high half of the 64-bit return address = 0), so
 * __tolower/__toupper return 0 for EVERY char -> Civ's case-insensitive
 * CompareNoCase("xml\\") mis-branches -> the Carbon XML Load Error modal.
 * The capture crossed the two abigen passes (modern abiconv.asm defined
 * ___tolower; legacy abiconv_legacy.asm emitted _____tolower), so no single
 * pass could see it at generation time — hence this LOAD-time repair.
 *
 * MECHANISM: abigen routes every inner call whose target has >= 3 leading
 * underscores (the exact universe of possible shim names, since a shim name
 * is "__"+"_...") through a data slot statically initialized `dq <target>` —
 * i.e. link-time-bound to the SAME definition the direct call used, so
 * before this constructor runs (and on any repair failure) behavior is
 * byte-for-byte the old behavior. Each abigen pass emits a {name, slot}
 * table; this constructor walks both tables and, ONLY when a slot's current
 * value structurally fingerprints as a marshalling shim (the
 * `cmp qword [rel __dyld_stub_binder_flag], 0` entry sequence every
 * abigen/hand shim starts with), re-points it at the real native definition.
 *
 * WHAT IS DELIBERATELY LEFT ALONE:
 *  - slots already bound outside this image (normal case: dyld bound the
 *    undefined `dq` reference to libSystem/CoreFoundation) — no fingerprint;
 *  - INTENTIONAL native implementations inside libabiconv that generated
 *    shims inner-call on purpose (file_shim.c's __srget/__swbuf, and any
 *    future functionality-first reimplementation): native C code does not
 *    carry the shim entry fingerprint;
 *  - slots whose capture cannot be re-resolved anywhere real (a genuinely
 *    removed symbol): loudly logged, old behavior kept.
 *
 * RESOLUTION ORDER (multi-copy-deploy safe — see the libabiconv multi-copy gotcha;
 * RTLD_NEXT would happily re-chain the double-wrap through a SIBLING
 * libabiconv copy's shim): explicit libSystem handle, explicit CoreFoundation
 * handle, then RTLD_DEFAULT with every candidate validated to (a) not live in
 * this or any same-named (libabiconv) image and (b) not fingerprint as a
 * marshalling shim itself. */

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
   const char *name; /* asm-level target name, e.g. "___tolower" */
   void **slot;      /* the shim's indirect-call slot */
} abic_nslot_ent;

/* Strong definitions come from the generated .asm footers
 * (ABIGenerator::emit_native_slot_table): tab0 = primary/modern pass,
 * tab1 = secondary/legacy pass. These weak zero fallbacks keep any link
 * that lacks one of the .asm files (e.g. no 10.6 SDK -> no legacy pass)
 * working unchanged. */
__attribute__((weak)) abic_nslot_ent _abiconv_nslot_tab0[1] = {{0, 0}};
__attribute__((weak)) uint64_t _abiconv_nslot_tab0_n = 0;
__attribute__((weak)) abic_nslot_ent _abiconv_nslot_tab1[1] = {{0, 0}};
__attribute__((weak)) uint64_t _abiconv_nslot_tab1_n = 0;

/* dyld_stub_binder.asm's global (asm __dyld_stub_binder_flag): the anchor of
 * the shim-entry fingerprint below. */
extern uint64_t _dyld_stub_binder_flag;

static const void *abic_own_fbase;
static const char *abic_own_bname; /* basename of this libabiconv image */

/* Structural fingerprint of a marshalling-shim ENTRY: every abigen-generated
 * (and hand MTSHIM-style) shim begins with
 *    cmp qword [rel __dyld_stub_binder_flag], 0
 * encoded 48 83 3D <disp32> 00 (REX.W 83 /7 ib), and the rip-relative
 * displacement must resolve to THIS image's __dyld_stub_binder_flag. A native
 * C implementation never starts with this exact instruction aimed at our
 * flag, and a sibling libabiconv copy's shim aims at ITS OWN flag — so this
 * identifies precisely "an i386-entry marshalling shim of THIS image", the
 * one thing an inner (already-marshalled, native) call must never reach. */
static int abic_is_marshal_shim(const void *p) {
   const uint8_t *b = (const uint8_t *)p;
   int32_t disp;
   if (b == 0) {
      return 0;
   }
   if (b[0] != 0x48 || b[1] != 0x83 || b[2] != 0x3D || b[7] != 0x00) {
      return 0;
   }
   memcpy(&disp, b + 3, sizeof(disp));
   return b + 8 + disp == (const uint8_t *)&_dyld_stub_binder_flag;
}

static const char *abic_basename(const char *path) {
   const char *s = path == 0 ? 0 : strrchr(path, '/');
   return s != 0 ? s + 1 : path;
}

/* Reject any candidate living in this image or in any image with the same
 * basename (a sibling libabiconv copy under Civ-style multi-copy deploys). */
static int abic_in_abiconv_image(const void *p) {
   Dl_info di;
   if (dladdr(p, &di) == 0) {
      return 1; /* unattributable -> treat as unsafe */
   }
   if (di.dli_fbase == abic_own_fbase) {
      return 1;
   }
   if (abic_own_bname != 0 && di.dli_fname != 0 &&
       strcmp(abic_basename(di.dli_fname), abic_own_bname) == 0) {
      return 1;
   }
   return 0;
}

static void *abic_resolve_native(const char *asm_name) {
   /* asm -> dlsym name: strip the single leading underscore the C ABI adds
    * (asm ___tolower -> dlsym "__tolower"). */
   const char *dl = asm_name[0] == '_' ? asm_name + 1 : asm_name;
   static void *handles[2];
   static int inited = 0;
   void *cand;
   int i;
   if (!inited) {
      inited = 1;
      handles[0] = dlopen("/usr/lib/libSystem.B.dylib",
                          RTLD_LAZY | RTLD_NOLOAD);
      handles[1] = dlopen("/System/Library/Frameworks/CoreFoundation.framework"
                          "/Versions/A/CoreFoundation",
                          RTLD_LAZY | RTLD_NOLOAD);
   }
   for (i = 0; i < 3; ++i) {
      if (i < 2) {
         if (handles[i] == 0) {
            continue;
         }
         cand = dlsym(handles[i], dl);
      } else {
         cand = dlsym(RTLD_DEFAULT, dl);
      }
      if (cand == 0) {
         continue;
      }
      if (abic_in_abiconv_image(cand)) {
         continue;
      }
      if (abic_is_marshal_shim(cand)) {
         continue;
      }
      return cand;
   }
   return 0;
}

static void abic_repair_table(const abic_nslot_ent *tab, uint64_t n) {
   uint64_t i;
   for (i = 0; i < n; ++i) {
      const char *nm = tab[i].name;
      void **slot = tab[i].slot;
      void *cur;
      void *fix;
      if (nm == 0 || slot == 0) {
         continue;
      }
      cur = *slot;
      if (!abic_is_marshal_shim(cur)) {
         continue;
      }
      fix = abic_resolve_native(nm);
      if (fix != 0) {
         *slot = fix;
      } else {
         /* Old behavior kept (double-marshal) — but now diagnosable. */
         fprintf(stderr, "abiconv nslot: WARNING: %s is captured by a "
                         "marshalling shim (%p) and no native definition was "
                         "found; leaving as-is\n",
                 nm, cur);
      }
   }
}

/* TIMING: dyld runs this before any translated code can reach a shim —
 * libabiconv is a dependency of every translated image, so its initializers
 * run before the app's mod_init_funcs (and shims are only entered from
 * translated i386 code). Within libabiconv, other constructors call ctype/
 * libc NATIVELY (compiled C binds straight to libSystem, never through a
 * marshalling shim), so they cannot double-wrap either way; priority 101
 * (the lowest non-reserved, ahead of all default-priority constructors in
 * this image) still removes the whole ordering question structurally. And
 * the fallback is benign by construction: an (impossible) pre-repair call
 * would get the slot's static link-time binding = exactly the OLD behavior. */
__attribute__((constructor(101))) static void abic_nslot_repair_init(void) {
   Dl_info di;
   if (dladdr((void *)(uintptr_t)&abic_nslot_repair_init, &di) != 0) {
      abic_own_fbase = di.dli_fbase;
      abic_own_bname = abic_basename(di.dli_fname);
   }
   abic_repair_table(_abiconv_nslot_tab0, _abiconv_nslot_tab0_n);
   abic_repair_table(_abiconv_nslot_tab1, _abiconv_nslot_tab1_n);
}
