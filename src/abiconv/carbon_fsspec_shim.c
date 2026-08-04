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
// Kill switch: ABICONV_FSSPEC_LEGACY=1 restores the historical stub behaviour exactly
// (fnfErr/nsvErr, output buffers untouched) so the fix can be A/B'd and cannot pass inertly.
//
// Wired through the MTSHIM trampoline (maptable_tramp.asm): rdi -> &i386 args[0], return in
// eax. i386 arg slots are 4 bytes; a translated pointer is a 32-bit low-4GB address.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/stat.h>

// The FSSpec <-> POSIX mapping below is this module's product, not its private
// business: carbon_alias_shim.c builds and resolves AliasHandles out of exactly
// these triples. Declaring the shared half in a header keeps ONE implementation
// of the volfs / dirID-table / HFS-name-swap rules instead of a drifting copy.
#include "carbon_fsspec.h"

// Classic File Manager OSErr codes (Files.h / MacErrors.h), stable across all of Mac history.
#define FM_NO_ERR      0
#define FM_PARAM_ERR (-50)   // paramErr — bad parameter
#define FM_NSV_ERR   (-35)   // nsvErr   — no such volume
#define FM_FNF_ERR   (-43)   // fnfErr   — file not found

#define FSREF_SIZE   80      // struct FSRef { UInt8 hidden[80]; } — identical on 32/64-bit
// FSSpec is packed: SInt16 vRefNum @0, SInt32 parID @2, Str63 name @6 (len byte + 63).
#define FSSPEC_SIZE  70
#define FSSPEC_NAME  6

#define FS_RT_DIR_ID  2      // fsRtDirID — the CNID of a volume's root directory

// ── kill switch ─────────────────────────────────────────────────────────────────────────
static int fsspec_legacy(void) {
    static int c = -1;
    if (c < 0) { const char *e = getenv("ABICONV_FSSPEC_LEGACY"); c = (e && *e && *e != '0'); }
    return c;
}
static int fsspec_trace(void) {
    static int c = -1;
    if (c < 0) { c = getenv("ABICONV_FSSPEC_TRACE") != NULL; }
    return c;
}

// ── native FSRef-generation entry points ────────────────────────────────────────────────
// Resolved lazily by name: they are deprecated-but-present CarbonCore exports, and dlsym
// keeps libabiconv loadable if a future OS finally drops one (we then degrade to the old
// graceful-error behaviour instead of failing to link).
static int32_t (*p_FSGetVolumeInfo)(int16_t vol, uint32_t volIndex, int16_t *actualVol,
                                    uint32_t whichInfo, void *info, void *volName,
                                    void *rootDirectory);
static int32_t (*p_FSRefMakePath)(const void *ref, uint8_t *path, uint32_t maxPathSize);
static int32_t (*p_FSPathMakeRef)(const uint8_t *path, void *ref, uint8_t *isDirectory);
static int32_t (*p_FSFindFolder)(int16_t vol, uint32_t folderType, uint8_t createFolder,
                                 void *foundRef);
static int32_t (*p_FSGetCatalogInfo)(const void *ref, uint32_t whichInfo, void *catInfo,
                                     void *outName, void *fsSpec, void *parentRef);

static int fs_native_ready(void) {
    static int done;
    if (!done) {
        p_FSGetVolumeInfo = (int32_t (*)(int16_t, uint32_t, int16_t *, uint32_t,
                                         void *, void *, void *))dlsym(RTLD_DEFAULT, "FSGetVolumeInfo");
        p_FSRefMakePath   = (int32_t (*)(const void *, uint8_t *, uint32_t))
                            dlsym(RTLD_DEFAULT, "FSRefMakePath");
        p_FSPathMakeRef   = (int32_t (*)(const uint8_t *, void *, uint8_t *))
                            dlsym(RTLD_DEFAULT, "FSPathMakeRef");
        p_FSFindFolder    = (int32_t (*)(int16_t, uint32_t, uint8_t, void *))
                            dlsym(RTLD_DEFAULT, "FSFindFolder");
        p_FSGetCatalogInfo = (int32_t (*)(const void *, uint32_t, void *, void *,
                                          void *, void *))dlsym(RTLD_DEFAULT, "FSGetCatalogInfo");
        done = 1;
    }
    return p_FSGetVolumeInfo && p_FSRefMakePath && p_FSPathMakeRef;
}

// ── directory-ID table ──────────────────────────────────────────────────────────────────
// A classic directory ID is the file system's catalog node id, which on HFS+/APFS is the
// inode number — so /.vol/<st_dev>/<dirID> resolves it exactly, and that is the primary
// path (it works for a dirID a translated app read out of a settings file written years
// ago). Two cases need a remembered mapping instead:
//   * an APFS inode can exceed the SInt32 a classic dirID is declared as, and
//   * ★modern CarbonCore's FindFolder does NOT return real node ids — it hands back small
//     sequential tokens (6, 7, 8, ...) from an internal table that no surviving API can
//     consume, so a FindFolder dirID is meaningless to everything downstream.
// shim_FindFolder therefore mints the dirID itself and records it here.
#define DIRTAB_CAP 64
static struct { int32_t id; int16_t vref; char path[1024]; } g_dirtab[DIRTAB_CAP];
static int g_dirtab_n;
static int32_t g_dirtab_next = 0x40000000;   // above any plausible real CNID we mint for

