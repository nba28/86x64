// shimdb/impl/alias.c — the classic ALIAS MANAGER, bridged onto the one Apple kept.
//
// ONE implementation for both ABIs: native frameworks call the entry points at the bottom
// (native AliasHandles) through shimgen; translated i386 code reaches the same record engine
// through libabiconv's glue (carbon_alias_shim.c), which only differs in how the 4-byte
// i386 AliasHandle is allocated and read.
//
// ════════════════════════════════════════════════════════════════════════════════════════
// WHAT WAS BROKEN
//
// find_null_jump_bridges.py classified these as NULL-JUMP ORPHANS: the legacy shim pass
// reads the 10.6 SDK's *i386* headers and emits a pass-through bridge `___NewAlias` that
// calls a native `_NewAlias`, and Apple deleted the FSSpec generation of the Alias Manager
// from 64-bit Carbon in 2009. The bridge could only ever `call 0`.
//
//     _NewAlias  _NewAliasMinimal  _NewAliasMinimalFromFullPath
//     _ResolveAlias  _ResolveAliasFile  _MatchAlias  _FSMatchAliasNoUI  _QTNewAlias
//
// iPhoto, iWeb and Pages resolve aliases constantly (every "recent item", every library
// reference, every linked media file), so this was a jump to zero on a very hot path.
//
// ════════════════════════════════════════════════════════════════════════════════════════
// ★★ WHY THIS IS A BRIDGE AND NOT A REIMPLEMENTATION
//
// MEASURED with the project's own x86_64 probe (src/86x64/nulljump_probe.c, built
// -arch x86_64 and run under Rosetta — the arch that matters, per 96df62e), against
// CoreServices + CoreFoundation + Carbon + QuickTime + libSystem:
//
//   ALIVE on x86_64 : FSNewAlias  FSNewAliasMinimal  FSResolveAlias  FSResolveAliasFile
//                     FSIsAliasFile  FSUpdateAlias  FSCopyAliasInfo  FSMatchAliasBulk
//                     GetAliasSize  GetAliasUserType  SetAliasUserType
//                     CFURLCreateBookmarkData  CFURLCreateByResolvingBookmarkData
//   DEAD  on x86_64 : the entire FSSpec generation (the list above), plus UpdateAlias,
//                     GetAliasInfo, IsAliasFile, FollowFinderAlias and every
//                     WithMountFlags / NoUI spelling.
//
// So — unlike the File Manager write path (4c9e3ca), where nothing survived and everything
// had to be built from scratch — Apple's own alias-record ENGINE is still here. It only
// lost its FSSpec-shaped front door. Bridging the dead spellings onto the live ones is
// strictly better than reimplementing:
//
//   * the records we MINT are byte-for-byte genuine Apple alias records, so Finder, a
//     future macOS, `mac_alias`, and the app's own 2005 code all still read them;
//   * the records we PARSE are parsed by APPLE'S OWN PARSER — the same code that read
//     them in 2005 — so a record an app saved into its document file years ago resolves
//     without us reverse-engineering the format at all.
//
// ════════════════════════════════════════════════════════════════════════════════════════
// ★★ THE ONE THING APPLE'S SURVIVING ENGINE NO LONGER DOES: FOLLOW A MOVE
//
// MEASURED (src/86x64/probes/aliasprobe2.c): FSNewAlias produces a 304-byte record; a
// resolve IN PLACE returns noErr and the right path; after `rename(one.txt, moved.txt)`
// the SAME record resolves to fnfErr (-43). The CNID/volume search that used to make an
// alias follow its target is gone from modern CarbonCore; what is left is effectively a
// stored path. The modern successor — CFURLCreateBookmarkData /
// CFURLCreateByResolvingBookmarkData — DOES follow the same move (measured: it returned
// the new path with stale=1).
//
// Neither half is sufficient on its own:
//   * bookmark only  — not a classic alias record. An app that writes the handle into its
//                      save file produces a blob nothing else can read, and a genuine
//                      historical record cannot be parsed at all.
//   * alias only     — genuine and reads history, but loses a file that moved, which is
//                      the single thing an alias exists to do.
//
// ── THE HYBRID, and why it is safe ──────────────────────────────────────────────────────
//
// A minted record is Apple's record VERBATIM, followed by an appendix:
//
//     [ genuine version-2 alias record, aliasSize bytes, ending in the -1 tag ]
//     [ "M64ALIAS" ][ UInt32 big-endian bookmark length ][ bookmark blob ]
//
// The appendix sits AFTER the record's own -1 terminator, which is Apple's OWN DOCUMENTED
// extension point — the 10.6 header says of GetAliasSize: "This will be smaller than the
// size returned by GetHandleSize if any custom data has been added (IM Files 4-13)".
// MEASURED (src/86x64/probes/aliasprobe4.c) on a real hybrid: Apple's FSResolveAlias
// returns the byte-identical answer it gives for the un-appended record, GetAliasSize is
// 304 and GetHandleSize is 1176. So:
//   * `aliasSize` at +4 keeps the truthful classic value Apple wrote — an app that
//     inspects that field, or copies exactly that many bytes, sees a normal alias;
//   * every existing parser stops at the -1 tag and never sees our bytes;
//   * an app that persists only `aliasSize` bytes still writes a VALID classic alias. It
//     merely loses the move-following half. Degradation, never corruption.
//
// An earlier variant that SPLICED a private tag in BEFORE the terminator also resolved,
// but made native GetAliasSize disagree with the record's own aliasSize field. Rejected.
//
// On the UInt16 size budget: `aliasSize` is a UInt16, but it describes only the record
// proper, which Apple keeps at a few hundred bytes. The appendix lives outside it and is
// bounded by GetHandleSize (a Size/long), so a ~900-byte bookmark costs nothing. We still
// cap the TOTAL at 64KB and fall back to record-only rather than let a pathological
// bookmark bloat an app's save file.
//
// ── RESOLUTION LADDER (any record, ours or foreign) ──────────────────────────────────────
//   1. our appendix bookmark  -> follows a move/rename. Identity beats path, which is the
//                                faithful classic ordering (the classic Alias Manager
//                                preferred the file ID over the stored path).
//   2. native FSResolveAlias  -> Apple's own parser. This is what reads GENUINE historical
//                                records, including PowerPC-era ones: the format is
//                                big-endian on disk on Intel too, so the layout never
//                                changed and the surviving parser still understands it.
//   3. our own tag-18 read    -> the POSIX path record OS X-era aliases carry, for the case
//                                where Apple's parser declines but the record names a path.
//   4. our own tag-2 read     -> the Carbon colon path, for pre-tag-18 records.
// Steps 3 and 4 are ~40 lines and are a strict addition: they can only turn a failure into
// a success. If none of the four finds an existing file we return fnfErr rather than
// fabricate a location — we deliberately do NOT do the classic volume-wide filename search,
// because on a modern multi-terabyte volume that is a multi-minute hang, not a feature.
//
#include "alias.h"
#include "fsspec.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>

