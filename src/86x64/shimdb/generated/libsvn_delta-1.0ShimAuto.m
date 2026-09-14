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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libsvn_delta-1.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long svn_compat_wrap_file_rev_handler(long a, long b, long c_, long d, long e, long f) __asm("_svn_compat_wrap_file_rev_handler");
long svn_compat_wrap_file_rev_handler(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_compat_wrap_file_rev_handler"); return 0; }

long svn_delta_default_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_delta_default_editor");
long svn_delta_default_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_delta_default_editor"); return 0; }

long svn_delta_depth_filter_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_delta_depth_filter_editor");
long svn_delta_depth_filter_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_delta_depth_filter_editor"); return 0; }

long svn_delta_get_cancellation_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_delta_get_cancellation_editor");
long svn_delta_get_cancellation_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_delta_get_cancellation_editor"); return 0; }

long svn_delta_noop_window_handler(long a, long b, long c_, long d, long e, long f) __asm("_svn_delta_noop_window_handler");
long svn_delta_noop_window_handler(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_delta_noop_window_handler"); return 0; }

long svn_delta_path_driver(long a, long b, long c_, long d, long e, long f) __asm("_svn_delta_path_driver");
long svn_delta_path_driver(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_delta_path_driver"); return 0; }

long svn_delta_version(long a, long b, long c_, long d, long e, long f) __asm("_svn_delta_version");
long svn_delta_version(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_delta_version"); return 0; }

long svn_txdelta(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta");
long svn_txdelta(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta"); return 0; }

long svn_txdelta_apply(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_apply");
long svn_txdelta_apply(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_apply"); return 0; }

long svn_txdelta_apply_instructions(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_apply_instructions");
long svn_txdelta_apply_instructions(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_apply_instructions"); return 0; }

long svn_txdelta_compose_windows(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_compose_windows");
long svn_txdelta_compose_windows(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_compose_windows"); return 0; }

long svn_txdelta_md5_digest(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_md5_digest");
long svn_txdelta_md5_digest(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_md5_digest"); return 0; }

long svn_txdelta_next_window(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_next_window");
long svn_txdelta_next_window(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_next_window"); return 0; }

long svn_txdelta_parse_svndiff(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_parse_svndiff");
long svn_txdelta_parse_svndiff(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_parse_svndiff"); return 0; }

long svn_txdelta_read_svndiff_window(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_read_svndiff_window");
long svn_txdelta_read_svndiff_window(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_read_svndiff_window"); return 0; }

long svn_txdelta_run(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_run");
long svn_txdelta_run(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_run"); return 0; }

long svn_txdelta_send_stream(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_send_stream");
long svn_txdelta_send_stream(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_send_stream"); return 0; }

long svn_txdelta_send_string(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_send_string");
long svn_txdelta_send_string(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_send_string"); return 0; }

long svn_txdelta_send_txstream(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_send_txstream");
long svn_txdelta_send_txstream(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_send_txstream"); return 0; }

long svn_txdelta_skip_svndiff_window(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_skip_svndiff_window");
long svn_txdelta_skip_svndiff_window(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_skip_svndiff_window"); return 0; }

long svn_txdelta_stream_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_stream_create");
long svn_txdelta_stream_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_stream_create"); return 0; }

long svn_txdelta_target_push(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_target_push");
long svn_txdelta_target_push(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_target_push"); return 0; }

long svn_txdelta_to_svndiff(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_to_svndiff");
long svn_txdelta_to_svndiff(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_to_svndiff"); return 0; }

long svn_txdelta_to_svndiff2(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_to_svndiff2");
long svn_txdelta_to_svndiff2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_to_svndiff2"); return 0; }

long svn_txdelta_window_dup(long a, long b, long c_, long d, long e, long f) __asm("_svn_txdelta_window_dup");
long svn_txdelta_window_dup(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_txdelta_window_dup"); return 0; }