static const char *dirtab_lookup(int16_t vref, int32_t id) {
    for (int i = 0; i < g_dirtab_n; i++) {
        if (g_dirtab[i].id == id && g_dirtab[i].vref == vref) { return g_dirtab[i].path; }
    }
    return NULL;
}
void cfs_dirtab_remember(int16_t vref, int32_t id, const char *path) {
    for (int i = 0; i < g_dirtab_n; i++) {
        if (g_dirtab[i].id == id && g_dirtab[i].vref == vref) { return; }
    }
    if (g_dirtab_n >= DIRTAB_CAP) { return; }
    g_dirtab[g_dirtab_n].id = id;
    g_dirtab[g_dirtab_n].vref = vref;
    snprintf(g_dirtab[g_dirtab_n].path, sizeof g_dirtab[0].path, "%s", path);
    g_dirtab_n++;
}

// ── FSSpec -> POSIX path ────────────────────────────────────────────────────────────────

// The POSIX/HFS name swap. A classic HFS name uses ':' as the path separator, so a '/' is a
// legal *character* inside a name — and the BSD layer of macOS presents exactly that name
// with '/' and ':' exchanged. Applying the same exchange to the leaf turns a classic name
// into the POSIX name of the same file.
void cfs_hfs_leaf_to_posix(const uint8_t *pname, char *out, size_t outsz) {
    size_t n = pname[0];
    if (n > 63) { n = 63; }
    if (n > outsz - 1) { n = outsz - 1; }
    for (size_t i = 0; i < n; i++) {
        char c = (char)pname[1 + i];
        out[i] = (c == '/') ? ':' : (c == ':') ? '/' : c;
    }
    out[n] = '\0';
}

// Directory (vRefNum, dirID) -> POSIX path. Exact, not heuristic:
//   * FSGetVolumeInfo maps the classic volume refNum to the volume's root FSRef, and
//   * a classic directory ID is the file system's catalog node ID = the inode number, which
//     macOS addresses directly as /.vol/<st_dev>/<CNID> (the volfs namespace).
// Returns 1 on success with a NUL-terminated path in `out`.
int cfs_fsdir_to_path(int16_t vRefNum, int32_t dirID, char *out, size_t outsz) {
    uint8_t rootRef[FSREF_SIZE];
    uint8_t volpath[1024];
    struct stat st;
    const char *known = dirtab_lookup(vRefNum, dirID);

    if (known) { snprintf(out, outsz, "%s", known); return stat(out, &st) == 0; }
    if (!fs_native_ready()) { return 0; }
    memset(rootRef, 0, sizeof rootRef);
    if (p_FSGetVolumeInfo(vRefNum, 0, NULL, 0 /*kFSVolInfoNone*/, NULL, NULL, rootRef) != 0) {
        return 0;
    }
    volpath[0] = '\0';
    if (p_FSRefMakePath(rootRef, volpath, (uint32_t)sizeof volpath) != 0) { return 0; }

    // fsRtDirID (2) — and any nonsensical id below it — means the volume root itself.
    if (dirID <= FS_RT_DIR_ID) {
        snprintf(out, outsz, "%s", (const char *)volpath);
        return stat(out, &st) == 0;
    }
    if (stat((const char *)volpath, &st) != 0) { return 0; }
    snprintf(out, outsz, "/.vol/%d/%d", (int)st.st_dev, (int)dirID);
    return stat(out, &st) == 0;
}

// Full FSSpec -> POSIX path (parent directory + optional leaf). Returns 1 on success.
// `exists` (optional) reports whether the resulting path is actually present: FSMakeFSSpec
// must return a fully-formed spec for a not-yet-existing file, so "resolvable" and "exists"
// are distinct answers.
int cfs_fsspec_to_path(const uint8_t *spec, char *out, size_t outsz, int *exists) {
    char parent[1024];
    char leaf[128];
    struct stat st;
    int16_t vRefNum;
    int32_t parID;

    memcpy(&vRefNum, spec, sizeof vRefNum);
    memcpy(&parID, spec + 2, sizeof parID);
    if (!cfs_fsdir_to_path(vRefNum, parID, parent, sizeof parent)) { return 0; }

    if (spec[FSSPEC_NAME] == 0) {
        snprintf(out, outsz, "%s", parent);
    } else {
        size_t pl = strlen(parent);
        cfs_hfs_leaf_to_posix(spec + FSSPEC_NAME, leaf, sizeof leaf);
        snprintf(out, outsz, "%s%s%s", parent,
                 (pl && parent[pl - 1] == '/') ? "" : "/", leaf);
    }
    if (exists) { *exists = (stat(out, &st) == 0); }
    return 1;
}