// Classic File Manager / Alias Manager OSErr codes callers branch on.
#define AM_NO_ERR         0
#define AM_PARAM_ERR    (-50)    // paramErr
#define AM_NSV_ERR      (-35)    // nsvErr        — no such volume
#define AM_FNF_ERR      (-43)    // fnfErr        — file not found
// userCanceledErr (-128) is deliberately absent: it is the code the classic call returned
// when the user dismissed the "where is it?" / "mount this volume?" dialog, and no path in
// this file ever shows UI or mounts anything, so it can never arise.

// Alias record layout (version 2), big-endian on disk on every architecture.
#define AR_USERTYPE_OFF    0     // OSType         userType
#define AR_SIZE_OFF        4     // UInt16         aliasSize
#define AR_VERSION_OFF     6     // SInt16         version (2)
#define AR_TAGS_OFF      150     // start of the variable-length tag list
#define AR_TAG_END        -1     // the terminating tag
#define AR_TAG_CARBON_PATH 2     // "/:Volume:dir:leaf"
#define AR_TAG_POSIX_PATH 18     // "private/tmp/x"  (no leading '/')

// Our appendix, after the record's -1 terminator. See the header comment.
#define AM_MAGIC      "M64ALIAS"
#define AM_MAGIC_LEN  8
#define AM_APPENDIX_HDR (AM_MAGIC_LEN + 4)
#define AM_TOTAL_CAP  (64u * 1024u)

// ── the surviving native entry points ───────────────────────────────────────────────────
// Resolved by name, following fsspec.c: these are deprecated-but-present
// CarbonCore exports, and dlsym keeps libabiconv loadable if a future macOS finally drops
// one (we then degrade to the graceful-error behaviour instead of failing to link).
static int32_t (*p_FSPathMakeRef)(const uint8_t *, void *, uint8_t *);
static int32_t (*p_FSRefMakePath)(const void *, uint8_t *, uint32_t);
static int32_t (*p_FSNewAlias)(const void *, const void *, void **);
static int32_t (*p_FSNewAliasMinimal)(const void *, void **);
static int32_t (*p_FSResolveAliasWithMountFlags)(const void *, void *, void *, uint8_t *,
                                                 unsigned long);
static int32_t (*p_FSResolveAliasFileWithMountFlags)(void *, uint8_t, uint8_t *, uint8_t *,
                                                     unsigned long);
static int32_t (*p_FSIsAliasFile)(const void *, uint8_t *, uint8_t *);
static void   *(*p_NewHandle)(long);
static void    (*p_DisposeHandle)(void *);
static long    (*p_GetHandleSize)(void *);

