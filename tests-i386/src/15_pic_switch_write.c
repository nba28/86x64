/*
 * 15_pic_switch_write — KNOWN-FAILING REPRO (no expected/ file → run-only SKIPs
 * it, keeping the suite green; the built binary is left for manual repro).
 *
 * REPRODUCES A REAL GENERIC TRANSLATOR BUG: clang i386 PIC jump tables are not
 * relocated, so the translated dispatch jumps to garbage and crashes.
 *
 * i386 codegen (clang, default PIC) for a dense switch:
 *     call .L; pop %eax            ; %eax = anchor (a __TEXT vmaddr)
 *     mov  %eax, -0xc(%ebp)        ; spill anchor
 *     ...
 *     mov  -0xc(%ebp), %eax        ; reload anchor
 *     mov  -0x8(%ebp), %ecx        ; %ecx = sel
 *     mov  0xc5(%eax,%ecx,4), %ecx ; %ecx = jumptable[sel] = (case - anchor)
 *     add  %ecx, %eax             ; %eax = anchor + (case - anchor) = case
 *     jmp  *%eax
 * The jump table sits at anchor+0xc5 in __TEXT and holds anchor-relative
 * deltas. TWO things break in the M64 output (verified via otool on the
 * translated .dylib + lldb: EXC_BAD_ACCESS code=2 executing at a low
 * zero-filled addr, i.e. jumped to garbage):
 *   (1) DetectPicAnchoredDisps (section.cc ~L449) SKIPS operands with an index
 *       register (`if (indexreg != XED_REG_INVALID) continue;`), so the
 *       anchor-relative jump-table READ disp `0xc5` is emitted VERBATIM — but
 *       __TEXT grew during translation, so anchor+0xc5 no longer points at the
 *       table.
 *   (2) The jump-table CONTENTS (the `case - anchor` deltas, stored in __TEXT)
 *       are i386-layout deltas, never recomputed for the M64 layout.
 * FIX DESIGN (next session): recognize `disp(%anchor,%idx,scale)` indexed
 * anchor loads → relocate disp to (table_M64 - anchor_M64) keeping base+index
 * (NOT rip-relative — the anchor reg supplies the base); AND parse the jump
 * table as N anchor-relative code-offset entries and rebuild each as
 * (case_M64 - anchor_M64). Table size from the `cmp $N; ja` bound just above
 * the dispatch. NOTE: distinct from iPhoto's 8th blocker — iPhoto's GCC-era
 * switches use compare-chains (`leal -1(%edx),%eax; cmp $0x17,%eax; jbe`) +
 * large-disp PIC (`0x53e257(%ebx)`), NOT clang jump tables. But this is a
 * generic correctness bug that breaks any clang-compiled i386 switch.
 *
 * Would-be exit code if fixed: 42  (g[0..7]=1..8 sum 36, + g_done?6).
 */
extern void exit(int status);

static volatile int g[8];
static volatile int g_done;

__attribute__((noinline)) int dispatch(int sel) {
    switch (sel) {
    case 0: g[0] = 1;  break;
    case 1: g[1] = 2;  break;
    case 2: g[2] = 3;  break;
    case 3: g[3] = 4;  break;
    case 4: g[4] = 5;  break;
    case 5: g[5] = 6;  break;
    case 6: g[6] = 7;  break;
    case 7: g[7] = 8;  break;
    default: return -1;
    }
    g_done = 1;          /* PIC flag write on the common tail after the switch */
    return 0;
}

int main(void) {
    int sum = 0;
    for (int i = 0; i < 8; i++) {
        dispatch(i);
    }
    for (int i = 0; i < 8; i++) sum += g[i];   /* 1+2+..+8 = 36 */
    sum += g_done ? 6 : 0;                      /* + 6 = 42 */
    exit(sum);
    return 0;
}
