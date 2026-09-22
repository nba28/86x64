/* 99_libsystem_header_protos — libSystem entry points whose ONLY declaring
 * header is not reached transitively must still get an abigen bridge.
 *
 * abigen emits a bridge only for symbols it has a PROTOTYPE for. The libSystem
 * umbrella in abigen_modern_manifest.py covered <mach/mach.h>, <time.h>, ...,
 * but not <mach-o/dyld.h> or <fnmatch.h>, so _NSGetExecutablePath / _fnmatch
 * stayed bound RAW to native libSystem: i386 stack args, SysV callee.
 *
 * MEASURED, Portal 2 2026-09-22 (libcef, Chromium base::FilePath):
 *     uint32_t size = 0; _NSGetExecutablePath(NULL, &size);
 *     s.reserve(size); s.resize(size - 1);
 * The native callee read garbage rdi/rsi, so `size` stayed 0 and resize(npos)
 * memset 4GB off the end of the low heap (SIGSEGV in libsystem_platform memset
 * from cow_string_shim g_resize). The call itself "returned" only because the
 * word above the 4-byte return address was the NULL buf argument.
 *
 * Exit codes: 0 = pass; 2/3/4 = _NSGetExecutablePath out-params wrong;
 * 5/6 = fnmatch wrong; a crash = raw native call.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fnmatch.h>
#include <mach-o/dyld.h>

int main(void) {
   uint32_t size = 0;
   int rc = _NSGetExecutablePath(NULL, &size);
   if (rc != -1 || size <= 1) { printf("probe rc=%d size=%u\n", rc, size); fflush(stdout); exit(2); }
   char *buf = malloc(size);
   uint32_t size2 = size;
   rc = _NSGetExecutablePath(buf, &size2);
   if (rc != 0) { printf("fill rc=%d\n", rc); fflush(stdout); exit(3); }
   size_t n = strlen(buf);
   if (n == 0 || n >= size) { printf("len=%zu size=%u\n", n, size); fflush(stdout); exit(4); }

   if (fnmatch("*.x86_64", "a.x86_64", 0) != 0) exit(5);
   if (fnmatch("*.c", "a.h", 0) != FNM_NOMATCH) exit(6);

   printf("ok\n");
   fflush(stdout);
   exit(0);
}