// ── POSIX path -> FSSpec (the inverse) ──────────────────────────────────────────────────
//
// WHY IT LIVES HERE. Everything above answers "where is this spec?"; the Alias Manager also
// needs the other direction, because ResolveAlias hands the caller back an FSSpec for a path
// it has just resolved. That is the same mapping read backwards, and it has to agree with
// cfs_fsspec_to_path EXACTLY or the round trip silently breaks — so it belongs in the module
// that owns the mapping, not in the one that happens to need it first.

// The same '/' <-> ':' exchange, POSIX leaf -> Pascal HFS name. The exchange is its own
// inverse, so this differs from cfs_hfs_leaf_to_posix only in the string representation.
void cfs_posix_leaf_to_hfs(const char *leaf, uint8_t *pout) {
    size_t n = leaf ? strlen(leaf) : 0;
    if (n > 63) { n = 63; }
    pout[0] = (uint8_t)n;
    for (size_t i = 0; i < n; i++) {
        char c = leaf[i];
        pout[1 + i] = (uint8_t)((c == '/') ? ':' : (c == ':') ? '/' : c);
    }
}

// POSIX path -> FSSpec. Returns 1 on success; `spec` (70 bytes) is fully written first,
// which is the out-param rule that came out of the Halo nil-CFStringRef crash.
//
// The parent directory is ALWAYS recorded in the dirID table on the way out. The primary
// mapping (parID = the parent's catalog node id, resolved back through /.vol/<dev>/<CNID>)
// is exact, but the volfs namespace is not readable on every volume and an APFS inode can
// overflow the SInt32 a classic dirID is declared as; remembering the path makes the round
// trip work anyway, exactly as shim_FindFolder already does for the folder ids it mints.
int cfs_path_to_fsspec(const char *path, uint8_t *spec) {
    char parent[1024];
    const char *slash;
    struct stat pst;
    int16_t vRefNum = 0;
    int32_t parID = FS_RT_DIR_ID;
    size_t plen;

    if (!spec) { return 0; }
    memset(spec, 0, FSSPEC_SIZE);                 // ★defined before anything can fail
    if (!path || !*path) { return 0; }

    // Split into parent directory + leaf. A bare "/" is the root of the boot volume: no leaf.
    slash = strrchr(path, '/');
    if (!slash || slash == path) {
        snprintf(parent, sizeof parent, "/");
    } else {
        plen = (size_t)(slash - path);
        if (plen >= sizeof parent) { plen = sizeof parent - 1; }
        memcpy(parent, path, plen);
        parent[plen] = '\0';
    }
    if (stat(parent, &pst) != 0) { return 0; }

    // The volume refNum the classic caller expects, taken from the same authority
    // shim_FindFolder uses: FSGetCatalogInfo's `volume` field at offset 2 of FSCatalogInfo.
    if (fs_native_ready() && p_FSGetCatalogInfo) {
        uint8_t ref[FSREF_SIZE], ci[512];
        memset(ref, 0, sizeof ref);
        memset(ci, 0, sizeof ci);
        if (p_FSPathMakeRef((const uint8_t *)parent, ref, NULL) == 0 &&
            p_FSGetCatalogInfo(ref, 0x00000004u /*kFSCatInfoVolume*/, ci, NULL, NULL, NULL) == 0) {
            memcpy(&vRefNum, ci + 2, sizeof vRefNum);
        }
    }
    // A directory ID is the catalog node id where it fits the classic SInt32, else a token.
    parID = ((uint64_t)pst.st_ino <= 0x7FFFFFFFULL) ? (int32_t)pst.st_ino : g_dirtab_next++;
    cfs_dirtab_remember(vRefNum, parID, parent);

    memcpy(spec, &vRefNum, sizeof vRefNum);
    memcpy(spec + 2, &parID, sizeof parID);
    if (slash && slash[1]) { cfs_posix_leaf_to_hfs(slash + 1, spec + FSSPEC_NAME); }
    return 1;
}

// ── the shims ───────────────────────────────────────────────────────────────────────────

// OSErr FSpMakeFSRef(const FSSpec *source, FSRef *newRef);
int shim_FSpMakeFSRef(uint32_t *args) {
    const uint8_t *spec = (const uint8_t *)(uintptr_t)args[0];
    uint8_t *newRef = (uint8_t *)(uintptr_t)args[1];
    char path[2048];
    int exists = 0;

    // ★Invariant first, unconditionally: a caller that ignores the OSErr must never read
    // its own stack garbage back as an FSRef.
    if (newRef && !fsspec_legacy()) { memset(newRef, 0, FSREF_SIZE); }
    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!spec || !newRef) { return FM_PARAM_ERR; }

    if (!cfs_fsspec_to_path(spec, path, sizeof path, &exists)) { return FM_NSV_ERR; }
    if (!exists) { return FM_FNF_ERR; }
    if (p_FSPathMakeRef((const uint8_t *)path, newRef, NULL) != 0) {
        memset(newRef, 0, FSREF_SIZE);
        return FM_FNF_ERR;
    }
    if (fsspec_trace()) { fprintf(stderr, "[fsspec] FSpMakeFSRef -> %s\n", path); fflush(stderr); }
    return FM_NO_ERR;
}