static int am_native_ready(void) {
    static int done;
    if (!done) {
#define AM_LOAD(n, T) p_##n = (T)dlsym(RTLD_DEFAULT, #n)
        AM_LOAD(FSPathMakeRef, int32_t (*)(const uint8_t *, void *, uint8_t *));
        AM_LOAD(FSRefMakePath, int32_t (*)(const void *, uint8_t *, uint32_t));
        AM_LOAD(FSNewAlias,    int32_t (*)(const void *, const void *, void **));
        AM_LOAD(FSNewAliasMinimal, int32_t (*)(const void *, void **));
        AM_LOAD(FSResolveAliasWithMountFlags,
                int32_t (*)(const void *, void *, void *, uint8_t *, unsigned long));
        AM_LOAD(FSResolveAliasFileWithMountFlags,
                int32_t (*)(void *, uint8_t, uint8_t *, uint8_t *, unsigned long));
        AM_LOAD(FSIsAliasFile, int32_t (*)(const void *, uint8_t *, uint8_t *));
        AM_LOAD(NewHandle,     void *(*)(long));
        AM_LOAD(DisposeHandle, void  (*)(void *));
        AM_LOAD(GetHandleSize, long  (*)(void *));
#undef AM_LOAD
        done = 1;
    }
    return p_FSPathMakeRef && p_FSRefMakePath && p_FSNewAlias &&
           p_FSResolveAliasWithMountFlags && p_NewHandle && p_GetHandleSize;
}

// ── big-endian readers for the on-disk record ───────────────────────────────────────────
// The alias record is a PERSISTENT format and stayed big-endian on Intel, which is exactly
// why a PowerPC-era blob and an Intel-era blob have identical layout — and why these are
// fixed-endian rather than host-endian.
static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

// The size of the alias record PROPER: the aliasSize field when it is sane, else the whole
// buffer. A foreign record has no appendix, so record-size == buffer-size for it.
static uint32_t am_record_size(const uint8_t *rec, uint32_t len) {
    uint32_t n = be16(rec + AR_SIZE_OFF);
    if (n >= AR_TAGS_OFF && n <= len) { return n; }
    return len;
}

// Find a tag's payload in the record's variable-length tag list. Returns its length, or 0.
// Every offset is bounds-checked against `len`: the record may have come out of a file
// written years ago by software we have never seen.
static uint32_t am_find_tag(const uint8_t *rec, uint32_t len, int16_t want,
                            const uint8_t **out) {
    uint32_t n = am_record_size(rec, len);
    uint32_t o = AR_TAGS_OFF;

    *out = NULL;
    if (n < AR_TAGS_OFF + 4 || be16(rec + AR_VERSION_OFF) != 2) { return 0; }
    while (o + 4 <= n) {
        int16_t tag = (int16_t)be16(rec + o);
        uint32_t tl = be16(rec + o + 2);
        if (tag == AR_TAG_END) { break; }
        if (o + 4 + tl > n) { break; }                 // truncated / corrupt: stop, do not read
        if (tag == want) { *out = rec + o + 4; return tl; }
        o += 4 + tl + (tl & 1);                        // payloads are padded to even
    }
    return 0;
}

// Our appendix, if this record is one we minted. It begins exactly at the record size, so
// this is an O(1) check rather than a scan — and the magic makes a false positive on a
// foreign record with trailing custom data effectively impossible.
static uint32_t am_find_appendix(const uint8_t *rec, uint32_t len, const uint8_t **out) {
    uint32_t n = be16(rec + AR_SIZE_OFF);
    uint32_t bl;

    *out = NULL;
    if (n < AR_TAGS_OFF || (uint64_t)n + AM_APPENDIX_HDR > len) { return 0; }
    if (memcmp(rec + n, AM_MAGIC, AM_MAGIC_LEN) != 0) { return 0; }
    bl = be32(rec + n + AM_MAGIC_LEN);
    if (bl == 0 || (uint64_t)n + AM_APPENDIX_HDR + bl > len) { return 0; }
    *out = rec + n + AM_APPENDIX_HDR;
    return bl;
}

// ── path <-> record ─────────────────────────────────────────────────────────────────────

// Resolve a bookmark blob. WithoutUI|WithoutMounting so it can never prompt or block on a
// network volume — a classic caller has no idea it might be asked to authenticate.
static int am_path_from_bookmark(const uint8_t *blob, uint32_t len, char *out, size_t outsz) {
    CFDataRef d = CFDataCreate(NULL, blob, (CFIndex)len);
    Boolean stale = false;
    CFURLRef u;
    struct stat st;
    int ok = 0;

    if (!d) { return 0; }
    u = CFURLCreateByResolvingBookmarkData(NULL, d,
            kCFURLBookmarkResolutionWithoutUIMask | kCFURLBookmarkResolutionWithoutMountingMask,
            NULL, NULL, &stale, NULL);
    if (u) {
        if (CFURLGetFileSystemRepresentation(u, true, (UInt8 *)out, (CFIndex)outsz) &&
            stat(out, &st) == 0) {
            ok = 1;                                    // a stale answer that still EXISTS is
        }                                              // exactly the moved file we wanted
        CFRelease(u);
    }
    CFRelease(d);
    return ok;
}

