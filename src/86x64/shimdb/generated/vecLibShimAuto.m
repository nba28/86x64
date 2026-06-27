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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "vecLib", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long create_fftsetup(long a, long b, long c_, long d, long e, long f) { shim_note("create_fftsetup"); return 0; }

long ctoz(long a, long b, long c_, long d, long e, long f) { shim_note("ctoz"); return 0; }

long destroy_fftsetup(long a, long b, long c_, long d, long e, long f) { shim_note("destroy_fftsetup"); return 0; }

long fft_zip(long a, long b, long c_, long d, long e, long f) { shim_note("fft_zip"); return 0; }

long fft_zop(long a, long b, long c_, long d, long e, long f) { shim_note("fft_zop"); return 0; }

long vadd(long a, long b, long c_, long d, long e, long f) { shim_note("vadd"); return 0; }

long vmul(long a, long b, long c_, long d, long e, long f) { shim_note("vmul"); return 0; }

long vsmul(long a, long b, long c_, long d, long e, long f) { shim_note("vsmul"); return 0; }

long ztoc(long a, long b, long c_, long d, long e, long f) { shim_note("ztoc"); return 0; }
