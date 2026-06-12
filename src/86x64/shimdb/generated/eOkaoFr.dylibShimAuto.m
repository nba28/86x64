#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "eOkaoFr.dylib", s); }

long OKAO_CreateAlbum(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_CreateAlbum called (auto-stub)"); return 0; }

long OKAO_CreateFaceData(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_CreateFaceData called (auto-stub)"); return 0; }

long OKAO_DeleteAlbum(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_DeleteAlbum called (auto-stub)"); return 0; }

long OKAO_DeleteFaceData(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_DeleteFaceData called (auto-stub)"); return 0; }

long OKAO_GetFaceData(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetFaceData called (auto-stub)"); return 0; }

long OKAO_GetFaceDataSize(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetFaceDataSize called (auto-stub)"); return 0; }

long OKAO_GetFrVersion(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetFrVersion called (auto-stub)"); return 0; }

long OKAO_Identify(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_Identify called (auto-stub)"); return 0; }

long OKAO_QueryRegistUserDataNum(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_QueryRegistUserDataNum called (auto-stub)"); return 0; }

long OKAO_QueryRegistUserNum(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_QueryRegistUserNum called (auto-stub)"); return 0; }

long OKAO_RegistAlbum(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_RegistAlbum called (auto-stub)"); return 0; }

long OKAO_SetFaceData(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetFaceData called (auto-stub)"); return 0; }

long OKAO_SetFaceDataBuffer(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetFaceDataBuffer called (auto-stub)"); return 0; }