// Hand the record to Apple's own parser. Needs a NATIVE Handle (CarbonCore dereferences 8
// bytes), so one is built here and disposed immediately; the i386 Handle the app owns is
// never handed to native code.
static int am_path_from_native(const uint8_t *rec, uint32_t len, char *out, size_t outsz) {
    void *h;
    uint8_t ref[CFS_FSREF_SIZE];
    uint8_t changed = 0;
    int ok = 0;

    if (!am_native_ready()) { return 0; }
    h = p_NewHandle((long)len);
    if (!h) { return 0; }
    memcpy(*(void **)h, rec, len);
    memset(ref, 0, sizeof ref);
    if (p_FSResolveAliasWithMountFlags(NULL, h, ref, &changed,
                                       1 /*kResolveAliasFileNoUI*/) == 0) {
        out[0] = '\0';
        ok = (p_FSRefMakePath(ref, (uint8_t *)out, (uint32_t)outsz) == 0 && out[0]);
    }
    if (p_DisposeHandle) { p_DisposeHandle(h); }
    return ok;
}

// The POSIX path an OS X-era record carries in tag 18. Apple stores it WITHOUT the leading
// '/' (measured: "private/tmp/x"), relative to the mountpoint in tag 19.
//
// ★`require_exists` is what separates the reader's TWO jobs, and getting it wrong is a real
// defect the guard caught: as a step in the resolution ladder the path is only an answer if
// something is actually there, but as the source of the `wasChanged` / `needsUpdate`
// comparison it is wanted precisely WHEN the recorded target no longer exists — that is the
// definition of the target having moved. One flag, two callers, no duplicated parser.
static int am_path_from_tag18(const uint8_t *rec, uint32_t len, char *out, size_t outsz,
                              int require_exists) {
    const uint8_t *p = NULL, *mp = NULL;
    uint32_t n = am_find_tag(rec, len, AR_TAG_POSIX_PATH, &p);
    uint32_t mn = am_find_tag(rec, len, 19 /*posix path to mountpoint*/, &mp);
    struct stat st;
    char mount[1024];

    if (!n) { return 0; }
    mount[0] = '\0';
    if (mn && mn < sizeof mount) {
        memcpy(mount, mp, mn);
        mount[mn] = '\0';
    }
    if (mount[0] != '/' || strcmp(mount, "/") == 0) { snprintf(out, outsz, "/%.*s", (int)n, p); }
    else { snprintf(out, outsz, "%s/%.*s", mount, (int)n, p); }
    return require_exists ? (stat(out, &st) == 0) : 1;
}

// The Carbon colon path (tag 2), for records predating tag 18. Measured form on this OS:
// "/:private:tmp:dir:leaf" — a mountpoint, then ':'-separated components with the usual
// '/' <-> ':' exchange inside each one.
static int am_path_from_tag2(const uint8_t *rec, uint32_t len, char *out, size_t outsz,
                             int require_exists) {
    const uint8_t *p = NULL;
    uint32_t n = am_find_tag(rec, len, AR_TAG_CARBON_PATH, &p);
    struct stat st;
    char buf[1024];
    size_t o = 0;
    uint32_t i = 0;

    if (!n || n >= sizeof buf) { return 0; }
    if (p[0] == '/') { i = 1; }                        // leading mountpoint marker
    buf[o++] = '/';
    for (; i < n && o + 1 < sizeof buf; i++) {
        char c = (char)p[i];
        if (c == ':') { if (o && buf[o - 1] != '/') { buf[o++] = '/'; } }
        else { buf[o++] = (c == '/') ? ':' : c; }
    }
    buf[o] = '\0';
    snprintf(out, outsz, "%s", buf);
    return require_exists ? (stat(out, &st) == 0) : 1;
}

// ★The resolution ladder. `moved` (optional) reports whether the answer differs from the
// path the record itself records — the classic `wasChanged` / `needsUpdate` question.
int am_path_from_record(const uint8_t *rec, uint32_t len, char *out, size_t outsz,
                        int *moved) {
    const uint8_t *blob = NULL;
    uint32_t bl;
    char recorded[1024];
    int have_recorded;
    int ok = 0;

    if (moved) { *moved = 0; }
    if (!rec || len < AR_TAGS_OFF) { return 0; }
    out[0] = '\0';

    // What the record itself says the path WAS, used only to answer `wasChanged`. Read with
    // require_exists = 0: the interesting case is precisely the one where that path is now
    // empty. Reading it cannot fail the resolve.
    recorded[0] = '\0';
    have_recorded = am_path_from_tag18(rec, len, recorded, sizeof recorded, 0) ||
                    am_path_from_tag2(rec, len, recorded, sizeof recorded, 0);

    bl = am_find_appendix(rec, len, &blob);
    if (bl && am_path_from_bookmark(blob, bl, out, outsz)) { ok = 1; }
    if (!ok && am_path_from_native(rec, len, out, outsz)) { ok = 1; }
    if (!ok && am_path_from_tag18(rec, len, out, outsz, 1)) { ok = 1; }
    if (!ok && am_path_from_tag2(rec, len, out, outsz, 1)) { ok = 1; }

    if (ok && moved && have_recorded) { *moved = (strcmp(out, recorded) != 0); }
    return ok;
}

