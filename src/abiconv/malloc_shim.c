/*
 * malloc_shim.c — low-4GB heap for the translated i386 program.
 *
 * The i386 target stores pointers in 32-bit slots, so every value malloc
 * hands back must fit in 32 bits. macOS's real malloc returns addresses
 * well above 4 GB; the i386 code truncates them and then faults on the
 * bogus low-32 pointer (photocd's fread destination buffer, for one).
 *
 * abiconv's autogen shims (`___malloc` etc.) do the i386->x86_64 ABI lift
 * and then `call _malloc` / `_free` / `_calloc` / `_realloc`. Defining
 * those symbols here intercepts the call — the same mechanism file_shim.c
 * uses for the stdio functions — and satisfies the request from a region
 * reserved below 4 GB.
 *
 * The allocator is deliberately minimal: a bump pointer over reserved
 * regions, plus a single first-fit free list so high-churn uniform-size
 * allocations get recycled. Freed blocks are never split or coalesced. The
 * translated targets are short-lived, so this trades a little high-water
 * memory for simplicity and the absence of a fragmentation class of bug.
 *
 * Thread-safety: all entry points take a single os_unfair_lock. Real GUI
 * targets (iPhoto, Tessera) spin up worker threads, so the older
 * single-threaded assumption corrupted the heap; the lock is mandatory.
 * Reserves grow on demand (additional regions) so large apps don't hit a
 * hard ceiling, and free() validates the pointer belongs to one of our
 * regions before touching it (a stray/double free from the translated
 * program would otherwise corrupt the free list).
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <os/lock.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <dlfcn.h>
#include <mach-o/getsect.h>
#include <mach-o/loader.h>

/* objc_slide.c: the i386 init stack's live range, 0 unless it is really ours. */
extern int _86x64_init_stack_range(uintptr_t *top, uintptr_t *bottom);
extern uint64_t cb_readable_span(uint64_t p, uint64_t want);   /* cb_bridge.c */

/* Reserve the heap inside the wrapper's low-4GB window [0x80000000,
 * 0xF0000000). Start the scan above the bottom 128 MB so the wrapper's
 * small shim-FILE / scratch allocations (which cluster at the base) don't
 * fragment the search. Regions are reserved, not committed — macOS faults
 * pages in on first touch, so an oversized reservation is free.
 *
 * ⚠ HEAP_SIZE is a FRAGMENTATION BUDGET, not just a growth granularity. The
 * whole window is only 0x68000000 (1.625 GiB), and a single allocation has to
 * fit inside ONE region, so a large default region can make a large request
 * impossible: at the old 768 MB, Portal 2's libvstdlib static initializers could
 * not get their 1 GiB block (768 MB + 1 GiB > the window), and the NULL became a
 * silent _exit(0). 256 MB leaves a 1 GiB request room to land while still
 * amortising one mapping over many small blocks, and regions grow on demand, so
 * total capacity is unchanged (the window, not HEAP_SIZE, is the real ceiling). */
#define HEAP_SCAN_LO  0x88000000UL
#define HEAP_SCAN_HI  0xF0000000UL
#define HEAP_SIZE     (256UL * 1024 * 1024)
#define HEAP_STEP     (16UL * 1024 * 1024)
#define MAX_REGIONS   16

/* OVERFLOW BAND — the anonymous-mmap band [0x10000000, 0x80000000) from
 * mmap_shim.c, the widest contiguous low-4GB gap (~1.75 GB).
 *
 * The heap's own window is only 1.625 GiB, and a 32-bit target can legitimately
 * want more than that: Portal 2's tier0 CStdMemAlloc reserves 1 GiB in one block
 * and then hundreds of MB more, which no arrangement of the primary window can
 * satisfy. Spilling into the mmap band is safe rather than merely expedient,
 * because BOTH allocators place every mapping with mach_vm_allocate +
 * VM_FLAGS_FIXED, which FAILS instead of clobbering when a slot is taken — so the
 * two simply compete for slots and neither can corrupt the other.
 *
 * It is a LAST RESORT, tried only once the heap's own window is exhausted: at
 * that point the alternative is returning NULL, which for a translated target is
 * normally fatal. Structural trigger (primary window full), not app-specific. */
#define HEAP_OVERFLOW_LO  0x10000000UL
#define HEAP_OVERFLOW_HI  0x80000000UL

