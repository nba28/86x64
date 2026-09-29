/* 99_classic_input_coords — guard for the classic GLOBAL-vs-LOCAL coordinate
 * contract (src/abiconv/classic_input_coords.c, osutil_shim.c, qd_shim.c).
 *
 * THE DEFECT. Classic Mac OS draws a hard line between GLOBAL (screen) and
 * LOCAL (current GrafPort) coordinates:
 *
 *     GetGlobalMouse(&p)  -> screen coordinates
 *     GetMouse(&p)        -> coordinates LOCAL TO THE CURRENT PORT
 *     GlobalToLocal(&p)   -> subtract the port origin
 *     LocalToGlobal(&p)   -> add it back
 *
 * Our implementations returned the GLOBAL position from BOTH mouse calls and
 * made both conversions identity no-ops. That is correct only while the port
 * origin is (0,0) — true for the offscreen GWorld case they were written for,
 * FALSE for a real on-screen window, which is inset by its title bar and by
 * wherever it sits on the desktop. An app that hit-tests its own menu with
 * GetMouse then compares a SCREEN point against WINDOW-LOCAL item rectangles
 * and finds the pointer inside nothing. Symptom: the cursor moves perfectly and
 * nothing is clickable.
 *
 * WHAT THIS ASSERTS. Make a real Carbon window at a known, non-zero position,
 * read back its content origin through the bridge, and then check both
 * directions against that origin — positively in BOTH arms, so neither arm can
 * pass by accident:
 *
 *   ON  : LocalToGlobal((0,0)) == content origin, GlobalToLocal is its exact
 *         inverse, and GetGlobalMouse - GetMouse == content origin.
 *   OFF : (M64_NO_CLASSIC_INPUT_FIX=1) all three are identity — LocalToGlobal
 *         leaves (0,0) alone and GetMouse == GetGlobalMouse.
 *
 * The fixture refuses to report a verdict if the window lands at origin (0,0),
 * where identity and correct behaviour are indistinguishable.
 *
 * NOT COVERED: StillDown()/WaitMouseUp(), the other half of d4756ac. They now
 * report CGEventSourceButtonState instead of a constant 0, but with no button
 * physically down that is also 0 — indistinguishable from the old stub without
 * synthesising input, which this suite deliberately does not do. Stated here
 * rather than faked.
 *
 * SIDE EFFECT: this briefly shows a 400x300 window. It takes no input and is
 * disposed immediately, but do not run the suite while a GUI target is under
 * live test.
 *
 * The Window Manager and QuickDraw entry points are undefined dynamic_lookup
 * imports resolved at translate time by static-interpose to libabiconv's
 * ___<name> trampolines.
 */

extern int  usleep(unsigned int usec);
extern long write(int fd, const void *buf, unsigned long n);
extern void exit(int status);

typedef struct { short top, left, bottom, right; } Rect;
typedef struct { short v, h; } Point;
typedef void *WindowRef;

extern int  CreateNewWindow(unsigned int cls, unsigned int attrs,
                            const Rect *bounds, WindowRef *outWindow);
extern int  GetWindowBounds(WindowRef w, unsigned short region, Rect *out);
extern void ShowWindow(WindowRef w);
extern void SelectWindow(WindowRef w);
extern void HideWindow(WindowRef w);
extern void DisposeWindow(WindowRef w);

extern void GlobalToLocal(Point *p);
extern void *GetWindowPort(WindowRef w);
extern void SetPort(void *port);
extern void LocalToGlobal(Point *p);
extern void GetMouse(Point *p);
extern void GetGlobalMouse(Point *p);

#define kDocumentWindowClass  6u
#define kWindowNoConstrain    0x80000000u
#define kWindowContentRgn     33

static void say(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    write(1, s, n);
}
static void sayf(const char *tag, int v) {   /* "<tag>=<v>\n", small ints only */
    char buf[32]; int i = 0, neg = 0; unsigned int u;
    while (*tag) { buf[i++] = *tag++; }
    buf[i++] = '=';
    if (v < 0) { neg = 1; u = (unsigned int)(-v); } else { u = (unsigned int)v; }
    { char d[12]; int k = 0;
      do { d[k++] = (char)('0' + (u % 10)); u /= 10; } while (u);
      if (neg) buf[i++] = '-';
      while (k) { buf[i++] = d[--k]; } }
    buf[i++] = '\n';
    write(1, buf, (unsigned long)i);
}

