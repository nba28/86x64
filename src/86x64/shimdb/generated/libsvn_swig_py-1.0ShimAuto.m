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
  fprintf(stderr, "[shimauto:%s] %s: removed-OS symbol CALLED with no curated impl -- returning 0 (write one in shimdb/impl/ if its behavior matters)\n", "libsvn_swig_py-1.0", sym);
  if (getenv("SHIMGEN_STUB_ABORT")) abort();
  pthread_mutex_unlock(&mtx);
}

long svn_swig_ConvertPtr(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_ConvertPtr");
long svn_swig_ConvertPtr(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_ConvertPtr"); return 0; }

long svn_swig_MustGetPtr(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_MustGetPtr");
long svn_swig_MustGetPtr(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_MustGetPtr"); return 0; }

long svn_swig_NewPointerObj(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_NewPointerObj");
long svn_swig_NewPointerObj(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_NewPointerObj"); return 0; }

long svn_swig_py_acquire_py_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_acquire_py_lock");
long svn_swig_py_acquire_py_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_acquire_py_lock"); return 0; }

long svn_swig_py_array_to_list(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_array_to_list");
long svn_swig_py_array_to_list(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_array_to_list"); return 0; }

long svn_swig_py_changed_path_hash_from_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_changed_path_hash_from_dict");
long svn_swig_py_changed_path_hash_from_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_changed_path_hash_from_dict"); return 0; }

long svn_swig_py_changed_path_hash_to_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_changed_path_hash_to_dict");
long svn_swig_py_changed_path_hash_to_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_changed_path_hash_to_dict"); return 0; }

long svn_swig_py_clear_application_pool(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_clear_application_pool");
long svn_swig_py_clear_application_pool(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_clear_application_pool"); return 0; }

long svn_swig_py_convert_hash(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_convert_hash");
long svn_swig_py_convert_hash(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_convert_hash"); return 0; }

long svn_swig_py_get_parent_pool(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_get_parent_pool");
long svn_swig_py_get_parent_pool(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_get_parent_pool"); return 0; }

long svn_swig_py_get_pool_arg(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_get_pool_arg");
long svn_swig_py_get_pool_arg(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_get_pool_arg"); return 0; }

long svn_swig_py_initialize(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_initialize");
long svn_swig_py_initialize(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_initialize"); return 0; }

long svn_swig_py_locationhash_to_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_locationhash_to_dict");
long svn_swig_py_locationhash_to_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_locationhash_to_dict"); return 0; }

long svn_swig_py_make_editor(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_make_editor");
long svn_swig_py_make_editor(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_make_editor"); return 0; }

long svn_swig_py_make_file(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_make_file");
long svn_swig_py_make_file(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_make_file"); return 0; }

long svn_swig_py_make_stream(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_make_stream");
long svn_swig_py_make_stream(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_make_stream"); return 0; }

long svn_swig_py_mergeinfo_catalog_to_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_mergeinfo_catalog_to_dict");
long svn_swig_py_mergeinfo_catalog_to_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_mergeinfo_catalog_to_dict"); return 0; }

long svn_swig_py_mergeinfo_from_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_mergeinfo_from_dict");
long svn_swig_py_mergeinfo_from_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_mergeinfo_from_dict"); return 0; }

long svn_swig_py_mergeinfo_to_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_mergeinfo_to_dict");
long svn_swig_py_mergeinfo_to_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_mergeinfo_to_dict"); return 0; }

long svn_swig_py_path_revs_hash_from_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_path_revs_hash_from_dict");
long svn_swig_py_path_revs_hash_from_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_path_revs_hash_from_dict"); return 0; }

long svn_swig_py_proparray_from_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_proparray_from_dict");
long svn_swig_py_proparray_from_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_proparray_from_dict"); return 0; }

long svn_swig_py_proparray_to_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_proparray_to_dict");
long svn_swig_py_proparray_to_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_proparray_to_dict"); return 0; }

long svn_swig_py_prophash_from_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_prophash_from_dict");
long svn_swig_py_prophash_from_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_prophash_from_dict"); return 0; }

long svn_swig_py_prophash_to_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_prophash_to_dict");
long svn_swig_py_prophash_to_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_prophash_to_dict"); return 0; }

long svn_swig_py_rangelist_to_array(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_rangelist_to_array");
long svn_swig_py_rangelist_to_array(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_rangelist_to_array"); return 0; }

long svn_swig_py_rangelist_to_list(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_rangelist_to_list");
long svn_swig_py_rangelist_to_list(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_rangelist_to_list"); return 0; }

long svn_swig_py_release_py_lock(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_release_py_lock");
long svn_swig_py_release_py_lock(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_release_py_lock"); return 0; }

long svn_swig_py_revarray_to_list(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_revarray_to_list");
long svn_swig_py_revarray_to_list(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_revarray_to_list"); return 0; }

long svn_swig_py_revnums_to_array(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_revnums_to_array");
long svn_swig_py_revnums_to_array(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_revnums_to_array"); return 0; }

long svn_swig_py_set_application_pool(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_set_application_pool");
long svn_swig_py_set_application_pool(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_set_application_pool"); return 0; }

long svn_swig_py_setup_ra_callbacks(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_setup_ra_callbacks");
long svn_swig_py_setup_ra_callbacks(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_setup_ra_callbacks"); return 0; }

long svn_swig_py_setup_wc_diff_callbacks2(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_setup_wc_diff_callbacks2");
long svn_swig_py_setup_wc_diff_callbacks2(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_setup_wc_diff_callbacks2"); return 0; }

long svn_swig_py_stringhash_from_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_stringhash_from_dict");
long svn_swig_py_stringhash_from_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_stringhash_from_dict"); return 0; }

long svn_swig_py_stringhash_to_dict(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_stringhash_to_dict");
long svn_swig_py_stringhash_to_dict(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_stringhash_to_dict"); return 0; }

long svn_swig_py_strings_to_array(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_strings_to_array");
long svn_swig_py_strings_to_array(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_strings_to_array"); return 0; }

long svn_swig_py_svn_exception(long a, long b, long c_, long d, long e, long f) __asm("_svn_swig_py_svn_exception");
long svn_swig_py_svn_exception(long a, long b, long c_, long d, long e, long f) { shim_note("_svn_swig_py_svn_exception"); return 0; }
