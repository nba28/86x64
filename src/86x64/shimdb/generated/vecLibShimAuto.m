#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "vecLib", s); }

long cblas_sgemm(long a, long b, long c_, long d, long e, long f) { shim_note("cblas_sgemm called (auto-stub)"); return 0; }

long sgesv_(long a, long b, long c_, long d, long e, long f) { shim_note("sgesv_ called (auto-stub)"); return 0; }
