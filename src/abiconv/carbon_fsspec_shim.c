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
// Scope: calls that hand back a File Manager refNum for another API to consume (FSpOpenRF,
// HOpenResFile, GetFPos, ...) are deliberately not here: a refNum we mint means nothing to
// native FSRead/FSClose.
//
// Kill switch: ABICONV_FSSPEC_LEGACY=1 restores the historical stub behaviour exactly
// (fnfErr/nsvErr, output buffers untouched) so the fix can be A/B'd and cannot pass inertly.
//
// Wired through the MTSHIM trampoline (maptable_tramp.asm): rdi -> &i386 args[0], return in
// eax. i386 arg slots are 4 bytes; a translated pointer is a 32-bit low-4GB address. FSSpec
// {SInt16 vRefNum, SInt32 parID, Str63 name} and FInfo are packed fixed-width records, so the
// i386 layout is used in place.

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/attr.h>
#include "carbon_fsspec.h"

/* Classic File Manager OSErr codes (Files.h / MacErrors.h). */
#define FM_NO_ERR      0
#define FM_PARAM_ERR (-50)   /* paramErr */
#define FM_NSV_ERR   (-35)   /* nsvErr   — no such volume */
#define FM_FNF_ERR   (-43)   /* fnfErr   — file not found */
#define FM_DUPFN_ERR (-48)   /* dupFNErr — name already exists */
#define FM_IO_ERR    (-36)   /* ioErr */
#define FM_BDNAM_ERR (-37)   /* bdNamErr — bad file name */
#define FM_FBSY_ERR  (-47)   /* fBsyErr  — file/directory busy */