// OSErr FSMakeFSSpec(SInt16 vRefNum, SInt32 dirID, ConstStr255Param fileName, FSSpec *spec);
// Documented classic behaviour: the spec is filled in EVEN when the target does not exist
// (that is how callers build a spec for a file they are about to create) — fnfErr with a
// valid spec. So the spec is always written; only the OSErr distinguishes the cases.
int shim_FSMakeFSSpec(uint32_t *args) {
    int16_t vRefNum = (int16_t)(uint16_t)args[0];
    int32_t dirID = (int32_t)args[1];
    const uint8_t *fileName = (const uint8_t *)(uintptr_t)args[2];
    uint8_t *spec = (uint8_t *)(uintptr_t)args[3];
    char path[2048];
    int exists = 0;

    if (fsspec_legacy()) { return FM_NSV_ERR; }
    if (!spec) { return FM_PARAM_ERR; }

    // ★Same invariant: always hand back a fully-defined FSSpec.
    memset(spec, 0, FSSPEC_SIZE);
    memcpy(spec, &vRefNum, sizeof vRefNum);
    memcpy(spec + 2, &dirID, sizeof dirID);
    if (fileName && fileName[0]) {
        uint8_t n = fileName[0] > 63 ? 63 : fileName[0];
        spec[FSSPEC_NAME] = n;
        memcpy(spec + FSSPEC_NAME + 1, fileName + 1, n);
    }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, &exists)) { return FM_NSV_ERR; }
    return exists ? FM_NO_ERR : FM_FNF_ERR;
}

// OSErr FindFolder(SInt16 vRefNum, OSType folderType, Boolean createFolder,
//                  SInt16 *foundVRefNum, SInt32 *foundDirID);
// ★The PRODUCER half of the (vRefNum, dirID) contract. It is hand-shimmed because modern
// CarbonCore's FindFolder returns a dirID that is NOT a catalog node id but a small
// sequential token (6, 7, 8, ...) out of an internal table — and every surviving API that
// once consumed a dirID (FSpMakeFSRef, FSMakeFSSpec, PBMakeFSRefSync, ...) has been
// deleted, so that token is a dead end for a translated caller. We delegate the actual
// lookup to the native FSRef-based FSFindFolder (which is exact and honours every domain /
// folder type), then mint a dirID that our own consumer half can resolve back to the same
// directory: the real catalog node id where it fits in the classic SInt32, else a table
// token. Either way the value is recorded, so FSpMakeFSRef/FSMakeFSSpec round-trip it.
int shim_FindFolder(uint32_t *args) {
    int16_t vRefNum = (int16_t)(uint16_t)args[0];
    uint32_t folderType = args[1];
    uint8_t createFolder = (uint8_t)args[2];
    int16_t *foundVRefNum = (int16_t *)(uintptr_t)args[3];
    int32_t *foundDirID = (int32_t *)(uintptr_t)args[4];
    uint8_t ref[FSREF_SIZE];
    uint8_t path[1024];
    struct stat st;
    int32_t e, dirID;
    int16_t outv;

    // ★Invariant first: both out-params defined before any early return.
    if (foundVRefNum && !fsspec_legacy()) { *foundVRefNum = 0; }
    if (foundDirID && !fsspec_legacy()) { *foundDirID = 0; }

    if (!fs_native_ready() || !p_FSFindFolder || fsspec_legacy()) {
        // No native FSRef generation (or the kill switch): fall back to whatever the
        // system's own FindFolder says, so we never regress below the old behaviour.
        int32_t (*nat)(int16_t, uint32_t, uint8_t, int16_t *, int32_t *) =
            (int32_t (*)(int16_t, uint32_t, uint8_t, int16_t *, int32_t *))
            dlsym(RTLD_DEFAULT, "FindFolder");
        return nat ? nat(vRefNum, folderType, createFolder, foundVRefNum, foundDirID)
                   : FM_NSV_ERR;
    }
    if (!foundVRefNum || !foundDirID) { return FM_PARAM_ERR; }

    memset(ref, 0, sizeof ref);
    e = p_FSFindFolder(vRefNum, folderType, createFolder, ref);
    if (e != 0) { return e; }
    path[0] = '\0';
    if (p_FSRefMakePath(ref, path, (uint32_t)sizeof path) != 0 || path[0] == '\0') {
        return FM_FNF_ERR;
    }
    if (stat((const char *)path, &st) != 0) { return FM_FNF_ERR; }

    // Volume refNum of the folder we found. FSGetCatalogInfo's `volume` field sits at
    // offset 2 of FSCatalogInfo (UInt16 nodeFlags; FSVolumeRefNum volume; ...).
    outv = vRefNum;
    if (p_FSGetCatalogInfo) {
        uint8_t ci[512];
        memset(ci, 0, sizeof ci);
        if (p_FSGetCatalogInfo(ref, 0x00000004u /*kFSCatInfoVolume*/, ci,
                               NULL, NULL, NULL) == 0) {
            memcpy(&outv, ci + 2, sizeof outv);
        }
    }
    // The real catalog node id when it fits a classic SInt32, else a minted token.
    dirID = ((uint64_t)st.st_ino <= 0x7FFFFFFFULL) ? (int32_t)st.st_ino : g_dirtab_next++;
    cfs_dirtab_remember(outv, dirID, (const char *)path);
    *foundVRefNum = outv;
    *foundDirID = dirID;
    if (fsspec_trace()) {
        char ft[5] = { (char)(folderType >> 24), (char)(folderType >> 16),
                       (char)(folderType >> 8), (char)folderType, 0 };
        fprintf(stderr, "[fsspec] FindFolder('%s') -> vRefNum=%d dirID=%d '%s'\n",
                ft, outv, dirID, (const char *)path);
        fflush(stderr);
    }
    return FM_NO_ERR;
}