// Mint the payload for a target path: Apple's genuine record, plus (with_bookmark) our
// bookmark appendix. `*out` is a malloc'd buffer the caller frees. Returns an OSErr.
int am_record_from_path(const char *path, int minimal, int with_bookmark,
                        uint8_t **out, uint32_t *outlen) {
    uint8_t ref[CFS_FSREF_SIZE];
    void *h = NULL;
    long n;
    const uint8_t *rec;
    uint8_t *buf;
    uint32_t total;
    CFURLRef u = NULL;
    CFDataRef bm = NULL;
    CFIndex bl = 0;

    *out = NULL;
    *outlen = 0;
    if (!am_native_ready()) { return AM_NSV_ERR; }

    memset(ref, 0, sizeof ref);
    if (p_FSPathMakeRef((const uint8_t *)path, ref, NULL) != 0) { return AM_FNF_ERR; }
    if (minimal && p_FSNewAliasMinimal) {
        if (p_FSNewAliasMinimal(ref, &h) != 0 || !h) { return AM_FNF_ERR; }
    } else {
        if (p_FSNewAlias(NULL, ref, &h) != 0 || !h) { return AM_FNF_ERR; }
    }
    n = p_GetHandleSize(h);
    rec = *(const uint8_t **)h;
    if (n < AR_TAGS_OFF || !rec) {
        if (p_DisposeHandle) { p_DisposeHandle(h); }
        return AM_FNF_ERR;
    }

    // The move-following half. A failure here is NOT fatal: we still hand back a perfectly
    // good classic alias, it just cannot follow a move.
    if (with_bookmark) {
        u = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path,
                                                    (CFIndex)strlen(path), false);
        if (u) {
            bm = CFURLCreateBookmarkData(NULL, u, 0, NULL, NULL, NULL);
            CFRelease(u);
        }
    }
    if (bm) { bl = CFDataGetLength(bm); }
    total = (uint32_t)n;
    if (bl > 0 && (uint64_t)n + AM_APPENDIX_HDR + (uint64_t)bl <= AM_TOTAL_CAP) {
        total = (uint32_t)(n + AM_APPENDIX_HDR + bl);
    } else {
        bl = 0;                                        // record-only; see the size note above
    }

    buf = (uint8_t *)malloc(total);
    if (buf) {
        memcpy(buf, rec, (size_t)n);
        if (bl > 0) {
            memcpy(buf + n, AM_MAGIC, AM_MAGIC_LEN);
            put_be32(buf + n + AM_MAGIC_LEN, (uint32_t)bl);
            memcpy(buf + n + AM_APPENDIX_HDR, CFDataGetBytePtr(bm), (size_t)bl);
        }
        *out = buf;
        *outlen = total;
    }
    if (bm) { CFRelease(bm); }
    if (p_DisposeHandle) { p_DisposeHandle(h); }
    return buf ? AM_NO_ERR : AM_NSV_ERR;
}

// ── NewAliasMinimalFromFullPath's path argument ─────────────────────────────────────────
// `fullPath` is `plen` raw bytes, NOT NUL-terminated and NOT a Pascal string. It is
// classically an HFS colon path ("Volume:dir:leaf"), but plenty of OS X-era code passes a
// POSIX path, so both are accepted — distinguished structurally by the leading '/'.
// zoneName/serverName (AppleTalk, removed in 10.6, no successor) are ignored by callers.
// Returns an OSErr; on success `path` names an existing file.
int am_path_from_fullpath(const char *fp, int plen, char *path, size_t pathsz) {
    char raw[1024], cand[1024];
    struct stat st;
    size_t o = 0;
    int i;
    char vol[256];
    size_t vlen = 0;

    if (!fp || plen <= 0 || (size_t)plen >= sizeof raw || pathsz < 2) { return AM_PARAM_ERR; }
    memcpy(raw, fp, (size_t)plen);
    raw[plen] = '\0';
    if (raw[0] == '/') {
        snprintf(path, pathsz, "%s", raw);
    } else {
        // HFS colon path. The first component names the VOLUME; the rest are directories,
        // each with the usual '/' <-> ':' exchange inside the name.
        for (i = 0; raw[i] && raw[i] != ':' && vlen + 1 < sizeof vol; i++) { vol[vlen++] = raw[i]; }
        vol[vlen] = '\0';
        for (; raw[i]; i++) {
            char c = raw[i];
            if (c == ':') { if (o && path[o - 1] != '/') { path[o++] = '/'; } }
            else if (o + 1 < pathsz) { path[o++] = (c == '/') ? ':' : c; }
            if (o + 1 >= pathsz) { break; }
        }
        path[o] = '\0';
        // Two candidate mount points for that volume name. Pick the one that actually names
        // an existing file — exact, rather than assuming the boot volume either way.
        snprintf(cand, sizeof cand, "/Volumes/%s%s%s", vol, path[0] == '/' ? "" : "/", path);
        if (stat(cand, &st) == 0) { snprintf(path, pathsz, "%s", cand); }
        else {
            snprintf(cand, sizeof cand, "%s%s", path[0] == '/' ? "" : "/", path);
            snprintf(path, pathsz, "%s", cand);
        }
    }
    return stat(path, &st) == 0 ? AM_NO_ERR : AM_FNF_ERR;
}

