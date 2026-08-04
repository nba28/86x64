// carbon_alias_shim.c — the classic ALIAS MANAGER, bridged onto the one Apple kept.
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
// ════════════════════════════════════════════════════════════════════════════════════════
// An AliasHandle is a classic Handle, so it is allocated through carbon_memory.c
// (cm_new_handle), NOT malloc: DisposeHandle / GetHandleSize / SetHandleSize all work on
// it, and the 4-byte i386 Handle representation is preserved.
//
// The FSSpec <-> POSIX mapping is taken from carbon_fsspec_shim.c through carbon_fsspec.h
// rather than copied, so there is exactly one implementation of the volfs / dirID-table /
// HFS-name-swap rules.
//
// UNIVERSAL: this triggers on a structural property (a dead FSSpec-generation Alias Manager
// entry point), never on an app name. Every translated Carbon app that resolves an alias
// benefits.
//
// Kill switch: ABICONV_ALIAS_LEGACY=1 makes every entry point decline with a File Manager
// error, out-params defined, nothing resolved and nothing minted — the pre-fix behaviour
// minus the jump to zero. That is the OFF arm of tests-i386 `make carbon-alias`.
// Trace: ABICONV_ALIAS_TRACE=1.
//
// Wired through carbon_alias_tramp.asm (MTSHIM: rdi -> &i386 args[0], result in eax).

#include "carbon_shim.h"
#include "carbon_fsspec.h"

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

// ── kill switch / trace ─────────────────────────────────────────────────────────────────
static int alias_legacy(void) {
    static int c = -1;
    if (c < 0) { const char *e = getenv("ABICONV_ALIAS_LEGACY"); c = (e && *e && *e != '0'); }
    return c;
}
static int alias_trace(void) {
    static int c = -1;
    if (c < 0) { c = getenv("ABICONV_ALIAS_TRACE") != NULL; }
    return c;
}
static void am_trace(const char *what, const char *a, int err) {
    if (!alias_trace()) { return; }
    fprintf(stderr, "[alias] %s '%s' -> %d\n", what, a ? a : "", err);
    fflush(stderr);
}

// ── the surviving native entry points ───────────────────────────────────────────────────
// Resolved by name, following carbon_fsspec_shim.c: these are deprecated-but-present
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
static int am_path_from_tag18(const uint8_t *rec, uint32_t len, char *out, size_t outsz) {
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
    return stat(out, &st) == 0;
}

// The Carbon colon path (tag 2), for records predating tag 18. Measured form on this OS:
// "/:private:tmp:dir:leaf" — a mountpoint, then ':'-separated components with the usual
// '/' <-> ':' exchange inside each one.
static int am_path_from_tag2(const uint8_t *rec, uint32_t len, char *out, size_t outsz) {
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
    return stat(out, &st) == 0;
}

// ★The resolution ladder. `moved` (optional) reports whether the answer differs from the
// path the record itself records — the classic `wasChanged` / `needsUpdate` question.
static int am_path_from_record(const uint8_t *rec, uint32_t len, char *out, size_t outsz,
                               int *moved) {
    const uint8_t *blob = NULL;
    uint32_t bl;
    char recorded[1024];
    int have_recorded;
    int ok = 0;

    if (moved) { *moved = 0; }
    if (!rec || len < AR_TAGS_OFF) { return 0; }
    out[0] = '\0';

    // What the record itself says the path was, used only to answer `wasChanged`. Reading
    // it cannot fail the resolve.
    recorded[0] = '\0';
    have_recorded = am_path_from_tag18(rec, len, recorded, sizeof recorded) ||
                    am_path_from_tag2(rec, len, recorded, sizeof recorded);

    bl = am_find_appendix(rec, len, &blob);
    if (bl && am_path_from_bookmark(blob, bl, out, outsz)) { ok = 1; }
    if (!ok && am_path_from_native(rec, len, out, outsz)) { ok = 1; }
    if (!ok && am_path_from_tag18(rec, len, out, outsz)) { ok = 1; }
    if (!ok && am_path_from_tag2(rec, len, out, outsz)) { ok = 1; }

    if (ok && moved && have_recorded) { *moved = (strcmp(out, recorded) != 0); }
    return ok;
}

