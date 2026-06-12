/*
 * 13_pic_global_write — PIC global-WRITE across multiple control-flow paths.
 *
 * Companion to 04 (PIC global READ) and 12 (absolute static write). This one
 * forces clang's i386 PIC codegen (default, no -fno-pic) to emit get_pc_thunk
 * anchored WRITES (`movl $imm, disp(%ebx)`) on several control-flow paths —
 * the shape that triggers the 8th-blocker: a flag store whose linearly-tracked
 * PIC anchor goes stale across branches, so the translated rip-relative disp
 * targets read-only __TEXT instead of __DATA/__bss.
 *
 * Structure mirrors iPhoto's crash function: an early-return fast path plus a
 * slow path reached by a forward branch, both touching globals, with the
 * callee-saved %ebx holding the anchor. If the anchor is mis-tracked on any
 * path, the corresponding global write lands at the wrong address and the
 * checksum (and exit code) changes.
 *
 * Expected exit code: 7  (g_a=1 + g_b=2 + g_c=4, summed below).
 */
extern void exit(int status);

static volatile int g_a;
static volatile int g_b;
static volatile int g_c;

__attribute__((noinline)) int slow(int x) {
    /* multiple exit paths, each writing a different global via the anchor */
    if (x < 0) {
        g_a = 1;
        return 0;
    }
    if (x == 0) {
        g_b = 2;          /* reached by a branch that bypasses the x<0 store */
        return 0;
    }
    g_c = 4;              /* fall-through path */
    return 0;
}

int main(void) {
    slow(-1);
    slow(0);
    slow(1);
    exit(g_a + g_b + g_c);
    return 0;
}
