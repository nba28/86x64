#!/bin/bash
#
# native-free — two-arm guard for libabiconv's malloc shim (malloc_shim.c)
# handing NATIVE allocations back to their malloc zone.
#
# Inside libabiconv, free()/realloc() ARE the i386 low-heap shim. free()
# ignored every pointer it did not own, so each native buffer freed through it
# (the runtime's own method_copyArgumentType / objc_copyClassList results, and
# every malloc'd buffer a native API hands a translated app) leaked; realloc()
# of a native buffer returned a fresh block WITHOUT its contents.
#
# ARMS (native x86_64 harness, dlopen of the REAL libabiconv):
#   ON  : 500 x 1 MB native buffers freed through the shim -> footprint flat;
#         a realloc'd native buffer keeps its bytes
#   OFF : M64_NO_NATIVE_FREE=1 -> the 500 MB leaks, the bytes are lost
set -u
cd "$(dirname "$0")"
LIBABICONV="$(cd .. && pwd)/build/src/abiconv/libabiconv.dylib"
[ -f "$LIBABICONV" ] || { echo "FAIL native-free (libabiconv not built)"; exit 1; }
TMP="$(mktemp -d)"; trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/h.c" <<'C'
#include <dlfcn.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static long footprint_mb(void) {
   task_vm_info_data_t v; mach_msg_type_number_t n = TASK_VM_INFO_COUNT;
   task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&v, &n);
   return (long)(v.phys_footprint >> 20);
}
int main(int argc, char **argv) {
   void *h = dlopen(argv[1], RTLD_LAZY);
   if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
   void (*shim_free)(void *) = (void (*)(void *))dlsym(h, "free");
   void *(*shim_realloc)(void *, size_t) = (void *(*)(void *, size_t))dlsym(h, "realloc");
   long before = footprint_mb();
   for (int i = 0; i < 500; ++i) {
      char *p = malloc(1 << 20);          /* the harness's malloc: libSystem */
      memset(p, i, 1 << 20);
      shim_free(p);
   }
   long grew = footprint_mb() - before;
   char *q = malloc(64);
   strcpy(q, "native bytes survive realloc");
   char *r = shim_realloc(q, 4096);
   int kept = r && strcmp(r, "native bytes survive realloc") == 0;
   printf("grew_mb=%ld kept=%d\n", grew, kept);
   return 0;
}
C
cc -arch x86_64 -O1 -o "$TMP/h" "$TMP/h.c" -Wl,-pagezero_size,0x1000 || { echo "FAIL native-free (cc)"; exit 1; }
off=$(M64_NO_NATIVE_FREE=1 "$TMP/h" "$LIBABICONV" 2>/dev/null | grep grew_mb)
on=$("$TMP/h" "$LIBABICONV" 2>/dev/null | grep grew_mb)       # ★ON arm last
echo "  ON  : $on"
echo "  OFF : $off"
fail=0
g_on=$(sed 's/.*grew_mb=\([-0-9]*\).*/\1/' <<<"$on"); g_off=$(sed 's/.*grew_mb=\([-0-9]*\).*/\1/' <<<"$off")
[ -n "$g_on" ] && [ "$g_on" -lt 50 ] && grep -q "kept=1" <<<"$on" || { echo "  ON : native buffers leaked or realloc lost the bytes"; fail=1; }
[ -n "$g_off" ] && [ "$g_off" -gt 300 ] && grep -q "kept=0" <<<"$off" || { echo "  OFF: the defect did not reproduce (guard inert)"; fail=1; }
[ $fail = 0 ] && echo "PASS native-free" || echo "FAIL native-free"
exit $fail
