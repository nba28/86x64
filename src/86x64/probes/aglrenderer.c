/* aglrenderer.c — measure what survives of the classic AGL renderer-enumeration API.
 *
 * Halo's IDirect3D_Mac::IDirect3D_Mac (and every other 32-bit-era D3D-over-AGL
 * shim layer) enumerates GPUs with the QuickDraw-typed pair
 *     aglQueryRendererInfo(const AGLDevice *gdevs, GLint ndev)     // AGLDevice == GDHandle
 *     aglDescribeRenderer(rend, AGL_ACCELERATED/RENDERER_ID/VIDEO_MEMORY/TEXTURE_MEMORY)
 * plus DMGetDisplayIDByGDevice(). GDHandle does not exist on 64-bit.
 *
 * This probe answers, on the live machine:
 *   [a] does aglQueryRendererInfo() survive at all, and what does it do with a
 *       non-NULL non-GDHandle pointer (what our shim substrate hands it)?
 *   [b] does aglQueryRendererInfoForCGDirectDisplayIDs() work?
 *   [c] do all four aglDescribeRenderer properties Halo needs answer on it?
 *
 * Build: clang -arch x86_64 -framework ApplicationServices -o aglrenderer aglrenderer.c
 * Run each mode in its own process; [a] may fault by design.
 *   ./aglrenderer legacy   ./aglrenderer modern
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ApplicationServices/ApplicationServices.h>
#include <dlfcn.h>
/* AGL.framework has NO SDK stub (`ld -framework AGL` fails) even though the code is
 * alive in the dyld shared cache — the same fact agl_drawable_shim.c documents.  So
 * declare the ABI locally and dlopen/dlsym it. */
typedef void *AGLRendererInfo;
typedef void *AGLDevice;
typedef int   GLint;
typedef unsigned char GLboolean;
#define AGL_RENDERER_ID    70
#define AGL_ACCELERATED    73
#define AGL_DISPLAY_MASK   84
#define AGL_VIDEO_MEMORY   120
#define AGL_TEXTURE_MEMORY 121
static AGLRendererInfo (*aglQueryRendererInfo)(const AGLDevice *, GLint);
static AGLRendererInfo (*aglQueryRendererInfoForCGDirectDisplayIDs)(const CGDirectDisplayID *, GLint);
static GLboolean (*aglDescribeRenderer)(AGLRendererInfo, GLint, GLint *);
static AGLRendererInfo (*aglNextRendererInfo)(AGLRendererInfo);
static void (*aglDestroyRendererInfo)(AGLRendererInfo);
static GLint (*aglGetError)(void);
static void agl_bind(void) {
    void *h = dlopen("/System/Library/Frameworks/AGL.framework/AGL", RTLD_LAZY | RTLD_GLOBAL);
    if (!h) { fprintf(stderr, "dlopen AGL failed: %s\n", dlerror()); exit(2); }
    #define B(n) do { *(void **)&n = dlsym(h, #n); \
        printf("[bind] %-42s %s\n", #n, n ? "PRESENT" : "*** ABSENT ***"); } while (0)
    B(aglQueryRendererInfo);
    B(aglQueryRendererInfoForCGDirectDisplayIDs);
    B(aglDescribeRenderer);
    B(aglNextRendererInfo);
    B(aglDestroyRendererInfo);
    B(aglGetError);
    #undef B
}

static void describe(AGLRendererInfo ri, const char *tag)
{
    int n = 0;
    for (AGLRendererInfo r = ri; r; r = aglNextRendererInfo(r), n++) {
        GLint acc = -1, id = -1, vram = -1, tex = -1, mask = -1;
        GLboolean b_acc  = aglDescribeRenderer(r, AGL_ACCELERATED,    &acc);
        GLboolean b_id   = aglDescribeRenderer(r, AGL_RENDERER_ID,    &id);
        GLboolean b_vram = aglDescribeRenderer(r, AGL_VIDEO_MEMORY,   &vram);
        GLboolean b_tex  = aglDescribeRenderer(r, AGL_TEXTURE_MEMORY, &tex);
        GLboolean b_mask = aglDescribeRenderer(r, AGL_DISPLAY_MASK,   &mask);
        printf("[%s] renderer %d: ACCELERATED ok=%d v=%d | RENDERER_ID ok=%d v=0x%x | "
               "VIDEO_MEMORY ok=%d v=%d | TEXTURE_MEMORY ok=%d v=%d | DISPLAY_MASK ok=%d v=0x%x\n",
               tag, n, b_acc, acc, b_id, id, b_vram, vram, b_tex, tex, b_mask, mask);
    }
    printf("[%s] renderer count = %d\n", tag, n);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "modern";
    agl_bind();

    if (!strcmp(mode, "legacy")) {
        /* What Halo does today: a synthetic 32-bit "GDHandle" from our qd registry.
         * Use a plausible low-4GB arena address, exactly the shape make_main_gdevice()
         * returns. Also try a genuinely NULL device list for contrast. */
        void *fake = (void *)(uintptr_t)0x8a9fd4d0u;
        printf("[legacy] calling aglQueryRendererInfo(&fake=0x%llx, 1)\n",
               (unsigned long long)(uintptr_t)fake);
        fflush(stdout);
        AGLRendererInfo ri = aglQueryRendererInfo((const AGLDevice *)&fake, 1);
        printf("[legacy] aglQueryRendererInfo(fakeGDHandle) -> %p  aglGetError=%d\n",
               (void *)ri, aglGetError());
        fflush(stdout);
        if (ri) { describe(ri, "legacy"); aglDestroyRendererInfo(ri); }

        void *nul = NULL;
        AGLRendererInfo r2 = aglQueryRendererInfo((const AGLDevice *)&nul, 1);
        printf("[legacy] aglQueryRendererInfo(NULL gdev) -> %p  aglGetError=%d\n",
               (void *)r2, aglGetError());
        if (r2) { describe(r2, "legacy-null"); aglDestroyRendererInfo(r2); }
        return 0;
    }

    /* The surviving replacement, CGDirectDisplayID-typed. */
    CGDirectDisplayID ids[16];
    uint32_t ndisp = 0;
    CGGetActiveDisplayList(16, ids, &ndisp);
    printf("[modern] CGGetActiveDisplayList -> %u display(s); CGMainDisplayID=0x%x\n",
           ndisp, (unsigned)CGMainDisplayID());
    for (uint32_t i = 0; i < ndisp; i++)
        printf("[modern]   display[%u] = 0x%x  openGLDisplayMask=0x%x\n",
               i, (unsigned)ids[i],
               (unsigned)CGDisplayIDToOpenGLDisplayMask(ids[i]));

    for (uint32_t i = 0; i < ndisp; i++) {
        AGLRendererInfo ri = aglQueryRendererInfoForCGDirectDisplayIDs(&ids[i], 1);
        printf("[modern] aglQueryRendererInfoForCGDirectDisplayIDs(0x%x,1) -> %p err=%d\n",
               (unsigned)ids[i], (void *)ri, aglGetError());
        if (ri) { describe(ri, "modern"); aglDestroyRendererInfo(ri); }
    }
    return 0;
}
