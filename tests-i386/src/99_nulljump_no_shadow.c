/* 99_nulljump_no_shadow — does static-interpose refuse to redirect a bind into a
 * bridge that can only jump to NULL?
 *
 * WHY THIS EXISTS. abigen emits a bridge `___X` for anything in its consider set
 * with a parseable 10.6 declaration. That is correct while a NATIVE `_X` still
 * exists to call. When Apple REMOVES the API the bridge survives, the pipeline
 * weakens the now-dangling native bind to NULL, and the bridge's `call` jumps to
 * 0. static-interpose then redirects the app's own bind INTO that bridge — so we
 * do not merely fail to help, we REPLACE whatever the app would have bound to
 * with a guaranteed crash.
 *
 * Measured 2026-08-03 (find_null_jump_bridges.py): 507 such bridges, and 358 of
 * them SHADOW a working implementation the app itself ships — 347 from its own
 * bundled translated QuickTime.framework, 11 from its bundled Python.
 * Civilization IV died at QTNewDataReferenceFromFSRef for exactly this reason
 * while its own QuickTime defined the symbol at 0x100aa300.
 *
 * Skipping them is strictly better in both directions:
 *   - the app ships an implementation -> the bind reaches real, working code;
 *   - nothing ships one -> the bind stays weak and resolves to NULL, which is
 *     what a classic `if (SomeAPI != NULL)` availability check needs to see.
 *     Our bridge is non-NULL, so interposing actively DEFEATS that check: the
 *     app concludes the API exists and calls into a jump-to-zero.
 *
 * This fixture imports `CTabChanged` — a classic QuickDraw colour-table call
 * that modern macOS no longer defines and that nothing else in the process
 * provides, i.e. a pure orphan from the list.
 *
 * ★IF CTabChanged IS EVER IMPLEMENTED, this guard must switch to another symbol
 *  that is still in build/src/abiconv/libabiconv.nulljump — the whole point is
 *  that the chosen symbol has NO provider. nulljump_no_shadow_test.sh checks the
 *  list at run time and says so rather than failing obscurely.
 *
 * The assertion is made on the BIND TABLE, not at run time: calling the symbol
 * would just crash in the unfixed arm, which proves less and is harder to read.
 * ⚠It must be read with `dyld_info -fixups` — `nm -m` and `otool -Iv` report the
 * NLIST table, which static-interpose does not rewrite, and would answer this
 * question wrongly in both arms.
 */
extern int  printf(const char *, ...);
extern void exit(int);

/* Weakly imported precisely as a classic app would declare an optional API. */
extern void CTabChanged(void *ctab) __attribute__((weak_import));

int main(void)
{
   /* Referencing it is enough to create the import the guard inspects. The
    * NULL test is also the real-world behaviour we are protecting: with the
    * bind left alone this is NULL and an app can skip the call, whereas an
    * interposed bridge is non-NULL and lures the app into jumping to zero. */
   printf("available=%d\n", &CTabChanged != 0);
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