// ── RESOLVE: record bytes -> FSSpec ─────────────────────────────────────────────────────
// `wasChanged` is reported truthfully (the resolved path differs from the one the record
// stores) but the record is NOT rewritten in place: that would resize a Handle the caller
// may already have dereferenced, and the classic API has UpdateAlias for it. Callers that
// check wasChanged and re-save get the right answer either way, because the bookmark half
// keeps working after any number of moves. Every out-param is defined before any failure
// return (the Halo nil-CFStringRef rule).
int am_resolve_bytes(const uint8_t *rec, uint32_t len, uint8_t *target, uint8_t *changed) {
    char path[2048];
    int moved = 0;
    if (changed) { *changed = 0; }
    if (target) { memset(target, 0, CFS_FSSPEC_SIZE); }
    if (!rec || !target || len < AR_TAGS_OFF) { return AM_PARAM_ERR; }
    if (!am_path_from_record(rec, len, path, sizeof path, &moved)) { return AM_FNF_ERR; }
    if (!cfs_path_to_fsspec(path, target)) { return AM_NSV_ERR; }
    if (changed) { *changed = (uint8_t)(moved != 0); }
    return AM_NO_ERR;
}

// ── ResolveAliasFile: resolve a file spec IN PLACE ──────────────────────────────────────
//
// Three things a translated app will actually meet, in the order it meets them:
//   1. a Finder ALIAS FILE — handed to the surviving native FSResolveAliasFile, which knows
//      the on-disk representation (resource fork, kIsAlias Finder bit, bookmark file);
//   2. a POSIX SYMLINK — the modern equivalent, and the one a 2026 file system actually
//      contains. The classic API never knew about it, so nothing else will follow it;
//   3. a plain file — noErr, wasAliased false, spec untouched, which is exactly what the
//      classic call does.
//
// `resolveAliasChains` selects between following the whole chain and taking one step.
// realpath() is the whole chain; a single readlink() is one step.
// OSErr ResolveAliasFile(FSSpec *theSpec, Boolean resolveAliasChains,
//                        Boolean *targetIsFolder, Boolean *wasAliased);
int16_t ResolveAliasFile(uint8_t *spec, uint8_t chains, uint8_t *targetIsFolder,
                         uint8_t *wasAliased) {
    char path[2048], resolved[2048];
    struct stat st;
    int aliased = 0;

    // ★Out-params first, on EVERY path including every early error.
    if (targetIsFolder) { *targetIsFolder = 0; }
    if (wasAliased) { *wasAliased = 0; }

    if (!spec) { return AM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return AM_NSV_ERR; }
    if (lstat(path, &st) != 0) { return AM_FNF_ERR; }

    snprintf(resolved, sizeof resolved, "%s", path);

    if (S_ISLNK(st.st_mode)) {
        // (2) POSIX symlink.
        if (chains) {
            if (!realpath(path, resolved)) { return AM_FNF_ERR; }
        } else {
            char link[2048];
            ssize_t n = readlink(path, link, sizeof link - 1);
            if (n <= 0) { return AM_FNF_ERR; }
            link[n] = '\0';
            if (link[0] == '/') { snprintf(resolved, sizeof resolved, "%s", link); }
            else {
                const char *slash = strrchr(path, '/');
                int dirlen = slash ? (int)(slash - path) : 0;
                snprintf(resolved, sizeof resolved, "%.*s/%s", dirlen, path, link);
            }
        }
        aliased = 1;
    } else if (am_native_ready() && p_FSResolveAliasFileWithMountFlags && p_FSPathMakeRef) {
        // (1) Finder alias file. Ask the surviving native implementation, which owns the
        // on-disk representation; we only marshal the FSSpec ends of the call.
        uint8_t ref[CFS_FSREF_SIZE];
        uint8_t folder = 0, was = 0;
        memset(ref, 0, sizeof ref);
        if (p_FSPathMakeRef((const uint8_t *)path, ref, NULL) == 0 &&
            p_FSResolveAliasFileWithMountFlags(ref, (uint8_t)(chains != 0), &folder, &was,
                                               1 /*kResolveAliasFileNoUI*/) == 0 && was) {
            resolved[0] = '\0';
            if (p_FSRefMakePath(ref, (uint8_t *)resolved, (uint32_t)sizeof resolved) == 0 &&
                resolved[0]) {
                aliased = 1;
            } else {
                snprintf(resolved, sizeof resolved, "%s", path);
            }
        }
    }

    if (aliased) {
        if (!cfs_path_to_fsspec(resolved, spec)) {
            // The spec was zeroed by cfs_path_to_fsspec; rebuild the INPUT spec so the
            // caller is left with what it passed in rather than a zeroed one.
            cfs_path_to_fsspec(path, spec);
            return AM_NSV_ERR;
        }
        if (wasAliased) { *wasAliased = 1; }
    }
    if (targetIsFolder && stat(resolved, &st) == 0) {
        *targetIsFolder = (uint8_t)(S_ISDIR(st.st_mode) != 0);
    }
    return AM_NO_ERR;
}

