// What does Halo's GPU capability check actually see on this machine?
// Halo (i386, 2006) does: CGDisplayIDToOpenGLDisplayMask -> CGLQueryRendererInfo
// -> CGLDescribeRenderer(..., 0x49 = kCGLRPAccelerated, 0x78 = kCGLRPVideoMemory).
// kCGLRPVideoMemory is DEPRECATED (32-bit byte count); on unified-memory Apple
// Silicon it may well report 0, which would make a 2006 "do I have a usable GPU?"
// test fail and take the never-tested error path.
#include <stdio.h>
#include <stdlib.h>
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

/* The mask is now an ARGUMENT, because which mask you pass is the whole question
 * for Portal 2. Its shaderapidx9 calls CGLQueryRendererInfo(0x1, ...) -- a
 * hardcoded-looking mask, not the one CGDisplayIDToOpenGLDisplayMask returns on
 * this machine -- and that call never returns: the process dies inside Metal on
 * com.Metal.DeviceDispatch. Run `cglprobe` for the real mask and `cglprobe 0x1`
 * for Portal 2's, in SEPARATE processes, since the second may fault by design.
 * If 0x1 faults natively too then this is a platform fact about a stale mask and
 * the cure is a shim, exactly as with the dead AGL enumeration API; if it
 * succeeds natively, the bug is ours. */
int main(int argc, char **argv) {
    CGDirectDisplayID disp = CGMainDisplayID();
    GLuint mask = CGDisplayIDToOpenGLDisplayMask(disp);
    printf("main display id = %u, OpenGL display mask = 0x%x\n", (unsigned)disp, (unsigned)mask);
    if (argc > 1) {
        mask = (GLuint)strtoul(argv[1], NULL, 0);
        printf("using CALLER-SUPPLIED mask 0x%x\n", (unsigned)mask);
    }
    fflush(stdout);

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
