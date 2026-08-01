// What does Halo's GPU capability check actually see on this machine?
// Halo (i386, 2006) does: CGDisplayIDToOpenGLDisplayMask -> CGLQueryRendererInfo
// -> CGLDescribeRenderer(..., 0x49 = kCGLRPAccelerated, 0x78 = kCGLRPVideoMemory).
// kCGLRPVideoMemory is DEPRECATED (32-bit byte count); on unified-memory Apple
// Silicon it may well report 0, which would make a 2006 "do I have a usable GPU?"
// test fail and take the never-tested error path.
#include <stdio.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>
#include <ApplicationServices/ApplicationServices.h>

static void show(CGLRendererInfoObj info, GLint i, const char *name, CGLRendererProperty p) {
    GLint v = -12345;
    CGLError e = CGLDescribeRenderer(info, i, p, &v);
    printf("    %-28s (0x%02x) -> ", name, (unsigned)p);
    if (e != kCGLNoError) printf("CGLError %d (%s)\n", (int)e, CGLErrorString(e));
    else printf("%d\n", (int)v);
}

int main(void) {
    CGDirectDisplayID disp = CGMainDisplayID();
    GLuint mask = CGDisplayIDToOpenGLDisplayMask(disp);
    printf("main display id = %u, OpenGL display mask = 0x%x\n", (unsigned)disp, (unsigned)mask);

    CGLRendererInfoObj info = NULL;
    GLint nrend = 0;
    CGLError err = CGLQueryRendererInfo(mask, &info, &nrend);
    printf("CGLQueryRendererInfo -> err=%d (%s), nrend=%d, info=%p\n",
           (int)err, CGLErrorString(err), (int)nrend, (void *)info);
    if (err != kCGLNoError || !info) return 1;

    for (GLint i = 0; i < nrend; i++) {
        printf("  renderer %d:\n", (int)i);
        show(info, i, "kCGLRPRendererID",           kCGLRPRendererID);
        show(info, i, "kCGLRPAccelerated",          kCGLRPAccelerated);   // 0x49 - Halo checks
        show(info, i, "kCGLRPVideoMemory(DEPREC)",  (CGLRendererProperty)120); // 0x78 - Halo checks
        show(info, i, "kCGLRPVideoMemoryMegabytes", kCGLRPVideoMemoryMegabytes);
        show(info, i, "kCGLRPTextureMemory(DEPR)",  (CGLRendererProperty)121);
        show(info, i, "kCGLRPTextureMemoryMegabyte",kCGLRPTextureMemoryMegabytes);
        show(info, i, "kCGLRPOnline",               kCGLRPOnline);
        show(info, i, "kCGLRPCompliant",            kCGLRPCompliant);
        show(info, i, "kCGLRPDisplayMask",          kCGLRPDisplayMask);
    }
    CGLDestroyRendererInfo(info);
    return 0;
}
