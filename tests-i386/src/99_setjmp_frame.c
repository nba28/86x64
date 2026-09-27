/* setjmp-frame: an i386 jmp_buf is 72 bytes. A setjmp that writes the native
 * 148-byte x86_64 layout tramples the frame above the buffer (saved esi/edi/
 * ebx/ebp, return address) -- PvZ lost its saved esi this way. The value kept
 * in a callee-saved register across f() must survive setjmp + longjmp. */
/* the SL sysroot mount carries no setjmp.h; Darwin i386 layout: */
typedef int jmp_buf[18];
int setjmp(jmp_buf);
void longjmp(jmp_buf, int) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));   /* -e _main: no return address */

static jmp_buf *g_env;

__attribute__((noinline)) static void thrower(int v) { longjmp(*g_env, v); }

__attribute__((noinline)) static int f(int v) {
    jmp_buf env;
    g_env = &env;
    int r = setjmp(env);
    if (r == 0) thrower(v);
    return r;
}

int main(void) {
    volatile int keep = 31;
    int k = keep;                            /* lives in a callee-saved reg */
    int r = f(11);                           /* longjmp delivers 11 */
    int z = f(0);                            /* longjmp(0) resumes as 1 */
    exit((r == 11 && z == 1) ? k + r : 1);   /* 42 */
}
