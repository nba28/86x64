/* dyld-image-list-verify.c — ONE job: prove src/abiconv/dyld_image_list.c agrees
 * with dyld itself, for every image in a healthy process.
 *
 * The module exists because dyld's INDEXED image APIs can abort the process
 * (see src/abiconv/dyld_image_list.c). Two of its answers are computed rather
 * than asked for, so they need proving, not assuming:
 *
 *   x64_img_slide()          — derived from the image's own mapped __TEXT vmaddr
 *                              instead of _dyld_get_image_vmaddr_slide(i).
 *   x64_img_path_for_header() / x64_img_find_leaf()
 *                            — resolve through infoArray, which is NOT in
 *                              Loader-index order, so a per-index comparison
 *                              would be meaningless. This matches by HEADER.
 *
 * It checks, for every image dyld reports: the image is present in our list
 * (matched by header), our slide equals dyld's exactly, and find-by-leaf resolves.
 * Exit 42 = full agreement; 1 = any disagreement, with the offenders printed.
 *
 * ⚠ Runs the infoArray path explicitly (M64_IMGLIST_INFOARRAY), since that path is
 * OFF by default in the runtime — verifying the default would just be comparing
 * dyld against itself and would pass while proving nothing.
 *
 * build:
 *   clang -arch x86_64 -I src/abiconv -o /tmp/dyld-image-list-verify \
 *         src/86x64/probes/dyld-image-list-verify.c src/abiconv/dyld_image_list.c
 * run (plain, ~45 images), then again with many images loaded:
 *   /tmp/dyld-image-list-verify
 *   DYLD_INSERT_LIBRARIES=<some.dylib> /tmp/dyld-image-list-verify
 * Measured 2026-09-12: 45/45 and 246/246 agree, 0 slide mismatches, 0 missing.
 */
#include "dyld_image_list.h"

#include <mach-o/dyld.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
   /* The module's infoArray path is opt-in; select it before the first call so
    * its one-shot env read latches the mode we actually want to test. */
   if (getenv("M64_IMGLIST_INFOARRAY") == NULL) {
      setenv("M64_IMGLIST_INFOARRAY", "1", 1);
   }

   const uint32_t dn = _dyld_image_count();
   /* x64_img_count() is an upper BOUND, not a compacted count: excluded and torn
    * entries are reported as NULL by the accessors rather than removed, so the
    * count stays a stable iteration bound. Expect it to exceed dyld's. */
   printf("dyld images=%u   our iteration bound=%u   (bound >= dyld's: infoArray "
          "also lists dyld itself and the Rosetta runtime, which the accessors "
          "return as NULL)\n", dn, x64_img_count());

   int checked = 0, slide_bad = 0, missing = 0, find_bad = 0;
   for (uint32_t i = 0; i < dn; ++i) {
      const struct mach_header *h = _dyld_get_image_header(i);
      const char *p = _dyld_get_image_name(i);
      if (h == NULL || p == NULL) { continue; }
      const intptr_t want = _dyld_get_image_vmaddr_slide(i);

      if (x64_img_path_for_header(h) == NULL) {
         printf("MISSING from our list: %s\n", p);
         ++missing;
         continue;
      }
      ++checked;

      const intptr_t got = x64_img_slide(h);
      if (got != want) {
         printf("SLIDE MISMATCH %-44s dyld=%#lx ours=%#lx\n",
                x64_img_leaf(p), (unsigned long)want, (unsigned long)got);
         ++slide_bad;
      }

      intptr_t fs = 0;
      if (x64_img_find_leaf(x64_img_leaf(p), &fs) == NULL) {
         printf("FIND-BY-LEAF FAILED for %s\n", x64_img_leaf(p));
         ++find_bad;
      }
   }

   printf("checked=%d  slide mismatches=%d  find failures=%d  missing=%d\n",
          checked, slide_bad, find_bad, missing);
   const int ok = (slide_bad == 0 && find_bad == 0 && missing == 0 && checked > 0);
   printf("%s\n", ok ? "AGREE" : "DISAGREE");
   exit(ok ? 42 : 1);
}
