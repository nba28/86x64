/*
 * objc_slide.c — auto slide-fixup for translated i386 __OBJC metadata.
 *
 * The 86x64 translator preserves __OBJC,__message_refs / __cls_refs /
 * __class / __module_info / etc. as 4-byte slots (matching i386's
 * instruction stride). dyld's REBASE_TYPE_POINTER is 8 bytes in x86_64
 * so the translator SKIPS emitting rebase entries for these sections.
 * Slots therefore contain the post-translate-time (unslid) M64 vmaddr.
 * At load time each translated binary may get a non-zero slide; without
 * an early per-image walk, the slot values are still pointing at the
 * preferred vmaddr and Tessera-class static initializers dispatch to
 * garbage on their first objc_msgSend.
 *
 * Originally a separate DYLD_INSERT_LIBRARIES shim
 * (/tmp/objcslideshim/objcslideshim.dylib). Folded in here so any
 * translated dylib that links libabiconv (which is all of them — the
 * pipeline inserts an LC_LOAD_DYLIB for libabiconv) auto-gets the fixup
 * without per-app deploy gymnastics or env-var setup.
 *
 * Runs once per image via _dyld_register_func_for_add_image — the
 * callback fires for every already-loaded image at registration and for
 * every future load, BEFORE the loaded image's own static initializers
 * run. That ordering is the whole point: the wrapper exec's
 * fixup_translated_dylib_slots in wrapper_setup.c runs from main(),
 * which is AFTER dyld init pass, too late for Tessera-style C++ static
 * initializers that dispatch on first reach.
 *
 * Bounded scope: walks ONLY __OBJC sections, where every 4-byte slot is
 * known to be an intra-image pointer. Other 4-byte sections
 * (__nl_symbol_ptr, __la_symbol_ptr) hold pre-bound dyld targets in
 * other dylibs (libsystem, etc.); sliding them would corrupt the bind.
 * Those need the wrapper-side fixup_translated_dylib_slots range check.
 */
#include <mach-o/loader.h>
#include <mach-o/dyld.h>
#include <mach-o/nlist.h>
#include <mach-o/getsect.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <runetype.h>
#include "dyld_image_list.h"

static int g_verbose = 0;

/* The host libc's rune-locale table, used to seed the i386-layout copy below. */
extern _RuneLocale _DefaultRuneLocale;

/* Defined in objc_shim.c — index this image's legacy ObjC-1.0 classes so the
 * objc bridge can dispatch class messages to their own translated IMPs. */
extern void _86x64_objc_index_legacy_classes(const struct mach_header_64 *mh,
                                             intptr_t slide);
/* Register this image's legacy classes with the modern objc runtime so AppKit/
 * Foundation can message them (principal class, delegates, NSClassFromString). */
extern void _86x64_objc_register_classes(const struct mach_header_64 *mh,
                                         intptr_t slide);

/* Cross-copy per-image processing claim (objc_shim.c, shared objc_shared_ctrl).
 * Returns 1 to the FIRST libabiconv copy that reaches a given mach_header, 0 to
 * every later copy — so a second co-located copy's add-image re-scan does NOT
 * re-walk an image another copy already processed (the multicopy re-scan race:
 * a not-yet-ready pointer field read back NULL and dereferenced at +offset, the
 * intermittent slide_objc `[rbx+0x88]` rbx=0 crash on Civ IV). Returns 1 when
 * there is no shared ctrl (single-copy deploys), so the per-copy g_processed
 * set below remains the sole guard there. */
extern int _86x64_objc_shared_claim_image(const void *mh);

/* Classic __DATA,__dyld crt-bootstrap shims (dyld_func_lookup.asm). The crt's
 * func_lookup slot is redirected to _86x64_dyld_func_lookup, which writes the
 * address of _86x64_dyld_noop into the crt's out-parameter so the bootstrap is
 * harmlessly satisfied with the i386 cdecl stack ABI (vs. dyld's native
 * legacyDyldLookup4OldBinaries reading args from registers -> NULL write). See
 * patch_dyld_section below. */
extern void _86x64_dyld_func_lookup(void);
extern void _86x64_dyld_noop(void);

/* ---------------------------------------------------------------------------
 * Low-4GB stack for translated static initializers (see the init high-stack bug
 * and init_trampoline.asm). dyld runs __mod_init_func initializers before the
 * wrapper exec installs its low stack; under Rosetta the main-thread stack is
 * >4GB, which i386 4-byte frame pointers can't represent. We rewrite each
 * translated image's init-func pointers to a JIT stub that funnels through
 * _abiconv_init_trampoline, which switches to this low stack.
 * ------------------------------------------------------------------------- */

/* Consumed by init_trampoline.asm. */
uint64_t g_init_shadow_sp     = 0;   /* grows down; 64 bytes per nesting level */
uint64_t g_init_low_stack_top = 0;   /* 16-aligned top of the low-4GB init stack */

/* ── Is the init phase REALLY single-threaded? ─────────────────────────────
 * init_trampoline.asm is correct only under that assumption, and it is stated
 * there as a fact rather than enforced: g_init_shadow_sp is ONE LIFO cursor,
 * and every entry arriving on a >4GB stack resets rsp to the SAME
 * g_init_low_stack_top. Two threads inside at once therefore descend onto each
 * other's frames, and .landing restores an rsp its own thread never saved.
 *
 * ⚠ The damage is NOT confined to translated code. A frame established on the
 * low init stack whose rsp is later restored to the real >4GB stack leaves a
 * TRUNCATED-looking rbp (low 32 bits only) while rsp keeps its high half, and
 * whatever runs next on that thread -- including a NATIVE dylib's initializer,
 * e.g. a GPU driver loaded by a dlopen on a second thread -- stores through it
 * into unmapped low memory. That is an rbp/rsp desync reported against someone
 * else's correct code, which is exactly the kind of symptom this project has
 * repeatedly chased into the wrong library.
 *
 * So MEASURE the assumption instead of trusting it. Two words, no lock on the
 * fast path; a violation is latched and reported once. */
uint64_t g_init_depth     = 0;   /* nesting levels currently live */
uint64_t g_init_owner_tid = 0;   /* mach tid that owns the low init stack */
uint64_t g_init_race_seen = 0;   /* latched: a second thread was observed inside */

/* Called immediately around abiconv_call_init. Returns nothing; a detected race
 * is reported once and latched for the fault reporter to print. */
static void init_enter(void *target) {
   uint64_t me = (uint64_t)pthread_mach_thread_np(pthread_self());
   uint64_t owner = g_init_owner_tid;
   if (g_init_depth != 0 && owner != 0 && owner != me) {
      if (__sync_bool_compare_and_swap(&g_init_race_seen, 0, me)) {
         fprintf(stderr, "[abiconv] ⚠ INIT-STACK RACE: tid %llu entered the "
                 "low-4GB init stack while tid %llu is %llu level(s) deep "
                 "(target=%p). The shadow save-stack and the stack TOP are both "
                 "process-global, so these two threads are now sharing frames.\n",
                 (unsigned long long)me, (unsigned long long)owner,
                 (unsigned long long)g_init_depth, target);
         fflush(stderr);
      }
   }
   g_init_owner_tid = me;
   __sync_fetch_and_add(&g_init_depth, 1);
}

static void init_leave(void) {
   __sync_fetch_and_sub(&g_init_depth, 1);
}
extern void abiconv_init_trampoline(void);
/* Run one translated __mod_init_func on the low-4GB init stack and return (see
 * init_trampoline.asm). Used by the run-now init path below (default; ABICONV_NO_RUN_INITS opts out). */
extern void abiconv_call_init(void *target, long argc, char **argv,
                              char **envp, char **apple);

/* The init stack lives at a FIXED low address shared by every co-located
 * libabiconv copy, allocated with mach_vm_allocate (NOT the shim malloc).
 *
 * Two pitfalls drove this design:
 *   - Drawing it from the shim's malloc heap forces EVERY copy's constructor to
 *     allocate a full 768MB heap region just to get 8MB; the ~1.6GB low window
 *     only fits ~2 such regions, so the 3rd+ copy aborts (calloc returns NULL).
 *   - A fixed mach_vm region placed inside the heap's scan window
 *     [0x88000000,0xF0000000) clips one of those 768MB regions, also aborting.
 * So we sit in the bottom 128MB band [0x80000000,0x88000000), which malloc_shim
 * explicitly avoids (HEAP_SCAN_LO=0x88000000) — neither breaking nor forcing a
 * heap region. dyld init is single-threaded: the first copy maps it + stamps a
 * magic marker; later copies see it mapped, verify the marker, and share it.
 * Sharing is fine — nested inits keep descending whatever low stack we already
 * switched to (init_trampoline.asm only resets to the top when rsp is >4GB).
 * The bottom band also holds the proxy arenas (objc_shim.c probes up from
 * 0x80000000) and the wrapper's main stack, but those probe first-fit from the
 * bottom and skip our already-mapped block; 0x87000000 is near the top of the
 * band, clear of their low-end footprint. */
#define INIT_STACK_ADDR  0x087000000UL              /* bottom band, above arenas */
#define INIT_STACK_SZ    (8U * 1024U * 1024U)       /* 8 MB ([0x87000000,0x87800000)) */
#define INIT_STACK_MAGIC 0x38367834696e6974ULL      /* sentinel stamped at base */

/* Bump allocator for per-slot JIT stubs (MAP_JIT RWX; may live >4GB — stubs
 * are reached via dyld's 8-byte __mod_init_func pointers). */
static uint8_t *g_stub_region = NULL;
static size_t   g_stub_used   = 0;
static size_t   g_stub_cap    = 0;

/* One-time setup of this copy's low init stack + shadow save-stack. Returns 0
 * on success. Safe to call repeatedly (idempotent per copy). */
/* The i386 init stack's live range, for diagnostics that need to walk TRANSLATED
 * frames. A translated static initializer runs on this stack, not the native one,
 * so a native backtrace or stack scan cannot see its callers at all -- which is
 * why a failing allocation could name the requesting library but never the
 * instruction (malloc_shim.c). Returns 0 unless the stack is really ours, checked
 * by the sentinel, so a caller never walks a stranger's mapping.
 *
 * `*top` is the stack BASE (highest address); it grows DOWN from there, so the
 * newest frame is at the LOWEST used address and a walk from *top downward runs
 * oldest-to-newest. Return addresses here are i386, i.e. FOUR bytes. */
int _86x64_init_stack_range(uintptr_t *top, uintptr_t *bottom) {
   if (!g_init_low_stack_top) { return 0; }
   if (*(volatile uint64_t *)INIT_STACK_ADDR != INIT_STACK_MAGIC) { return 0; }
   if (top)    { *top    = (uintptr_t)g_init_low_stack_top; }
   if (bottom) { *bottom = (uintptr_t)INIT_STACK_ADDR; }
   return 1;
}

static int init_stack_setup(void) {
   if (g_init_low_stack_top) { return 0; }

   /* The trampoline's .landing is used as a 4-byte i386 return address, so the
    * trampoline (i.e. libabiconv) MUST be mapped below 4GB. If it isn't, we
    * can't safely wrap — bail and leave the (buggy) status quo rather than
    * corrupt returns. */
   if ((uintptr_t)&abiconv_init_trampoline >> 32) {
      if (g_verbose) {
         fprintf(stderr, "abiconv init_stack: libabiconv mapped >4GB (%p); "
                 "init-stack switch disabled\n", (void *)&abiconv_init_trampoline);
      }
      return -1;
   }

   /* Map (or reuse) the shared init stack in the bottom band. */
   mach_vm_address_t addr = INIT_STACK_ADDR;
   kern_return_t kr = mach_vm_allocate(mach_task_self(), &addr, INIT_STACK_SZ,
                                       VM_FLAGS_FIXED);
   if (kr == KERN_SUCCESS) {
      /* First copy: stamp the marker at the base (stack grows DOWN from top). */
      *(volatile uint64_t *)INIT_STACK_ADDR = INIT_STACK_MAGIC;
   } else {
      /* Already mapped — presumably a sibling copy's shared stack. Reading is
       * safe (mapped, which is why allocate failed); verify the marker. */
      if (*(volatile uint64_t *)INIT_STACK_ADDR != INIT_STACK_MAGIC) {
         if (g_verbose) {
            fprintf(stderr, "abiconv init_stack: 0x%lx occupied by a non-shared "
                    "mapping; init-stack switch disabled\n",
                    (unsigned long)INIT_STACK_ADDR);
         }
         return -1;
      }
   }
   /* Top, 16-aligned. High bits are already 0 (low region). */
   g_init_low_stack_top = (INIT_STACK_ADDR + INIT_STACK_SZ) & ~(uint64_t)0xF;

   /* Shadow save-stack: tiny, anywhere. Use mach_vm too so we never trigger a
    * shim heap region just for init bookkeeping. */
   const size_t SHADOW_SZ = 0x10000;                /* 64KB = 1024 nesting levels */
   mach_vm_address_t shadow = 0;
   if (mach_vm_allocate(mach_task_self(), &shadow, SHADOW_SZ,
                        VM_FLAGS_ANYWHERE) != KERN_SUCCESS) {
      g_init_low_stack_top = 0;
      return -1;
   }
   g_init_shadow_sp = (uint64_t)(uintptr_t)shadow + SHADOW_SZ;
   return 0;
}

/* Build a per-slot JIT stub:  movabs r11,target ; movabs rax,tramp ; jmp rax */
static void *make_init_stub(void *target) {
   if (!g_stub_region || g_stub_used + 32 > g_stub_cap) {
      const size_t CHUNK = 64U * 1024U;
      void *r = mmap(NULL, CHUNK, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
      if (r == MAP_FAILED) {
         r = mmap(NULL, CHUNK, PROT_READ | PROT_WRITE | PROT_EXEC,
                  MAP_PRIVATE | MAP_ANON, -1, 0);
      }
      if (r == MAP_FAILED) { return NULL; }
      g_stub_region = (uint8_t *)r;
      g_stub_used = 0;
      g_stub_cap = CHUNK;
   }
   uint8_t *s = g_stub_region + g_stub_used;
   g_stub_used += 32;
   size_t k = 0;
   s[k++] = 0x49; s[k++] = 0xBB;                          /* movabs r11, imm64 */
   memcpy(s + k, &target, 8); k += 8;
   uint64_t tramp = (uint64_t)(uintptr_t)&abiconv_init_trampoline;
   s[k++] = 0x48; s[k++] = 0xB8;                          /* movabs rax, imm64 */
   memcpy(s + k, &tramp, 8); k += 8;
   s[k++] = 0xFF; s[k++] = 0xE0;                          /* jmp rax */
   return s;
}

/* Is this a TRANSLATED image (vs. a native system dylib)? Every binary the
 * pipeline emits links libabiconv (an LC_LOAD_DYLIB is inserted); native
 * dylibs do not, and libabiconv itself does not depend on itself. This is the
 * "is translated" signal for images WITHOUT an __OBJC segment — pure C++ GCC
 * dylibs (e.g. Portal 2's libtier0) — which the objc_seg gate misses. We must
 * only wrap the init funcs of translated images: a native initializer expects
 * the native ABI and would break if funnelled through the i386 low-stack
 * trampoline. */
static int image_links_libabiconv(const struct mach_header_64 *mh64) {
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB ||
          lc->cmd == LC_REEXPORT_DYLIB || lc->cmd == LC_LOAD_UPWARD_DYLIB) {
         const struct dylib_command *dc = (const struct dylib_command *)p;
         const char *name = (const char *)p + dc->dylib.name.offset;
         if (strstr(name, "libabiconv")) { return 1; }
      }
      p += lc->cmdsize;
   }
   return 0;
}

/* Per-process set of mach_headers we have already fully processed in slide_objc
 * (slid + initialized). dyld invokes our add-image callback once per image, but
 * in run-now mode (default) we ALSO process an image's dependencies proactively
 * (bottom-up) before running its own initializers — see process_deps below — so
 * an image can be reached before dyld delivers its own callback. This set makes
 * the later real callback a no-op and prevents double-init / double-register.
 * Inert in the default (non-RUN_INITS) path: there each image is processed
 * exactly once, by its own callback. */
#define MAX_PROCESSED_IMAGES 8192
static const struct mach_header *g_processed[MAX_PROCESSED_IMAGES];
static size_t g_n_processed = 0;
static int already_processed(const struct mach_header *mh) {
   for (size_t i = 0; i < g_n_processed; i++) {
      if (g_processed[i] == mh) { return 1; }
   }
   return 0;
}
static void mark_processed(const struct mach_header *mh) {
   if (g_n_processed < MAX_PROCESSED_IMAGES) { g_processed[g_n_processed++] = mh; }
}

/* ⚠ Must NOT use dyld's indexed APIs: this runs from the add-image callback and
 * from translated initializers, i.e. while another image is mid-load, and querying
 * an in-flight entry ABORTS the process. dyld_image_list.c answers from infoArray,
 * which also LISTS the in-flight image -- the property that keeps the dependency
 * lookup here working. (A self-maintained registry does NOT: a dependency is
 * mapped before the dependent's callback fires, so the registry misses it, which
 * regressed this to 10/10 SIGSEGV.) */
static const struct mach_header *find_loaded_image(const char *leaf,
                                                   intptr_t *slide_out) {
   return x64_img_find_leaf(leaf, slide_out);
}

