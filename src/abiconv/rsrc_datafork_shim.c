/*
 * rsrc_datafork_shim.c — FSOpenResFile that also finds a FLATTENED (data-fork)
 * classic resource file.  ONE JOB: make the classic resource-fork open succeed
 * after the pipeline has had to move the fork out of the HFS resource fork.
 *
 * WHY THIS IS NEEDED (structural, not app-specific)
 * ------------------------------------------------
 * Classic Mac apps keep real content in the HFS RESOURCE FORK: `*.rsrc` files
 * whose DATA fork is 0 bytes and whose entire payload lives in the
 * `com.apple.ResourceFork` xattr (`<file>/..namedfork/rsrc`).  They read it with
 * FSOpenResFile + UseResFile + Get1Resource.
 *
 * Modern `codesign` HARD-REFUSES any bundle containing a resource fork:
 *     $ codesign -f -s - T.app
 *       T.app: resource fork, Finder information, or similar detritus not allowed
 *       exit status 1
 * and on Apple Silicon an unsigned bundle cannot run at all.  So a translated
 * bundle CANNOT both keep its forks and be launchable — the pipeline must move
 * the fork out of the fork.  `m64 forks --flatten` (src/86x64/m64) therefore
 * migrates every resource fork to a codesign-safe DATA-fork resource file:
 *   - data fork empty (the classic `*.rsrc` case)  -> fork bytes become the file
 *   - data fork in use (a file with both forks)    -> fork bytes go to the hidden
 *                                                     sidecar <dir>/.86x64rsrc/<name>
 *
 * The MODERN Resource Manager still reads a data-fork resource file — that is
 * exactly how `.dfont` fonts work — through FSOpenResourceFile() with a
 * zero-length fork name.  Measured on the real 815758-byte Halo EULA.rsrc:
 *     FSOpenResFile(rsrc fork)        -> refnum=-1  ResError=-39
 *     FSOpenResourceFile(0, NULL)     -> err=0      refnum=8
 *       Get1Resource('TEXT',1024)     -> 20875 bytes    (the EULA license text)
 *
 * Without this fallback the classic call chain dies silently: FSOpenResFile
 * returns -1, Get1Resource returns NULL, and the app takes its "resource
 * missing" path.  For Halo that is the first-launch EULA gate —
 * CreateScrollingTextBoxControl('TEXT' 1024) returns -192 (resNotFound), so Halo
 * DisposeWindow()s the fully-built EULA dialog and exit()s before ever entering
 * its modal loop.  The same silent failure hits EVERY classic-era target with a
 * resource-fork resource file, so the trigger here is purely structural: the
 * resource-fork open failed, so look for the same resources where the
 * signable-bundle migration had to put them.
 *
 * Order of attempts (each is the real, native Resource Manager):
 *   1. the HFS resource fork          (untouched bundles / dev trees still work)
 *   2. the file's own DATA fork       (flattened *.rsrc)
 *   3. <dir>/.86x64rsrc/<name>        (sidecar, for files that use both forks)
 * ResError() is left set by whichever native call ran last, so a caller that
 * polls it sees noErr on success and the genuine error on total failure.
 *
 * MTSHIM convention (maptable_tramp.asm): rdi -> &i386 args[0] (4-byte cdecl
 * slots), return value in eax.  Static-interpose binds the translated image's
 * `_FSOpenResFile` import to `___FSOpenResFile` -> here.
 */

#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RSRC_SIDECAR_DIR ".86x64rsrc"

typedef int32_t RD_OSStatus;

#define RDDL(fn, ret, args) \
    static ret (*fn) args; if (!fn) fn = (ret (*) args)dlsym(RTLD_DEFAULT, #fn)

/* Build <dir>/.86x64rsrc/<name> from a POSIX path. Returns 0 on success. */
static int rd_sidecar_path(const char *path, char *out, size_t outsz) {
    const char *slash = strrchr(path, '/');
    if (!slash || slash == path) { return -1; }
    size_t dirlen = (size_t)(slash - path);
    const char *name = slash + 1;
    if (!*name) { return -1; }
    /* already inside the sidecar dir? never recurse */
    if (dirlen >= sizeof(RSRC_SIDECAR_DIR) &&
        strncmp(path + dirlen - (sizeof(RSRC_SIDECAR_DIR) - 1),
                RSRC_SIDECAR_DIR, sizeof(RSRC_SIDECAR_DIR) - 1) == 0) { return -1; }
    int n = snprintf(out, outsz, "%.*s/%s/%s", (int)dirlen, path,
                     RSRC_SIDECAR_DIR, name);
    return (n > 0 && (size_t)n < outsz) ? 0 : -1;
}

/*
 * ResFileRefNum FSOpenResFile(const FSRef *ref, SInt8 permission);
 * i386 slots: a[0] = FSRef *, a[1] = SInt8 permission.
 *
 * FSRef is an opaque 80-byte token with the SAME layout in i386 and x86_64, and
 * the translated image's copy was filled in by a native call (FSPathMakeRef /
 * CFURLGetFSRef) through the bridge, so the pointer is passed straight through.
 */
uint32_t shim_FSOpenResFile(uint32_t *a) {
    /* ResFileRefNum is SInt16 in 32-bit Carbon and int in LP64; the values are
     * small file-table indices, so the i386 caller's 16-bit read is safe. */
    RDDL(FSOpenResFile,       int32_t, (const void *, int8_t));
    RDDL(FSOpenResourceFile,  int16_t, (const void *, uint32_t, const uint16_t *,
                                        int8_t, int32_t *));
    RDDL(FSRefMakePath,       RD_OSStatus, (const void *, uint8_t *, uint32_t));
    RDDL(FSPathMakeRef,       RD_OSStatus, (const uint8_t *, void *, uint8_t *));

    const void *ref = (const void *)(uintptr_t)a[0];
    int8_t perm = (int8_t)a[1];
    if (!ref || !FSOpenResFile) { return (uint32_t)-1; }

    /* 1. the real HFS resource fork */
    int32_t rn = FSOpenResFile(ref, perm);
    if (rn > 0) { return (uint32_t)rn; }
    if (!FSOpenResourceFile) { return (uint32_t)rn; }

    /* 2. the file's own DATA fork (flattened resource file, like a .dfont) */
    int32_t drn = -1;
    int16_t e = FSOpenResourceFile(ref, 0, NULL, perm, &drn);
    if (e == 0 && drn > 0) {
        return (uint32_t)drn;
    }

    /* 3. the sidecar, for files that need BOTH forks */
    if (FSRefMakePath && FSPathMakeRef) {
        uint8_t path[1024], side[1200];
        if (FSRefMakePath(ref, path, sizeof path) == 0 &&
            rd_sidecar_path((const char *)path, (char *)side, sizeof side) == 0) {
            uint8_t sideref[80];
            if (FSPathMakeRef(side, sideref, NULL) == 0) {
                drn = -1;
                e = FSOpenResourceFile(sideref, 0, NULL, perm, &drn);
                if (e == 0 && drn > 0) {
                    return (uint32_t)drn;
                }
            }
        }
    }
    return (uint32_t)rn;
}