// ════════════════════════════════════════════════════════════════════════════════════════
// MATCH
// ════════════════════════════════════════════════════════════════════════════════════════
//
// OSErr MatchAlias(const FSSpec *fromFile, unsigned long rulesMask, AliasHandle alias,
//                  short *aliasCount, FSSpecArrayPtr aliasList, Boolean *needsUpdate,
//                  AliasFilterUPP aliasFilter, void *yourDataPtr);
//
// ★`aliasCount` is IN/OUT: on input it is how many entries `aliasList` can hold, on output
// how many were written. It is therefore READ before it is overwritten, and set to 0 before
// EVERY failure return — a caller that ignores the OSErr and loops `for (i = 0; i < count;
// i++)` over an untouched stack variable is the classic-Carbon idiom this rule exists for.
//
// We return at most ONE candidate: the alias's actual target. The classic call could return
// several because it performed a volume-wide search for same-named files; that search is
// what we deliberately do not do (on a modern multi-terabyte volume it is a multi-minute
// hang, not a feature), and one exact answer is strictly more useful than several guesses.
//
// Consequently `aliasFilter` is NOT invoked. Its purpose is to let the app discard unwanted
// candidates from a fuzzy multi-result search; with an exact single result there is nothing
// to discard, and invoking an i386 callback we do not need would add a reverse-trampoline
// dependency for no behavioural gain. Documented rather than silent.
//
// kARMNoUI is honoured trivially: no path here ever shows UI, so userCanceledErr can never
// arise. The code is shared with FSMatchAliasNoUI, which differs only in the element type of
// the output array (FSRef instead of FSSpec).
int am_match_bytes(const uint8_t *rec, uint32_t len, int16_t *countv, uint8_t *list,
                   uint8_t *needsUpdate, int fsref_list) {
    char path[2048];
    int moved = 0;
    int16_t maxOut;

    maxOut = countv ? *countv : 0;                     // IN: capacity, read before clearing
    if (countv) { *countv = 0; }                       // ★OUT: zeroed before any failure
    if (needsUpdate) { *needsUpdate = 0; }

    if (!rec || !countv || !list || len < AR_TAGS_OFF) { return AM_PARAM_ERR; }
    if (maxOut <= 0) { return AM_PARAM_ERR; }
    if (!am_path_from_record(rec, len, path, sizeof path, &moved)) { return AM_FNF_ERR; }

    if (fsref_list) {
        if (!am_native_ready() || !p_FSPathMakeRef) { return AM_NSV_ERR; }
        memset(list, 0, CFS_FSREF_SIZE);
        if (p_FSPathMakeRef((const uint8_t *)path, list, NULL) != 0) { return AM_FNF_ERR; }
    } else {
        if (!cfs_path_to_fsspec(path, list)) { return AM_NSV_ERR; }
    }
    *countv = 1;
    if (needsUpdate) { *needsUpdate = (uint8_t)(moved != 0); }
    return AM_NO_ERR;
}

// ════════════════════════════════════════════════════════════════════════════════════════
// NATIVE ENTRY POINTS (x86_64 callers; AliasHandle = native Handle)
// ════════════════════════════════════════════════════════════════════════════════════════

static void **am_native_handle(const uint8_t *b, uint32_t n) {
    void **h;
    if (!am_native_ready()) { return NULL; }
    h = (void **)p_NewHandle((long)n);
    if (h && *h) { memcpy(*h, b, n); }
    return h;
}

static int am_new_from_path(const char *path, int minimal, void ***alias) {
    uint8_t *buf = NULL;
    uint32_t blen = 0;
    int e = am_record_from_path(path, minimal, 1, &buf, &blen);
    if (e != AM_NO_ERR) { return e; }
    *alias = am_native_handle(buf, blen);
    free(buf);
    return *alias ? AM_NO_ERR : AM_NSV_ERR;
}

static int am_new_from_spec(const uint8_t *spec, int minimal, void ***alias) {
    char path[2048];
    int exists = 0;
    if (alias) { *alias = NULL; }
    if (!spec || !alias) { return AM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, &exists)) { return AM_NSV_ERR; }
    if (!exists) { return AM_FNF_ERR; }
    return am_new_from_path(path, minimal, alias);
}

