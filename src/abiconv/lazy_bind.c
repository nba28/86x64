/*
 * lazy_bind.c — in-process lazy-symbol binding for translated images.
 *
 * The old __dyld_stub_binder shim parked the interrupted thread's rsp in a
 * PROCESS-WIDE global (__dyld_stub_binder_flag) and let the real
 * dyld_stub_binder jump straight into the bound shim, whose preamble
 * repaired rsp from that global. Two threads lazy-binding (or one binding
 * while another entered any shim) adopted each other's stack — the
 * 23rd-blocker crash family: wild jumps through string/code bytes, arena
 * handles surfacing as native msgSend receivers, all timing-dependent and
 * clustered around the first moment background queues (FileCoordination,
 * StateRestoration) run translated code concurrently with the main thread.
 *
 * This helper replaces the dyld round-trip entirely: dyld_stub_binder.asm
 * keeps every register it needs on the calling thread's own stack, CALLS
 * x64_lazy_bind_helper() to resolve + write the lazy pointer, and jumps to
 * the returned target with the original i386 frame already restored. No
 * global, no cross-thread state, and the shim-preamble flag check is now a
 * permanent no-op (the flag stays 0; kept only for link compatibility).
 *
 * Only TRANSLATED images route through the shim (static-interpose rewrites
 * their dyld_stub_binder import), and macho-tool generates their bind info,
 * so the opcode vocabulary here is exactly what dyldinfo.cc emits for lazy
 * blobs: SET_DYLIB_ORDINAL_IMM/ULEB, SET_DYLIB_SPECIAL_IMM,
 * SET_SYMBOL_TRAILING_FLAGS_IMM, SET_TYPE_IMM, SET_SEGMENT_AND_OFFSET_ULEB,
 * DO_BIND (DONE separates entries). SET_ADDEND_SLEB/ADD_ADDR_ULEB are
 * accepted defensively.
 *
 * Thread-safety: all parsing is read-only over mapped images; the only
 * write is the final aligned 8-byte lazy-pointer store (atomic on x86-64,
 * and idempotent — two threads binding the same stub store the same value).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

#define MAX_SEGS 24
#define MAX_DEPS 96

static uint64_t read_uleb(const uint8_t **pp, const uint8_t *end) {
   uint64_t v = 0;
   unsigned shift = 0;
   const uint8_t *p = *pp;
   while (p < end) {
      uint8_t b = *p++;
      v |= (uint64_t)(b & 0x7f) << shift;
      shift += 7;
      if (!(b & 0x80)) break;
   }
   *pp = p;
   return v;
}

static int64_t read_sleb(const uint8_t **pp, const uint8_t *end) {
   int64_t v = 0;
   unsigned shift = 0;
   const uint8_t *p = *pp;
   uint8_t b = 0;
   while (p < end) {
      b = *p++;
      v |= (int64_t)(b & 0x7f) << shift;
      shift += 7;
      if (!(b & 0x80)) break;
   }
   if (shift < 64 && (b & 0x40)) v |= -(1LL << shift);
   *pp = p;
   return v;
}

__attribute__((noreturn))
static void die(const char *what, const char *detail) {
   fprintf(stderr, "abiconv: lazy bind: %s%s%s\n",
           what, detail ? ": " : "", detail ? detail : "");
   fflush(stderr);
   abort();
}

uint64_t x64_lazy_bind_helper(const void *image_mark, uint64_t lazy_off) {
   const int trace = getenv("LAZY_BIND_TRACE") != NULL;

   /* --- locate the image whose stub helper jumped here: the first qword the
    * helper head pushed is the address of that image's own __dyld_private,
    * i.e. an address inside one of its segments. --- */
   const struct mach_header_64 *mh = NULL;
   intptr_t slide = 0;
   const char *image_path = NULL;
   const uint32_t nimg = _dyld_image_count();
   for (uint32_t i = 0; i < nimg && !mh; ++i) {
      const struct mach_header *h = _dyld_get_image_header(i);
      if (!h || h->magic != MH_MAGIC_64) continue;
      const intptr_t sl = _dyld_get_image_vmaddr_slide(i);
      const struct mach_header_64 *h64 = (const struct mach_header_64 *)h;
      const struct load_command *lc = (const struct load_command *)(h64 + 1);
      for (uint32_t k = 0; k < h64->ncmds; ++k) {
         if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *sc =
               (const struct segment_command_64 *)lc;
            const uintptr_t lo = (uintptr_t)sc->vmaddr + (uintptr_t)sl;
            if (sc->vmsize != 0 && (uintptr_t)image_mark >= lo
                && (uintptr_t)image_mark - lo < sc->vmsize) {
               mh = h64;
               slide = sl;
               image_path = _dyld_get_image_name(i);
               break;
            }
         }
         lc = (const struct load_command *)((const char *)lc + lc->cmdsize);
      }
   }
   if (!mh) die("no loaded image contains the stub-helper mark", NULL);

   /* --- gather segments, dependent dylibs, dyld info --- */
   struct { uint64_t vmaddr; uint64_t vmsize; } segs[MAX_SEGS];
   const char *deps[MAX_DEPS];
   unsigned nsegs = 0, ndeps = 0;
   const struct dyld_info_command *di = NULL;
   uint64_t linkedit_vm = 0, linkedit_fileoff = 0;

   const struct load_command *lc = (const struct load_command *)(mh + 1);
   for (uint32_t k = 0; k < mh->ncmds; ++k) {
      switch (lc->cmd) {
      case LC_SEGMENT_64: {
         const struct segment_command_64 *sc =
            (const struct segment_command_64 *)lc;
         if (nsegs < MAX_SEGS) {
            segs[nsegs].vmaddr = sc->vmaddr;
            segs[nsegs].vmsize = sc->vmsize;
            ++nsegs;
         }
         if (strcmp(sc->segname, SEG_LINKEDIT) == 0) {
            linkedit_vm = sc->vmaddr;
            linkedit_fileoff = sc->fileoff;
         }
         break;
      }
      case LC_LOAD_DYLIB:
      case LC_LOAD_WEAK_DYLIB:
      case LC_REEXPORT_DYLIB:
      case LC_LOAD_UPWARD_DYLIB: {
         const struct dylib_command *dc = (const struct dylib_command *)lc;
         if (ndeps < MAX_DEPS) {
            deps[ndeps++] = (const char *)lc + dc->dylib.name.offset;
         }
         break;
      }
      case LC_DYLD_INFO:
      case LC_DYLD_INFO_ONLY:
         di = (const struct dyld_info_command *)lc;
         break;
      }
      lc = (const struct load_command *)((const char *)lc + lc->cmdsize);
   }
   if (!di || di->lazy_bind_size == 0)
      die("image has no lazy bind info", image_path);
   if (!linkedit_vm)
      die("image has no __LINKEDIT segment", image_path);
   if (lazy_off >= di->lazy_bind_size)
      die("lazy bind offset out of range", image_path);

   const uint8_t *blob = (const uint8_t *)
      (linkedit_vm + (uintptr_t)slide + (di->lazy_bind_off - linkedit_fileoff));
   const uint8_t *p = blob + lazy_off;
   const uint8_t *end = blob + di->lazy_bind_size;

   /* --- parse one lazy bind entry (terminated by DO_BIND) --- */
   int seg_index = -1;
   uint64_t seg_offset = 0;
   int ordinal = 0;                 /* BIND_SPECIAL_DYLIB_SELF default */
   const char *symname = NULL;
   int bound = 0;
   while (p < end && !bound) {
      const uint8_t byte = *p++;
      const uint8_t op = byte & BIND_OPCODE_MASK;
      const uint8_t imm = byte & BIND_IMMEDIATE_MASK;
      switch (op) {
      case BIND_OPCODE_DONE:
         die("hit DONE before DO_BIND (bad lazy offset)", image_path);
      case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
         ordinal = imm;
         break;
      case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
         ordinal = (int)read_uleb(&p, end);
         break;
      case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
         /* 0 = SELF; nonzero imms are sign-extended negatives (-1..-3) */
         ordinal = imm ? (int)(int8_t)(imm | 0xf0) : 0;
         break;
      case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
         symname = (const char *)p;
         while (p < end && *p) ++p;
         ++p;                        /* NUL */
         break;
      case BIND_OPCODE_SET_TYPE_IMM:
         break;
      case BIND_OPCODE_SET_ADDEND_SLEB:
         (void)read_sleb(&p, end);   /* lazy pointers carry no addend */
         break;
      case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
         seg_index = imm;
         seg_offset = read_uleb(&p, end);
         break;
      case BIND_OPCODE_ADD_ADDR_ULEB:
         seg_offset += read_uleb(&p, end);
         break;
      case BIND_OPCODE_DO_BIND:
         bound = 1;
         break;
      default:
         die("unexpected lazy bind opcode", image_path);
      }
   }
   if (!bound || !symname || seg_index < 0 || (unsigned)seg_index >= nsegs)
      die("malformed lazy bind entry", image_path);
   if (seg_offset + 8 > segs[seg_index].vmsize)
      die("lazy pointer offset out of segment", symname);

   /* --- resolve: try the bind's own dylib ordinal first (two-level), fall
    * back to a flat lookup (covers @rpath deps dlopen can't re-key, and the
    * special FLAT/SELF/MAIN ordinals; shim exports have unique names, so a
    * flat hit is unambiguous in practice). --- */
   const char *dlname = (symname[0] == '_') ? symname + 1 : symname;
   void *addr = NULL;
   if (ordinal > 0 && (unsigned)ordinal <= ndeps) {
      void *h = dlopen(deps[ordinal - 1], RTLD_LAZY | RTLD_NOLOAD);
      if (h) addr = dlsym(h, dlname);
   }
   if (!addr) addr = dlsym(RTLD_DEFAULT, dlname);
   if (!addr) die("symbol not found", symname);

   uint64_t *slot = (uint64_t *)
      ((uintptr_t)segs[seg_index].vmaddr + (uintptr_t)slide + seg_offset);
   *slot = (uint64_t)(uintptr_t)addr;

   if (trace) {
      fprintf(stderr, "[lb] %s: %s ord=%d -> %p (slot %p)\n",
              image_path ? image_path : "?", symname, ordinal, addr,
              (void *)slot);
      fflush(stderr);
   }
   return (uint64_t)(uintptr_t)addr;
}
