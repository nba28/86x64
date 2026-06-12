#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "eOkaoDt.dylib", s); }

long OKAO_CreateDetection(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_CreateDetection called (auto-stub)"); return 0; }

long OKAO_CreateDtResult(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_CreateDtResult called (auto-stub)"); return 0; }

long OKAO_DeleteDetection(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_DeleteDetection called (auto-stub)"); return 0; }

long OKAO_DeleteDtResult(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_DeleteDtResult called (auto-stub)"); return 0; }

long OKAO_Detection(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_Detection called (auto-stub)"); return 0; }

long OKAO_GetDtCorner(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetDtCorner called (auto-stub)"); return 0; }

long OKAO_GetDtFaceCount(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetDtFaceCount called (auto-stub)"); return 0; }

long OKAO_GetDtVersion(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_GetDtVersion called (auto-stub)"); return 0; }

long OKAO_SetDtDetectAngle(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetDtDetectAngle called (auto-stub)"); return 0; }

long OKAO_SetDtDetectDirection(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetDtDetectDirection called (auto-stub)"); return 0; }

long OKAO_SetDtFaceSizeRange(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetDtFaceSizeRange called (auto-stub)"); return 0; }

long OKAO_SetDtPose(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetDtPose called (auto-stub)"); return 0; }

long OKAO_SetDtThreshold(long a, long b, long c_, long d, long e, long f) { shim_note("OKAO_SetDtThreshold called (auto-stub)"); return 0; }
