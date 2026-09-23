/* 99_qword_high_skip.c — A/B fixture for the QWORD-HIGH-HALF skip in the
 * JUMP-TABLE COHESION gate (ParseEnv::code_alias_run_contradicted,
 * src/core/parse.cc). Kill switch M64_NO_QWORD_HIGH_SKIP=1.
 *
 * ★NAMED .c ON PURPOSE: the root .gitignore has a blanket `*.s`, which once ate
 * this guard family's fixture and left it SKIPping as a PASS for its whole life
 * (the silently-inert-guard gotcha).
 *
 * THE BLIND SPOT. An M32 image has no 8-byte pointers, so a const array of
 * 64-BIT INTEGERS is laid out as 8-aligned {lo, hi} pairs and its small entries
 * put a ZERO in every odd word. The cohesion walk used to treat that zero as an
 * OBJECT EDGE and stop dead on it, so every low half was judged with block == 1
 * and interior_edge == false: it could not see the proven mid-instruction
 * siblings sitting two words away. In __TEXT,__const the func-entry and
 * code-entry gates are deliberately disarmed (switch jump tables live there and
 * target unsymboled basic-block heads), and the record-field gate is vetoed as
 * soon as TWO entries of the same column alias code at a boundary — each one
 * declassifies the other. So any entry whose value happened to land on an exact
 * instruction BOUNDARY passed every gate and was rebased into a translated code
 * address.
 *
 * ★MEASURED (Quinn, 2026-09-23). The 32-entry u64 superincreasing-knapsack
 * vector at __TEXT,__const 0xb2c60 obfuscates the saved highscore file.
 * Entries 7 (0x29a5) and 9 (0xbd3c) are exact i386 instruction boundaries and
 * became 0x10001ee7 / 0x10011d2a, while their stride-8 siblings 0x73b5 /
 * 0x1ee7f / 0x425b6 land mid-instruction and were correctly left alone.
 * Encrypting with two inflated weights corrupted 63 of the 102 stored 8-byte
 * records; the NSArchiver stream read back with a mangled "streamtyped"
 * signature and every highscore was lost on restart.
 *
 * THE FIX. A zero at offset 4 of an 8-aligned slot, when the candidate slot is
 * itself 8-aligned, is the HIGH HALF OF THE SAME QWORD — not another object.
 * Skip it and keep walking. EXACT: a 4-byte jump table cannot present that
 * shape in its interior (a zero entry is not a branch target), and at a table's
 * end the downward walk counts the table's real entries and trips max_block.
 *
 * LAYOUT. `_alias_pad` is a large run of 0x00 bytes in __text. `00 00` decodes
 * as `add %al,(%eax)`, TWO bytes, so the linear sweep tiles the region with
 * 2-byte instructions: EVEN addresses inside it are instruction BOUNDARIES and
 * ODD addresses are strictly INTERIOR. qword_high_skip_test.sh re-asserts that
 * from the disassembly every run, so a layout change FAILS LOUDLY.
 *
 * kQwords models Quinn exactly — one 8-aligned u64 column:
 *   [0]  SEP        non-code: delimits the run
 *   [2]  interior   terminates the subject block below; also a CONTROL
 *   [4]  BOUNDARY   ★SUBJECT A
 *   [6]  interior   terminates A above and B below; also a CONTROL
 *   [8]  BOUNDARY   ★SUBJECT B — its stride-8 sibling A is "pointerish", which
 *                    is exactly what vetoes the record-field gate on Quinn, so
 *                    ONLY this fix can save either of them
 *   [10] interior   terminates B above; also a CONTROL
 *   [12] interior / [14] SEP   keep the column from merging with its neighbours
 * every odd index is the ZERO HIGH HALF that used to stop the walk.
 *
 * kTable is the POSITIVE CONTROL: a contiguous stride-4 run of 12 boundary
 * words with NO zero high halves — a jump table. The skip is inert there (the
 * word at +4 is another entry, not zero) and it must stay REBASED in both arms.
 * Without it, a fix that simply ignored every zero would score green here while
 * demoting real switch dispatch. Its value is a slid address, so it is asserted
 * from the translated BYTES by the harness, never printed by the program.
 */
#include <stdio.h>
#include <stdlib.h>

/* 320KB of zero "instructions" in __text (S_ATTR_PURE_INSTRUCTIONS). Never
 * executed; it exists only so the constants below alias decoded instructions. */
__asm__(".text\n"
        ".globl _alias_pad\n"
        "_alias_pad:\n"
        ".space 327680\n");

#define IVAL0 0x00008001u   /* INTERIOR (odd parity inside the pad) */
#define BSUBA 0x00008002u   /* BOUNDARY — subject A                 */
#define IVAL1 0x00008003u   /* INTERIOR                             */
#define BSUBB 0x00008004u   /* BOUNDARY — subject B                 */
#define IVAL2 0x00008005u   /* INTERIOR                             */
#define IVAL3 0x00008007u   /* INTERIOR                             */

/* Non-code separator: far above any segment, so it can never alias code. */
#define SEP   0x7f7f7f7fu

static const unsigned kQwords[] __attribute__((section("__TEXT,__const"),
                                               aligned(8), used)) = {
   SEP,   0u,
   IVAL0, 0u,
   BSUBA, 0u,
   IVAL1, 0u,
   BSUBB, 0u,
   IVAL2, 0u,
   IVAL3, 0u,
   SEP,   0u
};

static const unsigned kTable[] __attribute__((section("__TEXT,__const"),
                                              aligned(8), used)) = {
   SEP, SEP,
   0x00009002u, 0x00009004u, 0x00009006u, 0x00009008u,
   0x0000900au, 0x0000900cu, 0x0000900eu, 0x00009010u,
   0x00009012u, 0x00009014u, 0x00009016u, 0x00009018u,
   SEP, SEP
};

int main(void) {
   const volatile unsigned *q = kQwords;

   printf("qwords %08x %08x %08x %08x %08x %08x\n",
          q[2], q[4], q[6], q[8], q[10], q[12]);
   printf("hi %08x %08x %08x\n", q[5], q[9], q[13]);

   /* The interior words are the CONTROL: code_interior_alias alone already
    * protects them, in BOTH arms. If these move, the fixture is broken and the
    * gate is not what failed. */
   if (q[2] != IVAL0 || q[6] != IVAL1 || q[10] != IVAL2 || q[12] != IVAL3) {
      printf("FAIL interior control corrupted\n");
      exit(2);
   }
   /* The high halves must stay zero whatever happens. */
   if (q[5] != 0u || q[9] != 0u || q[13] != 0u) {
      printf("FAIL qword high half corrupted\n");
      exit(3);
   }
   /* The SUBJECTS: only the qword-high-half skip protects these. */
   if (q[4] != BSUBA || q[8] != BSUBB) {
      printf("FAIL u64 low half rebased\n");
      exit(1);
   }

   exit(42);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
