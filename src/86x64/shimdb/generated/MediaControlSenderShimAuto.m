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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "MediaControlSender", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long AVRouteDescription_HasFeature(long a, long b, long c_, long d, long e, long f) { shim_note("AVRouteDescription_HasFeature"); return 0; }

long AVSystemController_CopyProperty(long a, long b, long c_, long d, long e, long f) { shim_note("AVSystemController_CopyProperty"); return 0; }

long kAVRouteFeature_AirPlayScreen(long a, long b, long c_, long d, long e, long f) { shim_note("kAVRouteFeature_AirPlayScreen"); return 0; }

long kAVSystemControllerProperty_PickedRoute(long a, long b, long c_, long d, long e, long f) { shim_note("kAVSystemControllerProperty_PickedRoute"); return 0; }
