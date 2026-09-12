/* dyld-imagewalk-probe.c - ONE job: name the image whose Loader makes dyld abort
 * a walk of its loaded-image array, and the walk that touched it.
 *
 * The abort looks like this, and names nothing useful - not the image, not the
 * index, not the caller. It comes from inside dyld's own assert, so there is no
 * error to catch and no return value to inspect:
 *     dyld: Assertion failed: (this->magic == kMagic), function loadAddress,
 *           file Loader.cpp, line 202     (matchesPath, line 237 for the name)
 *
 * So interpose the two indexed query APIs and log, per call, the index, the live
 * count, the thread, the immediate caller, AND the image's identity - then chain
 * through. The last line before the abort is the answer.
 *
 * The identity comes from `dyld_all_image_infos.infoArray`, NOT from dyld's
 * indexed API: infoArray is a plain array of {header, path} that dyld maintains,
 * so reading it touches no Loader and cannot trip the assert.
 *
 * ⚠ But infoArray is NOT in Loader-index order, so infoArray[i] does NOT name
 * entry i. Measured 2026-09-12: dyld index 0 (the main executable) was infoArray
 * entry 3, infoArray entry 0 was dyld itself, and the skew is not a constant
 * offset -- 52011 of 52011 index/entry pairs disagreed. Any probe that prints
 * infoArray[i] as "the name of index i" is lying.
 *
 * So identify by ELIMINATION instead, which needs no ordering assumption: a walk
 * starts at index 0 and climbs, so record the header of every index that queries
 * SUCCESSFULLY, and the infoArray entries not yet accounted for are the only
 * candidates for the next index. By the tail of the walk that set is down to a
 * couple of names, and the aborting index is inside it. The last line before the
 * abort therefore names the corrupted image.
 *
 * ⚠ WHAT THIS IS *NOT* (measured 2026-09-12, native reproducers in
 *   scratchpad/dyldrepro - all four SURVIVED, none reproduced the abort):
 *     - walking by index from an add-image notifier during a dlopen
 *     - ... with 200 dependencies arriving inside that one dlopen
 *     - ... with a dangling LC_LOAD_WEAK_DYLIB, as our weaken-dangling-deps pass makes
 *     - a stale count snapshot read past the end after a dlclose shrank the list
 *     - 29392 index walks on one thread against 116 dlopen/dlclose cycles on another
 *   dyld publishes a whole batch of images, queryable, BEFORE firing any notifier,
 *   bounds-checks a stale index, and locks the indexed API against concurrent
 *   loads. So "the walk is unsafe during a dlopen" is NOT the mechanism, and a fix
 *   built on avoiding the newest entry, or on our own registry, treats the victim
 *   instead of the cause. The walk is simply the first thing to TOUCH a Loader
 *   whose magic was already clobbered - i.e. this is memory corruption, and what
 *   you want from this probe is the identity of the corrupted image.
 *
 * build:  clang -arch x86_64 -dynamiclib -O0 -g -o /tmp/imgwalk_probe.dylib \
 *               src/86x64/dyld-imagewalk-probe.c
 * use:    DYLD_INSERT_LIBRARIES=/tmp/imgwalk_probe.dylib <app>
 *
 * ⚠ Build it -arch x86_64: on Apple Silicon a default build is arm64 and dyld
 * silently ignores it for a Rosetta target.
 * ⚠ It perturbs timing - the abort becomes less frequent under the probe, so loop
 * the run until you catch one rather than concluding it is fixed.
 * IMGWALK_SHIELD=N answers the last N indices as "no such image" (NULL / "")
 * WITHOUT calling dyld, so the walk survives what would have aborted it. That is a
 * DIAGNOSTIC, not a fix -- but it discriminates the two possible causes for free:
 * if the bad Loader is transient state, the process gets past it and runs on; if it
 * is real corruption, the same image aborts again later from a LOWER index, once
 * further loads have pushed it out of the shielded tail. NULL is the safe thing to
 * return here: it is a legal answer from these APIs, whereas handing back an
 * infoArray header would be a WRONG header (the orders differ -- see above).
 *
 * IMGWALK_SCAN=N turns the fatal assert into a CATCHABLE test, which is what makes
 * the bad entries measurable at all. dyld's assert calls abort(), so the SIGABRT
 * handler can siglongjmp back out of it: each index can then be probed and
 * classified good/bad WITHOUT killing the process. The probe scans the top N
 * indices on every add-image notification and again after every dlopen returns,
 * and prints the map whenever it CHANGES. That answers the question a single abort
 * cannot: whether an entry is permanently corrupt or only temporarily unqueryable.
 * ⚠ Jumping out of dyld's assert is safe only because the assert checks and aborts
 * before mutating anything. It is a diagnostic, never something to ship.
 *
 * ⚠ Per-index logging is not viable at this scale: ~1000 images x 2 APIs x
 * thousands of walks is millions of lines. So the EXACT aborting index comes from
 * a SIGABRT handler instead -- the last index handed to dyld is kept in a global
 * and written from the handler with write(2). That costs nothing per call and is
 * precise, where tail-only logging can only bracket the answer.
 *
 * ⚠ Quiet by default: at Portal 2's ~1046 images a per-index line is ~10k lines
 * per walk. IMGWALK_ALL=1 logs every call; otherwise only the last few indices of
 * each walk (IMGWALK_TAIL, default 3) and anything anomalous are printed.
 */
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <stdint.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach-o/dyld_images.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>

