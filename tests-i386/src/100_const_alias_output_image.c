/* 100_const_alias_output_image — does the M64 RE-PARSE still "rebase" a plain
 * integer constant whose bytes happen to alias the TRANSLATED image?
 *
 * WHY THIS EXISTS. Section::DataParser pointer-detects any 4-byte aligned data
 * word whose value falls inside a segment, and a chain of gates rejects the
 * false positives — but every one of those gates is deliberately M32-only,
 * because each needs the ORIGINAL i386 image (its local reloc table, its text
 * symbols, its instruction decode). The pipeline does not stop after the M32
 * pass: `modify`, `strip-bind`, `static-interpose` and `convert` all RE-PARSE
 * the already-translated M64 image, and there DataParser runs completely
 * ungated.
 *
 * That leaves a hole, and it is the exact MIRROR of 99_zerofill_target_pair. On
 * the M32 side a (small, small) u16 pair looks like an address because the
 * SOURCE image is small. On the M64 side any constant whose high byte is 0x10
 * looks like a fine __text address because the OUTPUT image is based at
 * 0x10000000 — even though the M32 pass had already, correctly, ruled it a
 * constant precisely because it sat far ABOVE the little i386 image.
 *
 * ★MEASURED (2026-08-05) by diffing each i386 original against its translated
 * output at the same intra-section offset: Civ IV Steam 738 reclassified
 * constants, iPhoto 143, iMovie 10, Halo CE 1. 79% of the corrupted words were
 * "relocated" into __TEXT,__eh_frame; 14 landed in __DATA,__86x64_pcmap, a
 * section the translator itself synthesizes and that no original i386 pointer
 * can possibly target. iPhoto's 13 __DATA,__gcc_except_tab hits are round LSDA
 * length constants (0x11000000, 0x10910000) shifted by -0xF0 — i.e. silently
 * corrupted C++ exception tables. Halo CE: __TEXT,__const+0x2440 held
 * 0x10080808 and came out 0x10080780.
 *
 * ★NO VALUE TEST CAN SEPARATE THEM. Halo's 0x10080808 resolves to an EXACT
 * instruction boundary in the translated __text, so even the code-INTERIOR test
 * is blind to it, and the __eh_frame majority is outside every code/cstring
 * gate. The answer is instead carried across the file boundary: the M32 pass
 * writes its CONSTANT verdicts into __DATA,__86x64_cpin and the M64 re-parse
 * consults them. Membership is EXACT, not heuristic.
 *
 * ★UNLIKE 99_zerofill_target_pair, THE POISON NEEDS NO PATCHING. That fixture
 * had to patch its value into the built binary because it must equal a
 * LINK-TIME address in the SOURCE image. Here the aliased image is the OUTPUT
 * one, whose base (0x10000000) is fixed by macho-tool, so the poison is an
 * ordinary compile-time C initialiser.
 *
 * WHY A DENSE SWEEP AND NOT ONE VALUE. The defect only shows on a constant that
 * RESOLVES — i.e. one landing exactly on a blob boundary in the translated
 * image. Which addresses those are depends on this fixture's own translated
 * layout, so the table sweeps 0x10000800..0x100017FC at 4-byte steps, which
 * brackets the whole of a small fixture's __TEXT and __DATA. 0x10080808 (Halo's
 * own value) is carried separately as documentation. Every value here is far
 * above this fixture's i386 image, so the M32 pass rules them ALL constants —
 * which is the premise being guarded.
 *
 * ARMS: ON must preserve every aliasing constant across a re-layout. OFF
 * (M64_NO_CONST_PIN=1) must corrupt at least one — otherwise the guard is not
 * exercising anything.
 */
extern int  printf(const char *, ...);
extern void exit(int);

/* Distinct brackets so the script can find each table unambiguously. All are
 * far above any translated image, so none is itself pointer-shaped. */
#define SWEEP_HEAD 0x4E495043u
#define SWEEP_TAIL 0x4350494Eu
#define CTLH_HEAD  0x484C5443u
#define CTLH_TAIL  0x4354484Cu
#define CTLP_HEAD  0x504C5443u
#define CTLP_TAIL  0x4354504Cu

const char g_pointee[] = "real-pointee";

#define R4(b)    (b), (b) + 0x04u, (b) + 0x08u, (b) + 0x0Cu
#define R16(b)   R4(b), R4((b) + 0x10u), R4((b) + 0x20u), R4((b) + 0x30u)
#define R64(b)   R16(b), R16((b) + 0x40u), R16((b) + 0x80u), R16((b) + 0xC0u)
#define R256(b)  R64(b), R64((b) + 0x100u), R64((b) + 0x200u), R64((b) + 0x300u)
#define R1024(b) R256(b), R256((b) + 0x400u), R256((b) + 0x800u), R256((b) + 0xC00u)

/* A const table => __TEXT,__const, which is where 847 of the 894 measured
 * instances live. That section is also where switch jump tables live, which is
 * exactly why DataParser's func-entry and code-entry gates are disarmed for it
 * — they would reject the legitimate mid-function basic-block targets. So
 * nothing but provenance can save these words. */
const unsigned int g_sweep[] = {
   SWEEP_HEAD,
   R1024(0x10000800u),
   0x10080808u,      /* Halo CE's own constant, for the record */
   SWEEP_TAIL
};
#define SWEEP_N (1024 + 1)

/* CONTROL A: above ANY translated image, so out of range on BOTH sides. Must be
 * intact in every arm — a green ON therefore cannot just mean "the translator
 * changed nothing at all". */
const unsigned int g_ctl_high[] = { CTLH_HEAD, 0x7F123456u, 0x7EFEFEFEu, CTLH_TAIL };

/* CONTROL B: a GENUINE pointer. It must be RELOCATED by the re-layout in BOTH
 * arms, and identically. This is the load-bearing half of the guard: it proves
 * the pin only ever removes FALSE positives and never costs a real pointer its
 * tracking. */
const char *const g_ctl_ptr[] = { (const char *)(unsigned long)CTLP_HEAD,
                                  g_pointee,
                                  (const char *)(unsigned long)CTLP_TAIL };

int main(void)
{
   printf("survived=1\n");
   printf("sweep_head=%d\n", g_sweep[0] == SWEEP_HEAD);
   printf("halo_const=%d\n", g_sweep[SWEEP_N] == 0x10080808u);
   printf("ctl_high=%d\n", g_ctl_high[1] == 0x7F123456u);
   printf("ctl_ptr=%d\n", g_ctl_ptr[1] != 0 && g_ctl_ptr[1][0] == 'r');
   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
