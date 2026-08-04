/* nulljump_probe.c — "does this symbol resolve?", asked from the RIGHT ARCH.
 *
 * WHY THIS EXISTS. find_null_jump_bridges.py used to answer that question with
 * ctypes in its own process. On Apple Silicon `python3` is arm64, so it was
 * asking whether the ARM64 system libraries export the symbol — while the only
 * process that will ever bind these names is a TRANSLATED x86_64 app. The two
 * answers differ for every symbol whose existence is architecture-specific:
 *
 *   _objc_msgSend_stret, _objc_msgSend_fpret, _objc_msgSendSuper_stret
 *       real x86_64 entry points; arm64 libobjc has no struct/fpret variants.
 *   _stat$INODE64, _readdir$INODE64, _opendir$INODE64, _scandir$INODE64, ...
 *       the 32/64-bit-inode symbol suffix, alive on x86_64, absent on arm64.
 *   _drem, _finite, _gamma, _significand, _rinttol, _roundtol, _daemon$1050
 *       legacy libSystem compatibility exports kept for x86_64 only.
 *
 * Measured 2026-08-04: 33 of 147 "orphans" were this false positive. They are
 * exactly the ones where being wrong hurts most — the gate would have told
 * static-interpose to leave objc_msgSend_stret and the stat/readdir family bound
 * DIRECTLY to native x86_64 code, called with i386 4-byte argument slots and no
 * marshalling at all.
 *
 * Reads symbol names on stdin (leading underscore optional, as `nm` prints it)
 * and echoes back, verbatim, only those that resolve. argv = libraries to dlopen
 * first, so RTLD_DEFAULT sees the same scope a translated app's dyld would.
 *
 * ⚠Must be built `-arch x86_64` and run under `arch -x86_64`.
 * find_null_jump_bridges.py does both, and REFUSES to fall back to an in-process
 * answer: a plausible wrong list is worse than a loud failure.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
   for (int i = 1; i < argc; i++)
      dlopen(argv[i], RTLD_LAZY | RTLD_GLOBAL);   /* absent framework: that IS the finding */

   char line[1024];
   while (fgets(line, sizeof line, stdin)) {
      size_t n = strlen(line);
      while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
         line[--n] = '\0';
      if (!n)
         continue;
      /* dlsym takes the C name; nm prints the mangled one with a leading _. */
      const char *want = (line[0] == '_') ? line + 1 : line;
      if (dlsym(RTLD_DEFAULT, want))
         printf("%s\n", line);
   }
   return 0;
}
