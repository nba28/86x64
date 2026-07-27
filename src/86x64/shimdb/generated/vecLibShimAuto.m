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

long create_fftsetup(long a, long b, long c_, long d, long e, long f) __asm("_create_fftsetup");
long create_fftsetup(long a, long b, long c_, long d, long e, long f) { shim_note("_create_fftsetup"); return 0; }

long ctoz(long a, long b, long c_, long d, long e, long f) __asm("_ctoz");
long ctoz(long a, long b, long c_, long d, long e, long f) { shim_note("_ctoz"); return 0; }

long destroy_fftsetup(long a, long b, long c_, long d, long e, long f) __asm("_destroy_fftsetup");
long destroy_fftsetup(long a, long b, long c_, long d, long e, long f) { shim_note("_destroy_fftsetup"); return 0; }

long dotpr(long a, long b, long c_, long d, long e, long f) __asm("_dotpr");
long dotpr(long a, long b, long c_, long d, long e, long f) { shim_note("_dotpr"); return 0; }

long fft_zip(long a, long b, long c_, long d, long e, long f) __asm("_fft_zip");
long fft_zip(long a, long b, long c_, long d, long e, long f) { shim_note("_fft_zip"); return 0; }

long fft_zrip(long a, long b, long c_, long d, long e, long f) __asm("_fft_zrip");
long fft_zrip(long a, long b, long c_, long d, long e, long f) { shim_note("_fft_zrip"); return 0; }

long fft_zrop(long a, long b, long c_, long d, long e, long f) __asm("_fft_zrop");
long fft_zrop(long a, long b, long c_, long d, long e, long f) { shim_note("_fft_zrop"); return 0; }

long fft_zropt(long a, long b, long c_, long d, long e, long f) __asm("_fft_zropt");
long fft_zropt(long a, long b, long c_, long d, long e, long f) { shim_note("_fft_zropt"); return 0; }

long vadd(long a, long b, long c_, long d, long e, long f) __asm("_vadd");
long vadd(long a, long b, long c_, long d, long e, long f) { shim_note("_vadd"); return 0; }

long vmul(long a, long b, long c_, long d, long e, long f) __asm("_vmul");
long vmul(long a, long b, long c_, long d, long e, long f) { shim_note("_vmul"); return 0; }

long vsmul(long a, long b, long c_, long d, long e, long f) __asm("_vsmul");
long vsmul(long a, long b, long c_, long d, long e, long f) { shim_note("_vsmul"); return 0; }

long ztoc(long a, long b, long c_, long d, long e, long f) __asm("_ztoc");
long ztoc(long a, long b, long c_, long d, long e, long f) { shim_note("_ztoc"); return 0; }

long zvcma(long a, long b, long c_, long d, long e, long f) __asm("_zvcma");
long zvcma(long a, long b, long c_, long d, long e, long f) { shim_note("_zvcma"); return 0; }