/* Max translated __mod_init_func pointers we collect per image in run-now
 * mode (the default) before running them at the end of slide_objc. Overflow
 * falls back to the make_init_stub wrapping path — but on dyld4 those
 * out-of-image stubs are SILENTLY SKIPPED (the same in-image validation that
 * motivates run-now, see the wrap_mod_init_funcs comment), so any init beyond
 * the cap simply never runs. The cap must therefore comfortably exceed any real image's init count:
 * Civ IV's main dylib has 1063 __mod_init_func entries (libtier0 has 11), so the
 * former 1024 cap dropped ~39 of Civ IV's initializers. 4096 (a 32 KiB on-stack
 * pointer array in slide_objc) covers it with wide margin. */
#define INIT_COLLECT_MAX 4096

/* Rewrite every __DATA,__mod_init_func 8-byte pointer in a translated image to
 * a stack-switching stub. Runs from the add-image callback, BEFORE dyld reads
 * __mod_init_func to run the initializers.
 *
 * In run-now mode (the default) we do NOT run the initializers here — running
 * them mid-walk is too early (the image's other per-image fixups, esp. the
 * 4-byte __DATA pointer relocation and __OBJC slide, haven't happened yet, so a
 * C++ static ctor would dereference an unslid pointer — see
 * the Portal 2 translation notes s3b/s4). Instead we COLLECT the target addresses into
 * `collect[]` and NULL the slots (so neither dyld nor a nested re-entry runs
 * them); slide_objc runs the collected targets at its very END, after all
 * fixups. */
/* Read the 8-byte on-disk value at file offset `fo` of `imgname` (a thin
 * translated dylib, mach_header at offset 0). Returns 0 on any error. Used to
 * recover __mod_init_func entries clobbered by dyld's classic-__dyld overflow. */
static uint64_t read_image_qword(const char *imgname, uint64_t fo) {
   if (!imgname) { return 0; }
   int fd = open(imgname, O_RDONLY);
   if (fd < 0) { return 0; }
   uint64_t v = 0;
   if (pread(fd, &v, 8, (off_t)fo) != 8) { v = 0; }
   close(fd);
   return v;
}

static void wrap_mod_init_funcs(const struct mach_header_64 *mh64,
                                intptr_t slide, const char *imgname,
                                uint64_t text_lo, uint64_t text_hi,
                                void **collect, size_t *n_collect) {
   if (getenv("ABICONV_NO_INIT_STACKSWITCH")) { return; }
   if (init_stack_setup() != 0) { return; }

   /* Structural precondition for the classic __DATA,__dyld 8-byte overflow: an
    * 8-byte __dyld section whose END vmaddr equals a __mod_init_func's START
    * vmaddr means dyld's 8-byte func_lookup write at __dyld+8 lands on
    * __mod_init_func[0]. Precompute the set of clobbered init-section vmaddrs so
    * the per-slot recovery below can fire even when the clobbered value is a
    * LOW-4GB address (dyld may write a shared-cache pointer whose HIGH half —
    * e.g. 0x00007ff8 — ends up as init[0]'s value, below the >4GB threshold the
    * original recovery keyed on). Universal: triggers on the section adjacency,
    * not an app name. */
   /* dyld's func_lookup write is an 8-byte store at __dyld+8, i.e. the vmaddr
    * range [dyld.addr+8, dyld.addr+16). Any __mod_init_func[0] slot (8 bytes at
    * its section start) that INTERSECTS this range has its low bytes clobbered
    * — this is true even when 8-byte alignment leaves a 4-byte gap between the
    * i386-sized 8-byte __dyld and __mod_init_func (SFWordProcessing: __dyld ends
    * at +0x944, __mod_init_func starts at +0x948; the write at +0x944..+0x94c
    * still overwrites init[0]'s low 4 bytes at +0x948). */
   uint64_t dyld_wr_lo = 0, dyld_wr_hi = 0;  /* [lo,hi) of the __dyld+8 store (0 = none) */
   {
      const uint8_t *pp = (const uint8_t *)(mh64 + 1);
      for (uint32_t ci = 0; ci < mh64->ncmds; ci++) {
         const struct load_command *clc = (const struct load_command *)pp;
         if (clc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *cseg =
               (const struct segment_command_64 *)pp;
            const struct section_64 *csect =
               (const struct section_64 *)(cseg + 1);
            for (uint32_t cs = 0; cs < cseg->nsects; cs++, csect++) {
               if (strncmp(csect->sectname, "__dyld", 16) == 0 &&
                   csect->size == 8) {
                  dyld_wr_lo = csect->addr + 8;
                  dyld_wr_hi = csect->addr + 16;
               }
            }
         }
         pp += clc->cmdsize;
      }
   }

   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
         const struct section_64 *sect = (const struct section_64 *)(seg + 1);
         for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
            const uint32_t type = sect->flags & SECTION_TYPE;
            if (type != S_MOD_INIT_FUNC_POINTERS) { continue; }
            /* Is THIS the init section dyld's __dyld+8 write clobbers slot[0] of?
             * The 8-byte store [dyld_wr_lo,dyld_wr_hi) overlaps slot[0]'s 8-byte
             * range [sect->addr, sect->addr+8). */
            const int dyld_clobbers_slot0 =
               (dyld_wr_lo != 0 &&
                dyld_wr_lo < sect->addr + 8 && sect->addr < dyld_wr_hi);
            uintptr_t base = (uintptr_t)sect->addr + (uintptr_t)slide;
            size_t n = (size_t)sect->size / sizeof(void *);
            const size_t page = 4096;
            uintptr_t a0 = base & ~(uintptr_t)(page - 1);
            uintptr_t a1 = (base + sect->size + page - 1) & ~(uintptr_t)(page - 1);
            if (mprotect((void *)a0, a1 - a0, PROT_READ | PROT_WRITE) != 0) {
               if (g_verbose) {
                  fprintf(stderr, "abiconv init_stack: mprotect RW failed for "
                          "%s __mod_init_func: %s\n", imgname, strerror(errno));
               }
               continue;
            }
            void **slots = (void **)base;
            size_t wrapped = 0;
            /* Two strategies (see the Portal 2 translation notes s3b):
             *  - DEFAULT (run-now; was opt-in as ABICONV_RUN_INITS): COLLECT
             *    each init target, NULL the slot so dyld skips it, and let
             *    slide_objc run the collected list at its very END (after all
             *    per-image fixups), on the low-4GB init stack. This sidesteps
             *    dyld4's in-image validation entirely. Per-image, in
             *    image-load (≈bottom-up dependency) order; nested dlopen during
             *    an init re-enters here and the trampoline's shadow stack keeps
             *    nesting LIFO-correct. NULLed slots also make multi-copy
             *    deploys idempotent: a later libabiconv copy's re-scan finds
             *    nothing left to collect (vs. the stub path's stub-of-stub
             *    re-wrapping).
             *  - ABICONV_NO_RUN_INITS (legacy stub path, opt-out): rewrite each
             *    slot to a low-stack JIT stub and let dyld call it. Works on
             *    the dyld that shipped with the iPhoto-era macOS, but dyld4
             *    VALIDATES __mod_init_func entries are in-image and SILENTLY
             *    SKIPS our out-of-image stubs → the initializers never run.
             *    Civ IV s26 (2026-07-06): with run-now still opt-in, any launch
             *    that forgot ABICONV_RUN_INITS=1 (m64 run, Finder) silently
             *    lost ALL 1063 static ctors on this path; the zeroed static
             *    std::set header (GameRanger MSG_Mac sCallbackList) then
             *    crashed _Rb_tree_decrement at NULL+4 in main→CheckPreferences
             *    →InitGameRanger. Run-now is therefore the DEFAULT — the env
             *    var must not be a correctness switch. */
            const int run_now =
               collect != NULL && getenv("ABICONV_NO_RUN_INITS") == NULL;
            for (size_t j = 0; j < n; j++) {
               if (!slots[j]) { continue; }
               /* Skip patch_dyld_section's __dyld+8 artifact (multi-copy
                * re-scan). patch_dyld_section stamps the i386-cdecl
                * func_lookup shim into the 8-BYTE view of the classic
                * __DATA,__dyld func_lookup slot (__dyld+8) — which, in the
                * common Csu layout where the i386-sized 8-byte __dyld section
                * is immediately followed by __mod_init_func, IS
                * __mod_init_func[0]. Within one libabiconv copy the order is
                * safe (this collector runs first, NULLs the slot, then the
                * patch re-dirties it), but every OTHER loaded libabiconv copy
                * re-scans the image (the processed-set is per-copy) and finds
                * the slot non-NULL again, holding the first copy's shim — a
                * LOW-4GB address the >4GB clobber-recovery below can't flag.
                * Collected and run as an initializer it writes the noop fn-ptr
                * through a NULL i386 out-arg (argc=0/argv=NULL frame) -> NULL
                * write. (Halo CE + its bundled translated QuickTime, whose
                * embedded libabiconv copy is the second instance; 2026-07-05.)
                * A REAL translated init always points into its own image's
                * executable range; a slot holding the exact exported entry of
                * _86x64_dyld_func_lookup (any libabiconv copy) is that
                * artifact — the real initializer was already recovered,
                * collected, and run by the copy that patched. Skip; do NOT
                * recover from disk (that would run the real init twice). */
               {
                  const uintptr_t v = (uintptr_t)slots[j];
                  if (v < 0x100000000ULL &&
                      (v - (uintptr_t)slide < (uintptr_t)text_lo ||
                       v - (uintptr_t)slide >= (uintptr_t)text_hi)) {
                     Dl_info di;
                     if (dladdr((void *)v, &di) && di.dli_saddr == (void *)v &&
                         di.dli_sname != NULL &&
                         strcmp(di.dli_sname, "_86x64_dyld_func_lookup") == 0) {
                        if (g_verbose) {
                           fprintf(stderr, "abiconv init_stack: skipped "
                                   "__mod_init_func[%zu] in %s: __dyld+8 "
                                   "func_lookup shim artifact (already "
                                   "collected by an earlier libabiconv "
                                   "copy)\n", j, imgname);
                        }
                        continue;
                     }
                  }
               }
               /* Recover a __mod_init_func entry clobbered by dyld's classic
                * __DATA,__dyld overflow. A pre-10.5 i386 binary's __dyld section
                * is 8 bytes (two 4-byte slots: lazy-binder, func_lookup), but a
                * 64-bit dyld writes the classic linkage as two 8-BYTE pointers —
                * lazy-binder at __dyld+0 and func_lookup
                * (legacyDyldLookup4OldBinaries) at __dyld+8. When __mod_init_func
                * immediately follows __dyld (the common Csu layout), __dyld+8 is
                * __mod_init_func[0], so dyld stamps legacyDyldLookup over the
                * first C++ static initializer. Running it jumps into dyld's
                * old-binary lookup with an i386 stack frame -> NULL write SIGSEGV.
                * A valid translated init is always intra-image (<4GB); a >4GB
                * entry is this clobber. Restore the real ctor from the on-disk
                * static value + slide. Universal (triggers on the structural
                * ">4GB __mod_init_func entry", not an app name); the crt's own
                * func_lookup read is handled separately by patch_dyld_section. */
               /* Recovery trigger. The clobber shows up two ways:
                *  - >4GB value: dyld wrote a shared-cache 8-byte pointer whose
                *    LOW half survived into the slot (original threshold).
                *  - LOW-4GB but bogus value at slot[0] of the init section that
                *    directly follows an 8-byte __dyld: the same 8-byte __dyld+8
                *    write, but the surviving bytes form a small value (e.g. the
                *    0x00007ff8 HIGH half of a native selector/dyld pointer) that
                *    is below 4GB yet still NOT a valid intra-image init target.
                *    (SFWordProcessing init[0] = 0x7ff8; SIGSEGV at 0x7ff8.)
                * The disk value + slide is the real ctor in both cases. */
               {
                  const uintptr_t v = (uintptr_t)slots[j];
                  const uint64_t vl = (uint64_t)v;
                  const uint64_t lo_l = text_lo + (uint64_t)(int64_t)slide;
                  const uint64_t hi_l = text_hi + (uint64_t)(int64_t)slide;
                  const int in_loaded_text = (vl >= lo_l && vl < hi_l);
                  const int in_pref_text   = (vl >= text_lo && vl < text_hi);
                  const int slot0_clobbered =
                     (j == 0 && dyld_clobbers_slot0 &&
                      !in_loaded_text && !in_pref_text);
                  if (v >= 0x100000000ULL || slot0_clobbered) {
                     uint64_t orig =
                        read_image_qword(imgname, (uint64_t)sect->offset + j * 8);
                     void *fixed = orig
                        ? (void *)(uintptr_t)(orig + (uint64_t)slide) : NULL;
                     if (g_verbose) {
                        fprintf(stderr, "abiconv init_stack: recovered clobbered "
                                "__mod_init_func[%zu] in %s: %p -> %p (dyld __dyld "
                                "8-byte overflow)\n", j, imgname, slots[j], fixed);
                     }
                     slots[j] = fixed;
                     if (!slots[j]) { continue; }
                  }
               }
               /* Recover an UNREBASED __mod_init_func entry. Not every
                * translated image's dyld info carries rebase entries for the
                * widened 8-byte init slots (a classic-origin route-C dylib
                * does — Civ IV covers all 1063 — but a modern-i386-origin
                * dylib can come out with an EMPTY rebase table), so the slot
                * still holds the on-disk PREFERRED address after dyld slides
                * the image; calling it faults on unmapped memory
                * (81_static_init_default). Structural check, image-agnostic:
                * a correct target lies in the LOADED text range
                * [text_lo+slide, text_hi+slide); a value inside the PREFERRED
                * range [text_lo, text_hi) but NOT the loaded range is the
                * unrebased on-disk value -> add the slide. No-op for
                * correctly rebased images and for slide==0, and leaves
                * anything else (already-slid targets) untouched. */
               if (slide != 0) {
                  const uint64_t t = (uint64_t)(uintptr_t)slots[j];
                  const uint64_t lo_l = text_lo + (uint64_t)(int64_t)slide;
                  const uint64_t hi_l = text_hi + (uint64_t)(int64_t)slide;
                  if (!(t >= lo_l && t < hi_l) &&
                      t >= text_lo && t < text_hi) {
                     void *fixed =
                        (void *)(uintptr_t)(t + (uint64_t)(int64_t)slide);
                     if (g_verbose) {
                        fprintf(stderr, "abiconv init_stack: slid unrebased "
                                "__mod_init_func[%zu] in %s: %p -> %p\n",
                                j, imgname, slots[j], fixed);
                     }
                     slots[j] = fixed;
                  }
               }
               if (run_now && *n_collect < INIT_COLLECT_MAX) {
                  /* Collect the target + NULL the slot FIRST so dyld (and a
                   * nested dlopen re-entry) won't also run it; slide_objc runs
                   * the collected list after all per-image fixups. */
                  collect[(*n_collect)++] = slots[j];
                  slots[j] = NULL;
                  ++wrapped;
               } else {
                  void *stub = make_init_stub(slots[j]);
                  if (stub) { slots[j] = stub; ++wrapped; }
               }
            }
            if (g_verbose) {
               fprintf(stderr, "abiconv init_stack: %s %zu/%zu init funcs "
                       "in %s\n", run_now ? "collected" : "wrapped",
                       wrapped, n, imgname);
            }
         }
      }
      p += lc->cmdsize;
   }
}

/* ---------------------------------------------------------------------------
 * __DATA,__mod_term_func: static TERMINATORS (the classic-GCC C++ static-dtor
 * runners; Darwin GCC put them here, not through __cxa_atexit).
 *
 * dyld4's Loader::findAndRunAllInitializers -> forEachTerminator registers
 * every entry RAW with native __cxa_atexit(func, NULL, mh) — an in-image
 * translated address passes its validation (unlike our old out-of-image init
 * stubs, which is what c2eb10a's run-now sidesteps for INITS). At exit(),
 * native __cxa_finalize_ranges CALLS the translated i386-ABI terminator
 * directly: the 64-bit call pushes an 8-byte return address, the terminator
 * body runs fine (the main stack is already low-4GB), but the translated i386
 * epilogue (movl (%rsp),%r11d; lea 4(%rsp),%rsp; jmp *%r11) pops only the LOW
 * 4 BYTES -> jumps to the truncated low half of the libsystem_c return
 * address -> EXC_BAD_ACCESS (Halo: 0x1c08aeb3 = low32 of
 * __cxa_finalize_ranges+319; lldb-confirmed registration by
 * dyld`forEachTerminator with rdi = __mod_term_func[0]+slide). Surfaced when
 * c2eb10a made ctors live and the disk-space fix let Halo reach its voluntary
 * exit(0); universal for EVERY translated image with a non-empty
 * __mod_term_func in any process that calls exit().
 *
 * Fix (the mod_TERM twin of the run-now init path, functionality-first):
 * from the add-image callback — which runs during libabiconv's initializer,
 * i.e. BEFORE dyld4 initializes the translated image and reads this section —
 * NULL each slot so dyld4 skips it (NULL entries are silently skipped exactly
 * like NULLed __mod_init_func slots; lldb-verified on Halo: zeroed slot ->
 * clean exit 0, no dyld diagnostics), and re-register the real terminator
 * through the reverse callback bridge: x64_cb_wrap(fn32, {0 args, void})
 * hands native __cxa_atexit a NATIVE trampoline that re-enters the translated
 * terminator on a low-4GB stack with a proper i386 4-byte return frame (the
 * same mechanism shim_cxa_atexit uses for dlsym'd dtors). Static dtors
 * therefore still RUN at exit, and in the classic dyld2 LIFO position
 * (registered before any app atexit -> run last). NULLed slots keep
 * multi-copy deploys idempotent: a later libabiconv copy's re-scan finds
 * nothing left to register. Slot-value recovery (>4GB dyld __dyld-overflow
 * clobber, unrebased widened 8-byte slots) mirrors wrap_mod_init_funcs.
 */
