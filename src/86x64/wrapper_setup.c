/*
 * wrapper_setup.c — build an i386-shaped argv frame in 32-bit-addressable
 * memory and hand it off to the translated _main.
 *
 * The translated binary's entry point is the libc-style __start trampoline
 * (renamed _main by our convert step). It expects the stack frame the
 * macOS i386 kernel would set up:
 *
 *     esp -> argc                 (4 bytes)
 *            argv[0] ptr          (4 bytes, 32-bit)
 *            argv[1] ptr          ...
 *            ...
 *            argv[argc-1] ptr
 *            NULL                  (argv terminator)
 *            envp[0] ptr
 *            ...
 *            NULL                  (envp terminator)
 *            apple[0] ptr
 *            ...
 *            NULL                  (apple terminator)
 *
 * Everything must live below 4 GB because the translated code dereferences
 * 32-bit pointers. We allocate a 16 MB region with mmap (libinterpose
 * forces it into the low 4 GB via MAP_FIXED probing), put the argv strings
 * near the top of that region, then lay out the argument vector array just
 * below the strings. The returned `new_rsp` is the address the caller
 * should set rsp to before jumping into _main.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_param.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#define STACK_REGION_SIZE (16U * 1024U * 1024U)   /* 16 MB scratch region */
#define ARG_FRAME_SIZE    (1U * 1024U * 1024U)    /* room for argv+strings */

/*
 * Shim FILE structs published to the translated i386 program so that
 * pointers like `__stderrp`/`__stdoutp`/`__stdinp` are addressable inside
 * the 32-bit pointer space.
 *
 * The i386 code treats `FILE *` opaquely in photocd (it never reads struct
 * fields directly — every load of a FILE pointer immediately gets passed
 * to a libc function like fprintf or fwrite). We exploit that: each shim
 * is just an identifier the abiconv-side shims can use to look up the
 * real libsystem FILE *. If a target ever does start dereferencing fields
 * we can grow the struct to mimic the i386 __sFILE layout — the `real_fp`
 * field that abiconv reads stays at offset 0 either way.
 */
struct shim_FILE {
   void *real_fp;     /* offset 0: real libsystem FILE * (high-4GB) */
   uint32_t magic;    /* offset 8: identifies this as our shim */
   int fd;            /* offset 12: underlying fd (0/1/2 or fopen result) */
   char _pad[112];    /* keep struct comfortably bigger than i386 __sFILE */
};
#define SHIM_FILE_MAGIC 0x68690a55  /* "U\nih" — arbitrary sentinel */

/* These addresses are exported to abiconv so its FILE-taking shims can
 * recognize shim FILE pointers. abiconv reads them via dlsym from the
 * wrapper executable (linked at -e _main_wrapper, but symbols are
 * exported as globals). */
struct shim_FILE *_86x64_stderr_shim = NULL;
struct shim_FILE *_86x64_stdout_shim = NULL;
struct shim_FILE *_86x64_stdin_shim  = NULL;

#define LOW_REGION_BASE 0x080000000UL
#define LOW_REGION_END  0x0F0000000UL

/* The vmaddr base that macho-tool's transform/convert step chose for the
 * translated dylib. Used only to identify which dyld images are our
 * translated dylibs (image filter). The 256MB search window must be wide
 * enough to cover the largest dylib we translate, but is NOT used as the
 * value-classification range for the imm32 patcher — that uses the
 * dylib's actual segment vmaddr span (see dylib_vmaddr_lo/hi below).
 * A too-wide value range falsely classifies user-code literal constants
 * as stale pointers; see the libabiconv vararg bug notes. */
#define TRANSLATED_DYLIB_VMADDR  0x10000000U
#define TRANSLATED_DYLIB_VMSIZE  0x10000000U

static void allocate_shim_files(void);
static void redirect_stdio_symbol_ptrs(const struct mach_header_64 *mh,
                                       intptr_t slide);

