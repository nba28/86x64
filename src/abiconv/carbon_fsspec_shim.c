// carbon_fsspec_shim.c — the dead FSSpec-based Carbon File Manager, RESOLVED for real.
//
// The classic Mac OS File Manager had two generations of file references:
//   * FSSpec  {volume refNum, directory ID, HFS name} — the original HFS model, and
//   * FSRef   (opaque 80-byte token) — its Carbon-era replacement.
// Apple removed the FSSpec generation entirely on modern macOS (the symbols are gone and
// their declarations were stripped from <CoreServices/.../Files.h>), while the FSRef
// generation survives for binary compatibility. abigen therefore already generates proper
// ABI shims for every FSRef function our targets import (FSGetCatalogInfo, FSOpenFork,
// FSReadFork, FSPathMakeRef, FSMakeFSRefUnicode, ...) from the live header — but it cannot
// shim the FSSpec functions, because there is no declaration to read and no native symbol
// to call. Reached from translated i386-cdecl code through a bare indirect stub, a *missing*
// symbol's lazy pointer stays null, so the stub jumps through 0 -> rip=0 SIGSEGV.
//
// ── Why these are REAL implementations and not graceful-error stubs ──────────────────────
//
// They used to be one-liners that returned fnfErr/nsvErr and never touched their output
// parameters. That is wrong twice over:
//
//  1. FUNCTIONALLY. An FSSpec is {vRefNum, parID, name} and every one of those three still
//     has an exact modern meaning: `FSGetVolumeInfo` maps a vRefNum to the volume's root
//     FSRef, and a classic directory ID *is* the HFS+/APFS catalog node ID, i.e. the inode
//     number — which macOS exposes directly as `/.vol/<st_dev>/<CNID>`. So an FSSpec CAN be
//     resolved, exactly, with no heuristics. Halo's entire preferences story runs through
//     `FindFolder(...,'asup'/'sdat'/'docs',...) -> FSpMakeFSRef -> FSRefMakePath`; with the
//     stub it could never learn where its Application Support folder was, so it never read
//     or wrote its settings file (empty Server Port / Client Port / IP Address fields) and
//     eventually took its own "An unrecoverable error has occurred" path.
//
//  2. STRUCTURALLY. ★A shim that fails MUST still leave every caller-supplied output buffer
//     in a well-defined state. Classic Carbon code is full of call sites that ignore the
//     OSErr — on 2006 hardware the call could not fail, so the check was dead code and got
//     dropped. When our shim fails without writing `*newRef`, the caller reads its OWN
//     UNINITIALISED STACK as an FSRef. Halo (i386 0x2da50c) does exactly that:
//         FSpMakeFSRef(&spec, &fsref)    <- OSErr ignored, fsref = stack garbage
//         FSRefMakePath(&fsref, buf, 1024) <- fails; Apple documents `buf` as UNDEFINED
//                                             on error, so buf stays stack garbage too
//         CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8)  -> NULL for bytes
//                                             that are not valid UTF-8
//         CFStringGetCString(NULL, buf, 2048, kCFStringEncodingWindowsLatin1) -> SIGSEGV
//     That is the Halo Graphics-Settings-OK crash (EXC_BAD_ACCESS at 0x0, rdi=0x7 rsi=0x0
//     rdx=0x800 rcx=0x500), and because it depends on the byte content of the stack below
//     rsp it is a HEISENBUG: lldb, and even our own [callsite] trace, perturb that stack
//     enough to make the garbage decode as valid UTF-8 and the crash vanish.
//     Note that zeroing `*newRef` alone does NOT close this: a zeroed FSRef still makes
//     the NATIVE FSRefMakePath fail, and it is Apple's code that then leaves the path
//     buffer undefined. The invariant has to be enforced at FSRefMakePath as well — which
//     is why FSRefMakePath is hand-shimmed here even though the native one works.
//
// Both rules are app-agnostic and trigger on structure, not on an app name: any translated
// caller of these APIs gets a resolvable FSSpec, and no failing call ever hands back an
// undefined buffer.
//
// WHERE IT LIVES. The implementation is shimdb/impl/fsspec.c (FSSpec and FInfo are
// ABI-neutral records), linked hidden into libabiconv and compiled by shimgen for native
// callers. This file is the i386 glue: 4-byte argument slots, and the kill switch.
//
// Kill switch: ABICONV_FSSPEC_LEGACY=1 restores the historical stub behaviour exactly
// (fnfErr/nsvErr, output buffers untouched) so the fix can be A/B'd and cannot pass inertly.
//
// Wired through the MTSHIM trampoline (maptable_tramp.asm): rdi -> &i386 args[0], return in
// eax. i386 arg slots are 4 bytes; a translated pointer is a 32-bit low-4GB address.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include "fsspec.h"

#define FM_PARAM_ERR (-50)
#define FM_NSV_ERR   (-35)
#define FM_FNF_ERR   (-43)
#define P(n) ((void *)(uintptr_t)args[(n)])

