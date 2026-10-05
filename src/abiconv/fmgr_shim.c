// fmgr_shim.c — the classic (FSSpec/refNum) File Manager on POSIX.
//
// The classic File Manager generation (FSpOpenDF/FSRead/FSWrite/GetEOF/SetFPos and the
// FSSpec {vRefNum,dirID,name} + parameter-block calls) was removed from 64-bit/modern macOS;
// only the FSRef generation survives (and abigen already shims those from the live header).
// Data-fork I/O is REAL: FSpOpenDF resolves the FSSpec through carbon_fsspec_shim.c and
// mints a refNum over a POSIX fd (see the refNum table below).
//
// ★Calls that still cannot be honoured carry real DATA semantics, so — per the project's
// no-silent-corruption rule (the BlockMoveData lesson) — they return a graceful File Manager
// ERROR and zero any byte-count, NEVER a fake noErr with an unfilled buffer.
// HGetVol/HSetVol report a benign default volume (they are commonly called early just to
// learn the working volume; an error there can derail path setup before the game even runs).
//
// MTSHIM convention: rdi -> &i386 args[0]; OSErr result in eax.

#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include "gap.h"
#include "carbon_fsspec.h"   /* cfs_fsspec_to_path, cfs_dirid_for_path */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#define PTR(n) ((void *)(uintptr_t)args[(n)])

// Classic File Manager OSErr codes (Files.h / MacErrors.h), stable across all of Mac history.
#define FM_NO_ERR     (0)
#define FM_NSV_ERR   (-35)   // nsvErr   — no such volume
#define FM_IO_ERR    (-36)   // ioErr
#define FM_EOF_ERR   (-39)   // eofErr
#define FM_FNF_ERR   (-43)   // fnfErr   — file not found
#define FM_RFNUM_ERR (-51)   // rfNumErr — bad file reference number
#define FM_PARAM_ERR (-50)   // paramErr — "call not supported by this volume"

// Every classic PB variant (HVolumeParam/HIOParam/HFileInfo/DirInfo/...)
// shares the pack(2) header through ioVRefNum, so ioResult is ALWAYS at +16 —
// set it as well as returning the OSErr in eax (classic callers poll
// pb.ioResult after sync calls).
#define PB_IORESULT(pb)  (*(int16_t *)((uint8_t *)(pb) + 16))

// ---- FSSpec-addressed info/lock: no such file ----
// shim_FSpDelete has MOVED to carbon_fsspec_shim.c, which grew a real FSSpec->POSIX
// resolver and can therefore actually delete the file. That file now owns the whole
// classic write path (create/delete/rename/move/exchange/set-Finder-info); the graceful
// errors left here are the ones that still cannot be honoured.
// ★Same output-buffer invariant as shim_FSpOpenDF below (and carbon_fsspec_shim.c): a
// classic caller that skips the OSErr check would otherwise read its own uninitialised
// stack back as an FInfo — a garbage fdType/fdCreator is exactly the class of defect that
// produced the Halo nil-CFStringRef SIGSEGV. FInfo is 16 bytes: OSType fdType, OSType
// fdCreator, UInt16 fdFlags, Point fdLocation, SInt16 fdFldr.
uint32_t shim_FSpGetFInfo(uint32_t *args) {
    void *fndrInfo = PTR(1); if (fndrInfo) { memset(fndrInfo, 0, 16); }
    return (uint32_t)FM_FNF_ERR;
}
uint32_t shim_FSpRstFLock(uint32_t *args) { (void)args; return (uint32_t)FM_FNF_ERR; }
uint32_t shim_FSpSetFLock(uint32_t *args) { (void)args; return (uint32_t)FM_FNF_ERR; }

