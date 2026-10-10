/* 99_fsspec_native_ready -- FSpMakeFSRef must resolve its natives itself.
 *
 * A spec answered from libabiconv's dirID table (the dirID came from
 * FSGetCatalogInfo, which only records it) never reaches fs_native_ready, so
 * when FSpMakeFSRef is the first FS call that needs natives p_FSPathMakeRef is
 * NULL -> call to address 0 (Call of Duty 4's loader thread).
 *
 * The spec is hand-built (no FSMakeFSSpec before it, which would resolve the
 * natives). ON exits 42; OFF (M64_NO_FSREF_NATIVE_READY=1) crashes.
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern int  mkdir(const char *, unsigned short);
extern int  rmdir(const char *);
extern int  open(const char *, int, ...);
extern int  close(int);
extern int  unlink(const char *);
extern int  getpid(void);
extern int  snprintf(char *, unsigned long, const char *, ...);
extern int  strcmp(const char *, const char *);
extern void *memset(void *, int, unsigned long);

typedef short OSErr;
OSErr FSPathMakeRef(const unsigned char *path, void *ref, unsigned char *isDir);
OSErr FSGetCatalogInfo(const void *ref, unsigned int which, void *catInfo,
                       void *outName, void *fsSpec, void *parentRef);
OSErr FSpMakeFSRef(const void *spec, void *ref);
OSErr FSRefMakePath(const void *ref, unsigned char *path, unsigned int max);

int main(void) {
   char dir[128], file[160], got[1024];
   unsigned char ref[80], ci[144], spec[70], ref2[80];
   int step = 0;
   snprintf(dir, sizeof dir, "/private/tmp/86x64-fsspec-ready.%d", getpid());
   snprintf(file, sizeof file, "%s/f.txt", dir);
   mkdir(dir, 0755); close(open(file, 0x200 | 1, 0644));

   if (FSPathMakeRef((const unsigned char *)file, ref, 0)) { step = 1; goto out; }
   if (FSGetCatalogInfo(ref, 0x4 | 0x8, ci, 0, 0, 0)) { step = 2; goto out; }
   memset(spec, 0, sizeof spec);
   *(short *)spec = *(short *)(ci + 2);          /* volume */
   *(int *)(spec + 2) = *(int *)(ci + 4);        /* parentDirID */
   spec[6] = 5; spec[7] = 'f'; spec[8] = '.'; spec[9] = 't'; spec[10] = 'x'; spec[11] = 't';
   got[0] = 0;
   if (FSpMakeFSRef(spec, ref2)) { step = 3; goto out; }
   if (FSRefMakePath(ref2, (unsigned char *)got, sizeof got)) { step = 4; goto out; }
   if (strcmp(got, file)) { step = 5; goto out; }
   step = 42;
out:
   printf("step=%d got=%s\n", step, got);
   unlink(file); rmdir(dir);
   exit(step);
}
