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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libwx_macud_gl-2.8.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long ZN10wxGLCanvas10SetCurrentEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxGLCanvas10SetCurrentEv"); return 0; }

long ZN10wxGLCanvas11SwapBuffersEv(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxGLCanvas11SwapBuffersEv"); return 0; }

long ZN10wxGLCanvas9SetColourEPKw(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxGLCanvas9SetColourEPKw"); return 0; }

long ZN10wxGLCanvasC1EP8wxWindowPK11wxGLContextiRK7wxPointRK6wxSizelRK8wxStringPiRK9wxPalette(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxGLCanvasC1EP8wxWindowPK11wxGLContextiRK7wxPointRK6wxSizelRK8wxStringPiRK9wxPalette"); return 0; }

long ZN10wxGLCanvasC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxStringPiRK9wxPalette(long a, long b, long c_, long d, long e, long f) { shim_note("ZN10wxGLCanvasC1EP8wxWindowiRK7wxPointRK6wxSizelRK8wxStringPiRK9wxPalette"); return 0; }

long ZN11wxGLContextC1EP19__AGLPixelFormatRecP10wxGLCanvasRK9wxPalettePKS_(long a, long b, long c_, long d, long e, long f) { shim_note("ZN11wxGLContextC1EP19__AGLPixelFormatRecP10wxGLCanvasRK9wxPalettePKS_"); return 0; }
