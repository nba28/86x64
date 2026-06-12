#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "VideoToolbox", s); }

long kVTProfileLevel_H264_Baseline_1_3(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Baseline_1_3 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_Baseline_3_0(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Baseline_3_0 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_Extended_5_0(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Extended_5_0 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_High_5_0(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_High_5_0 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_Main_3_0(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Main_3_0 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_Main_3_1(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Main_3_1 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_Main_4_0(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Main_4_0 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_Main_4_1(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Main_4_1 called (auto-stub)"); return 0; }

long kVTProfileLevel_H264_Main_5_0(long a, long b, long c_, long d, long e, long f) { shim_note("kVTProfileLevel_H264_Main_5_0 called (auto-stub)"); return 0; }
