/* 99_classic_fileio — the classic refNum File Manager on real files.
 *
 * Call of Duty 4 opens every Bink cinematic like this: FSPathMakeRef +
 * FSGetCatalogInfo(..., &fsSpec) -> BinkMacOpen(&fsSpec) -> FSpOpenDF, then
 * streams with SetFPos/GetFPos/FSRead/PBReadAsync. Two walls: FSpOpenDF was a
 * graceful-fnfErr stub (no refNum ever existed), and the FSSpec FSGetCatalogInfo
 * returns carried modern CarbonCore's token parID, which no FSSpec consumer can
 * resolve.
 *
 * ON exits 42. OFF: M64_NO_CLASSIC_FILEIO=1 or M64_NO_FSCAT_REAL_DIRID=1 (run
 * time) each break the chain -> exit 1..9 (the step that failed).
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern int  open(const char *, int, ...);
extern int  write(int, const void *, unsigned long);
extern int  close(int);
extern int  unlink(const char *);
extern int  getpid(void);
extern int  snprintf(char *, unsigned long, const char *, ...);
extern int  memcmp(const void *, const void *, unsigned long);
extern void *memset(void *, int, unsigned long);

typedef short OSErr;
OSErr FSPathMakeRef(const unsigned char *path, void *ref, unsigned char *isDir);
OSErr FSGetCatalogInfo(const void *ref, unsigned int which, void *catInfo,
                       void *outName, void *fsSpec, void *parentRef);
OSErr FSpOpenDF(const void *spec, signed char perm, short *refNum);
OSErr FSRead(short refNum, int *count, void *buf);
OSErr GetEOF(short refNum, int *logEOF);
OSErr SetFPos(short refNum, short posMode, int posOff);
OSErr GetFPos(short refNum, int *filePos);
OSErr PBReadAsync(void *pb);
OSErr FSClose(short refNum);

int main(void) {
   char path[128], buf[16];
   unsigned char ref[80], spec[70], pb[50];
   short rn = 0;
   int n, step = 0;
   snprintf(path, sizeof path, "/tmp/86x64-fileio.%d", getpid());
   int fd = open(path, 0x200 /*O_CREAT*/ | 0x400 /*O_TRUNC*/ | 1, 0644);
   write(fd, "0123456789", 10);
   close(fd);

   if (FSPathMakeRef((const unsigned char *)path, ref, 0) ||
       FSGetCatalogInfo(ref, 0, 0, 0, spec, 0)) { step = 1; goto out; }
   if (FSpOpenDF(spec, 1 /*fsRdPerm*/, &rn) || !rn) { step = 2; goto out; }
   if (GetEOF(rn, &n) || n != 10) { step = 3; goto out; }
   if (SetFPos(rn, 1 /*fsFromStart*/, 3) || GetFPos(rn, &n) || n != 3) { step = 4; goto out; }
   n = 4; memset(buf, 0, sizeof buf);
   if (FSRead(rn, &n, buf) || n != 4 || memcmp(buf, "3456", 4)) { step = 5; goto out; }
   if (SetFPos(rn, 1, 20) != -39 /*eofErr*/ || GetFPos(rn, &n) || n != 10) { step = 6; goto out; }

   /* Bink's async read: posMode fsFromStart, a request that runs past the EOF. */
   memset(pb, 0, sizeof pb); memset(buf, 0, sizeof buf);
   *(short *)(pb + 24) = rn;                 /* ioRefNum    */
   *(int *)(pb + 32) = (int)buf;             /* ioBuffer    */
   *(int *)(pb + 36) = 5;                    /* ioReqCount  */
   *(short *)(pb + 44) = 1;                  /* ioPosMode   */
   *(int *)(pb + 46) = 8;                    /* ioPosOffset */
   if (PBReadAsync(pb) != -39) { step = 7; goto out; }
   if (*(short *)(pb + 16) != -39 || *(int *)(pb + 40) != 2 || memcmp(buf, "89", 2)) { step = 8; goto out; }
   if (FSClose(rn) || FSRead(rn, &n, buf) != -51 /*rfNumErr*/) { step = 9; goto out; }
   rn = 0;
   step = 42;
out:
   printf("step=%d refNum=%d\n", step, rn);
   if (rn) FSClose(rn);
   unlink(path);
   exit(step);
}