// ---- open / read / write / position: REAL, on a synthetic refNum -> fd table ----
// Call of Duty 4's Bink (libBinkMachOx86) opens every cinematic with
// FSMakeFSSpec + FSpOpenDF and streams it with SetFPos/GetFPos/FSRead/PBReadAsync;
// the old graceful fnfErr meant no video could ever play. carbon_fsspec_shim.c
// already resolves an FSSpec to its POSIX path, so these are a thin fd table.
// Our refNums live at FM_REF_BASE.. so they can never collide with the native
// FSOpenResFile refNums FSClose still hands to CloseResFile. Kill
// M64_NO_CLASSIC_FILEIO (the old fnfErr/rfNumErr answers); guard classic-fileio.
#define FM_POS_ERR   (-40)   // posErr — position before start of file
#define FM_REF_BASE  0x4000
#define FM_REF_MAX   256
static int g_fm_fd[FM_REF_MAX];          // fd + 1 (0 = free slot)
static int fm_disabled(void) {
    static int v = -1;
    if (v < 0) v = getenv("M64_NO_CLASSIC_FILEIO") != NULL;
    return v;
}
static int fm_fd(int16_t ref) {
    int i = ref - FM_REF_BASE;
    return (i >= 0 && i < FM_REF_MAX) ? g_fm_fd[i] - 1 : -1;
}
static int16_t fm_alloc(int fd) {
    for (int i = 0; i < FM_REF_MAX; i++)
        if (__sync_bool_compare_and_swap(&g_fm_fd[i], 0, fd + 1)) return (int16_t)(FM_REF_BASE + i);
    return 0;
}

// OSErr FSpOpenDF(const FSSpec *spec, SInt8 permission, SInt16 *refNum)
uint32_t shim_FSpOpenDF(uint32_t *args) {
    int16_t *refNum = (int16_t *)PTR(2); if (refNum) *refNum = 0;
    if (fm_disabled() || !PTR(0)) return (uint32_t)FM_FNF_ERR;
    char path[1024]; int exists = 0;
    if (!cfs_fsspec_to_path((const uint8_t *)PTR(0), path, sizeof path, &exists) || !exists)
        return (uint32_t)FM_FNF_ERR;
    int8_t perm = (int8_t)args[1];       // fsCurPerm 0, fsRdPerm 1, else read/write
    int fd = open(path, perm == 1 ? O_RDONLY : O_RDWR);
    if (fd < 0 && perm == 0) fd = open(path, O_RDONLY);   // fsCurPerm: best available
    if (fd < 0) return (uint32_t)(errno == EACCES ? -54 /*permErr*/ : FM_IO_ERR);
    int16_t r = fm_alloc(fd);
    if (!r) { close(fd); return (uint32_t)-42; /* tmfoErr: too many files open */ }
    if (refNum) *refNum = r;
    return FM_NO_ERR;
}

// FSClose — our refNums close their fd. Every OTHER incoming refNum is a
// resource-file refNum: the only other producer of classic file refNums in this
// runtime is the still-native FSOpenResFile, and classic FSClose on a resource
// refNum closed the resource file (same refNum space). Halo: EULA.rsrc is
// FSOpenResFile'd for the license window and FSClose'd on dismissal; a no-op
// stacked it on the resource chain forever, SHADOWING later Get1Resource lookups.
// CloseResFile on an unknown refNum just sets ResError (-193) and is harmless.
uint32_t shim_FSClose(uint32_t *args) {
    int16_t ref = (int16_t)args[0];
    int fd = fm_fd(ref);
    if (fd >= 0) { close(fd); g_fm_fd[ref - FM_REF_BASE] = 0; return FM_NO_ERR; }
    static void (*CloseResFile)(int16_t);
    if (!CloseResFile)
        CloseResFile = (void (*)(int16_t))dlsym(RTLD_DEFAULT, "CloseResFile");
    if (CloseResFile) CloseResFile(ref);
    return FM_NO_ERR;
}

