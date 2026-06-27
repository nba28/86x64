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