// shimdb/impl/fsspec.c (native signatures)
int16_t FSpMakeFSRef(const uint8_t *spec, uint8_t *newRef);
int16_t FSMakeFSSpec(int16_t vRefNum, int32_t dirID, const uint8_t *fileName, uint8_t *spec);
int16_t FSpOpenResFile(const uint8_t *spec, int8_t permission);
int16_t FSpCreate(const uint8_t *spec, uint32_t creator, uint32_t fileType, int16_t script);
int16_t FSpDirCreate(const uint8_t *spec, int16_t script, int32_t *createdDirID);
int16_t DirCreate(int16_t vRefNum, int32_t parentDirID, const uint8_t *name, int32_t *createdDirID);
int16_t HDelete(int16_t vRefNum, int32_t dirID, const uint8_t *fileName);
int16_t FSpDelete(const uint8_t *spec);
int16_t FSpRename(const uint8_t *spec, const uint8_t *newName);
int16_t HRename(int16_t vRefNum, int32_t dirID, const uint8_t *oldName, const uint8_t *newName);
int16_t CatMove(int16_t vRefNum, int32_t dirID, const uint8_t *oldName,
                int32_t newDirID, const uint8_t *newName);
int16_t FSpCatMove(const uint8_t *sspec, const uint8_t *dspec);
int16_t FSpExchangeFiles(const uint8_t *sspec, const uint8_t *dspec);
int16_t FSpSetFInfo(const uint8_t *spec, const uint8_t *finfo);
int16_t FlushVol(const uint8_t *volName, int16_t vRefNum);

static int fsspec_legacy(void) {
    static int c = -1;
    if (c < 0) { const char *e = getenv("ABICONV_FSSPEC_LEGACY"); c = (e && *e && *e != '0'); }
    return c;
}

#define V16(n) ((int16_t)(uint16_t)args[(n)])
#define S32(n) ((int32_t)args[(n)])

int shim_FSpMakeFSRef(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : FSpMakeFSRef(P(0), P(1));
}
int shim_FSMakeFSSpec(uint32_t *args) {
    return fsspec_legacy() ? FM_NSV_ERR : FSMakeFSSpec(V16(0), S32(1), P(2), P(3));
}
int shim_FindFolder(uint32_t *args) {
    if (fsspec_legacy()) {
        int32_t (*nat)(int16_t, uint32_t, uint8_t, int16_t *, int32_t *) =
            (int32_t (*)(int16_t, uint32_t, uint8_t, int16_t *, int32_t *))
            dlsym(RTLD_DEFAULT, "FindFolder");
        return nat ? nat(V16(0), args[1], (uint8_t)args[2], P(3), P(4)) : FM_NSV_ERR;
    }
    return cfs_find_folder(V16(0), args[1], (uint8_t)args[2], P(3), P(4));
}
int shim_FSpOpenResFile(uint32_t *args) { return FSpOpenResFile(P(0), (int8_t)args[1]); }

// OSStatus FSRefMakePath(const FSRef *ref, UInt8 *path, UInt32 maxPathSize);
// The native call is live and used as-is; hand-shimmed ONLY for the output-buffer rule:
// Apple documents `path` as undefined on failure, and a caller that skips the OSStatus
// check then reads its own stack garbage as a C string. An empty string is a safe answer.
int shim_FSRefMakePath(uint32_t *args) {
    static int32_t (*nat)(const void *, uint8_t *, uint32_t);
    uint8_t *path = P(1);
    uint32_t maxPathSize = args[2];
    int32_t st;
    if (!nat) { nat = (int32_t (*)(const void *, uint8_t *, uint32_t))dlsym(RTLD_DEFAULT, "FSRefMakePath"); }
    st = (nat && args[0]) ? nat(P(0), path, maxPathSize) : FM_PARAM_ERR;
    if (st != 0 && path && maxPathSize && !fsspec_legacy()) { path[0] = '\0'; }
    return st;
}

int shim_FSpCreate(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : FSpCreate(P(0), args[1], args[2], V16(3));
}
// createdDirID is defined even on the legacy path (the pre-move shim zeroed it first).
int shim_FSpDirCreate(uint32_t *args) {
    int32_t *out = P(2);
    if (out) { *out = 0; }
    return fsspec_legacy() ? FM_FNF_ERR : FSpDirCreate(P(0), V16(1), out);
}
int shim_DirCreate(uint32_t *args) {
    int32_t *out = P(3);
    if (out) { *out = 0; }
    return fsspec_legacy() ? FM_FNF_ERR : DirCreate(V16(0), S32(1), P(2), out);
}
int shim_HDelete(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : HDelete(V16(0), S32(1), P(2));
}
int shim_FSpDelete(uint32_t *args) { return fsspec_legacy() ? FM_FNF_ERR : FSpDelete(P(0)); }
int shim_FSpRename(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : FSpRename(P(0), P(1));
}
int shim_HRename(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : HRename(V16(0), S32(1), P(2), P(3));
}
int shim_CatMove(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : CatMove(V16(0), S32(1), P(2), S32(3), P(4));
}
int shim_FSpCatMove(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : FSpCatMove(P(0), P(1));
}
int shim_FSpExchangeFiles(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : FSpExchangeFiles(P(0), P(1));
}
int shim_FSpSetFInfo(uint32_t *args) {
    return fsspec_legacy() ? FM_FNF_ERR : FSpSetFInfo(P(0), P(1));
}
int shim_FlushVol(uint32_t *args) {
    return fsspec_legacy() ? FM_NSV_ERR : FlushVol(P(0), V16(1));
}
