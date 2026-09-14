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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libsvn_wc-1.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long svn_wc_add(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_add");
long svn_wc_add(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_add"); return 0; }

long svn_wc_add2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_add2");
long svn_wc_add2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_add2"); return 0; }

long svn_wc_add3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_add3");
long svn_wc_add3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_add3"); return 0; }

long svn_wc_add_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_add_lock");
long svn_wc_add_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_add_lock"); return 0; }

long svn_wc_add_repos_file(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_add_repos_file");
long svn_wc_add_repos_file(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_add_repos_file"); return 0; }

long svn_wc_add_repos_file2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_add_repos_file2");
long svn_wc_add_repos_file2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_add_repos_file2"); return 0; }

long svn_wc_add_repos_file3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_add_repos_file3");
long svn_wc_add_repos_file3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_add_repos_file3"); return 0; }

long svn_wc_adm_access_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_access_path");
long svn_wc_adm_access_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_access_path"); return 0; }

long svn_wc_adm_access_pool(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_access_pool");
long svn_wc_adm_access_pool(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_access_pool"); return 0; }

long svn_wc_adm_close(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_close");
long svn_wc_adm_close(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_close"); return 0; }

long svn_wc_adm_close2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_close2");
long svn_wc_adm_close2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_close2"); return 0; }

long svn_wc_adm_locked(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_locked");
long svn_wc_adm_locked(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_locked"); return 0; }

long svn_wc_adm_open(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_open");
long svn_wc_adm_open(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_open"); return 0; }

long svn_wc_adm_open2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_open2");
long svn_wc_adm_open2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_open2"); return 0; }

long svn_wc_adm_open3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_open3");
long svn_wc_adm_open3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_open3"); return 0; }

long svn_wc_adm_open_anchor(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_open_anchor");
long svn_wc_adm_open_anchor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_open_anchor"); return 0; }

long svn_wc_adm_probe_open(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_probe_open");
long svn_wc_adm_probe_open(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_probe_open"); return 0; }

long svn_wc_adm_probe_open2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_probe_open2");
long svn_wc_adm_probe_open2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_probe_open2"); return 0; }

long svn_wc_adm_probe_open3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_probe_open3");
long svn_wc_adm_probe_open3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_probe_open3"); return 0; }

long svn_wc_adm_probe_retrieve(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_probe_retrieve");
long svn_wc_adm_probe_retrieve(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_probe_retrieve"); return 0; }

long svn_wc_adm_probe_try(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_probe_try");
long svn_wc_adm_probe_try(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_probe_try"); return 0; }

long svn_wc_adm_probe_try2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_probe_try2");
long svn_wc_adm_probe_try2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_probe_try2"); return 0; }

long svn_wc_adm_probe_try3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_probe_try3");
long svn_wc_adm_probe_try3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_probe_try3"); return 0; }

long svn_wc_adm_retrieve(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_adm_retrieve");
long svn_wc_adm_retrieve(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_adm_retrieve"); return 0; }

long svn_wc_canonicalize_svn_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_canonicalize_svn_prop");
long svn_wc_canonicalize_svn_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_canonicalize_svn_prop"); return 0; }

long svn_wc_check_wc(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_check_wc");
long svn_wc_check_wc(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_check_wc"); return 0; }

long svn_wc_cleanup(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_cleanup");
long svn_wc_cleanup(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_cleanup"); return 0; }

long svn_wc_cleanup2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_cleanup2");
long svn_wc_cleanup2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_cleanup2"); return 0; }

long svn_wc_committed_queue_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_committed_queue_create");
long svn_wc_committed_queue_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_committed_queue_create"); return 0; }

long svn_wc_conflict_description_create_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_conflict_description_create_prop");
long svn_wc_conflict_description_create_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_conflict_description_create_prop"); return 0; }

long svn_wc_conflict_description_create_text(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_conflict_description_create_text");
long svn_wc_conflict_description_create_text(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_conflict_description_create_text"); return 0; }

long svn_wc_conflict_description_create_tree(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_conflict_description_create_tree");
long svn_wc_conflict_description_create_tree(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_conflict_description_create_tree"); return 0; }

long svn_wc_conflict_version_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_conflict_version_create");
long svn_wc_conflict_version_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_conflict_version_create"); return 0; }

long svn_wc_conflict_version_dup(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_conflict_version_dup");
long svn_wc_conflict_version_dup(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_conflict_version_dup"); return 0; }

long svn_wc_conflicted_p(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_conflicted_p");
long svn_wc_conflicted_p(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_conflicted_p"); return 0; }

long svn_wc_conflicted_p2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_conflicted_p2");
long svn_wc_conflicted_p2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_conflicted_p2"); return 0; }

long svn_wc_copy(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_copy");
long svn_wc_copy(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_copy"); return 0; }

long svn_wc_copy2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_copy2");
long svn_wc_copy2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_copy2"); return 0; }

long svn_wc_crawl_revisions(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_crawl_revisions");
long svn_wc_crawl_revisions(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_crawl_revisions"); return 0; }

long svn_wc_crawl_revisions2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_crawl_revisions2");
long svn_wc_crawl_revisions2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_crawl_revisions2"); return 0; }

long svn_wc_crawl_revisions3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_crawl_revisions3");
long svn_wc_crawl_revisions3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_crawl_revisions3"); return 0; }

long svn_wc_crawl_revisions4(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_crawl_revisions4");
long svn_wc_crawl_revisions4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_crawl_revisions4"); return 0; }

long svn_wc_create_conflict_result(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_create_conflict_result");
long svn_wc_create_conflict_result(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_create_conflict_result"); return 0; }

long svn_wc_create_notify(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_create_notify");
long svn_wc_create_notify(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_create_notify"); return 0; }

long svn_wc_create_notify_url(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_create_notify_url");
long svn_wc_create_notify_url(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_create_notify_url"); return 0; }

long svn_wc_create_tmp_file(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_create_tmp_file");
long svn_wc_create_tmp_file(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_create_tmp_file"); return 0; }

long svn_wc_create_tmp_file2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_create_tmp_file2");
long svn_wc_create_tmp_file2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_create_tmp_file2"); return 0; }

long svn_wc_crop_tree(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_crop_tree");
long svn_wc_crop_tree(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_crop_tree"); return 0; }

long svn_wc_delete(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_delete");
long svn_wc_delete(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_delete"); return 0; }

long svn_wc_delete2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_delete2");
long svn_wc_delete2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_delete2"); return 0; }

long svn_wc_delete3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_delete3");
long svn_wc_delete3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_delete3"); return 0; }

long svn_wc_diff(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_diff");
long svn_wc_diff(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_diff"); return 0; }

long svn_wc_diff2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_diff2");
long svn_wc_diff2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_diff2"); return 0; }

long svn_wc_diff3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_diff3");
long svn_wc_diff3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_diff3"); return 0; }

long svn_wc_diff4(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_diff4");
long svn_wc_diff4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_diff4"); return 0; }

long svn_wc_diff5(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_diff5");
long svn_wc_diff5(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_diff5"); return 0; }

long svn_wc_dup_notify(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_dup_notify");
long svn_wc_dup_notify(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_dup_notify"); return 0; }

long svn_wc_dup_status(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_dup_status");
long svn_wc_dup_status(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_dup_status"); return 0; }

long svn_wc_dup_status2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_dup_status2");
long svn_wc_dup_status2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_dup_status2"); return 0; }

long svn_wc_edited_externals(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_edited_externals");
long svn_wc_edited_externals(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_edited_externals"); return 0; }

long svn_wc_ensure_adm(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_ensure_adm");
long svn_wc_ensure_adm(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_ensure_adm"); return 0; }

long svn_wc_ensure_adm2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_ensure_adm2");
long svn_wc_ensure_adm2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_ensure_adm2"); return 0; }

long svn_wc_ensure_adm3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_ensure_adm3");
long svn_wc_ensure_adm3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_ensure_adm3"); return 0; }

long svn_wc_entries_read(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_entries_read");
long svn_wc_entries_read(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_entries_read"); return 0; }

long svn_wc_entry(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_entry");
long svn_wc_entry(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_entry"); return 0; }

long svn_wc_entry_dup(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_entry_dup");
long svn_wc_entry_dup(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_entry_dup"); return 0; }

long svn_wc_external_item2_dup(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_external_item2_dup");
long svn_wc_external_item2_dup(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_external_item2_dup"); return 0; }

long svn_wc_external_item_create(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_external_item_create");
long svn_wc_external_item_create(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_external_item_create"); return 0; }

long svn_wc_external_item_dup(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_external_item_dup");
long svn_wc_external_item_dup(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_external_item_dup"); return 0; }

long svn_wc_get_actual_target(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_actual_target");
long svn_wc_get_actual_target(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_actual_target"); return 0; }

long svn_wc_get_adm_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_adm_dir");
long svn_wc_get_adm_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_adm_dir"); return 0; }

long svn_wc_get_ancestry(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_ancestry");
long svn_wc_get_ancestry(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_ancestry"); return 0; }

long svn_wc_get_default_ignores(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_default_ignores");
long svn_wc_get_default_ignores(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_default_ignores"); return 0; }

long svn_wc_get_diff_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_diff_editor");
long svn_wc_get_diff_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_diff_editor"); return 0; }

long svn_wc_get_diff_editor2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_diff_editor2");
long svn_wc_get_diff_editor2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_diff_editor2"); return 0; }

long svn_wc_get_diff_editor3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_diff_editor3");
long svn_wc_get_diff_editor3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_diff_editor3"); return 0; }

long svn_wc_get_diff_editor4(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_diff_editor4");
long svn_wc_get_diff_editor4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_diff_editor4"); return 0; }

long svn_wc_get_diff_editor5(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_diff_editor5");
long svn_wc_get_diff_editor5(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_diff_editor5"); return 0; }

long svn_wc_get_ignores(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_ignores");
long svn_wc_get_ignores(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_ignores"); return 0; }

long svn_wc_get_pristine_contents(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_pristine_contents");
long svn_wc_get_pristine_contents(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_pristine_contents"); return 0; }

long svn_wc_get_pristine_copy_path(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_pristine_copy_path");
long svn_wc_get_pristine_copy_path(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_pristine_copy_path"); return 0; }

long svn_wc_get_prop_diffs(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_prop_diffs");
long svn_wc_get_prop_diffs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_prop_diffs"); return 0; }

long svn_wc_get_status_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_status_editor");
long svn_wc_get_status_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_status_editor"); return 0; }

long svn_wc_get_status_editor2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_status_editor2");
long svn_wc_get_status_editor2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_status_editor2"); return 0; }

long svn_wc_get_status_editor3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_status_editor3");
long svn_wc_get_status_editor3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_status_editor3"); return 0; }

long svn_wc_get_status_editor4(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_status_editor4");
long svn_wc_get_status_editor4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_status_editor4"); return 0; }

long svn_wc_get_switch_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_switch_editor");
long svn_wc_get_switch_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_switch_editor"); return 0; }

long svn_wc_get_switch_editor2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_switch_editor2");
long svn_wc_get_switch_editor2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_switch_editor2"); return 0; }

long svn_wc_get_switch_editor3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_switch_editor3");
long svn_wc_get_switch_editor3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_switch_editor3"); return 0; }

long svn_wc_get_update_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_update_editor");
long svn_wc_get_update_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_update_editor"); return 0; }

long svn_wc_get_update_editor2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_update_editor2");
long svn_wc_get_update_editor2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_update_editor2"); return 0; }

long svn_wc_get_update_editor3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_get_update_editor3");
long svn_wc_get_update_editor3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_get_update_editor3"); return 0; }

long svn_wc_has_binary_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_has_binary_prop");
long svn_wc_has_binary_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_has_binary_prop"); return 0; }

long svn_wc_init_traversal_info(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_init_traversal_info");
long svn_wc_init_traversal_info(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_init_traversal_info"); return 0; }

long svn_wc_is_adm_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_is_adm_dir");
long svn_wc_is_adm_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_is_adm_dir"); return 0; }

long svn_wc_is_entry_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_is_entry_prop");
long svn_wc_is_entry_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_is_entry_prop"); return 0; }

long svn_wc_is_normal_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_is_normal_prop");
long svn_wc_is_normal_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_is_normal_prop"); return 0; }

long svn_wc_is_wc_prop(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_is_wc_prop");
long svn_wc_is_wc_prop(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_is_wc_prop"); return 0; }

long svn_wc_is_wc_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_is_wc_root");
long svn_wc_is_wc_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_is_wc_root"); return 0; }

long svn_wc_locked(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_locked");
long svn_wc_locked(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_locked"); return 0; }

long svn_wc_mark_missing_deleted(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_mark_missing_deleted");
long svn_wc_mark_missing_deleted(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_mark_missing_deleted"); return 0; }

long svn_wc_match_ignore_list(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_match_ignore_list");
long svn_wc_match_ignore_list(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_match_ignore_list"); return 0; }

long svn_wc_maybe_set_repos_root(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_maybe_set_repos_root");
long svn_wc_maybe_set_repos_root(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_maybe_set_repos_root"); return 0; }

long svn_wc_merge(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_merge");
long svn_wc_merge(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_merge"); return 0; }

long svn_wc_merge2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_merge2");
long svn_wc_merge2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_merge2"); return 0; }

long svn_wc_merge3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_merge3");
long svn_wc_merge3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_merge3"); return 0; }

long svn_wc_merge_prop_diffs(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_merge_prop_diffs");
long svn_wc_merge_prop_diffs(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_merge_prop_diffs"); return 0; }

long svn_wc_merge_props(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_merge_props");
long svn_wc_merge_props(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_merge_props"); return 0; }

long svn_wc_merge_props2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_merge_props2");
long svn_wc_merge_props2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_merge_props2"); return 0; }

long svn_wc_parse_externals_description(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_parse_externals_description");
long svn_wc_parse_externals_description(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_parse_externals_description"); return 0; }

long svn_wc_parse_externals_description2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_parse_externals_description2");
long svn_wc_parse_externals_description2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_parse_externals_description2"); return 0; }

long svn_wc_parse_externals_description3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_parse_externals_description3");
long svn_wc_parse_externals_description3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_parse_externals_description3"); return 0; }

long svn_wc_process_committed(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_process_committed");
long svn_wc_process_committed(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_process_committed"); return 0; }

long svn_wc_process_committed2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_process_committed2");
long svn_wc_process_committed2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_process_committed2"); return 0; }

long svn_wc_process_committed3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_process_committed3");
long svn_wc_process_committed3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_process_committed3"); return 0; }

long svn_wc_process_committed4(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_process_committed4");
long svn_wc_process_committed4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_process_committed4"); return 0; }

long svn_wc_process_committed_queue(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_process_committed_queue");
long svn_wc_process_committed_queue(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_process_committed_queue"); return 0; }

long svn_wc_prop_get(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_prop_get");
long svn_wc_prop_get(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_prop_get"); return 0; }

long svn_wc_prop_list(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_prop_list");
long svn_wc_prop_list(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_prop_list"); return 0; }

long svn_wc_prop_set(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_prop_set");
long svn_wc_prop_set(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_prop_set"); return 0; }

long svn_wc_prop_set2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_prop_set2");
long svn_wc_prop_set2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_prop_set2"); return 0; }

long svn_wc_prop_set3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_prop_set3");
long svn_wc_prop_set3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_prop_set3"); return 0; }

long svn_wc_props_modified_p(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_props_modified_p");
long svn_wc_props_modified_p(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_props_modified_p"); return 0; }

long svn_wc_queue_committed(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_queue_committed");
long svn_wc_queue_committed(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_queue_committed"); return 0; }

long svn_wc_queue_committed2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_queue_committed2");
long svn_wc_queue_committed2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_queue_committed2"); return 0; }

long svn_wc_relocate(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_relocate");
long svn_wc_relocate(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_relocate"); return 0; }

long svn_wc_relocate2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_relocate2");
long svn_wc_relocate2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_relocate2"); return 0; }

long svn_wc_relocate3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_relocate3");
long svn_wc_relocate3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_relocate3"); return 0; }

long svn_wc_remove_from_revision_control(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_remove_from_revision_control");
long svn_wc_remove_from_revision_control(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_remove_from_revision_control"); return 0; }

long svn_wc_remove_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_remove_lock");
long svn_wc_remove_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_remove_lock"); return 0; }

long svn_wc_resolved_conflict(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_resolved_conflict");
long svn_wc_resolved_conflict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_resolved_conflict"); return 0; }

long svn_wc_resolved_conflict2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_resolved_conflict2");
long svn_wc_resolved_conflict2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_resolved_conflict2"); return 0; }

long svn_wc_resolved_conflict3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_resolved_conflict3");
long svn_wc_resolved_conflict3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_resolved_conflict3"); return 0; }

long svn_wc_resolved_conflict4(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_resolved_conflict4");
long svn_wc_resolved_conflict4(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_resolved_conflict4"); return 0; }

long svn_wc_revert(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_revert");
long svn_wc_revert(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_revert"); return 0; }

long svn_wc_revert2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_revert2");
long svn_wc_revert2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_revert2"); return 0; }

long svn_wc_revert3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_revert3");
long svn_wc_revert3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_revert3"); return 0; }

long svn_wc_revision_status(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_revision_status");
long svn_wc_revision_status(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_revision_status"); return 0; }

long svn_wc_set_adm_dir(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_set_adm_dir");
long svn_wc_set_adm_dir(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_set_adm_dir"); return 0; }

long svn_wc_set_changelist(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_set_changelist");
long svn_wc_set_changelist(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_set_changelist"); return 0; }

long svn_wc_status(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_status");
long svn_wc_status(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_status"); return 0; }

long svn_wc_status2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_status2");
long svn_wc_status2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_status2"); return 0; }

long svn_wc_status_set_repos_locks(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_status_set_repos_locks");
long svn_wc_status_set_repos_locks(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_status_set_repos_locks"); return 0; }

long svn_wc_text_modified_p(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_text_modified_p");
long svn_wc_text_modified_p(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_text_modified_p"); return 0; }

long svn_wc_translated_file(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_translated_file");
long svn_wc_translated_file(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_translated_file"); return 0; }

long svn_wc_translated_file2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_translated_file2");
long svn_wc_translated_file2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_translated_file2"); return 0; }

long svn_wc_translated_stream(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_translated_stream");
long svn_wc_translated_stream(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_translated_stream"); return 0; }

long svn_wc_transmit_prop_deltas(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_transmit_prop_deltas");
long svn_wc_transmit_prop_deltas(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_transmit_prop_deltas"); return 0; }

long svn_wc_transmit_text_deltas(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_transmit_text_deltas");
long svn_wc_transmit_text_deltas(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_transmit_text_deltas"); return 0; }

long svn_wc_transmit_text_deltas2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_transmit_text_deltas2");
long svn_wc_transmit_text_deltas2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_transmit_text_deltas2"); return 0; }

long svn_wc_traversed_depths(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_traversed_depths");
long svn_wc_traversed_depths(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_traversed_depths"); return 0; }

long svn_wc_version(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_version");
long svn_wc_version(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_version"); return 0; }

long svn_wc_walk_entries(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_walk_entries");
long svn_wc_walk_entries(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_walk_entries"); return 0; }

long svn_wc_walk_entries2(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_walk_entries2");
long svn_wc_walk_entries2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_walk_entries2"); return 0; }

long svn_wc_walk_entries3(long a, long b, long c_, long d, long e, long f) __asm("_svn_wc_walk_entries3");
long svn_wc_walk_entries3(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_wc_walk_entries3"); return 0; }
