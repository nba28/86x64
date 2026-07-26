/* 99_cgl_renderer_check — guard for the bridged CGL renderer-query round-trip.
 *
 * Halo CE runs exactly this sequence (i386 0x2c4d0e) before it will create its
 * render window, and its failure path is a minefield (see
 * 99_cstr_pagezero_arg), so the query itself has to be right:
 *
 *     mask = CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID());
 *     CGLQueryRendererInfo(mask, &info, &nrend);
 *     CGLDescribeRenderer(info, 0, kCGLRPRendererCount, &count);
 *     CGLDescribeRenderer(info, i, kCGLRPAccelerated,   &accel);
 *     CGLDescribeRenderer(info, i, kCGLRPVideoMemory,   &vram);
 *
 * The interesting part for the bridge is `CGLRendererInfoObj`: an opaque
 * pointer the native library hands back ABOVE 4GB, which abigen wraps into a
 * 32-bit proxy handle on the way out and unwraps on every subsequent call. If
 * that round-trip loses identity, every CGLDescribeRenderer after the query
 * fails and the app concludes the machine has no GPU.
 *
 * ASSERTIONS ARE DELIBERATELY HARDWARE- AND DISPLAY-INDEPENDENT: what is
 * checked is that the queries succeed and that the handle round-trips (the
 * renderer COUNT reported through the wrapped handle equals the one the query
 * filled in directly), not what GPU the host happens to have. The accelerated
 * and video-memory properties are queried too — they must not error — but their
 * values are not asserted. An all-displays mask is used for the assertions so
 * the test still means something in a headless session; the real display path
 * is exercised separately for consistency only.
 */
#include <stdio.h>
#include <stdlib.h>

typedef unsigned int   CGDirectDisplayID;
typedef unsigned int   GLuint;
typedef int            GLint;
typedef void          *CGLRendererInfoObj;
typedef int            CGLError;

#define kCGLRPRendererCount 0x80
#define kCGLRPAccelerated   0x49
#define kCGLRPVideoMemory   0x78

extern CGDirectDisplayID CGMainDisplayID(void);
extern GLuint   CGDisplayIDToOpenGLDisplayMask(CGDirectDisplayID);
extern CGLError CGLQueryRendererInfo(GLuint mask, CGLRendererInfoObj *rend, GLint *nrend);
extern CGLError CGLDescribeRenderer(CGLRendererInfoObj rend, GLint idx, int prop, GLint *v);
extern CGLError CGLDestroyRendererInfo(CGLRendererInfoObj rend);

int main(void)
{
    /* Exercise the display path the app actually uses. Its VALUE depends on the
     * session (0 when headless), so it is not asserted -- only that the two
     * bridged calls agree, which a broken marshal would not guarantee. */
    CGDirectDisplayID d = CGMainDisplayID();
    GLuint m1 = CGDisplayIDToOpenGLDisplayMask(d);
    GLuint m2 = CGDisplayIDToOpenGLDisplayMask(d);
    printf("mask_stable=%d\n", m1 == m2);

    CGLRendererInfoObj info;            /* Halo leaves this uninitialised too */
    GLint nrend = 0;
    CGLError e = CGLQueryRendererInfo(0xFFFFFFFFu, &info, &nrend);
    printf("queryinfo_err=%d\n", e);
    printf("info_nonnull=%d\n", info != 0);
    printf("nrend_positive=%d\n", nrend > 0);
    if (e || !info) { printf("BAIL\n"); exit(1); }

    /* The round-trip: this count comes back through the WRAPPED handle, so it
     * can only match the query's own out-param if the wrap/unwrap preserved
     * object identity. */
    GLint count = -1;
    CGLError ec = CGLDescribeRenderer(info, 0, kCGLRPRendererCount, &count);
    printf("rcount_err=%d\n", ec);
    printf("handle_roundtrip=%d\n", ec == 0 && count == nrend);

    /* Every renderer must answer both properties the app asks for. The values
     * are hardware-dependent; only the absence of an error is asserted. */
    int prop_errs = 0;
    for (GLint i = 0; i < count; i++) {
        GLint accel = 0, vram = 0;
        prop_errs += (CGLDescribeRenderer(info, i, kCGLRPAccelerated, &accel) != 0);
        prop_errs += (CGLDescribeRenderer(info, i, kCGLRPVideoMemory,  &vram)  != 0);
    }
    printf("prop_errs=%d\n", prop_errs);

    printf("destroy_err=%d\n", CGLDestroyRendererInfo(info));
    exit(0);
}
