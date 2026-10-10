/* 99_depth24_mode -- guard: a renderer's depth-mode mask lists 24-bit.
 * (src/abiconv/agl_renderer_shim.c classic_depth_modes; Call of Duty 4 rejects a
 * renderer without kCGL24Bit/AGL_24_BIT_BIT -> "Display: no valid displays".)
 * Apple Silicon reports kCGLRPDepthModes = 0x1001 (0 + 32-bit); every 32-bit-era
 * Mac listed 0x800 too. Both entry points are checked:
 *   cgl  CGLQueryRendererInfo + CGLDescribeRenderer(kCGLRPDepthModes)
 *   agl  aglQueryRendererInfo(&gd,1) + aglDescribeRenderer(AGL_DEPTH_MODES)
 * Exit 42 iff some renderer reports 0x800 on both paths. Values go to stderr.
 * OFF: M64_NO_DEPTH24_MODE=1 at run time. CGL/AGL/Carbon are undefined
 * dynamic_lookup imports bound to libabiconv shims. No crt0: end with exit(). */
extern int fprintf(void *, const char *, ...);
extern void *__stderrp;
extern void exit(int);

typedef void *GDHandle, *AGLRendererInfo, *CGLRendererInfoObj;
typedef int GLint;
typedef unsigned char GLboolean;

extern unsigned CGDisplayIDToOpenGLDisplayMask(unsigned);
extern unsigned CGMainDisplayID(void);
extern int CGLQueryRendererInfo(unsigned mask, CGLRendererInfoObj *r, GLint *n);
extern int CGLDescribeRenderer(CGLRendererInfoObj r, GLint i, int prop, GLint *v);
extern int CGLDestroyRendererInfo(CGLRendererInfoObj r);
extern GDHandle DMGetFirstScreenDevice(unsigned char);
extern AGLRendererInfo aglQueryRendererInfo(const GDHandle *, GLint);
extern AGLRendererInfo aglNextRendererInfo(AGLRendererInfo);
extern GLboolean aglDescribeRenderer(AGLRendererInfo, GLint, GLint *);

#define DEPTH_MODES 105   /* kCGLRPDepthModes == AGL_DEPTH_MODES */

int main(void)
{
    int cgl = 0, agl = 0;
    CGLRendererInfoObj info = 0; GLint n = 0;
    if (CGLQueryRendererInfo(CGDisplayIDToOpenGLDisplayMask(CGMainDisplayID()), &info, &n) == 0 && info) {
        for (GLint i = 0; i < n; i++) {
            GLint m = -1;
            int e = CGLDescribeRenderer(info, i, DEPTH_MODES, &m);
            fprintf(__stderrp, "cgl renderer %d err=%d depthModes=0x%x\n", i, e, m);
            if (!e && (m & 0x800)) cgl = 1;
        }
        CGLDestroyRendererInfo(info);
    }
    GDHandle gd = DMGetFirstScreenDevice(1);
    AGLRendererInfo ri = aglQueryRendererInfo(&gd, 1);
    int k = 0;
    for (AGLRendererInfo r = ri; r; r = aglNextRendererInfo(r), k++) {
        GLint m = -1;
        GLboolean ok = aglDescribeRenderer(r, DEPTH_MODES, &m);
        fprintf(__stderrp, "agl renderer %d ok=%d depthModes=0x%x\n", k, ok, m);
        if (ok && (m & 0x800)) agl = 1;
    }
    fprintf(__stderrp, "cgl=%d agl=%d\n", cgl, agl);
    exit(cgl && agl ? 42 : 1);
}
