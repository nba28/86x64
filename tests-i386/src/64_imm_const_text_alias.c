/* 64_imm_const_text_alias.c — instruction IMMEDIATES that ALIAS a __text
 * address. The instruction-side twin of 60_data_const_text_alias.
 *
 * A fixed-load-address i386 exec references its own globals/code by bare
 * absolute immediate, so the translator heuristically relocates any 32-bit
 * immediate whose value falls inside a segment. An integer constant that
 * merely aliases a mid-function __text address was falsely rewritten to
 * `lea reg,[rip+disp]` / a slid store. Real-world victim: EVERY GCC-4.x
 * C++ static-init stub passes init priority 65535 in edx
 * (`__GLOBAL__I_*: movl $0xffff,%edx; movl $1,%eax;
 *   jmp __static_initialization_and_destruction_0`);
 * 0xffff aliases __text, the relocated edx failed the callee's
 * `cmpl $0xffff,%edx` priority test, and ALL 1063 of Civ IV's C++ static
 * constructors were silently skipped — the first use of a never-constructed
 * static std::set (MSG_Mac sCallbackList) crashed in _Rb_tree_decrement on
 * the zeroed header node (fault addr 0x4).
 *
 * The gate (shared with DataParser, ParseEnv::code_alias_is_constant): with
 * local text symbols present, a genuine code-pointer immediate targets a
 * function ENTRY and matches a func_syms nlist; an immediate landing
 * MID-function inside an S_ATTR_*_INSTRUCTIONS section with NO symbol at its
 * value is an integer constant and must pass through UNCHANGED.
 *
 * Covers the three heuristic sites:
 *   B: `mov $imm32, %reg`      (MOV_GPRv_IMMv — the Civ IV shape)
 *   C: `mov $imm32, d(%esp)`   (stack-arg store)
 *   A: `mov $imm32, abs32`     (store to an absolute global)
 * plus the positive controls: a static-function pointer through each shape
 * must STILL be relocated (call through it afterwards).
 */
#include <stdio.h>
#include <stdlib.h>

/* 320KB of zero "instructions" in __text proper (S_ATTR_PURE_INSTRUCTIONS):
 * never executed, only there so 0xffff/0x40004 alias mid-code addresses. */
__asm__(".text\n\t.space 327680\n");

static void cb(void) { puts("cb called"); }

static unsigned g_slot;                     /* site A integer destination */
static void (*g_fp)(void);                  /* site A pointer destination */

/* Site C: cdecl callee reads its args from the stack; the caller sets them up
 * with `movl $imm32, (%esp)` / `movl $imm32, 4(%esp)`. */
static void check_args(unsigned v, void (*fn)(void)) {
   printf("arg 0x%x\n", v);
   fn();
}

int main(void) {
   /* Site B: the exact Civ IV static-init-priority shape. The asm block
    * forces `movl $0xffff, %edx` (ba ff ff 00 00) with 0xffff aliasing
    * __text; a mis-relocation turns it into lea edx,[rip+…] and the value
    * printed changes. */
   unsigned prio;
   __asm__ volatile ("movl $0xffff, %%edx\n\tmovl %%edx, %0"
                     : "=r"(prio) : : "edx");
   printf("prio 0x%x\n", prio);

   /* Site B, second constant: Quinn's 0x00040004 as a register immediate. */
   unsigned packed;
   __asm__ volatile ("movl $0x40004, %%ecx\n\tmovl %%ecx, %0"
                     : "=r"(packed) : : "ecx");
   printf("packed 0x%x\n", packed);

   /* Site C: integer stack arg aliasing __text + fn-ptr stack arg (positive
    * control: cb is a static fn with a local symbol -> still relocated). */
   check_args(0xffffu, cb);

   /* Site A: integer store to an absolute global (c7 05 abs32 imm32) — the
    * imm32 must stay a constant while the DESTINATION abs32 relocates. */
   g_slot = 0x40004u;
   printf("slot 0x%x\n", g_slot);

   /* Site A positive control: fn-ptr store to an absolute global — the imm32
    * IS a pointer (function entry, symboled) and must still relocate. */
   g_fp = cb;
   g_fp();

   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
