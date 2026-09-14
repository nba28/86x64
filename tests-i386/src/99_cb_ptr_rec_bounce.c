/* 99_cb_ptr_rec_bounce — a callback argument that POINTS AT A NATIVE RECORD.
 *
 * THE DEFECT. cb_bridge.c marshalled a callback pointer argument as
 *     case CBA_I32: case CBA_PTR: words[w++] = (uint32_t)v;
 * on the reasoning recorded in typeconv.cc: "pointer, truncate (process heap is
 * low-4GB)". That is true of OUR shim heap and false of libSystem's. It holds
 * for the common callback pointer -- a user-data/context word the i386 side
 * supplied earlier and is getting back -- but NOT for a record the NATIVE side
 * allocated and the callback READS.
 *
 * MEASURED, Portal 2 (2026-09-14): Source's `FileSelect(const dirent *)`, the
 * filter it hands to `scandir`. libSystem allocated the dirent at
 * 0x7f95fb809400; truncation gave FileSelect 0x748ea200, and its
 * `strcmp(d->d_name, ".")` took SIGSEGV on an unmapped low address, with rdi =
 * arg0+8 (i386 `struct dirent` puts d_name at offset 8).
 *
 * ⚠CBA_OBJ, one case label away, already handles a >4GB pointer -- but it is
 * NOT the answer here: it yields an opaque arena HANDLE, and this callee
 * DEREFERENCES its argument as a struct rather than passing it back to an API.
 * The cure is a bounce-copy of the pointee into the low-4GB shim heap.
 *
 * THIS TEST uses `scandir` itself, because the bug needs a native API that
 * allocates a record and calls back with it -- exactly what no synthetic
 * fixture can fake (a pointer the test itself allocates is already low, and
 * would pass in both arms).
 *
 * ARMS: ON -> the filter sees a readable dirent and counts "." and "..".
 *       OFF (M64_NO_CB_PTR_BOUNCE=1) -> the truncated pointer is unmapped and
 *       reading d_name faults, or counts nothing.
 *
 * ⚠Every fixture here calls exit(); RETURNING from main crashes a translated
 * program (a separate pre-existing gap, see the known-gaps list).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

static int g_seen_dot, g_seen_dotdot, g_calls;

static int filt(struct dirent *d) {
   ++g_calls;
   /* The deref that the truncated pointer made fatal. */
   if (strcmp(d->d_name, ".") == 0)  { g_seen_dot = 1; }
   if (strcmp(d->d_name, "..") == 0) { g_seen_dotdot = 1; }
   return 0;
}

int main(void) {
   struct dirent **nl = NULL;
   int n = scandir("/usr", &nl, filt, NULL);
   /* `g_calls` and `n` depend on what /usr holds, so they are NOT printed —
    * the oracle must be stable across machines. "." and ".." exist in every
    * directory, and `entered` proves the filter ran at all. */
   (void)n;
   printf("entered=%d dot=%d dotdot=%d\n", g_calls > 0, g_seen_dot, g_seen_dotdot);
   fflush(stdout);
   /* "." and ".." are present in every directory, so both must have been seen
    * through a readable pointee. g_calls alone is not enough: the filter is
    * entered either way, it is the DEREF that the defect breaks. */
   exit((g_seen_dot && g_seen_dotdot) ? 42 : 1);
   return 0;
}
