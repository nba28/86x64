/* 99_carbon_window_compositing — guard for the 64-bit Window Manager COMPOSITING
 * contract (src/abiconv/carbon_window_shim.c).
 *
 * 64-bit HIToolbox deleted the non-compositing (classic GrafPort-drawn) window
 * model.  NewWindowCommon's first act is `btl $0x13,%r14d / jae` on the
 * attributes, and a caller without kWindowCompositingAttribute (bit 19) gets
 * errUnsupportedWindowAttributesForClass (-5601) for EVERY class — even with no
 * other attribute requested.  That bit did not exist before Mac OS X 10.2 and
 * stayed opt-in through 10.6, so essentially every 32-bit-era Carbon app that
 * makes its own window fails on modern macOS with a NULL WindowRef.
 *
 * Live failure: Halo CE's render-window setup (i386 0x2c2522) calls
 *   fullscreen: CreateNewWindow(kPlainWindowClass 13,    kWindowNoConstrain 0x80000000)
 *   windowed:   CreateNewWindow(kDocumentWindowClass 6,  0x82000009)
 * and `test eax,eax / jne` on the returned WindowRef; both returned -5601 with
 * outWindow NULL, so the app took its error path and put up an alert instead of
 * a window.  The two class/attribute pairs below are Halo's, verbatim.
 *
 * The shim ORs kWindowCompositingAttribute in and forwards to the genuine native
 * CreateNewWindow, so what is asserted is a REAL window, not a status code:
 * outWindow non-NULL, and the content bounds read back through a second bridged
 * call (GetWindowBounds, which unwraps the arena handle) equal the Rect that was
 * passed down.  That pins the whole marshalling chain the bridge is responsible
 * for — the two zero-extended scalars, the `const Rect *` (4 packed SInt16,
 * passed by address), and the WindowRef out-parameter's wrap/copy-back.
 *
 * The third case is the DEGRADE path: kPlainWindowClass with decorations the
 * class does not offer is rejected (-5601) even WITH compositing, where 10.6
 * would have made a plain window; the shim retries bare-compositing so the app
 * still gets a window.  Its bounds are not asserted (no noConstrain attribute,
 * so the Window Manager may move it onto a screen).
 *
 * A/B: run the translated binary with M64_NO_CARBON_COMPOSITING=1 and every
 * status becomes -5601 with a NULL window — see carbon_window_compositing_test.sh.
 *
 * Carbon/HIToolbox isn't in the i386 sysroot, so these are undefined
 * dynamic_lookup imports resolved at translate time by static-interpose ->
 * libabiconv's ___CreateNewWindow (hand shim) / ___GetWindowBounds /
 * ___DisposeWindow (abigen legacy bridges).
 */

extern int printf(const char *, ...);
extern void exit(int status);

typedef struct { short top, left, bottom, right; } Rect;
typedef void *WindowRef;

extern int  CreateNewWindow(unsigned int cls, unsigned int attrs,
                            const Rect *bounds, WindowRef *outWindow);
extern int  GetWindowBounds(WindowRef w, unsigned short region, Rect *out);
extern void DisposeWindow(WindowRef w);

#define kWindowContentRgn 33

/* Halo's exact requests. */
#define kPlainWindowClass     13u
#define kDocumentWindowClass   6u
#define kWindowNoConstrain    0x80000000u
#define HALO_WINDOWED_ATTRS   0x82000009u   /* closeBox|collapseBox|stdHandler|noConstrain */

static int probe(const char *tag, unsigned int cls, unsigned int attrs,
                 const Rect *want, int check_bounds)
{
    WindowRef w = (WindowRef)0;
    int st = CreateNewWindow(cls, attrs, want, &w);
    printf("%s_status=%d\n", tag, st);
    printf("%s_window=%d\n", tag, w != 0);
    if (check_bounds) {
        Rect got = { 0, 0, 0, 0 };
        int gb = (st == 0 && w) ? GetWindowBounds(w, kWindowContentRgn, &got) : -1;
        printf("%s_bounds_match=%d\n", tag,
               gb == 0 && got.top == want->top && got.left == want->left &&
                          got.bottom == want->bottom && got.right == want->right);
    }
    if (st == 0 && w) DisposeWindow(w);
    return st == 0 && w != 0;
}

int main(void)
{
    Rect r = { 50, 60, 530, 700 };

    probe("fullscreen", kPlainWindowClass,    kWindowNoConstrain, &r, 1);
    probe("windowed",   kDocumentWindowClass, HALO_WINDOWED_ATTRS, &r, 1);
    /* decoration the class does not offer -> shim's bare-compositing retry */
    probe("degrade",    kPlainWindowClass,    0x9u,                &r, 0);

    exit(0);
}
