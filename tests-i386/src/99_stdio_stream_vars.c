/*
 * 99_stdio_stream_vars — stderr/stdout/stdin must be usable from translated code.
 *
 * WHY (Portal 2 2026-09-14). `stderr` is the macro `__stderrp`, an extern FILE*
 * variable in libSystem. A translated image binds it into a __DATA,__nl_symbol_ptr
 * slot and reads it like any scalar extern:
 *     movl slot, %eax        ; eax = &__stderrp, TRUNCATED to 32 bits
 *     movl (%eax), %r11d     ; deref -> SIGSEGV
 * dyld writes the real 64-bit &__stderrp, so the 4-byte load keeps only the low
 * half. Proven in the faulting process: &__stderrp = 0x7ff852955e38, low32
 * 0x52955e38, fault address 0x52955e38 exactly. 31 such binds across Portal 2's
 * tree; the one that bit was libsteam_api building an fprintf(stderr, ...) to
 * report why Steam init failed -- so this defect also SUPPRESSES the diagnostics
 * you would use to find the next bug.
 *
 * objc_slide.c's patch_import_pointers now redirects those three slots to a
 * low-4GB cell holding the stream as a wrapped FILE handle.
 *
 * fileno() is used for stdout/stdin rather than printing on them: when the
 * harness redirects stdout to a file it is FULLY buffered while stderr is not, so
 * writing to both would make the output ORDER nondeterministic and the test flaky.
 * fileno still passes the FILE* through a real bridge, which is the thing under
 * test. The single stderr write then proves an actual write works end to end.
 *
 * 42 = all three usable.  9/8/7 = stdin/stdout/stderr wrong fd (handle resolved
 * to the wrong object).  A crash = the truncation defect itself.
 *
 * ⚠⚠ THIS IS A POSITIVE-ONLY GUARD -- it cannot catch a regression, and saying so
 * matters more than the green tick. With M64_NO_STDIO_STREAM_CELL=1 (which really
 * does leave the slots bound to libSystem: verified, ABICONV_OBJC_SLIDE_VERBOSE
 * prints no redirect) this test STILL passes in the harness, because the truncated
 * low32 of &__stderrp happens to be readable here AND to hold the right value. On
 * Portal 2 the same switch reproduces the real crash exactly (fault addr
 * 0x55c99e38 == low32(&__stderrp), at libsteam_api+0x6d7c), so the defect and the
 * cure are both real -- this harness just cannot express the failing address
 * layout. Why the low alias resolves correctly in a small translated test process
 * is NOT understood; do not read this test's pass as proof the slots are redirected
 * (check ABICONV_OBJC_SLIDE_VERBOSE for `abiconv stdio:` lines instead).
 */
#include <stdio.h>

extern void exit(int status);

int main(void) {
   if (fileno(stdin)  != 0) { exit(9); }
   if (fileno(stdout) != 1) { exit(8); }
   if (fileno(stderr) != 2) { exit(7); }
   fputs("stdio-streams-ok\n", stderr);
   fflush(stderr);
   exit(42);
}
