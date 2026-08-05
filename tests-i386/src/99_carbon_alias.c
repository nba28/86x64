/* 99_carbon_alias — does a classic alias actually behave like an ALIAS?
 *
 * WHY THIS EXISTS. NewAlias / NewAliasMinimal / NewAliasMinimalFromFullPath /
 * ResolveAlias / ResolveAliasFile / MatchAlias / FSMatchAliasNoUI / QTNewAlias
 * were NULL-JUMP ORPHANS: the legacy shim pass reads the 10.6 SDK's *i386*
 * headers and emits a pass-through bridge `___NewAlias` that calls a native
 * `_NewAlias`, and Apple deleted the FSSpec generation of the Alias Manager from
 * 64-bit Carbon in 2009. The bridge could only ever `call 0`. iPhoto, iWeb and
 * Pages resolve aliases constantly, so this was a jump to zero on a hot path.
 *
 * ★EVERY ASSERTION IS MADE THROUGH THE CLASSIC API ITSELF, never through POSIX.
 * FSMakeFSSpec returns noErr exactly when the target exists and fnfErr when it
 * does not, so it is an independent witness; a test that stat()ed the answer
 * would be asserting on our own resolver twice.
 *
 * ★★AND THE CENTRAL ASSERTION IS THE ONE THAT DISTINGUISHES A REAL ALIAS FROM A
 * STORED PATH STRING: create a file, make an alias to it, RENAME the file
 * through the classic File Manager, then resolve the alias and check the spec
 * that comes back names the NEW file. A path string cannot do that. (Measured
 * separately: the surviving native FSResolveAlias cannot either — the CNID
 * search is gone from modern CarbonCore — which is exactly why a minted record
 * carries a CFURL bookmark appendix after its -1 terminator.)
 *
 * The whole run happens inside a directory this test creates under the system
 * temporary folder and removes again, so it leaves nothing behind.
 *
 * ARMS: ON must mint and resolve for real. carbon_alias_test.sh adds the OFF side
 * via ABICONV_ALIAS_LEGACY=1, where every call must decline with a File Manager
 * error, leave its out-params DEFINED, and resolve nothing — the pre-fix
 * behaviour, minus the jump to zero.
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
#pragma pack(pop)

/* AliasHandle is a classic Handle: a 4-byte master pointer on i386. */
typedef void **AliasHandle;
struct FSRefX { unsigned char hidden[80]; };

extern OSErr FindFolder(SInt16, OSType, Boolean, SInt16 *, SInt32 *);
extern OSErr FSMakeFSSpec(SInt16, SInt32, const unsigned char *, struct FSSpec *);
extern OSErr FSpCreate(const struct FSSpec *, OSType, OSType, SInt16);
extern OSErr FSpDelete(const struct FSSpec *);
extern OSErr FSpRename(const struct FSSpec *, const unsigned char *);
extern OSErr FSpCatMove(const struct FSSpec *, const struct FSSpec *);
extern OSErr DirCreate(SInt16, SInt32, const unsigned char *, SInt32 *);
extern OSErr HDelete(SInt16, SInt32, const unsigned char *);

extern OSErr NewAlias(const struct FSSpec *, const struct FSSpec *, AliasHandle *);
extern OSErr NewAliasMinimal(const struct FSSpec *, AliasHandle *);
extern OSErr NewAliasMinimalFromFullPath(short, const void *, const unsigned char *,
                                         const unsigned char *, AliasHandle *);
extern OSErr ResolveAlias(const struct FSSpec *, AliasHandle, struct FSSpec *, Boolean *);
extern OSErr ResolveAliasFile(struct FSSpec *, Boolean, Boolean *, Boolean *);
extern OSErr MatchAlias(const struct FSSpec *, unsigned long, AliasHandle, short *,
                        struct FSSpec *, Boolean *, void *, void *);
extern OSErr FSMatchAliasNoUI(const void *, unsigned long, AliasHandle, short *,
                              struct FSRefX *, Boolean *, void *, void *);
