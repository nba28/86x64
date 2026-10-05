/* dyld_image_list.c — ONE job: enumerate the loaded images WITHOUT asking dyld to
 * query a Loader, because doing that can abort the whole process.
 *
 * ★ WHY THIS EXISTS (measured on Portal 2, 2026-09-12)
 *
 * `_dyld_get_image_header(i)`, `_dyld_get_image_name(i)` and
 * `_dyld_get_image_vmaddr_slide(i)` all ask dyld to query entry i's `Loader`. An
 * entry whose image is still mid-load is NOT queryable, and dyld does not return
 * NULL or an error for it — it aborts the process from inside its own assert:
 *
 *     dyld: Assertion failed: (this->magic == kMagic), function loadAddress,
 *           file Loader.cpp, line 202      (matchesPath, line 237 for the name)
 *
 * Our runtime walks the image list constantly (to find a dependency, to locate a
 * symbol, to build the sibling tables), and those walks run from an add-image
 * callback and from translated static initializers — i.e. exactly while another
 * image is mid-load. Portal 2 died this way in 9 of 12 runs: engine.dylib is the
 * only image it dlopens that late, and the walk that touched engine's entry
 * killed the process.
 *
 * ★ THE STATE IS TRANSIENT, WHICH IS WHY NOTHING CHEAPER WORKS
 * Measured with `src/86x64/dyld-imagewalk-probe.c` (which can catch the assert by
 * siglongjmp and so classify entries good/bad without dying): the badness MOVES.
 * One scan of indices 1037..1044 read [X.....X.], the next [......X.] — index 1037
 * had become queryable while 1043 (engine.dylib, named by elimination) had not.
 * So an entry cannot be pre-tested, and there is no index rule to apply:
 *   - "skip the newest entry" is WRONG — the bad index was count-1 in one run and
 *     count-2 in others, because the dlopen target's own dependencies are appended
 *     after it.
 *   - "only guard inside our add-image callback" is WRONG — the unsafe window is
 *     the whole load, and dyld runs initializers, which walk, after the callback
 *     returns.
 *   - "iterate a self-maintained registry instead" REGRESSES: a dependency is
 *     mapped before the dependent's callback fires, so the registry misses it and
 *     the dependency lookup that `process_deps` needs starts failing (measured:
 *     the assert went away and 10/10 runs SIGSEGV'd instead).
 *
 * ★ WHAT IS ACTUALLY SAFE
 * `dyld_all_image_infos.infoArray` is a plain array of {header, path} that dyld
 * maintains for debuggers and the crash reporter. Reading it touches NO Loader, so
 * it cannot trip the assert — and it lists in-flight images too: the probe read
 * `engine.dylib @0x13194000` out of infoArray at a moment when that same image's
 * Loader was unqueryable. That is the property the registry lacked, so this both
 * fixes the abort and keeps the dependency lookup working.
 *
 * ⚠ infoArray is NOT in Loader-index order (measured: dyld index 0 was infoArray
 * entry 3, and 52011 of 52011 index/entry pairs disagreed), and it also contains
 * entries that are not in the Loader list at all (dyld itself, the Rosetta
 * runtime). So these functions deliberately expose NO relationship to a dyld
 * index: this is an unordered set of loaded images, which is all our callers ever
 * wanted. Never pass an index from here to a dyld API, or the abort comes back.
 *
 * ⚠ The array can be mutated while we read it, so every entry is validated (a real
 * Mach-O magic, a non-NULL path) and a torn entry is skipped rather than trusted.
 *
 * ★ infoArray IS NOT THE SAME SET AS dyld's LOADER LIST — it holds exactly two more
 * entries: dyld itself and, under Rosetta, /usr/libexec/rosetta/libRosettaRuntime.
 * Those are not part of the app's image graph, and handing them to callers that were
 * written against the Loader list is a REAL behaviour change, not a cosmetic one:
 * a caller that brackets an address by its LOW 32 BITS on the
 * documented assumption that the image lives in the low 4 GB, and libRosettaRuntime
 * at 0x10000a000 has low32 0xa000, so it would match and return a base above 4 GB.
 * So both are filtered out. dyld is identified structurally, from
 * dyld_all_image_infos.dyldImageLoadAddress; the Rosetta runtime by its OS path,
 * which is a property of the platform's translation runtime, not of any app.
 *
 * ★★⛔ THE infoArray PATH IS OFF BY DEFAULT, AND IT IS NOT A CORRECT FIX YET.
 * Measured on Portal 2 (2026-09-12), 6 runs per arm, same probe in all arms:
 *     legacy (dyld indexed APIs)   -> 3/6 abort, LATE: idx 1042 of 1044 (engine load)
 *     infoArray, non-app filtered  -> 5/6 abort, EARLY: idx 0 of 697
 *     infoArray, unfiltered        -> same as filtered (the filter is irrelevant)
 * The infoArray path removes the abort from OUR walks and then dies EARLIER, at a
 * LOWER index, because it silently redefines "loaded image": infoArray lists images
 * that are MAPPED but not yet READY, so find_loaded_image starts succeeding for an
 * image dyld has not finished, and process_deps acts on it. Index 0 (the main
 * executable, never in flight) becoming unqueryable says dyld's own state is then
 * genuinely corrupt, not merely mid-load.
 *
 * ⚠ AND THE ABORT IS NOT OURS TO FIX BY CHOOSING WHO WALKS: with every walk of ours
 * converted, **CoreFoundation's `_CFGetHandleForLoadedLibrary` walk aborted instead**.
 * Any full walk dies once an entry is unqueryable, so the real bug is whatever MAKES
 * an entry unqueryable. Eight native reproducer arms failed to reproduce that
 * (see the dyld image-walk-during-dlopen gotcha) -- including a nested dlopen, from inside
 * the add-image notifier, of an image the OUTER dlopen was itself still loading,
 * which is the closest analogue of process_deps.
 *
 * So: `M64_IMGLIST_INFOARRAY=1` opts INTO the infoArray path for further
 * experiments; the default is the dyld APIs, i.e. exactly the pre-existing
 * behaviour. What is unconditionally used is x64_img_slide(), which needs no dyld
 * call and is validated identical to _dyld_get_image_vmaddr_slide on 45/45 and
 * 246/246 images.
 */
