// fmgr_shim.c — graceful shims for the dead classic (FSSpec/refNum) File Manager API.
//
// The classic File Manager generation (FSpOpenDF/FSRead/FSWrite/GetEOF/SetFPos and the
// FSSpec {vRefNum,dirID,name} + parameter-block calls) was removed from 64-bit/modern macOS;
// only the FSRef generation survives (and abigen already shims those from the live header).
// The FSSpec volume model has no meaning on a modern volume, so these cannot be made to open
// real files.
//
// ★These carry real DATA semantics, so — per the project's no-silent-corruption rule
// (the BlockMoveData lesson) — they return a graceful File Manager ERROR and zero any
// byte-count, NEVER a fake noErr with an unfilled buffer (which would feed the caller
// garbage). Callers take their normal "file not found / I/O failed" fallback.
// HGetVol/HSetVol report a benign default volume (they are commonly called early just to
// learn the working volume; an error there can derail path setup before the game even runs).
//
// ⚠FOLLOW-UP: if Civ IV turns out to load assets through this classic path (vs POSIX/Python),
// FSpOpenDF/FSRead/FSWrite would need real POSIX-backed implementations keyed on a synthetic
// refNum->fd table. Deferred until a runtime wall proves it is exercised.
//
// MTSHIM convention: rdi -> &i386 args[0]; OSErr result in eax.

#include <stdint.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])

// Classic File Manager OSErr codes (Files.h / MacErrors.h), stable across all of Mac history.
#define FM_NO_ERR     (0)
#define FM_NSV_ERR   (-35)   // nsvErr   — no such volume
#define FM_IO_ERR    (-36)   // ioErr
#define FM_EOF_ERR   (-39)   // eofErr
#define FM_FNF_ERR   (-43)   // fnfErr   — file not found
#define FM_RFNUM_ERR (-51)   // rfNumErr — bad file reference number

// ---- open / delete / lock / info (FSSpec-addressed): no such file/volume ----
uint32_t shim_FSpOpenDF(uint32_t *args) {
    int16_t *refNum = (int16_t *)PTR(2); if (refNum) *refNum = 0;
    return (uint32_t)FM_FNF_ERR;
}
uint32_t shim_FSpDelete(uint32_t *args)   { (void)args; return (uint32_t)FM_FNF_ERR; }
uint32_t shim_FSpGetFInfo(uint32_t *args) { (void)args; return (uint32_t)FM_FNF_ERR; }
uint32_t shim_FSpRstFLock(uint32_t *args) { (void)args; return (uint32_t)FM_FNF_ERR; }
uint32_t shim_FSpSetFLock(uint32_t *args) { (void)args; return (uint32_t)FM_FNF_ERR; }

// ---- read / write / position (refNum-addressed): there is no valid open refNum ----
uint32_t shim_FSClose(uint32_t *args) { (void)args; return FM_NO_ERR; }   // close: nothing to do
uint32_t shim_FSRead(uint32_t *args) {
    int32_t *count = (int32_t *)PTR(1); if (count) *count = 0;   // 0 bytes transferred
    return (uint32_t)FM_EOF_ERR;
}
uint32_t shim_FSWrite(uint32_t *args) {
    int32_t *count = (int32_t *)PTR(1); if (count) *count = 0;
    return (uint32_t)FM_IO_ERR;
}
uint32_t shim_GetEOF(uint32_t *args) {
    int32_t *logEOF = (int32_t *)PTR(1); if (logEOF) *logEOF = 0;
    return (uint32_t)FM_RFNUM_ERR;
}
uint32_t shim_SetEOF(uint32_t *args)  { (void)args; return (uint32_t)FM_RFNUM_ERR; }
uint32_t shim_SetFPos(uint32_t *args) { (void)args; return (uint32_t)FM_RFNUM_ERR; }

// ---- parameter-block calls: report no volume / no file ----
uint32_t shim_PBHGetVInfoSync(uint32_t *args) { (void)args; return (uint32_t)FM_NSV_ERR; }
uint32_t shim_PBMakeFSRefSync(uint32_t *args) { (void)args; return (uint32_t)FM_FNF_ERR; }

// ---- classic HParamBlockRec / CInfoPBRec parameter-block calls (Halo NULL-bind set) ----
//
// All PB structs in the 10.6 SDK Files.h sit under `#pragma pack(push, 2)` (68k
// heritage), so the i386 field offsets are the classic Inside-Macintosh ones.
// HIOParam (the variant PBHGetVolParmsSync uses):
//   +0 qLink(4) +4 qType(2) +6 ioTrap(2) +8 ioCmdAddr(4) +12 ioCompletion(4)
//   +16 ioResult(OSErr,2) +18 ioNamePtr(4) +22 ioVRefNum(2) +24 ioRefNum(2)
//   +26 ioVersNum(1) +27 ioPermssn(1) +28 ioMisc(4)
//   +32 ioBuffer(4) +36 ioReqCount(4) +40 ioActCount(4)
// Every PB variant (HFileInfo/DirInfo/copy/access) shares the header through
// ioVRefNum, so ioResult is ALWAYS at +16 — set it as well as returning the
// OSErr in eax (classic callers poll pb.ioResult after sync calls).
#define FM_PARAM_ERR (-50)   // paramErr — "call not supported by this volume"
#define PB_IORESULT(pb)  (*(int16_t *)((uint8_t *)(pb) + 16))

