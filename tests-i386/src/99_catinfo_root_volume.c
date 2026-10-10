/* 99_catinfo_root_volume — PBGetCatInfoSync on the volume root answers like
 * classic HFS: the root directory's name is the VOLUME name and its parent is
 * fsRtParID (1).
 *
 * Halo builds the full path of its folder by walking ioDrParID up to the root
 * and prepending each ioNamePtr. The root used to come back named "/" with
 * itself as parent, so the path read "/:Users:..." and Halo refused it
 * ("contains a slash, backslash, or period").
 *
 * ON exits 42. OFF: M64_NO_CATINFO_ROOT_VOL=1 (run time) -> 3 (root named
 * "/") or 4 (root parent not fsRtParID).
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
extern void *memset(void *, int, unsigned long);
extern void *memcpy(void *, const void *, unsigned long);

typedef short OSErr;
OSErr FSPathMakeRef(const unsigned char *path, void *ref, unsigned char *isDir);
OSErr FSGetCatalogInfo(const void *ref, unsigned int which, void *catInfo,
                       void *outName, void *fsSpec, void *parentRef);
OSErr PBGetCatInfoSync(void *pb);

#define kFSCatInfoVolume      0x4
#define kFSCatInfoParentDirID 0x8

int main(void) {
   char dir[128], file[160], path[1024];
   unsigned char ref[80], ci[144], pb[108], name[64];
   int step = 0, depth = 0, len = 0;
   snprintf(dir, sizeof dir, "/tmp/86x64-root-vol.%d", getpid());
   snprintf(file, sizeof file, "%s/f", dir);
   mkdir(dir, 0755); close(open(file, 0x200 /*O_CREAT*/ | 1, 0644));

   if (FSPathMakeRef((const unsigned char *)file, ref, 0) ||
       FSGetCatalogInfo(ref, kFSCatInfoVolume | kFSCatInfoParentDirID, ci, 0, 0, 0)) {
      step = 1; goto out;
   }
   short vol = *(short *)(ci + 2);
   int id = *(int *)(ci + 4);                       /* the test dir's dirID */
   path[0] = 0;
   for (;;) {                                       /* Halo's walk to the root */
      memset(pb, 0, sizeof pb);
      *(int *)(pb + 18) = (int)name;
      *(short *)(pb + 22) = vol; *(short *)(pb + 28) = -1; *(int *)(pb + 48) = id;
      if (PBGetCatInfoSync(pb) || ++depth > 32) { step = 2; goto out; }
      char tmp[1024];                               /* prepend "name:" */
      int n = name[0];
      memcpy(tmp, name + 1, n); tmp[n] = ':';
      memcpy(tmp + n + 1, path, len + 1);
      len += n + 1; memcpy(path, tmp, len + 1);
      int par = *(int *)(pb + 100);                 /* ioDrParID */
      if (id == 2 || par == 1 || par == id) break;  /* the root (fsRtDirID / fsRtParID) */
      id = par;
   }
   for (int i = 0; i < name[0]; i++)
      if (name[1 + i] == '/') { step = 3; goto out; }
   if (*(int *)(pb + 100) != 1) { step = 4; goto out; }
   step = 42;
out:
   printf("step=%d depth=%d path=%s\n", step, depth, path);
   unlink(file); rmdir(dir);
   exit(step);
}