int main(void) {
    /* Deliberately off-origin and inside any plausible screen. kWindowNoConstrain
     * asks the Window Manager not to move it. */
    Rect want = { 200, 300, 500, 700 };   /* top, left, bottom, right */
    Rect content = { 0, 0, 0, 0 };
    WindowRef w = (WindowRef)0;
    Point p, g, l, g2;
    int tries;

    if (CreateNewWindow(kDocumentWindowClass, kWindowNoConstrain, &want, &w) != 0 || !w) {
        say("window=0\n");
        exit(1);
    }
    ShowWindow(w);
    SelectWindow(w);
    usleep(150000);                       /* let the Window Manager place it */

    /* The reference the fix is supposed to use: this window's content region. */
    if (GetWindowBounds(w, kWindowContentRgn, &content) != 0) {
        say("window=0\n");
        HideWindow(w); DisposeWindow(w);
        exit(1);
    }
    say("window=1\n");
    sayf("origin_h", content.left);
    sayf("origin_v", content.top);
    /* At origin (0,0) identity and correctness are the same answer. */
    say((content.left != 0 || content.top != 0) ? "origin_nonzero=1\n"
                                                : "origin_nonzero=0\n");

    /* --- LocalToGlobal: (0,0) local is the content origin in global coords --- */
    p.v = 0; p.h = 0;
    LocalToGlobal(&p);
    say((p.h == content.left && p.v == content.top) ? "l2g_global=1\n" : "l2g_global=0\n");
    say((p.h == 0 && p.v == 0)                      ? "l2g_identity=1\n" : "l2g_identity=0\n");

    /* --- GlobalToLocal must be its exact inverse --------------------------- */
    GlobalToLocal(&p);
    say((p.h == 0 && p.v == 0) ? "roundtrip=1\n" : "roundtrip=0\n");

    /* --- GetMouse is LOCAL, GetGlobalMouse is GLOBAL ------------------------ */
    /* Read global-local-global and accept only a coherent sample, so a cursor
     * that moves mid-read cannot decide the verdict either way. */
    l.v = l.h = g.v = g.h = 0;
    for (tries = 0; tries < 8; tries++) {
        GetGlobalMouse(&g);
        GetMouse(&l);
        GetGlobalMouse(&g2);
        if (g.h == g2.h && g.v == g2.v) break;
        usleep(20000);
    }
    say((g.h == g2.h && g.v == g2.v) ? "mouse_stable=1\n" : "mouse_stable=0\n");
    say((g.h - l.h == content.left && g.v - l.v == content.top)
        ? "mouse_local=1\n" : "mouse_local=0\n");
    say((g.h == l.h && g.v == l.v) ? "mouse_identity=1\n" : "mouse_identity=0\n");

    /* --- the CURRENT PORT decides, not the active window ------------------
     * A second window elsewhere becomes active while the app draws into (and
     * converts against) the first -- PvZ after fullscreen->windowed, 2026-09-29. */
    {
        Rect want2 = { 120, 60, 320, 360 };
        WindowRef w2 = (WindowRef)0;
        if (CreateNewWindow(kDocumentWindowClass, kWindowNoConstrain, &want2, &w2) == 0 && w2) {
            ShowWindow(w2);
            SelectWindow(w2);
            usleep(150000);
            SetPort(GetWindowPort(w));
            p.v = 0; p.h = 0;
            LocalToGlobal(&p);
            say((p.h == content.left && p.v == content.top) ? "port_wins=1\n" : "port_wins=0\n");
            HideWindow(w2); DisposeWindow(w2);
        } else {
            say("port_wins=0\n");
        }
    }

    HideWindow(w);
    DisposeWindow(w);
    say("done=1\n");
    exit(0);
    return 0;
}
