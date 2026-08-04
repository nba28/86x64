/* 99_carbon_fileops — does the classic File Manager WRITE path actually write?
 *
 * WHY THIS EXISTS. FSpCreate / FSpDirCreate / DirCreate / HDelete / FSpDelete /
 * FSpRename / HRename / CatMove / FSpCatMove / FSpExchangeFiles / FSpSetFInfo /
 * FlushVol are the calls a classic Carbon app uses to SAVE. They were NULL-JUMP
 * ORPHANS: the legacy shim pass reads the 10.6 SDK's *i386* headers and emits a
 * pass-through bridge `___FSpCreate` that calls a native `_FSpCreate` which has
 * never existed on x86_64. Checked against the REAL Mojave 10.14 x86_64
 * CarbonCore — the last macOS that could run 32-bit code, so the most generous
 * original available — only 2 of our 114 NULL-jump orphans are present there,
 * and none of these. Nothing in the process supplies them, so unlike the
 * QuickTime family this cannot be fixed by staying out of the path.
 *
 * ★EVERY ASSERTION IS MADE THROUGH THE CLASSIC API ITSELF, never through POSIX.
 * FSMakeFSSpec returns noErr exactly when the target exists and fnfErr when it
 * does not, so it is an independent witness: "rename worked" is proved by the
 * NEW name existing and the OLD one not, as seen by a different call. A test
 * that stat()ed the path would be asserting on our own resolver twice.
 *
 * The whole run happens inside a directory this test creates under the system
 * temporary folder and removes again, so it leaves nothing behind.
 *
 * ARMS: ON must create/rename/move/delete for real. carbon_fileops_test.sh adds
 * the OFF side via ABICONV_FSSPEC_LEGACY=1, where every call must decline with a
 * File Manager error and NOTHING may be created — the pre-fix behaviour, minus
 * the jump to zero.
 */
extern int  printf(const char *, ...);
extern void exit(int);

typedef short          OSErr;
typedef short          SInt16;
typedef int            SInt32;
typedef unsigned int   OSType;
typedef unsigned char  Boolean;
typedef unsigned char  Str63[64];

#pragma pack(push, 2)
struct FSSpec { SInt16 vRefNum; SInt32 parID; Str63 name; };
struct FInfo  { OSType fdType, fdCreator; unsigned short fdFlags;
                short fdLocV, fdLocH, fdFldr; };
#pragma pack(pop)

extern OSErr FindFolder(SInt16, OSType, Boolean, SInt16 *, SInt32 *);
extern OSErr FSMakeFSSpec(SInt16, SInt32, const unsigned char *, struct FSSpec *);
extern OSErr FSpCreate(const struct FSSpec *, OSType, OSType, SInt16);
extern OSErr FSpDelete(const struct FSSpec *);
extern OSErr FSpRename(const struct FSSpec *, const unsigned char *);
extern OSErr FSpCatMove(const struct FSSpec *, const struct FSSpec *);
extern OSErr FSpSetFInfo(const struct FSSpec *, const struct FInfo *);
extern OSErr DirCreate(SInt16, SInt32, const unsigned char *, SInt32 *);
extern OSErr HDelete(SInt16, SInt32, const unsigned char *);
extern OSErr FlushVol(const unsigned char *, SInt16);

#define kOnSystemDisk        (-32768)
#define kTemporaryFolderType 0x74656D70  /* 'temp' */
#define dupFNErr             (-48)
#define noErr                0

/* Pascal strings, as every classic File Manager call takes them. */
static const unsigned char P_DIR[]  = "\014m64fmtestdir";
static const unsigned char P_SUB[]  = "\014m64fmtestsub";
static const unsigned char P_A[]    = "\005a.dat";
static const unsigned char P_B[]    = "\005b.dat";

