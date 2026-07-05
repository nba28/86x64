// carbon_fwd_shim.c — i386-frame FORWARD-to-native marshalling shims for PRESENT
// but header-gated Carbon symbols that translated QuickTime.framework calls
// (Civ IV / Halo s34).
//
// Distinct from qt_hitoolbox_shim.c (which NO-OPs REMOVED symbols): these
// symbols still EXIST in the modern 64-bit dyld namespace (dlsym RTLD_DEFAULT
// resolves them — classic Resource Manager / QuickDraw / File Manager entries
// that survive in CarbonCore/ApplicationServices), but their SDK headers gate
// the declaration behind `#if !__LP64__`, so abigen (x86_64 parse) never sees
// them and cannot emit a marshalling shim. static-interpose therefore leaves
// QuickTime's bind NATIVE, and the translated i386 caller's 4-byte-return cdecl
// frame is 8-byte over-popped by the native callee's `ret` -> fused/garbage PC
// (the s28 ABI family; first hit: _RMOpenResourceFileRef from QuickTime's
// OpenWarholForkMapped during movie/resource init).
//
// Since these are PRESENT, a NO-OP would lose real behavior; instead FORWARD to
// the genuine native function with i386->x86_64 ABI conversion: widen the i386
// cdecl 4-byte arg slots to native 64-bit args (a low-4GB pointer zero-extends
// to a valid native pointer; a small int/enum passes as its value) and return
// the native result truncated into eax. The native fn is resolved once via
// dlsym(RTLD_DEFAULT, "<Name>") and cached. arg COUNT comes from the caller's
// cdecl `add $N,%rsp` stack cleanup (N/4 args).
//
// MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0]; uint32_t in eax.
// Wired ___<Name> -> _shim_<Name>. Reached either by static-interpose (on a
// retranslate, once ___<Name> exists) OR at runtime by import_repair, which
// redirects a translated image's native-bound slot to the shim when one exists.
//
// UNIVERSAL: any translated i386 program calling a present-but-__LP64__-gated
// Carbon entry benefits. Add a symbol here (+ its MTSHIM line) as it surfaces on
// a boot path — abigen cannot auto-cover them, so this is the hand-written
// forward-marshalling family, extended by the same iterate loop as s29->s32.

#include <stdint.h>
#include <dlfcn.h>

// Forward N i386 cdecl arg slots (widened) to native <name>, return eax.
// A pointer arg is a low-4GB i386 address that zero-extends to a valid native
// pointer; native writes any out-parameter back through it (the i386 buffer is
// mapped low-4GB). A default result is returned if the symbol vanished.
#define FWD_DEFAULT_ERR ((uint32_t)-192)   /* resNotFound-ish, benign */

#define FWD4(NAME)                                                            \
   uint32_t shim_##NAME(uint32_t *a) {                                        \
      static long (*fn)(long, long, long, long);                             \
      if (!fn) { fn = (long (*)(long, long, long, long))                      \
                        dlsym(RTLD_DEFAULT, #NAME); }                         \
      if (!fn) { return FWD_DEFAULT_ERR; }                                    \
      return (uint32_t)fn((long)a[0], (long)a[1], (long)a[2], (long)a[3]);    \
   }

// ---- Resource Manager (CarbonCore; header-gated, present) ----
// OSErr RMOpenResourceFileRef(const FSRef*, HFSUniStr255*, SInt8 perm,
//                             FSIORefNum* refNum) — 4 args (caller add $0x10).
// QuickTime's OpenWarholForkMapped/HereIsWarholResFile use it to open QuickTime's
// own legacy component RESOURCE fork; forwarding to native sends QuickTime down
// that classic Resource-Manager reader, which faults in the modern environment
// (no such fork on the translated framework). Return fnfErr(-43) so the caller
// takes its "no resource file" path and SKIPS legacy resource loading — a path
// Civ/Halo never need (they don't play movies). Promote to a real FWD4 forward
// if a target genuinely needs QuickTime component resources.
uint32_t shim_RMOpenResourceFileRef(uint32_t *a) { (void)a; return (uint32_t)-43; }
