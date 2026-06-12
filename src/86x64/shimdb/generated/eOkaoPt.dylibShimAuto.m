#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "eOkaoPt.dylib", s); }

long OKAO_CreatePointer(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_CreatePointer called (auto-stub)"); return 0; }

long OKAO_CreatePtResult(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_CreatePtResult called (auto-stub)"); return 0; }

long OKAO_DeletePointer(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_DeletePointer called (auto-stub)"); return 0; }

long OKAO_DeletePtResult(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_DeletePtResult called (auto-stub)"); return 0; }

long OKAO_GetPtPoint(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetPtPoint called (auto-stub)"); return 0; }

long OKAO_GetPtVersion(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetPtVersion called (auto-stub)"); return 0; }

long OKAO_Pointer(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_Pointer called (auto-stub)"); return 0; }

long OKAO_SetPtMode(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetPtMode called (auto-stub)"); return 0; }

long OKAO_SetPtPosition(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetPtPosition called (auto-stub)"); return 0; }
