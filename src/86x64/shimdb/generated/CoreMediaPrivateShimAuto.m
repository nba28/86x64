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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "CoreMediaPrivate", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long FigFormatDescriptionRelease(long a, long b, long c_, long d, long e, long f) __asm("_FigFormatDescriptionRelease");
long FigFormatDescriptionRelease(long a, long b, long c_, long d, long e, long f) { shim_note("_FigFormatDescriptionRelease"); return 0; }

long FigMuxedFormatDescriptionCreate(long a, long b, long c_, long d, long e, long f) __asm("_FigMuxedFormatDescriptionCreate");
long FigMuxedFormatDescriptionCreate(long a, long b, long c_, long d, long e, long f) { shim_note("_FigMuxedFormatDescriptionCreate"); return 0; }

long FigReentrantMutexCreate(long a, long b, long c_, long d, long e, long f) __asm("_FigReentrantMutexCreate");
long FigReentrantMutexCreate(long a, long b, long c_, long d, long e, long f) { shim_note("_FigReentrantMutexCreate"); return 0; }

long FigReentrantMutexDestroy(long a, long b, long c_, long d, long e, long f) __asm("_FigReentrantMutexDestroy");
long FigReentrantMutexDestroy(long a, long b, long c_, long d, long e, long f) { shim_note("_FigReentrantMutexDestroy"); return 0; }

long FigReentrantMutexLock(long a, long b, long c_, long d, long e, long f) __asm("_FigReentrantMutexLock");
long FigReentrantMutexLock(long a, long b, long c_, long d, long e, long f) { shim_note("_FigReentrantMutexLock"); return 0; }

long FigReentrantMutexUnlock(long a, long b, long c_, long d, long e, long f) __asm("_FigReentrantMutexUnlock");
long FigReentrantMutexUnlock(long a, long b, long c_, long d, long e, long f) { shim_note("_FigReentrantMutexUnlock"); return 0; }

long FigSampleBufferRelease(long a, long b, long c_, long d, long e, long f) __asm("_FigSampleBufferRelease");
long FigSampleBufferRelease(long a, long b, long c_, long d, long e, long f) { shim_note("_FigSampleBufferRelease"); return 0; }
