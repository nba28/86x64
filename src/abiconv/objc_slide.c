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

static int g_verbose = 0;

/* Defined in objc_shim.c — index this image's legacy ObjC-1.0 classes so the
 * objc bridge can dispatch class messages to their own translated IMPs. */
extern void _86x64_objc_index_legacy_classes(const struct mach_header_64 *mh,
                                             intptr_t slide);
/* Register this image's legacy classes with the modern objc runtime so AppKit/
 * Foundation can message them (principal class, delegates, NSClassFromString). */
extern void _86x64_objc_register_classes(const struct mach_header_64 *mh,
                                         intptr_t slide);

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
extern void abiconv_init_trampoline(void);
/* Run one translated __mod_init_func on the low-4GB init stack and return (see
 * init_trampoline.asm). Used by the ABICONV_RUN_INITS path below. */
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
 * in ABICONV_RUN_INITS mode we ALSO process an image's dependencies proactively
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

static const struct mach_header *find_loaded_image(const char *leaf,
                                                   intptr_t *slide_out) {
   for (uint32_t i = 0; i < _dyld_image_count(); i++) {
      const char *n = _dyld_get_image_name(i);
      if (!n) { continue; }
      const char *b = strrchr(n, '/');
      b = b ? b + 1 : n;
      if (strcmp(b, leaf) == 0) {
         if (slide_out) { *slide_out = _dyld_get_image_vmaddr_slide(i); }
         return _dyld_get_image_header(i);
      }
   }
   return NULL;
}

/* Max translated __mod_init_func pointers we collect per image in RUN_INITS
 * mode before running them at the end of slide_objc. libtier0 has 11; any real
 * image is well under this. Overflow falls back to wrapping (the default path)
 * so nothing is silently dropped. */
#define INIT_COLLECT_MAX 1024

/* Rewrite every __DATA,__mod_init_func 8-byte pointer in a translated image to
 * a stack-switching stub. Runs from the add-image callback, BEFORE dyld reads
 * __mod_init_func to run the initializers.
 *
 * In ABICONV_RUN_INITS mode we do NOT run the initializers here — running them
 * mid-walk is too early (the image's other per-image fixups, esp. the 4-byte
 * __DATA pointer relocation and __OBJC slide, haven't happened yet, so a C++
 * static ctor would dereference an unslid pointer — see the Portal 2 translation notes
 * s3b/s4). Instead we COLLECT the target addresses into `collect[]` and NULL
 * the slots (so neither dyld nor a nested re-entry runs them); slide_objc runs
 * the collected targets at its very END, after all fixups. */