#define FSREF_SIZE   CFS_FSREF_SIZE
#define FSSPEC_SIZE  CFS_FSSPEC_SIZE
#define FSSPEC_NAME  CFS_FSSPEC_NAME
#define FS_RT_DIR_ID 2       /* fsRtDirID — the CNID of a volume's root directory */

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
static void cfs_dirtab_remember(int16_t vref, int32_t id, const char *path) {
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
static void cfs_hfs_leaf_to_posix(const uint8_t *pname, char *out, size_t outsz) {
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
static int cfs_fsdir_to_path(int16_t vRefNum, int32_t dirID, char *out, size_t outsz) {
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
static void cfs_posix_leaf_to_hfs(const char *leaf, uint8_t *pout) {
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

// ── FindFolder: the PRODUCER half of the (vRefNum, dirID) contract ──────────────────────
// Modern CarbonCore's FindFolder still exists, but returns a dirID that is a small sequential
// token out of an internal table rather than a catalog node id, and every API that consumed
// one is gone. So the lookup goes through the exact native FSFindFolder and the dirID is
// minted here: the real catalog node id where it fits the classic SInt32, else a table token,
// recorded either way so the consumers above resolve it back. Out-params are defined first.
static int32_t cfs_find_folder(int16_t vRefNum, uint32_t folderType, uint8_t createFolder,
                        int16_t *foundVRefNum, int32_t *foundDirID) {
    uint8_t ref[FSREF_SIZE];
    uint8_t path[1024];
    struct stat st;
    int32_t e, dirID;
    int16_t outv;

    if (foundVRefNum) { *foundVRefNum = 0; }
    if (foundDirID) { *foundDirID = 0; }
    if (!fs_native_ready() || !p_FSFindFolder) {
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
    // FSGetCatalogInfo's `volume` field sits at offset 2 of FSCatalogInfo.
    outv = vRefNum;
    if (p_FSGetCatalogInfo) {
        uint8_t ci[512];
        memset(ci, 0, sizeof ci);
        if (p_FSGetCatalogInfo(ref, 0x00000004u /*kFSCatInfoVolume*/, ci,
                               NULL, NULL, NULL) == 0) {
            memcpy(&outv, ci + 2, sizeof outv);
        }
    }
    dirID = ((uint64_t)st.st_ino <= 0x7FFFFFFFULL) ? (int32_t)st.st_ino : g_dirtab_next++;
    cfs_dirtab_remember(outv, dirID, (const char *)path);
    *foundVRefNum = outv;
    *foundDirID = dirID;
    return FM_NO_ERR;
}

// ── the removed calls ─────────────────────────────────────────────────────────────────

// OSErr FSpMakeFSRef(const FSSpec *source, FSRef *newRef);
static int16_t FSpMakeFSRef(const uint8_t *spec, uint8_t *newRef) {
    char path[2048];
    int exists = 0;
    if (newRef) { memset(newRef, 0, FSREF_SIZE); }   // defined even if the OSErr is ignored
    if (!spec || !newRef) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, &exists)) { return FM_NSV_ERR; }
    if (!exists) { return FM_FNF_ERR; }
    if (p_FSPathMakeRef((const uint8_t *)path, newRef, NULL) != 0) {
        memset(newRef, 0, FSREF_SIZE);
        return FM_FNF_ERR;
    }
    return FM_NO_ERR;
}

// OSErr FSMakeFSSpec(SInt16 vRefNum, SInt32 dirID, ConstStr255Param fileName, FSSpec *spec);
// The spec is filled in even when the target does not exist (that is how a caller builds a
// spec for a file it is about to create): fnfErr with a valid spec.
static int16_t FSMakeFSSpec(int16_t vRefNum, int32_t dirID, const uint8_t *fileName, uint8_t *spec) {
    char path[2048];
    int exists = 0;
    if (!spec) { return FM_PARAM_ERR; }
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

// ResFileRefNum FSpOpenResFile(const FSSpec *spec, SignedByte permission);
// FSSpec-addressed resource forks are gone; the flattened data-fork path is reached through
// FSOpenResFile. -1 is the canonical "could not open" refNum.
static int16_t FSpOpenResFile(const uint8_t *spec, int8_t permission) {
    (void)spec; (void)permission;
    return -1;
}

// ── the write path: create, delete, rename, move, exchange, Finder info ──────────────

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

// A directory's classic dirID is its catalog node id = the inode number. Callers pass a
// `long *` that must be DEFINED even when the call fails (the out-param rule that cost us
// the Halo nil-CFStringRef crash), so it is written before anything can go wrong.
static void set_dir_id(int32_t *out, const char *path) {
    struct stat st;
    if (!out) { return; }
    *out = 0;
    if (path && stat(path, &st) == 0 && (uint64_t)st.st_ino <= 0x7FFFFFFFULL) {
        *out = (int32_t)st.st_ino;
    }
}

// OSErr FSpCreate(const FSSpec *spec, OSType creator, OSType fileType, ScriptCode script);
// Creates an EMPTY data fork and fails with dupFNErr if the name is taken — never truncates.
static int16_t FSpCreate(const uint8_t *spec, uint32_t creator, uint32_t fileType, int16_t script) {
    char path[2048];
    int fd;
    (void)script;
    if (!spec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    fd = open(path, O_CREAT | O_EXCL | O_WRONLY, 0666);
    if (fd < 0) { return fm_err_from_errno(); }
    close(fd);
    if (creator || fileType) {
        // FInfo: OSType fdType @0, OSType fdCreator @4, big-endian as classic code stores them.
        struct attrlist al;
        uint8_t fi[32];
        memset(&al, 0, sizeof al);
        memset(fi, 0, sizeof fi);
        al.bitmapcount = ATTR_BIT_MAP_COUNT;
        al.commonattr = ATTR_CMN_FNDRINFO;
        fi[0] = (uint8_t)(fileType >> 24); fi[1] = (uint8_t)(fileType >> 16);
        fi[2] = (uint8_t)(fileType >> 8);  fi[3] = (uint8_t)fileType;
        fi[4] = (uint8_t)(creator >> 24);  fi[5] = (uint8_t)(creator >> 16);
        fi[6] = (uint8_t)(creator >> 8);   fi[7] = (uint8_t)creator;
        setattrlist(path, &al, fi, sizeof fi, 0);
    }
    return FM_NO_ERR;
}

// OSErr FSpDirCreate(const FSSpec *spec, ScriptCode script, SInt32 *createdDirID);
static int16_t FSpDirCreate(const uint8_t *spec, int16_t script, int32_t *createdDirID) {
    char path[2048];
    (void)script;
    set_dir_id(createdDirID, NULL);                  // defined before any failure path
    if (!spec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    if (mkdir(path, 0777) != 0) { return fm_err_from_errno(); }
    set_dir_id(createdDirID, path);
    return FM_NO_ERR;
}

// OSErr DirCreate(SInt16 vRefNum, SInt32 parentDirID, ConstStr255Param name, SInt32 *createdDirID);
static int16_t DirCreate(int16_t vRefNum, int32_t parentDirID, const uint8_t *name, int32_t *createdDirID) {
    char path[2048];
    set_dir_id(createdDirID, NULL);
    if (!hfs_to_path(vRefNum, parentDirID, name, path, sizeof path)) { return FM_NSV_ERR; }
    if (mkdir(path, 0777) != 0) { return fm_err_from_errno(); }
    set_dir_id(createdDirID, path);
    return FM_NO_ERR;
}

// Classic Delete/HDelete removes a file OR an empty directory; POSIX splits the two.
static int fm_delete_path(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) { return FM_FNF_ERR; }
    if (S_ISDIR(st.st_mode)) { return rmdir(path) == 0 ? FM_NO_ERR : fm_err_from_errno(); }
    return unlink(path) == 0 ? FM_NO_ERR : fm_err_from_errno();
}

// OSErr HDelete(SInt16 vRefNum, SInt32 dirID, ConstStr255Param fileName);
static int16_t HDelete(int16_t vRefNum, int32_t dirID, const uint8_t *fileName) {
    char path[2048];
    if (!hfs_to_path(vRefNum, dirID, fileName, path, sizeof path)) { return FM_NSV_ERR; }
    return (int16_t)fm_delete_path(path);
}

// OSErr FSpDelete(const FSSpec *spec);
static int16_t FSpDelete(const uint8_t *spec) {
    char path[2048];
    if (!spec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    return (int16_t)fm_delete_path(path);
}

// OSErr FSpRename(const FSSpec *spec, ConstStr255Param newName);
// Renames within the SAME directory (moving is CatMove).
static int16_t FSpRename(const uint8_t *spec, const uint8_t *newName) {
    char path[2048], dest[2048];
    if (!spec || !newName || newName[0] == 0) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    path_with_new_leaf(path, newName, dest, sizeof dest);
    return rename(path, dest) == 0 ? FM_NO_ERR : (int16_t)fm_err_from_errno();
}

// OSErr HRename(SInt16 vRefNum, SInt32 dirID, ConstStr255Param oldName, ConstStr255Param newName);
static int16_t HRename(int16_t vRefNum, int32_t dirID, const uint8_t *oldName, const uint8_t *newName) {
    char path[2048], dest[2048];
    if (!newName || newName[0] == 0) { return FM_PARAM_ERR; }
    if (!hfs_to_path(vRefNum, dirID, oldName, path, sizeof path)) { return FM_NSV_ERR; }
    path_with_new_leaf(path, newName, dest, sizeof dest);
    return rename(path, dest) == 0 ? FM_NO_ERR : (int16_t)fm_err_from_errno();
}

// OSErr CatMove(SInt16 vRefNum, SInt32 dirID, ConstStr255Param oldName,
//               SInt32 newDirID, ConstStr255Param newName);
// Moves to a new PARENT. A non-empty `newName` names the destination DIRECTORY inside
// newDirID (classic quirk), and the moved item keeps its leaf.
static int16_t CatMove(int16_t vRefNum, int32_t dirID, const uint8_t *oldName,
                int32_t newDirID, const uint8_t *newName) {
    char src[2048], dstdir[2048], dest[2048];
    char leaf[128];
    size_t dl;
    if (!hfs_to_path(vRefNum, dirID, oldName, src, sizeof src)) { return FM_NSV_ERR; }
    if (!hfs_to_path(vRefNum, newDirID, newName, dstdir, sizeof dstdir)) { return FM_NSV_ERR; }
    cfs_hfs_leaf_to_posix(oldName, leaf, sizeof leaf);
    dl = strlen(dstdir);
    snprintf(dest, sizeof dest, "%s%s%s", dstdir, (dl && dstdir[dl - 1] == '/') ? "" : "/", leaf);
    return rename(src, dest) == 0 ? FM_NO_ERR : (int16_t)fm_err_from_errno();
}

// OSErr FSpCatMove(const FSSpec *source, const FSSpec *dest);
// `dest` names the destination DIRECTORY; the moved item keeps its own leaf name.
static int16_t FSpCatMove(const uint8_t *sspec, const uint8_t *dspec) {
    char src[2048], dstdir[2048], dest[2048];
    const char *slash;
    size_t dl;
    if (!sspec || !dspec) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(sspec, src, sizeof src, NULL)) { return FM_NSV_ERR; }
    if (!cfs_fsspec_to_path(dspec, dstdir, sizeof dstdir, NULL)) { return FM_NSV_ERR; }
    slash = strrchr(src, '/');
    dl = strlen(dstdir);
    snprintf(dest, sizeof dest, "%s%s%s", dstdir, (dl && dstdir[dl - 1] == '/') ? "" : "/",
             slash ? slash + 1 : src);
    return rename(src, dest) == 0 ? FM_NO_ERR : (int16_t)fm_err_from_errno();
}

// OSErr FSpExchangeFiles(const FSSpec *source, const FSSpec *dest);
// The classic safe-save primitive: swap two files' CONTENTS while each keeps its identity.
// renameatx_np(RENAME_SWAP) is the modern equivalent; exchangedata() is the direct
// descendant but unimplemented on APFS. Both are looked up by name; with neither, an honest
// error rather than a half-swap.
static int16_t FSpExchangeFiles(const uint8_t *sspec, const uint8_t *dspec) {
    static int (*p_renameatx)(int, const char *, int, const char *, unsigned int);
    static int (*p_exchangedata)(const char *, const char *, unsigned int);
    static int probed;
    char a[2048], b[2048];
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
        return p_renameatx(AT_FDCWD, a, AT_FDCWD, b, 0x0002 /* RENAME_SWAP */) == 0
               ? FM_NO_ERR : (int16_t)fm_err_from_errno();
    }
    if (p_exchangedata) {
        return p_exchangedata(a, b, 0) == 0 ? FM_NO_ERR : (int16_t)fm_err_from_errno();
    }
    return FM_IO_ERR;
}

// OSErr FSpSetFInfo(const FSSpec *spec, const FInfo *fndrInfo);
// FInfo is the first 16 bytes of the 32-byte Finder-info blob; the rest (FXInfo) is read
// back first so setting a file type does not wipe the extended info.
static int16_t FSpSetFInfo(const uint8_t *spec, const uint8_t *finfo) {
    char path[2048];
    struct attrlist al;
    struct { uint32_t len; uint8_t fi[32]; } buf;
    uint8_t out[32];
    if (!spec || !finfo) { return FM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return FM_NSV_ERR; }
    memset(&al, 0, sizeof al);
    al.bitmapcount = ATTR_BIT_MAP_COUNT;
    al.commonattr = ATTR_CMN_FNDRINFO;
    memset(out, 0, sizeof out);
    if (getattrlist(path, &al, &buf, sizeof buf, 0) == 0) { memcpy(out, buf.fi, sizeof out); }
    memcpy(out, finfo, 16);
    return setattrlist(path, &al, out, sizeof out, 0) == 0 ? FM_NO_ERR
                                                          : (int16_t)fm_err_from_errno();
}

// OSErr FlushVol(ConstStr63Param volName, SInt16 vRefNum);
// Every write above is unbuffered at the syscall layer, so the data is already committed;
// sync() would be a whole-system stall.
static int16_t FlushVol(const uint8_t *volName, int16_t vRefNum) {
    (void)volName; (void)vRefNum;
    return FM_NO_ERR;
}

// ── i386 entry points ──────────────────────────────────────────────────────────────────

#define P(n) ((void *)(uintptr_t)args[(n)])

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
