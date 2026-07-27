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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "IOKit", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long IOConnectMethodScalarIScalarO(long a, long b, long c_, long d, long e, long f) __asm("_IOConnectMethodScalarIScalarO");
long IOConnectMethodScalarIScalarO(long a, long b, long c_, long d, long e, long f) { shim_note("_IOConnectMethodScalarIScalarO"); return 0; }

long IOConnectMethodScalarIStructureO(long a, long b, long c_, long d, long e, long f) __asm("_IOConnectMethodScalarIStructureO");
long IOConnectMethodScalarIStructureO(long a, long b, long c_, long d, long e, long f) { shim_note("_IOConnectMethodScalarIStructureO"); return 0; }

long IOConnectMethodStructureIStructureO(long a, long b, long c_, long d, long e, long f) __asm("_IOConnectMethodStructureIStructureO");
long IOConnectMethodStructureIStructureO(long a, long b, long c_, long d, long e, long f) { shim_note("_IOConnectMethodStructureIStructureO"); return 0; }