#include "dyld_image_list.h"
#include "gap.h"

#include <string.h>
#include <mach/mach.h>
#include <mach/task_info.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <stdlib.h>

/* Default OFF: the indexed dyld APIs are what every caller was written against.
 * See the header comment for the measurement that made this the default. */
int x64_img_legacy_mode(void)
{
   static int legacy = -1;
   if (legacy < 0) { legacy = (getenv("M64_IMGLIST_INFOARRAY") == NULL); }
   return legacy;
}

static const struct dyld_all_image_infos *all_infos(void)
{
   static const struct dyld_all_image_infos *cached;
   static int tried;
   if (!tried) {
      tried = 1;
      struct task_dyld_info ti;
      mach_msg_type_number_t cnt = TASK_DYLD_INFO_COUNT;
      if (task_info(mach_task_self(), TASK_DYLD_INFO, (task_info_t)&ti, &cnt)
          == KERN_SUCCESS) {
         cached = (const struct dyld_all_image_infos *)(uintptr_t)ti.all_image_info_addr;
      }
   }
   return cached;
}

/* A header we are willing to believe. The array can be read mid-update, so a
 * plausible-looking pointer is not enough. */
static int header_ok(const struct mach_header *mh)
{
   if (mh == NULL) { return 0; }
   const uint32_t m = mh->magic;
   return (m == MH_MAGIC_64 || m == MH_MAGIC);
}

/* Entries that exist in infoArray but not in dyld's Loader list, and are not part
 * of the app's image graph. See the header comment for why this matters. */
static int entry_excluded(const struct dyld_all_image_infos *ai, uint32_t i)
{
   const struct dyld_image_info *e = &ai->infoArray[i];
   if (e->imageLoadAddress == (const struct mach_header *)ai->dyldImageLoadAddress) {
      return 1;                                   /* dyld itself, structurally */
   }
   const char *p = e->imageFilePath;
   if (p != NULL && strncmp(p, "/usr/libexec/rosetta/", 21) == 0) { return 1; }
   return 0;
}

uint32_t x64_img_count(void)
{
   if (x64_img_legacy_mode()) { return _dyld_image_count(); }
   const struct dyld_all_image_infos *ai = all_infos();
   if (ai == NULL || ai->infoArray == NULL) { return 0; }
   return ai->infoArrayCount;
}

const struct mach_header *x64_img_header(uint32_t i)
{
   if (x64_img_legacy_mode()) { return _dyld_get_image_header(i); }
   const struct dyld_all_image_infos *ai = all_infos();
   if (ai == NULL || ai->infoArray == NULL || i >= ai->infoArrayCount) { return NULL; }
   if (entry_excluded(ai, i)) { return NULL; }
   const struct mach_header *mh = ai->infoArray[i].imageLoadAddress;
   return header_ok(mh) ? mh : NULL;
}

const char *x64_img_path(uint32_t i)
{
   if (x64_img_legacy_mode()) { return _dyld_get_image_name(i); }
   const struct dyld_all_image_infos *ai = all_infos();
   if (ai == NULL || ai->infoArray == NULL || i >= ai->infoArrayCount) { return NULL; }
   if (entry_excluded(ai, i)) { return NULL; }
   if (!header_ok(ai->infoArray[i].imageLoadAddress)) { return NULL; }
   return ai->infoArray[i].imageFilePath;
}

const char *x64_img_leaf(const char *path)
{
   if (path == NULL) { return NULL; }
   const char *b = strrchr(path, '/');
   return b ? b + 1 : path;
}

