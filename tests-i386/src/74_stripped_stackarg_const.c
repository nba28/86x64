/* 74_stripped_stackarg_const.c — regression guard for the stack-arg imm32
 * constant mis-relocation in TEXT-SYMBOL-STRIPPED binaries.
 *
 * This is exactly how Halo's CPU-speed self-check misfired (2026-07-04): the
 * i386 divisor `movl $0x000F4240, 0x8(%esp)` (the integer constant 1000000, fed
 * to ___udivdi3 as freq/1e6) got RELOCATED as if it were a __TEXT pointer,
 * because 0xF4240 happens to alias the __TEXT vmaddr range. At runtime the
 * load-slide was added → 49557824 → MHz computed as 48 → "requires a faster
 * CPU" nag + _exit.
 *
 * ROOT: the stack-arg imm32 heuristic (core/instruction.cc, `mov [esp+d],imm32`)
 * relocates an immediate that lands in a mapped segment, guarded by
 * ParseEnv::code_alias_is_constant() — which is DISARMED when the image has no
 * local text symbols (parse.cc: `if (!have_local_text_syms) return false`).
 * Halo (and this test) are fully text-symbol-stripped, so the guard never fires
 * and an integer constant that aliases __text is wrongly slid.
 *
 * REPRO: a minimal C main() has NO local text symbols (only external _main), so
 * have_local_text_syms=false. `divisor` is volatile so clang stores it with
 * `movl $K, N(%esp)` and emits a real ___udivdi3 call. K (0x1f00) is chosen to
 * land inside THIS binary's __TEXT,__text vmaddr range (verified post-build:
 * __text starts at ~0x1ef0). dividend = K * 500, so the correct quotient is
 * exactly 500; if K is mis-relocated (slid), the quotient is wrong.
 *
 * The printf format-string pointer (a GENUINE __cstring pointer passed the same
 * `movl $&fmt,(%esp)` way) must STILL relocate for the output to appear — so
 * this simultaneously guards that the fix does not over-reject legitimate
 * string/data stack args (it only rejects PURE_INSTRUCTIONS-section targets).
 *
 * Expected (fixed): q=500.  With the bug: q != 500 (divisor corrupted).
 */

extern int  printf(const char *, ...);
extern void exit(int);

#define K 0x1f00u          /* integer constant that aliases __TEXT,__text */

int main(void) {
   volatile unsigned long long divisor  = (unsigned long long)K;      /* movl $K, N(%esp) */
   volatile unsigned long long dividend = (unsigned long long)K * 500ULL;
   unsigned long long q = dividend / divisor;                          /* ___udivdi3 */
   printf("q=%d\n", (int)q);
   exit(0);
}
