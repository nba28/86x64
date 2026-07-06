/*
 * 82_mod_term_func — a translated __DATA,__mod_term_func static TERMINATOR
 * must RUN at exit() at the correct address AND return cleanly, without
 * truncating the caller's 64-bit return address.
 *
 * Classic-GCC i386 binaries (Halo CE, Civ IV) emit their C++ static
 * destructors as pointers in a __DATA,__mod_term_func (S_MOD_TERM_FUNC_POINTERS)
 * section. On dyld4, Loader::findAndRunAllInitializers -> forEachTerminator
 * registers each entry RAW with native __cxa_atexit(func, NULL, mh) — an
 * in-image translated address that PASSES dyld's in-image validation (unlike
 * the out-of-image init stubs that run-now sidesteps for __mod_init_func), so
 * the __cxa_atexit shim is bypassed (dyld, not translated code, registers it).
 * At exit(), native __cxa_finalize CALLS the translated i386-ABI terminator
 * directly: the body runs, but its translated i386 epilogue
 * (movl (%rsp),%r11d; lea 4(%rsp),%rsp; jmp *%r11) pops only the LOW 4 bytes
 * of libsystem_c's 8-byte return address -> jumps to the truncated low half
 * -> EXC_BAD_ACCESS. (Halo, 2026-07-06: fault 0x1c08aeb3 = low32 of
 * __cxa_finalize_ranges+319; surfaced once c2eb10a made static ctors live and
 * the disk-space fix let Halo reach its voluntary exit(0).)
 *
 * libabiconv's wrap_mod_term_funcs (add-image callback, before dyld reads the
 * section) NULLs each slot so dyld skips it, and re-registers the real
 * terminator through the reverse callback bridge (x64_cb_wrap -> native
 * __cxa_atexit), so the terminator runs on a proper low-4GB i386 frame with a
 * 4-byte return convention that matches its epilogue.
 *
 * Output goes through write(2), not stdio: the terminator both RAN (at the
 * correct address, reading the marker main set) and RETURNED cleanly if the
 * "term marker=42" line appears AND the process exits 0. Without the fix the
 * process crashes in __cxa_finalize (a signal exit, no "term" line) and the
 * diff fails. (stdio is avoided so this guards ONLY the terminator-truncation
 * path, not the separate ___sF FILE-array data-shadow marshalling gap.)
 */

#include <unistd.h>
#include <string.h>

extern void exit(int status);

static volatile int g_marker = 0;

static void my_term(void) {
   /* Would fault at a truncated wild address without the fix; with it, runs on
    * a clean i386 frame with g_marker == 42 (set by main). */
   const char *msg = (g_marker == 42) ? "term marker=42\n"
                                       : "term BADSTATE\n";
   write(1, msg, strlen(msg));
}

/* Force a genuine S_MOD_TERM_FUNC_POINTERS section (the classic-GCC static-dtor
 * shape). Modern clang's -fuse-cxa-atexit default routes
 * __attribute__((destructor)) through __mod_init_func + __cxa_atexit instead,
 * which would NOT exercise the dyld forEachTerminator path this guards. */
__attribute__((used, section("__DATA,__mod_term_func,mod_term_funcs")))
static void (*const g_term_slot)(void) = my_term;

int main(void) {
   g_marker = 42;
   write(1, "main\n", 5);
   /* The wrapper enters _main via jmp, so call exit() explicitly (per the
    * tests-i386 convention) — this is also what drives __cxa_finalize to run
    * the terminator, i.e. the exact path under test. */
   exit(0);
}