static void fixup_translated_dylib_slots(void) {
   /* NOTE 2026-05-29: this runs from build_i386_main_frame, which fires
    * AFTER all dyld static initializers. For images whose initializers
    * touch slid pointers (Tessera's static init does), the fixup is
    * too late. The runtime path that fires earlier is
    * /tmp/objcslideshim/objcslideshim.dylib (constructor +
    * _dyld_register_func_for_add_image); add it to DYLD_INSERT_LIBRARIES.
    * The wrapper-side fixup is kept for non-__OBJC sections
    * (__DATA,__data; __TEXT,__const/__text) the slideshim doesn't
    * touch, and as a backstop for images whose initializers run after
    * main. */
   uint32_t image_count = _dyld_image_count();
   for (uint32_t i = 0; i < image_count; ++i) {
      const char *name = _dyld_get_image_name(i);
      if (!name) continue;
      const char *base = strrchr(name, '/');
      base = base ? base + 1 : name;
      /* Match any *.dylib that loads at the translated vmaddr — typically
       * just one (photocd.dylib or whatever the user converted). */
      const struct mach_header *mh = _dyld_get_image_header(i);
      if (!mh || mh->magic != MH_MAGIC_64) continue;

      intptr_t slide = _dyld_get_image_vmaddr_slide(i);
      /* Heuristic: only process the dylib whose preferred vmaddr falls in
       * the range we chose for translated dylibs. */

      const struct mach_header_64 *mh64 = (const struct mach_header_64 *)mh;
      const uint8_t *cmd_ptr = (const uint8_t *)(mh64 + 1);
      int is_translated = 0;
      uintptr_t data_runtime_addr = 0;
      uintptr_t data_size = 0;
      /*
       * Collect the actual pre-slide vmaddr span of this dylib by
       * walking its LC_SEGMENT_64 list. The old code keyed every imm32
       * check off TRANSLATED_DYLIB_VMSIZE = 256MB — way bigger than any
       * real translated dylib. A 32-bit literal constant in user code
       * like `mov [esp+4], 0x11111111` falls into that 256MB window and
       * gets misclassified as a stale pointer, then patched with
       * `(value + slide)` — producing garbage args at runtime
       * (4-arg %x printf bug). The real range is tight: TEXT base to
       * the end of the highest segment.
       */
      uint64_t dylib_vmaddr_lo = ~(uint64_t)0;
      uint64_t dylib_vmaddr_hi = 0;

      for (uint32_t c = 0; c < mh64->ncmds; ++c) {
         const struct load_command *lc = (const struct load_command *)cmd_ptr;
         if (lc->cmd == LC_SEGMENT_64) {
            const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
            if (seg->vmaddr >= TRANSLATED_DYLIB_VMADDR
                && seg->vmaddr <  TRANSLATED_DYLIB_VMADDR + TRANSLATED_DYLIB_VMSIZE) {
               is_translated = 1;
            }
            /* Skip __PAGEZERO and __LINKEDIT from the range — pagezero
             * is always at 0 (or absent in a dylib) and __LINKEDIT
             * doesn't hold patchable code/data. */
            if (strcmp(seg->segname, "__PAGEZERO") != 0
                && strcmp(seg->segname, "__LINKEDIT") != 0
                && seg->vmsize > 0) {
               if (seg->vmaddr < dylib_vmaddr_lo) dylib_vmaddr_lo = seg->vmaddr;
               if (seg->vmaddr + seg->vmsize > dylib_vmaddr_hi)
                  dylib_vmaddr_hi = seg->vmaddr + seg->vmsize;
            }
            if (strcmp(seg->segname, "__DATA") == 0) {
               const struct section_64 *sects =
                  (const struct section_64 *)(seg + 1);
               for (uint32_t s = 0; s < seg->nsects; ++s) {
                  if (strcmp(sects[s].sectname, "__data") == 0) {
                     data_runtime_addr = sects[s].addr + slide;
                     data_size = sects[s].size;
                     break;
                  }
               }
            }
         }
         cmd_ptr += lc->cmdsize;
      }

      if (!is_translated) continue;
      /* slide==0 still needs stdio redirect (the dyld bind already
       * pointed slots at libsystem high-memory addresses). */
      redirect_stdio_symbol_ptrs(mh64, slide);
      if (slide == 0) continue;

      /* Walk relevant sections 4 bytes at a time; rewrite any slot whose
       * value points into the dylib's expected (pre-slide) vmaddr range.
       * Cover both __DATA (rw, no mprotect needed) and __TEXT,__const
       * (rx, needs a temporary mprotect to make writable). */
      struct fixup_section {
         const char *segname;
         const char *sectname;
         int needs_mprotect;
         int aligned4;  /* require 4-byte-aligned reads */
      } targets[] = {
         { "__DATA", "__data",          0, 1 },
         /* LOCAL entries in __nl_symbol_ptr hold compile-time
          * pointers that the translator now preserves through
          * convert (see NonLazySymbolPointer::raw_data). Those
          * values are pre-slide vmaddrs and need the runtime
          * slide added like everything else in __DATA. dyld
          * already overwrites the bound entries (___stderrp,
          * etc.) before this runs, and our patched values fall
          * inside the dylib's vmaddr range while libsystem's
          * don't — so the heuristic naturally skips bound
          * entries. */
         { "__DATA", "__nl_symbol_ptr", 0, 1 },
         /* C++ vtables / fn-ptr dispatch tables and const pointer arrays the
          * compiler emitted into __DATA,__const (RW, no mprotect). Their
          * entries are internal pointers the translator relocated to new
          * pre-slide vmaddrs at convert time; slide them like __data. Without
          * this a vtable dispatch reads an un-slid __text entry and jumps to
          * the pre-slide address -> SIGBUS/KERN_PROTECTION_FAILURE (exposed
          * once `movl $vtable,(%reg)` installs the table correctly — the
          * pointer-table store fix in instruction.cc). */
         { "__DATA", "__const",         0, 1 },
         { "__TEXT", "__const",         1, 1 },
         /*
          * __text holds instructions whose disp32 immediates point at
          * jump tables in __TEXT,__const (e.g. `jmpq *<disp32>(,%rax,4)`).
          * The translator rewrites disp32 to the new vmaddr at convert
          * time, but the value is encoded inside an instruction at an
          * arbitrary byte offset, so we have to scan unaligned to catch it.
          * False positives are mostly harmless because we only patch values
          * that fall in our private dylib vmaddr range.
          */
         { "__TEXT", "__text",  1, 0 },
         /*
          * Legacy ObjC metadata. __message_refs / __cls_refs are 4-byte
          * pointer arrays into __cstring; __module_info entries carry
          * name/symtab pointers. Without sliding these, the objc bridge
          * hands stale pre-slide cstring pointers to objc_getClass /
          * sel_registerName. The __OBJC segment is mapped RW (initprot
          * 0x3) so no mprotect is needed.
          */
         { "__OBJC", "__message_refs", 0, 1 },
         { "__OBJC", "__cls_refs",     0, 1 },
         { "__OBJC", "__module_info",  0, 1 },
         { NULL, NULL, 0, 0 },
      };

      cmd_ptr = (const uint8_t *)(mh64 + 1);
      for (uint32_t c = 0; c < mh64->ncmds; ++c) {
         const struct load_command *lc = (const struct load_command *)cmd_ptr;
         cmd_ptr += lc->cmdsize;
         if (lc->cmd != LC_SEGMENT_64) continue;
         const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
         const struct section_64 *sects = (const struct section_64 *)(seg + 1);
         for (uint32_t s = 0; s < seg->nsects; ++s) {
            int matched = -1;
            for (int t = 0; targets[t].segname; ++t) {
               if (strcmp(seg->segname, targets[t].segname) == 0
                   && strcmp(sects[s].sectname, targets[t].sectname) == 0) {
                  matched = t;
                  break;
               }
            }
            if (matched < 0) continue;

            uintptr_t addr = sects[s].addr + slide;
            size_t sz = sects[s].size;
            if (targets[matched].needs_mprotect) {
               /* Page-align and mprotect RW. __TEXT is mapped RX otherwise. */
               uintptr_t pg = addr & ~(uintptr_t)0xFFF;
               size_t pglen = ((addr + sz + 0xFFF) & ~(uintptr_t)0xFFF) - pg;
               if (mprotect((void *)pg, pglen, PROT_READ | PROT_WRITE) != 0) {
                  perror("wrapper: mprotect rw __TEXT,__const");
                  /* keep going — maybe section had no pointer entries */
                  continue;
               }
            }

            size_t patched = 0;
            if (targets[matched].aligned4) {
               uint32_t *p = (uint32_t *)addr;
               size_t n = sz / sizeof(uint32_t);
               for (size_t k = 0; k < n; ++k) {
                  uint32_t v = p[k];
                  if (v >= dylib_vmaddr_lo
                      && v <  dylib_vmaddr_hi) {
                     p[k] = (uint32_t)((uintptr_t)v + slide);
                     ++patched;
                  }
               }
            } else {
               /*
                * __text scan: only patch when we recognize a specific
                * known prefix for `[disp32 + idx*scale]` addressing where
                * disp32 follows in the next 4 bytes. Patching at every
                * byte offset matched random instruction bytes (mov rax,
                * <imm32>, etc.) and corrupted code. The opcode/ModR/M/SIB
                * combinations below cover the indirect jmp/call/mov forms
                * the i386 compiler used for switch dispatch — extend the
                * table if a new translated binary uses something else.
                *
                * Format: opcode, modrm, sib_mask, sib_match
                *   opcode = first byte
                *   modrm  = ModR/M with reg field zeroed (we match only
                *            mod=00 rm=100 = "SIB follows")
                *   sib_*  = restricts SIB to base=101 (no-base, disp32) at
                *            scale=4 (top 2 bits = 10). Index can be any.
                */
               static const struct {
                  uint8_t op;
                  uint8_t modrm;
               } patterns[] = {
                  /* FF /4: jmp [disp32 + idx*4]; ModR/M for /4 is 100 -> 0x24 */
                  { 0xFF, 0x24 },
                  /* FF /2: call [disp32 + idx*4]; ModR/M for /2 is 010 -> 0x14 */
                  { 0xFF, 0x14 },
                  /* 8B /r: mov rN, [disp32 + idx*4]; we accept any /r since
                   * the SIB-base=disp32 distinguishes the addressing. */
                  { 0x8B, 0x00 },  /* matched as "8B + any ModR/M with mod=00,rm=100" */
                  /* 03 /r: add rN, [disp32 + idx*4] */
                  { 0x03, 0x00 },
                  /* 89 /r: mov [disp32 + idx*4], rN — the STORE form. Missing
                   * from this table until Civ IV s21: NiStaticDataManager::
                   * AddLibrary writes its ms_apfnInitFunctions bss array via
                   * `67 89 14 85 <disp32>` and the unslid disp32 SIGSEGV'd at
                   * the preferred vmaddr. (objc_slide.c's patch_text_abs32
                   * now also patches these at add-image time, BEFORE the
                   * ABICONV_RUN_INITS ctors — this wrapper pass fires too
                   * late for those; kept as the backstop for non-RUN_INITS
                   * and post-main loads.) */
                  { 0x89, 0x00 },
                  /* 8D /r: lea rN, [disp32 + idx*scale] — address-of-element
                   * (Civ IV: &FConsoleCmd::m_SigTypes[i] handed to strcmp). */
                  { 0x8D, 0x00 },
                  /* sentinel */
                  { 0, 0 },
               };

               uint8_t *p = (uint8_t *)addr;
               for (size_t k = 0; k + 7 <= sz; ++k) {
                  /*
                   * Optional 0x67 address-size override. The translator
                   * prepends 0x67 to `[disp32 + idx*scale]` memory operands so
                   * the i386 32-bit effective-address WRAP is preserved in
                   * x86_64 (instruction.cc copy ctor — a negative/sentinel
                   * index must wrap mod 2^32 instead of computing a >4GB EA).
                   * The prefix shifts the opcode/ModRM/SIB/disp32 one byte, so
                   * skip it here or the disp32 below would never be slid.
                   */
                  /* Prefix window: the 0x67 address-size override plus at most
                   * one SSE mandatory prefix (F2 movsd / F3 movss / 66), in
                   * either order — e.g. `67 F2 0F 10 04 C5 disp32` (Quinn
                   * -[QuinnGame enableTimer] `movsd disp(,%eax,8), %xmm0`
                   * loading its repeating-NSTimer interval from the per-level
                   * speed table; unslid it read garbage and the tiny-positive
                   * interval tripped CF's "interval of 0 is set to repeat"
                   * ud2 in __CFRunLoopDoTimer). */
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
                  /* Optional two-byte-opcode escape (0x0F). The sign/zero-extend
                   * loads movsbl/movswl (0F BE/BF) and movzbl/movzwl (0F B6/B7)
                   * also use `[disp32 + idx*scale]` (e.g. Quinn's
                   * -[QuinnGame incrementScore...]'s `movswl disp(,%eax,8)` into
                   * __TEXT,__const), as do the SSE scalar/packed memory forms.
                   * The 0F escape shifts ModRM/SIB/disp32 one byte, so account
                   * for it or the disp32 below is never slid. */
                  size_t esc = (k + pfx < sz && p[k + pfx] == 0x0F) ? 1 : 0;
                  if (k + pfx + esc + 7 > sz) continue;
                  uint8_t op = p[k + pfx + esc];
                  uint8_t modrm = p[k + pfx + esc + 1];
                  uint8_t sib = p[k + pfx + esc + 2];
                  /* SIB must encode base=disp32 (low 3 bits = 101) at scale=4
                   * (0x85, 4-byte table entries) or scale=8 (0xC5, 8-byte
                   * entries — e.g. Quinn's `movl disp(,%edx,8)`). Index any. */
                  const uint8_t sc = sib & 0xC7;
                  if (sc != 0x85 && sc != 0xC5) continue;
                  /* mod=00 rm=100 means "SIB follows with disp32 base" */
                  if ((modrm & 0xC7) != 0x04) continue;
                  int matched_pat = 0;
                  if (esc) {
                     /* Two-byte (0F) table accesses: movsx/movzx integer loads
                      * (BE/BF/B6/B7) and the SSE memory-operand family —
                      * movss/movsd/movups/movaps + 66-prefixed pd forms
                      * (10/11/28/29), cvtsi2ss/sd, cvttss/sd2si, ucomiss/sd
                      * (2A/2C/2D/2E/2F), sqrt/logic/arith/min/max (51,54-5F),
                      * movd/movdqa/movdqu/movq (6E/6F/7E/7F/D6), cvtdq (E6).
                      * The disp32-base SIB above already disambiguates. */
                     matched_pat = (op == 0xBE || op == 0xBF
                                    || op == 0xB6 || op == 0xB7)
                                   || (op == 0x10 || op == 0x11
                                       || op == 0x28 || op == 0x29)
                                   || (op == 0x2A || op == 0x2C || op == 0x2D
                                       || op == 0x2E || op == 0x2F)
                                   || (op == 0x51 || (op >= 0x54 && op <= 0x5F))
                                   || (op == 0x6E || op == 0x6F || op == 0x7E
                                       || op == 0x7F || op == 0xD6
                                       || op == 0xE6);
                  } else if (op >= 0xD8 && op <= 0xDF) {
                     /* x87 escape opcodes: fld/fst/fadd/... memory forms — the
                      * pre-SSE compilers' indexed FP-table access
                      * (`fldl disp(,%eax,8)` = DD 04 C5 disp32). Any /r. */
                     matched_pat = 1;
                  } else {
                     for (int q = 0; patterns[q].op; ++q) {
                        if (op != patterns[q].op) continue;
                        /* When patterns.modrm != 0, the /N reg field must match. */
                        if (patterns[q].modrm != 0
                            && (modrm & 0x38) != (patterns[q].modrm & 0x38)) continue;
                        matched_pat = 1;
                        break;
                     }
                  }
                  if (!matched_pat) continue;
                  uint32_t v;
                  memcpy(&v, p + k + pfx + esc + 3, sizeof v);
                  if (v >= dylib_vmaddr_lo
                      && v <  dylib_vmaddr_hi) {
                     uint32_t patched_val = (uint32_t)((uintptr_t)v + slide);
                     memcpy(p + k + pfx + esc + 3, &patched_val, sizeof patched_val);
                     ++patched;
                     k += pfx + esc + 6;  /* skip past this instruction window */
                  }
               }

               /*
                * Second pass: `mov DWORD PTR [mem], imm32`.
                * Encoded `c7 04 24 imm32`              (no disp,        7 bytes)
                *         `c7 44 24 disp8 imm32`        (1-byte disp,    8 bytes)
                *         `c7 84 24 disp32 imm32`       (32-byte disp,  11 bytes)
                *         `c7 05 disp32 imm32`          (rip-relative,  10 bytes)
                * i386 compilers use the rsp forms to push call-arg slots
                * and the rip form to store a pointer into a global; the
                * imm32 is frequently a pointer that no longer lives at
                * the original vmaddr after our transform.
                */
               for (size_t k = 0; k < sz; ++k) {
                  if (p[k] != 0xC7) continue;
                  /* /0 in ModR/M reg field (bits 3..5 == 000). */
                  if (k + 1 >= sz || (p[k + 1] & 0x38) != 0x00) continue;
                  uint8_t mod = (p[k + 1] >> 6) & 0x3;
                  uint8_t rm  = p[k + 1] & 0x7;
                  size_t imm_off = 0;
                  size_t inst_len = 0;
                  if (rm == 0x4) {
                     /* SIB follows. We only handle base=rsp, index=none
                      * (SIB=0x24) which is what compilers use for the
                      * arg-pushing pattern. */
                     if (k + 2 >= sz || p[k + 2] != 0x24) continue;
                     if (mod == 0x0)      { imm_off = k + 3;  inst_len = 7;  }
                     else if (mod == 0x1) { imm_off = k + 4;  inst_len = 8;  }
                     else if (mod == 0x2) { imm_off = k + 7;  inst_len = 11; }
                     else continue;
                  } else if (rm == 0x5) {
                     /*
                      * rm=101 covers three encodings, distinguished by
                      * mod:
                      *   mod=00  `mov [rip+disp32], imm32`   (10 bytes)
                      *   mod=01  `mov [rbp+disp8],  imm32`   ( 7 bytes)
                      *   mod=10  `mov [rbp+disp32], imm32`   (10 bytes)
                      * The displacement is slide-invariant in every
                      * case; only the trailing imm32 is patched. i386
                      * compilers use the rbp forms to stage tail-call
                      * args (pointer literals among them).
                      */
                     if (mod == 0x0)      { imm_off = k + 6;  inst_len = 10; }
                     else if (mod == 0x1) { imm_off = k + 3;  inst_len = 7;  }
                     else if (mod == 0x2) { imm_off = k + 6;  inst_len = 10; }
                     else continue;
                  } else continue;
                  if (imm_off + 4 > sz) continue;
                  uint32_t v;
                  memcpy(&v, p + imm_off, sizeof v);
                  if (v >= dylib_vmaddr_lo
                      && v <  dylib_vmaddr_hi) {
                     uint32_t patched_val = (uint32_t)((uintptr_t)v + slide);
                     memcpy(p + imm_off, &patched_val, sizeof patched_val);
                     ++patched;
                     k += inst_len - 1;
                  }
               }
            }

            if (targets[matched].needs_mprotect) {
               uintptr_t pg = addr & ~(uintptr_t)0xFFF;
               size_t pglen = ((addr + sz + 0xFFF) & ~(uintptr_t)0xFFF) - pg;
               if (mprotect((void *)pg, pglen, PROT_READ | PROT_EXEC) != 0) {
                  perror("wrapper: mprotect rx __TEXT,__const");
               }
            }

            if (getenv("WRAPPER_DEBUG")) {
               fprintf(stderr,
                       "wrapper: patched %zu slots in %s,%s of %s (slide=0x%lx)\n",
                       patched, seg->segname, sects[s].sectname, base, (long)slide);
            }
         }
      }
   }
}

