/* 99_sizewindow_guard — SizeWindow on a SHOWN document window must not reach
 * the native resize: on modern macOS a size change of a shown Carbon window
 * forces a CGS context for its NSCarbonWindow and AppKit traps (NSCGSPanic,
 * SIGILL). Call of Duty 4 resizes its GL window this way on every video
 * restart, after HideWindow (which does not help).
 *
 * ON exits 42: the call returns and the window keeps working. OFF
 * (M64_NO_SIZEWINDOW_GUARD=1, run time): the native SizeWindow -> SIGILL.
 * GUI: needs a real (briefly shown) window. check-gui only. */
extern void exit(int);
extern int printf(const char *, ...);
typedef struct { short top, left, bottom, right; } Rect;
int  CreateNewWindow(unsigned cls, unsigned attrs, const Rect *bounds, void **win);
void ShowWindow(void *win);
void HideWindow(void *win);
void SizeWindow(void *win, short w, short h, unsigned char update);
void DisposeWindow(void *win);
int main(void) {
   Rect r = { 100, 100, 340, 420 };
   void *win = 0;
   if (CreateNewWindow(6 /*kDocumentWindowClass*/, 0, &r, &win) || !win) exit(1);
   ShowWindow(win);
   HideWindow(win);                      /* Call of Duty 4's order */
   SizeWindow(win, 640, 480, 1);
   printf("survived\n");
   DisposeWindow(win);
   exit(42);
}