// Shared transfer: FSRead/FSWrite/PBRead*. A short read is eofErr (classic).
static int16_t fm_xfer(int16_t ref, void *buf, int32_t req, int32_t *act, int wr) {
    *act = 0;
    int fd = fm_fd(ref);
    if (fd < 0) return FM_RFNUM_ERR;
    if (req < 0) return FM_PARAM_ERR;
    ssize_t n = wr ? write(fd, buf, (size_t)req) : read(fd, buf, (size_t)req);
    if (n < 0) return FM_IO_ERR;
    *act = (int32_t)n;
    return (n < req) ? (wr ? -34 /*dskFulErr*/ : FM_EOF_ERR) : FM_NO_ERR;
}
// OSErr FSRead / FSWrite(SInt16 refNum, SInt32 *count, void *buffPtr)
uint32_t shim_FSRead(uint32_t *args) {
    int32_t *count = (int32_t *)PTR(1); int32_t act = 0;
    int16_t e = count ? fm_xfer((int16_t)args[0], PTR(2), *count, &act, 0) : FM_PARAM_ERR;
    if (count) *count = act;
    return (uint32_t)e;
}
uint32_t shim_FSWrite(uint32_t *args) {
    int32_t *count = (int32_t *)PTR(1); int32_t act = 0;
    int16_t e = count ? fm_xfer((int16_t)args[0], PTR(2), *count, &act, 1) : FM_PARAM_ERR;
    if (count) *count = act;
    return (uint32_t)e;
}
// OSErr GetEOF(SInt16 refNum, SInt32 *logEOF) / SetEOF(SInt16, SInt32)
uint32_t shim_GetEOF(uint32_t *args) {
    int32_t *logEOF = (int32_t *)PTR(1); if (logEOF) *logEOF = 0;
    struct stat st; int fd = fm_fd((int16_t)args[0]);
    if (fd < 0 || fstat(fd, &st) != 0) return (uint32_t)FM_RFNUM_ERR;
    if (logEOF) *logEOF = (int32_t)st.st_size;
    return FM_NO_ERR;
}
uint32_t shim_SetEOF(uint32_t *args) {
    int fd = fm_fd((int16_t)args[0]);
    if (fd < 0) return (uint32_t)FM_RFNUM_ERR;
    return ftruncate(fd, (off_t)(int32_t)args[1]) == 0 ? FM_NO_ERR : (uint32_t)FM_IO_ERR;
}
// Classic positioning: posMode low 2 bits = fsAtMark 0 / fsFromStart 1 /
// fsFromLEOF 2 / fsFromMark 3. Before the start is posErr; past the logical
// EOF the mark stops AT the EOF and the call reports eofErr.
static int16_t fm_seek(int fd, int16_t mode, int32_t off) {
    off_t cur = lseek(fd, 0, SEEK_CUR), end = lseek(fd, 0, SEEK_END), to;
    switch (mode & 3) {
    case 0:  to = cur; break;
    case 1:  to = off; break;
    case 2:  to = end + off; break;
    default: to = cur + off; break;
    }
    if (to < 0) { lseek(fd, cur, SEEK_SET); return FM_POS_ERR; }
    if (to > end) { lseek(fd, end, SEEK_SET); return FM_EOF_ERR; }
    lseek(fd, to, SEEK_SET);
    return FM_NO_ERR;
}
// OSErr SetFPos(SInt16 refNum, SInt16 posMode, SInt32 posOff)
uint32_t shim_SetFPos(uint32_t *args) {
    int fd = fm_fd((int16_t)args[0]);
    if (fd < 0) return (uint32_t)FM_RFNUM_ERR;
    return (uint32_t)fm_seek(fd, (int16_t)args[1], (int32_t)args[2]);
}
// OSErr GetFPos(SInt16 refNum, SInt32 *filePos)
uint32_t shim_GetFPos(uint32_t *args) {
    int32_t *pos = (int32_t *)PTR(1); if (pos) *pos = 0;
    int fd = fm_fd((int16_t)args[0]);
    if (fd < 0) return (uint32_t)FM_RFNUM_ERR;
    if (pos) *pos = (int32_t)lseek(fd, 0, SEEK_CUR);
    return FM_NO_ERR;
}