/* 16-byte block header; payload (returned to caller) follows, 16-aligned.
 * `size` is the payload capacity. `next` links free blocks.
 *
 * The top bit of `size` is a FREED marker. A live block has it clear; free()
 * sets it and malloc() clears it on reuse. This makes a double-free O(1)
 * detectable: without it, free() pushes an already-free block onto the list a
 * second time, so two later malloc()s hand back the SAME block — two live
 * allocations aliasing one region. With reverse-IMP low-4GB stacks (512 KB
 * each) flowing through this same heap, that aliasing let one frame's carve
 * zero another frame's translated return slot -> a `ret` to 0 (rip=0). Real
 * apps double-free occasionally; the system allocator tolerates some, this one
 * must too. Capacities are <=768 MB so bit 63 is always free for the flag. */
#define BLK_FREED_BIT (1ULL << 63)
#define BLK_CAP(b)    ((b)->size & ~BLK_FREED_BIT)
struct block {
   uint64_t size;
   struct block *next;
};

struct region {
   char *base;
   char *cur;
   char *end;
};

/* ---- Cross-copy shared allocator state -------------------------------------
 * libabiconv is loaded MANY times in a translated app (one copy co-located
 * with each translated binary). The heap state MUST be shared: the low-4GB
 * window only fits a couple of 768MB regions, so the FIRST copy to reserve one
 * wins and every later copy's VM_FLAGS_FIXED add_region fails — leaving that
 * copy with g_nregions==0 and a malloc that always returns NULL. Because each
 * copy's cxx_shim / abigen malloc shim binds same-image to ITS OWN malloc, a
 * regionless copy then aborts ("operator new failed") even though another copy
 * holds a perfectly good 1.5GB heap. (This is the blocker that looked like
 * "heap exhaustion" in iPhoto and iWeb.)
 *
 * So the regions/bump-cursors/free-list/lock all live in a single process-wide
 * control block, published by the first copy via an env var and adopted by
 * address (the same mechanism objc_shim's arena_init uses). The control block
 * is reserved with mach_vm_allocate, NOT malloc, to avoid a circular
 * dependency (malloc needs the block the block can't need malloc). */
struct heap_ctrl {
   uint64_t       magic;
   os_unfair_lock lock;
   int            nregions;
   struct region  regions[MAX_REGIONS];
   struct block  *free_list;
};

#define HEAP_CTRL_ENV   "ABICONV_HEAP_CTRL"
#define HEAP_CTRL_MAGIC 0x3836583634484541ULL  /* "86X64HEA" */

static struct heap_ctrl *g_hc = NULL;
/* Per-copy lock serialising heap_init within this copy: real targets call
 * malloc from many threads, and the first calls can race before g_hc is set;
 * without this two threads would each create a control block (two heaps, env
 * var written twice) and corrupt allocation. The cross-COPY race (different
 * copies racing to be first) is still resolved by the env-var + magic
 * mechanism below; copy constructors run dyld-serialized so it is rare. */
static os_unfair_lock g_init_lock = OS_UNFAIR_LOCK_INIT;

/* Attach to the shared control block, creating it if we are the first copy.
 * Idempotent; safe to call at the top of every entry point. */