// Mint the payload for a target path: Apple's genuine record, plus our bookmark appendix.
// `*out` is a malloc'd buffer the caller frees. Returns an OSErr.
static int am_record_from_path(const char *path, int minimal, uint8_t **out, uint32_t *outlen) {
    uint8_t ref[CFS_FSREF_SIZE];
    void *h = NULL;
    long n;
    const uint8_t *rec;
    uint8_t *buf;
    uint32_t total;
    CFURLRef u;
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
    u = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path,
                                                (CFIndex)strlen(path), false);
    if (u) {
        bm = CFURLCreateBookmarkData(NULL, u, 0, NULL, NULL, NULL);
        CFRelease(u);
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

// ── i386 Handle plumbing ────────────────────────────────────────────────────────────────
// An AliasHandle IS a classic Handle, so it comes from carbon_memory.c: DisposeHandle,
// GetHandleSize and SetHandleSize all work on it and the 4-byte i386 representation holds.
static uint32_t am_handle_from_bytes(const uint8_t *b, uint32_t n) {
    uint32_t h = cm_new_handle(n, 0);
    if (h) { memcpy(cm_handle_block(h), b, n); }
    return h;
}

// Read an i386 AliasHandle back. Handles BOTH a Handle we minted and one the app built
// itself (NewHandle + read the bytes out of its own save file) — cm_handle_size knows the
// exact logical size of either, because shim_NewHandle wrote the same header.
static const uint8_t *am_bytes_from_handle(uint32_t hdl, uint32_t *len) {
    void *blk;
    *len = 0;
    if (!hdl) { return NULL; }
    blk = cm_handle_block(hdl);
    if (!blk) { return NULL; }
    *len = cm_handle_size(hdl);
    return (const uint8_t *)blk;
}

// Mint a record for a target FSSpec and publish it into the caller's AliasHandle* slot.
// `slot` is an i386 pointer; it is zeroed BEFORE anything can fail.
static int am_new_from_spec(uint32_t specp, uint32_t slotp, int minimal, const char *what) {
    uint32_t *slot = (uint32_t *)(uintptr_t)slotp;
    const uint8_t *spec = (const uint8_t *)(uintptr_t)specp;
    char path[2048];
    uint8_t *buf = NULL;
    uint32_t blen = 0;
    int exists = 0, e;

    if (slot) { *slot = 0; }                           // ★out-param defined first, always
    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!spec || !slot) { return AM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, &exists)) { return AM_NSV_ERR; }
    if (!exists) { am_trace(what, path, AM_FNF_ERR); return AM_FNF_ERR; }

    e = am_record_from_path(path, minimal, &buf, &blen);
    if (e != AM_NO_ERR) { am_trace(what, path, e); return e; }
    *slot = am_handle_from_bytes(buf, blen);
    free(buf);
    if (!*slot) { return AM_NSV_ERR; }
    am_trace(what, path, AM_NO_ERR);
    return AM_NO_ERR;
}

// ════════════════════════════════════════════════════════════════════════════════════════
// CREATE
// ════════════════════════════════════════════════════════════════════════════════════════

// OSErr NewAlias(const FSSpec *fromFile, const FSSpec *target, AliasHandle *alias);
// `fromFile` is the classic relative-search anchor. It is accepted and ignored: the anchor
// only ever mattered for the volume-wide search this implementation deliberately does not
// perform (see the header), and both the bookmark and Apple's own parser are absolute.
int shim_NewAlias(uint32_t *a) { return am_new_from_spec(a[1], a[2], 0, "NewAlias"); }

// OSErr NewAliasMinimal(const FSSpec *target, AliasHandle *alias);
int shim_NewAliasMinimal(uint32_t *a) {
    return am_new_from_spec(a[0], a[1], 1, "NewAliasMinimal");
}

// OSErr QTNewAlias(const FSSpec *fss, AliasHandle *alias, Boolean minimal);
// QuickTime's spelling of exactly this job — same FSSpec in, same AliasHandle out, with the
// minimal/full choice as an argument instead of a separate entry point. It lives here rather
// than with the QuickTime shims because minting an alias record is THIS module's job, and a
// second copy of the record builder is precisely what "one shim, one job" forbids.
int shim_QTNewAlias(uint32_t *a) {
    return am_new_from_spec(a[0], a[1], a[2] != 0, "QTNewAlias");
}