// OSErr PBReadAsync(ParmBlkPtr) — IOParam (i386, pack(2)): ioCompletion @12,
// ioResult @16, ioRefNum @24, ioBuffer @32, ioReqCount @36, ioActCount @40,
// ioPosMode @44, ioPosOffset @46. Served synchronously: the request is done
// (ioResult final) before we return, which every async poller accepts; the
// completion routine, if any, still runs once, as the Device Manager would.
#include "cb_bridge.h"
uint32_t shim_PBReadAsync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    if (!pb) return (uint32_t)FM_PARAM_ERR;
    int16_t ref = *(int16_t *)(pb + 24);
    int32_t act = 0;
    int fd = fm_fd(ref);
    int16_t e = fd < 0 ? FM_RFNUM_ERR
                       : fm_seek(fd, *(int16_t *)(pb + 44), *(int32_t *)(pb + 46));
    if (e == FM_NO_ERR)
        e = fm_xfer(ref, (void *)(uintptr_t)*(uint32_t *)(pb + 32), *(int32_t *)(pb + 36), &act, 0);
    *(int32_t *)(pb + 40) = act;
    if (fd >= 0) *(int32_t *)(pb + 46) = (int32_t)lseek(fd, 0, SEEK_CUR);
    PB_IORESULT(pb) = e;
    uint32_t done = *(uint32_t *)(pb + 12);
    if (done) {   // void (*IOCompletionProcPtr)(ParmBlkPtr)
        static const x64_cb_sig sig = { 1, CBR_VOID, { CBA_PTR }, { 0 } };
        void (*fn)(void *) = (void (*)(void *))(uintptr_t)x64_cb_wrap(done, &sig);
        if (fn) fn(pb);
    }
    return (uint32_t)e;
}

// ---- parameter-block calls ----

// PBHGetVInfoSync — REAL free/total-space report for the boot volume (the
// single-volume model this runtime maintains everywhere: HGetVol reports
// vRefNum -1 / root, PBHGetVolParmsSync reports a plain local volume).
// The old nsvErr no-op made every classic startup disk-space check FAIL —
// Halo computes free = ioVFrBlk(u16@+62) * ioVAlBlkSiz(u32@+48), needs
// > 0xF9FFFFF (250MB), and aborts with its "Free up some space on your local
// disk" alert when the call errors.
//
// HVolumeParam layout (classic pack(2); field offsets GROUND-TRUTHED against
// Halo's own compiled accesses — memset(pb,0,0x7A)=122-byte struct, writes
// ioVRefNum@+22 / ioVolIndex@+28, reads ioVAlBlkSiz@+48 / ioVFrBlk@+62 —
// which uniquely pin the Inside-Macintosh layout; the shared 22-byte header
// through ioVRefNum is documented at the HIOParam comment below):
//   +16 ioResult(2)   +18 ioNamePtr(4)   +22 ioVRefNum(2) +24 filler2(4)
//   +28 ioVolIndex(2) +30 ioVCrDate(4)   +34 ioVLsMod(4)  +38 ioVAtrb(2)
//   +40 ioVNmFls(2)   +42 ioVBitMap(2)   +44 ioAllocPtr(2)
//   +46 ioVNmAlBlks(2)+48 ioVAlBlkSiz(4) +52 ioVClpSiz(4) +56 ioAlBlSt(2)
//   +58 ioVNxtCNID(4) +62 ioVFrBlk(2)    +64 ioVSigWord(2)+66 ioVDrvInfo(2)
//   +68 ioVDRefNum(2) +70 ioVFSID(2)     +72 ioVBkUp(4)   +76 ioVSeqNum(2)
//   +78 ioVWrCnt(4)   +82 ioVFilCnt(4)   +86 ioVDirCnt(4) +90 ioVFndrInfo(32)
//
// The 16-bit block counts cannot represent a modern disk, so apply the
// DOCUMENTED classic pinning (real Mac OS did exactly this on >2GB volumes):
// pin the byte figures at 0x7FFFFFFF, then scale ioVAlBlkSiz up (power of
// two) until the total-block count fits in 15 bits — the reported
// count*blocksize products stay = min(real, ~2GB), honest and overflow-free
// even through a caller's 32-bit multiply.
#define MAC_EPOCH_DELTA 2082844800u   /* 1904-01-01 -> 1970-01-01 seconds */

static void pstr27(uint8_t *dst, const char *src) {
    uint32_t n = (uint32_t)strlen(src);
    if (n > 27) { n = 27; }                      /* classic Str27 volume name */
    dst[0] = (uint8_t)n;
    memcpy(dst + 1, src, n);
}

/* Boot-volume name via getattrlist(ATTR_VOL_NAME); fall back to the mount
 * point spelling if unavailable. */
