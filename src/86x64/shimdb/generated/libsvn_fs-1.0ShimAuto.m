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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libsvn_fs-1.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long svn_fs_abort_txn(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_abort_txn");
long svn_fs_abort_txn(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_abort_txn"); return 0; }

long svn_fs_access_add_lock_token(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_access_add_lock_token");
long svn_fs_access_add_lock_token(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_access_add_lock_token"); return 0; }

long svn_fs_access_add_lock_token2(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_access_add_lock_token2");
long svn_fs_access_add_lock_token2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_access_add_lock_token2"); return 0; }

long svn_fs_access_get_username(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_access_get_username");
long svn_fs_access_get_username(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_access_get_username"); return 0; }

long svn_fs_apply_text(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_apply_text");
long svn_fs_apply_text(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_apply_text"); return 0; }

long svn_fs_apply_textdelta(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_apply_textdelta");
long svn_fs_apply_textdelta(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_apply_textdelta"); return 0; }

long svn_fs_begin_txn(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_begin_txn");
long svn_fs_begin_txn(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_begin_txn"); return 0; }

long svn_fs_begin_txn2(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_begin_txn2");
long svn_fs_begin_txn2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_begin_txn2"); return 0; }

long svn_fs_berkeley_logfiles(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_berkeley_logfiles");
long svn_fs_berkeley_logfiles(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_berkeley_logfiles"); return 0; }

long svn_fs_berkeley_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_berkeley_path");
long svn_fs_berkeley_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_berkeley_path"); return 0; }

long svn_fs_berkeley_recover(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_berkeley_recover");
long svn_fs_berkeley_recover(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_berkeley_recover"); return 0; }

long svn_fs_change_node_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_change_node_prop");
long svn_fs_change_node_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_change_node_prop"); return 0; }

long svn_fs_change_rev_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_change_rev_prop");
long svn_fs_change_rev_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_change_rev_prop"); return 0; }

long svn_fs_change_txn_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_change_txn_prop");
long svn_fs_change_txn_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_change_txn_prop"); return 0; }

long svn_fs_change_txn_props(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_change_txn_props");
long svn_fs_change_txn_props(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_change_txn_props"); return 0; }

long svn_fs_check_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_check_path");
long svn_fs_check_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_check_path"); return 0; }

long svn_fs_check_related(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_check_related");
long svn_fs_check_related(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_check_related"); return 0; }

long svn_fs_close_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_close_root");
long svn_fs_close_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_close_root"); return 0; }

long svn_fs_closest_copy(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_closest_copy");
long svn_fs_closest_copy(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_closest_copy"); return 0; }

long svn_fs_commit_txn(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_commit_txn");
long svn_fs_commit_txn(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_commit_txn"); return 0; }

long svn_fs_compare_ids(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_compare_ids");
long svn_fs_compare_ids(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_compare_ids"); return 0; }

long svn_fs_contents_changed(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_contents_changed");
long svn_fs_contents_changed(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_contents_changed"); return 0; }

long svn_fs_copied_from(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_copied_from");
long svn_fs_copied_from(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_copied_from"); return 0; }

long svn_fs_copy(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_copy");
long svn_fs_copy(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_copy"); return 0; }

long svn_fs_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_create");
long svn_fs_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_create"); return 0; }

long svn_fs_create_access(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_create_access");
long svn_fs_create_access(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_create_access"); return 0; }

long svn_fs_create_berkeley(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_create_berkeley");
long svn_fs_create_berkeley(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_create_berkeley"); return 0; }

long svn_fs_delete(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_delete");
long svn_fs_delete(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_delete"); return 0; }

long svn_fs_delete_berkeley(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_delete_berkeley");
long svn_fs_delete_berkeley(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_delete_berkeley"); return 0; }

long svn_fs_delete_fs(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_delete_fs");
long svn_fs_delete_fs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_delete_fs"); return 0; }

long svn_fs_deltify_revision(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_deltify_revision");
long svn_fs_deltify_revision(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_deltify_revision"); return 0; }

long svn_fs_dir_entries(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_dir_entries");
long svn_fs_dir_entries(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_dir_entries"); return 0; }

long svn_fs_file_checksum(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_file_checksum");
long svn_fs_file_checksum(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_file_checksum"); return 0; }

long svn_fs_file_contents(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_file_contents");
long svn_fs_file_contents(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_file_contents"); return 0; }

long svn_fs_file_length(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_file_length");
long svn_fs_file_length(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_file_length"); return 0; }

long svn_fs_file_md5_checksum(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_file_md5_checksum");
long svn_fs_file_md5_checksum(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_file_md5_checksum"); return 0; }

long svn_fs_generate_lock_token(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_generate_lock_token");
long svn_fs_generate_lock_token(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_generate_lock_token"); return 0; }

long svn_fs_get_access(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_get_access");
long svn_fs_get_access(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_get_access"); return 0; }

long svn_fs_get_file_delta_stream(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_get_file_delta_stream");
long svn_fs_get_file_delta_stream(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_get_file_delta_stream"); return 0; }

long svn_fs_get_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_get_lock");
long svn_fs_get_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_get_lock"); return 0; }

long svn_fs_get_locks(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_get_locks");
long svn_fs_get_locks(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_get_locks"); return 0; }

long svn_fs_get_mergeinfo(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_get_mergeinfo");
long svn_fs_get_mergeinfo(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_get_mergeinfo"); return 0; }

long svn_fs_get_uuid(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_get_uuid");
long svn_fs_get_uuid(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_get_uuid"); return 0; }

long svn_fs_history_location(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_history_location");
long svn_fs_history_location(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_history_location"); return 0; }

long svn_fs_history_prev(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_history_prev");
long svn_fs_history_prev(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_history_prev"); return 0; }

long svn_fs_hotcopy(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_hotcopy");
long svn_fs_hotcopy(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_hotcopy"); return 0; }

long svn_fs_hotcopy_berkeley(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_hotcopy_berkeley");
long svn_fs_hotcopy_berkeley(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_hotcopy_berkeley"); return 0; }

long svn_fs_initialize(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_initialize");
long svn_fs_initialize(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_initialize"); return 0; }

long svn_fs_is_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_is_dir");
long svn_fs_is_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_is_dir"); return 0; }

long svn_fs_is_file(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_is_file");
long svn_fs_is_file(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_is_file"); return 0; }

long svn_fs_is_revision_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_is_revision_root");
long svn_fs_is_revision_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_is_revision_root"); return 0; }

long svn_fs_is_txn_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_is_txn_root");
long svn_fs_is_txn_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_is_txn_root"); return 0; }

long svn_fs_list_transactions(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_list_transactions");
long svn_fs_list_transactions(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_list_transactions"); return 0; }

long svn_fs_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_lock");
long svn_fs_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_lock"); return 0; }

long svn_fs_make_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_make_dir");
long svn_fs_make_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_make_dir"); return 0; }

long svn_fs_make_file(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_make_file");
long svn_fs_make_file(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_make_file"); return 0; }

long svn_fs_merge(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_merge");
long svn_fs_merge(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_merge"); return 0; }

long svn_fs_new(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_new");
long svn_fs_new(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_new"); return 0; }

long svn_fs_node_created_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_node_created_path");
long svn_fs_node_created_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_node_created_path"); return 0; }

long svn_fs_node_created_rev(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_node_created_rev");
long svn_fs_node_created_rev(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_node_created_rev"); return 0; }

long svn_fs_node_history(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_node_history");
long svn_fs_node_history(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_node_history"); return 0; }

long svn_fs_node_id(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_node_id");
long svn_fs_node_id(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_node_id"); return 0; }

long svn_fs_node_origin_rev(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_node_origin_rev");
long svn_fs_node_origin_rev(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_node_origin_rev"); return 0; }

long svn_fs_node_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_node_prop");
long svn_fs_node_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_node_prop"); return 0; }

long svn_fs_node_proplist(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_node_proplist");
long svn_fs_node_proplist(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_node_proplist"); return 0; }

long svn_fs_open(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_open");
long svn_fs_open(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_open"); return 0; }

long svn_fs_open_berkeley(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_open_berkeley");
long svn_fs_open_berkeley(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_open_berkeley"); return 0; }

long svn_fs_open_txn(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_open_txn");
long svn_fs_open_txn(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_open_txn"); return 0; }

long svn_fs_pack(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_pack");
long svn_fs_pack(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_pack"); return 0; }

long svn_fs_parse_id(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_parse_id");
long svn_fs_parse_id(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_parse_id"); return 0; }

long svn_fs_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_path");
long svn_fs_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_path"); return 0; }

long svn_fs_path_change2_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_path_change2_create");
long svn_fs_path_change2_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_path_change2_create"); return 0; }

long svn_fs_paths_changed(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_paths_changed");
long svn_fs_paths_changed(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_paths_changed"); return 0; }

long svn_fs_paths_changed2(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_paths_changed2");
long svn_fs_paths_changed2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_paths_changed2"); return 0; }

long svn_fs_print_modules(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_print_modules");
long svn_fs_print_modules(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_print_modules"); return 0; }

long svn_fs_props_changed(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_props_changed");
long svn_fs_props_changed(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_props_changed"); return 0; }

long svn_fs_purge_txn(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_purge_txn");
long svn_fs_purge_txn(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_purge_txn"); return 0; }

long svn_fs_recover(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_recover");
long svn_fs_recover(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_recover"); return 0; }

long svn_fs_revision_link(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_revision_link");
long svn_fs_revision_link(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_revision_link"); return 0; }

long svn_fs_revision_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_revision_prop");
long svn_fs_revision_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_revision_prop"); return 0; }

long svn_fs_revision_proplist(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_revision_proplist");
long svn_fs_revision_proplist(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_revision_proplist"); return 0; }

long svn_fs_revision_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_revision_root");
long svn_fs_revision_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_revision_root"); return 0; }

long svn_fs_revision_root_revision(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_revision_root_revision");
long svn_fs_revision_root_revision(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_revision_root_revision"); return 0; }

long svn_fs_root_fs(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_root_fs");
long svn_fs_root_fs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_root_fs"); return 0; }

long svn_fs_set_access(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_set_access");
long svn_fs_set_access(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_set_access"); return 0; }

long svn_fs_set_uuid(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_set_uuid");
long svn_fs_set_uuid(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_set_uuid"); return 0; }

long svn_fs_set_warning_func(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_set_warning_func");
long svn_fs_set_warning_func(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_set_warning_func"); return 0; }

long svn_fs_txn_base_revision(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_txn_base_revision");
long svn_fs_txn_base_revision(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_txn_base_revision"); return 0; }

long svn_fs_txn_name(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_txn_name");
long svn_fs_txn_name(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_txn_name"); return 0; }

long svn_fs_txn_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_txn_prop");
long svn_fs_txn_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_txn_prop"); return 0; }

long svn_fs_txn_proplist(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_txn_proplist");
long svn_fs_txn_proplist(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_txn_proplist"); return 0; }

long svn_fs_txn_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_txn_root");
long svn_fs_txn_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_txn_root"); return 0; }

long svn_fs_txn_root_base_revision(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_txn_root_base_revision");
long svn_fs_txn_root_base_revision(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_txn_root_base_revision"); return 0; }

long svn_fs_txn_root_name(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_txn_root_name");
long svn_fs_txn_root_name(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_txn_root_name"); return 0; }

long svn_fs_type(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_type");
long svn_fs_type(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_type"); return 0; }

long svn_fs_unlock(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_unlock");
long svn_fs_unlock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_unlock"); return 0; }

long svn_fs_unparse_id(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_unparse_id");
long svn_fs_unparse_id(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_unparse_id"); return 0; }

long svn_fs_upgrade(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_upgrade");
long svn_fs_upgrade(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_upgrade"); return 0; }

long svn_fs_version(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_version");
long svn_fs_version(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_version"); return 0; }

long svn_fs_youngest_rev(long a, long b, long c_, long d, long e, long f) __asm("_svn_fs_youngest_rev");
long svn_fs_youngest_rev(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_fs_youngest_rev"); return 0; }