struct term_cb_sig { uint32_t nargs; uint32_t ret_kind; uint8_t arg_kinds[16]; };
extern uint64_t x64_cb_wrap(uint32_t fn32, const struct term_cb_sig *sig);
extern int __cxa_atexit(void (*func)(void *), void *arg, void *dso);

static void wrap_mod_term_funcs(const struct mach_header_64 *mh64,
                                intptr_t slide, const char *imgname,
                                uint64_t text_lo, uint64_t text_hi) {
   /* nargs=0, ret=CBR_VOID: the SysV shape of `void terminator(void)`.
    * Static storage: x64_cb_wrap keeps the sig POINTER in its binding. */
   static const struct term_cb_sig term_sig = { 0, 0, { 0 } };

   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
         const struct section_64 *sect = (const struct section_64 *)(seg + 1);
         for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
            const uint32_t type = sect->flags & SECTION_TYPE;
            if (type != S_MOD_TERM_FUNC_POINTERS) { continue; }
            uintptr_t base = (uintptr_t)sect->addr + (uintptr_t)slide;
            size_t n = (size_t)sect->size / sizeof(void *);
            const size_t page = 4096;
            uintptr_t a0 = base & ~(uintptr_t)(page - 1);
            uintptr_t a1 = (base + sect->size + page - 1) & ~(uintptr_t)(page - 1);
            if (mprotect((void *)a0, a1 - a0, PROT_READ | PROT_WRITE) != 0) {
               if (g_verbose) {
                  fprintf(stderr, "abiconv mod_term: mprotect RW failed for "
                          "%s __mod_term_func: %s\n", imgname, strerror(errno));
               }
               continue;
            }
            void **slots = (void **)base;
            size_t wrapped = 0;
            for (size_t j = 0; j < n; j++) {
               if (!slots[j]) { continue; }
               /* Recover a slot clobbered by dyld's classic-__dyld 8-byte
                * overflow (structurally possible whenever __mod_term_func
                * directly follows the i386-sized __dyld section; same cure as
                * the __mod_init_func case). */
               if ((uintptr_t)slots[j] >= 0x100000000ULL) {
                  uint64_t orig =
                     read_image_qword(imgname, (uint64_t)sect->offset + j * 8);
                  void *fixed = orig
                     ? (void *)(uintptr_t)(orig + (uint64_t)slide) : NULL;
                  if (g_verbose) {
                     fprintf(stderr, "abiconv mod_term: recovered clobbered "
                             "__mod_term_func[%zu] in %s: %p -> %p\n",
                             j, imgname, slots[j], fixed);
                  }
                  slots[j] = fixed;
                  if (!slots[j]) { continue; }
               }
               /* Recover an UNREBASED widened 8-byte slot (empty-rebase-table
                * modern-i386-origin dylibs; same structural check as the init
                * path: preferred-range-but-outside-loaded -> add the slide). */
               if (slide != 0) {
                  const uint64_t t = (uint64_t)(uintptr_t)slots[j];
                  const uint64_t lo_l = text_lo + (uint64_t)(int64_t)slide;
                  const uint64_t hi_l = text_hi + (uint64_t)(int64_t)slide;
                  if (!(t >= lo_l && t < hi_l) &&
                      t >= text_lo && t < text_hi) {
                     void *fixed =
                        (void *)(uintptr_t)(t + (uint64_t)(int64_t)slide);
                     if (g_verbose) {
                        fprintf(stderr, "abiconv mod_term: slid unrebased "
                                "__mod_term_func[%zu] in %s: %p -> %p\n",
                                j, imgname, slots[j], fixed);
                     }
                     slots[j] = fixed;
                  }
               }
               const uintptr_t fn = (uintptr_t)slots[j];
               /* NULL the slot FIRST: whatever happens below, dyld4 must
                * never register the raw i386-ABI address. */
               slots[j] = NULL;
               if (fn >> 32) {
                  if (g_verbose) {
                     fprintf(stderr, "abiconv mod_term: dropped >4GB "
                             "__mod_term_func[%zu] in %s (0x%lx)\n",
                             j, imgname, (unsigned long)fn);
                  }
                  continue;
               }
               uint64_t tramp = x64_cb_wrap((uint32_t)fn, &term_sig);
               if (!tramp) {          /* slots exhausted: drop, don't crash */
                  if (g_verbose) {
                     fprintf(stderr, "abiconv mod_term: cb slots exhausted; "
                             "dropped __mod_term_func[%zu] in %s\n",
                             j, imgname);
                  }
                  continue;
               }
               __cxa_atexit((void (*)(void *))(uintptr_t)tramp, NULL,
                            (void *)mh64);
               ++wrapped;
            }
            if (g_verbose) {
               fprintf(stderr, "abiconv mod_term: registered %zu/%zu term "
                       "funcs in %s via cb bridge\n", wrapped, n, imgname);
            }
         }
      }
      p += lc->cmdsize;
   }
}

static void slide_section_4byte(const char *imgname,
                                 const char *segname, const char *sectname,
                                 uint64_t vmaddr, uint64_t size,
                                 intptr_t slide,
                                 uint64_t vmaddr_lo, uint64_t vmaddr_hi) {
   if (size == 0) { return; }
   void *base = (void *)(uintptr_t)(vmaddr + slide);
   /* mprotect page-aligned: round base down, round size up to page. The
    * __OBJC segment is RW on disk but dyld can have mapped it RX by the
    * time we get the callback; toggle RW then back to RW (or RO on
    * request) after the writes. */
   const size_t page = 4096;
   uintptr_t addr = (uintptr_t)base;
   uintptr_t end  = addr + size;
   uintptr_t addr_aligned = addr & ~(uintptr_t)(page - 1);
   size_t len_aligned = ((end + page - 1) & ~(uintptr_t)(page - 1)) - addr_aligned;
   if (mprotect((void *)addr_aligned, len_aligned,
                PROT_READ | PROT_WRITE) != 0) {
      if (g_verbose) {
         fprintf(stderr, "abiconv objc_slide: mprotect RW failed at %p: %s\n",
                 (void *)addr_aligned, strerror(errno));
      }
      return;
   }

   uint32_t *p = (uint32_t *)base;
   size_t n = size / 4;
   size_t slid = 0;
   for (size_t i = 0; i < n; i++) {
      uint32_t v = p[i];
      /* Only slide slots whose CURRENT value is an intra-image pre-slide
       * vmaddr. This is what makes repeated callback firings idempotent:
       * once a slot has been slid (by an earlier callback, or by the
       * wrapper-side fixup), its value is the runtime address, which falls
       * OUTSIDE the pre-slide [vmaddr_lo,vmaddr_hi) span, so we skip it.
       * Without this guard a second pass computes v + 2*slide and truncates
       * to a wild low-4GB pointer — the classic objc_msgSend-to-garbage
       * crash. Null and non-pointer small values are naturally skipped. */
      if (v >= vmaddr_lo && v < vmaddr_hi) {
         p[i] = (uint32_t)((uint64_t)v + (uint64_t)slide);
         ++slid;
      }
   }

   /* Optionally lock RO so libobjc's legacy walker can't write uniqued
    * SEL pointers back into the 4-byte slots (which would truncate to
    * low 32 bits of an 8-byte SEL ptr → garbage cstring ptr on the
    * translated binary's next read of that slot). Defaults to leaving
    * RW because some legacy metadata layouts depend on libobjc's
    * mutability — only flip RO when troubleshooting. */
   if (getenv("ABICONV_OBJC_SLIDE_LOCK_RO")) {
      (void)mprotect((void *)addr_aligned, len_aligned, PROT_READ);
   }

   if (g_verbose) {
      fprintf(stderr, "abiconv objc_slide: slid %zu/%zu slots in %s,%s of %s\n",
              slid, n, segname, sectname, imgname);
   }
}

/* Slide the `str` pointer (offset 16) of each 32-byte x86_64 CFConstantString
 * record in __DATA,__cfstring. macho-tool's CFStringBlob expands the i386
 * 16-byte records to the x86_64 32-byte layout {isa:8, flags:8, str:8,
 * length:8} and embeds the UNSLID cstring vmaddr in `str`; the i386 source had
 * no dyld rebases, so dyld doesn't relocate it. Real CoreFoundation
 * dereferences str (e.g. __CFStringHash), so it must hold the runtime address.
 * isa is bound by dyld; flags/length are literals — only str needs sliding.
 * Idempotent + range-checked exactly like slide_section_4byte: a slot already
 * holding a runtime (post-slide) address falls outside the pre-slide span and
 * is skipped, so the N libabiconv copies' callbacks don't double-slide. */
static void slide_cfstrings(const struct mach_header_64 *mh64, intptr_t slide,
                            uint64_t vmaddr_lo, uint64_t vmaddr_hi,
                            const char *imgname) {
   const size_t rec = 32;            /* x86_64 record size */
   const size_t str_off = 16;        /* offset of the str field */
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg =
            (const struct segment_command_64 *)p;
         const struct section_64 *sect = (const struct section_64 *)(seg + 1);
         for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
            if (strncmp(sect->sectname, "__cfstring", 16) != 0) { continue; }
            if (sect->size == 0 || (sect->size % rec) != 0) { continue; }
            uint8_t *base = (uint8_t *)(uintptr_t)(sect->addr + slide);
            const size_t page = 4096;
            uintptr_t a = (uintptr_t)base, e = a + sect->size;
            uintptr_t aa = a & ~(uintptr_t)(page - 1);
            size_t la = ((e + page - 1) & ~(uintptr_t)(page - 1)) - aa;
            if (mprotect((void *)aa, la, PROT_READ | PROT_WRITE) != 0) {
               if (g_verbose) {
                  fprintf(stderr, "abiconv objc_slide: cfstring mprotect RW "
                          "failed at %p: %s\n", (void *)aa, strerror(errno));
               }
               continue;
            }
            size_t nrec = sect->size / rec, slid = 0;
            for (size_t r = 0; r < nrec; r++) {
               uint64_t *strp = (uint64_t *)(base + r * rec + str_off);
               uint64_t v = *strp;
               if (v >= vmaddr_lo && v < vmaddr_hi) {
                  *strp = (uint64_t)((int64_t)v + (int64_t)slide);
                  ++slid;
               }
            }
            if (g_verbose) {
               fprintf(stderr, "abiconv objc_slide: slid %zu/%zu cfstring str "
                       "ptrs in %s\n", slid, nrec, imgname);
            }
         }
      }
      p += lc->cmdsize;
   }
}

/* Repair + slide a legacy selector/class-ref section (__message_refs /
 * __cls_refs) from the ON-DISK original instead of the live slot value.
 *
 * Modern libobjc's old-ABI (__OBJC,__module_info) reader runs at map_images,
 * BEFORE our add-image callback, and uniques the selector refs by writing the
 * 8-byte native SEL back into each ref. The translated binary keeps these refs
 * as 4-byte slots (i386 stride), so each 8-byte write clobbers TWO 4-byte
 * slots: the low half lands in slot[i], the high half (0x00007ff8…) in
 * slot[i+1]. The translated code then reads slot[i+1] as a selector -> nil
 * sel -> objc_msgSend(obj,nil) -> jump to garbage. The original 4-byte cstring
 * pointers are unrecoverable from the corrupted live slots, but they are
 * intact in the file. We mmap the file, read each original 4-byte slot, and
 * (re)write original+slide into the live section — idempotent, and immune to
 * whatever libobjc did. Only the pure ref arrays are repaired this way; other
 * __OBJC sections (which our reverse registration legitimately mutates) keep
 * the in-place slide. */
static int repair_refs_from_file(const char *imgname,
                                 const char *sectname,
                                 uint64_t vmaddr, uint64_t fileoff,
                                 uint64_t size, intptr_t slide,
                                 uint64_t vmaddr_lo, uint64_t vmaddr_hi) {
   if (size == 0 || !imgname) { return 0; }
   int fd = open(imgname, O_RDONLY);
   if (fd < 0) { return 0; }
   struct stat stb;
   if (fstat(fd, &stb) != 0 || (uint64_t)stb.st_size < fileoff + size) {
      close(fd); return 0;
   }
   /* The translated image may be a thin slice inside a fat file, but the
    * wrapper-installed dylibs are thin (translate-bundle lipo-thins), so the
    * mach_header is at offset 0 and section file offsets are absolute. */
   void *fmap = mmap(NULL, (size_t)(fileoff + size), PROT_READ, MAP_PRIVATE, fd, 0);
   close(fd);
   if (fmap == MAP_FAILED) { return 0; }
   const uint32_t *orig = (const uint32_t *)((uintptr_t)fmap + fileoff);

   void *base = (void *)(uintptr_t)(vmaddr + slide);
   const size_t page = 4096;
   uintptr_t a = (uintptr_t)base, e = a + size;
   uintptr_t aa = a & ~(uintptr_t)(page - 1);
   size_t la = ((e + page - 1) & ~(uintptr_t)(page - 1)) - aa;
   if (mprotect((void *)aa, la, PROT_READ | PROT_WRITE) != 0) {
      munmap(fmap, (size_t)(fileoff + size)); return 0;
   }
   uint32_t *live = (uint32_t *)base;
   size_t n = size / 4, fixed = 0;
   for (size_t i = 0; i < n; i++) {
      uint32_t ov = orig[i];
      uint32_t want = (ov >= vmaddr_lo && ov < vmaddr_hi)
                          ? (uint32_t)((uint64_t)ov + (uint64_t)slide) : ov;
      if (live[i] != want) { live[i] = want; ++fixed; }
   }
   if (g_verbose && fixed) {
      fprintf(stderr, "abiconv objc_slide: repaired %zu/%zu slots in __OBJC,%s "
              "of %s from file\n", fixed, n, sectname, imgname);
   }
   munmap(fmap, (size_t)(fileoff + size));
   return 1;
}

/* Repair the SEL-name + type-encoding pointer slots inside classic ObjC1
 * method-list sections (__inst_meth / __cls_meth / __cat_inst_meth /
 * __cat_cls_meth) from the ON-DISK original.
 *
 * Same root cause as repair_refs_from_file, one level deeper. libobjc's old-ABI
 * reader (map_images, BEFORE this callback) uniques method-name selectors by
 * writing the 8-byte native SEL into the classic 4-byte legacy_objc_method.name
 * slot (name at +0); the write spans 8 bytes, so the high half (0x00007ff8…)
 * spills into the adjacent .types slot (+4). reverse_add_methods (objc_shim.c)
 * later READS meth.name as a cstring and SKIPS any method whose name fails
 * legacy_cstr_ok, and reads meth.types for the ObjC encoding — so a clobbered
 * category method (iPhoto -[ArchiveAlbum projectUUID], a category accessor) is
 * either never registered ("unrecognized selector") or registered with a bogus
 * encoding. The corruption is heap/ASLR-layout dependent (the clobber value),
 * hence intermittent; and, like the refs, it is independent of the image slide.
 * The in-place slide cannot recover a clobbered slot; the file is pristine.
 *
 * We walk each PACKED method_list { uint32 obsolete; int32 count; method[count] }
 * (method = { name, types, imp }, i386 4-byte stride) using the file — its
 * count fields are integers and are never SEL-clobbered (the 8-byte name write
 * only reaches name+types of its OWN entry, never a list header or imp) — and
 * restore each entry's name(+0) and types(+4) slots to original+slide.
 * imp(+8) is PRESERVED: it is never clobbered (outside the 8-byte name write)
 * and holds the authoritative translated IMP that reverse_add_methods records
 * for reverse dispatch. Idempotent (only rewrites a slot whose live value
 * differs) and fully bounded against the section; a malformed count stops the
 * walk (fail-safe: under-repair rather than corrupt). Runs in the same phase as
 * repair_refs_from_file — after the in-place slide, BEFORE the legacy-class
 * indexing/registration that consumes these lists. */
