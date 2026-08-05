/* 99_fscatinfo_bulk — does the classic BULK directory enumeration fill the whole
 * ARRAY, or only its first element?
 *
 * WHY THIS EXISTS. FSCatalogInfo is one of the very few classic structs whose
 * two layouts genuinely differ: FSPermissionInfo ends with an
 * FSFileSecurityRef POINTER, so the struct is 144 bytes on i386 and 148 on
 * x86_64. abigen sees that, correctly refuses the raw pass-through, and routes
 * `FSCatalogInfo *` to typeconv's deep-copy path — which stages the pointee in a
 * scratch slot of the shim's own frame sized for exactly ONE struct.
 *
 * That is right for FSGetCatalogInfo and CATASTROPHIC for FSGetCatalogInfoBulk,
 * which fills `actualObjects` (up to `maximumObjects`) entries: the native
 * callee writes actual*148 bytes into a one-element buffer, overruns the shim's
 * 880-byte frame, and scribbles up through rbp into the i386 CALLER's locals.
 * Measured on Civilization IV 2026-08-05: SIGSEGV at Civ.dylib+0x136c6e with the
 * caller's two just-null-checked `operator new[]` pointers reading back 0.
 *
 * ★THE DISCRIMINATOR IS THE ARRAY, NOT THE CRASH. A crash depends on stack
 * geometry and would make a flaky guard. Instead this asserts the property the
 * one-element bounce structurally cannot satisfy: every entry beyond index 0
 * must come back filled, with the size of the file whose name came back in the
 * parallel (layout-identical, therefore always-correct) HFSUniStr255 array.
 * A single stack canary is carried alongside as a cheap frame-smash witness.
 *
 * The files are created with POSIX and read back through the classic API, so
 * the witness is independent of the code under test.
 *
 * ARMS: ON must enumerate all six files with their true sizes. OFF
 * (M64_NO_FSCATINFO=1) must decline with every out-param DEFINED — zeroed
 * arrays, *actualObjects == 0, an error return — and must NOT enumerate.
 */
extern int   printf(const char *, ...);
extern void  exit(int);
extern int   mkdir(const char *, unsigned short);
extern int   open(const char *, int, ...);
extern long  write(int, const void *, unsigned long);
extern int   close(int);
extern int   unlink(const char *);
extern int   rmdir(const char *);
extern void *memset(void *, int, unsigned long);
extern int   snprintf(char *, unsigned long, const char *, ...);

typedef short           OSErr;
typedef unsigned char   Boolean;

/* The i386 images of the classic File Manager records. All of these live inside
 * CarbonCore's `#pragma pack(push, 2)`, which is what keeps UTCDateTime at 8
 * bytes (its natural size would be 12) — get that wrong and every field from
 * contentModDate onwards lands at the wrong offset. */
#pragma pack(push, 2)
typedef struct { unsigned short highSeconds; unsigned int lowSeconds;
                 unsigned short fraction; } UTCDateTime;              /* 8 */
typedef struct { unsigned int userID, groupID;
                 unsigned char reserved1, userAccess;
                 unsigned short mode;
                 unsigned int fileSec; } FSPermissionInfo;            /* 16 on i386 */
typedef struct {
   unsigned short   nodeFlags;
   short            volume;
   unsigned int     parentDirID;
   unsigned int     nodeID;
   unsigned char    sharingFlags, userPrivileges, reserved1, reserved2;
   UTCDateTime      createDate, contentModDate, attributeModDate,
                    accessDate, backupDate;
   FSPermissionInfo permissions;
   unsigned char    finderInfo[16];
   unsigned char    extFinderInfo[16];
   unsigned long long dataLogicalSize, dataPhysicalSize,
                      rsrcLogicalSize, rsrcPhysicalSize;
   unsigned int     valence;
   unsigned int     textEncodingHint;
} FSCatalogInfo;                                                      /* 144 on i386 */
typedef struct { unsigned char hidden[80]; } FSRef;                   /* 80 */
typedef struct { unsigned short length; unsigned short unicode[255]; } HFSUniStr255; /* 512 */
#pragma pack(pop)

typedef struct OpaqueFSIterator *FSIterator;

extern OSErr FSPathMakeRef(const unsigned char *path, FSRef *ref, Boolean *isDir);
extern OSErr FSOpenIterator(const FSRef *container, unsigned long flags, FSIterator *iter);
extern OSErr FSCloseIterator(FSIterator iter);
extern OSErr FSGetCatalogInfoBulk(FSIterator iter, unsigned long maximumObjects,
                                  unsigned long *actualObjects,
                                  Boolean *containerChanged,
                                  unsigned int whichInfo,
                                  FSCatalogInfo *catalogInfos, FSRef *refs,
                                  void *specs, HFSUniStr255 *names);

#define kFSIterateFlat          0
#define kFSCatInfoNodeFlags     0x00000002
#define kFSCatInfoNodeID        0x00000010
#define kFSCatInfoCreateDate    0x00000020
#define kFSCatInfoDataSizes     0x00004000
#define errFSNoMoreItems        (-1417)
#define noErr                   0

