/* 99_zerofill_target_pair — does the translator still "rebase" two adjacent u16
 * fields whose bytes happen to spell an address into ZERO-FILL memory?
 *
 * WHY THIS EXISTS. Pointer detection in a reloc-less fixed-address i386 image is
 * a heuristic over 4-byte values: any aligned word landing inside a segment is
 * treated as a pointer and slid. A ZERO-FILL target (__bss/__common) is its
 * weakest evidence class — the pointee has no file content to inspect, the image
 * carries no relocation naming the slot, and a locals-stripped image has no
 * symbol there either. Nothing can corroborate it, yet a false positive is never
 * inert: it rewrites a live integer.
 *
 * ★MEASURED on Halo CE (task #25). Its tag-class descriptor records carry three
 * u16 fields at +0xa/+0xc/+0xe. In the 'scen' and 'lifi' records the PAIR at
 * +0xc spells 0x0048021C / 0x005802D0, both landing in __DATA,__common, so both
 * were rebased: (540,72) became (27356,4272) and (720,88) became (27536,4288).
 * The 'bipd' record survived only because its pair spells 0x00780234, above the
 * image. The consuming loop processed exactly the two corrupted records, so
 * `base + 564` became `base - 9508` and the deref took a wild address every run.
 *
 * ★THE TRAP IS THAT THE IMAGE IS SMALL: every valid address then has a small
 * high half, which is exactly what makes a (small, small) u16 pair look like an
 * address. No value-based test can separate them — hence a gate keyed on the
 * TARGET's section kind, not on the value.
 *
 * HOW THIS IS ASSERTED. The poisonous value cannot be written as a C constant
 * (it must equal a link-time address), so zerofill_target_pair_test.sh PATCHES
 * it into the built i386 binary and translates the SAME bytes twice, one env var
 * apart, comparing the record in the output. That is a byte-level assertion on
 * the translator, which is what the defect actually is.
 *
 * ★g_zf is `static` and this fixture links with `-x`, so the zero-fill section
 * carries NO symbol. That is deliberate and models the real condition: the gate
 * relocates a zero-fill target that an object symbol ANCHORS (the legitimate
 * `&freqstruct[150000]` interior-pointer shape, guarded by
 * 96_zerofill_common_interior and needed by Halo's own non-lazy slots) and
 * rejects one sitting in symbol-free space, which is where Halo's 0x0048021C and
 * 0x005802D0 both fall — below the lowest __common symbol, 0x005B4620.
 *
 * ARMS: ON must preserve the pair. OFF (M64_NO_ZEROFILL_TARGET_GATE=1 at
 * TRANSLATE time) must mangle it — otherwise the guard is not exercising
 * anything.
 */
extern int  printf(const char *, ...);
extern void exit(int);

/* Zero-fill: uninitialised file-scope data, no initialiser ⇒ __bss/__common.
 * 1 MB so a (small, small) u16 pair can plausibly address into it. */
static unsigned char g_zf[1024 * 1024];

/* Real Halo KEEPS __common symbols; its bad targets fall BELOW the lowest one.
 * This global (it survives -x) is that lowest symbol, placed after g_zf, so the
 * patched target sits in symbol-free space of a section that has symbols.
 * Without it the section has none at all, which is the stripped shape the gate
 * deliberately does not judge (see 99_zerofill_stripped_ptr). */
unsigned char g_zf_anchor[16] __attribute__((section("__DATA,__bss")));

/* A Halo-shaped descriptor: leading pointer, 4CC, then u16 fields. MAGIC makes
 * the record findable in the built binary by the test script. */
struct rec {
   const char    *name;      /* +0    a REAL pointer — must STILL be relocated */
   unsigned int   fourcc;    /* +4    locator magic */
   unsigned short f8;        /* +8 */
   unsigned short fa;        /* +0xa */
   unsigned short pair_lo;   /* +0xc  <- patched to spell a g_zf interior addr */
   unsigned short pair_hi;   /* +0xe */
   unsigned int   tail;      /* +0x10 */
};

/* 0x5A46504Bu = 'ZFPK'. Placeholder pair is patched by the test script. */
struct rec g_rec = { "descriptor", 0x5A46504Bu, 504, 528, 0x1111, 0x2222, 4 };

/* CONTROL: its pair (0x00780234) is deliberately far ABOVE any test image, i.e.
 * the 'bipd' case that survived even unfixed. It must be intact in BOTH arms, so
 * a green ON result cannot be "the translator changed nothing at all". */
struct rec g_ctl = { "control", 0x4C54434Bu, 504, 528, 0x0234, 0x0078, 4 };

int main(void)
{
   /* Referencing everything keeps the linker from dropping it, and gives a
    * runtime sanity line for anyone who wants to execute the fixture. */
   printf("survived=1\n");
   printf("zf_nonzero=%d\n", (void *)g_zf != 0);
   printf("anchor=%d\n", (void *)g_zf_anchor != 0);
   printf("ptr_ok=%d\n", g_rec.name != 0 && g_rec.name[0] == 'd');
   printf("ctl_ptr_ok=%d\n", g_ctl.name != 0 && g_ctl.name[0] == 'c');
   printf("neighbours=%d\n", g_rec.f8 == 504 && g_rec.fa == 528 && g_rec.tail == 4);
   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
