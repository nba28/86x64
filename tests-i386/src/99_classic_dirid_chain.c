/* 99_classic_dirid_chain — the classic (vRefNum, dirID) contract across producers
 * and consumers: FSGetCatalogInfo hands out dirIDs, PBGetCatInfoSync and
 * FSMakeFSSpec resolve them.
 *
 * Call of Duty 4 finds "Call of Duty 4 Data" beside its .app exactly like this:
 * FSGetCatalogInfo(parentDirID) of the app and of its executable, PBGetCatInfoSync
 * (ioFDirIndex -1) on Contents/MacOS for ioDrParID, FSMakeFSSpec(..., name) in
 * Contents and then beside the app, PBGetCatInfoSync by name for the directory bit.
 * Two walls: modern CarbonCore's parentDirID is a small sequential token our
 * FSMakeFSSpec (/.vol/<dev>/<dirID>) cannot resolve, and PBGetCatInfoSync was an
 * always-fnfErr stub. Either one -> "could not locate the data folder".
 *
 * ON exits 42. OFF: M64_NO_FSCAT_REAL_DIRID=1 or M64_NO_PBGETCATINFO=1 (run time)
 * each break the chain -> exit 1..6 (the step that failed).
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

typedef short OSErr;
OSErr FSPathMakeRef(const unsigned char *path, void *ref, unsigned char *isDir);
OSErr FSGetCatalogInfo(const void *ref, unsigned int which, void *catInfo,
                       void *outName, void *fsSpec, void *parentRef);
OSErr FSMakeFSSpec(short vRefNum, int dirID, const unsigned char *name, void *spec);
OSErr PBGetCatInfoSync(void *pb);

#define kFSCatInfoVolume      0x4
#define kFSCatInfoParentDirID 0x8

/* parentDirID (and volume) of a path, via the modern producer */
static int parent_of(const char *path, short *vol) {
   unsigned char ref[80], ci[144];
   if (FSPathMakeRef((const unsigned char *)path, ref, 0)) return 0;
   if (FSGetCatalogInfo(ref, kFSCatInfoVolume | kFSCatInfoParentDirID, ci, 0, 0, 0)) return 0;
   if (vol) *vol = *(short *)(ci + 2);   /* pack(2): nodeFlags, volume, parentDirID */
   return *(int *)(ci + 4);
}

int main(void) {
   char root[128], app[160], contents[176], macos[192], exe[208], data[160];
   unsigned char pb[108], spec[70];
   static const unsigned char name[] = "\x04" "Data";
   short vol = 0;
   int step = 0;
   snprintf(root, sizeof root, "/tmp/86x64-dirid-chain.%d", getpid());
   snprintf(app, sizeof app, "%s/App.app", root);
   snprintf(contents, sizeof contents, "%s/Contents", app);
   snprintf(macos, sizeof macos, "%s/MacOS", contents);
   snprintf(exe, sizeof exe, "%s/App", macos);
   snprintf(data, sizeof data, "%s/Data", root);
   mkdir(root, 0755); mkdir(app, 0755); mkdir(contents, 0755); mkdir(macos, 0755);
   mkdir(data, 0755); close(open(exe, 0x200 /*O_CREAT*/ | 1, 0644));

   int app_parent = parent_of(app, &vol);
   int exe_parent = parent_of(exe, 0);                       /* Contents/MacOS */
   if (!app_parent || !exe_parent) { step = 1; goto out; }

   memset(pb, 0, sizeof pb);                                 /* about MacOS itself */
   *(short *)(pb + 22) = vol; *(short *)(pb + 28) = -1; *(int *)(pb + 48) = exe_parent;
   if (PBGetCatInfoSync(pb) || !(pb[30] & 0x10)) { step = 2; goto out; }
   int contents_id = *(int *)(pb + 100);                     /* ioDrParID */

   if (FSMakeFSSpec(vol, contents_id, name, spec) == 0) { step = 3; goto out; }  /* not in Contents */
   if (FSMakeFSSpec(vol, app_parent, name, spec) != 0) { step = 4; goto out; }   /* beside the app */

   memset(pb, 0, sizeof pb);                                 /* the folder, by name */
   *(int *)(pb + 18) = (int)(spec + 6);
   *(short *)(pb + 22) = *(short *)spec; *(int *)(pb + 48) = *(int *)(spec + 2);
   if (PBGetCatInfoSync(pb)) { step = 5; goto out; }
   if (!(pb[30] & 0x10) || *(int *)(pb + 100) != app_parent) { step = 6; goto out; }
   step = 42;
out:
   printf("step=%d vol=%d app_parent=%d exe_parent=%d\n", step, vol, app_parent, exe_parent);
   unlink(exe); rmdir(data); rmdir(macos); rmdir(contents); rmdir(app); rmdir(root);
   exit(step);
}
