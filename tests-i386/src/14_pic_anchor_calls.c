/*
 * 14_pic_anchor_calls — PIC anchor preserved across external calls, then a
 * conditional global WRITE on a slow path. This mirrors iPhoto's 8th-blocker
 * crash function: the anchor lives in callee-saved %ebx (because it must
 * survive two `call`s that clobber the caller-saved regs), is spilled/reloaded
 * around each call, and a `flag = 0` store on a branch-reached path uses it.
 *
 * If DetectPicAnchoredDisps mis-tracks the anchor across the spill/reload +
 * branch, the flag store's translated rip-relative disp targets read-only
 * __TEXT (the live bug) and the program SIGSEGVs / writes garbage.
 *
 * extern-call shims (g_helper / g_other) are real out-of-line calls so the
 * compiler is forced to keep the PIC base in a callee-saved register across
 * them, exactly like the iPhoto site (two objc_msgSend-style stub calls).
 *
 * Expected exit code: 6  (g_x=2 + g_y=4 after the slow path runs).
 */
extern void exit(int status);

static volatile int g_x;
static volatile int g_y;
static volatile int g_z;

__attribute__((noinline)) int g_helper(int v) { return v + 1; }   /* nonzero */
__attribute__((noinline)) int g_other(int v)  { return v + 1; }   /* nonzero */

__attribute__((noinline)) int slow(int a) {
    int r = g_helper(a);     /* call 1 — clobbers caller-saved, anchor -> %ebx */
    if (r) {
        int s = g_other(r);  /* call 2 */
        if (!s) {
            g_z = 9;         /* should NOT run when s != 0 */
        }
        g_x = 2;             /* PIC write after both calls (anchor reloaded) */
    }
    g_y = 4;                 /* PIC write on the common tail */
    return r;
}

int main(void) {
    slow(1);
    exit(g_x + g_y + g_z);
    return 0;
}
