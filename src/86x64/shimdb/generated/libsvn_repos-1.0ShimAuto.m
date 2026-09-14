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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libsvn_repos-1.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long svn_repos_abort_report(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_abort_report");
long svn_repos_abort_report(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_abort_report"); return 0; }

long svn_repos_authz_check_access(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_authz_check_access");
long svn_repos_authz_check_access(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_authz_check_access"); return 0; }

long svn_repos_authz_read(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_authz_read");
long svn_repos_authz_read(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_authz_read"); return 0; }

long svn_repos_begin_report(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_begin_report");
long svn_repos_begin_report(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_begin_report"); return 0; }

long svn_repos_begin_report2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_begin_report2");
long svn_repos_begin_report2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_begin_report2"); return 0; }

long svn_repos_check_revision_access(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_check_revision_access");
long svn_repos_check_revision_access(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_check_revision_access"); return 0; }

long svn_repos_conf_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_conf_dir");
long svn_repos_conf_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_conf_dir"); return 0; }

long svn_repos_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_create");
long svn_repos_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_create"); return 0; }

long svn_repos_dated_revision(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_dated_revision");
long svn_repos_dated_revision(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_dated_revision"); return 0; }

long svn_repos_db_env(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_db_env");
long svn_repos_db_env(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_db_env"); return 0; }

long svn_repos_db_lockfile(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_db_lockfile");
long svn_repos_db_lockfile(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_db_lockfile"); return 0; }

long svn_repos_db_logfiles(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_db_logfiles");
long svn_repos_db_logfiles(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_db_logfiles"); return 0; }

long svn_repos_db_logs_lockfile(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_db_logs_lockfile");
long svn_repos_db_logs_lockfile(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_db_logs_lockfile"); return 0; }

long svn_repos_delete(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_delete");
long svn_repos_delete(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_delete"); return 0; }

long svn_repos_delete_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_delete_path");
long svn_repos_delete_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_delete_path"); return 0; }

long svn_repos_deleted_rev(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_deleted_rev");
long svn_repos_deleted_rev(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_deleted_rev"); return 0; }

long svn_repos_dir_delta(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_dir_delta");
long svn_repos_dir_delta(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_dir_delta"); return 0; }

long svn_repos_dir_delta2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_dir_delta2");
long svn_repos_dir_delta2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_dir_delta2"); return 0; }

long svn_repos_dump_fs(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_dump_fs");
long svn_repos_dump_fs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_dump_fs"); return 0; }

long svn_repos_dump_fs2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_dump_fs2");
long svn_repos_dump_fs2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_dump_fs2"); return 0; }

long svn_repos_find_root_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_find_root_path");
long svn_repos_find_root_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_find_root_path"); return 0; }

long svn_repos_finish_report(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_finish_report");
long svn_repos_finish_report(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_finish_report"); return 0; }

long svn_repos_fs(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs");
long svn_repos_fs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs"); return 0; }

long svn_repos_fs_begin_txn_for_commit(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_begin_txn_for_commit");
long svn_repos_fs_begin_txn_for_commit(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_begin_txn_for_commit"); return 0; }

long svn_repos_fs_begin_txn_for_commit2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_begin_txn_for_commit2");
long svn_repos_fs_begin_txn_for_commit2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_begin_txn_for_commit2"); return 0; }

long svn_repos_fs_begin_txn_for_update(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_begin_txn_for_update");
long svn_repos_fs_begin_txn_for_update(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_begin_txn_for_update"); return 0; }

long svn_repos_fs_change_node_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_change_node_prop");
long svn_repos_fs_change_node_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_change_node_prop"); return 0; }

long svn_repos_fs_change_rev_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_change_rev_prop");
long svn_repos_fs_change_rev_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_change_rev_prop"); return 0; }

long svn_repos_fs_change_rev_prop2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_change_rev_prop2");
long svn_repos_fs_change_rev_prop2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_change_rev_prop2"); return 0; }

long svn_repos_fs_change_rev_prop3(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_change_rev_prop3");
long svn_repos_fs_change_rev_prop3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_change_rev_prop3"); return 0; }

long svn_repos_fs_change_txn_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_change_txn_prop");
long svn_repos_fs_change_txn_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_change_txn_prop"); return 0; }

long svn_repos_fs_change_txn_props(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_change_txn_props");
long svn_repos_fs_change_txn_props(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_change_txn_props"); return 0; }

long svn_repos_fs_commit_txn(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_commit_txn");
long svn_repos_fs_commit_txn(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_commit_txn"); return 0; }

long svn_repos_fs_get_locks(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_get_locks");
long svn_repos_fs_get_locks(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_get_locks"); return 0; }

long svn_repos_fs_get_mergeinfo(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_get_mergeinfo");
long svn_repos_fs_get_mergeinfo(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_get_mergeinfo"); return 0; }

long svn_repos_fs_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_lock");
long svn_repos_fs_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_lock"); return 0; }

long svn_repos_fs_pack(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_pack");
long svn_repos_fs_pack(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_pack"); return 0; }

long svn_repos_fs_revision_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_revision_prop");
long svn_repos_fs_revision_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_revision_prop"); return 0; }

long svn_repos_fs_revision_proplist(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_revision_proplist");
long svn_repos_fs_revision_proplist(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_revision_proplist"); return 0; }

long svn_repos_fs_unlock(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_fs_unlock");
long svn_repos_fs_unlock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_fs_unlock"); return 0; }

long svn_repos_get_commit_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_commit_editor");
long svn_repos_get_commit_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_commit_editor"); return 0; }

long svn_repos_get_commit_editor2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_commit_editor2");
long svn_repos_get_commit_editor2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_commit_editor2"); return 0; }

long svn_repos_get_commit_editor3(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_commit_editor3");
long svn_repos_get_commit_editor3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_commit_editor3"); return 0; }

long svn_repos_get_commit_editor4(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_commit_editor4");
long svn_repos_get_commit_editor4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_commit_editor4"); return 0; }

long svn_repos_get_commit_editor5(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_commit_editor5");
long svn_repos_get_commit_editor5(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_commit_editor5"); return 0; }

long svn_repos_get_committed_info(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_committed_info");
long svn_repos_get_committed_info(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_committed_info"); return 0; }

long svn_repos_get_file_revs(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_file_revs");
long svn_repos_get_file_revs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_file_revs"); return 0; }

long svn_repos_get_file_revs2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_file_revs2");
long svn_repos_get_file_revs2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_file_revs2"); return 0; }

long svn_repos_get_fs_build_parser(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_fs_build_parser");
long svn_repos_get_fs_build_parser(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_fs_build_parser"); return 0; }

long svn_repos_get_fs_build_parser2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_fs_build_parser2");
long svn_repos_get_fs_build_parser2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_fs_build_parser2"); return 0; }

long svn_repos_get_logs(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_logs");
long svn_repos_get_logs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_logs"); return 0; }

long svn_repos_get_logs2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_logs2");
long svn_repos_get_logs2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_logs2"); return 0; }

long svn_repos_get_logs3(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_logs3");
long svn_repos_get_logs3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_logs3"); return 0; }

long svn_repos_get_logs4(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_get_logs4");
long svn_repos_get_logs4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_get_logs4"); return 0; }

long svn_repos_has_capability(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_has_capability");
long svn_repos_has_capability(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_has_capability"); return 0; }

long svn_repos_history(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_history");
long svn_repos_history(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_history"); return 0; }

long svn_repos_history2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_history2");
long svn_repos_history2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_history2"); return 0; }

long svn_repos_hook_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_hook_dir");
long svn_repos_hook_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_hook_dir"); return 0; }

long svn_repos_hotcopy(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_hotcopy");
long svn_repos_hotcopy(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_hotcopy"); return 0; }

long svn_repos_link_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_link_path");
long svn_repos_link_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_link_path"); return 0; }

long svn_repos_link_path2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_link_path2");
long svn_repos_link_path2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_link_path2"); return 0; }

long svn_repos_link_path3(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_link_path3");
long svn_repos_link_path3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_link_path3"); return 0; }

long svn_repos_load_fs(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_load_fs");
long svn_repos_load_fs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_load_fs"); return 0; }

long svn_repos_load_fs2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_load_fs2");
long svn_repos_load_fs2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_load_fs2"); return 0; }

long svn_repos_lock_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_lock_dir");
long svn_repos_lock_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_lock_dir"); return 0; }

long svn_repos_node_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_node_editor");
long svn_repos_node_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_node_editor"); return 0; }

long svn_repos_node_from_baton(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_node_from_baton");
long svn_repos_node_from_baton(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_node_from_baton"); return 0; }

long svn_repos_node_location_segments(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_node_location_segments");
long svn_repos_node_location_segments(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_node_location_segments"); return 0; }

long svn_repos_open(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_open");
long svn_repos_open(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_open"); return 0; }

long svn_repos_parse_dumpstream(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_parse_dumpstream");
long svn_repos_parse_dumpstream(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_parse_dumpstream"); return 0; }

long svn_repos_parse_dumpstream2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_parse_dumpstream2");
long svn_repos_parse_dumpstream2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_parse_dumpstream2"); return 0; }

long svn_repos_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_path");
long svn_repos_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_path"); return 0; }

long svn_repos_post_commit_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_post_commit_hook");
long svn_repos_post_commit_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_post_commit_hook"); return 0; }

long svn_repos_post_lock_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_post_lock_hook");
long svn_repos_post_lock_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_post_lock_hook"); return 0; }

long svn_repos_post_revprop_change_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_post_revprop_change_hook");
long svn_repos_post_revprop_change_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_post_revprop_change_hook"); return 0; }

long svn_repos_post_unlock_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_post_unlock_hook");
long svn_repos_post_unlock_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_post_unlock_hook"); return 0; }

long svn_repos_pre_commit_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_pre_commit_hook");
long svn_repos_pre_commit_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_pre_commit_hook"); return 0; }

long svn_repos_pre_lock_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_pre_lock_hook");
long svn_repos_pre_lock_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_pre_lock_hook"); return 0; }

long svn_repos_pre_revprop_change_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_pre_revprop_change_hook");
long svn_repos_pre_revprop_change_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_pre_revprop_change_hook"); return 0; }

long svn_repos_pre_unlock_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_pre_unlock_hook");
long svn_repos_pre_unlock_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_pre_unlock_hook"); return 0; }

long svn_repos_recover(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_recover");
long svn_repos_recover(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_recover"); return 0; }

long svn_repos_recover2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_recover2");
long svn_repos_recover2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_recover2"); return 0; }

long svn_repos_recover3(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_recover3");
long svn_repos_recover3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_recover3"); return 0; }

long svn_repos_remember_client_capabilities(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_remember_client_capabilities");
long svn_repos_remember_client_capabilities(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_remember_client_capabilities"); return 0; }

long svn_repos_replay(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_replay");
long svn_repos_replay(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_replay"); return 0; }

long svn_repos_replay2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_replay2");
long svn_repos_replay2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_replay2"); return 0; }

long svn_repos_set_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_set_path");
long svn_repos_set_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_set_path"); return 0; }

long svn_repos_set_path2(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_set_path2");
long svn_repos_set_path2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_set_path2"); return 0; }

long svn_repos_set_path3(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_set_path3");
long svn_repos_set_path3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_set_path3"); return 0; }

long svn_repos_start_commit_hook(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_start_commit_hook");
long svn_repos_start_commit_hook(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_start_commit_hook"); return 0; }

long svn_repos_stat(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_stat");
long svn_repos_stat(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_stat"); return 0; }

long svn_repos_svnserve_conf(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_svnserve_conf");
long svn_repos_svnserve_conf(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_svnserve_conf"); return 0; }

long svn_repos_trace_node_locations(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_trace_node_locations");
long svn_repos_trace_node_locations(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_trace_node_locations"); return 0; }

long svn_repos_upgrade(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_upgrade");
long svn_repos_upgrade(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_upgrade"); return 0; }

long svn_repos_verify_fs(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_verify_fs");
long svn_repos_verify_fs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_verify_fs"); return 0; }

long svn_repos_version(long a, long b, long c_, long d, long e, long f) __asm("_svn_repos_version");
long svn_repos_version(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_repos_version"); return 0; }