/*
 * Carve a chunk out of a single low-4GB scratch page. Used by abiconv's
 * fopen shim to allocate new shim_FILE structs without going through
 * malloc (which on macOS returns high-memory addresses). Thread-safe to
 * the extent the translated code itself is single-threaded — photocd is.
 */
static char *low_scratch_cur = NULL;
static char *low_scratch_end = NULL;

void *_86x64_alloc_low_4gb(size_t n) {
   /* Lazy-allocate the scratch region on first call. 64 KB is plenty for
    * a few dozen shim_FILE structs from fopen. */
   if (low_scratch_cur == NULL) {
      const size_t pglen = 64 * 1024;
      mach_vm_address_t addr = 0;
      for (uintptr_t a = LOW_REGION_BASE; a + pglen <= LOW_REGION_END;
           a += pglen + 0x1000) {
         addr = a;
         kern_return_t kr = mach_vm_allocate(mach_task_self(), &addr,
                                             pglen, VM_FLAGS_FIXED);
         if (kr == KERN_SUCCESS) break;
         addr = 0;
      }
      if (addr == 0) return NULL;
      low_scratch_cur = (char *)(uintptr_t)addr;
      low_scratch_end = low_scratch_cur + pglen;
   }
   n = (n + 15) & ~(size_t)15;   /* 16-byte align */
   if (low_scratch_cur + n > low_scratch_end) return NULL;
   void *result = low_scratch_cur;
   low_scratch_cur += n;
   return result;
}

