/* 99_gap_raw — does a RAW native call (no libabiconv bridge) speak on its
 * first hit?
 *
 * WHY THIS EXISTS. static-interpose redirects a translated bind to libabiconv
 * only when a bridge exists; the rest stay on the native framework and become
 * raw cross-ABI calls (i386 pushes a 4-byte return address and stack args, the
 * native callee reads registers and `ret`s 8 bytes) -- the unbridged native-call bug,
 * found one crash at a time (Portal 2 libsteam_api, Civ IV dladdr). gap.c now
 * reports the first call of each such slot through a one-shot stub armed
 * when the image loads (dyld4 has already bound even the lazy slots by then).
 *
 * _NSGetArgc is native libSystem with no bridge. It takes no arguments, so the
 * pushed 0 becomes the HIGH half of its 8-byte `ret` and control comes back to
 * the true return address: the fixture survives and prints survived=1.
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern int *_NSGetArgc(int pad);

int main(void)
{
   _NSGetArgc(0);
   printf("survived=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
