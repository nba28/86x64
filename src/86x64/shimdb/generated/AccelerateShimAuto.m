#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "Accelerate", s); }

long create_fftsetup(long a, long b, long c_, long d, long e, long f) { shim_note("create_fftsetup called (auto-stub)"); return 0; }

long destroy_fftsetup(long a, long b, long c_, long d, long e, long f) { shim_note("destroy_fftsetup called (auto-stub)"); return 0; }

long fft2d_zip(long a, long b, long c_, long d, long e, long f) { shim_note("fft2d_zip called (auto-stub)"); return 0; }

long vsmul(long a, long b, long c_, long d, long e, long f) { shim_note("vsmul called (auto-stub)"); return 0; }

long zvmul(long a, long b, long c_, long d, long e, long f) { shim_note("zvmul called (auto-stub)"); return 0; }
