/* 92_wide_stdio.c — the wide-char FILE*-taking stdio family must unwrap the
 * wrapper's shim FILE.
 *
 * fgetwc/fputwc/fgetws/fputws/getwc/putwc/ungetwc/fwide are FILE*-taking libc
 * entries that were in abigen's consider set (from libSystem) but had NO
 * prototype (no <wchar.h> in includes.h), so abigen emitted no ___fgetwc shim.
 * The bind fell through to native libc: the i386 4-byte-ret call over-popped by
 * the native 8-byte ret (fused PC) AND, on a std/fopen'd stream, the raw low-4GB
 * shim FILE* faulted on its zeroed pad (the fflush/file_shim class of bug).
 *
 * Fix: <wchar.h> added to includes.h so abigen emits pointer-widening shims
 * whose `call _fgetwc` reaches file_shim.c's FILE*-unwrapping definitions.
 *
 * Legs on a real fopen'd stream (a fresh wrap_file shim): write wide chars +
 * a wide string with fputwc/fputws, read them back with fgetwc/fgetws, and
 * push one back with ungetwc. Pre-fix: the first fputwc faults (exit 139) or
 * mismarshals; post-fix the round-trip matches. Output is plain ASCII so the
 * harness diff is byte-stable.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

int main(void) {
    const char *path = "/tmp/86x64_92_wide_stdio.txt";
    FILE *f = fopen(path, "w+");
    if (!f) { puts("FAIL fopen"); exit(1); }

    /* fwide sets the stream orientation to wide; must reach the real stream. */
    fwide(f, 1);

    /* fputwc a few wide chars, then fputws a wide string. */
    if (fputwc(L'A', f) == WEOF) { puts("FAIL fputwc"); exit(1); }
    if (fputwc(L'B', f) == WEOF) { puts("FAIL fputwc"); exit(1); }
    if (fputws(L"CDE\n", f) < 0) { puts("FAIL fputws"); exit(1); }
    fflush(f);

    /* Read back: getwc/fgetwc for the first two, fgetws for the rest. */
    rewind(f);
    wint_t c0 = getwc(f);
    wint_t c1 = fgetwc(f);
    if (c0 != L'A' || c1 != L'B') { printf("FAIL read wc %lc%lc\n", (wint_t)c0, (wint_t)c1); exit(1); }

    /* ungetwc pushes B back; the next read must see it again. */
    if (ungetwc(c1, f) == WEOF) { puts("FAIL ungetwc"); exit(1); }
    wint_t c1b = fgetwc(f);
    if (c1b != L'B') { printf("FAIL ungetwc reread %lc\n", (wint_t)c1b); exit(1); }

    wchar_t line[16];
    if (!fgetws(line, 16, f)) { puts("FAIL fgetws"); exit(1); }
    if (wcscmp(line, L"CDE\n") != 0) { puts("FAIL fgetws content"); exit(1); }

    fclose(f);
    remove(path);

    /* std-stream leg: fputwc to stdout (a ___sF / ___stdoutp shim) must resolve
     * and not fault. Emit an ASCII newline via the wide path. */
    fputwc(L'\n', stdout);
    fflush(stdout);

    puts("wide ok");
    fflush(stdout);
    exit(0);
}
