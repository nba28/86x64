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

long AddFolderRouting(long a, long b, long c_, long d, long e, long f) __asm("_AddFolderRouting");
long AddFolderRouting(long a, long b, long c_, long d, long e, long f) { shim_note("_AddFolderRouting"); return 0; }

long AllocContig(long a, long b, long c_, long d, long e, long f) __asm("_AllocContig");
long AllocContig(long a, long b, long c_, long d, long e, long f) { shim_note("_AllocContig"); return 0; }

long Allocate(long a, long b, long c_, long d, long e, long f) __asm("_Allocate");
long Allocate(long a, long b, long c_, long d, long e, long f) { shim_note("_Allocate"); return 0; }

long ComponentFunctionImplemented(long a, long b, long c_, long d, long e, long f) __asm("_ComponentFunctionImplemented");
long ComponentFunctionImplemented(long a, long b, long c_, long d, long e, long f) { shim_note("_ComponentFunctionImplemented"); return 0; }

long ComponentSetTarget(long a, long b, long c_, long d, long e, long f) __asm("_ComponentSetTarget");
long ComponentSetTarget(long a, long b, long c_, long d, long e, long f) { shim_note("_ComponentSetTarget"); return 0; }

long FSpCreateResFile(long a, long b, long c_, long d, long e, long f) __asm("_FSpCreateResFile");
long FSpCreateResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_FSpCreateResFile"); return 0; }

long FSpOpenOrphanResFile(long a, long b, long c_, long d, long e, long f) __asm("_FSpOpenOrphanResFile");
long FSpOpenOrphanResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_FSpOpenOrphanResFile"); return 0; }

long FSpOpenRF(long a, long b, long c_, long d, long e, long f) __asm("_FSpOpenRF");
long FSpOpenRF(long a, long b, long c_, long d, long e, long f) { shim_note("_FSpOpenRF"); return 0; }

long FSpResourceFileAlreadyOpen(long a, long b, long c_, long d, long e, long f) __asm("_FSpResourceFileAlreadyOpen");
long FSpResourceFileAlreadyOpen(long a, long b, long c_, long d, long e, long f) { shim_note("_FSpResourceFileAlreadyOpen"); return 0; }

long FindFolderRouting(long a, long b, long c_, long d, long e, long f) __asm("_FindFolderRouting");
long FindFolderRouting(long a, long b, long c_, long d, long e, long f) { shim_note("_FindFolderRouting"); return 0; }

long FlushIconRefs(long a, long b, long c_, long d, long e, long f) __asm("_FlushIconRefs");
long FlushIconRefs(long a, long b, long c_, long d, long e, long f) { shim_note("_FlushIconRefs"); return 0; }

long FlushIconRefsByVolume(long a, long b, long c_, long d, long e, long f) __asm("_FlushIconRefsByVolume");
long FlushIconRefsByVolume(long a, long b, long c_, long d, long e, long f) { shim_note("_FlushIconRefsByVolume"); return 0; }

long FollowFinderAlias(long a, long b, long c_, long d, long e, long f) __asm("_FollowFinderAlias");
long FollowFinderAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_FollowFinderAlias"); return 0; }

long GetAliasInfo(long a, long b, long c_, long d, long e, long f) __asm("_GetAliasInfo");
long GetAliasInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_GetAliasInfo"); return 0; }

long GetComponentIconSuite(long a, long b, long c_, long d, long e, long f) __asm("_GetComponentIconSuite");
long GetComponentIconSuite(long a, long b, long c_, long d, long e, long f) { shim_note("_GetComponentIconSuite"); return 0; }

long GetComponentVersion(long a, long b, long c_, long d, long e, long f) __asm("_GetComponentVersion");
long GetComponentVersion(long a, long b, long c_, long d, long e, long f) { shim_note("_GetComponentVersion"); return 0; }

long GetFPos(long a, long b, long c_, long d, long e, long f) __asm("_GetFPos");
long GetFPos(long a, long b, long c_, long d, long e, long f) { shim_note("_GetFPos"); return 0; }

long GetFolderName(long a, long b, long c_, long d, long e, long f) __asm("_GetFolderName");
long GetFolderName(long a, long b, long c_, long d, long e, long f) { shim_note("_GetFolderName"); return 0; }

long GetIconRefFromFile(long a, long b, long c_, long d, long e, long f) __asm("_GetIconRefFromFile");
long GetIconRefFromFile(long a, long b, long c_, long d, long e, long f) { shim_note("_GetIconRefFromFile"); return 0; }

long GetVRefNum(long a, long b, long c_, long d, long e, long f) __asm("_GetVRefNum");
long GetVRefNum(long a, long b, long c_, long d, long e, long f) { shim_note("_GetVRefNum"); return 0; }

long HCreate(long a, long b, long c_, long d, long e, long f) __asm("_HCreate");
long HCreate(long a, long b, long c_, long d, long e, long f) { shim_note("_HCreate"); return 0; }

