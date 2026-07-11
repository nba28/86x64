/* 90_sf_data_shadow.c — the non-UNIX03 `&__sF[N]` std-stream data-shadow.
 *
 * Older i386 binaries (compiled with __DARWIN_UNIX03 == 0) do NOT reach the
 * three std streams through the ___stdinp/___stdoutp/___stderrp POINTER slots
 * that wrapper_setup.c patches. Instead <stdio.h> defines
 *     #define stdin  (&__sF[0])
 *     #define stdout (&__sF[1])
 *     #define stderr (&__sF[2])
 * over `extern FILE __sF[]`, so the binary imports the DATA symbol ___sF and
 * forms `&__sF[N]` = `___sF + N*88` (the i386 sizeof(FILE) is 88; the +N*88 is
 * added by the instruction stream). dyld would bind ___sF to libc's real __sF,
 * which lives above 4 GB, so the truncated 32-bit `&__sF[N]` FILE* faults on
 * any use — the exact class of crash the ___stdoutp pointer-slot patch and the
 * fflush wrapper already cure for the UNIX03 path.
 *
 * The fix (file_shim.c): libabiconv exports a low-4GB `___sF` array laid out at
 * the i386 FILE stride (88 bytes) whose entries are shim_FILE structs (real_fp
 * @0, magic @8, fd @12); static-interpose redirects the ___sF bind to it, so
 * `&__sF[N]` is a shim FILE* that resolve_file recognises and unwraps.
 *
 * This test references `&__sF[N]` DIRECTLY (the non-UNIX03 form) instead of
 * relying on the header's UNIX03 setting, so it deterministically exercises the
 * ___sF bind. Pre-fix: the first fputs through &__sF[1] faults (SIGSEGV/exit
 * 139). Post-fix: writes go to the right streams and a flush round-trips.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The non-UNIX03 array symbol. Declared directly so the reference is the raw
 * ___sF data bind regardless of the SDK header's __DARWIN_UNIX03 value. */
extern FILE __sF[];

int main(void) {
    FILE *out = &__sF[1];   /* stdout */
    FILE *err = &__sF[2];   /* stderr */

    /* Write through the shadow stdout. Pre-fix this is the first fault. */
    fputs("sf stdout leg\n", out);
    fflush(out);            /* fflush also unwraps the ___sF shim */

    /* stderr leg (not captured by the harness; must not fault). */
    fputs("sf stderr leg\n", err);
    fflush(err);

    /* fprintf through the shadow stdout — the varargs path + resolve_file. */
    fprintf(out, "sf fprintf %d\n", 42);
    fflush(out);

    /* fileno on the shadow must report the real fd (1), proving it resolved to
     * our shim and not a truncated garbage pointer. */
    if (fileno(out) != 1) { printf("FAIL fileno(out)=%d\n", fileno(out)); exit(1); }
    if (fileno(err) != 2) { printf("FAIL fileno(err)=%d\n", fileno(err)); exit(1); }

    printf("sf ok\n");
    fflush(out);
    exit(0);
}