// ResFileRefNum FSpOpenResFile(const FSSpec *spec, SignedByte permission);
// No output buffer; resource forks addressed by FSSpec are gone and the flattened
// data-fork path is reached through FSOpenResFile (rsrc_datafork_shim.c). -1 is the
// canonical "could not open" refNum.
int shim_FSpOpenResFile(uint32_t *args) { (void)args; return -1; }

// OSStatus FSRefMakePath(const FSRef *ref, UInt8 *path, UInt32 maxPathSize);
// The native call is real and is used as-is. It is hand-shimmed ONLY to enforce the
// output-buffer invariant: Apple documents `path` as undefined when the call fails, and a
// translated caller that skips the OSStatus check will then read its own stack garbage as a
// C string (and hand it to CFStringCreateWithCString, which returns NULL for non-UTF8
// bytes). An empty string is a safe, well-defined answer for every such caller.
int shim_FSRefMakePath(uint32_t *args) {
    const void *ref = (const void *)(uintptr_t)args[0];
    uint8_t *path = (uint8_t *)(uintptr_t)args[1];
    uint32_t maxPathSize = args[2];
    int32_t st;

    if (!fs_native_ready()) {
        if (path && maxPathSize && !fsspec_legacy()) { path[0] = '\0'; }
        return FM_PARAM_ERR;
    }
    st = ref ? p_FSRefMakePath(ref, path, maxPathSize) : FM_PARAM_ERR;
    if (st != 0 && path && maxPathSize && !fsspec_legacy()) { path[0] = '\0'; }
    return st;
}

// ════════════════════════════════════════════════════════════════════════════════════════
// THE CLASSIC FILE MANAGER *WRITE* PATH
//
// Everything above answers "where is this file?". This half answers "make it so" —
// create, delete, rename, move, exchange, set Finder info. It is the same job (the dead
// FSSpec/HFS File Manager, resolved for real against the live file system) and shares the
// same resolver and the same ABICONV_FSSPEC_LEGACY kill switch, so it lives here rather
// than in a dylib of its own.
//
// WHY IT WAS MISSING. find_null_jump_bridges.py (2026-08-04) classified these as NULL-JUMP
// ORPHANS: the legacy shim pass reads the 10.6 SDK's *i386* headers and emits a
// pass-through bridge `___FSpCreate` that calls a native `_FSpCreate`. That native has NEVER
// existed on x86_64 — checking the 10.6 SDK's own stub library, FSpCreate/NewAlias/
// ResolveAlias/UpperString/... are present in the i386 slice and absent from the x86_64
// slice, i.e. Apple had already dropped them from 64-bit Carbon in 2009. So the bridge could
// only ever `call 0`. Nothing else in the process supplies them, which is exactly the case
// the shadowing fix cannot help with: they have to be implemented.
//
// This is the family a classic app uses to SAVE, so the whole write path of every Carbon
// target depended on it.
//
// SCOPE. Implemented here are the calls that COMPLETE inside the shim. Deliberately NOT
// here: the ones that hand back a File Manager refNum for some other API to consume
// (FSpOpenRF, HOpenResFile, PBGetFCBInfoSync, GetFPos) — a refNum we mint means nothing to
// native FSRead/FSClose, so those need the fork/refNum layer and are tracked separately.

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/attr.h>

#define FM_DUPFN_ERR (-48)   // dupFNErr  — a file with that name already exists
#define FM_IO_ERR     (-36)  // ioErr     — generic I/O failure
#define FM_BDNAM_ERR  (-37)  // bdNamErr  — bad file name
#define FM_FBSY_ERR   (-47)  // fBsyErr   — file/directory busy

// errno -> the OSErr a classic caller knows how to branch on.
static int fm_err_from_errno(void) {
    switch (errno) {
    case 0:        return FM_NO_ERR;
    case EEXIST:   return FM_DUPFN_ERR;
    case ENOENT:   return FM_FNF_ERR;
    case ENOTEMPTY:
    case EBUSY:    return FM_FBSY_ERR;
    case EINVAL:
    case ENAMETOOLONG: return FM_BDNAM_ERR;
    default:       return FM_IO_ERR;
    }
}