static void wrap_mod_init_funcs(const struct mach_header_64 *mh64,
                                intptr_t slide, const char *imgname,
                                void **collect, size_t *n_collect) {
   if (getenv("ABICONV_NO_INIT_STACKSWITCH")) { return; }
   if (init_stack_setup() != 0) { return; }

   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg = (const struct segment_command_64 *)p;
         const struct section_64 *sect = (const struct section_64 *)(seg + 1);
         for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
            const uint32_t type = sect->flags & SECTION_TYPE;
            if (type != S_MOD_INIT_FUNC_POINTERS) { continue; }
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
             *  - DEFAULT: rewrite each slot to a low-stack JIT stub and let dyld
             *    call it. Works on the dyld that shipped with the iPhoto-era
             *    macOS, but dyld4 VALIDATES __mod_init_func entries are in-image
             *    and SILENTLY SKIPS our out-of-image stubs → the initializers
             *    never run.
             *  - ABICONV_RUN_INITS: run each init OURSELVES right here (the
             *    add-image callback fires before dyld's init pass), on the
             *    low-4GB init stack, then NULL the slot so dyld skips it. This
             *    sidesteps dyld4's in-image validation entirely. Per-image, in
             *    image-load (≈bottom-up dependency) order; nested dlopen during
             *    an init re-enters here and the trampoline's shadow stack keeps
             *    nesting LIFO-correct. */
            const int run_now =
               collect != NULL && getenv("ABICONV_RUN_INITS") != NULL;
            for (size_t j = 0; j < n; j++) {
               if (!slots[j]) { continue; }
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
static void slide_data_fnptrs(const struct mach_header_64 *mh64, intptr_t slide,
                              uint64_t text_lo, uint64_t text_hi,
                              uint64_t vmaddr_lo, uint64_t vmaddr_hi,
                              const char *imgname) {
   if (slide == 0 || vmaddr_lo >= vmaddr_hi) { return; }
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
               if (strncmp(sect->sectname, "__cfstring", 16) == 0) { continue; }
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

/* real 64-bit pointer -> low-4GB proxy handle (objc_shim.c). */
extern uint32_t x64_objc_wrap(uint64_t real);

/* Resolve an xrel target symbol to a LOW-4GB-usable address (0 if not found).
 * Mirrors the data-shadow ctor (objc_shim.c x64_init_data_shadows): dlsym the C
 * name (strip the linker's leading '_'); a defined-local or libabiconv symbol
 * resolves <4GB and is used directly; a native >4GB symbol (e.g. libc++abi's
 * __cxxabiv1 typeinfo vtables, ___cxa_pure_virtual) is wrapped into a low-4GB
 * proxy handle so the 4-byte slot is non-NULL and READABLE — which is all that's
 * needed to stop the dropped-reloc deref SIGBUS. (Full C++ RTTI correctness
 * across the i386/x86_64 typeinfo layout difference is a separate Tier-2
 * concern; see the known-gaps list.) */
static uint64_t xrel_resolve(const char *name) {
   if (name == NULL || name[0] == '\0') { return 0; }
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
   {
      const uint8_t *p = (const uint8_t *)(mh64 + 1);
      for (uint32_t i = 0; i < mh64->ncmds && !xrel; i++) {
         const struct load_command *lc = (const struct load_command *)p;
         if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *seg =
               (const struct segment_command_64 *)p;
            const struct section_64 *sect =
               (const struct section_64 *)(seg + 1);
            for (uint32_t s = 0; s < seg->nsects; s++, sect++) {
               if (strncmp(sect->sectname, "__86x64_xrel", 16) == 0) {
                  xrel = (const uint8_t *)(uintptr_t)(sect->addr + slide);
                  break;
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
      if (target == 0) { ++unresolved; continue; }        /* leave it NULL */

      uint32_t *slot =
         (uint32_t *)(uintptr_t)((uint64_t)slot_vmaddr + (int64_t)slide);
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

static void slide_objc(const struct mach_header *mh, intptr_t slide);

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
   const struct mach_header_64 *mh64 = (const struct mach_header_64 *)mh;
   const uint8_t *p = (const uint8_t *)(mh64 + 1);
   const char *imgname = "?";
   for (uint32_t i = 0; i < _dyld_image_count(); i++) {
      if (_dyld_get_image_header(i) == mh) {
         imgname = _dyld_get_image_name(i);
         break;
      }
   }

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
      wrap_mod_init_funcs(mh64, slide, imgname, init_targets, &n_init);
      /* Slide 4-byte __DATA intra-image pointer slots (vtables, fn-ptr tables,
       * AND data->data global pointers) by the load slide BEFORE the collected
       * static initializers run (a C++ ctor stores/derefs these). Universal:
       * triggers on the structural property "4-byte __DATA slot whose value is
       * an intra-image pointer" (macho-tool's exact DataParser predicate), not
       * on any app. */
      slide_data_fnptrs(mh64, slide, text_lo, text_hi,
                        vmaddr_lo, vmaddr_hi, imgname);
      /* Bind the classic external __DATA relocations dyld can't process (the
       * lifted C++ vtable/RTTI imports in __DATA,__86x64_xrel) before any C++
       * static ctor dereferences them. Universal: triggers only on the presence
       * of the macho-tool-emitted section. */
      bind_external_relocs(mh64, slide, imgname);
   }

   /* the legacy-ObjC1 fixups below are __OBJC-only, but the collected
    * static initializers (n_init>0, ABICONV_RUN_INITS mode) must still run at
    * the END for a pure-C++ no-__OBJC image (e.g. Portal 2's libtier0), so we
    * branch around the __OBJC work instead of returning early. */
   if (objc_seg) {

   /* Slide the __OBJC/__cfstring pointer slots if the image moved. When
    * slide==0 (loaded at preferred vmaddr) the slots are already correct. */
   if (slide != 0 && vmaddr_lo <= vmaddr_hi) {
      /* Slide __DATA,__cfstring str pointers (the x86_64 32-byte records
       * macho-tool emits). Independent of __OBJC, but translated ObjC apps
       * always have both. */
      slide_cfstrings(mh64, slide, vmaddr_lo, vmaddr_hi, imgname);

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
             || strncmp(sect->sectname, "__cls_refs", 16) == 0) {
            repair_refs_from_file(imgname, sect->sectname, sect->addr,
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
    * relevant in RUN_INITS mode (n_init>0); the default path lets dyld order the
    * wrapped-stub initializers and never populates init_targets. */
   if (n_init > 0) {
      process_deps(mh64);
   }

   /* Run the collected translated static initializers LAST — after every
    * per-image fixup (the future 4-byte __DATA pointer slide, the __OBJC slide,
    * cfstring/ref repair, class registration) so a C++ static ctor or an
    * ObjC +load never dereferences an unslid pointer. On the low-4GB init
    * stack, in collection (≈section, bottom-up dependency) order; a nested
    * dlopen during one of these re-enters slide_objc and runs its own inits
    * first (the trampoline's shadow stack keeps nesting LIFO-correct).
    * n_init>0 only in ABICONV_RUN_INITS mode; the default path wrapped the
    * slots into dyld-called stubs instead and leaves n_init==0. */
   for (size_t i = 0; i < n_init; i++) {
      abiconv_call_init(init_targets[i], 0, NULL, NULL, NULL);
   }
}

__attribute__((constructor))
static void objc_slide_init(void) {
   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) { g_verbose = 1; }
   _dyld_register_func_for_add_image(&slide_objc);
}