static void boot_volume_name(char *out, size_t cap) {
    struct attrlist al;
    /* u_int32_t length + attrreference_t + the name bytes it points at */
    char abuf[sizeof(uint32_t) + sizeof(attrreference_t) + 256];
    memset(&al, 0, sizeof al);
    al.bitmapcount = ATTR_BIT_MAP_COUNT;
    al.volattr = ATTR_VOL_INFO | ATTR_VOL_NAME;
    out[0] = '\0';
    if (getattrlist("/", &al, abuf, sizeof abuf, 0) == 0) {
        attrreference_t *ref = (attrreference_t *)(abuf + sizeof(uint32_t));
        const char *nm = (const char *)ref + ref->attr_dataoffset;
        size_t n = ref->attr_length;             /* includes the NUL */
        if (n > 0 && n <= cap) {
            memcpy(out, nm, n);
            out[cap - 1] = '\0';
        }
    }
    if (!out[0]) { strlcpy(out, "Macintosh HD", cap); }
}

uint32_t shim_PBHGetVInfoSync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    if (!pb) { return (uint32_t)FM_PARAM_ERR; }
    int16_t volIndex = *(int16_t *)(pb + 28);
    if (volIndex > 1) {                     /* single-volume model: indexed */
        PB_IORESULT(pb) = (int16_t)FM_NSV_ERR;   /* enumeration ends after 1 */
        return (uint32_t)FM_NSV_ERR;
    }
    /* volIndex==1, ==0 (use ioVRefNum: 0=default, -1=our boot vol, anything
     * maps there) and <0 (use ioNamePtr) all resolve to the boot volume. */
    struct statfs sf;
    struct stat st;
    if (statfs("/", &sf) != 0) {
        PB_IORESULT(pb) = (int16_t)FM_NSV_ERR;
        return (uint32_t)FM_NSV_ERR;
    }
    uint64_t blksz  = sf.f_bsize ? (uint64_t)sf.f_bsize : 512u;
    uint64_t total  = (uint64_t)sf.f_blocks * blksz;
    uint64_t freeb  = (uint64_t)sf.f_bavail * blksz;
    if (total > 0x7FFFFFFFu) { total = 0x7FFFFFFFu; }     /* classic 2GB pin */
    if (freeb > 0x7FFFFFFFu) { freeb = 0x7FFFFFFFu; }
    while (total / blksz > 0x7FFFu) { blksz <<= 1; }      /* 15-bit counts */
    uint16_t nmblks = (uint16_t)(total / blksz);
    uint16_t frblks = (uint16_t)(freeb / blksz);

    uint32_t namep = *(uint32_t *)(pb + 18);   /* ioNamePtr: Pascal vol name */
    if (namep) {
        char vname[128];
        boot_volume_name(vname, sizeof vname);
        pstr27((uint8_t *)(uintptr_t)namep, vname);
    }
    if (volIndex > 0) {                      /* indexed: return the vRefNum */
        *(int16_t *)(pb + 22) = -1;          /* boot volume == HGetVol's -1 */
    }
    uint32_t crdate = 0, moddate = 0;
    if (stat("/", &st) == 0) {
        crdate  = (uint32_t)((uint64_t)st.st_birthtime + MAC_EPOCH_DELTA);
        moddate = (uint32_t)((uint64_t)st.st_mtime + MAC_EPOCH_DELTA);
    }
    uint64_t filcnt = (uint64_t)sf.f_files - (uint64_t)sf.f_ffree;
    if (filcnt > 0xFFFFFFFFu) { filcnt = 0xFFFFFFFFu; }

    *(uint32_t *)(pb + 30) = crdate;                  /* ioVCrDate  */
    *(uint32_t *)(pb + 34) = moddate;                 /* ioVLsMod   */
    *(int16_t  *)(pb + 38) = 0;                       /* ioVAtrb: unlocked */
    *(uint16_t *)(pb + 40) = 0;                       /* ioVNmFls   */
    *(uint16_t *)(pb + 42) = 0;                       /* ioVBitMap  */
    *(uint16_t *)(pb + 44) = 0;                       /* ioAllocPtr */
    *(uint16_t *)(pb + 46) = nmblks;                  /* ioVNmAlBlks */
    *(uint32_t *)(pb + 48) = (uint32_t)blksz;         /* ioVAlBlkSiz */
    *(uint32_t *)(pb + 52) = (uint32_t)blksz;         /* ioVClpSiz  */
    *(uint16_t *)(pb + 56) = 0;                       /* ioAlBlSt   */
    *(uint32_t *)(pb + 58) = 16;                      /* ioVNxtCNID */
    *(uint16_t *)(pb + 62) = frblks;                  /* ioVFrBlk   */
    *(int16_t  *)(pb + 64) = 0x4244;                  /* ioVSigWord: HFS 'BD' */
    *(int16_t  *)(pb + 66) = 1;                       /* ioVDrvInfo: drive 1 */
    *(int16_t  *)(pb + 68) = -1;                      /* ioVDRefNum */
    *(int16_t  *)(pb + 70) = 0;                       /* ioVFSID: local FM */
    *(uint32_t *)(pb + 72) = 0;                       /* ioVBkUp: never */
    *(int16_t  *)(pb + 76) = 0;                       /* ioVSeqNum  */
    *(uint32_t *)(pb + 78) = 0;                       /* ioVWrCnt   */
    *(uint32_t *)(pb + 82) = (uint32_t)filcnt;        /* ioVFilCnt  */
    *(uint32_t *)(pb + 86) = 0;                       /* ioVDirCnt  */
    memset(pb + 90, 0, 32);                           /* ioVFndrInfo */
    PB_IORESULT(pb) = 0;
    return FM_NO_ERR;
}

