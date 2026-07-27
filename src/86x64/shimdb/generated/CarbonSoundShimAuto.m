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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "CarbonSound", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long GetCompressionInfo(long a, long b, long c_, long d, long e, long f) __asm("_GetCompressionInfo");
long GetCompressionInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_GetCompressionInfo"); return 0; }

long SoundComponentGetInfo(long a, long b, long c_, long d, long e, long f) __asm("_SoundComponentGetInfo");
long SoundComponentGetInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_SoundComponentGetInfo"); return 0; }

long SoundComponentSetInfo(long a, long b, long c_, long d, long e, long f) __asm("_SoundComponentSetInfo");
long SoundComponentSetInfo(long a, long b, long c_, long d, long e, long f) { shim_note("_SoundComponentSetInfo"); return 0; }
