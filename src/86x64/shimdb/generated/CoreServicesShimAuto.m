#import <Cocoa/Cocoa.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

static void shim_note(const char *sym) {
  static const char *seen[2048]; static int n;
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mtx);
  for (int i = 0; i < n; i++)
    if (seen[i] == sym) { pthread_mutex_unlock(&mtx); return; }
  if (n < 2048) seen[n++] = sym;
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "CoreServices", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long Allocate(long a, long b, long c_, long d, long e, long f) __asm("_Allocate");
long Allocate(long a, long b, long c_, long d, long e, long f) { shim_note("_Allocate"); return 0; }

long ComponentFunctionImplemented(long a, long b, long c_, long d, long e, long f) __asm("_ComponentFunctionImplemented");
long ComponentFunctionImplemented(long a, long b, long c_, long d, long e, long f) { shim_note("_ComponentFunctionImplemented"); return 0; }

long CopyCStringToPascal(long a, long b, long c_, long d, long e, long f) __asm("_CopyCStringToPascal");
long CopyCStringToPascal(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyCStringToPascal"); return 0; }

long CopyPascalStringToC(long a, long b, long c_, long d, long e, long f) __asm("_CopyPascalStringToC");
long CopyPascalStringToC(long a, long b, long c_, long d, long e, long f) { shim_note("_CopyPascalStringToC"); return 0; }

long DTInstall(long a, long b, long c_, long d, long e, long f) __asm("_DTInstall");
long DTInstall(long a, long b, long c_, long d, long e, long f) { shim_note("_DTInstall"); return 0; }

long DateString(long a, long b, long c_, long d, long e, long f) __asm("_DateString");
long DateString(long a, long b, long c_, long d, long e, long f) { shim_note("_DateString"); return 0; }

long DateToSeconds(long a, long b, long c_, long d, long e, long f) __asm("_DateToSeconds");
long DateToSeconds(long a, long b, long c_, long d, long e, long f) { shim_note("_DateToSeconds"); return 0; }

long DirCreate(long a, long b, long c_, long d, long e, long f) __asm("_DirCreate");
long DirCreate(long a, long b, long c_, long d, long e, long f) { shim_note("_DirCreate"); return 0; }

long FSMatchAlias(long a, long b, long c_, long d, long e, long f) __asm("_FSMatchAlias");
long FSMatchAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_FSMatchAlias"); return 0; }

long FSSpecToNativePathName(long a, long b, long c_, long d, long e, long f) __asm("_FSSpecToNativePathName");
long FSSpecToNativePathName(long a, long b, long c_, long d, long e, long f) { shim_note("_FSSpecToNativePathName"); return 0; }

long FSpCreateResFile(long a, long b, long c_, long d, long e, long f) __asm("_FSpCreateResFile");
long FSpCreateResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_FSpCreateResFile"); return 0; }

long FlushVol(long a, long b, long c_, long d, long e, long f) __asm("_FlushVol");
long FlushVol(long a, long b, long c_, long d, long e, long f) { shim_note("_FlushVol"); return 0; }

long FreeMem(long a, long b, long c_, long d, long e, long f) __asm("_FreeMem");
long FreeMem(long a, long b, long c_, long d, long e, long f) { shim_note("_FreeMem"); return 0; }

long GetAliasInfo(long a, long b, long c_, long d, long e, long f) __asm("_GetAliasInfo");
long GetAliasInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_GetAliasInfo"); return 0; }

long GetComponentVersion(long a, long b, long c_, long d, long e, long f) __asm("_GetComponentVersion");
long GetComponentVersion(long a, long b, long c_, long d, long e, long f) { shim_note("_GetComponentVersion"); return 0; }

long GetFPos(long a, long b, long c_, long d, long e, long f) __asm("_GetFPos");
long GetFPos(long a, long b, long c_, long d, long e, long f) { shim_note("_GetFPos"); return 0; }

long GetString(long a, long b, long c_, long d, long e, long f) __asm("_GetString");
long GetString(long a, long b, long c_, long d, long e, long f) { shim_note("_GetString"); return 0; }

long HOpenResFile(long a, long b, long c_, long d, long e, long f) __asm("_HOpenResFile");
long HOpenResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_HOpenResFile"); return 0; }

long LMGetCurApName(long a, long b, long c_, long d, long e, long f) __asm("_LMGetCurApName");
long LMGetCurApName(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetCurApName"); return 0; }

long LMGetCurApRefNum(long a, long b, long c_, long d, long e, long f) __asm("_LMGetCurApRefNum");
long LMGetCurApRefNum(long a, long b, long c_, long d, long e, long f) { shim_note("_LMGetCurApRefNum"); return 0; }

long MakeDataExecutable(long a, long b, long c_, long d, long e, long f) __asm("_MakeDataExecutable");
long MakeDataExecutable(long a, long b, long c_, long d, long e, long f) { shim_note("_MakeDataExecutable"); return 0; }

long MatchAlias(long a, long b, long c_, long d, long e, long f) __asm("_MatchAlias");
long MatchAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_MatchAlias"); return 0; }

long MoveHHi(long a, long b, long c_, long d, long e, long f) __asm("_MoveHHi");
long MoveHHi(long a, long b, long c_, long d, long e, long f) { shim_note("_MoveHHi"); return 0; }

long NativePathNameToFSSpec(long a, long b, long c_, long d, long e, long f) __asm("_NativePathNameToFSSpec");
long NativePathNameToFSSpec(long a, long b, long c_, long d, long e, long f) { shim_note("_NativePathNameToFSSpec"); return 0; }

long NewAlias(long a, long b, long c_, long d, long e, long f) __asm("_NewAlias");
long NewAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_NewAlias"); return 0; }

long NewAliasMinimal(long a, long b, long c_, long d, long e, long f) __asm("_NewAliasMinimal");
long NewAliasMinimal(long a, long b, long c_, long d, long e, long f) { shim_note("_NewAliasMinimal"); return 0; }

long PBCloseSync(long a, long b, long c_, long d, long e, long f) __asm("_PBCloseSync");
long PBCloseSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBCloseSync"); return 0; }

long PBDirCreateSync(long a, long b, long c_, long d, long e, long f) __asm("_PBDirCreateSync");
long PBDirCreateSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBDirCreateSync"); return 0; }

long PBFlushFileSync(long a, long b, long c_, long d, long e, long f) __asm("_PBFlushFileSync");
long PBFlushFileSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBFlushFileSync"); return 0; }

long PBFlushVolSync(long a, long b, long c_, long d, long e, long f) __asm("_PBFlushVolSync");
long PBFlushVolSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBFlushVolSync"); return 0; }

long PBGetEOFSync(long a, long b, long c_, long d, long e, long f) __asm("_PBGetEOFSync");
long PBGetEOFSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBGetEOFSync"); return 0; }

long PBGetFCBInfoSync(long a, long b, long c_, long d, long e, long f) __asm("_PBGetFCBInfoSync");
long PBGetFCBInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBGetFCBInfoSync"); return 0; }

long PBGetFPosSync(long a, long b, long c_, long d, long e, long f) __asm("_PBGetFPosSync");
long PBGetFPosSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBGetFPosSync"); return 0; }

long PBHGetFInfoSync(long a, long b, long c_, long d, long e, long f) __asm("_PBHGetFInfoSync");
long PBHGetFInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBHGetFInfoSync"); return 0; }

long PBHOpenDenySync(long a, long b, long c_, long d, long e, long f) __asm("_PBHOpenDenySync");
long PBHOpenDenySync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBHOpenDenySync"); return 0; }

long PBReadAsync(long a, long b, long c_, long d, long e, long f) __asm("_PBReadAsync");
long PBReadAsync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBReadAsync"); return 0; }

long PBReadSync(long a, long b, long c_, long d, long e, long f) __asm("_PBReadSync");
long PBReadSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBReadSync"); return 0; }

long PBSetCatInfoSync(long a, long b, long c_, long d, long e, long f) __asm("_PBSetCatInfoSync");
long PBSetCatInfoSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBSetCatInfoSync"); return 0; }

long PBSetEOFSync(long a, long b, long c_, long d, long e, long f) __asm("_PBSetEOFSync");
long PBSetEOFSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBSetEOFSync"); return 0; }

long PBWriteAsync(long a, long b, long c_, long d, long e, long f) __asm("_PBWriteAsync");
long PBWriteAsync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBWriteAsync"); return 0; }

long PBWriteSync(long a, long b, long c_, long d, long e, long f) __asm("_PBWriteSync");
long PBWriteSync(long a, long b, long c_, long d, long e, long f) { shim_note("_PBWriteSync"); return 0; }

long PurgeSpace(long a, long b, long c_, long d, long e, long f) __asm("_PurgeSpace");
long PurgeSpace(long a, long b, long c_, long d, long e, long f) { shim_note("_PurgeSpace"); return 0; }

long ResolveAlias(long a, long b, long c_, long d, long e, long f) __asm("_ResolveAlias");
long ResolveAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_ResolveAlias"); return 0; }

long ResolveAliasFile(long a, long b, long c_, long d, long e, long f) __asm("_ResolveAliasFile");
long ResolveAliasFile(long a, long b, long c_, long d, long e, long f) { shim_note("_ResolveAliasFile"); return 0; }

long SetA5(long a, long b, long c_, long d, long e, long f) __asm("_SetA5");
long SetA5(long a, long b, long c_, long d, long e, long f) { shim_note("_SetA5"); return 0; }

long TempFreeMem(long a, long b, long c_, long d, long e, long f) __asm("_TempFreeMem");
long TempFreeMem(long a, long b, long c_, long d, long e, long f) { shim_note("_TempFreeMem"); return 0; }

long TimeString(long a, long b, long c_, long d, long e, long f) __asm("_TimeString");
long TimeString(long a, long b, long c_, long d, long e, long f) { shim_note("_TimeString"); return 0; }

long UpdateAlias(long a, long b, long c_, long d, long e, long f) __asm("_UpdateAlias");
long UpdateAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_UpdateAlias"); return 0; }

long UpperString(long a, long b, long c_, long d, long e, long f) __asm("_UpperString");
long UpperString(long a, long b, long c_, long d, long e, long f) { shim_note("_UpperString"); return 0; }
