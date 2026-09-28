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
// WHERE IT LIVES. The record engine and the native entry points are shimdb/impl/alias.c
// (linked hidden; native frameworks get it through shimgen). This file is the i386 glue:
// argument slots, the 4-byte i386 AliasHandle, and the kill switches.
//
// UNIVERSAL: this triggers on a structural property (a dead FSSpec-generation Alias Manager
// entry point), never on an app name. Every translated Carbon app that resolves an alias
// benefits.
//
// Kill switch: ABICONV_ALIAS_LEGACY=1 makes every entry point decline with a File Manager
// error, out-params defined, nothing resolved and nothing minted — the pre-fix behaviour
// minus the jump to zero. That is the OFF arm of tests-i386 `make carbon-alias`.
//
// Wired through carbon_alias_tramp.asm (MTSHIM: rdi -> &i386 args[0], result in eax).

#include "carbon_shim.h"
#include "alias.h"
#include "fsspec.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#define AM_NO_ERR      0
#define AM_PARAM_ERR (-50)
#define AM_NSV_ERR   (-35)
#define AM_FNF_ERR   (-43)
#define P(n) ((void *)(uintptr_t)a[(n)])

static int alias_legacy(void) {
    static int c = -1;
    if (c < 0) { const char *e = getenv("ABICONV_ALIAS_LEGACY"); c = (e && *e && *e != '0'); }
    return c;
}
// ★A SECOND, NARROWER kill switch. ABICONV_ALIAS_LEGACY=1 turns the whole Alias Manager off,
// which proves the family is implemented but says nothing about WHICH half makes an alias
// behave like an alias. ABICONV_ALIAS_NO_BOOKMARK=1 keeps the bridge onto Apple's surviving
// engine and omits only the bookmark appendix, so the guard can show that resolving IN PLACE
// still works while following a RENAME stops working.
static int alias_no_bookmark(void) {
    static int c = -1;
    if (c < 0) { const char *e = getenv("ABICONV_ALIAS_NO_BOOKMARK"); c = (e && *e && *e != '0'); }
    return c;
}

// ── i386 Handle plumbing ────────────────────────────────────────────────────────────────
// An AliasHandle IS a classic Handle, so it comes from carbon_memory.c: DisposeHandle,
// GetHandleSize and SetHandleSize all work on it and the 4-byte i386 representation holds.
static uint32_t am_handle_from_bytes(const uint8_t *b, uint32_t n) {
    uint32_t h = cm_new_handle(n, 0);
    if (h) { memcpy(cm_handle_block(h), b, n); }
    return h;
}

// Read an i386 AliasHandle back: one we minted, or one the app built itself (NewHandle +
// bytes out of its own save file). cm_handle_size knows the logical size of either.
static const uint8_t *am_bytes_from_handle(uint32_t hdl, uint32_t *len) {
    void *blk;
    *len = 0;
    if (!hdl) { return NULL; }
    blk = cm_handle_block(hdl);
    if (!blk) { return NULL; }
    *len = cm_handle_size(hdl);
    return (const uint8_t *)blk;
}

// Mint a record for an existing path into the caller's i386 AliasHandle* slot.
static int am_publish(const char *path, int minimal, uint32_t *slot) {
    uint8_t *buf = NULL;
    uint32_t blen = 0;
    int e = am_record_from_path(path, minimal, !alias_no_bookmark(), &buf, &blen);
    if (e != AM_NO_ERR) { return e; }
    *slot = am_handle_from_bytes(buf, blen);
    free(buf);
    return *slot ? AM_NO_ERR : AM_NSV_ERR;
}

// `slot` is zeroed BEFORE anything can fail.
static int am_new_from_spec(uint32_t specp, uint32_t slotp, int minimal) {
    uint32_t *slot = (uint32_t *)(uintptr_t)slotp;
    const uint8_t *spec = (const uint8_t *)(uintptr_t)specp;
    char path[2048];
    int exists = 0;

    if (slot) { *slot = 0; }
    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!spec || !slot) { return AM_PARAM_ERR; }
    if (!cfs_fsspec_to_path(spec, path, sizeof path, &exists)) { return AM_NSV_ERR; }
    if (!exists) { return AM_FNF_ERR; }
    return am_publish(path, minimal, slot);
}