// (vRefNum, dirID, HFS leaf name) -> POSIX path. The HFS-style half of the API family
// addresses files this way instead of by FSSpec; the resolution is identical, so this just
// assembles the same two pieces cfs_fsspec_to_path uses.
static int hfs_to_path(int16_t vRefNum, int32_t dirID, const uint8_t *pname,
                       char *out, size_t outsz) {
    char parent[1024];
    char leaf[128];
    size_t pl;

    if (!cfs_fsdir_to_path(vRefNum, dirID, parent, sizeof parent)) { return 0; }
    if (!pname || pname[0] == 0) { snprintf(out, outsz, "%s", parent); return 1; }
    cfs_hfs_leaf_to_posix(pname, leaf, sizeof leaf);
    pl = strlen(parent);
    snprintf(out, outsz, "%s%s%s", parent, (pl && parent[pl - 1] == '/') ? "" : "/", leaf);
    return 1;
}

// Replace the last path component, keeping the directory. Used by the rename calls, which
// are documented to rename IN PLACE (a new name, never a new parent).
static void path_with_new_leaf(const char *path, const uint8_t *pname,
                               char *out, size_t outsz) {
    char leaf[128];
    const char *slash = strrchr(path, '/');
    size_t dirlen = slash ? (size_t)(slash - path) : 0;

    cfs_hfs_leaf_to_posix(pname, leaf, sizeof leaf);
    snprintf(out, outsz, "%.*s/%s", (int)dirlen, path, leaf);
}

static void fm_trace(const char *what, const char *a, const char *b, int err) {
    if (!fsspec_trace()) { return; }
    fprintf(stderr, "[fsspec] %s '%s'%s%s -> %d\n", what, a ? a : "", b ? "' -> '" : "",
            b ? b : "", err);
    fflush(stderr);
}

// A directory's classic dirID is its catalog node id = the inode number. Callers pass a
// `long *` that must be DEFINED even when the call fails (the out-param rule that cost us
// the Halo nil-CFStringRef crash), so it is written before anything can go wrong.
static void set_dir_id(uint32_t slot, const char *path) {
    int32_t *out = (int32_t *)(uintptr_t)slot;
    struct stat st;
    if (!out) { return; }
    *out = 0;
    if (path && stat(path, &st) == 0 && (uint64_t)st.st_ino <= 0x7FFFFFFFULL) {
        *out = (int32_t)st.st_ino;
    }
}

// ── create ──────────────────────────────────────────────────────────────────────────────

// OSErr FSpCreate(const FSSpec *spec, OSType creator, OSType fileType, ScriptCode script);
// Classic contract: creates an EMPTY data fork and fails with dupFNErr if the name is
// taken — never truncates an existing file. O_EXCL is exactly that.
int shim_FSpCreate(uint32_t *args) {
    const uint8_t *spec = (const uint8_t *)(uintptr_t)args[0];
    char path[2048];
    int fd;

    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!spec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }

    fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0666);
    if (fd < 0) { int e = fm_err_from_errno(); fm_trace("FSpCreate", path, NULL, e); return e; }
    close(fd);
    // creator/fileType are Finder metadata; set through the same path FSpSetFInfo uses so
    // there is one implementation of the Finder-info write.
    {
        struct attrlist al;
        uint8_t fi[32];
        memset(&al, 0, sizeof al);
        memset(fi, 0, sizeof fi);
        al.bitmapcount = ATTR_BIT_MAP_COUNT;
        al.commonattr = ATTR_CMN_FNDRINFO;
        // FInfo: OSType fdType @0, OSType fdCreator @4 — big-endian on disk, as classic
        // code already stores them.
        fi[0] = (uint8_t)(args[2] >> 24); fi[1] = (uint8_t)(args[2] >> 16);
        fi[2] = (uint8_t)(args[2] >> 8);  fi[3] = (uint8_t)args[2];
        fi[4] = (uint8_t)(args[1] >> 24); fi[5] = (uint8_t)(args[1] >> 16);
        fi[6] = (uint8_t)(args[1] >> 8);  fi[7] = (uint8_t)args[1];
        if (args[1] || args[2]) { setattrlist(path, &al, fi, sizeof fi, 0); }
    }
    fm_trace("FSpCreate", path, NULL, FM_NO_ERR);
    return FM_NO_ERR;
}

// OSErr FSpDirCreate(const FSSpec *spec, ScriptCode script, long *createdDirID);
int shim_FSpDirCreate(uint32_t *args) {
    const uint8_t *spec = (const uint8_t *)(uintptr_t)args[0];
    char path[2048];
    int e;

    set_dir_id(args[2], NULL);                    // defined before any failure path
    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!spec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    if (mkdir(path, 0777) != 0) {
        e = fm_err_from_errno(); fm_trace("FSpDirCreate", path, NULL, e); return e;
    }
    set_dir_id(args[2], path);
    fm_trace("FSpDirCreate", path, NULL, FM_NO_ERR);
    return FM_NO_ERR;
}

