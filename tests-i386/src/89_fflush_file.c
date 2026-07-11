/* 89_fflush_file.c — fflush(FILE*) must unwrap the wrapper's shim FILE.
 *
 * The wrapper patches the translated binary's ___stdoutp/___stderrp slots to
 * low-4GB shim_FILE structs (wrapper_setup.c) and libabiconv's file_shim.c
 * interposes the FILE*-taking libc surface to substitute the real FILE*.
 * fflush was the ONE entry point file_shim.c resolved (real_fflush) but never
 * wrapped, so the abigen ___fflush shim's `call _fflush` reached the REAL
 * libc fflush with the raw shim struct: its zeroed pad reads as
 * _extra == NULL -> flockfile -> pthread_mutex_lock(&NULL->fl_mutex) ->
 * EXC_BAD_ACCESS at address 0x8. EVERY translated `fflush(stdout)` /
 * `fflush(stderr)` / `fflush(fopen'd FILE*)` crashed; only fflush(NULL)
 * (libc flush-all, no FILE* marshalled) was safe — which is why the rest of
 * the suite dodged it (51_eh_throw_int / 66_cpp_cow_string / 84_cow_empty_rep
 * all note the dodge). First seen as a "fragile-ObjC1 + fprintf(%p) crash at
 * the first objc message": the ObjC/varargs framing was a red herring — the
 * minimal trigger is fflush on any real FILE*.
 *
 * Legs: fflush(stdout) between two prints (order proves the flush call
 * returns and output survives), fflush(stderr), fflush on an fopen'd stream
 * (wrap_file shim) with a read-back through the flushed handle, and
 * fflush(NULL) which must stay the safe flush-all.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    printf("before fflush\n");
    fflush(stdout);                    /* crashed: raw shim -> real fflush */

    fprintf(stderr, "stderr leg\n");   /* stderr not captured by the harness */
    fflush(stderr);

    /* fopen'd stream: fopen returns a fresh wrap_file shim; fflush must
     * unwrap that one too, and the data must actually reach the file. */
    const char *path = "/tmp/86x64_89_fflush_file.txt";
    FILE *f = fopen(path, "w+");
    if (!f) { puts("FAIL fopen"); exit(1); }
    fputs("payload", f);
    fflush(f);
    fseek(f, 0, SEEK_SET);
    char buf[16];
    memset(buf, 0, sizeof buf);
    fread(buf, 1, 7, f);
    if (strcmp(buf, "payload") == 0) puts("ok file leg");
    else { printf("FAIL file leg got '%s'\n", buf); exit(1); }
    fclose(f);
    remove(path);

    fflush(NULL);                      /* flush-all must remain safe */
    printf("after fflush\n");
    fflush(stdout);
    exit(0);
}