// PBHGetVolParmsSync — the LIVE Halo blocker (audio/volume startup probes the
// boot volume). Smart fill, not a stub: a modern APFS/HFS+ boot volume IS a
// plain LOCAL volume, so report exactly that. GetVolParmsInfoBuffer (pack 2):
//   +0 vMVersion(2) +2 vMAttrib(4) +6 vMLocalHand(4) +10 vMServerAdr(4)
//   +14 vMVolumeGrade(4) +18 vMForeignPrivID(2) +20 vMExtendedAttributes(4)
// vMServerAdr==0 is THE documented "local volume" discriminator (callers of
// this era check it before treating a volume as AppleShare). vMAttrib=0 =
// no optional volume features (no CopyFile/DesktopMgr/...): truthful for the
// dead classic feature set, and steers callers onto their plain-file paths.
// Only ioReqCount bytes are returned (documented short-read behavior).
uint32_t shim_PBHGetVolParmsSync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    if (!pb) return (uint32_t)FM_PARAM_ERR;
    uint8_t *buf    = *(uint32_t *)(pb + 32) ? (uint8_t *)(uintptr_t)(*(uint32_t *)(pb + 32)) : 0;
    int32_t  reqcnt = *(int32_t *)(pb + 36);
    if (!buf || reqcnt <= 0) {
        PB_IORESULT(pb) = (int16_t)FM_PARAM_ERR;
        return (uint32_t)FM_PARAM_ERR;
    }
    uint8_t info[24] = {0};                       // version-3 image, all zero
    info[0] = 3; info[1] = 0;                     // vMVersion = 3 (little-endian SInt16)
    int32_t n = reqcnt < (int32_t)sizeof(info) ? reqcnt : (int32_t)sizeof(info);
    for (int32_t i = 0; i < n; ++i) buf[i] = info[i];
    *(int32_t *)(pb + 40) = n;                    // ioActCount
    PB_IORESULT(pb) = 0;
    return FM_NO_ERR;
}

// PBGetCatInfoSync — the classic catalog-info workhorse (CInfoPBRec: union of
// HFileInfo/DirInfo, addressed by vRefNum/dirID/name). A REAL implementation
// needs the classic volume model (vRefNum/dirID -> path registry) that this
// runtime does not maintain (the FSSpec shims above return fnfErr for the same
// reason). Per the no-silent-corruption rule: report file-not-found, never a
// fake noErr over an unfilled record. ⚠FOLLOW-UP: if a runtime wall proves a
// target loads assets through this path, back it with a synthetic dirID->fd
// table + stat() (same follow-up as FSpOpenDF above).
uint32_t shim_PBGetCatInfoSync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    if (!pb) return (uint32_t)FM_PARAM_ERR;
    PB_IORESULT(pb) = (int16_t)FM_FNF_ERR;
    return (uint32_t)FM_FNF_ERR;
}

// PBHCopyFileSync / PBHGetDirAccessSync — AFP-server-only operations (bulk
// server-side copy; directory access privileges). The DOCUMENTED response of a
// plain local volume (bHasCopyFile=0 in our vMAttrib, no privilege model) is
// paramErr: callers of this era then fall back to manual copy / skip ACLs.
uint32_t shim_PBHCopyFileSync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    if (pb) PB_IORESULT(pb) = (int16_t)FM_PARAM_ERR;
    return (uint32_t)FM_PARAM_ERR;
}
uint32_t shim_PBHGetDirAccessSync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    if (pb) PB_IORESULT(pb) = (int16_t)FM_PARAM_ERR;
    return (uint32_t)FM_PARAM_ERR;
}

// ---- default-volume queries: report a benign default (vRefNum -1, dirID 2 = root) ----
uint32_t shim_HGetVol(uint32_t *args) {
    uint8_t *volName  = (uint8_t *)PTR(0);
    int16_t *vRefNum  = (int16_t *)PTR(1);
    int32_t *dirID    = (int32_t *)PTR(2);
    if (volName) volName[0] = 0;     // empty Pascal string
    if (vRefNum) *vRefNum = -1;      // default working volume
    if (dirID)   *dirID = 2;         // fsRtDirID — root directory
    return FM_NO_ERR;
}
uint32_t shim_HSetVol(uint32_t *args) { (void)args; return FM_NO_ERR; }
