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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libsvn_ra-1.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long svn_ra_change_rev_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_change_rev_prop");
long svn_ra_change_rev_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_change_rev_prop"); return 0; }

long svn_ra_check_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_check_path");
long svn_ra_check_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_check_path"); return 0; }

long svn_ra_create_callbacks(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_create_callbacks");
long svn_ra_create_callbacks(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_create_callbacks"); return 0; }

long svn_ra_do_diff(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_diff");
long svn_ra_do_diff(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_diff"); return 0; }

long svn_ra_do_diff2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_diff2");
long svn_ra_do_diff2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_diff2"); return 0; }

long svn_ra_do_diff3(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_diff3");
long svn_ra_do_diff3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_diff3"); return 0; }

long svn_ra_do_status(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_status");
long svn_ra_do_status(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_status"); return 0; }

long svn_ra_do_status2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_status2");
long svn_ra_do_status2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_status2"); return 0; }

long svn_ra_do_switch(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_switch");
long svn_ra_do_switch(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_switch"); return 0; }

long svn_ra_do_switch2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_switch2");
long svn_ra_do_switch2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_switch2"); return 0; }

long svn_ra_do_update(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_update");
long svn_ra_do_update(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_update"); return 0; }

long svn_ra_do_update2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_do_update2");
long svn_ra_do_update2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_do_update2"); return 0; }

long svn_ra_get_commit_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_commit_editor");
long svn_ra_get_commit_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_commit_editor"); return 0; }

long svn_ra_get_commit_editor2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_commit_editor2");
long svn_ra_get_commit_editor2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_commit_editor2"); return 0; }

long svn_ra_get_commit_editor3(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_commit_editor3");
long svn_ra_get_commit_editor3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_commit_editor3"); return 0; }

long svn_ra_get_dated_revision(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_dated_revision");
long svn_ra_get_dated_revision(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_dated_revision"); return 0; }

long svn_ra_get_deleted_rev(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_deleted_rev");
long svn_ra_get_deleted_rev(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_deleted_rev"); return 0; }

long svn_ra_get_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_dir");
long svn_ra_get_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_dir"); return 0; }

long svn_ra_get_dir2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_dir2");
long svn_ra_get_dir2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_dir2"); return 0; }

long svn_ra_get_file(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_file");
long svn_ra_get_file(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_file"); return 0; }

long svn_ra_get_file_revs(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_file_revs");
long svn_ra_get_file_revs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_file_revs"); return 0; }

long svn_ra_get_file_revs2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_file_revs2");
long svn_ra_get_file_revs2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_file_revs2"); return 0; }

long svn_ra_get_latest_revnum(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_latest_revnum");
long svn_ra_get_latest_revnum(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_latest_revnum"); return 0; }

long svn_ra_get_location_segments(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_location_segments");
long svn_ra_get_location_segments(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_location_segments"); return 0; }

long svn_ra_get_locations(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_locations");
long svn_ra_get_locations(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_locations"); return 0; }

long svn_ra_get_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_lock");
long svn_ra_get_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_lock"); return 0; }

long svn_ra_get_locks(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_locks");
long svn_ra_get_locks(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_locks"); return 0; }

long svn_ra_get_log(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_log");
long svn_ra_get_log(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_log"); return 0; }

long svn_ra_get_log2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_log2");
long svn_ra_get_log2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_log2"); return 0; }

long svn_ra_get_mergeinfo(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_mergeinfo");
long svn_ra_get_mergeinfo(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_mergeinfo"); return 0; }

long svn_ra_get_ra_library(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_ra_library");
long svn_ra_get_ra_library(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_ra_library"); return 0; }

long svn_ra_get_repos_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_repos_root");
long svn_ra_get_repos_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_repos_root"); return 0; }

long svn_ra_get_repos_root2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_repos_root2");
long svn_ra_get_repos_root2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_repos_root2"); return 0; }

long svn_ra_get_session_url(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_session_url");
long svn_ra_get_session_url(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_session_url"); return 0; }

long svn_ra_get_uuid(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_uuid");
long svn_ra_get_uuid(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_uuid"); return 0; }

long svn_ra_get_uuid2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_get_uuid2");
long svn_ra_get_uuid2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_get_uuid2"); return 0; }

long svn_ra_has_capability(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_has_capability");
long svn_ra_has_capability(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_has_capability"); return 0; }

long svn_ra_init_ra_libs(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_init_ra_libs");
long svn_ra_init_ra_libs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_init_ra_libs"); return 0; }

long svn_ra_initialize(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_initialize");
long svn_ra_initialize(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_initialize"); return 0; }

long svn_ra_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_lock");
long svn_ra_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_lock"); return 0; }

long svn_ra_open(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_open");
long svn_ra_open(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_open"); return 0; }

long svn_ra_open2(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_open2");
long svn_ra_open2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_open2"); return 0; }

long svn_ra_open3(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_open3");
long svn_ra_open3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_open3"); return 0; }

long svn_ra_print_modules(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_print_modules");
long svn_ra_print_modules(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_print_modules"); return 0; }

long svn_ra_print_ra_libraries(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_print_ra_libraries");
long svn_ra_print_ra_libraries(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_print_ra_libraries"); return 0; }

long svn_ra_reparent(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_reparent");
long svn_ra_reparent(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_reparent"); return 0; }

long svn_ra_replay(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_replay");
long svn_ra_replay(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_replay"); return 0; }

long svn_ra_replay_range(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_replay_range");
long svn_ra_replay_range(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_replay_range"); return 0; }

long svn_ra_rev_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_rev_prop");
long svn_ra_rev_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_rev_prop"); return 0; }

long svn_ra_rev_proplist(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_rev_proplist");
long svn_ra_rev_proplist(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_rev_proplist"); return 0; }

long svn_ra_stat(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_stat");
long svn_ra_stat(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_stat"); return 0; }

long svn_ra_unlock(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_unlock");
long svn_ra_unlock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_unlock"); return 0; }

long svn_ra_version(long a, long b, long c_, long d, long e, long f) __asm("_svn_ra_version");
long svn_ra_version(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_ra_version"); return 0; }
