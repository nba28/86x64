#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "CoreServices", s); }

long DTInstall(long a, long b, long c_, long d, long e, long f) { shim_note("DTInstall called (auto-stub)"); return 0; }

long FSMakeFSSpec(long a, long b, long c_, long d, long e, long f) { shim_note("FSMakeFSSpec called (auto-stub)"); return 0; }

long FSpMakeFSRef(long a, long b, long c_, long d, long e, long f) { shim_note("FSpMakeFSRef called (auto-stub)"); return 0; }

long FSpOpenResFile(long a, long b, long c_, long d, long e, long f) { shim_note("FSpOpenResFile called (auto-stub)"); return 0; }

long FlushVol(long a, long b, long c_, long d, long e, long f) { shim_note("FlushVol called (auto-stub)"); return 0; }

long GetIconRefFromFile(long a, long b, long c_, long d, long e, long f) { shim_note("GetIconRefFromFile called (auto-stub)"); return 0; }

long HDelete(long a, long b, long c_, long d, long e, long f) { shim_note("HDelete called (auto-stub)"); return 0; }

long HRstFLock(long a, long b, long c_, long d, long e, long f) { shim_note("HRstFLock called (auto-stub)"); return 0; }

long LMGetCurApName(long a, long b, long c_, long d, long e, long f) { shim_note("LMGetCurApName called (auto-stub)"); return 0; }

long MatchAlias(long a, long b, long c_, long d, long e, long f) { shim_note("MatchAlias called (auto-stub)"); return 0; }

long NewAliasMinimal(long a, long b, long c_, long d, long e, long f) { shim_note("NewAliasMinimal called (auto-stub)"); return 0; }

long PBDTGetIconSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBDTGetIconSync called (auto-stub)"); return 0; }

long PBDTGetPath(long a, long b, long c_, long d, long e, long f) { shim_note("PBDTGetPath called (auto-stub)"); return 0; }

long PBFlushFileSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBFlushFileSync called (auto-stub)"); return 0; }

long PBFlushVolAsync(long a, long b, long c_, long d, long e, long f) { shim_note("PBFlushVolAsync called (auto-stub)"); return 0; }

long PBGetCatInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBGetCatInfoSync called (auto-stub)"); return 0; }

long PBHGetDirAccessSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBHGetDirAccessSync called (auto-stub)"); return 0; }

long PBHGetVInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBHGetVInfoSync called (auto-stub)"); return 0; }

long PBHGetVolParmsSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBHGetVolParmsSync called (auto-stub)"); return 0; }

long PBHSetDirAccessSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBHSetDirAccessSync called (auto-stub)"); return 0; }

long PBSetCatInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBSetCatInfoSync called (auto-stub)"); return 0; }

long PBXGetVolInfoAsync(long a, long b, long c_, long d, long e, long f) { shim_note("PBXGetVolInfoAsync called (auto-stub)"); return 0; }

long PBXGetVolInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("PBXGetVolInfoSync called (auto-stub)"); return 0; }

long RegisterIconRefFromResource(long a, long b, long c_, long d, long e, long f) { shim_note("RegisterIconRefFromResource called (auto-stub)"); return 0; }

long TempDisposeHandle(long a, long b, long c_, long d, long e, long f) { shim_note("TempDisposeHandle called (auto-stub)"); return 0; }

long UnmountVol(long a, long b, long c_, long d, long e, long f) { shim_note("UnmountVol called (auto-stub)"); return 0; }