// OSErr DirCreate(short vRefNum, long parentDirID, ConstStr255Param name, long *createdDirID);
int shim_DirCreate(uint32_t *args) {
    char path[2048];
    int e;

    set_dir_id(args[3], NULL);
    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!hfs_to_path((int16_t)(uint16_t)args[0], (int32_t)args[1],
                     (const uint8_t *)(uintptr_t)args[2], path, sizeof path)) {
        return FM_NSV_ERR;
    }
    if (mkdir(path, 0777) != 0) {
        e = fm_err_from_errno(); fm_trace("DirCreate", path, NULL, e); return e;
    }
    set_dir_id(args[3], path);
    fm_trace("DirCreate", path, NULL, FM_NO_ERR);
    return FM_NO_ERR;
}

// ── delete ──────────────────────────────────────────────────────────────────────────────

// Classic Delete/HDelete removes a file OR an empty directory; POSIX splits the two.
static int fm_delete_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) { return FM_FNF_ERR; }
    if (S_ISDIR(st.st_mode)) { return rmdir(path) == 0 ? FM_NO_ERR : fm_err_from_errno(); }
    return unlink(path) == 0 ? FM_NO_ERR : fm_err_from_errno();
}

// OSErr HDelete(short vRefNum, long dirID, ConstStr255Param fileName);
int shim_HDelete(uint32_t *args) {
    char path[2048];
    int e;
    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!hfs_to_path((int16_t)(uint16_t)args[0], (int32_t)args[1],
                     (const uint8_t *)(uintptr_t)args[2], path, sizeof path)) {
        return FM_NSV_ERR;
    }
    e = fm_delete_path(path);
    fm_trace("HDelete", path, NULL, e);
    return e;
}

// OSErr FSpDelete(const FSSpec *spec);
int shim_FSpDelete(uint32_t *args) {
    const uint8_t *spec = (const uint8_t *)(uintptr_t)args[0];
    char path[2048];
    int e;
    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!spec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    e = fm_delete_path(path);
    fm_trace("FSpDelete", path, NULL, e);
    return e;
}

// ── rename / move ───────────────────────────────────────────────────────────────────────

// OSErr FSpRename(const FSSpec *spec, ConstStr255Param newName);
// Renames within the SAME directory — the classic call cannot move a file (that is CatMove).
int shim_FSpRename(uint32_t *args) {
    const uint8_t *spec = (const uint8_t *)(uintptr_t)args[0];
    const uint8_t *newName = (const uint8_t *)(uintptr_t)args[1];
    char path[2048], dest[2048];
    int e;

    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!spec || !newName || newName[0] == 0) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    path_with_new_leaf(path, newName, dest, sizeof dest);
    e = rename(path, dest) == 0 ? FM_NO_ERR : fm_err_from_errno();
    fm_trace("FSpRename", path, dest, e);
    return e;
}

// OSErr HRename(short vRefNum, long dirID, ConstStr255Param oldName, ConstStr255Param newName);
int shim_HRename(uint32_t *args) {
    const uint8_t *newName = (const uint8_t *)(uintptr_t)args[3];
    char path[2048], dest[2048];
    int e;

    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!newName || newName[0] == 0) { return FM_PARAM_ERR; }
    if (!hfs_to_path((int16_t)(uint16_t)args[0], (int32_t)args[1],
                     (const uint8_t *)(uintptr_t)args[2], path, sizeof path)) {
        return FM_NSV_ERR;
    }
    path_with_new_leaf(path, newName, dest, sizeof dest);
    e = rename(path, dest) == 0 ? FM_NO_ERR : fm_err_from_errno();
    fm_trace("HRename", path, dest, e);
    return e;
}

// OSErr CatMove(short vRefNum, long dirID, ConstStr255Param oldName,
//               long newDirID, ConstStr255Param newName);
// Moves to a new PARENT. `newName` names the destination DIRECTORY when non-empty
// (classic quirk: it is a directory name inside newDirID, not the file's new leaf), so the
// destination directory is (newDirID / newName) and the leaf is unchanged.
int shim_CatMove(uint32_t *args) {
    int16_t vRefNum = (int16_t)(uint16_t)args[0];
    const uint8_t *oldName = (const uint8_t *)(uintptr_t)args[2];
    char src[2048], dstdir[2048], dest[2048];
    char leaf[128];
    size_t dl;
    int e;

    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!hfs_to_path(vRefNum, (int32_t)args[1], oldName, src, sizeof src)) { return FM_NSV_ERR; }
    if (!hfs_to_path(vRefNum, (int32_t)args[3], (const uint8_t *)(uintptr_t)args[4],
                     dstdir, sizeof dstdir)) {
        return FM_NSV_ERR;
    }
    cfs_hfs_leaf_to_posix(oldName, leaf, sizeof leaf);
    dl = strlen(dstdir);
    snprintf(dest, sizeof dest, "%s%s%s", dstdir, (dl && dstdir[dl - 1] == '/') ? "" : "/", leaf);
    e = rename(src, dest) == 0 ? FM_NO_ERR : fm_err_from_errno();
    fm_trace("CatMove", src, dest, e);
    return e;
}

