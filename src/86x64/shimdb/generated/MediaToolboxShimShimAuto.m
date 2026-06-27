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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "MediaToolboxShim", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long FigTrackReaderGetFigBaseObject(long a, long b, long c_, long d, long e, long f) { shim_note("FigTrackReaderGetFigBaseObject"); return 0; }

long kFigFormatWriterOption_FileFormat_QuickTimeMovie(long a, long b, long c_, long d, long e, long f) { shim_note("kFigFormatWriterOption_FileFormat_QuickTimeMovie"); return 0; }

long kFigFormatWriterOption_FileFormat_iTunesFamily(long a, long b, long c_, long d, long e, long f) { shim_note("kFigFormatWriterOption_FileFormat_iTunesFamily"); return 0; }

long kFigMetadataItemProperty_Key(long a, long b, long c_, long d, long e, long f) { shim_note("kFigMetadataItemProperty_Key"); return 0; }

long kFigMetadataItemProperty_Keyspace(long a, long b, long c_, long d, long e, long f) { shim_note("kFigMetadataItemProperty_Keyspace"); return 0; }

long kFigMetadataItemProperty_Locale(long a, long b, long c_, long d, long e, long f) { shim_note("kFigMetadataItemProperty_Locale"); return 0; }

long kFigTrackProperty_Enabled(long a, long b, long c_, long d, long e, long f) { shim_note("kFigTrackProperty_Enabled"); return 0; }

long kFigTrackProperty_UneditedDuration(long a, long b, long c_, long d, long e, long f) { shim_note("kFigTrackProperty_UneditedDuration"); return 0; }
