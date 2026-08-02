/* 99_agl_renderer_info — guard for the classic GPU-ENUMERATION contract
 * (src/abiconv/agl_renderer_shim.c + the DM* half in src/abiconv/carbon_ui_shim.c,
 *  bridged through src/abiconv/qd_gworld.c's GDevice registry).
 *
 * Every 32-bit-era Mac 3D layer answers "what GPUs are there, and is any of
 * them accelerated?" with the same QuickDraw-typed sequence:
 *
 *      GDHandle gd = DMGetFirstScreenDevice(true);
 *      DMGetDisplayIDByGDevice(gd, &displayID, true);
 *      AGLRendererInfo ri = aglQueryRendererInfo(&gd, 1);
 *      for (r = ri; r; r = aglNextRendererInfo(r))
 *          aglDescribeRenderer(r, AGL_ACCELERATED / AGL_RENDERER_ID /
 *                                 AGL_VIDEO_MEMORY / AGL_TEXTURE_MEMORY, &v);
 *
 * On 64-bit macOS both halves are dead on arrival, in DIFFERENT ways, and both
 * used to fail silently in a way the caller reads as "this machine has no GPU":
 *
 *   aglQueryRendererInfo takes an AGLDevice, which IS GDHandle (AGL/agl.h:39,
 *   deprecated in 10.5).  A GDHandle cannot exist on 64-bit, and the surviving
 *   entry point returns NULL for EVERY input without even setting an AGL error
 *   (measured, src/86x64/probes/aglrenderer.c).  So the renderer count is zero.
 *
 *   DMGetDisplayIDByGDevice was shimmed to paramErr because the Display Manager
 *   is gone — but the information is one we already hold, since qd_gworld.c
 *   mints the screen GDevice FROM a CGDirectDisplayID.
 *
 * Halo CE's IDirect3D_Mac::IDirect3D_Mac runs exactly this sequence and raises
 * its own "An unrecoverable error has occurred" alert on either failure (i386
 * 0x2b477f 'renderer count changed' / 0x2b47a5 'unable to read DisplayIDType').
 * The fix routes the query to the surviving CGDirectDisplayID-typed
 * aglQueryRendererInfoForCGDirectDisplayIDs and answers the DM pair from the
 * registry.
 *
 * What is asserted here is not a status code but REAL HARDWARE FACTS: an
 * accelerated renderer exists and reports a renderer id and a video-memory
 * figure.  Those can only come from a live driver query.
 *
 * The properties this pins:
 *   gdevice    DMGetFirstScreenDevice returns a screen device at all.
 *   displayid  DMGetDisplayIDByGDevice reports noErr AND a non-zero
 *              CGDirectDisplayID (a lying noErr with a zero id would not do).
 *   info       aglQueryRendererInfo(&gd, 1) returns a renderer list.
 *   accel      at least one renderer reports AGL_ACCELERATED != 0.
 *   props      that renderer answers AGL_RENDERER_ID, AGL_VIDEO_MEMORY and
 *              AGL_TEXTURE_MEMORY (all three GLbooleans true, id non-zero).
 *   outdef     RULE A: aglDescribeRenderer on a renderer it cannot describe
 *              still leaves the caller's GLint DEFINED.  Classic callers store
 *              the value and check the GLboolean late, or never.  This is an
 *              invariant of the shim itself, so it holds in BOTH A/B arms.
 *
 * A/B: re-run with M64_NO_AGL_RENDERERINFO=1 M64_NO_DM_DISPLAYID=1 and both
 * halves are disarmed — the query forwards raw to the dead native entry point
 * and the DM pair returns paramErr, i.e. exactly the pre-fix behaviour.  See
 * agl_renderer_info_test.sh; without that half a passing fixture would not
 * prove these shims are what fixed it.
 *
 * Neither Carbon nor AGL is in the i386 sysroot, so the whole surface is an
 * undefined dynamic_lookup import resolved at translate time by static-
 * interpose to libabiconv's ___<sym> shims.
 *
 * NOTE: no crt0 — main must end in exit(), never return.
 */

extern int printf(const char *, ...);
extern void exit(int status);

typedef void *GDHandle, *AGLRendererInfo;
typedef unsigned int  DisplayIDType;
typedef unsigned char GLboolean;
typedef int           GLint;

extern GDHandle DMGetFirstScreenDevice(unsigned char activeOnly);
extern short    DMGetDisplayIDByGDevice(GDHandle dev, DisplayIDType *outID,
                                        unsigned char failToMain);

extern AGLRendererInfo aglQueryRendererInfo(const GDHandle *gdevs, GLint ndev);
extern AGLRendererInfo aglNextRendererInfo(AGLRendererInfo r);
extern GLboolean       aglDescribeRenderer(AGLRendererInfo r, GLint prop, GLint *value);
extern void            aglDestroyRendererInfo(AGLRendererInfo r);

#define AGL_RENDERER_ID    70
#define AGL_ACCELERATED    73
#define AGL_VIDEO_MEMORY   120
#define AGL_TEXTURE_MEMORY 121

int main(void)
{
    /* --- RULE A first: it must hold even when everything else fails. ------ */
    GLint sentinel = 0x5a5a5a5a;
    aglDescribeRenderer((AGLRendererInfo)0, AGL_ACCELERATED, &sentinel);
    printf("outdef=%d\n", sentinel == 0);

    GDHandle gd = DMGetFirstScreenDevice(1);
    printf("gdevice=%d\n", gd != 0);

    DisplayIDType did = 0;
    short err = DMGetDisplayIDByGDevice(gd, &did, 1);
    printf("displayid=%d\n", (err == 0 && did != 0));

    /* --- the classic idiom, verbatim --- */
    AGLRendererInfo ri = aglQueryRendererInfo(&gd, 1);
    printf("info=%d\n", ri != 0);
    if (!ri) { printf("accel=0\nprops=0\n"); exit(0); }

    int accel = 0, props = 0;
    for (AGLRendererInfo r = ri; r; r = aglNextRendererInfo(r)) {
        GLint acc = 0;
        if (!aglDescribeRenderer(r, AGL_ACCELERATED, &acc) || !acc) continue;
        accel = 1;
        GLint id = 0, vram = -1, tex = -1;
        GLboolean a = aglDescribeRenderer(r, AGL_RENDERER_ID,    &id);
        GLboolean b = aglDescribeRenderer(r, AGL_VIDEO_MEMORY,   &vram);
        GLboolean c = aglDescribeRenderer(r, AGL_TEXTURE_MEMORY, &tex);
        if (a && b && c && id != 0 && vram >= 0 && tex >= 0) props = 1;
        break;
    }
    printf("accel=%d\n", accel);
    printf("props=%d\n", props);

    aglDestroyRendererInfo(ri);
    exit(0);
}
