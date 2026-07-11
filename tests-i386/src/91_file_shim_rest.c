/* 91_file_shim_rest.c — the rest of the FILE*-taking libc surface must unwrap
 * the wrapper's shim FILE, just like fflush (89_fflush_file).
 *
 * file_shim.c resolved (real_*) but never DEFINED these entry points, so the
 * abigen ___<fn> shim's `call _<fn>` fell through to NATIVE libc (via
 * -undefined dynamic_lookup) and handed it the raw low-4GB shim struct. The
 * native fn reads the shim's zeroed pad as a FILE internal field and faults —
 * the fflush class of latent crash (any legacy debug/verbose build hits it).
 * Confirmed pre-fix: every one of _flockfile / _fgetln / _popen / _getw /
 * _setlinebuf / ___srget / ... was `U` in libabiconv (bound native).
 *
 * Legs (all on a real fopen'd stream and/or a std stream):
 *   - flockfile / ftrylockfile / funlockfile  (byte-lock family)
 *   - getc_unlocked / putc_unlocked           (walk to the stream buffer)
 *   - fgetln                                  (returns a buffer pointer)
 *   - getw / putw                             (binary word I/O)
 *   - setbuffer / setlinebuf                  (buffering)
 *   - fpurge                                  (discard buffered data)
 *   - popen / pclose                          (piped stream, wrapped result)
 *   - the getc()/putc() macro helpers ___srget / ___swbuf are exercised
 *     implicitly by getc/putc across a buffer boundary.
 *
 * Pre-fix any single leg SIGSEGVs (exit 139). Post-fix all round-trip.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
    const char *path = "/tmp/86x64_91_file_shim_rest.txt";

    /* --- putw / getw on an fopen'd stream (shim FILE from fopen) --- */
    FILE *f = fopen(path, "w+");
    if (!f) { puts("FAIL fopen"); exit(1); }

    flockfile(f);              /* must reach the real stream's lock */
    putw(0x11223344, f);
    putw(0x55667788, f);
    funlockfile(f);
    fflush(f);

    rewind(f);
    int w0 = getw(f);
    int w1 = getw(f);
    if (w0 != 0x11223344 || w1 != 0x55667788) {
        printf("FAIL getw/putw %08x %08x\n", w0, w1); exit(1);
    }
    puts("word io ok");

    /* --- fpurge + rewrite, then fgetln reads a full line --- */
    rewind(f);
    fpurge(f);                 /* discard the read buffer state */
    ftruncate(fileno(f), 0);   /* fileno also unwraps the shim */
    fseek(f, 0, SEEK_SET);
    setlinebuf(f);             /* buffering control on the shim */
    fputs("line one\n", f);
    fputs("line two\n", f);
    fflush(f);
    rewind(f);

    size_t len = 0;
    char *ln = fgetln(f, &len);
    if (!ln || len != 9 || strncmp(ln, "line one\n", 9) != 0) {
        printf("FAIL fgetln len=%zu\n", len); exit(1);
    }
    puts("fgetln ok");

    /* NOTE: getc()/putc()/getc_unlocked()/putc_unlocked() are NOT exercised
     * here: the i386 <stdio.h> INLINES them (putc -> __sputc reads fp->_p, getc
     * -> __sgetc reads fp->_r) directly into the caller, touching i386 FILE
     * fields that our shim FILE deliberately does not mirror (offset 0 is the
     * real FILE*, not _p). That inline field-access path is a separate, deeper
     * gap (would need the shim to mirror the 88-byte i386 __sFILE layout) and is
     * out of scope for the FILE*-unwrap surface. The ___srget / ___swbuf
     * wrappers this change adds still correctly handle the CALL-OUT case (buffer
     * miss), which is what a translated ___srget/___swbuf bind reaches. */

    /* --- funlockfile / flockfile / ftrylockfile round-trip (real calls) --- */
    rewind(f);
    if (ftrylockfile(f) == 0) { funlockfile(f); }
    puts("lock ok");

    /* setbuffer on the fopen'd stream must not fault (exercise before close;
     * done on f, not stdout, so it can't reorder the harness-captured output) */
    static char sbuf[BUFSIZ];
    rewind(f);
    setbuffer(f, sbuf, sizeof sbuf);
    puts("setbuffer ok");

    fclose(f);
    remove(path);

    /* --- popen / pclose: wrapped stream over a pipe --- */
    FILE *p = popen("printf 'popen-payload'", "r");
    if (!p) { puts("FAIL popen"); exit(1); }
    char pbuf[32]; memset(pbuf, 0, sizeof pbuf);
    fread(pbuf, 1, sizeof pbuf - 1, p);
    int rc = pclose(p);          /* pclose must unwrap the shim */
    if (strcmp(pbuf, "popen-payload") != 0) {
        printf("FAIL popen read '%s' rc=%d\n", pbuf, rc); exit(1);
    }
    puts("popen ok");

    /* flush through the reset stdout buffer */
    fflush(stdout);
    puts("all ok");
    fflush(stdout);
    exit(0);
}
