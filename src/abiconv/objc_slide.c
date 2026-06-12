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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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

/* Rewrite every __DATA,__mod_init_func 8-byte pointer in a translated image to
 * a stack-switching stub. Runs from the add-image callback, BEFORE dyld reads
 * __mod_init_func to run the initializers. */
static void wrap_mod_init_funcs(const struct mach_header_64 *mh64,
                                intptr_t slide, const char *imgname) {
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
            for (size_t j = 0; j < n; j++) {
               if (!slots[j]) { continue; }
               void *stub = make_init_stub(slots[j]);
               if (stub) { slots[j] = stub; ++wrapped; }
            }
            if (g_verbose) {
               fprintf(stderr, "abiconv init_stack: wrapped %zu/%zu init funcs "
                       "in %s\n", wrapped, n, imgname);
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

static void slide_objc(const struct mach_header *mh, intptr_t slide) {
   if (mh->magic != MH_MAGIC_64) { return; }
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
   for (uint32_t i = 0; i < mh64->ncmds; i++) {
      const struct load_command *lc = (const struct load_command *)p;
      if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg =
            (const struct segment_command_64 *)p;
         if (strncmp(seg->segname, "__OBJC", 16) == 0) {
            objc_seg = seg;
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
   if (!objc_seg) { return; }  /* not a translated binary with legacy ObjC1 */

   /* Run translated static initializers on a low-4GB stack. Independent of
    * slide (the >4GB-stack bug bites even at the preferred vmaddr), and must
    * happen before the slide==0 early-out below. See the init high-stack bug. */
   wrap_mod_init_funcs(mh64, slide, imgname);

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

   /* Now that __OBJC pointer slots are correct, index this image's legacy
    * classes so the objc bridge can dispatch class messages to their own
    * translated IMPs (objc_shim.c). Runs for slide==0 images too. */
   _86x64_objc_index_legacy_classes(mh64, slide);

   /* Then register them with the modern runtime (reverse x86_64->i386 IMP
    * bridge) so NSClassFromString / AppKit can instantiate and message them. */
   _86x64_objc_register_classes(mh64, slide);
}

__attribute__((constructor))
static void objc_slide_init(void) {
   if (getenv("ABICONV_OBJC_SLIDE_VERBOSE")) { g_verbose = 1; }
   _dyld_register_func_for_add_image(&slide_objc);
}
