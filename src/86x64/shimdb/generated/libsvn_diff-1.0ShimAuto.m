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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libsvn_diff-1.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long svn_diff_contains_conflicts(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_contains_conflicts");
long svn_diff_contains_conflicts(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_contains_conflicts"); return 0; }

long svn_diff_contains_diffs(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_contains_diffs");
long svn_diff_contains_diffs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_contains_diffs"); return 0; }

long svn_diff_diff(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_diff");
long svn_diff_diff(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_diff"); return 0; }

long svn_diff_diff3(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_diff3");
long svn_diff_diff3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_diff3"); return 0; }

long svn_diff_diff4(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_diff4");
long svn_diff_diff4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_diff4"); return 0; }

long svn_diff_file_diff(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_diff");
long svn_diff_file_diff(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_diff"); return 0; }

long svn_diff_file_diff3(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_diff3");
long svn_diff_file_diff3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_diff3"); return 0; }

long svn_diff_file_diff3_2(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_diff3_2");
long svn_diff_file_diff3_2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_diff3_2"); return 0; }

long svn_diff_file_diff4(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_diff4");
long svn_diff_file_diff4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_diff4"); return 0; }

long svn_diff_file_diff4_2(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_diff4_2");
long svn_diff_file_diff4_2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_diff4_2"); return 0; }

long svn_diff_file_diff_2(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_diff_2");
long svn_diff_file_diff_2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_diff_2"); return 0; }

long svn_diff_file_options_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_options_create");
long svn_diff_file_options_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_options_create"); return 0; }

long svn_diff_file_options_parse(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_options_parse");
long svn_diff_file_options_parse(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_options_parse"); return 0; }

long svn_diff_file_output_merge(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_output_merge");
long svn_diff_file_output_merge(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_output_merge"); return 0; }

long svn_diff_file_output_merge2(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_output_merge2");
long svn_diff_file_output_merge2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_output_merge2"); return 0; }

long svn_diff_file_output_unified(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_output_unified");
long svn_diff_file_output_unified(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_output_unified"); return 0; }

long svn_diff_file_output_unified2(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_output_unified2");
long svn_diff_file_output_unified2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_output_unified2"); return 0; }

long svn_diff_file_output_unified3(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_file_output_unified3");
long svn_diff_file_output_unified3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_file_output_unified3"); return 0; }

long svn_diff_mem_string_diff(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_mem_string_diff");
long svn_diff_mem_string_diff(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_mem_string_diff"); return 0; }

long svn_diff_mem_string_diff3(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_mem_string_diff3");
long svn_diff_mem_string_diff3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_mem_string_diff3"); return 0; }

long svn_diff_mem_string_diff4(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_mem_string_diff4");
long svn_diff_mem_string_diff4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_mem_string_diff4"); return 0; }

long svn_diff_mem_string_output_merge(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_mem_string_output_merge");
long svn_diff_mem_string_output_merge(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_mem_string_output_merge"); return 0; }

long svn_diff_mem_string_output_merge2(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_mem_string_output_merge2");
long svn_diff_mem_string_output_merge2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_mem_string_output_merge2"); return 0; }

long svn_diff_mem_string_output_unified(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_mem_string_output_unified");
long svn_diff_mem_string_output_unified(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_mem_string_output_unified"); return 0; }

long svn_diff_output(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_output");
long svn_diff_output(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_output"); return 0; }

long svn_diff_version(long a, long b, long c_, long d, long e, long f) __asm("_svn_diff_version");
long svn_diff_version(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_diff_version"); return 0; }