uint32_t shim_PBMakeFSRefSync(uint32_t *args) { (void)args; return (uint32_t)FM_FNF_ERR; }

// PBXGetVolInfoSync / PBXGetVolInfoAsync — private Apple extension to the HFS
// File Manager that returns 64-bit total/free byte counts plus the standard
// HVolumeParam fields in an XVolumeParam "union" variant.  Both symbols were
// REMOVED from modern macOS (dlsym returns NULL — verified), yet the abigen
// legacy pass-through shims still reference the dead native symbols via a
// flat-namespace lazy bind: the first call would trip a dyld missing-symbol
// failure or read garbage.
//
// Classic i386 apps (iPhoto's PhotoCDManager, Civ-era Carbon startup) call
// PBXGetVolInfoSync to learn a volume's free/total bytes.  This shim restores
// real functionality: it fills the shared HVolumeParam body via the existing
// shim_PBHGetVInfoSync and appends the real 64-bit statfs("/") byte counts, so
// any classic caller gets a truthful answer for the boot volume instead of a
// dead-stub failure.  UNIVERSAL (keyed on the API, not any app).
//
// XVolumeParam layout (pack 2, i386 — from the 10.6 SDK Files.h; it extends
// HVolumeParam through +90, the sole structural difference being that the old
// HVolumeParam filler2 (4B) at +24 is now the named ioXVersion, version = 0):
//     +16 ioResult(2)  +18 ioNamePtr(4)  +22 ioVRefNum(2)
//     +24 ioXVersion(4)  +28 ioVolIndex(2)
//     +30 ioVCrDate(4)   +34 ioVLsMod(4)   +38 ioVAtrb(2)
//     +40 ioVNmFls(2)    +46 ioVNmAlBlks(2) +48 ioVAlBlkSiz(4)
//     +56 ioAlBlSt(2)    +58 ioVNxtCNID(4)  +62 ioVFrBlk(2)
//     +64 ioVSigWord(2)  +66 ioVDrvInfo(2)  +68 ioVDRefNum(2) +70 ioVFSID(2)
//     +72 ioVBkUp(4)     +76 ioVSeqNum(2)   +78 ioVWrCnt(4)
//     +82 ioVFilCnt(4)   +86 ioVDirCnt(4)   +90 ioVFndrInfo[8] (32B)
//   XVolumeParam-specific additions:
//     +122 ioVTotalBytes (UInt64, 8B) — total bytes on volume
//     +130 ioVFreeBytes  (UInt64, 8B) — free bytes on volume
//
// IMPLEMENTATION: delegate to shim_PBHGetVInfoSync for the shared HVolumeParam
// body (+16..+90, identical layout), stamp ioXVersion = 0, then write the
// 64-bit real total/free byte counts from statfs("/").  Classic 68k PB "async"
// is completed inline on modern macOS (no async I/O manager); iPhoto only calls
// the sync variant in practice, but the async shim is wired for completeness.
uint32_t shim_PBXGetVolInfoSync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    if (!pb) return (uint32_t)FM_PARAM_ERR;

    /* Fill the HVolumeParam portion (+16..+90) via the shared helper.
     * shim_PBHGetVInfoSync reads ioVolIndex@+28 and handles naming/error
     * paths identically — XVolumeParam is layout-compatible there. */
    uint32_t err = shim_PBHGetVInfoSync(args);

    /* ioXVersion at +24 (old HVolumeParam filler2): version tag 0. */
    *(uint32_t *)(pb + 24) = 0;

    if (err == (uint32_t)FM_NO_ERR) {
        struct statfs sf;
        if (statfs("/", &sf) == 0) {
            uint64_t blksz = sf.f_bsize ? (uint64_t)sf.f_bsize : 512u;
            uint64_t total = (uint64_t)sf.f_blocks * blksz;
            uint64_t freeb = (uint64_t)sf.f_bavail * blksz;
            memcpy(pb + 122, &total, 8);   /* ioVTotalBytes */
            memcpy(pb + 130, &freeb, 8);   /* ioVFreeBytes  */
        } else {
            memset(pb + 122, 0, 16);       /* statfs failed: zero both fields */
        }
    }
    return err;
}