static int repair_method_lists_from_file(const char *imgname,
                                         const char *sectname,
                                         uint64_t vmaddr, uint64_t fileoff,
                                         uint64_t size, intptr_t slide,
                                         uint64_t vmaddr_lo, uint64_t vmaddr_hi) {
   if (size < 8 || !imgname) { return 0; }
   int fd = open(imgname, O_RDONLY);
   if (fd < 0) { return 0; }
   struct stat stb;
   if (fstat(fd, &stb) != 0 || (uint64_t)stb.st_size < fileoff + size) {
      close(fd); return 0;
   }
   void *fmap = mmap(NULL, (size_t)(fileoff + size), PROT_READ, MAP_PRIVATE, fd, 0);
   close(fd);
   if (fmap == MAP_FAILED) { return 0; }
   const uint32_t *orig = (const uint32_t *)((uintptr_t)fmap + fileoff);

   void *base = (void *)(uintptr_t)(vmaddr + slide);
   const size_t page = 4096;
   uintptr_t a = (uintptr_t)base, e = a + size;
   uintptr_t aa = a & ~(uintptr_t)(page - 1);
   size_t la = ((e + page - 1) & ~(uintptr_t)(page - 1)) - aa;
   if (mprotect((void *)aa, la, PROT_READ | PROT_WRITE) != 0) {
      munmap(fmap, (size_t)(fileoff + size)); return 0;
   }
   uint32_t *live = (uint32_t *)base;
   size_t n = size / 4;                 /* whole-section 4-byte slot count */
   size_t i = 0, fixed = 0, lists = 0;
   /* Walk packed method_lists from the pristine file image. */
   while (i + 2 <= n) {
      int32_t count = (int32_t)orig[i + 1];        /* header: [obsolete][count] */
      if (count <= 0 || count > 100000) { break; } /* malformed -> fail-safe stop */
      size_t entries = i + 2;
      if ((size_t)count > (n - entries) / 3) { break; }  /* overflow guard */
      for (int32_t k = 0; k < count; ++k) {
         size_t nm = entries + (size_t)k * 3;       /* name(+0), types(+1); imp(+2) kept */
         for (size_t s = nm; s <= nm + 1; ++s) {
            uint32_t ov = orig[s];
            uint32_t want = (ov >= vmaddr_lo && ov < vmaddr_hi)
                                ? (uint32_t)((uint64_t)ov + (uint64_t)slide) : ov;
            if (live[s] != want) { live[s] = want; ++fixed; }
         }
      }
      i = entries + (size_t)count * 3;
      ++lists;
   }
   if (g_verbose && fixed) {
      fprintf(stderr, "abiconv objc_slide: repaired %zu name/types slots across "
              "%zu method-lists in __OBJC,%s of %s from file\n",
              fixed, lists, sectname, imgname);
   }
   munmap(fmap, (size_t)(fileoff + size));
   return 1;
}

/* Slide 4-byte __DATA pointer slots that point into this image's own __TEXT —
 * i.e. C++ vtable entries and function-pointer tables (and switch/PIC fnptr
 * arrays). macho-tool emits these as 4-byte `Immediate` blobs holding the new
 * preferred vmaddr; dyld CANNOT rebase a 4-byte slot (both classic dyld and
 * dyld4 reject 64-bit local relocs with r_length!=3 — "bad local relocation
 * length"), and we can't widen 4->8 because the translated code reads them with
 * a 4-byte load on baked-in i386 struct layouts. So they're left at the unslid
 * preferred vmaddr → a C++ static ctor that stores/derefs a vtable ptr jumps to
 * a wild low-4GB address (Portal 2's libtier0 g_CmdLine, the Portal 2 translation notes
 * s4). The image loads <4GB (translated dylibs map low), so a 4-byte slot CAN
 * hold the slid value — slide it here.
 *
 * Discriminator = macho-tool's EXACT DataParser pointer predicate (section.cc):
 * value lands in the image's pre-slide vmaddr span [vmaddr_lo,vmaddr_hi) AND,
 * for a NON-executable (data) target, is 4-byte aligned (misaligned data-range
 * values are integer/fixed-point constants — counts, sizes, hash seeds — not
 * pointers); executable (__TEXT) targets need no alignment. Replicating the
 * translate-time predicate slides exactly the set macho-tool resolved as
 * intra-image pointers, so it covers BOTH the DATA->TEXT vtable/fnptr case AND
 * pure DATA->DATA global pointers (ptr-to-__DATA-global, e.g. Portal 2's
 * g_pMemAlloc = &s_StdMemAlloc) with zero divergence from translate-time and no
 * new false positives. Idempotent across the N libabiconv copies' callbacks: an
 * already-slid slot's value falls below vmaddr_lo and is skipped. Walks
 * WRITABLE, non-__OBJC sections,
 * skipping dyld-managed pointer sections (symbol-pointer / mod-init-func, which
 * are 8-byte and rebased elsewhere) and __cfstring (handled by slide_cfstrings). */
/* Slide the CODE-embedded absolute addresses of a translated image:
 *   - `[disp32(,idx,scale)]` absolute-indexed memory operands (i386 switch
 *     dispatch, global-array reads AND writes) whose disp32 macho-tool
 *     rewrote to the image's pre-slide M64 vmaddr at convert time;
 *   - `mov [mem], imm32` stores whose imm32 is such an address;
 *   - the 4-byte pointer slots of __TEXT,__const (switch jump tables).
 * wrapper_setup.c's fixup_translated_dylib_slots does the same scan, but it
 * only runs at WRAPPER ENTRY (build_i386_main_frame) — AFTER dyld inits, and
 * in run-now mode (default) the collected static ctors run at ADD-IMAGE time
 * (end of slide_objc), i.e. BEFORE the wrapper's pass. Civ IV s21:
 * NiAnimationSDM's ctor -> NiStaticDataManager::AddLibrary stores through
 * `67 89 14 85 <disp32>` (mov [disp32+eax*4], edx; ms_apfnInitFunctions in
 * __common) with the disp32 still unslid -> SIGSEGV at the preferred vmaddr.
 * The store opcode 0x89 was ALSO missing from the wrapper's pattern table
 * (only FF/4, FF/2, 8B, 03), so even the late pass never fixed it.
 * This pass runs in the add-image callback, before any collected init.
 * Idempotent vs the wrapper's later scan and across the N co-located
 * libabiconv copies' callbacks: both patch only values inside the PRE-slide
 * span [vmaddr_lo,vmaddr_hi); an already-slid value falls outside and is
 * skipped. __TEXT pages are toggled RW and restored to RX exactly like the
 * wrapper does. */
static void patch_text_abs32(const struct mach_header_64 *mh64, intptr_t slide,
                             uint64_t vmaddr_lo, uint64_t vmaddr_hi,
                             const char *imgname) {
   if (slide == 0 || vmaddr_lo >= vmaddr_hi) { return; }

   /* EXACT-TABLE PATH. A current-pipeline translation carries
    * __DATA,__86x64_abs32 (Archive::inject_abs32_section): the tool's own
    * list of every 4-byte __TEXT field that holds a pre-slide intra-image
    * absolute address (abs [disp32+idx*scale] operand disps + __TEXT,__const
    * pointer slots). Walk it and add the slide — no byte-pattern scan at all.
    * The scan below stays ONLY as the legacy fallback for older translations:
    * on Civ IV (Steam) it produced 10 pass-1 + 5 pass-2 PHANTOM matches that
    * straddled real instructions and corrupted them when "patched" (a
    * `c7 45 00` train formed by a call-sim jmp rel32 tail + the landing
    * `movl %eax,disp32(%rip)` became `mov [rbp+0],imm32` -> the landing
    * decayed to garbage -> the boost.python registration wall), plus 33
    * integer constants aliasing the image span (MD5 IVs 0x10325476 among
    * them) silently slid. Idempotent across the N libabiconv copies and the
    * wrapper's later pass: a slid value falls outside the pre-slide window
    * and is skipped. ABICONV_ABS32_NO_TABLE=1 forces the legacy scan (A/B
    * debugging). */
   if (getenv("ABICONV_ABS32_NO_TABLE") == NULL) {
      unsigned long absz = 0;
      const uint8_t *ab = getsectiondata(mh64, "__DATA", "__86x64_abs32", &absz);
      if (ab != NULL && absz >= 8 &&
          ((const uint32_t *)ab)[0] == 0x32336261u /* "ab32" */) {
         uint32_t cnt = ((const uint32_t *)ab)[1];
         if ((unsigned long)cnt * 4 + 8 > absz) {
            cnt = (uint32_t)((absz - 8) / 4);
         }
         const uint32_t *sites = (const uint32_t *)(ab + 8);
         size_t patched = 0;
         /* Entries are sorted. Do NOT mprotect one [first,last] span: the
          * table covers __text AND __TEXT,__const, and the pages BETWEEN them
          * (__unwind_info/__gcc_except_tab/...) can refuse PROT_WRITE under
          * Rosetta -> a single 27MB mprotect fails ENOMEM (observed on Civ IV)
          * and nothing gets slid. Instead cluster consecutive sites (new
          * cluster when the gap exceeds 16 pages) and RW exactly each
          * cluster's page range — mirroring the per-section mprotects the
          * legacy scan always did. */
         for (uint32_t i = 0; i < cnt; ) {
            uint32_t j = i + 1;
            uintptr_t clo = (uintptr_t)sites[i] + (uintptr_t)slide;
            uintptr_t chi = clo + 4;
            while (j < cnt) {
               uintptr_t a = (uintptr_t)sites[j] + (uintptr_t)slide;
               if (a > chi + 16 * 0x1000) { break; }
               if (a + 4 > chi) { chi = a + 4; }
               ++j;
            }
            uintptr_t pg = clo & ~(uintptr_t)0xFFF;
            size_t pglen = ((chi + 0xFFF) & ~(uintptr_t)0xFFF) - pg;
            if (mprotect((void *)pg, pglen, PROT_READ | PROT_WRITE) != 0) {
               fprintf(stderr, "abiconv abs32-table: mprotect RW failed for "
                       "cluster %#lx+%#zx of %s: %s — %u site(s) left "
                       "UNPATCHED\n", (unsigned long)pg, pglen, imgname,
                       strerror(errno), (unsigned)(j - i));
               i = j;
               continue;
            }
            for (uint32_t k = i; k < j; k++) {
               uint32_t *f = (uint32_t *)((uintptr_t)sites[k] + (uintptr_t)slide);
               uint32_t v;
               memcpy(&v, f, sizeof v);
               if (v >= vmaddr_lo && v < vmaddr_hi) { /* still pre-slide */
                  v = (uint32_t)((uint64_t)v + (uint64_t)slide);
                  memcpy(f, &v, sizeof v);
                  ++patched;
               }
            }
            mprotect((void *)pg, pglen, PROT_READ | PROT_EXEC);
            i = j;
         }
         if (g_verbose) {
            fprintf(stderr, "abiconv abs32 table: slid %zu/%u site(s) in %s\n",
                    patched, (unsigned)cnt, imgname);
         }
         return;
      }
   }

   const uint8_t *lcp = (const uint8_t *)(mh64 + 1);
   for (uint32_t ci = 0; ci < mh64->ncmds; ci++) {
      const struct load_command *lc = (const struct load_command *)lcp;
      lcp += lc->cmdsize;
      if (lc->cmd != LC_SEGMENT_64) { continue; }
      const struct segment_command_64 *seg =
         (const struct segment_command_64 *)lc;
      if (strcmp(seg->segname, "__TEXT") != 0) { continue; }
      const struct section_64 *sect = (const struct section_64 *)(seg + 1);
      for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
         const int is_text  = strncmp(sect->sectname, "__text",  16) == 0;
         const int is_const = strncmp(sect->sectname, "__const", 16) == 0;
         if ((!is_text && !is_const) || sect->size == 0) { continue; }

         uintptr_t addr = (uintptr_t)(sect->addr + slide);
         size_t sz = (size_t)sect->size;

         /* IDEMPOTENCY GROUND TRUTH: the section's ON-DISK bytes. A site is
          * patched ONLY IF its in-memory value still equals the file value
          * (i.e. nobody patched it yet). The range check alone is NOT a
          * sufficient guard: with a small |slide| (an image loaded near its
          * preferred base) a once-slid value can land back inside the
          * pre-slide span, and the N co-located libabiconv copies each run
          * this callback — observed as libcrypto's 582 __TEXT,__const slots
          * patched TWICE (v+2*slide). Disk-compare is cross-copy safe with
          * no shared state, and also hardens the byte-pattern scan against
          * false positives (a random in-range byte train that something
          * already modified in memory is left alone). If the file cannot be
          * read, SKIP the section rather than risk a double patch. */
         /* mmap the on-disk bytes rather than shim-malloc(25MB)+pread: the
          * low-4GB shim heap can transiently fail a large alloc (ASLR-layout
          * dependent), and the resulting SILENT skip left the whole __text
          * unpatched — Civ IV's flaky face: the CRC-32 xor read its table at
          * the unslid preferred vmaddr (KERN_INVALID at 0x11fb5d1c) in the
          * runs where the alloc failed, while other runs patched fine. A
          * PROT_READ file mapping needs no heap and may land anywhere. */
         uint8_t *orig = NULL;
         void *omap = MAP_FAILED;
         size_t omap_len = 0;
         {
            int fd = open(imgname, O_RDONLY);
            if (fd >= 0) {
               const off_t foff = (off_t)sect->offset;
               const off_t aoff = foff & ~(off_t)0xFFF;
               omap_len = (size_t)(foff - aoff) + sz;
               omap = mmap(NULL, omap_len, PROT_READ, MAP_PRIVATE, fd, aoff);
               if (omap != MAP_FAILED) {
                  orig = (uint8_t *)omap + (foff - aoff);
               }
               close(fd);
            }
         }
         if (!orig) {
            /* NOT gated on g_verbose: a skipped section is a latent crash
             * (every abs32 site in it stays unslid). */
            fprintf(stderr, "abiconv text_abs32: cannot map on-disk "
                    "%s,%s of %s — section left UNPATCHED\n", seg->segname,
                    sect->sectname, imgname);
            continue;
         }
         uintptr_t pg = addr & ~(uintptr_t)0xFFF;
         size_t pglen = ((addr + sz + 0xFFF) & ~(uintptr_t)0xFFF) - pg;
         if (mprotect((void *)pg, pglen, PROT_READ | PROT_WRITE) != 0) {
            /* NOT gated on g_verbose (same latent-crash reasoning as above). */
            fprintf(stderr, "abiconv text_abs32: mprotect RW failed for "
                    "%s,%s of %s: %s — section left UNPATCHED\n", seg->segname,
                    sect->sectname, imgname, strerror(errno));
            munmap(omap, omap_len);
            continue;
         }

         /* Patch helper predicate: current memory value == on-disk value
          * (not yet patched) AND the disk value is a pre-slide intra-image
          * address. All pattern MATCHING below runs on the ON-DISK bytes so
          * a prior patch of a neighboring site can never desync the scan. */
         size_t patched = 0;
         uint8_t *mem = (uint8_t *)addr;
         if (is_const) {
            /* switch jump tables: aligned 4-byte pre-slide code pointers. */
            size_t n = sz / 4;
            for (size_t k = 0; k < n; k++) {
               uint32_t fv, mv;
               memcpy(&fv, orig + k * 4, 4);
               memcpy(&mv, mem + k * 4, 4);
               if (fv >= vmaddr_lo && fv < vmaddr_hi && mv == fv) {
                  const uint32_t nv = (uint32_t)((uint64_t)fv + (uint64_t)slide);
                  memcpy(mem + k * 4, &nv, 4);
                  ++patched;
               }
            }
         } else {
            const uint8_t *p = orig;
            /* Pass 1: `op modrm(mod=00,rm=100) sib(base=101,scale=4|8)
             * disp32` — absolute-indexed memory operands, with the
             * translator's optional 0x67 address-size prefix (32-bit EA
             * wrap). Opcodes: FF/4 jmp, FF/2 call, 8B load, 03 add and —
             * the one the wrapper's table lacked — 89 STORE. */
            for (size_t k = 0; k + 7 <= sz; ++k) {
               /* Prefix window: the translator's optional 0x67 address-size
                * prefix (32-bit EA wrap) plus at most one SSE mandatory
                * prefix (F2 movsd / F3 movss / 66 packed|movd|op16), in
                * either order. Observed in the wild:
                * `67 F2 0F 10 04 C5 disp32` — Quinn -[QuinnGame enableTimer]
                * loading its NSTimer interval from the per-level speed table
                * (`movsd disp(,%eax,8), %xmm0`). */
               size_t pfx = 0;
               uint8_t ssepfx = 0;
               int seen67 = 0;
               while (k + pfx < sz && pfx < 2) {
                  const uint8_t pb = p[k + pfx];
                  if (pb == 0x67 && !seen67) { seen67 = 1; ++pfx; continue; }
                  if ((pb == 0xF2 || pb == 0xF3 || pb == 0x66) && !ssepfx) {
                     ssepfx = pb; ++pfx; continue;
                  }
                  break;
               }
               /* Optional two-byte-opcode escape (0x0F): movsbl/movswl
                * (0F BE/BF) and movzbl/movzwl (0F B6/B7) also index a disp32
                * table (Quinn's -[QuinnGame incrementScore...]
                * `movswl disp(,%eax,8)` into __TEXT,__const), as do the SSE
                * scalar/packed memory forms below. The 0F shifts
                * ModRM/SIB/disp32 one byte. */
               size_t esc = (k + pfx < sz && p[k + pfx] == 0x0F) ? 1 : 0;
               if (k + pfx + esc + 7 > sz) { continue; }
               const uint8_t op    = p[k + pfx + esc];
               const uint8_t modrm = p[k + pfx + esc + 1];
               const uint8_t sib   = p[k + pfx + esc + 2];
               const uint8_t sc = sib & 0xC7;
               if (sc != 0x85 && sc != 0xC5) { continue; }
               if ((modrm & 0xC7) != 0x04) { continue; }
               int ok = 0;
               if (esc) {
                  /* Two-byte (0F) table accesses:
                   *  - movsx/movzx integer loads (BE/BF/B6/B7);
                   *  - SSE loads/stores/converts/compares/arith with a
                   *    memory operand: movss/movsd/movups/movaps and the
                   *    66-prefixed pd forms (10/11/28/29), cvtsi2ss/sd,
                   *    cvttss/sd2si, ucomiss/sd (2A/2C/2D/2E/2F),
                   *    sqrt/logic/arith/min/max (51,54-5F incl. cvt 5A),
                   *    movd/movdqa/movdqu/movq (6E/6F/7E/7F/D6), cvtdq (E6).
                   *    Quinn's -[QuinnGame enableTimer] reads its repeating
                   *    NSTimer interval via `movsd disp(,%eax,8)` — this slot
                   *    missing left the rebased disp32 UNSLID, the load read
                   *    garbage, and the tiny-positive interval on a repeating
                   *    timer tripped CF's "A CFRunLoopTimer with an interval
                   *    of 0 is set to repeat" ud2 in __CFRunLoopDoTimer
                   *    (deterministic SIGILL at fire time). */
                  ok = (op == 0xBE || op == 0xBF || op == 0xB6 || op == 0xB7) ||
                       (op == 0x10 || op == 0x11 || op == 0x28 || op == 0x29) ||
                       (op == 0x2A || op == 0x2C || op == 0x2D || op == 0x2E ||
                        op == 0x2F) ||
                       (op == 0x51 || (op >= 0x54 && op <= 0x5F)) ||
                       (op == 0x6E || op == 0x6F || op == 0x7E || op == 0x7F ||
                        op == 0xD6 || op == 0xE6) ||
                       (op == 0xAF /* imul r32, [tbl(,i,s)] */);
               } else if (op >= 0xD8 && op <= 0xDF) {
                  /* x87 escape opcodes: fld/fst/fadd/fmul/... with a memory
                   * operand — the pre-SSE compilers' indexed FP-table form
                   * (`fldl disp(,%eax,8)` = DD 04 C5 disp32). Any /r: the reg
                   * field selects the x87 operation; every mod=00 rm=100
                   * SIB-base=disp32 form is a table access needing the slide. */
                  ok = 1;
               } else if (op == 0xFF) {
                  const uint8_t reg = modrm & 0x38;
                  ok = (reg == 0x20 /* /4 jmp */) || (reg == 0x10 /* /2 call */)
                       || (reg == 0x00 /* /0 inc */) || (reg == 0x08 /* /1 dec */)
                       || (reg == 0x30 /* /6 push */);
               } else {
                  /* 8B load / 89 STORE / 8D LEA (address-of-element: Civ IV
                   * `lea eax,[disp32+rax*8]` computing
                   * &FConsoleCmd::m_SigTypes[i], then passed to strcmp) — and
                   * the single-byte ALU family with a memory operand and NO
                   * trailing immediate, BOTH directions: `op r32,[tbl(,i,s)]`
                   * loads 03/0B/13/1B/23/2B/33/3B (add/or/adc/sbb/and/sub/
                   * xor/cmp) and the read-modify-write / compare
                   * `op [tbl(,i,s)],r32` forms 01/09/11/19/21/29/31/39, plus
                   * test (85) and xchg (87). Any /r: the SIB base=disp32
                   * shape already disambiguates a table access. The one that
                   * FOUND this gap: Civ IV's CRC-32
                   * `xorl tbl(,%eax,4),%edx` (67 33 14 85 disp32) — the
                   * table-BUILD stores (89) were slid but the xor LOAD kept
                   * the pre-slide disp32 -> KERN_INVALID at the preferred
                   * vmaddr on the first byte checksummed (the twin store/
                   * load asymmetry proven from the live process: store disp
                   * patched, xor disp raw).
                   *
                   * IMMEDIATE-group forms (81/83 ALU-imm, C7 mov-imm, F7
                   * test-imm) are DELIBERATELY excluded here: macho-tool's
                   * instruction.cc `[disp32+idx*scale]` handler SKIPS a
                   * memory operand that also carries a trailing imm32
                   * (has_trailing_imm32 guard — xed_patch_disp can't cleanly
                   * patch the disp+imm combo), so those disps ship at the
                   * UN-rebased i386 vmaddr, which never lands in this pass's
                   * M64 [vmaddr_lo,vmaddr_hi) window — matching them would do
                   * nothing but widen the false-positive surface (and Pass 2
                   * below already handles the C7 store form). That
                   * translate-time gap is a separate concern. Byte-operand
                   * forms (8A/88/02/…) and scale=1/2 SIBs also stay excluded
                   * (different shape, none observed). */
                  ok = (op == 0x8B) || (op == 0x89) || (op == 0x8D) ||
                       (op == 0x03) || (op == 0x0B) || (op == 0x13) ||
                       (op == 0x1B) || (op == 0x23) || (op == 0x2B) ||
                       (op == 0x33) || (op == 0x3B) ||
                       (op == 0x01) || (op == 0x09) || (op == 0x11) ||
                       (op == 0x19) || (op == 0x21) || (op == 0x29) ||
                       (op == 0x31) || (op == 0x39) ||
                       (op == 0x85) || (op == 0x87);
               }
               if (!ok) { continue; }
               uint32_t fv, mv;
               memcpy(&fv, p + k + pfx + esc + 3, sizeof fv);
               memcpy(&mv, mem + k + pfx + esc + 3, sizeof mv);
               if (fv >= vmaddr_lo && fv < vmaddr_hi && mv == fv) {
                  const uint32_t nv = (uint32_t)((uint64_t)fv + (uint64_t)slide);
                  memcpy(mem + k + pfx + esc + 3, &nv, sizeof nv);
                  ++patched;
                  k += pfx + esc + 6;
               }
            }
            /* Pass 2: `c7 /0 ... imm32` — mov DWORD [mem], imm32 stores of
             * pointer literals (arg staging / global installs). Same
             * encodings the wrapper handles: rsp-SIB mod 0/1/2, and rm=101
             * rip-rel / rbp+disp8 / rbp+disp32. */
            for (size_t k = 0; k + 2 < sz; ++k) {
               if (p[k] != 0xC7) { continue; }
               if ((p[k + 1] & 0x38) != 0x00) { continue; }
               const uint8_t mod = (p[k + 1] >> 6) & 0x3;
               const uint8_t rm  = p[k + 1] & 0x7;
               size_t imm_off = 0, inst_len = 0;
               if (rm == 0x4) {
                  if (k + 2 >= sz || p[k + 2] != 0x24) { continue; }
                  if (mod == 0x0)      { imm_off = k + 3; inst_len = 7;  }
                  else if (mod == 0x1) { imm_off = k + 4; inst_len = 8;  }
                  else if (mod == 0x2) { imm_off = k + 7; inst_len = 11; }
                  else { continue; }
               } else if (rm == 0x5) {
                  if (mod == 0x0)      { imm_off = k + 6; inst_len = 10; }
                  else if (mod == 0x1) { imm_off = k + 3; inst_len = 7;  }
                  else if (mod == 0x2) { imm_off = k + 6; inst_len = 10; }
                  else { continue; }
                  /* PHANTOM tighteners (this pass has no instruction-boundary
                   * ground truth; a mid-instruction byte train can form a
                   * plausible `c7 45/05 ...`):
                   *  - `mov [rbp+0x0], imm32` is never real compiler output
                   *    (it would clobber the saved frame pointer). The Civ IV
                   *    boost.python wall was EXACTLY this shape: the last 3
                   *    bytes of a call-sim `jmp rel32` (…c7 45 00) + the
                   *    landing store's first 4 bytes read as the "imm32".
                   *  - a real `mov [rip+disp32], imm32` pointer store targets
                   *    one of the image's own __DATA globals; a phantom's
                   *    random disp32 points anywhere. Require the dest to
                   *    land inside the image span. */
                  if (mod == 0x1 && p[k + 2] == 0x00) { continue; }
                  if (mod == 0x0) {
                     uint32_t destdisp;
                     memcpy(&destdisp, p + k + 2, sizeof destdisp);
                     const uint64_t dest = (uint64_t)sect->addr + k + 10 +
                        (int64_t)(int32_t)destdisp;
                     if (dest < vmaddr_lo || dest >= vmaddr_hi) { continue; }
                  }
               } else { continue; }
               if (imm_off + 4 > sz) { continue; }
               uint32_t fv, mv;
               memcpy(&fv, p + imm_off, sizeof fv);
               memcpy(&mv, mem + imm_off, sizeof mv);
               if (fv >= vmaddr_lo && fv < vmaddr_hi && mv == fv) {
                  const uint32_t nv = (uint32_t)((uint64_t)fv + (uint64_t)slide);
                  memcpy(mem + imm_off, &nv, sizeof nv);
                  ++patched;
                  k += inst_len - 1;
               }
            }
         }
         munmap(omap, omap_len);

         mprotect((void *)pg, pglen, PROT_READ | PROT_EXEC);
         if (g_verbose) {
            fprintf(stderr, "abiconv text_abs32: patched %zu abs32 site(s) "
                    "in %s,%s of %s\n", patched, seg->segname,
                    sect->sectname, imgname);
         }
      }
   }
}