static void heap_init(void) {
   if (g_hc) { return; }
   os_unfair_lock_lock(&g_init_lock);
   if (g_hc) { os_unfair_lock_unlock(&g_init_lock); return; }
   const char *e = getenv(HEAP_CTRL_ENV);
   if (e && *e) {
      struct heap_ctrl *c =
         (struct heap_ctrl *)(uintptr_t)strtoull(e, NULL, 16);
      volatile uint64_t *mp = &c->magic;
      for (int i = 0; i < 1000000 && *mp != HEAP_CTRL_MAGIC; ++i) { }
      if (*mp == HEAP_CTRL_MAGIC) {
         g_hc = c;
         os_unfair_lock_unlock(&g_init_lock);
         return;
      }
   }
   /* First copy: reserve the control block (zero-filled by the kernel). */
   mach_vm_address_t addr = 0;
   if (mach_vm_allocate(mach_task_self(), &addr, sizeof(struct heap_ctrl),
                        VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
      os_unfair_lock_unlock(&g_init_lock);
      return;   /* leaves g_hc NULL; malloc will return NULL (caller handles) */
   }
   struct heap_ctrl *c = (struct heap_ctrl *)(uintptr_t)addr;
   c->lock = OS_UNFAIR_LOCK_INIT;
   __sync_synchronize();
   c->magic = HEAP_CTRL_MAGIC;
   /* Publish g_hc BEFORE setenv: setenv strdups its value via malloc, which
    * re-enters this copy's malloc -> heap_init. With g_hc already set, that
    * re-entrant call hits the lock-free fast path and returns immediately
    * instead of deadlocking on the non-recursive g_init_lock. */
   g_hc = c;
   char buf[32];
   snprintf(buf, sizeof buf, "0x%llx", (unsigned long long)(uintptr_t)c);
   setenv(HEAP_CTRL_ENV, buf, 1);
   os_unfair_lock_unlock(&g_init_lock);
}

static size_t round_up(size_t n, size_t a) {
   return (n + (a - 1)) & ~(a - 1);
}

/* Reserve one more low-4GB region with room for `need` bytes in ONE block.
 * Caller holds g_hc->lock. Returns the new region or NULL if the address window
 * cannot accommodate it.
 *
 * ⚠ A region is the unit a single allocation must fit inside, so its size cannot
 * be a fixed constant: a request larger than HEAP_SIZE could never be served no
 * matter how much total arena was free, and it failed SILENTLY. Portal 2's
 * libvstdlib static initializers ask for exactly 1 GiB in one call, against
 * HEAP_SIZE = 768 MB with 1.5 GB free across two regions — served NULL, which
 * Source turns into Plat_ExitProcess -> _exit(0) (a silent status-0 death).
 *
 * So size the mapping to the caller: prefer HEAP_SIZE, so ordinary small
 * allocations keep amortising one mapping over many blocks, but grow it when a
 * single request needs more, and if the window no longer has room for the
 * preferred size, fall back to the smallest mapping that still serves the
 * request rather than failing outright. */
static struct region *add_region(size_t need) {
   if (g_hc->nregions >= MAX_REGIONS) {
      return NULL;
   }
   const size_t gran = 2UL * 1024 * 1024;      /* keep mappings 2MB-aligned */
   size_t least = round_up(need ? need : 1, gran);
   /* Kill switch (guard tests-i386 lowmem-big-alloc): restore the old
    * fixed-size, primary-window-only behaviour so the defect arm can be
    * reproduced on demand — a request bigger than one region gets NULL. */
   static int fixed_regions = -1;
   if (fixed_regions < 0) {
      fixed_regions = getenv("M64_HEAP_FIXED_REGIONS") ? 1 : 0;
   }
   if (fixed_regions && least > HEAP_SIZE) { return NULL; }
   const size_t want  = least > HEAP_SIZE ? least : HEAP_SIZE;
   const size_t primary  = (size_t)(HEAP_SCAN_HI - HEAP_SCAN_LO);
   const size_t overflow = (size_t)(HEAP_OVERFLOW_HI - HEAP_OVERFLOW_LO);
   const size_t window   = primary > overflow ? primary : overflow;
   if (least > window) {
      return NULL;                 /* larger than any low-4GB band we have */
   }
   /* An OVERSIZED region is placed as HIGH in its band as it will go, and the
    * ordinary ones stay low: growing both from the same end would interleave
    * them, and one 256 MB region landing in the middle of the window is enough to
    * make every later GiB-scale request unsatisfiable. Big-high/small-low keeps
    * the large contiguous span at the top intact for as long as possible. */
   const int from_top = (least > HEAP_SIZE);
   /* BELOW 2 GB FIRST. Real 32-bit Darwin handed out heap addresses under
    * 0x80000000, and i386 code leans on it: a pointer is POSITIVE as a signed
    * int. PvZ's reanim image lookup takes `Image*` or a small id through
    * `if (id <= 1000) return base + (id-1)*20` (signed): a 0x8xxxxxxx heap
    * pointer is negative, passes, and the result is wild (SIGSEGV entering
    * Adventure mode). The upper window is the spill. Guard heap-below-2g;
    * M64_HEAP_HIGH_FIRST=1 restores the old order. */
   static int high_first = -1;
   if (high_first < 0) { high_first = getenv("M64_HEAP_HIGH_FIRST") ? 1 : 0; }
   for (int band = 0; band < (fixed_regions ? 1 : 2); ++band) {
      const int primary_band = (fixed_regions || high_first) ? (band == 0) : (band == 1);
      const uintptr_t blo = primary_band ? HEAP_SCAN_LO : HEAP_OVERFLOW_LO;
      const uintptr_t bhi = primary_band ? HEAP_SCAN_HI : HEAP_OVERFLOW_HI;
      for (int pass = 0; pass < 2; ++pass) {
         const size_t rsize = (pass == 0) ? want : least;
         if (pass == 1 && least >= want) { break; }  /* nothing smaller to try */
         if (rsize > (size_t)(bhi - blo)) { continue; }
         const uintptr_t last = (uintptr_t)(bhi - rsize);
         for (uintptr_t i = blo; i <= last; i += HEAP_STEP) {
            /* Same candidate set, walked from whichever end suits this region. */
            const uintptr_t a = from_top ? (last - (i - blo)) : i;
            mach_vm_address_t addr = a;
            if (mach_vm_allocate(mach_task_self(), &addr, rsize,
                                 VM_FLAGS_FIXED) == KERN_SUCCESS) {
               struct region *r = &g_hc->regions[g_hc->nregions++];
               r->base = (char *)(uintptr_t)addr;
               r->cur  = r->base;
               r->end  = r->base + rsize;
               if (getenv("ABICONV_HEAP_TRACE")) {
                  fprintf(stderr,
                          "[heap] region %d: [0x%llx,0x%llx) %llu MB  band=%s\n",
                          g_hc->nregions - 1, (unsigned long long)(uintptr_t)r->base,
                          (unsigned long long)(uintptr_t)r->end,
                          (unsigned long long)(rsize / (1024 * 1024)),
                          primary_band ? "high" : "low");
               }
               return r;
            }
         }
      }
   }
   return NULL;
}

/* True if p points into the payload area of some region we own. */
static int owned(const void *p) {
   if (!g_hc) { return 0; }
   uintptr_t a = (uintptr_t)p;
   for (int i = 0; i < g_hc->nregions; ++i) {
      if (a > (uintptr_t)g_hc->regions[i].base
          && a <= (uintptr_t)g_hc->regions[i].end) {
         return 1;
      }
   }
   return 0;
}

/* Carve a fresh block of `cap` payload bytes whose payload is aligned to
 * `align` (>=16). Caller holds g_hc->lock. Writes the block header immediately
 * before the returned payload so free()/realloc() recover it at p-sizeof. */
/* Name WHO asked for the allocation that could not be served.
 *
 * A size that cannot be served is useless on its own: the interesting question is
 * always which code computed it, and a mistranslated size looks exactly like a
 * legitimate one. The request arrives through a bridge from translated code, whose
 * frames do not unwind (no x86_64 unwind info), so backtrace() stops short. But the
 * `call` that reached the bridge still pushed its return address, and translated
 * dylibs ARE real loaded images -- so dladdr resolves a stack word to
 * "<image>+0x<off>", which is exactly what pcmap-diff.py needs to recover the
 * original i386 site. Same technique as exit-trace-probe.c's stack scan.
 *
 * ⚠ TWO LIMITS, both measured on Portal 2 (2026-09-12), so nobody reads this output
 * more confidently than it deserves:
 *   - dladdr resolves ANY address inside an image's mapped range, including Mach-O
 *     header and data words, so a hit is not necessarily code. Check the offset
 *     against the image's __text start before believing it (libvstdlib+0x740 looked
 *     like a call site and is below __text at 0x...e90).
 *   - a TRANSLATED static initializer runs on the low-4GB init stack, not this one
 *     (see init_high_stack_bug), so its frames are NOT here at all. What survives is
 *     the native chain (the bridge, and the translated dylib frames that the bridge
 *     re-entered), which is usually enough to name the requesting LIBRARY but not
 *     the instruction. For the instruction, the exit-trace probe's scan plus
 *     pcmap-diff.py is the current route. */
/* Is `v` inside the __TEXT,__text of the image dladdr attributed it to? dladdr
 * resolves ANY address in an image's mapped range, so a Mach-O header or a data
 * word answers with an image and a plausible offset -- which is how
 * libvstdlib+0x740 (below __text at +0xe90) read as a call site. Checking the
 * actual code section is what makes a reported word worth believing. */
static int addr_is_code(const void *v, const Dl_info *info)
{
   if (info->dli_fbase == NULL) { return 0; }
   const struct mach_header_64 *mh = (const struct mach_header_64 *)info->dli_fbase;
   if (mh->magic != MH_MAGIC_64) { return 0; }
   unsigned long sz = 0;
   const uint8_t *txt = getsectiondata(mh, "__TEXT", "__text", &sz);
   if (txt == NULL || sz == 0) { return 0; }
   return ((const uint8_t *)v >= txt && (const uint8_t *)v < txt + sz);
}

static void heap_report_requesters(void)
{
   /* ⚠ Walk the stack in FOUR-byte steps, not eight. Translated code keeps an
    * i386-granular stack even on the native stack -- a call is emitted as
    * `pushw %ax; pushw %ax; movl %r11d,(%rsp); jmp target`, i.e. a 4-byte return
    * address. Stepping 8 bytes only catches the ones that happen to land aligned,
    * which is why an earlier version of this report saw a couple of translated
    * frames and missed the rest of the chain. */
   const uint32_t *sp = (const uint32_t *)__builtin_frame_address(0);
   int words = 2048;   /* 8 KB at 4 bytes/step */
   /* Never read past the stack's own mapping: the main thread runs near the
    * top of the wrapper's low 16 MB stack, so a fixed 8 KB scan faulted at the
    * region end and turned an ALLOCATION FAILED report into a SIGSEGV here
    * (Quinn 2026-09-26, fault 0x82236000 = wrapper stack top). */
   const uint64_t span = cb_readable_span((uint64_t)(uintptr_t)sp, (uint64_t)words * 4);
   if ((uint64_t)words * 4 > span) { words = (int)(span / 4); }
   int shown = 0;
   const int maxshow = 40;
   fprintf(stderr, "[heap]   who asked (stack words pointing INTO __TEXT,__text of "
                   "a non-system image; live and dead frames are mixed, so this is "
                   "a candidate set, not a call chain):\n");
   for (int i = 0; i < words && shown < maxshow; ++i) {
      void *v = (void *)(uintptr_t)sp[i];
      if (v == NULL) { continue; }
      Dl_info info;
      if (dladdr(v, &info) == 0 || info.dli_fname == NULL) { continue; }
      if (strstr(info.dli_fname, "/usr/lib/") != NULL ||
          strstr(info.dli_fname, "/System/") != NULL ||
          strstr(info.dli_fname, "libabiconv") != NULL) { continue; }
      if (!addr_is_code(v, &info)) { continue; }   /* header/data word, not a site */
      const char *base = strrchr(info.dli_fname, '/');
      base = base ? base + 1 : info.dli_fname;
      fprintf(stderr, "[heap]     [sp+0x%03x] %p  %s+0x%lx  %s\n",
              (unsigned)(i * 4), v, base,
              (unsigned long)((char *)v - (char *)info.dli_fbase),
              info.dli_sname ? info.dli_sname : "(no symbol)");
      ++shown;
   }
   if (shown == 0) {
      fprintf(stderr, "[heap]     (none found)\n");
   }

   /* Now the stack that actually matters. A translated static initializer runs on
    * the i386 init stack, so its frames are NOT on the native stack scanned above
    * -- that is why the native scan can name the requesting library but never the
    * instruction. Return addresses there are i386, i.e. FOUR bytes, so this walks
    * 32-bit words. The stack grows DOWN from the base, so walking down from the
    * base runs oldest-to-newest. */
   uintptr_t istop = 0, isbot = 0;
   if (_86x64_init_stack_range(&istop, &isbot)) {
      const int iwords = 4096;          /* 16 KB of stack */
      fprintf(stderr, "[heap]   i386 init-stack frames (base %#lx, newest LAST; "
                      "feed an offset to pcmap-diff.py):\n",
              (unsigned long)istop);
      int ishown = 0;
      for (int i = 1; i <= iwords && ishown < 40; ++i) {
         const uintptr_t at = istop - (uintptr_t)i * 4;
         if (at < isbot) { break; }
         const uint32_t v = *(const volatile uint32_t *)at;
         if (v == 0) { continue; }
         Dl_info info;
         if (dladdr((void *)(uintptr_t)v, &info) == 0 || info.dli_fname == NULL) { continue; }
         if (strstr(info.dli_fname, "/usr/lib/") != NULL ||
             strstr(info.dli_fname, "/System/") != NULL ||
             strstr(info.dli_fname, "libabiconv") != NULL) { continue; }
         if (!addr_is_code((const void *)(uintptr_t)v, &info)) { continue; }
         const char *base = strrchr(info.dli_fname, '/');
         base = base ? base + 1 : info.dli_fname;
         fprintf(stderr, "[heap]     [%#lx] %#010x  %s+0x%lx  %s\n",
                 (unsigned long)at, v, base,
                 (unsigned long)((uintptr_t)v - (uintptr_t)info.dli_fbase),
                 info.dli_sname ? info.dli_sname : "(no symbol)");
         ++ishown;
      }
      if (ishown == 0) {
         fprintf(stderr, "[heap]     (no translated frames found)\n");
      }
   }
   fflush(stderr);
}

static void *bump(size_t cap, size_t align) {
   if (align < 16) align = 16;
   for (int attempt = 0; attempt < 2; ++attempt) {
      for (int i = 0; i < g_hc->nregions; ++i) {
         struct region *r = &g_hc->regions[i];
         uintptr_t cur = (uintptr_t)r->cur;
         /* payload must clear a header and satisfy alignment */
         uintptr_t payload = round_up(cur + sizeof(struct block), align);
         uintptr_t next = payload + cap;
         if (next <= (uintptr_t)r->end) {
            struct block *b = (struct block *)(payload - sizeof(struct block));
            b->size = cap;
            b->next = NULL;
            r->cur = (char *)next;
            return (void *)payload;
         }
      }
      /* No region had room — reserve another, sized for THIS block, and retry
       * once. The request must include the header and worst-case alignment
       * slack, or a region sized to `cap` alone would still not fit the block. */
      if (!add_region(sizeof(struct block) + align + cap)) break;
   }
   /* ── A FAILED LOW-4GB ALLOCATION MUST SPEAK ───────────────────────────────
    * Returning NULL here is almost always terminal, and the way it kills the
    * process makes it invisible: a Source-engine target routes the NULL into
    * CStdMemAlloc::SetCRTAllocFailed -> Plat_ExitProcess -> _exit(0), so the
    * app vanishes with status 0, no output and no crash report (Portal 2,
    * 2026-09-12: libvstdlib's static initializers died exactly this way).
    * Anything that ends the process deserves one line saying why, so report the

    * request that could not be served and what the arena had left. Same
    * principle as fault_report_shim.c. */
   {
      static int reported = 0;
      if (!reported || getenv("ABICONV_HEAP_TRACE")) {
         reported = 1;
         size_t freebytes = 0;
         for (int i = 0; i < g_hc->nregions; ++i) {
            freebytes += (size_t)((uintptr_t)g_hc->regions[i].end -
                                  (uintptr_t)g_hc->regions[i].cur);
         }
         fprintf(stderr,
                 "[heap] ALLOCATION FAILED: %llu bytes (align %llu) — the "
                 "low-4GB arena cannot serve it.\n"
                 "[heap]   regions %d/%d of %llu MB each, %llu bytes free in "
                 "the tail; window [0x%lx,0x%lx)\n"
                 "[heap]   a single request larger than one region can NEVER be "
                 "served — raise HEAP_SIZE or add regions.\n",
                 (unsigned long long)cap, (unsigned long long)align,
                 g_hc->nregions, MAX_REGIONS,
                 (unsigned long long)(HEAP_SIZE / (1024 * 1024)),
                 (unsigned long long)freebytes,
                 (unsigned long)HEAP_SCAN_LO, (unsigned long)HEAP_SCAN_HI);
         heap_report_requesters();
      }
   }
   return NULL;
}

void *malloc(size_t n) {
   if (n == 0) n = 16;
   const size_t cap = round_up(n, 16);
   heap_init();
   if (!g_hc) { errno = ENOMEM; return NULL; }
   os_unfair_lock_lock(&g_hc->lock);
   if (g_hc->nregions == 0 && !add_region(sizeof(struct block) + 16 + cap)) {
      os_unfair_lock_unlock(&g_hc->lock);
      errno = ENOMEM;
      return NULL;
   }
   /* first-fit reuse (16-aligned blocks always satisfy default alignment).
    *
    * ⚠ Blocks are never split or coalesced, so a recycled block keeps its whole
    * capacity. Without a fit cap, a 16-byte request takes a just-freed 4 MB
    * block, and the next 4 MB request bumps fresh arena: libabiconv mallocs and
    * frees a 4 MB low stack PER native->i386 callback (cb_bridge, ae_shim) and
    * 256 KB ones elsewhere, so every callback followed by a long-lived small
    * allocation leaked 4 MB. Quinn hosting a network game (a CFSocket callback
    * per packet) exhausted the whole low-4GB window in 4 minutes (2026-09-26).
    * So: reuse a block only if it wastes at most max(cap, 4 KB). Kill switch
    * ABICONV_NO_HEAP_FIT_CAP=1 restores the uncapped first fit. */
   static int fit_cap = -1;
   if (fit_cap < 0) { fit_cap = getenv("ABICONV_NO_HEAP_FIT_CAP") == NULL; }
   const size_t slack = cap > 4096 ? cap : 4096;
   /* ponytail: linear free-list scan; size-class bins if skipped blocks pile up */
   for (struct block **pp = &g_hc->free_list; *pp != NULL; pp = &(*pp)->next) {
      if (BLK_CAP(*pp) >= cap && (!fit_cap || BLK_CAP(*pp) - cap <= slack)) {
         struct block *b = *pp;
         *pp = b->next;
         b->size = BLK_CAP(b);          /* clear FREED: block is live again */
         os_unfair_lock_unlock(&g_hc->lock);
         return (char *)b + sizeof(struct block);
      }
   }
   void *p = bump(cap, 16);
   os_unfair_lock_unlock(&g_hc->lock);
   if (!p) errno = ENOMEM;
   /* Large-allocation trail: the SEQUENCE of big requests distinguishes a
    * legitimate one-off reservation from a doubling probe (and a doubling probe
    * from a garbage size produced by a mistranslated length). */
   if (cap >= (64UL << 20) && getenv("ABICONV_HEAP_TRACE")) {
      fprintf(stderr, "[heap] large malloc %llu bytes (0x%llx) -> %p\n",
              (unsigned long long)cap, (unsigned long long)cap, p);
   }
   return p;
}

void free(void *p) {
   if (p == NULL) return;
   heap_init();
   if (!g_hc) { return; }
   os_unfair_lock_lock(&g_hc->lock);
   /* Ignore pointers that aren't ours: a stray or double free from the
    * translated program must not be allowed to corrupt the free list. */
   if (!owned(p)) {
      os_unfair_lock_unlock(&g_hc->lock);
      return;
   }
   struct block *b = (struct block *)((char *)p - sizeof(struct block));
   /* Double-free guard: a block already on the free list carries FREED.
    * Pushing it again would alias it out to two callers (see BLK_FREED_BIT).
    * Ignore the second free, exactly as a hardened allocator would. */
   if (b->size & BLK_FREED_BIT) {
      static _Atomic unsigned warned;
      if (getenv("ABICONV_HEAP_TRACE") &&
          __c11_atomic_fetch_add(&warned, 1, __ATOMIC_RELAXED) < 32) {
         fprintf(stderr, "[heap] double-free ignored: p=%p cap=%llu\n",
                 p, (unsigned long long)BLK_CAP(b));
      }
      os_unfair_lock_unlock(&g_hc->lock);
      return;
   }
   b->size |= BLK_FREED_BIT;
   /* Diagnostic poison: stamp freed payload (header is before p, untouched) so
    * a use-after-free read shows 0xCD bytes instead of stale/zero data — this
    * distinguishes UAF from an explicit zero write at the fault site. Off
    * unless ABICONV_HEAP_TRACE is set. */
   if (getenv("ABICONV_HEAP_TRACE")) { memset(p, 0xCD, BLK_CAP(b)); }
   b->next = g_hc->free_list;
   g_hc->free_list = b;
   os_unfair_lock_unlock(&g_hc->lock);
}

void *calloc(size_t count, size_t size) {
   size_t n;
   if (__builtin_mul_overflow(count, size, &n)) {
      errno = ENOMEM;
      return NULL;
   }
   void *p = malloc(n);
   if (p != NULL) {
      memset(p, 0, n);
   }
   return p;
}

void *realloc(void *p, size_t n) {
   if (p == NULL) {
      return malloc(n);
   }
   if (n == 0) {
      free(p);
      return malloc(0);
   }
   /* If it isn't ours, we can't safely inspect its header; allocate fresh.
    * (We can't know the old size to copy, so this only preserves data for
    * our own pointers — which is all the translated program should pass.) */
   heap_init();
   if (!g_hc) { return malloc(n); }
   os_unfair_lock_lock(&g_hc->lock);
   int mine = owned(p);
   uint64_t oldcap = 0;
   if (mine) oldcap = BLK_CAP((struct block *)((char *)p - sizeof(struct block)));
   os_unfair_lock_unlock(&g_hc->lock);

   if (mine && oldcap >= round_up(n, 16)) {
      return p;   /* current block already big enough */
   }
   void *np = malloc(n);
   if (np != NULL && mine) {
      memcpy(np, p, oldcap < n ? oldcap : n);
      free(p);
   }
   return np;
}

/* reallocf: like realloc but frees the original on failure (BSD/macOS). */
void *reallocf(void *p, size_t n) {
   void *np = realloc(p, n);
   if (!np && p) free(p);
   return np;
}

/* Aligned allocators. These bypass the free list (recycled blocks may not
 * meet the requested alignment) and bump a freshly aligned block. */
int posix_memalign(void **out, size_t align, size_t n) {
   if (out == NULL) return EINVAL;
   /* alignment must be a power of two and a multiple of sizeof(void*) */
   if (align < sizeof(void *) || (align & (align - 1)) != 0) {
      return EINVAL;
   }
   if (n == 0) { *out = NULL; return 0; }
   heap_init();
   if (!g_hc) { return ENOMEM; }
   os_unfair_lock_lock(&g_hc->lock);
   if (g_hc->nregions == 0) {
      add_region(sizeof(struct block) + align + round_up(n, 16));
   }
   void *p = bump(round_up(n, 16), align);
   os_unfair_lock_unlock(&g_hc->lock);
   if (!p) return ENOMEM;
   *out = p;
   return 0;
}

void *aligned_alloc(size_t align, size_t n) {
   void *p = NULL;
   if (posix_memalign(&p, align, n) != 0) return NULL;
   return p;
}

void *valloc(size_t n) {
   void *p = NULL;
   if (posix_memalign(&p, 4096, n) != 0) return NULL;
   return p;
}

/* malloc_size / malloc_good_size: Foundation and CF occasionally query the
 * usable size of a block. Report the payload capacity for our pointers, 0
 * for anything we didn't allocate. */
size_t malloc_size(const void *p) {
   if (p == NULL) return 0;
   heap_init();
   if (!g_hc) { return 0; }
   os_unfair_lock_lock(&g_hc->lock);
   size_t sz = 0;
   if (owned(p)) {
      sz = (size_t)((struct block *)((char *)p - sizeof(struct block)))->size;
   }
   os_unfair_lock_unlock(&g_hc->lock);
   return sz;
}

size_t malloc_good_size(size_t n) {
   return round_up(n == 0 ? 16 : n, 16);
}

/* ---- CFAllocator family -> low-4GB heap ------------------------------------
 * CFAllocatorAllocate/Reallocate/Deallocate are abigen-generated by default,
 * so they marshal the i386 call into NATIVE CoreFoundation. The default/system/
 * malloc CFAllocators then hand back addresses well above 4 GB, which the i386
 * program truncates into its 32-bit pointer slots. Later freeing that truncated
 * value via the native default malloc zone aborts the process inside
 * malloc_zone_error ("pointer being freed was not allocated" / "double free" /
 * "incorrect checksum") — the dominant non-deterministic iPhoto startup crash,
 * detected at __CFAllocatorDeallocate but rooted in the >4GB allocation.
 *
 * Routing these three through THIS heap (the same fix malloc_shim already
 * applies to malloc/free) makes every CFAllocator block low-4GB-representable
 * AND freed by the same allocator that produced it. It is the exact analogue of
 * the malloc/free interception and is universal: any i386 binary that calls
 * CFAllocatorAllocate hits the truncation otherwise.
 *
 * These are wired via custom.syms (excluded from abigen) + the MTSHIM
 * trampolines in maptable_tramp.asm, so they keep the export names the i386
 * binds already point at (___CFAllocator*) — a libabiconv-only change, no
 * retranslate. The allocator argument (a[0]) is deliberately ignored: the
 * default/system/malloc allocators are all just malloc, and bypassing a custom
 * allocator's bookkeeping for a DIRECT CFAllocatorAllocate call is benign (the
 * far more common CFAllocatorCreate path, used to hand a custom allocator to CF
 * object creators, stays abigen-generated and unaffected). CFAllocatorDeallocate
 * frees only blocks we own; a foreign/Null-allocator pointer is ignored rather
 * than passed to a native free that would abort — leak-over-crash, matching
 * free()'s own non-owned policy.
 *
 * i386 cdecl arg slots (MTSHIM hands a = &args[0], 4-byte slots; CFIndex and
 * CFOptionFlags are both 4 bytes on i386):
 *   CFAllocatorAllocate(alloc, size, hint)        -> a[0],a[1],a[2]
 *   CFAllocatorReallocate(alloc, ptr, size, hint) -> a[0],a[1],a[2],a[3]
 *   CFAllocatorDeallocate(alloc, ptr)             -> a[0],a[1]
 */
uint32_t shim_CFAllocatorAllocate(uint32_t *a) {
   int32_t size = (int32_t)a[1];
   if (size <= 0) { return 0; }            /* CF returns NULL for size 0 */
   return (uint32_t)(uintptr_t)malloc((size_t)size);
}

uint32_t shim_CFAllocatorReallocate(uint32_t *a) {
   void   *ptr  = (void *)(uintptr_t)a[1];
   int32_t size = (int32_t)a[2];
   if (size <= 0) {                        /* CF: size 0 => deallocate + NULL */
      if (ptr && owned(ptr)) { free(ptr); }
      return 0;
   }
   return (uint32_t)(uintptr_t)realloc(ptr, (size_t)size);
}

uint32_t shim_CFAllocatorDeallocate(uint32_t *a) {
   void *ptr = (void *)(uintptr_t)a[1];
   if (ptr && owned(ptr)) { free(ptr); }
   /* non-owned (foreign / kCFAllocatorNull no-copy / truncated): ignore — a
    * native free would abort, our heap's free already ignores non-owned. */
   return 0;
}
