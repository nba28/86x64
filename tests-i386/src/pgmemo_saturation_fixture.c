/* pgmemo_saturation_fixture.c — native driver for pgmemo_saturation_test.sh.
 * Fills libabiconv's page-readability memo past its capacity (262144 pages),
 * then times lookups of pages the table does not hold. Prints the time in
 * microseconds; the script compares the shipped arm with the unbounded walk. */
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <time.h>

#define FILL_PAGES 300000u     /* > PGMEMO_CAP */
#define MISS_PROBES 2000u

int main(int argc, char **argv) {
   /* RTLD_LAZY: libabiconv binds removed legacy APIs lazily (flat namespace). */
   void *h = dlopen(argv[1], RTLD_LAZY);
   if (!h) { printf("dlopen: %s\n", dlerror()); return 2; }
   int (*readable)(uintptr_t, size_t) = (int (*)(uintptr_t, size_t))dlsym(h, "_86x64_test_mem_readable");
   if (!readable) { printf("no _86x64_test_mem_readable\n"); return 2; }

   char *fill = mmap(NULL, (size_t)FILL_PAGES * 4096, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
   if (fill == MAP_FAILED) { printf("mmap fill failed\n"); return 2; }
   for (uint32_t i = 0; i < FILL_PAGES; ++i) {
      if (!readable((uintptr_t)fill + (uintptr_t)i * 4096, 1)) { printf("fill page %u unreadable\n", i); return 2; }
   }
   /* Pages never seen: a fresh mapping. Each probe misses the saturated table. */
   char *miss = mmap(NULL, (size_t)MISS_PROBES * 4096, PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
   if (miss == MAP_FAILED) { printf("mmap miss failed\n"); return 2; }
   struct timespec a, b;
   clock_gettime(CLOCK_MONOTONIC, &a);
   for (uint32_t i = 0; i < MISS_PROBES; ++i) {
      if (!readable((uintptr_t)miss + (uintptr_t)i * 4096, 1)) { printf("miss page %u unreadable\n", i); return 2; }
   }
   clock_gettime(CLOCK_MONOTONIC, &b);
   printf("%lld\n", (long long)(b.tv_sec - a.tv_sec) * 1000000 + (b.tv_nsec - a.tv_nsec) / 1000);
   return 0;
}