extern OSErr QTNewAlias(const struct FSSpec *, AliasHandle *, Boolean);

/* The classic Memory Manager, which owns an AliasHandle. Proving GetHandleSize
 * works on it is proving it really is a Handle and not a bare malloc block. */
extern long GetHandleSize(AliasHandle);
extern void DisposeHandle(AliasHandle);

#define kOnSystemDisk        (-32768)
#define kTemporaryFolderType 0x74656D70  /* 'temp' */
#define noErr                0

static const unsigned char P_DIR[]  = "\013m64altestdir";
static const unsigned char P_A[]    = "\005a.dat";
static const unsigned char P_B[]    = "\005b.dat";
static const unsigned char P_SUB[]  = "\013m64altestsub";

/* Compare two FSSpecs the way a classic caller would: same parent, same name. */
static int same_spec(const struct FSSpec *x, const struct FSSpec *y)
{
   int i, n = x->name[0];
   if (x->parID != y->parID || n != y->name[0]) { return 0; }
   for (i = 1; i <= n; i++) { if (x->name[i] != y->name[i]) { return 0; } }
   return 1;
}

int main(void)
{
   SInt16 vref = 0;
   SInt32 tmpID = 0, dirID = 0, subID = 0;
   struct FSSpec sa, sb, sdir, got, sub;
   AliasHandle al = 0, almin = 0, alqt = 0, alfp = 0;
   Boolean changed = 9, folder = 9, aliased = 9, needs = 9;
   short count;
   struct FSRefX refs[4];
   OSErr e;

   e = FindFolder(kOnSystemDisk, kTemporaryFolderType, 1, &vref, &tmpID);
   printf("survived=1\n");                 /* reaching here = no jump to 0 */
   printf("findfolder=%d\n", e == noErr && tmpID != 0);
   if (e != noErr) { printf("done=1\n"); exit(0); }

   /* ---- a private scratch directory with one file in it ------------------- */
   HDelete(vref, tmpID, P_DIR);            /* clean any earlier aborted run */
   DirCreate(vref, tmpID, P_DIR, &dirID);
   FSMakeFSSpec(vref, dirID, P_A, &sa);
   printf("setup=%d\n", dirID != 0 && FSpCreate(&sa, 0x54455354, 0x54455854, 0) == noErr);

   /* ---- 1. mint an alias --------------------------------------------------
    * The handle must be a real classic Handle: GetHandleSize is an independent
    * witness (it only answers for a block carbon_memory.c allocated), and the
    * record must be at least the 150-byte version-2 alias header. */
   e = NewAlias(0, &sa, &al);
   printf("newalias=%d\n", e == noErr && al != 0);
   printf("newalias_ishandle=%d\n", al != 0 && GetHandleSize(al) >= 150);

   /* ---- 2. resolve it where it stands ------------------------------------- */
   changed = 9;
   e = ResolveAlias(0, al, &got, &changed);
   printf("resolve=%d\n", e == noErr);
   printf("resolve_same=%d\n", e == noErr && same_spec(&got, &sa));
   printf("resolve_wasChanged_defined=%d\n", changed == 0 || changed == 1);

   /* ---- 3. ★THE ALIAS ASSERTION: RENAME the target, then resolve ----------
    * A stored path string cannot survive this. The alias must come back naming
    * b.dat, and FSMakeFSSpec must agree that b.dat is what exists. */
   printf("rename=%d\n", FSpRename(&sa, P_B) == noErr);
   printf("rename_seen=%d\n", FSMakeFSSpec(vref, dirID, P_B, &sb) == noErr);
   printf("rename_old_gone=%d\n", FSMakeFSSpec(vref, dirID, P_A, &sa) != noErr);
   changed = 9;
   e = ResolveAlias(0, al, &got, &changed);
   printf("resolve_after_rename=%d\n", e == noErr);
   printf("FOLLOWED_RENAME=%d\n", e == noErr && same_spec(&got, &sb));
   printf("wasChanged=%d\n", changed == 1);

   /* ---- 4. ★and a MOVE to a different directory --------------------------- */
   DirCreate(vref, dirID, P_SUB, &subID);
   FSMakeFSSpec(vref, dirID, P_SUB, &sdir);
   printf("move=%d\n", subID != 0 && FSpCatMove(&sb, &sdir) == noErr);
   FSMakeFSSpec(vref, subID, P_B, &sub);
   e = ResolveAlias(0, al, &got, &changed);
   printf("FOLLOWED_MOVE=%d\n", e == noErr && same_spec(&got, &sub));

   /* ---- 5. MatchAlias returns the target and a count ----------------------
    * aliasCount is IN/OUT: the input value is the capacity of aliasList. */
   count = 4; needs = 9;
   e = MatchAlias(0, 0, al, &count, &got, &needs, 0, 0);
   printf("match=%d\n", e == noErr && count == 1 && same_spec(&got, &sub));
   printf("match_needsUpdate_defined=%d\n", needs == 0 || needs == 1);

   count = 4;
   e = FSMatchAliasNoUI(0, 0, al, &count, refs, &needs, 0, 0);
   printf("fsmatch=%d\n", e == noErr && count == 1);

   /* ---- 6. the other three producers all mint a resolvable record ---------- */
   e = NewAliasMinimal(&sub, &almin);
   printf("newaliasminimal=%d\n", e == noErr && almin != 0 &&
          ResolveAlias(0, almin, &got, &changed) == noErr && same_spec(&got, &sub));

   e = QTNewAlias(&sub, &alqt, 1);
   printf("qtnewalias=%d\n", e == noErr && alqt != 0 &&
          ResolveAlias(0, alqt, &got, &changed) == noErr && same_spec(&got, &sub));

   /* NewAliasMinimalFromFullPath takes raw path bytes and a length, not a
    * C string. A POSIX path is accepted alongside the classic colon form. */
   {  const char *p = "/private/tmp";
      short n = 12;
      e = NewAliasMinimalFromFullPath(n, p, 0, 0, &alfp);
      printf("fromfullpath=%d\n", e == noErr && alfp != 0 &&
             ResolveAlias(0, alfp, &got, &changed) == noErr);
   }

   /* ---- 7. ResolveAliasFile on a PLAIN file -------------------------------
    * Classic contract: noErr, wasAliased false, the spec left alone. Both
    * out-params must be DEFINED whatever happens. */
   got = sub; folder = 9; aliased = 9;
   e = ResolveAliasFile(&got, 1, &folder, &aliased);
   printf("rafile_plain=%d\n", e == noErr && aliased == 0 && same_spec(&got, &sub));
   printf("rafile_outparams_defined=%d\n",
          (folder == 0 || folder == 1) && (aliased == 0 || aliased == 1));

   /* ---- 8. ResolveAliasFile on a DIRECTORY: targetIsFolder must be true ---- */
   got = sdir; folder = 9; aliased = 9;
   e = ResolveAliasFile(&got, 1, &folder, &aliased);
   printf("rafile_folder=%d\n", e == noErr && folder == 1);

   /* ---- 9. bad input must still leave every out-param defined -------------- */
   changed = 9;
   e = ResolveAlias(0, 0, &got, &changed);
   printf("badinput_defined=%d\n", e != noErr && (changed == 0 || changed == 1));
   count = 4; needs = 9;
   e = MatchAlias(0, 0, 0, &count, &got, &needs, 0, 0);
   printf("badinput_count_zero=%d\n", e != noErr && count == 0);

   /* ---- clean up ----------------------------------------------------------- */
   if (al)    { DisposeHandle(al); }
   if (almin) { DisposeHandle(almin); }
   if (alqt)  { DisposeHandle(alqt); }
   if (alfp)  { DisposeHandle(alfp); }
   FSpDelete(&sub);
   HDelete(vref, dirID, P_SUB);
   HDelete(vref, tmpID, P_DIR);
   printf("cleanup=%d\n", FSMakeFSSpec(vref, tmpID, P_DIR, &sdir) != noErr);

   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
