/* 99_record_field_pair — does the translator still "rebase" a u16 pair held in
 * an INTEGER FIELD of a record array, when the address it spells is MAPPED?
 *
 * WHY THIS EXISTS, and how it differs from 99_zerofill_target_pair. That guard
 * covers the same (small,small) u16-pair defect for a ZERO-FILL target, where
 * the gate can argue from the absence of an anchoring symbol. This one covers
 * the targets that ARE mapped, where that argument does not exist:
 *
 *   - a __DATA,__data target — for which there was no discriminator at all; and
 *   - a __TEXT,__text target held in a slot that lives in __TEXT,__const, where
 *     the func-entry and code-entry gates are DELIBERATELY disarmed (switch jump
 *     tables live in __TEXT,__const and legitimately target unsymboled
 *     basic-block heads), leaving only code_interior_alias — which passes any
 *     value that happens to land on a decoded instruction BOUNDARY.
 *
 * ★MEASURED on Halo CE (task #35). Its sound code walks an array of 8-byte
 * records and reads a u16 index from each. Two records hold (small,small) u16
 * pairs whose bytes spell in-image addresses — 0x00030002 (-> __TEXT,__text) and
 * 0x00380000 (-> __DATA,__data) — so both were "rebased" to 0x1003ac81 /
 * 0x104a2000 and the u16 index became the LOW HALF of a relocated pointer:
 * (0,12,2,3) became (0,12,44161,4099) and (0,56,6,2) became (4096,4170,6,2).
 * Proven two ways: across two runs the garbage index tracked ASLR while implying
 * the same K for P = base + K (a genuine u16 index cannot move with ASLR), and by
 * a static i386-vs-translated diff of the array.
 *
 * ★THE TRAP IS THAT THE IMAGE IS SMALL: every valid address then has a small
 * high half, so a (small,small) u16 pair is byte-identical to a real address and
 * NO value-based test can separate them. The evidence has to be POSITIONAL — the
 * slot's siblings at the record stride are the SAME FIELD of neighbouring
 * records, so if they are plainly integers, so is this.
 *
 * ★WHAT THE THREE ARRAYS BELOW ARE FOR. Getting this gate wrong is easy and
 * expensive; each measured failure has a fixture:
 *
 *   g_poison  — an 8-byte record array in __TEXT,__const. The test script
 *               patches record 3's field to a MID-FUNCTION __text instruction
 *               boundary (no symbol, no prologue) and its six stride-siblings to
 *               symbol-free zero-fill addresses, so the positional evidence is
 *               unambiguous. ON must preserve it; OFF must mangle it.
 *
 *   g_jt      — a dense run of code addresses, i.e. what a switch JUMP TABLE in
 *               __TEXT,__const looks like. The first version of the gate accepted
 *               `code_alias_lacks_entry_evidence` as declassifying a SIBLING —
 *               precisely the test jump tables are exempt from — so every entry
 *               declassified every other entry and Halo lost 7889 real code
 *               pointers in one section. These must still relocate, in BOTH arms.
 *
 *   g_ctlptr  — Quinn's exact shape: 32-byte records {ptr, 0, 1, float, float,
 *               float, float, magic}. At stride 8 the pointer's "siblings" are
 *               that record's OWN 0/1/floats — three nonzero "integers", enough
 *               to satisfy a naive bar — so an earlier version demoted 7 genuine
 *               __DATA,__cfstring pointers. The cure is that only an
 *               ADDRESS-SHAPED sibling counts as evidence; a 0, a small count or
 *               a float addresses nothing, so it cannot be the same field as an
 *               address-shaped candidate and is NEUTRAL. These pointers must
 *               still relocate, in BOTH arms.
 *
 * HOW THIS IS ASSERTED. The poisonous value must EQUAL a link-time address, so
 * it cannot be a C constant: record_field_pair_test.sh patches it into the built
 * i386 binary and translates the SAME bytes twice, one env var apart, then
 * compares the words in the output. That is a byte-level assertion on the
 * translator, which is what the defect actually is.
 *
 * ARMS: ON must preserve the pair. OFF (M64_NO_RECORD_FIELD_GATE=1 at TRANSLATE
 * time) must mangle it — otherwise the guard is not exercising anything.
 */
extern int  printf(const char *, ...);
extern void exit(int);

/* Zero-fill, and `static` + a `-x` link so the section carries NO symbol: the
 * sibling values are patched to point in here, where zerofill_target_unattested
 * declassifies them exactly as it does for Halo's real siblings. */
static unsigned char g_zf[1024 * 1024];

/* ---- the POISON: 8-byte records, const and pointer-free => __TEXT,__const ----
 * Word [0] is the locator magic. Word [2i+1] is the field under test; the script
 * patches records 0,1,2,4,5,6 to zero-fill addresses and record 3 to a
 * mid-function __text instruction boundary. 0x5246504Bu = 'RFPK'; it is far
 * above any test image, so it is never itself mistaken for a pointer. */