// OSErr NewAliasMinimalFromFullPath(short fullPathLength, const void *fullPath,
//                                   ConstStr32Param zoneName, ConstStr31Param serverName,
//                                   AliasHandle *alias);
//
// `fullPath` is fullPathLength raw bytes, NOT NUL-terminated and NOT a Pascal string. It is
// classically an HFS colon path ("Volume:dir:leaf"), but plenty of OS X-era code passes a
// POSIX path, so both are accepted — distinguished structurally by the leading '/', never by
// guesswork about the caller.
//
// zoneName/serverName are the AppleTalk zone and AFP server name. AppleTalk was removed from
// macOS in 10.6 and there is no successor to map them onto, so they are ignored; a caller
// that passes them still gets its local path resolved rather than a jump to zero.
int shim_NewAliasMinimalFromFullPath(uint32_t *a) {
    int16_t plen = (int16_t)(uint16_t)a[0];
    const char *fp = (const char *)(uintptr_t)a[1];
    uint32_t *slot = (uint32_t *)(uintptr_t)a[4];
    char raw[1024], path[1024], cand[1024];
    uint8_t *buf = NULL;
    uint32_t blen = 0;
    struct stat st;
    size_t o = 0;
    int e, i;
    char vol[256];
    size_t vlen = 0;

    if (slot) { *slot = 0; }                           // ★out-param defined first
    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!fp || plen <= 0 || !slot) { return AM_PARAM_ERR; }
    if ((size_t)plen >= sizeof raw) { return AM_PARAM_ERR; }
    memcpy(raw, fp, (size_t)plen);
    raw[plen] = '\0';

    if (raw[0] == '/') {
        snprintf(path, sizeof path, "%s", raw);
    } else {
        // HFS colon path. The first component names the VOLUME; the rest are directories,
        // each with the usual '/' <-> ':' exchange inside the name.
        for (i = 0; raw[i] && raw[i] != ':' && vlen + 1 < sizeof vol; i++) { vol[vlen++] = raw[i]; }
        vol[vlen] = '\0';
        o = 0;
        for (; raw[i]; i++) {
            char c = raw[i];
            if (c == ':') { if (o && path[o - 1] != '/') { path[o++] = '/'; } }
            else if (o + 1 < sizeof path) { path[o++] = (c == '/') ? ':' : c; }
            if (o + 1 >= sizeof path) { break; }
        }
        path[o] = '\0';
        // Two candidate mount points for that volume name. Pick the one that actually names
        // an existing file — exact, rather than assuming the boot volume either way.
        snprintf(cand, sizeof cand, "/Volumes/%s%s%s", vol, path[0] == '/' ? "" : "/", path);
        if (stat(cand, &st) == 0) { snprintf(path, sizeof path, "%s", cand); }
        else {
            snprintf(cand, sizeof cand, "%s%s", path[0] == '/' ? "" : "/", path);
            snprintf(path, sizeof path, "%s", cand);
        }
    }
    if (stat(path, &st) != 0) { am_trace("NewAliasMinimalFromFullPath", path, AM_FNF_ERR);
                                return AM_FNF_ERR; }

    e = am_record_from_path(path, 1, &buf, &blen);
    if (e != AM_NO_ERR) { am_trace("NewAliasMinimalFromFullPath", path, e); return e; }
    *slot = am_handle_from_bytes(buf, blen);
    free(buf);
    if (!*slot) { return AM_NSV_ERR; }
    am_trace("NewAliasMinimalFromFullPath", path, AM_NO_ERR);
    return AM_NO_ERR;
}

// ════════════════════════════════════════════════════════════════════════════════════════
// RESOLVE
// ════════════════════════════════════════════════════════════════════════════════════════