#define NFILES  6
#define MAXOBJ  16
#define CANARY  0xC0DEFACEu

static const char *DIRPATH = "/tmp/m64_fscatinfo_bulk";
/* distinct, non-monotonic sizes: a bounce that filled only entry 0 and left the
 * rest zero could not accidentally satisfy these */
static const unsigned expect_size[NFILES] = { 11, 233, 47, 1024, 5, 613 };

/* One flat stack frame holding the two arrays with canaries either side. The
 * overrun the fix removes runs UP the stack from the shim's frame, so `hi`
 * is the one that mattered; `lo` is carried for symmetry. */
struct frame {
   volatile unsigned lo;
   FSCatalogInfo     info[MAXOBJ];
   HFSUniStr255      names[MAXOBJ];
   volatile unsigned hi;
};

int main(void)
{
   struct frame f;
   FSRef dirRef;
   FSIterator iter = 0;
   unsigned long actual = 0;
   Boolean isDir = 0;
   char path[256];
   unsigned char buf[1024];
   int i, err, found = 0, sized = 0, beyond = 0, ided = 0;

   printf("sizeof_i386_catinfo=%d\n", (int)sizeof(FSCatalogInfo));
   printf("sizeof_ok=%d\n", sizeof(FSCatalogInfo) == 144 &&
                            sizeof(HFSUniStr255) == 512 &&
                            sizeof(FSRef) == 80);

   /* ---- fixture: a fresh directory with six files of known, distinct sizes -- */
   for (i = 0; i < NFILES; i++) {
      snprintf(path, sizeof path, "%s/f%d.dat", DIRPATH, i);
      unlink(path);
   }
   rmdir(DIRPATH);
   if (mkdir(DIRPATH, 0755) != 0) { printf("fixture=0\ndone=1\n"); exit(0); }
   memset(buf, 'x', sizeof buf);
   for (i = 0; i < NFILES; i++) {
      int fd;
      snprintf(path, sizeof path, "%s/f%d.dat", DIRPATH, i);
      fd = open(path, 0x0601 /* O_WRONLY|O_CREAT|O_TRUNC */, 0644);
      if (fd < 0) { printf("fixture=0\ndone=1\n"); exit(0); }
      write(fd, buf, expect_size[i]);
      close(fd);
   }
   printf("fixture=1\n");

   /* ---- the classic enumeration ------------------------------------------ */
   f.lo = CANARY; f.hi = CANARY;
   memset(f.info,  0, sizeof f.info);
   memset(f.names, 0, sizeof f.names);

   err = FSPathMakeRef((const unsigned char *)DIRPATH, &dirRef, &isDir);
   printf("pathref=%d\n", err == noErr);

   err = FSOpenIterator(&dirRef, kFSIterateFlat, &iter);
   printf("iter_ok=%d\n", err == noErr && iter != 0);

   if (err == noErr && iter) {
      err = FSGetCatalogInfoBulk(iter, MAXOBJ, &actual, 0,
                                 kFSCatInfoNodeFlags | kFSCatInfoNodeID |
                                 kFSCatInfoCreateDate | kFSCatInfoDataSizes,
                                 f.info, 0, 0, f.names);
      printf("bulk_err_ok=%d\n", err == noErr || err == errFSNoMoreItems);
   }
   printf("survived=1\n");
   printf("canary_ok=%d\n", f.lo == CANARY && f.hi == CANARY);
   printf("actual=%lu\n", actual);
   printf("count_ok=%d\n", actual == NFILES);

   /* Match every returned entry by NAME (HFSUniStr255 is layout-identical on
    * both ABIs, so it is a trustworthy witness) and check its size. */
   if (actual <= MAXOBJ) {
      for (i = 0; i < (int)actual; i++) {
         int k, n = f.names[i].length, idx = -1;   /* "fN.dat" == 6 UniChars */
         if (n == 6 && f.names[i].unicode[0] == 'f' &&
             f.names[i].unicode[1] >= '0' && f.names[i].unicode[1] <= '5')
            idx = f.names[i].unicode[1] - '0';
         if (idx < 0) continue;
         found++;
         if (f.info[i].dataLogicalSize == expect_size[idx]) sized++;
         if (f.info[i].nodeID != 0) ided++;
         if (i > 0 && f.info[i].dataLogicalSize == expect_size[idx]) beyond++;
         (void)k;
      }
   }
   printf("found=%d\n", found);
   printf("names_ok=%d\n", found == NFILES);
   /* every entry carries its OWN file's size */
   printf("all_sized=%d\n", sized == NFILES);
   /* the array property the one-element bounce cannot satisfy */
   printf("beyond_first=%d\n", beyond == NFILES - 1);
   printf("nodeids_ok=%d\n", ided == NFILES);

   if (iter) FSCloseIterator(iter);

   for (i = 0; i < NFILES; i++) {
      snprintf(path, sizeof path, "%s/f%d.dat", DIRPATH, i);
      unlink(path);
   }
   rmdir(DIRPATH);
   printf("done=1\n");
   exit(0);
   return 0;
}