static const unsigned int g_poison[16] = {
   0x5246504Bu, 0x11110001u,   /* rec0: locator            | sibling  */
   0u,          0x11110002u,   /* rec1                     | sibling  */
   0u,          0x11110003u,   /* rec2                     | sibling  */
   0u,          0x11110004u,   /* rec3                     | CANDIDATE */
   0u,          0x11110005u,   /* rec4                     | sibling  */
   0u,          0x11110006u,   /* rec5                     | sibling  */
   0u,          0x11110007u,   /* rec6                     | sibling  */
   0u,          0x11110008u,   /* rec7  (bounds padding)             */
};

/* ---- CONTROL 1: a switch JUMP TABLE, buried in look-alike filler ------------
 * A dense run of nearby code addresses — what a switch table in __TEXT,__const
 * is — which MUST still be relocated in BOTH arms.
 *
 * ★The FILLER either side is not padding, it is the second half of the control,
 * and it models the exact coincidence that made an earlier version of the gate
 * demote real pointers. The gate must test only ONE stride hypothesis: a record
 * array has exactly one stride, so trying twelve and firing if ANY of them looks
 * integer-ish gives twelve chances to be wrong, and in a real __const section a
 * run of six unrelated non-pointer words at some stride is a certainty, not a
 * coincidence. MEASURED on Halo: the jump table at 0x341540 is correctly VETOED
 * at stride 8 (its true neighbours are code pointers) and was then wrongly
 * ACCEPTED at stride 64, where the "siblings" were 0x7fffffff sentinels from a
 * different table entirely.
 *
 * So: one contiguous array, so the linker cannot reorder it. Word 48 is the
 * locator; words 49..64 are the jump table; everything else is patched to
 * symbol-free zero-fill addresses. The entry at word 57 therefore has
 *   stride 8  siblings (words 57 +-2,4,6) -> all jump-table entries -> VETO
 *   stride 64 siblings (words 57 +-16,32,48) -> all zero-fill filler -> would
 *                                               ACCEPT if the gate went fishing
 * 0x4A544B4Cu = 'JTKL'. */
#define JT_FILL 48
#define JT_N    16
static const unsigned int g_jt[JT_FILL + 1 + JT_N + JT_FILL] = {
   [0 ... JT_FILL - 1] = 0x33330000u,
   [JT_FILL] = 0x4A544B4Cu,
   [JT_FILL + 1 ... JT_FILL + JT_N] = 0x22220000u,
   [JT_FILL + JT_N + 1 ... JT_FILL + JT_N + JT_FILL] = 0x33330001u,
};

/* ---- CONTROL 2: Quinn's 32-byte record, pointer field at +0 -----------------
 * No patching needed — a real `char *` IS a link-time address, and the linker
 * relocates it. Must still be relocated in BOTH arms. 0x51435452u = 'QCTR'. */
struct qrec {
   const char  *s;      /* +0   a REAL pointer — must STILL be relocated */
   unsigned int zero;   /* +4  */
   unsigned int one;    /* +8  */
   float        a, b, c, d;   /* +0xc .. +0x18 */
   unsigned int magic;  /* +0x1c  locator */
};
struct qrec g_ctlptr[4] = {
   { "alpha", 0, 1, 7.0f, 11.0f, 2.0f, 4.0f, 0x51435452u },
   { "bravo", 0, 1, 7.0f, 11.0f, 2.0f, 4.0f, 0x51435452u },
   { "chnnl", 0, 1, 7.0f, 11.0f, 2.0f, 4.0f, 0x51435452u },
   { "delta", 0, 1, 7.0f, 11.0f, 2.0f, 4.0f, 0x51435452u },
};

/* ---- CONTROL 3: an INERT pair, far above any image -------------------------
 * The 'bipd' case that survived even unfixed. Intact in BOTH arms, so a green ON
 * cannot just mean "the translator changed nothing at all". 0x494E5254u='INRT'. */
static const unsigned int g_inert[4] = {
   0x494E5254u, 0x7F123456u, 0u, 0u,
};

int main(void)
{
   unsigned i, acc = 0;
   for (i = 0; i < 16; ++i) { acc += g_poison[i]; }
   for (i = 0; i < sizeof(g_jt) / sizeof(g_jt[0]); ++i) { acc += g_jt[i]; }
   for (i = 0; i < 4;  ++i) { acc += g_inert[i]; }
   printf("survived=1\n");
   printf("zf_nonzero=%d\n", (void *)g_zf != 0);
   printf("acc_nonzero=%d\n", acc != 0);
   printf("ctl_ptr_ok=%d\n", g_ctlptr[0].s[0] == 'a' && g_ctlptr[3].s[0] == 'd');
   printf("ctl_fields=%d\n", g_ctlptr[1].one == 1 && g_ctlptr[2].zero == 0);
   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
