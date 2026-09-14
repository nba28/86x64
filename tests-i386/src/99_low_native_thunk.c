/* 99_low_native_thunk — a NATIVE function mapped BELOW 4GB must still be bridged.
 *
 * `fnptr_lookup_result` (posix_shim.c) used to hand any sub-4GB dlsym result
 * straight back to the i386 caller, its comment reading "translated/low:
 * callable". That conflates two different questions. "Fits in 32 bits" is a
 * property of where dyld happened to map an image; "speaks the i386 convention"
 * is a property of how the code was built. They coincide for every image the
 * pipeline produces (based at 0x10000000) and for system frameworks (always in
 * the high shared cache) -- and they come apart the moment dyld maps a native
 * dylib low, which it does whenever there is room.
 *
 * MEASURED, Portal 2 2026-09-14: modern steamclient.dylib is x86_64 + arm64 with
 * NO i386 slice, and dyld mapped its 24 MB at 0x2143f000. libsteam_api dlsym'd
 * `CreateInterface`, got the raw low address back as "callable", and called
 * native SysV code with an i386 cdecl frame. The callee read garbage argument
 * registers and stored an out-param through one:
 *     SIGBUS err=0x6, `movl $1,(%rbx)`, rbx inside steamclient's OWN r-x __TEXT,
 *     while %rsp was still the low, 4-byte-aligned TRANSLATED stack.
 * That pairing -- native rip, low 4-byte-aligned rsp -- is the signature of an
 * unbridged cross-ABI call. dlsym is its second delivery mechanism, and the one
 * the static-bind audit (unbridged-native-calls.py) structurally cannot see.
 *
 * THE FIX: ask the question we mean, using the structural test posix_shim.c
 * already defines for the mirror-image case -- an image is translated iff it
 * carries an LC_LOAD_DYLIB naming libabiconv. A low NATIVE function now gets the
 * same callable thunk a high one does.
 *
 * ARMS: ON -> triple(14) = 42.  OFF (M64_NO_LOW_NATIVE_THUNK=1) -> the raw low
 * native address is called with the i386 cdecl frame, so the callee reads
 * whatever %edi held instead of the argument: any answer but 42, or a crash.
 */
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdint.h>
#include <stdlib.h>

int main(void) {
   /* ⚠argv does NOT reach a translated tests-i386 fixture (argc arrives as
    * garbage), so every knob here is an environment variable. */
   const char *path = getenv("M64_TEST_HELPER");
   if (path == NULL) { path = "build/lownative_helper.dylib"; }
   void *h = dlopen(path, RTLD_NOW);
   if (h == NULL) {
      printf("dlopen failed\n"); fflush(stdout);
      exit(2);
   }
   void *p = dlsym(h, "m64_lownative_triple");
   if (p == NULL) {
      printf("dlsym failed\n"); fflush(stdout);
      exit(3);
   }
   if (getenv("M64_TEST_SKIP_CALL")) {
      printf("skipped\n"); fflush(stdout);
      exit(0);
   }
   int (*fn)(int) = (int (*)(int))p;
   int got = fn(14);
   printf("triple=%d\n", got);
   fflush(stdout);
   /* ⚠Every fixture in this harness calls exit(); RETURNING from main crashes
    * the translated program (rc=139), which is a separate pre-existing gap.
    * Logged in the known-gaps list -- do not let it masquerade as this guard failing. */
   exit(got == 42 ? 0 : 1);
   return 0;
}