long HCreateResFile(long a, long b, long c_, long d, long e, long f) __asm("_HCreateResFile");
long HCreateResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_HCreateResFile"); return 0; }

long HGetFInfo(long a, long b, long c_, long d, long e, long f) __asm("_HGetFInfo");
long HGetFInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_HGetFInfo"); return 0; }

long HOpen(long a, long b, long c_, long d, long e, long f) __asm("_HOpen");
long HOpen(long a, long b, long c_, long d, long e, long f) { shim_note("_HOpen"); return 0; }

long HOpenDF(long a, long b, long c_, long d, long e, long f) __asm("_HOpenDF");
long HOpenDF(long a, long b, long c_, long d, long e, long f) { shim_note("_HOpenDF"); return 0; }

long HOpenRF(long a, long b, long c_, long d, long e, long f) __asm("_HOpenRF");
long HOpenRF(long a, long b, long c_, long d, long e, long f) { shim_note("_HOpenRF"); return 0; }

long HOpenResFile(long a, long b, long c_, long d, long e, long f) __asm("_HOpenResFile");
long HOpenResFile(long a, long b, long c_, long d, long e, long f) { shim_note("_HOpenResFile"); return 0; }

long HRstFLock(long a, long b, long c_, long d, long e, long f) __asm("_HRstFLock");
long HRstFLock(long a, long b, long c_, long d, long e, long f) { shim_note("_HRstFLock"); return 0; }

long HSetFInfo(long a, long b, long c_, long d, long e, long f) __asm("_HSetFInfo");
long HSetFInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_HSetFInfo"); return 0; }

long HSetFLock(long a, long b, long c_, long d, long e, long f) __asm("_HSetFLock");
long HSetFLock(long a, long b, long c_, long d, long e, long f) { shim_note("_HSetFLock"); return 0; }

long IsAliasFile(long a, long b, long c_, long d, long e, long f) __asm("_IsAliasFile");
long IsAliasFile(long a, long b, long c_, long d, long e, long f) { shim_note("_IsAliasFile"); return 0; }

long OpenRFPerm(long a, long b, long c_, long d, long e, long f) __asm("_OpenRFPerm");
long OpenRFPerm(long a, long b, long c_, long d, long e, long f) { shim_note("_OpenRFPerm"); return 0; }

long OverrideIconRefFromResource(long a, long b, long c_, long d, long e, long f) __asm("_OverrideIconRefFromResource");
long OverrideIconRefFromResource(long a, long b, long c_, long d, long e, long f) { shim_note("_OverrideIconRefFromResource"); return 0; }

long ReadIconFile(long a, long b, long c_, long d, long e, long f) __asm("_ReadIconFile");
long ReadIconFile(long a, long b, long c_, long d, long e, long f) { shim_note("_ReadIconFile"); return 0; }

long RegisterIconRefFromIconFile(long a, long b, long c_, long d, long e, long f) __asm("_RegisterIconRefFromIconFile");
long RegisterIconRefFromIconFile(long a, long b, long c_, long d, long e, long f) { shim_note("_RegisterIconRefFromIconFile"); return 0; }

long RegisterIconRefFromResource(long a, long b, long c_, long d, long e, long f) __asm("_RegisterIconRefFromResource");
long RegisterIconRefFromResource(long a, long b, long c_, long d, long e, long f) { shim_note("_RegisterIconRefFromResource"); return 0; }

long RemoveFolderRouting(long a, long b, long c_, long d, long e, long f) __asm("_RemoveFolderRouting");
long RemoveFolderRouting(long a, long b, long c_, long d, long e, long f) { shim_note("_RemoveFolderRouting"); return 0; }

long SetA5(long a, long b, long c_, long d, long e, long f) __asm("_SetA5");
long SetA5(long a, long b, long c_, long d, long e, long f) { shim_note("_SetA5"); return 0; }

long SetCurrentA5(long a, long b, long c_, long d, long e, long f) __asm("_SetCurrentA5");
long SetCurrentA5(long a, long b, long c_, long d, long e, long f) { shim_note("_SetCurrentA5"); return 0; }

long UnmountVol(long a, long b, long c_, long d, long e, long f) __asm("_UnmountVol");
long UnmountVol(long a, long b, long c_, long d, long e, long f) { shim_note("_UnmountVol"); return 0; }

long UpdateAlias(long a, long b, long c_, long d, long e, long f) __asm("_UpdateAlias");
long UpdateAlias(long a, long b, long c_, long d, long e, long f) { shim_note("_UpdateAlias"); return 0; }

long WriteIconFile(long a, long b, long c_, long d, long e, long f) __asm("_WriteIconFile");
long WriteIconFile(long a, long b, long c_, long d, long e, long f) { shim_note("_WriteIconFile"); return 0; }