/* dyld's plain {header, path} array. Reading it touches no Loader, so it is the
 * one identification channel that cannot trip the assert we are chasing. */
static const struct dyld_all_image_infos *all_infos(void) {
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

static int env_flag(const char *name, int dflt) {
   const char *v = getenv(name);
   return v ? atoi(v) : dflt;
}

/* Headers resolved so far in the CURRENT walk. A walk is recognised by index 0,
 * which is where every full walk starts. */
#define SEEN_CAP 4096
static const struct mach_header *g_seen[SEEN_CAP];
static uint32_t g_nseen;

static void seen_reset(void) { g_nseen = 0; }

static void seen_add(const struct mach_header *h) {
   if (h == NULL || g_nseen >= SEEN_CAP) { return; }
   for (uint32_t i = 0; i < g_nseen; ++i) { if (g_seen[i] == h) { return; } }
   g_seen[g_nseen++] = h;
}

static int seen_has(const struct mach_header *h) {
   for (uint32_t i = 0; i < g_nseen; ++i) { if (g_seen[i] == h) { return 1; } }
   return 0;
}

static const char *leaf(const char *path) {
   if (path == NULL) { return "(null path)"; }
   const char *b = strrchr(path, '/');
   return b ? b + 1 : path;
}

/* Print the images this walk has NOT yet resolved: the next index is one of them,
 * so when the walk aborts this list contains the culprit. */
static void print_candidates(const char *why) {
   const struct dyld_all_image_infos *ai = all_infos();
   if (ai == NULL || ai->infoArray == NULL) {
      fprintf(stderr, "[imgwalk]   %s: (no infoArray -- cannot name candidates)\n", why);
      return;
   }
   const uint32_t max_print = (uint32_t)env_flag("IMGWALK_CANDIDATES", 8);
   uint32_t nunseen = 0;
   fprintf(stderr, "[imgwalk]   %s -- candidates for the next index:\n", why);
   for (uint32_t i = 0; i < ai->infoArrayCount; ++i) {
      const struct dyld_image_info *e = &ai->infoArray[i];
      if (seen_has(e->imageLoadAddress)) { continue; }
      ++nunseen;
      if (nunseen <= max_print) {
         fprintf(stderr, "[imgwalk]     %-40s @%p\n",
                 leaf(e->imageFilePath), (void *)e->imageLoadAddress);
      }
   }
   if (nunseen == 0) {
      fprintf(stderr, "[imgwalk]     (none -- every infoArray entry already resolved)\n");
   } else if (nunseen > max_print) {
      fprintf(stderr, "[imgwalk]     ... and %u more (IMGWALK_CANDIDATES to raise)\n",
              nunseen - max_print);
   }
   fflush(stderr);
}

/* The last index we handed to dyld, for the SIGABRT handler. dyld's assert kills
 * the process from inside the call below, so this is the only record of where. */
static volatile uint32_t g_last_idx = 0xffffffff;
static volatile uint32_t g_last_count;
static volatile const char *g_last_fn = "?";

/* write(2)-only: async-signal-safe, unlike fprintf. */
static void wr(const char *s2) { (void)!write(2, s2, strlen(s2)); }
static void wr_u32(uint32_t v) {
   char b[12]; int n = 0;
   if (v == 0) { b[n++] = '0'; }
   char t[12]; int m = 0;
   while (v) { t[m++] = (char)('0' + (v % 10)); v /= 10; }
   while (m) { b[n++] = t[--m]; }
   (void)!write(2, b, (size_t)n);
}

/* Set while a deliberate probe-query is in flight, so the handler knows to jump
 * back instead of reporting a fatal abort. */
static volatile sig_atomic_t g_probing;
static sigjmp_buf g_jmp;

/* The REAL APIs: our own calls would otherwise hit our own interposers. */
static const struct mach_header *(*real_hdr)(uint32_t);

static void on_abort(int sig) {
   if (g_probing) { siglongjmp(g_jmp, 1); }
   wr("\n[imgwalk] ===== SIGABRT (");
   wr_u32((uint32_t)sig);
   wr(") during ");
   wr((const char *)g_last_fn);
   wr("(");
   wr_u32(g_last_idx);
   wr(") of count=");
   wr_u32(g_last_count);
   wr(") =====\n[imgwalk] that index owns the Loader with the bad magic.\n");
   _exit(134);
}

/* Query index i, catching dyld's assert. 1 = this entry's Loader is bad. */
static int entry_is_bad(uint32_t i) {
   if (real_hdr == NULL) { return 0; }
   if (sigsetjmp(g_jmp, 1) != 0) { g_probing = 0; return 1; }
   g_probing = 1;
   (void)real_hdr(i);
   g_probing = 0;
   return 0;
}

/* Scan the top N entries and print the good/bad map, but only when it changes --
 * this runs on every image load, and an unchanging map is noise. */
static void scan_tail(const char *when) {
   const uint32_t n = (uint32_t)env_flag("IMGWALK_SCAN", 0);
   if (n == 0) { return; }
   const uint32_t count = _dyld_image_count();
   const uint32_t lo = (count > n) ? count - n : 0;

   char map[80]; uint32_t m = 0;
   uint32_t nbad = 0;
   for (uint32_t i = lo; i < count && m < sizeof map - 1; ++i) {
      const int bad = entry_is_bad(i);
      map[m++] = bad ? 'X' : '.';
      nbad += (uint32_t)bad;
   }
   map[m] = '\0';

   static char prev[80];
   static uint32_t prev_count;
   static uint32_t repeat;
   const int changed = (strcmp(prev, map) != 0 || prev_count != count);
   if (!changed) {
      /* An unchanging all-good map is noise; an unchanging BAD map still matters,
       * but only every so often. */
      if (nbad == 0 || ++repeat % 64 != 0) { return; }
   } else {
      repeat = 0;
   }
   snprintf(prev, sizeof prev, "%s", map); prev_count = count;

   fprintf(stderr, "[imgwalk] SCAN %-18s count=%u idx %u..%u  [%s]  bad=%u\n",
           when, count, lo, count - 1, map, nbad);
   if (nbad > 0) {
      for (uint32_t i = lo; i < count; ++i) {
         if (!entry_is_bad(i)) { continue; }
         /* Name it by elimination: everything below i resolves, so the unresolved
          * infoArray entries are the candidates. */
         seen_reset();
         for (uint32_t k = 0; k < i; ++k) { seen_add(real_hdr(k)); }
         fprintf(stderr, "[imgwalk]   BAD index %u:\n", i);
         print_candidates("candidates");
      }
   }
   fflush(stderr);
}

static void probe_on_add(const struct mach_header *mh, intptr_t slide) {
   (void)mh; (void)slide;
   scan_tail("on add-image");
}

__attribute__((constructor)) static void install_abort_handler(void) {
   real_hdr = (const struct mach_header *(*)(uint32_t))
              dlsym(RTLD_NEXT, "_dyld_get_image_header");
   struct sigaction sa;
   memset(&sa, 0, sizeof sa);
   sa.sa_handler = on_abort;
   sa.sa_flags = SA_NODEFER;
   sigaction(SIGABRT, &sa, NULL);
   if (env_flag("IMGWALK_SCAN", 0) > 0) {
      _dyld_register_func_for_add_image(probe_on_add);
   }
}

/* Scan again once a dlopen has RETURNED: if an entry that was bad during the load
 * is good now, the badness was transient state, not corruption. */
static void *probe_dlopen(const char *path, int mode) {
   void *h = dlopen(path, mode);
   scan_tail("after dlopen");
   return h;
}

static void note(const char *fn, uint32_t idx, void *ret) {
   Dl_info info;
   const char *img = "?"; const char *sym = "?";
   unsigned long off = 0;
   if (dladdr(ret, &info) && info.dli_fname) {
      img = leaf(info.dli_fname);
      sym = info.dli_sname ? info.dli_sname : "(nosym)";
      off = (unsigned long)((char *)ret - (char *)info.dli_fbase);
   }
   fprintf(stderr, "[imgwalk] %s(%u) count=%u resolved=%u tid=%p from %s+0x%lx %s\n",
           fn, idx, _dyld_image_count(), g_nseen,
           (void *)(uintptr_t)pthread_self(), img, off, sym);
   fflush(stderr);
}

/* Log the tail of every walk (where the abort lands) without drowning a
 * 1046-image process in 10k lines per walk. */
static int should_log(uint32_t idx) {
   if (env_flag("IMGWALK_ALL", 0)) { return 1; }
   const uint32_t count = _dyld_image_count();
   const uint32_t tail = (uint32_t)env_flag("IMGWALK_TAIL", 3);
   return (idx + tail >= count);
}

/* Is index `i` inside the shielded tail? */
static int shielded(uint32_t i) {
   const uint32_t n = (uint32_t)env_flag("IMGWALK_SHIELD", 0);
   if (n == 0) { return 0; }
   const uint32_t count = _dyld_image_count();
   return (i + n >= count);
}

static const struct mach_header *probe_hdr(uint32_t i) {
   if (i == 0) {
      seen_reset();
      /* A full walk starts here. Scanning now separates "the entry was ALREADY bad
       * before this walk began" from "it went bad during the walk". */
      scan_tail("at walk start");
   }
   if (shielded(i)) {
      static uint32_t last_logged = 0xffffffff;
      if (i != last_logged) {
         last_logged = i;
         fprintf(stderr, "[imgwalk] SHIELD: answering header(%u) of %u as NULL "
                 "without asking dyld\n", i, _dyld_image_count());
         fflush(stderr);
      }
      return NULL;
   }
   if (should_log(i)) {
      note("_dyld_get_image_header", i, __builtin_return_address(0));
      print_candidates("about to query");
   }
   /* If dyld is about to abort, it aborts INSIDE this call -- so record where we
    * are first; the SIGABRT handler reports it. */
   g_last_idx = i; g_last_count = _dyld_image_count();
   g_last_fn = "_dyld_get_image_header";
   const struct mach_header *h = _dyld_get_image_header(i);
   seen_add(h);
   return h;
}

static const char *probe_name(uint32_t i) {
   if (i == 0) { seen_reset(); }
   if (shielded(i)) { return ""; }
   if (should_log(i)) {
      note("_dyld_get_image_name", i, __builtin_return_address(0));
      print_candidates("about to query");
   }
   g_last_idx = i; g_last_count = _dyld_image_count();
   g_last_fn = "_dyld_get_image_name";
   return _dyld_get_image_name(i);
}

__attribute__((used)) static struct { const void *r; const void *o; }
interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)&probe_hdr,  (const void *)&_dyld_get_image_header },
   { (const void *)&probe_name, (const void *)&_dyld_get_image_name  },
   { (const void *)&probe_dlopen, (const void *)&dlopen },
};