// OSErr NewAlias(const FSSpec *fromFile, const FSSpec *target, AliasHandle *alias);
// `fromFile` (the relative-search anchor) is accepted and ignored: it only mattered for the
// volume-wide search this implementation deliberately does not perform.
int16_t NewAlias(const uint8_t *fromFile, const uint8_t *target, void ***alias) {
    (void)fromFile;
    return (int16_t)am_new_from_spec(target, 0, alias);
}
// OSErr NewAliasMinimal(const FSSpec *target, AliasHandle *alias);
int16_t NewAliasMinimal(const uint8_t *target, void ***alias) {
    return (int16_t)am_new_from_spec(target, 1, alias);
}
// OSErr QTNewAlias(const FSSpec *fss, AliasHandle *alias, Boolean minimal);
int16_t QTNewAlias(const uint8_t *fss, void ***alias, uint8_t minimal) {
    return (int16_t)am_new_from_spec(fss, minimal != 0, alias);
}
// OSErr NewAliasMinimalFromFullPath(short fullPathLength, const void *fullPath,
//                                   ConstStr32Param zoneName, ConstStr31Param serverName,
//                                   AliasHandle *alias);
int16_t NewAliasMinimalFromFullPath(int16_t len, const void *fullPath, const uint8_t *zone,
                                    const uint8_t *server, void ***alias) {
    char path[1024];
    int e;
    (void)zone; (void)server;
    if (alias) { *alias = NULL; }
    if (!alias) { return AM_PARAM_ERR; }
    e = am_path_from_fullpath((const char *)fullPath, len, path, sizeof path);
    return (int16_t)(e != AM_NO_ERR ? e : am_new_from_path(path, 1, alias));
}

static const uint8_t *am_native_bytes(void **alias, uint32_t *len) {
    *len = 0;
    if (!alias || !*alias || !am_native_ready()) { return NULL; }
    *len = (uint32_t)p_GetHandleSize(alias);
    return (const uint8_t *)*alias;
}

// OSErr ResolveAlias(const FSSpec *fromFile, AliasHandle alias, FSSpec *target,
//                    Boolean *wasChanged);
int16_t ResolveAlias(const uint8_t *fromFile, void **alias, uint8_t *target, uint8_t *wasChanged) {
    uint32_t len;
    const uint8_t *rec = am_native_bytes(alias, &len);
    (void)fromFile;
    return (int16_t)am_resolve_bytes(rec, len, target, wasChanged);
}
// The mount flags only select whether the user may be prompted; we never prompt or mount.
int16_t ResolveAliasWithMountFlags(const uint8_t *fromFile, void **alias, uint8_t *target,
                                   uint8_t *wasChanged, unsigned long mountFlags) {
    (void)mountFlags;
    return ResolveAlias(fromFile, alias, target, wasChanged);
}
int16_t ResolveAliasFileWithMountFlags(uint8_t *spec, uint8_t chains, uint8_t *targetIsFolder,
                                       uint8_t *wasAliased, unsigned long mountFlags) {
    (void)mountFlags;
    return ResolveAliasFile(spec, chains, targetIsFolder, wasAliased);
}
int16_t ResolveAliasFileWithMountFlagsNoUI(uint8_t *spec, uint8_t chains,
                                           uint8_t *targetIsFolder, uint8_t *wasAliased,
                                           unsigned long mountFlags) {
    (void)mountFlags;
    return ResolveAliasFile(spec, chains, targetIsFolder, wasAliased);
}

// OSErr MatchAlias(const FSSpec *fromFile, unsigned long rulesMask, AliasHandle alias,
//                  short *aliasCount, FSSpecArrayPtr aliasList, Boolean *needsUpdate,
//                  AliasFilterUPP aliasFilter, void *yourDataPtr);
int16_t MatchAlias(const uint8_t *fromFile, unsigned long rules, void **alias, int16_t *count,
                   uint8_t *list, uint8_t *needsUpdate, void *filter, void *data) {
    uint32_t len;
    const uint8_t *rec = am_native_bytes(alias, &len);
    (void)fromFile; (void)rules; (void)filter; (void)data;
    return (int16_t)am_match_bytes(rec, len, count, list, needsUpdate, 0);
}
int16_t MatchAliasNoUI(const uint8_t *fromFile, unsigned long rules, void **alias,
                       int16_t *count, uint8_t *list, uint8_t *needsUpdate, void *filter,
                       void *data) {
    return MatchAlias(fromFile, rules, alias, count, list, needsUpdate, filter, data);
}
// The FSRef-array spelling: dead on x86_64 even though the rest of the FSRef generation
// survives (Apple kept only FSMatchAliasBulk).
int16_t FSMatchAliasNoUI(const uint8_t *fromFile, unsigned long rules, void **alias,
                         int16_t *count, uint8_t *list, uint8_t *needsUpdate, void *filter,
                         void *data) {
    uint32_t len;
    const uint8_t *rec = am_native_bytes(alias, &len);
    (void)fromFile; (void)rules; (void)filter; (void)data;
    return (int16_t)am_match_bytes(rec, len, count, list, needsUpdate, 1);
}
