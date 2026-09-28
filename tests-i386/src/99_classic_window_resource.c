/* 99_classic_window_resource — the classic window creators that 64-bit Carbon
 * dropped (GetNewCWindow / GetNewWindow / NewWindow) must build a real window.
 *
 * GetNewCWindow reads its parameters from a BIG-endian 'WIND' resource (built
 * by Rez from 99_classic_window_resource.r into the resource fork of
 * build/99_classic_window_resource.rsrc). Asserts the window's content bounds
 * and refCon come from the resource, and NewWindow builds one from arguments.
 * Carbon isn't in the i386 sysroot: everything is undefined dynamic_lookup and
 * static-interpose binds it to libabiconv's shims at translate time.
 * Exit 42 = all checks passed. */
extern int printf(const char *, ...);
extern void exit(int);

typedef struct { short top, left, bottom, right; } Rect;
typedef void *WindowRef;
typedef struct { unsigned char hidden[80]; } FSRef;

extern int   FSPathMakeRef(const unsigned char *path, FSRef *ref, unsigned char *isDir);
extern short FSOpenResFile(const FSRef *ref, signed char permission);
extern WindowRef GetNewCWindow(short id, void *storage, WindowRef behind);
extern WindowRef NewWindow(void *storage, const Rect *bounds, const unsigned char *title,
                           unsigned char visible, short procID, WindowRef behind,
                           unsigned char goAway, long refCon);
extern int   GetWindowBounds(WindowRef w, unsigned short region, Rect *out);
extern long  GetWRefCon(WindowRef w);
extern void  DisposeWindow(WindowRef w);

#define kWindowContentRgn 33

static int check(const char *what, WindowRef w, const Rect *want, long refcon) {
   Rect r = { 0, 0, 0, 0 };
   if (!w) { printf("%s: NULL window\n", what); return 1; }
   GetWindowBounds(w, kWindowContentRgn, &r);
   long rc = GetWRefCon(w);
   int ok = r.top == want->top && r.left == want->left && r.bottom == want->bottom &&
            r.right == want->right && rc == refcon;
   printf("%s: bounds %d,%d,%d,%d refcon 0x%lx %s\n", what, r.top, r.left, r.bottom,
          r.right, (unsigned long)rc, ok ? "ok" : "WRONG");
   return !ok;
}

int main(void) {
   FSRef ref;
   if (FSPathMakeRef((const unsigned char *)"build/99_classic_window_resource.rsrc", &ref, 0) != 0 ||
       FSOpenResFile(&ref, 1 /* fsRdPerm */) == -1) {
      printf("cannot open the WIND resource file\n");
      exit(2);
   }
   int bad = 0;
   Rect want = { 100, 120, 300, 420 };
   WindowRef w = GetNewCWindow(128, 0, (WindowRef)-1);
   bad |= check("GetNewCWindow", w, &want, 0x12345678);
   if (w) { DisposeWindow(w); }

   Rect nb = { 60, 70, 160, 270 };
   WindowRef n = NewWindow(0, &nb, (const unsigned char *)"\x02Hi", 0, 0, (WindowRef)-1, 1, 7);
   bad |= check("NewWindow", n, &nb, 7);
   if (n) { DisposeWindow(n); }
   exit(bad ? 1 : 42);
}
