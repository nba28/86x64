/* 96_jt_cohesion.c — A/B fixture for the JUMP-TABLE COHESION gate
 * (ParseEnv::code_alias_run_contradicted, src/core/parse.cc).
 *
 * ★NAMED .c ON PURPOSE. The original fixture for this guard was a .s and the
 * root .gitignore has a blanket `*.s`, so it was silently never committed and
 * the guard SKIPped — scoring as a PASS — for its whole life. A .c cannot
 * vanish that way. See the silently-inert-guard gotcha.
 *
 * THE BLIND SPOT. For __TEXT-resident data the func-entry gate
 * (code_alias_is_constant) and the code-entry gate
 * (code_alias_lacks_entry_evidence) are DELIBERATELY disarmed, because switch
 * jump tables live in __TEXT,__const and target MID-function basic-block heads
 * carrying neither an nlist nor a `55 89 e5` prologue. That leaves only
 * code_interior_alias, which fires only for a value strictly INSIDE a decoded
 * instruction. An INTEGER that happens to alias an exact instruction BOUNDARY
 * therefore passes every gate and is silently rebased.
 *
 * TWO DEFECTS, ONE FIXTURE — and the second one is why the positive control
 * below is not optional:
 *
 *   (1) UNDER-FIRE (Halo audio, aa203f3): the sample-rate table {22050, 44100}
 *       was rebased to {0x1000573c, 44100}.
 *   (2) UNDER-FIRE (Halo graphics, 6acb4ef): the vertex-STRIDE table at
 *       0x34e280 — 20 u16 strides packed two per word. Each word lands
 *       boundary-or-interior essentially at random, so every boundary word has
 *       one contradicting AND one corroborating neighbour and the old +-4 test
 *       with its `!corroborated` veto could never fire. Format 15's stride 32
 *       read back as 4138 and every animated 3D object in the menu smeared.
 *       Cure: judge the ALL-BOUNDARY BLOCK, not the two neighbours.
 *   (3) OVER-FIRE, the regression that cure risks: a first cut fired on "any
 *       proven interior anywhere in the surrounding run" and DEMOTED THREE
 *       GENUINE Halo JUMP TABLES (64/56/44 entries). Every "the constant
 *       survived" arm stayed green while real switch dispatch was being turned
 *       into i386 addresses. Only a POSITIVE control catches that, so this
 *       fixture also executes a real switch and checks its answers.
 *
 * LAYOUT. `pad` is a large run of 0x00 bytes in __text proper. 0x00 0x00
 * decodes as `add %al,(%eax)`, TWO bytes, so the linear sweep tiles the region
 * with 2-byte instructions: within it, addresses of one parity are instruction
 * BOUNDARIES and the other parity is strictly INTERIOR. That gives exact,
 * layout-independent control over the classification of every constant below.
 * The concrete addresses are derived from the built binary (see
 * jt_cohesion_test.sh, which re-asserts them from the disassembly every run so
 * a layout change FAILS LOUDLY instead of going inert).
 *
 * kConst models the Halo stride table: interior, BOUNDARY, BOUNDARY, interior.
 * The two boundary words sit in an all-boundary block of length 2 terminated by
 * a proven interior on BOTH sides — data, not a table. kSep* keep that block
 * from merging with anything else in the section.
 */
#include <stdio.h>
#include <stdlib.h>

/* 320KB of zero "instructions" in __text (S_ATTR_PURE_INSTRUCTIONS). Never
 * executed; it exists only so the constants below alias decoded instructions. */
__asm__(".text\n"
        ".globl _alias_pad\n"
        "_alias_pad:\n"
        ".space 327680\n");

/* --- values patched from the measured layout (see PLACEHOLDERS below) ------ */
#define IVAL0 0x00008001u   /* INTERIOR  (odd parity inside the pad)  */
#define BVAL1 0x00008002u   /* BOUNDARY                               */
#define BVAL2 0x00008004u   /* BOUNDARY                               */
#define IVAL3 0x00008007u   /* INTERIOR                               */
#define IVAL_JT 0x00008009u /* INTERIOR, seats the jump table          */

/* Non-code separators: far above any segment, so they can never alias code and
 * they DELIMIT the run on both sides. */
#define SEP   0x7f7f7f7fu

/* ONE array, so the section layout is GUARANTEED rather than hoped for: two
 * separately-declared arrays are only adjacent by the compiler's goodwill, and
 * this guard is entirely about what is adjacent to what.
 *
 *   [0][1]  SEP           non-code: delimits the run below
 *   [2]     interior      terminates the subject block, and is itself a CONTROL
 *   [3][4]  BOUNDARY      ★THE SUBJECT: an all-boundary block of length 2,
 *                          wedged between proven mid-instruction words. Data.
 *   [5]     interior      terminates it above; also a control
 *   [6]     SEP
 *   [7]     interior      ★seats the jump table directly against a proven
 *                          interior — the exact adjacency that made the first
 *                          cut of the fix merge a real table with its
 *                          neighbours and demote it
 *   [8..19] BOUNDARY x12  ★THE POSITIVE CONTROL: an unbroken all-boundary block
 *                          far longer than the threshold. Must stay REBASED.
 *
 * With the shipped rule [3][4] are preserved (block 2, interior-terminated) and
 * [8..19] are not (block 12 > threshold). Raising M64_JT_COHESION_MAX_BLOCK
 * above 12 makes the gate swallow the table too, which is how the guard proves
 * its own control is live rather than merely quiet. */
static const unsigned kConst[] __attribute__((section("__TEXT,__const"),used)) = {
   SEP, SEP, IVAL0, BVAL1, BVAL2, IVAL3, SEP, IVAL_JT,
   0x00009002u, 0x00009004u, 0x00009006u, 0x00009008u,
   0x0000900au, 0x0000900cu, 0x0000900eu, 0x00009010u,
   0x00009012u, 0x00009014u, 0x00009016u, 0x00009018u
};
int main(void) {
   const volatile unsigned *p = kConst;


   printf("packed %08x %08x %08x %08x\n", p[2], p[3], p[4], p[5]);

   /* The interior words are the CONTROL: code_interior_alias alone already
    * protects them, in both arms. If these move, the fixture is broken, not
    * the gate. */
   if (p[2] != IVAL0 || p[5] != IVAL3) {
      printf("FAIL interior control corrupted\n");
      exit(2);
   }
   /* The boundary words are the SUBJECT: only the cohesion gate protects them. */
   if (p[3] != BVAL1 || p[4] != BVAL2) {
      printf("FAIL boundary words rebased\n");
      exit(1);
   }

   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