/*
 * Allocate the FILE shim region in low 4 GB. Returns the base of a page
 * whose contents are zeroed except for the three `struct shim_FILE`s at
 * offsets 0, sizeof(shim_FILE), 2*sizeof(shim_FILE). Aborts on failure
 * because nothing downstream works without these.
 */
static void allocate_shim_files(void) {
   if (_86x64_stderr_shim) return;

   const size_t need = sizeof(struct shim_FILE) * 3 + 64 /* slop */;
   const size_t pglen = (need + 0xFFF) & ~(size_t)0xFFF;
   void *page = NULL;
   for (uintptr_t a = LOW_REGION_BASE; a + pglen <= LOW_REGION_END;
        a += pglen + 0x1000) {
      mach_vm_address_t addr = a;
      kern_return_t kr = mach_vm_allocate(mach_task_self(), &addr,
                                          pglen, VM_FLAGS_FIXED);
      if (kr != KERN_SUCCESS) continue;
      page = (void *)(uintptr_t)addr;
      break;
   }
   if (!page) {
      fprintf(stderr, "wrapper: no low-4GB page available for FILE shims\n");
      abort();
   }

   memset(page, 0, pglen);
   _86x64_stderr_shim = (struct shim_FILE *)((char *)page + 0);
   _86x64_stdout_shim = (struct shim_FILE *)((char *)page + sizeof(struct shim_FILE));
   _86x64_stdin_shim  = (struct shim_FILE *)((char *)page + sizeof(struct shim_FILE) * 2);

   _86x64_stderr_shim->magic = SHIM_FILE_MAGIC;
   _86x64_stderr_shim->fd = 2;
   _86x64_stderr_shim->real_fp = stderr;

   _86x64_stdout_shim->magic = SHIM_FILE_MAGIC;
   _86x64_stdout_shim->fd = 1;
   _86x64_stdout_shim->real_fp = stdout;

   _86x64_stdin_shim->magic = SHIM_FILE_MAGIC;
   _86x64_stdin_shim->fd = 0;
   _86x64_stdin_shim->real_fp = stdin;
}