static void slide_data_fnptrs(const struct mach_header_64 *mh64, intptr_t slide,
                              uint64_t text_lo, uint64_t text_hi,
                              uint64_t vmaddr_lo, uint64_t vmaddr_hi,
                              const char *imgname) {
   if (slide == 0 || vmaddr_lo >= vmaddr_hi) { return; }
   /* Kill switch. This pass identifies pointers by VALUE -- any 4-byte word whose
    * content falls inside the image's pre-slide vmaddr span -- so an INTEGER that
    * merely aliases that window is indistinguishable from a pointer and gets slid,
    * turning a plain number into an address. When a translated program computes a
    * nonsensical, load-address-dependent value, this pass is the first suspect;
    * being able to switch it off tells you in one run whether it is responsible.
    * ⚠ Switching it off breaks any image that genuinely needs the slide, so this
    * is a diagnostic, not a supported mode. */
   if (getenv("ABICONV_NO_DATA_PTR_SLIDE")) {
      if (g_verbose) {
         fprintf(stderr, "abiconv objc_slide: data-pointer slide DISABLED for %s "
                 "(ABICONV_NO_DATA_PTR_SLIDE)\n", imgname ? imgname : "?");
      }
      return;
   }
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   size_t total_slid = 0;
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg =
            (const struct segment_command_64 *)p;
         if ((seg->initprot & VM_PROT_WRITE) &&
             strncmp(seg->segname, "__OBJC", 16) != 0 &&
             strcmp(seg->segname, "__LINKEDIT") != 0) {
            const struct section_64 *sect =
               (const struct section_64 *)(seg + 1);
            for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
               const uint32_t type = sect->flags & SECTION_TYPE;
               if (type == S_NON_LAZY_SYMBOL_POINTERS ||
                   type == S_LAZY_SYMBOL_POINTERS ||
                   type == S_MOD_INIT_FUNC_POINTERS ||
                   type == S_MOD_TERM_FUNC_POINTERS) { continue; }
               /* NEVER scan zerofill sections (__bss / __common / TLS
                * zerofill). They have NO file bytes, so a translate-time
                * relocated static pointer physically CANNOT live here — the
                * translator only ever rewrites pointers in file-backed
                * sections. Every 4-byte word here is written at RUNTIME by
                * the program (a heap pointer, an int, a live object field).
                * The value-window predicate below is only sound BEFORE the
                * program runs; a co-located libabiconv copy (multicopy
                * deploy) whose add-image callback re-fires AFTER static
                * inits would re-scan this image, and by then the image's
                * vacated preferred-vmaddr window has been recycled by the
                * low-4GB heap, so a legitimate runtime heap pointer is
                * indistinguishable from an unslid static slot -> it gets a
                * SECOND slide and turns into a wild (e.g. r-x __TEXT)
                * address (Civ IV: GTokenizer::FreeAllBuffers()'s __common
                * buffer array double-slid -> write-into-__TEXT at exit).
                * Skipping zerofill is exact: nothing the translator wrote
                * ever needs sliding here. */
               if (type == S_ZEROFILL ||
                   type == S_GB_ZEROFILL ||
                   type == S_THREAD_LOCAL_ZEROFILL) { continue; }
               if (strncmp(sect->sectname, "__cfstring", 16) == 0) { continue; }
               /* __86x64_xrel records carry a `slot_vmaddr` field that is itself
                * a 4-aligned intra-image __DATA pointer. Sliding it here would
                * double-apply the slide: bind_external_relocs (run right after)
                * reads slot_vmaddr and slides it AGAIN, writing the bound value
                * to slot_orig + 2*slide (a wild address). Only bites at slide!=0,
                * so the slide==0 standalone-dylib test never caught it. Skip the
                * whole section here; bind_external_relocs owns its slots. */
               if (strncmp(sect->sectname, "__86x64_xrel", 16) == 0) { continue; }
               /* __86x64_abs32 entries are 4-aligned intra-image __TEXT
                * addresses BY DESIGN — sliding them here would make the
                * abs32-table pass (run right after) compute double-slid field
                * addresses and fail its mprotect on unmapped pages. The other
                * metadata tables' values never alias the window today, but
                * skip them on principle: they are OUR bookkeeping, not the
                * program's pointers. */
               if (strncmp(sect->sectname, "__86x64_abs32", 16) == 0) { continue; }
               if (strncmp(sect->sectname, "__86x64_pcmap", 16) == 0) { continue; }
               if (strncmp(sect->sectname, "__86x64_ehlsda", 16) == 0) { continue; }
               /* __86x64_cpin is a table of 4-aligned intra-image __DATA/__TEXT
                * slot vmaddrs (the M32 pass's constant verdicts, read only by
                * macho-tool at parse time). Every entry aliases the window by
                * construction, so the scan would "slide" the whole table. It is
                * inert at runtime, but it is OUR bookkeeping, not the program's
                * pointers — same rule as the four above. */
               if (strncmp(sect->sectname, "__86x64_cpin", 16) == 0) { continue; }
               if (sect->size < 4) { continue; }
               void *base = (void *)(uintptr_t)(sect->addr + slide);
               const size_t page = 4096;
               uintptr_t a = (uintptr_t)base, e = a + sect->size;
               uintptr_t aa = a & ~(uintptr_t)(page - 1);
               size_t la = ((e + page - 1) & ~(uintptr_t)(page - 1)) - aa;
               if (mprotect((void *)aa, la, PROT_READ | PROT_WRITE) != 0) {
                  continue;
               }
               uint32_t *w = (uint32_t *)base;
               size_t n = (size_t)sect->size / 4;
               for (size_t k = 0; k < n; k++) {
                  uint32_t v = w[k];
                  /* Match macho-tool's DataParser pointer predicate exactly
                   * (section.cc): a 4-byte word is an intra-image pointer iff
                   * its value lands in the image's pre-slide vmaddr span AND —
                   * for a NON-executable (data) target — is 4-byte aligned
                   * (misaligned data-range values are integer/fixed-point
                   * constants, not pointers); executable (__TEXT) targets need
                   * no alignment (function entries aren't 4-aligned). This
                   * slides exactly the set macho-tool resolved as pointers, so
                   * it covers DATA->DATA global-pointers (e.g. Portal 2
                   * g_pMemAlloc = &s_StdMemAlloc) in addition to the DATA->TEXT
                   * vtable/fnptr case, with zero divergence from translate-time
                   * and no new false positives. Idempotent: an already-slid
                   * value falls below vmaddr_lo (slide is large & negative for
                   * the low-loaded translated images). */
                  if (v >= vmaddr_lo && v < vmaddr_hi &&
                      ((v >= text_lo && v < text_hi) || (v & 3) == 0)) {
                     w[k] = (uint32_t)((uint64_t)v + (uint64_t)slide);
                     ++total_slid;
                  }
               }
            }
         }
      }
      p += lc->cmdsize;
   }
   if (g_verbose && total_slid) {
      fprintf(stderr, "abiconv objc_slide: slid %zu intra-image 4-byte "
              "__DATA pointer slots in %s\n", total_slid, imgname);
   }
}

/* Test-only entry point (zerofill_reslide_test.sh): drive slide_data_fnptrs
 * over a caller-built in-memory Mach-O header with a chosen nonzero slide, so
 * a guard can assert the zerofill-skip STRUCTURALLY without needing the live
 * image to actually load at a slid address (the tests-i386 harness always
 * places translated no_pie images at their preferred base -> real slide == 0,
 * which would make an end-to-end double-slide unreproducible). Adds no behavior
 * to normal execution — nothing in libabiconv calls it. */
void _86x64_test_slide_data_fnptrs(const struct mach_header_64 *mh64,
                                   long slide,
                                   uint64_t text_lo, uint64_t text_hi,
                                   uint64_t vmaddr_lo, uint64_t vmaddr_hi) {
   slide_data_fnptrs(mh64, (intptr_t)slide, text_lo, text_hi,
                     vmaddr_lo, vmaddr_hi, "test");
}

/* real 64-bit pointer -> low-4GB proxy handle (objc_shim.c). */
extern uint32_t x64_objc_wrap(uint64_t real);