// OSErr FSpCatMove(const FSSpec *source, const FSSpec *dest);
// `dest` names the destination DIRECTORY; the moved item keeps its own leaf name.
int shim_FSpCatMove(uint32_t *args) {
    const uint8_t *sspec = (const uint8_t *)(uintptr_t)args[0];
    const uint8_t *dspec = (const uint8_t *)(uintptr_t)args[1];
    char src[2048], dstdir[2048], dest[2048];
    const char *slash;
    size_t dl;
    int e;

    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!sspec || !dspec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(sspec, src, sizeof src, NULL)) { return FM_NSV_ERR; }
    if (!cfs_fsspec_to_path(dspec, dstdir, sizeof dstdir, NULL)) { return FM_NSV_ERR; }
    slash = strrchr(src, '/');
    dl = strlen(dstdir);
    snprintf(dest, sizeof dest, "%s%s%s", dstdir, (dl && dstdir[dl - 1] == '/') ? "" : "/",
             slash ? slash + 1 : src);
    e = rename(src, dest) == 0 ? FM_NO_ERR : fm_err_from_errno();
    fm_trace("FSpCatMove", src, dest, e);
    return e;
}

// OSErr FSpExchangeFiles(const FSSpec *source, const FSSpec *dest);
// The classic safe-save primitive: swap two files' CONTENTS while both keep their own
// identity, so an open refNum / alias to `dest` sees the new data. renameatx_np(RENAME_SWAP)
// is the modern equivalent; exchangedata() is the direct descendant but is deprecated and
// unimplemented on APFS. Both are looked up by name so libabiconv stays loadable if either
// disappears, and the fallback is an honest error rather than a silent half-swap.
int shim_FSpExchangeFiles(uint32_t *args) {
    static int (*p_renameatx)(int, const char *, int, const char *, unsigned int);
    static int (*p_exchangedata)(const char *, const char *, unsigned int);
    static int probed;
    const uint8_t *sspec = (const uint8_t *)(uintptr_t)args[0];
    const uint8_t *dspec = (const uint8_t *)(uintptr_t)args[1];
    char a[2048], b[2048];
    int e;

    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!sspec || !dspec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(sspec, a, sizeof a, NULL)) { return FM_NSV_ERR; }
    if (!cfs_fsspec_to_path(dspec, b, sizeof b, NULL)) { return FM_NSV_ERR; }
    if (!probed) {
        p_renameatx = (int (*)(int, const char *, int, const char *, unsigned int))
                      dlsym(RTLD_DEFAULT, "renameatx_np");
        p_exchangedata = (int (*)(const char *, const char *, unsigned int))
                         dlsym(RTLD_DEFAULT, "exchangedata");
        probed = 1;
    }
    if (p_renameatx) {
        e = p_renameatx(AT_FDCWD, a, AT_FDCWD, b, 0x0002 /* RENAME_SWAP */) == 0
            ? FM_NO_ERR : fm_err_from_errno();
    } else if (p_exchangedata) {
        e = p_exchangedata(a, b, 0) == 0 ? FM_NO_ERR : fm_err_from_errno();
    } else {
        e = FM_IO_ERR;
    }
    fm_trace("FSpExchangeFiles", a, b, e);
    return e;
}

// ── Finder info ─────────────────────────────────────────────────────────────────────────

// OSErr FSpSetFInfo(const FSSpec *spec, const FInfo *fndrInfo);
// FInfo is the first 16 bytes of the 32-byte Finder-info blob setattrlist takes; the rest
// (FXInfo) is preserved by reading it back first, so setting a file type does not silently
// wipe the extended info.
int shim_FSpSetFInfo(uint32_t *args) {
    const uint8_t *spec = (const uint8_t *)(uintptr_t)args[0];
    const uint8_t *finfo = (const uint8_t *)(uintptr_t)args[1];
    char path[2048];
    struct attrlist al;
    struct { uint32_t len; uint8_t fi[32]; } buf;
    uint8_t out[32];

    if (fsspec_legacy()) { return FM_FNF_ERR; }
    if (!spec || !finfo) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }

    memset(&al, 0, sizeof al);
    al.bitmapcount = ATTR_BIT_MAP_COUNT;
    al.commonattr = ATTR_CMN_FNDRINFO;
    memset(out, 0, sizeof out);
    if (getattrlist(path, &al, &buf, sizeof buf, 0) == 0) { memcpy(out, buf.fi, sizeof out); }
    memcpy(out, finfo, 16);                       // FInfo replaces only its own half
    if (setattrlist(path, &al, out, sizeof out, 0) != 0) {
        int e = fm_err_from_errno(); fm_trace("FSpSetFInfo", path, NULL, e); return e;
    }
    fm_trace("FSpSetFInfo", path, NULL, FM_NO_ERR);
    return FM_NO_ERR;
}

// OSErr FlushVol(ConstStr63Param volName, short vRefNum);
// Classic "commit this volume's cached blocks". POSIX has no per-volume flush that an
// unprivileged process may call, and every write above is already unbuffered at the syscall
// layer, so the honest answer is success: the data a caller is asking us to commit is
// already committed. sync() is deliberately NOT used — it is a whole-system stall.
int shim_FlushVol(uint32_t *args) { (void)args; return fsspec_legacy() ? FM_NSV_ERR : FM_NO_ERR; }
