/* 60_data_const_text_alias.c — __DATA constants that ALIAS a __text address.
 *
 * Fixed-address i386 execs carry no rebase info, so the translator's
 * DataParser HEURISTICALLY treats any aligned 4-byte __DATA word whose value
 * falls inside a segment as a pointer and relocates it. A constant that merely
 * aliases a mid-function __text address was falsely "rebased" to a translated
 * code address: Quinn's piece size {4,4} (= 0x00040004, inside Quinn's __text)
 * became a slide-dependent {w=8156,h=657}, so -[QuinnMatrix usedRect] scanned
 * 1.7MB off a 16-byte piece shape — blank board + non-deterministic SIGBUS.
 * Thousands of words in Quinn's __DATA/__OBJC alias __text this way.
 *
 * The gate: with local text symbols present (statics symboled), a genuine
 * __DATA code pointer targets a function entry and matches a func_syms nlist,
 * so a word pointing MID-function inside an S_ATTR_*_INSTRUCTIONS section with
 * NO symbol at its value is a constant and must pass through UNCHANGED.
 * Section-granular: pointers into __TEXT's DATA sections (__cstring/__const)
 * stay permissive — selector refs/string ptrs legitimately have no symbols.
 *
 * The .space blob stretches __text past 0x50000 so all three constants below —
 * including Quinn's exact 0x00040004 — are guaranteed to alias instruction
 * bytes regardless of test-binary size. `fp` is a genuine pointer to a static
 * (local-symbol) function: it must STILL be detected and relocated, or the
 * indirect call lands on a stale i386 vmaddr.
 */
#include <stdio.h>
#include <stdlib.h>

/* 320KB of zero "instructions" in __text proper (S_ATTR_PURE_INSTRUCTIONS):
 * never executed, only there so small constants alias mid-code addresses. */
__asm__(".text\n\t.space 327680\n");

static void cb(void) { puts("cb called"); }

static void (*fp)(void) = cb;                /* genuine code ptr: keep rebasing */
static unsigned kConstEven = 0x3004;         /* 4-aligned text alias -> constant */
static unsigned kConstOdd  = 0x2005;         /* odd text alias -> constant */
static struct { short w, h; } kSize = { 4, 4 };  /* 0x00040004: Quinn's exact
                                                  * piece-size word */

int main(void) {
   printf("even 0x%x\n", kConstEven);
   printf("odd 0x%x\n", kConstOdd);
   printf("size %d %d\n", kSize.w, kSize.h);
   fp();
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