/* Resolve an xrel target symbol to a LOW-4GB-usable address (0 if not found).
 * Mirrors the data-shadow ctor (objc_shim.c x64_init_data_shadows): dlsym the C
 * name (strip the linker's leading '_'); a defined-local or libabiconv symbol
 * resolves <4GB and is used directly; a native >4GB symbol is wrapped into a
 * low-4GB proxy handle so the 4-byte slot is non-NULL and READABLE (which stops
 * the dropped-reloc deref SIGBUS, but is semantically opaque).
 *
 * ★INTERPOSE-FIRST (the load-bearing step). A bind that macho-tool DIVERTED into
 * __86x64_xrel has, by construction, left the dyld bind stream — so
 * static-interpose (which rewrites bind opcodes at translate time) never saw it
 * and never got the chance to redirect it to libabiconv's own replacement. The
 * two binding paths must not disagree about what a symbol means, so this one
 * applies the SAME rule static-interpose does: shim export == PREFIX "__" +
 * the full nlist symbol name; prefer it whenever libabiconv defines it.
 *
 * Structural, not a name list: the only symbols affected are the ones libabiconv
 * deliberately re-implements in i386 layout, and for those the raw native symbol
 * is the WRONG answer at any width. The concrete case that exposed it: the three
 * __cxxabiv1 type_info vtables. Diverted (96_rtti_const_vtable_bind) -> resolved
 * raw -> macOS libc++abi's 8-byte-layout vtable -> >4GB -> proxy handle, so
 * cxx_shim's ti_kind_of() saw a pointer matching none of its sentinels, called
 * every typeinfo TI_UNKNOWN, and returned NULL from EVERY typed dynamic_cast
 * (guard 97_dynamic_cast_crosscast).
 *
 * Kill switch ABICONV_XREL_NO_SHIM_PREFIX=1 restores the raw-name-only lookup. */
static int xrel_no_shim_prefix(void) {
   static int v = -1;
   if (v < 0) { v = getenv("ABICONV_XREL_NO_SHIM_PREFIX") != NULL; }
   return v;
}

/* libabiconv's replacement for `name`, or 0. The dlsym argument is the symbol
 * name minus ONE leading underscore (dlsym re-adds it), so asking for the
 * PREFIX+name symbol means asking dlsym for "_" + name. Only accepted when it
 * lands <4GB — a >4GB "replacement" would have to be proxy-wrapped, which is
 * exactly the opaque outcome this path exists to avoid. */
static uint64_t xrel_shim_lookup(const char *name) {
   if (xrel_no_shim_prefix() || name[0] != '_') { return 0; }
   size_t n = strlen(name);
   char stackbuf[256], *buf = stackbuf;
   if (n + 2 > sizeof stackbuf) {           /* mangled C++ names can be huge */
      buf = (char *)malloc(n + 2);
      if (buf == NULL) { return 0; }
   }
   buf[0] = '_';
   memcpy(buf + 1, name, n + 1);
   void *p = dlsym(RTLD_DEFAULT, buf);
   if (buf != stackbuf) { free(buf); }
   uintptr_t v = (uintptr_t)p;
   return (p != NULL && v < 0x100000000UL) ? (uint64_t)v : 0;
}

static uint64_t xrel_resolve(const char *name) {
   if (name == NULL || name[0] == '\0') { return 0; }
   uint64_t shim = xrel_shim_lookup(name);
   if (shim != 0) { return shim; }                     /* libabiconv replacement */
   const char *dn = (name[0] == '_') ? name + 1 : name;
   void *p = dlsym(RTLD_DEFAULT, dn);
   if (p == NULL) { return 0; }
   uintptr_t v = (uintptr_t)p;
   if (v < 0x100000000UL) { return (uint64_t)v; }       /* defined-local / libabiconv */
   return (uint64_t)x64_objc_wrap((uint64_t)v);          /* native >4GB -> handle */
}

/* Bind the classic EXTERNAL __DATA relocations macho-tool lifted into
 * __DATA,__86x64_xrel (see Archive::inject_xrel_section). dyld can't process
 * them (4-byte slots; it requires 8-byte/r_length=3 64-bit relocs), so the slots
 * ship NULL -> SIGBUS on first deref of an imported C++ vtable/RTTI pointer
 * (Portal 2, Pages, Civ IV, Front Row). We resolve each target to a low-4GB
 * address and write (target + addend) into its 4-byte slot here, at the
 * add-image callback, BEFORE the image's C++ static initializers run. Idempotent:
 * re-binding writes the same value (the resolved symbol address is stable). */
static void bind_external_relocs(const struct mach_header_64 *mh64, intptr_t slide,
                                 const char *imgname) {
   const uint8_t *xrel = NULL;
   /* ── A BINDER MUST ONLY WRITE INSIDE THE IMAGE IT IS BINDING ────────────────
    * Each record's `slot_vmaddr` is a value we READ OUT OF THE IMAGE, so a bad
    * record (or a slide applied to an already-slid field — the 2026-06-26
    * double-slide bug) aims the write at an arbitrary address. The write below is
    * preceded by an mprotect(PROT_READ|PROT_WRITE), so an out-of-image slot does
    * not merely fault: it makes SOMEONE ELSE'S page writable and then scribbles 4
    * bytes into it. dyld's own Loader objects are exactly the kind of neighbour
    * that gets hit, and a clobbered Loader surfaces much later and far away as
    *   dyld: Assertion failed: (this->magic == kMagic) ... Loader.cpp
    * with no hint of who did it. So collect this image's WRITABLE segment ranges
    * up front and refuse any slot that does not lie wholly inside one.
    *
    * Measured on Portal 2 2026-09-12: zero refusals, so this is an invariant being
    * held rather than a bug being masked — which is exactly what ruled the wild
    * write OUT as the cause of that assertion. */
   struct { uint64_t lo, hi; } wr[24];
   int nwr = 0;
   {
      const uint8_t *p = (const uint8_t *)(mh64 + 1);
      for (uint32_t i = 0; i < mh64->ncmds; i++) {
         const struct load_command *lc = (const struct load_command *)p;
         if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *seg =
               (const struct segment_command_64 *)p;
            if ((seg->initprot & VM_PROT_WRITE) != 0 && nwr < 24) {
               wr[nwr].lo = seg->vmaddr + (uint64_t)slide;
               wr[nwr].hi = wr[nwr].lo + seg->vmsize;
               ++nwr;
            }
            const struct section_64 *sect =
               (const struct section_64 *)(seg + 1);
            for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
               if (xrel == NULL &&
                   strncmp(sect->sectname, "__86x64_xrel", 16) == 0) {
                  xrel = (const uint8_t *)(uintptr_t)(sect->addr + slide);
               }
            }
         }
         p += lc->cmdsize;
      }
   }
   if (xrel == NULL) { return; }

   uint32_t magic, count;
   memcpy(&magic, xrel + 0, 4);
   memcpy(&count, xrel + 4, 4);
   if (magic != 0x6c657278u) {   /* "xrel" */
      if (g_verbose) {
         fprintf(stderr, "abiconv objc_slide: %s __86x64_xrel bad magic 0x%08x\n",
                 imgname, magic);
      }
      return;
   }

   size_t bound = 0, unresolved = 0;
   for (uint32_t i = 0; i < count; i++) {
      const uint8_t *e = xrel + 8 + (size_t)i * 12;
      uint32_t slot_vmaddr, name_off;
      int32_t addend;
      memcpy(&slot_vmaddr, e + 0, 4);
      memcpy(&addend,      e + 4, 4);
      memcpy(&name_off,    e + 8, 4);

      const char *name = (const char *)(xrel + name_off); /* section-relative */
      uint64_t target = xrel_resolve(name);
      if (g_verbose) {
         const uint32_t *pk =
            (const uint32_t *)(uintptr_t)((uint64_t)slot_vmaddr + (int64_t)slide);
         fprintf(stderr, "abiconv objc_slide:   xrel %s slot=%#x -> %#llx%s "
                 "[pre %08x %08x]\n", name, slot_vmaddr,
                 (unsigned long long)target,
                 xrel_shim_lookup(name) ? " (libabiconv shim)" : "",
                 pk[0], pk[1]);
      }
      if (target == 0) { ++unresolved; continue; }        /* leave it NULL */

      const uint64_t slot_addr = (uint64_t)slot_vmaddr + (int64_t)slide;
      /* Refuse a slot outside this image's writable segments (see above). Report
       * it: that would be a translator-side defect in the xrel table, or in the
       * slide applied to it, and silently skipping would hide it. */
      int in_image = 0;
      for (int w = 0; w < nwr; ++w) {
         if (slot_addr >= wr[w].lo && slot_addr + 4 <= wr[w].hi) {
            in_image = 1;
            break;
         }
      }
      if (!in_image) {
         static int complained = 0;
         if (!complained || g_verbose) {
            complained = 1;
            fprintf(stderr, "abiconv objc_slide: REFUSED out-of-image xrel slot "
                    "%#llx (record %u, vmaddr %#x, slide %#llx, symbol %s) in %s "
                    "— a binder must never write outside the image it is binding\n",
                    (unsigned long long)slot_addr, i, slot_vmaddr,
                    (unsigned long long)(int64_t)slide, name, imgname);
         }
         ++unresolved;
         continue;
      }
      uint32_t *slot = (uint32_t *)(uintptr_t)slot_addr;
      /* Defensively make the slot's page writable (the slots live in __DATA so
       * they normally already are; mprotect failure is non-fatal). A 4-byte,
       * 4-aligned slot never straddles a page boundary. */
      uintptr_t pg = (uintptr_t)slot & ~(uintptr_t)0xfff;
      mprotect((void *)pg, 0x1000, PROT_READ | PROT_WRITE);
      *slot = (uint32_t)((uint64_t)target + (int64_t)addend);
      ++bound;
   }
   if (g_verbose) {
      fprintf(stderr, "abiconv objc_slide: bound %zu/%u external relocs "
              "(%zu unresolved) in __DATA,__86x64_xrel of %s\n",
              bound, count, unresolved, imgname);
   }
}

/* Neutralize the classic __DATA,__dyld crt bootstrap of a pre-10.5 i386 binary.
 *
 * Old crt1.o __start tail-jumps through the 8-byte __DATA,__dyld section (two
 * 4-byte slots: [+0]=lazy-symbol-binder, [+4]=dyld_func_lookup) to ask dyld for
 * __dyld_make_delayed_module_initializer_calls / __dyld_mod_term_funcs, passing
 * the name + an out-pointer on the i386 cdecl STACK. dyld, treating the image as
 * legacy, points the func_lookup slot at native legacyDyldLookup4OldBinaries,
 * which reads its args from registers (x86_64 ABI) — garbage — and writes the
 * looked-up fp through the garbage out-pointer -> SIGSEGV writing 0x0 (Halo CE,
 * Civ IV). Our own runtime (wrapper + slide_objc + run-now inits) already
 * performs that legacy bootstrap, so we overwrite the slots to route the crt's
 * func_lookup into our i386-cdecl-honoring shim (dyld_func_lookup.asm), which
 * satisfies the lookup harmlessly. Defensive: the lazy-binder slot is pointed at
 * a no-op too (translated binaries don't use classic lazy binding, and the static
 * placeholder 0x8fe01000 would fault).
 *
 * Universal: triggers on the structural presence of __DATA,__dyld, not an app
 * name. Runs from the add-image callback (before dyld's findAndRunAllInitializers
 * reaches the crt, and before slide_objc runs the collected init funcs in
 * run-now init mode), so the slot is patched before the crt ever reads it.
 * Idempotent: re-running writes the same shim addresses. */
static void patch_dyld_section(const struct mach_header_64 *mh64, intptr_t slide,
                               const char *imgname) {
   /* The crt reads the slots with a 32-bit `movl (slot),%eax` and tail-jumps via
    * `jmpq *%rax`, so our shim addresses must fit in 32 bits — i.e. libabiconv
    * must map <4GB (it normally does; see init_stack_setup). If somehow not, bail
    * and leave dyld's (buggy) native handler rather than write a truncated addr. */
   if (((uintptr_t)&_86x64_dyld_func_lookup >> 32) ||
       ((uintptr_t)&_86x64_dyld_noop >> 32)) {
      if (g_verbose) {
         fprintf(stderr, "abiconv dyld_section: libabiconv mapped >4GB; __dyld "
                 "patch disabled for %s\n", imgname);
      }
      return;
   }
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg =
            (const struct segment_command_64 *)p;
         const struct section_64 *sect = (const struct section_64 *)(seg + 1);
         for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
            if (strncmp(sect->sectname, "__dyld", 16) != 0) { continue; }
            if (sect->size < 8) { continue; }
            uint8_t *base = (uint8_t *)(uintptr_t)(sect->addr + slide);
            /* We write up to __dyld+16 (the 8-byte-layout func_lookup slot may sit
             * past the i386-sized 8-byte section, in the next section); cover both
             * possible pages. */
            uintptr_t pg0 = (uintptr_t)base & ~(uintptr_t)0xfff;
            uintptr_t pg1 = ((uintptr_t)base + 16 - 1) & ~(uintptr_t)0xfff;
            if (mprotect((void *)pg0, (pg1 - pg0) + 0x1000,
                         PROT_READ | PROT_WRITE) != 0) {
               if (g_verbose) {
                  fprintf(stderr, "abiconv dyld_section: mprotect failed for "
                          "%s: %s\n", imgname, strerror(errno));
               }
               continue;
            }
            /* The i386 crt reads func_lookup through __dyld; the classic section
             * is two pointers (lazy-binder, func_lookup). For an i386 binary that
             * is 8 bytes (2x4), but a 64-bit dyld AND the translated helper treat
             * these as 8-BYTE slots: lazy-binder at __dyld+0, func_lookup at
             * __dyld+8 — which, because the section was kept at the i386 size of 8
             * bytes (not the 16 two 8-byte slots need), overlaps the FIRST entry of
             * the next section (commonly __mod_init_func[0]). dyld stamps native
             * legacyDyldLookup4OldBinaries into __dyld+8; the i386 crt's func_lookup
             * tail-jumps there with cdecl args on the stack -> wrong-ABI NULL write.
             * Write our i386-cdecl shim into BOTH the 4-byte view (+4) and the
             * 8-byte view (+8) so whichever the translated helper reads lands in
             * the shim. wrap_mod_init_funcs has already recovered + collected the
             * real __mod_init_func[0] initializer (it sees the >4GB legacyDyldLookup
             * clobber and restores the on-disk value), so overwriting the live slot
             * at +8 here does not lose it. The proper cure is a 16-byte __dyld at
             * translate time; see the known-gaps list. */
            uint32_t *slot = (uint32_t *)base;
            slot[0] = (uint32_t)(uintptr_t)&_86x64_dyld_noop;        /* lazy binder (4B view) */
            slot[1] = (uint32_t)(uintptr_t)&_86x64_dyld_func_lookup; /* func_lookup (4B view) */
            *(uint64_t *)(base + 8) =
               (uint64_t)(uintptr_t)&_86x64_dyld_func_lookup;        /* func_lookup (8B view) */
            if (g_verbose) {
               fprintf(stderr, "abiconv dyld_section: patched __DATA,__dyld in "
                       "%s (noop=%p lookup=%p, +4 and +8)\n", imgname,
                       (void *)&_86x64_dyld_noop,
                       (void *)&_86x64_dyld_func_lookup);
            }
         }
      }
      p += lc->cmdsize;
   }
}

/* Low-4GB shadow words for the classic libc/crt-internal __IMPORT,__pointers
 * symbols (patch_import_pointers). libabiconv maps <4GB, so the ADDRESS of a
 * static here is a valid 32-bit pointer for the i386 4-byte slot read.
 *  - zero_word: the crt's `if (*hook) (*hook)()` reads through the slot; making
 *    the slot point here (which stays 0) makes the hook test false -> skipped.
 *  - errno_word: the crt's `*errno = 0` writes 4 bytes through the slot; this
 *    absorbs the write. (Per-copy; errno cross-image/thread fidelity is a
 *    documented residual — startup only needs a writable landing pad.) */
static uint64_t g_import_zero_word  = 0;
static uint64_t g_import_errno_word = 0;

/* ── The three stdio STREAM VARIABLES (Portal 2 2026-09-14) ────────────────
 * A translated image binds libSystem/___stderrp (also ___stdoutp / ___stdinp)
 * into a __DATA,__nl_symbol_ptr slot and then reads it the same way it reads any
 * scalar extern:
 *     movl slot, %eax        ; eax = &__stderrp, TRUNCATED to 32 bits
 *     movl (%eax), %r11d     ; deref -> SIGSEGV
 * dyld writes the real 64-bit &__stderrp, so the 4-byte load keeps only the low
 * half. PROVEN inside the faulting process: &__stderrp = 0x7ff852955e38, low32
 * 0x52955e38, and the fault address was 0x52955e38 exactly. 31 such binds exist
 * across Portal 2's tree (19 stderr, 8 stdout, 4 stdin), and the one that bit was
 * libsteam_api building an fprintf(stderr, ...) to report why Steam init failed.
 *
 * Point the slot at a low-4GB cell instead, holding the stream as a WRAPPED
 * ARENA HANDLE -- the same representation typeconv now gives every FILE*, so the
 * handle unwraps to the real native stream in whichever bridge receives it
 * (including the hand-marshalled printf family). Storing the raw FILE* would not
 * work: it is itself >4GB (measured: fopen -> 0x7ff8_5277abb8).
 *
 * ⛔ NOT fdopen(2,"w"): that allocates through LIBSYSTEM's malloc under two-level
 * namespace, not our low-4GB shim, so it just returns another >4GB FILE*. */