// OSErr ResolveAlias(const FSSpec *fromFile, AliasHandle alias, FSSpec *target,
//                    Boolean *wasChanged);
//
// `wasChanged` is reported truthfully (the resolved path differs from the one the record
// stores) but the record is NOT rewritten in place. Rewriting would have to resize a Handle
// the caller may already have dereferenced, and the classic API has an explicit call for
// that — UpdateAlias — which is not on the NULL-jump list, so nothing needs it. Callers that
// check wasChanged and re-save get the right answer either way, because the bookmark half
// of the record keeps working after any number of moves.
static int am_resolve_common(uint32_t hdl, uint32_t targetp, uint32_t changedp,
                             const char *what) {
    uint8_t *target = (uint8_t *)(uintptr_t)targetp;
    uint8_t *changed = (uint8_t *)(uintptr_t)changedp;
    const uint8_t *rec;
    uint32_t len = 0;
    char path[2048];
    int moved = 0;

    // ★Every out-param defined before any failure return. A classic caller that ignores the
    // OSErr must never read its own uninitialised stack back as an FSSpec — the rule that
    // came out of the Halo nil-CFStringRef crash.
    if (changed) { *changed = 0; }
    if (target) { memset(target, 0, CFS_FSSPEC_SIZE); }

    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!hdl || !target) { return AM_PARAM_ERR; }
    rec = am_bytes_from_handle(hdl, &len);
    if (!rec || len < AR_TAGS_OFF) { return AM_PARAM_ERR; }

    if (!am_path_from_record(rec, len, path, sizeof path, &moved)) {
        am_trace(what, "", AM_FNF_ERR);
        return AM_FNF_ERR;
    }
    if (!cfs_path_to_fsspec(path, target)) { am_trace(what, path, AM_NSV_ERR); return AM_NSV_ERR; }
    if (changed) { *changed = (uint8_t)(moved != 0); }
    am_trace(what, path, AM_NO_ERR);
    return AM_NO_ERR;
}

int shim_ResolveAlias(uint32_t *a) {
    return am_resolve_common(a[1], a[2], a[3], "ResolveAlias");
}

// OSErr ResolveAliasWithMountFlags(const FSSpec *fromFile, AliasHandle alias, FSSpec *target,
//                                  Boolean *wasChanged, unsigned long mountFlags);
// The mount flags select whether the user may be prompted to mount a volume. We never
// prompt and never mount (a classic caller has no idea it might be asked to authenticate),
// so this is the same function with one more ignored argument.
int shim_ResolveAliasWithMountFlags(uint32_t *a) {
    return am_resolve_common(a[1], a[2], a[3], "ResolveAliasWithMountFlags");
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
static int am_resolve_file_common(uint32_t specp, uint32_t chains, uint32_t folderp,
                                  uint32_t aliasedp, const char *what) {
    uint8_t *spec = (uint8_t *)(uintptr_t)specp;
    uint8_t *targetIsFolder = (uint8_t *)(uintptr_t)folderp;
    uint8_t *wasAliased = (uint8_t *)(uintptr_t)aliasedp;
    char path[2048], resolved[2048];
    struct stat st;
    int aliased = 0;

    // ★Out-params first, on EVERY path including the kill switch and every early error.
    if (targetIsFolder) { *targetIsFolder = 0; }
    if (wasAliased) { *wasAliased = 0; }

    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!spec) { return AM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, NULL)) { return AM_NSV_ERR; }
    if (lstat(path, &st) != 0) { am_trace(what, path, AM_FNF_ERR); return AM_FNF_ERR; }

    snprintf(resolved, sizeof resolved, "%s", path);

    if (S_ISLNK(st.st_mode)) {
        // (2) POSIX symlink.
        if (chains) {
            if (!realpath(path, resolved)) { am_trace(what, path, AM_FNF_ERR); return AM_FNF_ERR; }
        } else {
            char link[2048];
            ssize_t n = readlink(path, link, sizeof link - 1);
            if (n <= 0) { am_trace(what, path, AM_FNF_ERR); return AM_FNF_ERR; }
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
            am_trace(what, resolved, AM_NSV_ERR);
            return AM_NSV_ERR;
        }
        if (wasAliased) { *wasAliased = 1; }
    }
    if (targetIsFolder && stat(resolved, &st) == 0) {
        *targetIsFolder = (uint8_t)(S_ISDIR(st.st_mode) != 0);
    }
    am_trace(what, resolved, AM_NO_ERR);
    return AM_NO_ERR;
}

