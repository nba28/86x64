/* dyld-imagewalk-probe.c — ONE job: find the walk of dyld's loaded-image array
 * that made dyld abort the process.
 *
 * A full walk of that array is unsafe while a dlopen is IN FLIGHT: the entry for
 * the image being loaded is already in the array but its `Loader` cannot be queried
 * yet. dyld does not return an error for that — it aborts from inside its own
 * assert, naming nothing that helps:
 *     dyld: Assertion failed: (this->magic == kMagic), function loadAddress,
 *           file Loader.cpp, line 202     (matchesPath, line 237 for the name)
 *
 * So interpose the two indexed query APIs and log, per call, the index, the live count,
 * the thread, and the immediate caller; then chain through. The LAST line before
 * the abort names the offending walk and proves which index it died on. That is
 * how Portal 2's engine.dylib wall was pinned.
 *
 * ⚠ The bad index is NOT reliably the last one, so do not read one run as a rule.
 * Two runs of the same binary: count 1043, aborted on 1042 (= count-1); count 1046,
 * aborted on 1044 (= count-2). Both times the index was the dlopen TARGET — its own
 * dependencies get appended after it. Log the index, do not predict it.
 *
 * build:  clang -arch x86_64 -dynamiclib -O0 -g -o /tmp/imgwalk_probe.dylib \
 *               src/86x64/dyld-imagewalk-probe.c
 * use:    DYLD_INSERT_LIBRARIES=/tmp/imgwalk_probe.dylib <app>
 *
 * ⚠ Build it -arch x86_64: on Apple Silicon a default build is arm64 and dyld
 * silently ignores it for a Rosetta target.
 * ⚠ It perturbs timing — the abort becomes less frequent under the probe, so loop
 * the run until you catch one rather than concluding it is fixed.
 */
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>
#include <mach-o/dyld.h>
#include <pthread.h>

static void note(const char *fn, uint32_t idx, void *ret) {
   Dl_info info;
   const char *img = "?"; const char *sym = "?";
   unsigned long off = 0;
   if (dladdr(ret, &info) && info.dli_fname) {
      const char *b = strrchr(info.dli_fname, '/');
      img = b ? b + 1 : info.dli_fname;
      sym = info.dli_sname ? info.dli_sname : "(nosym)";
      off = (unsigned long)((char *)ret - (char *)info.dli_fbase);
   }
   fprintf(stderr, "[imgwalk] %s(%u) count=%u tid=%p from %s+0x%lx %s\n",
           fn, idx, _dyld_image_count(), (void *)(uintptr_t)pthread_self(),
           img, off, sym);
   fflush(stderr);
}

static const struct mach_header *probe_hdr(uint32_t i) {
   note("_dyld_get_image_header", i, __builtin_return_address(0));
   return _dyld_get_image_header(i);
}
static const char *probe_name(uint32_t i) {
   note("_dyld_get_image_name", i, __builtin_return_address(0));
   return _dyld_get_image_name(i);
}

__attribute__((used)) static struct { const void *r; const void *o; }
interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)&probe_hdr,  (const void *)&_dyld_get_image_header },
   { (const void *)&probe_name, (const void *)&_dyld_get_image_name  },
};