static uint64_t g_import_stdio_cell[3];   /* [0]=stdin [1]=stdout [2]=stderr */

extern uint32_t x64_objc_wrap(uint64_t real);   /* objc_shim.c */

static uint64_t *stdio_stream_cell(const char *name) {
   /* M64_NO_STDIO_STREAM_CELL=1 leaves the slot bound to libSystem's &__stderrp,
    * i.e. reproduces the real defect (a >4GB address read through a 4-byte load),
    * so the guard's OFF arm faults for the genuine reason instead of an
    * approximation of it. */
   static int off = -1;
   if (off < 0) { off = getenv("M64_NO_STDIO_STREAM_CELL") ? 1 : 0; }
   if (off) { return NULL; }
   int idx;
   FILE *f;
   if (strcmp(name, "___stdinp") == 0)        { idx = 0; f = stdin;  }
   else if (strcmp(name, "___stdoutp") == 0)  { idx = 1; f = stdout; }
   else if (strcmp(name, "___stderrp") == 0)  { idx = 2; f = stderr; }
   else { return NULL; }
   if (!g_import_stdio_cell[idx]) {
      if (!f) { return NULL; }
      const uint32_t h = x64_objc_wrap((uint64_t)(uintptr_t)f);
      if (!h) { return NULL; }        /* arena exhausted: leave the slot alone */
      g_import_stdio_cell[idx] = (uint64_t)h;
      if (g_verbose) {
         fprintf(stderr, "abiconv stdio: %s -> handle %#x (real %p)\n",
                 name, h, (void *)f);
      }
   }
   return &g_import_stdio_cell[idx];
}

/* Low-4GB i386-layout copy of `_DefaultRuneLocale` (the C rune-locale table the
 * inlined <ctype.h> macros — isalnum/isspace/tolower/... — index directly). An
 * i386 binary reads `__DefaultRuneLocale.__runetype[c]` as
 * `movl <slot>,%eax; movl 0x34(%eax,%c,4),%eax` where <slot> is the classic
 * __IMPORT,__pointers non-lazy slot bound to the 64-bit libSystem symbol; the
 * 4-byte `movl` keeps only the low 32 bits of the >4GB &_DefaultRuneLocale ->
 * garbage table base -> SIGSEGV in the ctype fast path (chars < 0x80). The
 * >= 0x80 path calls ___maskrune (a shimmed FUNCTION) and already works, so only
 * the inline table read faults. Quinn: -[KeyTypeCell isEntryAcceptable:]
 * validating a typed Settings key ('a') faulted at &table+0x34+'a'*4
 * (crashlog Quinn-2026-07-04-005805.ips, fault 0x5b061150 = low32(&table)+0x1b8).
 *
 * The i386 `_RuneLocale` layout differs from x86_64 (its two internal function
 * pointers are 4 bytes, not 8), so the arrays sit 8 bytes earlier:
 *   __invalid_rune @ 0x30, __runetype @ 0x34, __maplower @ 0x434, __mapupper
 *   @ 0x834 (each 256 * 4-byte entries; __magic @ 0x00, __encoding @ 0x08 match).
 * The _CTYPE_* bit values are ABI-stable, so the native __runetype/__maplower/
 * __mapupper values are copied verbatim into the i386 offsets. Only c < 256 is
 * read inline; c >= 256 goes through the native ___maskrune shim, so the ext
 * ranges past __mapupper stay zero. patch_import_pointers redirects the slot to
 * &g_i386_rune (a libabiconv static, hence < 4GB), so the i386 `movl` reads a
 * valid low-4GB table base. UNIVERSAL: any i386 app using inline ctype macros. */
#define I386_RUNE_MAGIC      0x000
#define I386_RUNE_ENCODING   0x008
#define I386_RUNE_INVALID    0x030
#define I386_RUNE_RUNETYPE   0x034
#define I386_RUNE_MAPLOWER   0x434
#define I386_RUNE_MAPUPPER   0x834
#define I386_RUNE_SIZE       0x1000       /* > __mapupper end (0xc34); tail zero */
static uint8_t  g_i386_rune[I386_RUNE_SIZE];
static int      g_i386_rune_ready = 0;

static void i386_rune_build(void) {
   if (g_i386_rune_ready) { return; }
   const _RuneLocale *n = &_DefaultRuneLocale;
   memset(g_i386_rune, 0, sizeof g_i386_rune);
   memcpy(g_i386_rune + I386_RUNE_MAGIC,    n->__magic,    sizeof n->__magic);
   memcpy(g_i386_rune + I386_RUNE_ENCODING, n->__encoding, sizeof n->__encoding);
   memcpy(g_i386_rune + I386_RUNE_INVALID,  &n->__invalid_rune, 4);
   for (int c = 0; c < 256; ++c) {
      uint32_t rt = (uint32_t)n->__runetype[c];
      int32_t  ml = (int32_t)n->__maplower[c];
      int32_t  mu = (int32_t)n->__mapupper[c];
      memcpy(g_i386_rune + I386_RUNE_RUNETYPE + c * 4, &rt, 4);
      memcpy(g_i386_rune + I386_RUNE_MAPLOWER + c * 4, &ml, 4);
      memcpy(g_i386_rune + I386_RUNE_MAPUPPER + c * 4, &mu, 4);
   }
   g_i386_rune_ready = 1;
}

/* Redirect the classic libc/crt-internal __IMPORT,__pointers slots whose i386
 * 4-byte `movl <slot>(%rip),%eax` read would truncate a 64-bit-bound libSystem
 * symbol address to its low 32 bits (then deref/write through the garbage) ->
 * SIGSEGV. The shared root behind Halo (fault 0x5346cea8), Civ IV (0x54268ea8),
 * Numbers and iWeb.
 *
 * Pre-10.5 crt1.o bootstraps through three S_NON_LAZY_SYMBOL_POINTERS slots:
 *   _mach_init_routine     : `movl slot,%eax; movl (%rax),%eax; test; je; call`
 *   __cthread_init_routine : same deref-then-test-then-call hook pattern
 *   _errno                 : `movl slot,%eax; movl $0,(%rax)` (clears errno)
 * dyld binds each slot to the 64-bit address of the libSystem symbol; the i386
 * `movl` keeps only the low 32 bits -> an unmapped pointer -> fault on the deref
 * (mach/cthread) or the store (errno). Our runtime already initialises mach and
 * cthread (the host x86_64 libSystem did), so the hooks must be NO-OPS: point
 * their slots at a low-4GB word that stays 0, so the crt's NULL test skips the
 * call. errno's slot points at a low-4GB writable word so the `*errno = 0` store
 * lands harmlessly.
 *
 * Universal: triggers on the STRUCTURE (a S_NON_LAZY_SYMBOL_POINTERS slot whose
 * indirect symbol is one of these well-known classic libc/crt-internal data
 * symbols), present in every classic i386 binary — not on an app name. The
 * broader __IMPORT,__pointers truncation for framework data constants (CF/CG
 * allocator/runloop refs), C++ vtables/RTTI and __sF needs per-symbol type info
 * (object-wrap vs scalar-copy vs function-thunk) that only the TRANSLATE-TIME
 * data-shadow table carries; extending that is the complete cure (see
 * the known-gaps list). This runtime pass unblocks the crt bootstrap, which is the
 * gate for all four targets. */
/* objc_shim.c: (re)fill + return a low-4GB value-copy data-shadow by name (no
 * leading underscore), or NULL if the symbol is not a value-typed data shadow. */
extern uint64_t *x64_value_data_shadow(const char *name_no_underscore);

static void patch_import_pointers(const struct mach_header_64 *mh64,
                                  intptr_t slide, const char *imgname) {
   if (((uintptr_t)&g_import_zero_word >> 32) ||
       ((uintptr_t)&g_import_errno_word >> 32) ||
       ((uintptr_t)&g_import_stdio_cell[0] >> 32) ||
       ((uintptr_t)&g_i386_rune[0] >> 32)) {
      if (g_verbose) {
         fprintf(stderr, "abiconv import_pointers: libabiconv >4GB; skip %s\n",
                 imgname);
      }
      return;
   }

   /* Locate LC_SYMTAB, LC_DYSYMTAB and __LINKEDIT (to map the LINKEDIT file
    * offsets the symtab/strtab/indirect tables use to runtime addresses). */
   const struct symtab_command *st = NULL;
   const struct dysymtab_command *dy = NULL;
   uint64_t le_vmaddr = 0, le_fileoff = 0;
   int have_le = 0;
   {
      const uint8_t *p = (const uint8_t *)(mh64 + 1);
      for (uint32_t i = 0; i < mh64->ncmds; i++) {
         const struct load_command *lc = (const struct load_command *)p;
         if (lc->cmd == LC_SYMTAB) {
            st = (const struct symtab_command *)p;
         } else if (lc->cmd == LC_DYSYMTAB) {
            dy = (const struct dysymtab_command *)p;
         } else if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *seg =
               (const struct segment_command_64 *)p;
            if (strcmp(seg->segname, "__LINKEDIT") == 0) {
               le_vmaddr = seg->vmaddr; le_fileoff = seg->fileoff; have_le = 1;
            }
         }
         p += lc->cmdsize;
      }
   }
   if (!st || !dy || !have_le || dy->nindirectsyms == 0) { return; }

   /* file offset in __LINKEDIT -> live runtime address. */
   #define LE_ADDR(fo) (uintptr_t)(le_vmaddr + (uint64_t)slide \
                                   + ((uint64_t)(fo) - le_fileoff))
   const struct nlist_64 *symtab = (const struct nlist_64 *)LE_ADDR(st->symoff);
   const char *strtab            = (const char *)LE_ADDR(st->stroff);
   const uint32_t *indirect      = (const uint32_t *)LE_ADDR(dy->indirectsymoff);

   size_t patched = 0;
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg =
            (const struct segment_command_64 *)p;
         const struct section_64 *sect = (const struct section_64 *)(seg + 1);
         for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
            if ((sect->flags & SECTION_TYPE) != S_NON_LAZY_SYMBOL_POINTERS) {
               continue;
            }
            /* Translated images carry 8-byte non-lazy pointer slots (one
             * indirect-symtab entry each, based at reserved1). */
            size_t nslot = (size_t)sect->size / 8;
            for (size_t k = 0; k < nslot; k++) {
               uint32_t ii = sect->reserved1 + (uint32_t)k;
               if (ii >= dy->nindirectsyms) { continue; }
               uint32_t isym = indirect[ii];
               if (isym & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
                  continue;                       /* intra-image, dyld rebases */
               }
               if (isym >= st->nsyms) { continue; }
               const char *name = strtab + symtab[isym].n_un.n_strx;
               uint64_t *target = NULL;
               if (strcmp(name, "_mach_init_routine") == 0 ||
                   strcmp(name, "__cthread_init_routine") == 0 ||
                   strcmp(name, "_cthread_init_routine") == 0) {
                  target = &g_import_zero_word;   /* deref->0 => crt skips hook */
               } else if (strcmp(name, "_errno") == 0) {
                  target = &g_import_errno_word;  /* `*errno = 0` lands here     */
               } else if (strcmp(name, "__DefaultRuneLocale") == 0) {
                  /* Point the slot at the low-4GB i386-layout rune table so the
                   * inline ctype macros' `movl 0x34(%eax,%c,4)` read a valid
                   * table base instead of the truncated 64-bit libSystem addr. */
                  i386_rune_build();
                  target = (uint64_t *)(void *)g_i386_rune;
               } else if (strcmp(name, "___stderrp") == 0 ||
                          strcmp(name, "___stdoutp") == 0 ||
                          strcmp(name, "___stdinp")  == 0) {
                  /* stdio stream variable: hand the i386 double-deref a low cell
                   * holding a wrapped FILE handle (see stdio_stream_cell). The
                   * generic value-shadow below would copy 4 bytes of the >4GB
                   * FILE* verbatim, which is still a truncated pointer. */
                  target = stdio_stream_cell(name);
               } else if (name[0] == '_') {
                  /* General value-typed data constant (scalar / small record,
                   * e.g. HIToolbox's `const HIViewID kHIViewWindowContentID`):
                   * the i386 code reads &var via this slot then derefs the
                   * value bytes; the real 64-bit &var truncates on the 4-byte
                   * load -> fault (Civ EULADialog+356: kHIViewWindowContentID ->
                   * HIViewFindByID). abigen emits a low-4GB shadow COPY for such
                   * constants (data-shadow table); apply it HERE at load time —
                   * the RESYNC-class equivalent of the translate-time static-
                   * interpose redirect, so a non-retranslated classic binary is
                   * cured on a plain libabiconv resync. x64_value_data_shadow
                   * (re)fills from the live symbol via dlsym (this pass runs at
                   * the image's add-image, after its framework deps are in).
                   * Object/CF (handle-wrap) shadows are left to translate time.
                   * Regression-safe: only fires for a data constant already
                   * broken-on-read (its high &var truncates). */
                  target = x64_value_data_shadow(name + 1);
               }
               if (!target) { continue; }

               uint64_t *slot =
                  (uint64_t *)(uintptr_t)((uint64_t)sect->addr + (uint64_t)slide
                                          + (uint64_t)k * 8);
               uintptr_t pg = (uintptr_t)slot & ~(uintptr_t)0xfff;
               if (mprotect((void *)pg, 0x1000, PROT_READ | PROT_WRITE) != 0) {
                  continue;
               }
               *slot = (uint64_t)(uintptr_t)target;   /* <4GB, zero-extended */
               ++patched;
               if (g_verbose) {
                  fprintf(stderr, "abiconv import_pointers: %s slot[%zu] %s "
                          "-> low-4GB %p in %s\n", sect->sectname, k, name,
                          (void *)target, imgname);
               }
            }
         }
      }
      p += lc->cmdsize;
   }
   #undef LE_ADDR
   if (g_verbose && patched) {
      fprintf(stderr, "abiconv import_pointers: redirected %zu crt-internal "
              "__IMPORT,__pointers slot(s) in %s\n", patched, imgname);
   }
}

static void slide_objc(const struct mach_header *mh, intptr_t slide);
static void cxx_typeinfo_init_all_copies(void);
/* import_repair.c — eager repair of never-bound indirect-pointer slots in
 * translated images (half-wired artifacts without the static-interpose /
 * libabiconv wiring; Civ IV s29). */
extern void _86x64_import_repair(const struct mach_header_64 *mh64,
                                 intptr_t slide, const char *imgname);

/* Bottom-up dependency ordering for the run-inits path: before an image's
 * collected static initializers run, recursively process each of its TRANSLATED
 * (libabiconv-linked) dependencies that is already mapped but not yet processed.
 * dyld maps a dylib's load-time dependencies before firing the dependent's
 * add-image callback, but it does NOT guarantee it delivers the dependencies'
 * callbacks first — so a dependent (e.g. Portal 2's launcher) can run its ctors,
 * which dereference a dependency's globals (e.g. libtier0's g_pMemAlloc /
 * s_StdMemAlloc vtable), before that dependency was slid+initialized. Walking
 * the dependency edges here reproduces dyld's own bottom-up initializer order.
 * The processed-set marks each image before recursing, so dependency cycles
 * terminate (mirrors dyld's upward-edge handling). */
static void process_deps(const struct mach_header_64 *mh64) {
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_LOAD_DYLIB || lc->cmd == LC_LOAD_WEAK_DYLIB ||
          lc->cmd == LC_REEXPORT_DYLIB || lc->cmd == LC_LOAD_UPWARD_DYLIB) {
         const struct dylib_command *dc = (const struct dylib_command *)p;
         const char *path = (const char *)p + dc->dylib.name.offset;
         const char *leaf = strrchr(path, '/');
         leaf = leaf ? leaf + 1 : path;
         if (strstr(leaf, "libabiconv") == NULL) {
            intptr_t dslide = 0;
            const struct mach_header *dmh = find_loaded_image(leaf, &dslide);
            if (dmh && !already_processed(dmh) &&
                dmh->magic == MH_MAGIC_64 &&
                image_links_libabiconv((const struct mach_header_64 *)dmh)) {
               slide_objc(dmh, dslide);
            }
         }
      }
      p += lc->cmdsize;
   }
}

