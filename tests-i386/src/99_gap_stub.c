/* 99_gap_stub — does a silent STUB shim speak on its first hit?
 *
 * WHY THIS EXISTS. Blocker after blocker was "we don't have this call, so it
 * returned 0" (Halo's checkbox: Get/SetControl32BitValue gone -> every read 0),
 * found one debugging session at a time. Every constant-return shim now carries
 * GAP_STUB (src/abiconv/gap.h): its first hit per process writes ONE stderr line
 * naming the symbol and the i386 caller, and appends it to the reach ledger.
 *
 * TextWidth is a marked stub (no classic font raster); EqualPt is listed in
 * coverage-audit.ok (a real implementation) and must stay quiet. gap_stub_test.sh
 * checks: ON = exactly one line + one ledger entry for TextWidth, none for
 * EqualPt; OFF (M64_GAP=0) = silent, no ledger; M64_GAP=abort = dies.
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern short TextWidth(const void *, short, short);
extern unsigned char EqualPt(unsigned int, unsigned int);

int main(void)
{
   int w = TextWidth("abc", 0, 3) + TextWidth("abc", 0, 3);
   printf("width=%d equal=%d\n", w, EqualPt(0x10002, 0x10002));
   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