/* The slide, computed from the image's OWN mapped header rather than asked of
 * dyld: it is the distance between where __TEXT was linked to live and where the
 * header actually is. Pure memory reads, so it is safe for an in-flight image
 * exactly like the rest of this file. */
intptr_t x64_img_slide(const struct mach_header *mh)
{
   if (!header_ok(mh)) { return 0; }
   const int is64 = (mh->magic == MH_MAGIC_64);
   const uint8_t *p = (const uint8_t *)mh +
                      (is64 ? sizeof(struct mach_header_64) : sizeof(struct mach_header));
   for (uint32_t i = 0; i < mh->ncmds; ++i) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmdsize == 0) { break; }
      if (is64 && lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *sg = (const struct segment_command_64 *)p;
         if (strncmp(sg->segname, SEG_TEXT, sizeof sg->segname) == 0) {
            return (intptr_t)((uintptr_t)mh - (uintptr_t)sg->vmaddr);
         }
      } else if (!is64 && lc->cmd == LC_SEGMENT) {
         const struct segment_command *sg = (const struct segment_command *)p;
         if (strncmp(sg->segname, SEG_TEXT, sizeof sg->segname) == 0) {
            return (intptr_t)((uintptr_t)mh - (uintptr_t)sg->vmaddr);
         }
      }
      p += lc->cmdsize;
   }
   return 0;
}

const struct mach_header *x64_img_find_leaf(const char *leaf, intptr_t *slide_out)
{
   if (leaf == NULL) { return NULL; }
   /* Legacy mode goes through the accessors below, which chain to dyld. */
   const uint32_t n = x64_img_count();
   for (uint32_t i = 0; i < n; ++i) {
      const char *path = x64_img_path(i);
      const char *b = x64_img_leaf(path);
      if (b == NULL || strcmp(b, leaf) != 0) { continue; }
      const struct mach_header *mh = x64_img_header(i);
      if (mh == NULL) { continue; }
      if (slide_out != NULL) { *slide_out = x64_img_slide(mh); }
      return mh;
   }
   return NULL;
}

const char *x64_img_path_for_header(const struct mach_header *mh)
{
   if (mh == NULL) { return NULL; }
   const uint32_t n = x64_img_count();
   for (uint32_t i = 0; i < n; ++i) {
      if (x64_img_header(i) == mh) { return x64_img_path(i); }
   }
   return NULL;
}

int x64_img_is_translated(const void *base) {
   const struct mach_header_64 *mh = (const struct mach_header_64 *)base;
   if (mh == NULL || mh->magic != MH_MAGIC_64) { return 0; }
   const struct load_command *lc = (const struct load_command *)(mh + 1);
   for (uint32_t i = 0; i < mh->ncmds; i++) {
      if (lc->cmdsize < sizeof *lc) { return 0; }   /* malformed: refuse to walk */
      if (lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB ||
          lc->cmd == LC_REEXPORT_DYLIB || lc->cmd == LC_LOAD_UPWARD_DYLIB) {
         const struct dylib_command *dc = (const struct dylib_command *)lc;
         if (dc->dylib.name.offset < dc->cmdsize) {
            const char *nm = (const char *)lc + dc->dylib.name.offset;
            if (strstr(nm, "libabiconv") != NULL) { return 1; }
         }
      }
      lc = (const struct load_command *)((const char *)lc + lc->cmdsize);
   }
   return 0;
}

/* const struct segment_command *getsegbyname(const char *segname);
 * Looks the name up in the main executable's segments. i386 callers bound it raw
 * (no prototype in the modern SDK), so the native 8-byte `ret` popped the i386
 * return address into garbage. Call of Duty 4 probes `__BOOKKEEPING`, a segment
 * it does not have: the original answer is NULL and its optional hook stays off.
 * The i386 executable's segments survive by name in its translated image, so
 * "absent" is exact. A PRESENT segment would need a synthesized 32-bit
 * segment_command (unslid i386 vmaddr): loud gap until a target reaches it.
 * No kill switch: the defect was the raw translate-time bind itself, so the guard
 * (getsegbyname-absent) asserts the bind and the NULL answer. */
uint32_t shim_getsegbyname(uint32_t *args) {
   const char *name = (const char *)(uintptr_t)args[0];
   if (!name) return 0;
   const uint32_t n = x64_img_count();
   for (uint32_t i = 0; i < n; ++i) {
      const struct mach_header_64 *mh = (const struct mach_header_64 *)x64_img_header(i);
      if (!x64_img_is_translated(mh)) continue;
      const struct load_command *lc = (const struct load_command *)(mh + 1);
      for (uint32_t c = 0; c < mh->ncmds; c++) {
         if (lc->cmd == LC_SEGMENT_64 &&
             strncmp(((const struct segment_command_64 *)lc)->segname, name, 16) == 0) {
            GAP_STUB(args);
            return 0;
         }
         lc = (const struct load_command *)((const char *)lc + lc->cmdsize);
      }
   }
   return 0;
}