static void slide_objc(const struct mach_header *mh, intptr_t slide) {
   if (mh->magic != MH_MAGIC_64) { return; }
   if (already_processed(mh)) { return; }
   mark_processed(mh);
   /* Cross-copy guard: only ONE libabiconv copy ever processes a given image.
    * A co-located second copy's own _dyld_register_func_for_add_image(slide_objc)
    * re-delivers every already-mapped image to THIS function with an empty
    * per-copy g_processed — re-running the slide / ObjC-register / run-inits work
    * over metadata the first copy already processed (or is mid-processing on
    * another thread), so a pointer field reads back NULL and gets dereferenced
    * at +offset (the intermittent `[rbx+0x88]`, rbx=0 crash). The per-image work
    * is once-per-IMAGE, not once-per-copy, so claiming it process-globally is
    * exact. mark_processed above already ran, so this copy won't retry the image
    * via its own later callback or a process_deps edge. Single-copy deploys and
    * a failed ctrl init both return 1 here (the local g_processed stays the
    * guard), so this is inert unless there really is >1 copy. */
   int claimed = _86x64_objc_shared_claim_image(mh);
   const struct mach_header_64 *mh64 = (const struct mach_header_64 *)mh;
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   const char *imgname = x64_img_path_for_header(mh);
   if (imgname == NULL) { imgname = "?"; }
   /* Test signal (dyld_multicopy_reprocess_test.sh): one line per (copy,image)
    * decision, so the harness can count how many times slide_objc actually
    * PROCESSED each image across all copies. Env-gated, inert otherwise. */
   if (getenv("ABICONV_XCOPY_TRACE")) {
      const char *leaf = strrchr(imgname, '/');
      leaf = leaf ? leaf + 1 : imgname;
      fprintf(stderr, "[xcopy] %s %s\n", claimed ? "process" : "skip", leaf);
      fflush(stderr);
   }
   if (!claimed) { return; }

   const struct segment_command_64 *objc_seg = NULL;
   uint64_t vmaddr_lo = ~(uint64_t)0;
   uint64_t vmaddr_hi = 0;
   uint64_t text_lo = ~(uint64_t)0;       /* pre-slide executable vmaddr range */
   uint64_t text_hi = 0;
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg =
            (const struct segment_command_64 *)p;
         if (strncmp(seg->segname, "__OBJC", 16) == 0) {
            objc_seg = seg;
         }
         if ((seg->initprot & VM_PROT_EXECUTE) && seg->vmsize > 0) {
            if (seg->vmaddr < text_lo) text_lo = seg->vmaddr;
            if (seg->vmaddr + seg->vmsize > text_hi)
               text_hi = seg->vmaddr + seg->vmsize;
         }
         /* Accumulate the image's pre-slide vmaddr span (the range that an
          * intra-image pointer can fall in), skipping __PAGEZERO/__LINKEDIT
          * which never hold patchable intra-image pointers. This is the
          * range slide_section_4byte uses to decide what to slide, which is
          * what keeps the operation idempotent across repeated callbacks. */
         if (strcmp(seg->segname, "__PAGEZERO") != 0
             && strcmp(seg->segname, "__LINKEDIT") != 0
             && seg->vmsize > 0) {
            if (seg->vmaddr < vmaddr_lo) vmaddr_lo = seg->vmaddr;
            if (seg->vmaddr + seg->vmsize > vmaddr_hi)
               vmaddr_hi = seg->vmaddr + seg->vmsize;
         }
      }
      p += lc->cmdsize;
   }

   /* Repair never-bound indirect-pointer slots FIRST (import_repair.c).
    * Deliberately NOT gated on image_links_libabiconv: the half-wired
    * translated artifacts this cures (Civ IV s29, bundled QuickTime) have no
    * libabiconv dependency at all — that missing wiring IS the defect. The
    * repair itself gates structurally (macho-tool __TEXT layout base +
    * post-load unbound slot state) and is inert for native images and for
    * complete current-pipeline translations. Must precede the collected
    * initializers below: a translated static init may call straight through
    * an unbound stub (jmp *0). */
   _86x64_import_repair(mh64, slide, imgname);

   /* Run translated static initializers on a low-4GB stack. Independent of
    * slide (the >4GB-stack bug bites even at the preferred vmaddr) AND of
    * __OBJC: a pure C++ translated dylib (no __OBJC segment, e.g. Portal 2's
    * libtier0) still has __mod_init_func ctors that dyld would otherwise call
    * directly with a 64-bit return address, which the translated `ret`
    * (pop r11d; jmp r11) truncates to a wild low-4GB target. Gate on "is
    * translated" (links libabiconv), NOT on objc_seg. See the init high-stack bug. */
   void *init_targets[INIT_COLLECT_MAX];
   size_t n_init = 0;
   if (image_links_libabiconv(mh64)) {
      wrap_mod_init_funcs(mh64, slide, imgname, text_lo, text_hi,
                          init_targets, &n_init);
      /* Re-register static TERMINATORS (__DATA,__mod_term_func) through the
       * reverse callback bridge so dyld4 never calls the raw i386-ABI address
       * at exit(): the translated epilogue's 4-byte return-pop would truncate
       * libsystem_c's 8-byte return address (Halo exit crash 0x1c08aeb3 =
       * low32 of __cxa_finalize_ranges+319). Same add-image timing as the init
       * path — runs during libabiconv's ctor, before dyld4 initializes this
       * image and reads the section. */
      wrap_mod_term_funcs(mh64, slide, imgname, text_lo, text_hi);
      /* Slide 4-byte __DATA intra-image pointer slots (vtables, fn-ptr tables,
       * AND data->data global pointers) by the load slide BEFORE the collected
       * static initializers run (a C++ ctor stores/derefs these). Universal:
       * triggers on the structural property "4-byte __DATA slot whose value is
       * an intra-image pointer" (macho-tool's exact DataParser predicate), not
       * on any app. */
      slide_data_fnptrs(mh64, slide, text_lo, text_hi,
                        vmaddr_lo, vmaddr_hi, imgname);
      /* Slide the CODE-embedded absolute [disp32(,idx,scale)] operands,
       * mov-imm32 pointer stores, and __TEXT,__const jump tables BEFORE the
       * collected static initializers run. wrapper_setup.c repeats this scan
       * at wrapper entry (idempotent: only pre-slide-range values are
       * patched), but that is too late for the ctors run here — Civ IV s21,
       * NiStaticDataManager::AddLibrary's `mov [disp32+idx*4], reg` store
       * faulted on the unslid preferred vmaddr of ms_apfnInitFunctions. */
      patch_text_abs32(mh64, slide, vmaddr_lo, vmaddr_hi, imgname);
      /* Bind the classic external __DATA relocations dyld can't process (the
       * lifted C++ vtable/RTTI imports in __DATA,__86x64_xrel) before any C++
       * static ctor dereferences them. Universal: triggers only on the presence
       * of the macho-tool-emitted section. */
      bind_external_relocs(mh64, slide, imgname);
      /* Neutralize the classic __DATA,__dyld crt bootstrap (pre-10.5 i386
       * binaries) before the crt's func_lookup is reached — by dyld's init pass
       * (default mode) or by the collected init funcs run at the end of this
       * function (run-now init mode). Universal: triggers only on the structural
       * presence of __DATA,__dyld. */
      patch_dyld_section(mh64, slide, imgname);
      /* Redirect the classic libc/crt-internal __IMPORT,__pointers slots
       * (_mach_init_routine / __cthread_init_routine / _errno) whose i386 4-byte
       * read would truncate a 64-bit-bound libSystem address. Same add-image
       * timing requirement as patch_dyld_section: before the crt bootstrap (and
       * the collected inits) reads them. Universal: triggers on the structural
       * presence of those symbols in a S_NON_LAZY_SYMBOL_POINTERS section. */
      patch_import_pointers(mh64, slide, imgname);
   }

   /* the legacy-ObjC1 fixups below are __OBJC-only, but the collected
    * static initializers (n_init>0, run-now mode) must still run at
    * the END for a pure-C++ no-__OBJC image (e.g. Portal 2's libtier0), so we
    * branch around the __OBJC work instead of returning early. */
   /* Slide __DATA,__cfstring str pointers (the x86_64 32-byte records
    * macho-tool emits). MUST run OUTSIDE the objc_seg branch: a pure
    * Carbon/C++ translated app (Civ IV, Portal 2 family) has CFSTR("...")
    * constants — a __cfstring section — but NO __OBJC segment. Gated inside
    * the old `if (objc_seg)`, their records kept UNSLID preferred `str`
    * vmaddrs; i386_cfstr_to_real's slide-on-demand fallback only fires when
    * the unslid address is UNREADABLE, so whenever another mapping covered
    * the preferred page the recognizer read foreign bytes, failed the exact-
    * strnlen check, and passed the RAW record to native CF -> objc_msgSend
    * on isa = the wrapped class handle -> "Attempt to use unknown class
    * 0x800xxxxx" _objc_fatal (Civ IV CFStringReplace, trace-verified:
    * "[cfstr] 0x0d7165d0 REJECT strnlen cstr=0x10dae5f4 got=2"). Triggers
    * on the structural presence of __cfstring, not on __OBJC. */
   if (image_links_libabiconv(mh64) && slide != 0 && vmaddr_lo <= vmaddr_hi) {
      slide_cfstrings(mh64, slide, vmaddr_lo, vmaddr_hi, imgname);
   }

   if (objc_seg) {

   /* Slide the __OBJC pointer slots if the image moved. When
    * slide==0 (loaded at preferred vmaddr) the slots are already correct. */
   if (slide != 0 && vmaddr_lo <= vmaddr_hi) {
      if (g_verbose) {
         fprintf(stderr, "abiconv objc_slide: processing %s slide=0x%lx "
                 "__OBJC at vmaddr 0x%llx span [0x%llx,0x%llx)\n",
                 imgname, (long)slide,
                 (unsigned long long)objc_seg->vmaddr,
                 (unsigned long long)vmaddr_lo, (unsigned long long)vmaddr_hi);
      }

      const struct section_64 *sect =
         (const struct section_64 *)(objc_seg + 1);
      for (uint32_t i = 0; i < objc_seg->nsects; i++, sect++) {
         slide_section_4byte(imgname, sect->segname, sect->sectname,
                             sect->addr, sect->size, slide,
                             vmaddr_lo, vmaddr_hi);
      }
   }

   /* Repair the selector/class ref arrays from the on-disk originals. libobjc's
    * old-ABI reader corrupts these at map_images (before this callback), so the
    * in-place slide above can't recover them — see repair_refs_from_file. Runs
    * for slide==0 images too (the corruption is independent of the slide). */
   {
      const struct section_64 *sect =
         (const struct section_64 *)(objc_seg + 1);
      for (uint32_t i = 0; i < objc_seg->nsects; i++, sect++) {
         if (strncmp(sect->sectname, "__message_refs", 16) == 0
             || strncmp(sect->sectname, "__cls_refs", 16) == 0
             /* __category structs are all-pointer (name/class_name/
              * instance_methods/class_methods/protocols) — a flat pointer-array
              * repair is exactly correct and guards the category->method-list
              * pointer the reverse registration follows. */
             || strncmp(sect->sectname, "__category", 16) == 0) {
            repair_refs_from_file(imgname, sect->sectname, sect->addr,
                                  sect->offset, sect->size, slide,
                                  vmaddr_lo, vmaddr_hi);
         } else if (strncmp(sect->sectname, "__inst_meth", 16) == 0
                    || strncmp(sect->sectname, "__cls_meth", 16) == 0
                    || strncmp(sect->sectname, "__cat_inst_meth", 16) == 0
                    || strncmp(sect->sectname, "__cat_cls_meth", 16) == 0) {
            /* Method lists carry an integer count field, so they need the
             * structure-aware name/types repair (not the flat one). */
            repair_method_lists_from_file(imgname, sect->sectname, sect->addr,
                                          sect->offset, sect->size, slide,
                                          vmaddr_lo, vmaddr_hi);
         }
      }
   }

   /* Now that __OBJC pointer slots are correct, index this image's legacy
    * classes so the objc bridge can dispatch class messages to their own
    * translated IMPs (objc_shim.c). Runs for slide==0 images too. */
   _86x64_objc_index_legacy_classes(mh64, slide);

   /* Then register them with the modern runtime (reverse x86_64->i386 IMP
    * bridge) so NSClassFromString / AppKit can instantiate and message them. */
   _86x64_objc_register_classes(mh64, slide);

   }  /* end if (objc_seg) */

   /* Before running THIS image's collected initializers, make sure every
    * translated dependency has been slid + initialized (bottom-up order). Only
    * relevant in run-now mode (n_init>0, default); the legacy stub path lets dyld order the
    * wrapped-stub initializers and never populates init_targets. */
   if (n_init > 0) {
      process_deps(mh64);
      /* And every mapped copy of this runtime: the initializers below may read
       * redirected __ZTI* typeinfos bound to a libabiconv copy dyld has not
       * constructed yet (multi-copy deploys, Civ IV s24).  Image-count gated,
       * idempotent. */
      cxx_typeinfo_init_all_copies();
   }

   /* Run the collected translated static initializers LAST — after every
    * per-image fixup (the future 4-byte __DATA pointer slide, the __OBJC slide,
    * cfstring/ref repair, class registration) so a C++ static ctor or an
    * ObjC +load never dereferences an unslid pointer. On the low-4GB init
    * stack, in collection (≈section, bottom-up dependency) order; a nested
    * dlopen during one of these re-enters slide_objc and runs its own inits
    * first (the trampoline's shadow stack keeps nesting LIFO-correct).
    * n_init>0 in run-now mode (default); the legacy ABICONV_NO_RUN_INITS path wrapped the
    * slots into dyld-called stubs instead and leaves n_init==0. */
   for (size_t i = 0; i < n_init; i++) {
      if (getenv("ABICONV_INIT_TRACE")) {
         fprintf(stderr, "[abiconv] call_init[%zu/%zu] in %s fn=%p\n",
                 i, n_init, imgname ? imgname : "?", init_targets[i]);
         fflush(stderr);
      }
      init_enter(init_targets[i]);
      abiconv_call_init(init_targets[i], 0, NULL, NULL, NULL);
      init_leave();
   }
}

/* Fill the i386-layout libstdc++ RTTI typeinfo surface (cxx_typeinfo.c) of
 * EVERY mapped copy of this runtime, not just our own image (Civ IV s24).
 *
 * Multi-copy deploys co-locate one libabiconv per directory that hosts a
 * translated binary (the libabiconv multi-copy gotcha); each translated image's
 * @loader_path data binds resolve to ITS OWN co-located copy.  dyld constructs
 * the copies in dependency order, but the FIRST copy whose constructor runs
 * registers the add-image callback and thereby runs EVERY translated image's
 * static initializers right then — before dyld has constructed the OTHER
 * copies.  An app static init that reads a redirected __ZTI* typeinfo bound to
 * a not-yet-constructed copy sees a zero-filled object: typeid(T).name()
 * returns NULL without faulting, boost::python-style registrations cache the
 * NULL, and the eventual name strcmp derefs 0 (Civ IV s24: strcmp(0,0) in
 * converter registration insert_unique).
 *
 * Fix: before any translated initializer runs, walk the mapped images and
 * explicitly run the (idempotent, done-guarded) typeinfo init of every copy of
 * ourselves.  "Copy of ourselves" is identified by sharing our own basename —
 * derived from dladdr on this image, no hardcoded install names — and probed
 * with dlopen(RTLD_NOLOAD)+dlsym, so a renamed runtime family still matches
 * and non-copies are skipped.  Gated on the image count so repeat calls after
 * new dlopens stay cheap.  Only inits DESIGNED idempotent may be cross-called
 * this way (dyld will still run the other copy's constructors later); if
 * another zero-until-constructor surface shows up, add it next to the
 * _86x64_cxx_typeinfo_init call below rather than inventing a new walk. */
static void cxx_typeinfo_init_all_copies(void) {
   static uint32_t seen_images = 0;
   uint32_t n = x64_img_count();
   if (n == seen_images) { return; }
   seen_images = n;
   Dl_info self;
   if (!dladdr((void *)&cxx_typeinfo_init_all_copies, &self) ||
       self.dli_fname == NULL) {
      return;
   }
   const char *self_base = strrchr(self.dli_fname, '/');
   self_base = self_base ? self_base + 1 : self.dli_fname;
   for (uint32_t i = 0; i < n; i++) {
      const char *path = x64_img_path(i);
      if (path == NULL) { continue; }
      const char *base = x64_img_leaf(path);
      if (strcmp(base, self_base) != 0) { continue; }
      void *h = dlopen(path, RTLD_NOLOAD | RTLD_LAZY);
      if (h == NULL) { continue; }
      void (*fn)(void) = (void (*)(void))dlsym(h, "_86x64_cxx_typeinfo_init");
      if (fn != NULL) { fn(); }
      dlclose(h);
   }
}

__attribute__((constructor))
static void objc_slide_init(void) {
   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) { g_verbose = 1; }
   /* Fill the i386-layout libstdc++ RTTI typeinfos (cxx_typeinfo.c) BEFORE
    * registering the add-image callback: _dyld_register_func_for_add_image runs
    * slide_objc synchronously for already-mapped images, i.e. it runs the
    * translated app's static initializers right here — and those (boost::python
    * converter registration) read __ZTI* typeinfo __name fields. Mach-O does not
    * honor constructor priorities, so we cannot rely on cxx_typeinfo.c's own
    * constructor having run first; call it explicitly (idempotent), and do the
    * same for every OTHER mapped copy of this runtime (multi-copy deploys; the
    * translated images' typeinfo binds resolve per @loader_path to copies dyld
    * may not have constructed yet — Civ IV s24). */
   extern void _86x64_cxx_typeinfo_init(void);
   _86x64_cxx_typeinfo_init();
   cxx_typeinfo_init_all_copies();
   _dyld_register_func_for_add_image(&slide_objc);
}