uint32_t shim_PBXGetVolInfoAsync(uint32_t *args) {
    /* Execute synchronously (classic PB async is always synchronous on modern
     * macOS — no async I/O manager exists).  We do NOT fire the ioCompletion
     * callback at +12: that's an i386 cdecl function pointer inside the
     * translated binary that would require the full reverse-bridge to invoke.
     * iPhoto only calls the sync variant; the async entry exists to satisfy
     * the import so the lazy stub doesn't reference the dead native symbol. */
    return shim_PBXGetVolInfoSync(args);
}

// ---- classic HParamBlockRec / CInfoPBRec parameter-block calls (Halo NULL-bind set) ----
//
// All PB structs in the 10.6 SDK Files.h sit under `#pragma pack(push, 2)` (68k
// heritage), so the i386 field offsets are the classic Inside-Macintosh ones.
// HIOParam (the variant PBHGetVolParmsSync uses):
//   +0 qLink(4) +4 qType(2) +6 ioTrap(2) +8 ioCmdAddr(4) +12 ioCompletion(4)
//   +16 ioResult(OSErr,2) +18 ioNamePtr(4) +22 ioVRefNum(2) +24 ioRefNum(2)
//   +26 ioVersNum(1) +27 ioPermssn(1) +28 ioMisc(4)
//   +32 ioBuffer(4) +36 ioReqCount(4) +40 ioActCount(4)
// (FM_PARAM_ERR / PB_IORESULT defined with the error codes at the top.)

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
// HFileInfo/DirInfo, addressed by vRefNum/dirID/name), resolved through the same
// (vRefNum, dirID) -> path mapping as the FSSpec calls (carbon_fsspec_shim.c), so a
// dirID one of them handed out round-trips. Call of Duty 4 finds its data folder with
// it: ioFDirIndex -1 on Contents/MacOS for ioDrParID, then the folder by name for
// ioFlAttrib's directory bit; the old always-fnfErr stub showed "could not locate the
// Call of Duty 4 Data folder". Indexed enumeration (ioFDirIndex > 0) is not reached by
// any target yet: loud gap. Kill M64_NO_PBGETCATINFO (the old fnfErr).
//
// i386 CInfoPBRec (pack 2, 108 bytes): ioResult 16, ioNamePtr 18, ioVRefNum 22,
// ioFDirIndex 28, ioFlAttrib 30, Finder info 32..47, ioDirID/ioDrDirID 48,
// ioFlLgLen 54 | ioDrNmFls 52, ioFlPyLen 58, ioFlCrDat/ioDrCrDat 72,
// ioFlMdDat/ioDrMdDat 76, ioFlParID/ioDrParID 100.
#define CIPB_SIZE   108
#define HFS_EPOCH   2082844800u     /* 1904-01-01 -> 1970-01-01, seconds */
static void put32(uint8_t *pb, int off, uint32_t v) { memcpy(pb + off, &v, 4); }
uint32_t shim_PBGetCatInfoSync(uint32_t *args) {
    uint8_t *pb = (uint8_t *)PTR(0);
    static int enabled = -1;
    if (!pb) return (uint32_t)FM_PARAM_ERR;
    if (enabled < 0) { const char *e = getenv("M64_NO_PBGETCATINFO"); enabled = !(e && *e && *e != '0'); }
    if (!enabled) { PB_IORESULT(pb) = (int16_t)FM_FNF_ERR; return (uint32_t)FM_FNF_ERR; }

    int16_t vref, index; int32_t dirID; uint32_t name32;
    memcpy(&vref, pb + 22, 2); memcpy(&index, pb + 28, 2);
    memcpy(&dirID, pb + 48, 4); memcpy(&name32, pb + 18, 4);
    uint8_t *name = (uint8_t *)(uintptr_t)name32;
    if (index > 0) {                      // the Nth entry of a directory
        GAP_STUB(args);
        PB_IORESULT(pb) = (int16_t)FM_FNF_ERR;
        return (uint32_t)FM_FNF_ERR;
    }

    uint8_t spec[70];
    char path[2048], real[1024];
    int exists = 0;
    struct stat st;
    memset(spec, 0, sizeof spec);
    memcpy(spec, &vref, 2); memcpy(spec + 2, &dirID, 4);
    if (index == 0 && name && name[0]) {
        uint8_t n = name[0] > 63 ? 63 : name[0];
        spec[6] = n; memcpy(spec + 7, name + 1, n);
    }
    // a /.vol/<dev>/<ino> path has no meaningful parent: canonicalize first
    int fd = -1;
    if (!cfs_fsspec_to_path(spec, path, sizeof path, &exists) || !exists ||
        (fd = open(path, O_RDONLY)) < 0 || fcntl(fd, F_GETPATH, real) < 0 || fstat(fd, &st) != 0) {
        if (fd >= 0) close(fd);
        PB_IORESULT(pb) = (int16_t)FM_FNF_ERR;
        return (uint32_t)FM_FNF_ERR;
    }
    close(fd);

    char parent[1024];
    snprintf(parent, sizeof parent, "%s", real);
    char *slash = strrchr(parent, '/');
    if (slash == parent) slash[1] = '\0'; else if (slash) *slash = '\0';
    const int isdir = S_ISDIR(st.st_mode);

    memset(pb + 30, 0, CIPB_SIZE - 30);
    if (index < 0 && name) {              // ioNamePtr is an OUTPUT here: the dir's own name
        const char *leaf = strrchr(real, '/');
        leaf = (leaf && leaf[1]) ? leaf + 1 : real;
        size_t n = strlen(leaf) > 31 ? 31 : strlen(leaf);
        name[0] = (uint8_t)n;
        for (size_t k = 0; k < n; k++) name[1 + k] = (uint8_t)(leaf[k] == ':' ? '/' : leaf[k]);
    }
    if (isdir) {
        pb[30] = 0x10;                    // ioFlAttrib: kioFlAttribDirMask
        put32(pb, 48, (uint32_t)cfs_dirid_for_path(vref, real));
        uint16_t nfiles = 0;
        DIR *d = opendir(real);
        if (d) {
            struct dirent *e;
            while ((e = readdir(d)) != NULL)
                if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..") && nfiles < 0xFFFF) nfiles++;
            closedir(d);
        }
        memcpy(pb + 52, &nfiles, 2);
    } else {
        put32(pb, 48, st.st_ino <= 0x7FFFFFFFULL ? (uint32_t)st.st_ino : 0);
        put32(pb, 54, (uint32_t)st.st_size);
        put32(pb, 58, (uint32_t)(st.st_blocks * 512));
    }
    put32(pb, 72, (uint32_t)(st.st_birthtimespec.tv_sec + HFS_EPOCH));
    put32(pb, 76, (uint32_t)(st.st_mtimespec.tv_sec + HFS_EPOCH));
    put32(pb, 100, (uint32_t)cfs_dirid_for_path(vref, parent));
    PB_IORESULT(pb) = (int16_t)FM_NO_ERR;
    return (uint32_t)FM_NO_ERR;
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
uint32_t shim_HSetVol(uint32_t *args) { GAP_STUB(args); return FM_NO_ERR; }