int main(void)
{
   SInt16 vref = 0;
   SInt32 tmpID = 0, dirID = 0, subID = 0;
   struct FSSpec sa, sb, sdir;
   struct FInfo fi;
   OSErr e;

   /* The PRODUCER of the (vRefNum, dirID) pair everything else consumes. */
   e = FindFolder(kOnSystemDisk, kTemporaryFolderType, 1, &vref, &tmpID);
   printf("survived=1\n");                 /* reaching here = no jump to 0 */
   printf("findfolder=%d\n", e == noErr && tmpID != 0);
   if (e != noErr) { printf("done=1\n"); exit(0); }

   /* --- create a directory, and prove the returned dirID is usable --------- */
   HDelete(vref, tmpID, P_DIR);            /* clean any earlier aborted run */
   e = DirCreate(vref, tmpID, P_DIR, &dirID);
   printf("dircreate=%d\n", e == noErr && dirID != 0);

   /* Creating it twice must report dupFNErr, not a generic failure: classic
    * callers branch on that specific code to mean "already there". */
   { SInt32 again = 0;
     printf("dircreate_dup=%d\n", DirCreate(vref, tmpID, P_DIR, &again) == dupFNErr); }

   /* --- create a file inside it ------------------------------------------- */
   FSMakeFSSpec(vref, dirID, P_A, &sa);    /* fnfErr expected: it does not exist yet */
   e = FSpCreate(&sa, 0x54455354 /*'TEST'*/, 0x54455854 /*'TEXT'*/, 0);
   printf("fspcreate=%d\n", e == noErr);
   /* Independent witness: the same spec must now RESOLVE. */
   printf("fspcreate_seen=%d\n", FSMakeFSSpec(vref, dirID, P_A, &sa) == noErr);
   printf("fspcreate_dup=%d\n",
          FSpCreate(&sa, 0x54455354, 0x54455854, 0) == dupFNErr);

   /* --- Finder info round-trips through the same file --------------------- */
   fi.fdType = 0x54455854; fi.fdCreator = 0x54455354; fi.fdFlags = 0;
   fi.fdLocV = 0; fi.fdLocH = 0; fi.fdFldr = 0;
   printf("setfinfo=%d\n", FSpSetFInfo(&sa, &fi) == noErr);

   /* --- rename in place ---------------------------------------------------- */
   e = FSpRename(&sa, P_B);
   printf("fsprename=%d\n", e == noErr);
   printf("fsprename_new=%d\n", FSMakeFSSpec(vref, dirID, P_B, &sb) == noErr);
   printf("fsprename_old=%d\n", FSMakeFSSpec(vref, dirID, P_A, &sa) != noErr);

   /* --- move to another directory ----------------------------------------- */
   e = DirCreate(vref, dirID, P_SUB, &subID);
   printf("subdir=%d\n", e == noErr && subID != 0);
   FSMakeFSSpec(vref, dirID, P_SUB, &sdir);
   e = FSpCatMove(&sb, &sdir);
   printf("fspcatmove=%d\n", e == noErr);
   printf("fspcatmove_new=%d\n", FSMakeFSSpec(vref, subID, P_B, &sb) == noErr);
   printf("fspcatmove_old=%d\n", FSMakeFSSpec(vref, dirID, P_B, &sdir) != noErr);

   printf("flushvol=%d\n", FlushVol(0, vref) == noErr);

   /* --- delete, and prove it is gone --------------------------------------- */
   printf("fspdelete=%d\n", FSpDelete(&sb) == noErr);
   printf("fspdelete_gone=%d\n", FSMakeFSSpec(vref, subID, P_B, &sb) != noErr);

   /* HDelete removes an empty directory too — the classic call covers both. */
   printf("hdelete_sub=%d\n", HDelete(vref, dirID, P_SUB) == noErr);
   printf("hdelete_dir=%d\n", HDelete(vref, tmpID, P_DIR) == noErr);
   printf("hdelete_gone=%d\n", FSMakeFSSpec(vref, tmpID, P_DIR, &sdir) != noErr);

   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
