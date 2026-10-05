/* 99_get_window_resize_limits — GetWindowResizeLimits is 32-bit only; its abigen
 * bridge called the missing native (NULL). Call of Duty 4's setup dialog reads it.
 * ON exits 42: unimpErr, both HISizes defined (zeroed over garbage).
 * OFF (M64_NO_GETWINRESIZE=1, run time): the native-NULL forward -> crash. */
extern void exit(int);
extern int printf(const char *, ...);
typedef struct { float width, height; } HISize;
int GetWindowResizeLimits(void *win, HISize *outMin, HISize *outMax);
int main(void) {
   HISize mn = { 123.f, 456.f }, mx = { 789.f, 1011.f };
   int e = GetWindowResizeLimits((void *)0, &mn, &mx);
   printf("err=%d min=%gx%g max=%gx%g\n", e, mn.width, mn.height, mx.width, mx.height);
   exit(e == -4 && mn.width == 0 && mn.height == 0 && mx.width == 0 && mx.height == 0 ? 42 : 1);
}