// OSErr ResolveAliasFile(FSSpec *theSpec, Boolean resolveAliasChains,
//                        Boolean *targetIsFolder, Boolean *wasAliased);
int shim_ResolveAliasFile(uint32_t *a) {
    return am_resolve_file_common(a[0], a[1], a[2], a[3], "ResolveAliasFile");
}

// OSErr ResolveAliasFileWithMountFlags(FSSpec *, Boolean, Boolean *, Boolean *, unsigned long);
// OSErr ResolveAliasFileWithMountFlagsNoUI(FSSpec *, Boolean, Boolean *, Boolean *, unsigned long);
// We never prompt and never mount, so both are the same function with one ignored argument.
int shim_ResolveAliasFileWithMountFlags(uint32_t *a) {
    return am_resolve_file_common(a[0], a[1], a[2], a[3], "ResolveAliasFileWithMountFlags");
}
int shim_ResolveAliasFileWithMountFlagsNoUI(uint32_t *a) {
    return am_resolve_file_common(a[0], a[1], a[2], a[3], "ResolveAliasFileWithMountFlagsNoUI");
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
static int am_match_common(uint32_t hdl, uint32_t countp, uint32_t listp, uint32_t needsp,
                           int fsref_list, const char *what) {
    int16_t *countv = (int16_t *)(uintptr_t)countp;
    uint8_t *list = (uint8_t *)(uintptr_t)listp;
    uint8_t *needsUpdate = (uint8_t *)(uintptr_t)needsp;
    const uint8_t *rec;
    uint32_t len = 0;
    char path[2048];
    int moved = 0;
    int16_t maxOut;

    maxOut = countv ? *countv : 0;                     // IN: capacity, read before clearing
    if (countv) { *countv = 0; }                       // ★OUT: zeroed before any failure
    if (needsUpdate) { *needsUpdate = 0; }

    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!hdl || !countv || !list) { return AM_PARAM_ERR; }
    if (maxOut <= 0) { return AM_PARAM_ERR; }
    rec = am_bytes_from_handle(hdl, &len);
    if (!rec || len < AR_TAGS_OFF) { return AM_PARAM_ERR; }
    if (!am_path_from_record(rec, len, path, sizeof path, &moved)) {
        am_trace(what, "", AM_FNF_ERR);
        return AM_FNF_ERR;
    }

    if (fsref_list) {
        if (!am_native_ready() || !p_FSPathMakeRef) { return AM_NSV_ERR; }
        memset(list, 0, CFS_FSREF_SIZE);
        if (p_FSPathMakeRef((const uint8_t *)path, list, NULL) != 0) { return AM_FNF_ERR; }
    } else {
        if (!cfs_path_to_fsspec(path, list)) { return AM_NSV_ERR; }
    }
    *countv = 1;
    if (needsUpdate) { *needsUpdate = (uint8_t)(moved != 0); }
    am_trace(what, path, AM_NO_ERR);
    return AM_NO_ERR;
}

int shim_MatchAlias(uint32_t *a) {
    return am_match_common(a[2], a[3], a[4], a[5], 0, "MatchAlias");
}
// OSErr MatchAliasNoUI(...) — identical arguments; no path here ever shows UI.
int shim_MatchAliasNoUI(uint32_t *a) {
    return am_match_common(a[2], a[3], a[4], a[5], 0, "MatchAliasNoUI");
}
// OSErr FSMatchAliasNoUI(const FSRef *fromFile, unsigned long rulesMask, AliasHandle inAlias,
//                        short *aliasCount, FSRef *aliasList, Boolean *needsUpdate,
//                        AliasFilterUPP aliasFilter, void *yourDataPtr);
// The FSRef-array spelling. It is DEAD on x86_64 even though the rest of the FSRef
// generation survives (Apple kept only FSMatchAliasBulk), which is why it is here.
int shim_FSMatchAliasNoUI(uint32_t *a) {
    return am_match_common(a[2], a[3], a[4], a[5], 1, "FSMatchAliasNoUI");
}
