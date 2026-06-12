#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "CoreMediaAuthoring", s); }

long RFExportCreateCopy(long a, long b, long c_, long d, long e, long f) { shim_note("RFExportCreateCopy called (auto-stub)"); return 0; }

long RFExportSourceCreateWithMovieData(long a, long b, long c_, long d, long e, long f) { shim_note("RFExportSourceCreateWithMovieData called (auto-stub)"); return 0; }

long RFExportSourceCreateWithURL(long a, long b, long c_, long d, long e, long f) { shim_note("RFExportSourceCreateWithURL called (auto-stub)"); return 0; }

long kRFExportOption_EnableHardwareVideoEncoder(long a, long b, long c_, long d, long e, long f) { shim_note("kRFExportOption_EnableHardwareVideoEncoder called (auto-stub)"); return 0; }

long kRFExportOption_UseQTSourceAlways(long a, long b, long c_, long d, long e, long f) { shim_note("kRFExportOption_UseQTSourceAlways called (auto-stub)"); return 0; }