/*
 * Walk a 64-bit Mach-O image's load commands looking for an indirect
 * (lazy or non-lazy) symbol pointer that resolves to `target_name`.
 * Returns a pointer to the slot in process memory (or NULL).
 *
 * Mach-O lays this out as:
 *   __DATA,__nl_symbol_ptr (or __la_symbol_ptr) — array of N pointers
 *   sect->reserved1 — index into LC_DYSYMTAB.indirectsymoff
 *   that table holds symbol-table indices; LC_SYMTAB names them.
 */
static uint64_t *find_symbol_ptr_slot(const struct mach_header_64 *mh,
                                      intptr_t slide,
                                      const char *target_name) {
   const uint8_t *cmd_ptr = (const uint8_t *)(mh + 1);
   const struct symtab_command *symtab = NULL;
   const struct dysymtab_command *dysymtab = NULL;
   const struct segment_command_64 *linkedit_seg = NULL;

   /* First pass: find LC_SYMTAB / LC_DYSYMTAB / __LINKEDIT. */
   const uint8_t *cp = cmd_ptr;
   for (uint32_t c = 0; c < mh->ncmds; ++c) {
      const struct load_command *lc = (const struct load_command *)cp;
      if (lc->cmd == LC_SYMTAB) {
         symtab = (const struct symtab_command *)lc;
      } else if (lc->cmd == LC_DYSYMTAB) {
         dysymtab = (const struct dysymtab_command *)lc;
      } else if (lc->cmd == LC_SEGMENT_64) {
         const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
         if (strcmp(seg->segname, "__LINKEDIT") == 0) {
            linkedit_seg = seg;
         }
      }
      cp += lc->cmdsize;
   }
   if (!symtab || !dysymtab || !linkedit_seg) return NULL;

   /* __LINKEDIT contains the string table, symbol table, and indirect
    * symbol table at file offsets we have to translate to runtime
    * addresses using the segment's mapping. */
   const uintptr_t linkedit_runtime =
      (uintptr_t)linkedit_seg->vmaddr + slide;
   const uintptr_t linkedit_fileoff = linkedit_seg->fileoff;
   const struct nlist_64 *symbols = (const struct nlist_64 *)
      (linkedit_runtime + (symtab->symoff - linkedit_fileoff));
   const char *strtab = (const char *)
      (linkedit_runtime + (symtab->stroff - linkedit_fileoff));
   const uint32_t *indirect = (const uint32_t *)
      (linkedit_runtime + (dysymtab->indirectsymoff - linkedit_fileoff));

   /* Second pass: walk __nl_symbol_ptr / __la_symbol_ptr sections and
    * check each slot against the indirect-symbol → nlist → strtab chain. */
   cp = cmd_ptr;
   for (uint32_t c = 0; c < mh->ncmds; ++c) {
      const struct load_command *lc = (const struct load_command *)cp;
      cp += lc->cmdsize;
      if (lc->cmd != LC_SEGMENT_64) continue;
      const struct segment_command_64 *seg = (const struct segment_command_64 *)lc;
      const struct section_64 *sects = (const struct section_64 *)(seg + 1);
      for (uint32_t s = 0; s < seg->nsects; ++s) {
         uint32_t flags = sects[s].flags & SECTION_TYPE;
         if (flags != S_NON_LAZY_SYMBOL_POINTERS
             && flags != S_LAZY_SYMBOL_POINTERS) continue;
         uint32_t nslots = sects[s].size / sizeof(uint64_t);
         uint64_t *slot = (uint64_t *)((uintptr_t)sects[s].addr + slide);
         for (uint32_t k = 0; k < nslots; ++k) {
            uint32_t sym_idx = indirect[sects[s].reserved1 + k];
            if (sym_idx == INDIRECT_SYMBOL_LOCAL
                || sym_idx == INDIRECT_SYMBOL_ABS
                || sym_idx == (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
               continue;
            }
            const char *name = strtab + symbols[sym_idx].n_un.n_strx;
            if (strcmp(name, target_name) == 0) {
               return &slot[k];
            }
         }
      }
   }
   return NULL;
}

/*
 * After dyld has bound the translated dylib's __nl_symbol_ptr entries to
 * libsystem's `__stderrp`/`__stdoutp`/`__stdinp` (which live in high
 * 4 GB), overwrite them with the low-4GB addresses of our shim FILE-ptr
 * variables. This is the only way the i386 code's `mov eax, [stderrp]`
 * can produce a usable 32-bit pointer.
 *
 * The slots in our converted dylib hold 8-byte values (it's a 64-bit
 * Mach-O). We write the full 64-bit address; the i386 code reads only
 * the low 4 bytes via `mov eax`, which is what we want — those low
 * bytes are the full 32-bit shim address.
 *
 * Run this *after* fixup_translated_dylib_slots so the data-section
 * scan doesn't double-patch the new pointer values.
 */
static void redirect_stdio_symbol_ptrs(const struct mach_header_64 *mh,
                                       intptr_t slide) {
   /* Per-symbol slot in low 4 GB whose VALUE is the shim FILE pointer.
    * The dyld slot we patch points HERE, and `mov eax, [eax]` reads
    * the shim FILE pointer out of HERE. */
   static uint32_t *stderrp_holder = NULL;
   static uint32_t *stdoutp_holder = NULL;
   static uint32_t *stdinp_holder  = NULL;

   if (!stderrp_holder) {
      /* Allocate one low-4GB page to hold the three FILE-pointer
       * variables. The shim_FILE structs themselves are already in
       * low-4GB memory via allocate_shim_files(). */
      mach_vm_address_t addr = LOW_REGION_BASE;
      for (uintptr_t a = LOW_REGION_BASE;
           a + 0x1000 <= LOW_REGION_END; a += 0x2000) {
         addr = a;
         kern_return_t kr = mach_vm_allocate(mach_task_self(), &addr,
                                             0x1000, VM_FLAGS_FIXED);
         if (kr == KERN_SUCCESS) break;
         addr = 0;
      }
      if (addr == 0) {
         fprintf(stderr, "wrapper: no low-4GB page for stdio holders\n");
         abort();
      }
      uint32_t *p = (uint32_t *)(uintptr_t)addr;
      stderrp_holder = &p[0];
      stdoutp_holder = &p[1];
      stdinp_holder  = &p[2];
      *stderrp_holder = (uint32_t)(uintptr_t)_86x64_stderr_shim;
      *stdoutp_holder = (uint32_t)(uintptr_t)_86x64_stdout_shim;
      *stdinp_holder  = (uint32_t)(uintptr_t)_86x64_stdin_shim;
   }

   struct { const char *name; uint32_t *holder; } map[] = {
      { "___stderrp", stderrp_holder },
      { "___stdoutp", stdoutp_holder },
      { "___stdinp",  stdinp_holder  },
   };
   const char *dbg = getenv("WRAPPER_DEBUG");
   for (int i = 0; i < 3; ++i) {
      uint64_t *slot = find_symbol_ptr_slot(mh, slide, map[i].name);
      if (!slot) {
         if (dbg) fprintf(stderr, "wrapper: %s not found\n", map[i].name);
         continue;
      }
      uintptr_t holder_addr = (uintptr_t)map[i].holder;
      *slot = holder_addr;
      if (dbg) {
         fprintf(stderr, "wrapper: %s slot at %p -> 0x%lx (shim @ 0x%lx)\n",
                 map[i].name, (void *)slot, (long)holder_addr,
                 (long)(uintptr_t)*map[i].holder);
      }
   }
}

/* Exported with a leading underscore so the asm wrapper can `extern` it. */
uint32_t build_i386_main_frame(int64_t argc, char **argv);

uint32_t build_i386_main_frame(int64_t argc, char **argv) {
   const char *dbg = getenv("WRAPPER_DEBUG");
   if (argc < 0 || argc > 4096) {
      fprintf(stderr, "wrapper: refusing absurd argc %lld\n", (long long)argc);
      abort();
   }

   /* Set up shim FILE structs in low 4 GB and apply ASLR slide to
    * internal pointers in the translated dylib's __data / __TEXT,__const
    * / __text sections. Both must run before we hand control off to
    * translated code, since most of those slots are dereferenced almost
    * immediately by the i386 startup sequence. */
   allocate_shim_files();
   fixup_translated_dylib_slots();
   if (dbg) {
      fprintf(stderr, "wrapper: argc=%lld argv=%p\n", (long long)argc, (void *)argv);
      for (int64_t i = 0; i < argc; ++i) {
         fprintf(stderr, "wrapper:   argv[%lld]=%p \"%s\"\n",
                 (long long)i, (void *)argv[i], argv[i]);
      }
   }

   void *region = mmap(NULL, STACK_REGION_SIZE,
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANON, -1, 0);
   if (region == MAP_FAILED) {
      perror("wrapper: mmap stack region");
      abort();
   }
   uintptr_t base = (uintptr_t)region;
   if (base + STACK_REGION_SIZE > 0xFFFFFFFFULL) {
      fprintf(stderr,
              "wrapper: mmap returned a high-memory region (%p); the "
              "interpose layer should have forced low-4GB. Aborting.\n",
              region);
      abort();
   }

   /* Strings packed near the top of the region. */
   char *strings_top = (char *)region + STACK_REGION_SIZE;
   char *strings_cursor = strings_top;
   uint32_t *argv32 = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)(argc + 1));
   if (!argv32) {
      perror("wrapper: malloc");
      abort();
   }
   for (int64_t i = 0; i < argc; ++i) {
      size_t len = strlen(argv[i]) + 1;
      strings_cursor -= len;
      memcpy(strings_cursor, argv[i], len);
      argv32[i] = (uint32_t)(uintptr_t)strings_cursor;
   }
   argv32[argc] = 0;

   /*
    * Lay out:
    *   [esp ...] argc, argv[0..argc-1], 0 (argv terminator),
    *             0 (envp terminator), 0 (apple terminator)
    *
    * Place the frame in the bottom of the region, well below the strings,
    * 16-byte aligned. We need (argc + 4) 4-byte words. Round up.
    */
   size_t frame_words = (size_t)argc + 4;        /* argc + argv + 3 terminators */
   size_t frame_bytes = ((frame_words * 4) + 15) & ~(size_t)15;
   uintptr_t frame_top = base + ARG_FRAME_SIZE;  /* leave the upper part for strings/scratch */
   uintptr_t frame_bottom = (frame_top - frame_bytes) & ~(uintptr_t)15;

   uint32_t *frame = (uint32_t *)frame_bottom;
   frame[0] = (uint32_t)argc;
   for (int64_t i = 0; i <= argc; ++i) {       /* includes the NULL */
      frame[1 + i] = argv32[i];
   }
   frame[1 + argc + 1] = 0;   /* envp terminator (empty env) */
   frame[1 + argc + 2] = 0;   /* apple terminator */

   free(argv32);

   if (dbg) {
      fprintf(stderr, "wrapper: frame_bottom=0x%llx (returning esp)\n",
              (unsigned long long)frame_bottom);
      fprintf(stderr, "wrapper: frame contents (12 dwords):\n");
      for (int i = 0; i < 12; ++i) {
         fprintf(stderr, "  [esp+%2d] = 0x%08x\n", i * 4, frame[i]);
      }
      fflush(stderr);
   }

   return (uint32_t)frame_bottom;
}