// OSErr NewAlias(const FSSpec *fromFile, const FSSpec *target, AliasHandle *alias);
int shim_NewAlias(uint32_t *a) { return am_new_from_spec(a[1], a[2], 0); }
// OSErr NewAliasMinimal(const FSSpec *target, AliasHandle *alias);
int shim_NewAliasMinimal(uint32_t *a) { return am_new_from_spec(a[0], a[1], 1); }
// OSErr QTNewAlias(const FSSpec *fss, AliasHandle *alias, Boolean minimal);
// QuickTime's spelling of the same job; minting a record is this module's job, not QuickTime's.
int shim_QTNewAlias(uint32_t *a) { return am_new_from_spec(a[0], a[1], a[2] != 0); }

// OSErr NewAliasMinimalFromFullPath(short fullPathLength, const void *fullPath,
//                                   ConstStr32Param zoneName, ConstStr31Param serverName,
//                                   AliasHandle *alias);
int shim_NewAliasMinimalFromFullPath(uint32_t *a) {
    uint32_t *slot = P(4);
    char path[1024];
    int e;

    if (slot) { *slot = 0; }
    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!slot) { return AM_PARAM_ERR; }
    e = am_path_from_fullpath(P(1), (int16_t)(uint16_t)a[0], path, sizeof path);
    return e != AM_NO_ERR ? e : am_publish(path, 1, slot);
}

// OSErr ResolveAlias(const FSSpec *fromFile, AliasHandle alias, FSSpec *target,
//                    Boolean *wasChanged);  (…WithMountFlags adds an ignored flags word)
static int am_resolve(uint32_t hdl, uint8_t *target, uint8_t *changed) {
    uint32_t len = 0;
    const uint8_t *rec;
    if (changed) { *changed = 0; }
    if (target) { memset(target, 0, CFS_FSSPEC_SIZE); }
    if (alias_legacy()) { return AM_FNF_ERR; }
    if (!hdl || !target) { return AM_PARAM_ERR; }
    rec = am_bytes_from_handle(hdl, &len);
    return am_resolve_bytes(rec, len, target, changed);
}
int shim_ResolveAlias(uint32_t *a) { return am_resolve(a[1], P(2), P(3)); }
int shim_ResolveAliasWithMountFlags(uint32_t *a) { return am_resolve(a[1], P(2), P(3)); }

// OSErr ResolveAliasFile(FSSpec *theSpec, Boolean resolveAliasChains,
//                        Boolean *targetIsFolder, Boolean *wasAliased);
// (…WithMountFlags[NoUI] add an ignored flags word: we never prompt and never mount.)
static int am_resolve_file(uint32_t *a) {
    uint8_t *folder = P(2), *aliased = P(3);
    if (alias_legacy()) {
        if (folder) { *folder = 0; }
        if (aliased) { *aliased = 0; }
        return AM_FNF_ERR;
    }
    return ResolveAliasFile(P(0), (uint8_t)(a[1] != 0), folder, aliased);
}
int shim_ResolveAliasFile(uint32_t *a) { return am_resolve_file(a); }
int shim_ResolveAliasFileWithMountFlags(uint32_t *a) { return am_resolve_file(a); }
int shim_ResolveAliasFileWithMountFlagsNoUI(uint32_t *a) { return am_resolve_file(a); }

// OSErr MatchAlias(const FSSpec *fromFile, unsigned long rulesMask, AliasHandle alias,
//                  short *aliasCount, FSSpecArrayPtr aliasList, Boolean *needsUpdate,
//                  AliasFilterUPP aliasFilter, void *yourDataPtr);
// FSMatchAliasNoUI is the FSRef-array spelling. `aliasCount` is IN/OUT (capacity in, count
// out) and is zeroed before every failure return, the legacy arm included.
static int am_match(uint32_t *a, int fsref_list) {
    int16_t *countv = P(3);
    uint8_t *needs = P(5);
    uint32_t len = 0;
    const uint8_t *rec;
    if (alias_legacy()) {
        if (countv) { *countv = 0; }
        if (needs) { *needs = 0; }
        return AM_FNF_ERR;
    }
    rec = a[2] ? am_bytes_from_handle(a[2], &len) : NULL;
    return am_match_bytes(rec, len, countv, P(4), needs, fsref_list);
}
int shim_MatchAlias(uint32_t *a) { return am_match(a, 0); }
int shim_MatchAliasNoUI(uint32_t *a) { return am_match(a, 0); }
int shim_FSMatchAliasNoUI(uint32_t *a) { return am_match(a, 1); }
