#include <typeinfo>
#include <cassert>
#include <sstream>
#include <string>
#include <iterator>
#include <set>
#include <map>
#include <utility>
#include <unordered_map>
#include <unordered_set>

#include "section.hh"
#include "segment.hh"
#include "parse.hh"
#include "build.hh"
#include "transform.hh"
#include "section_blob.hh" // LazySymbolPointer
#include "instruction.hh"
#include "stub_helper.hh"
#include "archive.hh"
#include "segment.hh"

namespace MachO {

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      section_t<bits> sect = img.at<section_t<bits>>(offset);
      uint32_t flags = sect.flags;
      
      /* check whether contains instructions.
       *
       * __StaticInit is the legacy i386 ObjC section name for C++ static
       * initializers (GCC/Clang emitted this through ~10.6). Modern x86_64
       * compilers merge static-init code into __text. Without translation
       * its i386 bytes (`89 e5` mov ebp, esp; `c9` leave) pass through as
       * x86_64 instructions that zero-extend the stack pointer, corrupting
       * rbp/rsp. Observed on iWeb's SFControls HUDView.mm static init —
       * fault at first `leave` reading the truncated rsp. Same fix applies
       * to __textcoal_nt / __coalesced (legacy template-instantiation
       * code sections) for completeness, though we haven't seen those hit
       * the stack-reg pattern yet.
       *
       * PURE_INSTRUCTIONS attribute (S_ATTR_PURE_INSTRUCTIONS) could be
       * used as a fallback signal but isn't reliable: __StaticInit on i386
       * doesn't always set it. Whitelist by section name. */
      std::vector<std::string> text_sectnames = {
         SECT_TEXT, SECT_STUBS, SECT_SYMBOL_STUB,
         "__StaticInit", "__textcoal_nt", "__coalesced",
      };
      for (const std::string& sectname : text_sectnames) {
         if (sectname == sect.sectname) {
            return new Section<bits>(img, offset, env, TextParser);
         }
      }

      if (std::string(sect.sectname) == SECT_STUB_HELPER) {
         return new Section<bits>(img, offset, env, StubHelperParser);
      }
      
      /*
       * Normalise: test by SECTION_TYPE (low 8 bits), not the full flags
       * field, so attribute bits (PURE_INSTRUCTIONS, NO_TOC, etc.) don't
       * break section-type matching. Section ATTRIBUTES are orthogonal
       * to section TYPE.
       */
      const uint32_t stype = flags & SECTION_TYPE;
      if (stype == S_LAZY_SYMBOL_POINTERS) {
         return new Section<bits>(img, offset, env, LazySymbolPointer<bits>::Parse);
      } else if (stype == S_NON_LAZY_SYMBOL_POINTERS) {
         return new Section<bits>(img, offset, env, NonLazySymbolPointer<bits>::Parse);
      } else if (stype == S_CSTRING_LITERALS) {
         return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
      } else if (stype == S_MOD_INIT_FUNC_POINTERS ||
                 stype == S_MOD_TERM_FUNC_POINTERS) {
         /*
          * Arrays of function pointers run by dyld at module init /
          * teardown. Use NonLazySymbolPointer (not DataParser+Immediate)
          * so each slot is ptr_t<bits>: 4 bytes in M32 input, 8 bytes in
          * M64 output. DataParser would keep 4-byte slots in M64 and dyld
          * would reject the section ("size N is not a multiple of pointer
          * size" + at-call-time the upper 4 bytes of each pointer would
          * be garbage). NonLazySymbolPointer also pointer-detects + rebases
          * the value, so __text functions still resolve through transform.
          */
         return new Section<bits>(img, offset, env, NonLazySymbolPointer<bits>::Parse);
      } else if (stype == S_REGULAR) {
         /*
          * Regular data section. Use DataParser, which detects internal
          * pointers (4-byte aligned values that fall inside one of the
          * binary's segments) and tracks them as Immediate blobs. Without
          * this, hardcoded absolute pointers like `extern char *gptr =
          * &common_var;` baked into __data by the original i386 linker
          * become stale when our i386→x86_64 transform shifts segments.
          *
          * Apply to all __DATA sections AND to __TEXT,__const. The latter
          * is where i386 compilers stash function-pointer jump tables
          * (e.g. switch-statement dispatch arrays); without rewriting their
          * entries to new __text vmaddrs the first indirect jmp crashes.
          * __TEXT,__cstring is handled as S_CSTRING_LITERALS above so it
          * doesn't hit this branch.
          */
         const std::string segname(
            sect.segname, strnlen(sect.segname, sizeof(sect.segname)));
         const std::string sectname(
            sect.sectname, strnlen(sect.sectname, sizeof(sect.sectname)));
         const bool is_data = segname.rfind("__DATA", 0) == 0;
         /*
          * __DATA,__cfstring is an array of i386 CFConstantString records
          * {isa:4, flags:4, str:4, length:4}. Parse each 16-byte record as a
          * single CFStringBlob so the transform can EXPAND it to the x86_64
          * 32-byte layout {isa:8, flags:8, str:8, length:8} that real
          * CoreFoundation reads (DataParser would keep 4-byte fields and CF
          * would fault on str@16/length@24). Only when the section is a clean
          * multiple of the record size. */
         if (is_data && sectname == "__cfstring" &&
             (sect.size % CFStringBlob<bits>::record_size) == 0) {
            return new Section<bits>(img, offset, env, CFStringBlob<bits>::Parse);
         }
         const bool is_text_const = segname == SEG_TEXT && sectname == "__const";
         /*
          * Legacy i386 ObjC ABI (__OBJC segment): __message_refs / __cls_refs
          * are arrays of 4-byte pointers into __cstring (selector and class
          * names); __module_info entries carry name/symtab pointers. These
          * must be pointer-detected and rebased like __DATA — DataBlob would
          * leave them as stale i386 vmaddrs. Slots stay 4-byte: the translated
          * instructions index __OBJC with the original 4-byte stride.
          */
         const bool is_objc = segname == SEG_OBJC;
         if (is_data || is_text_const || is_objc) {
            return new Section<bits>(img, offset, env, DataParser);
         }
         return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
      } else if (stype == S_LITERAL_POINTERS) {
         /*
          * Legacy ObjC __message_refs / __cls_refs are S_LITERAL_POINTERS:
          * arrays of 4-byte pointers into __cstring. DataParser
          * pointer-detects and rebases the 4-byte slots.
          */
         return new Section<bits>(img, offset, env, DataParser);
      } else if (stype == S_ZEROFILL || stype == S_GB_ZEROFILL ||
                 stype == S_THREAD_LOCAL_ZEROFILL) {
         return new Section<bits>(img, offset, env, ZeroBlob<bits>::Parse);
      }

      /*
       * Unknown / unrecognised section type. Fall back to byte-at-a-time
       * DataBlob parse — preserves the bytes verbatim through transform.
       * Was: throw "bad section flags". This catches S_COALESCED (0xb,
       * iPhoto's __eh_frame), S_INTERPOSING, S_DTRACE_DOF, etc., which
       * we don't have explicit handling for but whose bytes should
       * survive the M32→M64 trip even without pointer-aware rebasing.
       *
       * Hits with a one-line warning if MACHO_BUILD_DEBUG is set so we
       * notice section types we may want to handle smarter.
       */
      if (std::getenv("MACHO_BUILD_DEBUG")) {
         fprintf(stderr, "section %.16s flags=0x%x stype=0x%x — "
                 "falling back to byte DataBlob parse\n",
                 sect.sectname, (unsigned)flags, (unsigned)stype);
      }
      return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
   }

   template <Bits bits>
   SectionBlob<bits> *Section<bits>::DataParser(const Image& img, const Location& loc,
                                                ParseEnv<bits>& env) {
      /*
       * For 4-byte aligned positions, read 4 bytes and emit them as an
       * Immediate blob (either pointer or plain). Doing this consistently
       * for every aligned slot — instead of falling back to 1-byte
       * DataBlobs when the value isn't pointer-shaped — keeps subsequent
       * iterations aligned, so placeholders that reference later __data
       * addresses always find a 4-byte-aligned blob to attach to.
       *
       * Unaligned positions (e.g. the trailing tail of a section whose
       * size isn't a multiple of 4) fall back to byte-at-a-time DataBlob.
       */
      if ((loc.vmaddr & 3) != 0 || loc.offset + 4 > img.size()) {
         return DataBlob<bits>::Parse(img, loc, env);
      }
      /*
       * Don't emit a 4-byte Immediate that would spill past the end of
       * the section being parsed. Sections whose size isn't a multiple
       * of 4 (e.g. __gcc_except_tab, 69 bytes) have a sub-4-byte tail;
       * a 4-byte read there oversteps `end` in Section::Parse1, whose
       * `it != end` loop would then never terminate and walk off the
       * mapped image.
       */
      if (env.current_section != nullptr) {
         const auto& cs = env.current_section->sect;
         if (loc.offset + 4 > cs.offset + cs.size) {
            return DataBlob<bits>::Parse(img, loc, env);
         }
      }

      const uint32_t value = img.at<uint32_t>(loc.offset);

      /*
       * Legacy ObjC __OBJC,__symbols holds the per-module objc_symtab structs
       * { sel_ref_cnt:4, refs:4, cls_def_cnt:2, cat_def_cnt:2, defs[]:4… }.
       * Its only pointers are `refs` (→ __OBJC,__message_refs) and the `defs[]`
       * array (→ __class / __category structs, also in __OBJC) — never code or
       * cstring pointers. But the packed `cls_def_cnt|cat_def_cnt` 32-bit count
       * word is a small integer that frequently aliases a __TEXT,__text vmaddr
       * (e.g. AppController's module: cls=1,cat=5 → 0x00050001, a valid code
       * address). The generic heuristic below would "rebase" that count word to
       * a translated __text address, corrupting cls_def_cnt/cat_def_cnt so the
       * runtime category/class walk reads garbage counts and silently drops the
       * module's categories. Since a real objc_symtab pointer NEVER targets an
       * executable segment, reject executable targets outright in this section.
       * Universal to every i386 ObjC binary (objc_symtab is part of the ABI).
       */
      bool in_objc_symbols = false;
      if (env.current_section != nullptr) {
         const auto& cs = env.current_section->sect;
         in_objc_symbols =
            strncmp(cs.segname, SEG_OBJC, sizeof(cs.segname)) == 0 &&
            strncmp(cs.sectname, "__symbols", sizeof(cs.sectname)) == 0;
      }

      bool is_pointer = false;
      /* Reject obvious non-pointers up-front so we don't waste resolver
       * traffic on integer constants or float bit patterns. */
      if (value >= 0x1000 && value < 0x80000000U) {
         for (Segment<bits> *seg : env.archive.segments()) {
            std::string name(seg->segment_command.segname,
                             strnlen(seg->segment_command.segname,
                                     sizeof(seg->segment_command.segname)));
            if (name == SEG_PAGEZERO || name == SEG_LINKEDIT) continue;
            if (seg->contains_vmaddr(value)) {
               /* Pointer-detection is a HEURISTIC: fixed-address i386 execs
                * carry no relocation/rebase info (rebase_size=0, nreloc=0), so
                * we can't know for sure which 4-byte __data words are pointers
                * vs integer/fixed-point constants. A constant that happens to
                * fall in a segment's vmaddr range would be falsely "rebased"
                * (+segment delta) and corrupted. Discriminator: a real pointer
                * into a NON-executable (data) segment points at an ALIGNED
                * global, so a misaligned value landing in data range is almost
                * certainly a constant, not a pointer. (Observed: photocd's YCC
                * green coefficient 199313=0x30a91 lands in __DATA,__bss and was
                * falsely rebased +0x10004108 -> 0x10034b99, blowing up the
                * Cb->Green table so the whole green channel clamped to 0xFF.)
                * Code pointers (into __TEXT) CAN be unaligned (function entries
                * aren't 4-aligned), so only gate non-executable targets. */
               const bool exec =
                  (seg->segment_command.initprot & VM_PROT_EXECUTE) != 0;
               if (!exec && (value & 3) != 0) {
                  break;   /* misaligned data-range value -> treat as constant */
               }
               if (exec && in_objc_symbols) {
                  break;   /* objc_symtab count word aliasing __text -> constant */
               }
               is_pointer = true;
               break;
            }
         }
      }
      return Immediate<bits>::Parse(img, loc, env, is_pointer);
   }

   template <Bits bits>
   SectionBlob<bits> *Section<bits>::TextParser(const Image& img, const Location& loc,
                                                    ParseEnv<bits>& env) {
      /*
       * Pick instruction vs data:
       *   1. LC_DATA_IN_CODE entries are authoritative — they say "this range
       *      is jump-table / nop padding / etc."
       *   2. Otherwise we'd like to decode as an instruction, but linear-sweep
       *      disassembly slips on data interleaved with code (release builds
       *      from circa 2010 emit unmarked jump tables inside __text). When
       *      xed cannot decode the bytes, fall back to a single-byte DataBlob
       *      and let the section's parse loop advance one byte and retry.
       *      That gradually re-syncs once we walk past the data region.
       */
      if (env.data_in_code.contains(loc.offset)) {
         return DataBlob<bits>::Parse(img, loc, env);
      }
      if (!Instruction<bits>::CanDecode(img, loc)) {
         return DataBlob<bits>::Parse(img, loc, env);
      }
      return Instruction<bits>::Parse(img, loc, env);
   }


   template <Bits bits>
   Section<bits>:: ~Section() {
      for (SectionBlob<bits> *elem : content) {
         delete elem;
      }
   }
   
   template <Bits bits>
   void Section<bits>::Parse1(const Image& img, ParseEnv<bits>& env) {
      env.current_section = this;

      /*
       * Progress: per-section banner so the long Section::Parse1 loop on
       * a 17 MB binary is not silent. Cheap (one line per section).
       */
      fprintf(stderr, "parse: %.16s,%.16s vmaddr=0x%zx size=%zu\n",
              sect.segname, sect.sectname,
              (size_t)sect.addr, (size_t)sect.size);

      const std::size_t begin = sect.offset;
      const std::size_t end = begin + sect.size;
      std::size_t it = begin;
      std::size_t vmaddr = sect.addr;
      while (it != end) {
         SectionBlob<bits> *elem = parser(img, Location(it, vmaddr), env);
         elem->iter = content.insert(content.end(), elem);
         const std::size_t step = elem->size();
         if (step == 0 || it + step > end) {
            throw error("section %s: blob at offset 0x%zx (size %zu) "
                        "overruns section end 0x%zx",
                        name().c_str(), it, step, end);
         }
         it += step;
         vmaddr += step;
      }

      DetectPicAnchoredDisps(env);

      env.current_section = nullptr;
   }

   /*
    * Detect i386 PIC anchor patterns and rewrite anchored `[reg+disp32]`
    * reads/writes so subsequent transform can emit them as rip-relative.
    *
    * Pattern recognized:
    *   e8 00 00 00 00         ; call $+0 (CALL_NEAR_RELBRz with brdisp=0)
    *   58+r                   ; pop %reg
    *   ... <reg used as `[reg + disp32]` base> ...
    *
    * After `pop %reg`, %reg holds the vmaddr of the pop instruction
    * itself (= the call's return address). Subsequent loads/stores of
    * the form `op disp(%reg)` access `[anchor_vmaddr + disp]`. The
    * compiler computed `disp = target_vmaddr - anchor_vmaddr` at link
    * time for the i386 layout, but the M32→M64 transform reshapes the
    * layout, so the disp is wrong in the translated binary.
    *
    * Fix: at parse time, walk the linear instruction stream, track which
    * register holds which anchor vmaddr, and for each anchored memory
    * operand: resolve `anchor + disp` to a SectionBlob and stash it as
    * `memdisp` with `pic_anchored=true`. The transform then emits the
    * load as `mov target(%rip), %reg` (rip-relative), bypassing the
    * (still-correct-low-32, but unused) base register entirely.
    *
    * Linear walk + function-scoped tracking. A linear byte-order walk
    * doesn't follow branches — but real PIC patterns set the anchor in
    * a function prologue and use it from multiple basic blocks reached
    * via intra-procedural branches. The function-exit restore (e.g.
    * `mov %ebx, -0xc(%rbp)` followed by `leave; ret`) lies AFTER the
    * pop in byte order but BEFORE the slow-path anchored reads (which
    * are jumped over by a fast-path `je`). Clearing the anchor on the
    * restore would disable the rewrite for the slow path.
    *
    * So: an anchor lives until RET (function boundary). Within a
    * function, register reuse for non-anchor purposes (`pop %reg;
    * ...; mov $42, %reg; mov disp(%reg)`) would mis-rewrite the
    * second read, but this pattern is essentially unheard of in
    * compiled PIC code (the compiler picks ONE register as the PIC
    * anchor and keeps it for the whole function).
    *
    *   - Clear ALL anchors on RET (function exit) and on
    *     INTERRUPT / SYSCALL / SYSRET.
    *   - On CALL: clear caller-saved regs only (EAX, ECX, EDX per i386
    *     System V ABI). Callee-saved EBX / ESI / EDI / EBP survive.
    *   - Do NOT clear on COND_BR / UNCOND_BR — intra-procedural.
    *   - Do NOT clear on REG0 writes — function-exit restores are
    *     reached lexically BEFORE the slow-path reads in the byte
    *     stream. Anchor survives until RET.
    *   - Anchored read is rewritten BEFORE the same instruction's
    *     anchor-establishing pop is recorded (so a self-aliased
    *     `mov disp(%reg)` at the pop's vmaddr — won't happen in
    *     practice — would be left alone).
    *   - SIB-indexed forms (`[reg + idx*N + disp32]`) are NOT rewritten;
    *     the transform would also have to fold the index. Rare in PIC
    *     access patterns. Leaving them unrewritten preserves the
    *     pre-existing (broken) behavior, which is no worse than today.
    *
    * Triggers only for M32 sections; M64 is a no-op (no i386 PIC).
    */
   template <Bits bits>
   void Section<bits>::DetectPicAnchoredDisps(ParseEnv<bits>& env) {
      if constexpr (bits != Bits::M32) {
         return;
      }
      if (parser != TextParser) {
         return;
      }

      /* reg → anchor vmaddr (= vmaddr of the pop instruction that loaded the
       * anchor). Only EAX..EDI tracked. */
      std::unordered_map<xed_reg_enum_t, std::size_t> anchors;

      /* frame-slot displacement → anchor vmaddr. Compilers spill the PIC
       * register to the stack (`mov %esi, -0x2c(%ebp)`) and later reload it
       * into a DIFFERENT register (`mov -0x2c(%ebp), %eax; mov 0x2d6(%eax),..`)
       * — common once %esi is needed for something else. Track the anchor
       * through the spill slot so the reloaded register is recognized as an
       * anchor too. Keyed by (base_reg, disp) of the frame slot. */
      std::map<std::pair<xed_reg_enum_t, ssize_t>, std::size_t> anchor_slots;

      /* Forward branch targets we've seen but not yet reached in the
       * linear walk. Used to recognize "mid-function" RETs: if there's
       * pending forward-branch flow that hasn't been resolved by visiting
       * the target, we're still inside the function and shouldn't clear
       * anchors at this RET. Models the Tessera/cxa_guard pattern where
       * the fast path returns early (RET in middle of byte stream) and
       * the slow path lives at higher addresses, reached via the
       * fast-path JE around it. */
      std::set<std::size_t> pending_forward_targets;

      Instruction<bits> *prev_inst = nullptr;
      for (SectionBlob<bits> *blob : content) {
         auto *inst = dynamic_cast<Instruction<bits> *>(blob);
         if (!inst) {
            /* Data interleaved in __text — clear tracking, anchors
             * almost certainly don't survive past it. */
            anchors.clear();
            anchor_slots.clear();
            prev_inst = nullptr;
            continue;
         }

         const xed_decoded_inst_t& xedd = inst->xedd;
         const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);

         /* (1) Detect the anchor: pop %reg immediately after `call $+0`.
          *     The call sets a candidate; the pop completes the pattern. */
         bool is_anchor_pop = false;
         if (iform == XED_IFORM_POP_GPRv_58 && prev_inst &&
             prev_inst->instbuf.size() == 5 &&
             prev_inst->instbuf[0] == 0xe8 &&
             prev_inst->instbuf[1] == 0x00 &&
             prev_inst->instbuf[2] == 0x00 &&
             prev_inst->instbuf[3] == 0x00 &&
             prev_inst->instbuf[4] == 0x00) {
            const xed_reg_enum_t reg =
               xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            if (reg >= XED_REG_EAX && reg <= XED_REG_EDI) {
               /* A fresh `call $+0; pop %reg` establishes a NEW PIC context.
                * Any spill-slot entries recorded BEFORE this pop are bogus:
                * you cannot spill an anchor before you pop it, so a prior
                * `mov %reg, slot` was a callee-save register save that only
                * looked like an anchor spill because a STALE anchor (leaked
                * from a previous function whose RET didn't clear tracking)
                * lingered in anchors[%reg]. If left in anchor_slots, this
                * function's epilogue restore `mov slot, %reg` would be
                * misread as an anchor reload and corrupt the anchor for any
                * slow-path code reached by a branch that bypasses the
                * epilogue. Clearing here scopes anchor spills to post-pop.
                * (Real post-pop spills are recorded after this point, so
                * the documented spill/reload-into-another-reg case is
                * unaffected.) */
               anchor_slots.clear();
               anchors[reg] = inst->loc.vmaddr;
               is_anchor_pop = true;
            }
         }

         /* (2) Rewrite anchored reads/writes BEFORE handling writes
          *     (so `mov %ebx, disp(%ebx)` still transforms its load).
          *     Override an ABSOLUTE memdisp too: instruction.cc's
          *     `[base+disp32]` absolute-table path (parse, runs before this)
          *     can't know `base` is a PIC anchor, so it eagerly resolves the
          *     RAW disp as an absolute vmaddr (memdisp_absolute=true, often a
          *     deferred resolve into &memdisp). When we recognise `base` as an
          *     anchor here, anchor+disp is authoritative — take over and cancel
          *     the competing resolve below so do_resolve() can't clobber the
          *     placeholder. A non-absolute, non-null memdisp (only EIP-relative
          *     in M32, which doesn't occur) is left untouched. */
         if (!anchors.empty() &&
             (inst->memdisp == nullptr || inst->memdisp_absolute)) {
            const xed_operand_values_t* ops =
               xed_decoded_inst_operands_const(&xedd);
            const unsigned nops = xed_decoded_inst_noperands(&xedd);
            for (unsigned i = 0; i < nops; ++i) {
               const xed_reg_enum_t basereg =
                  xed_decoded_inst_get_base_reg(ops, i);
               const xed_reg_enum_t indexreg =
                  xed_decoded_inst_get_index_reg(ops, i);
               if (basereg < XED_REG_EAX || basereg > XED_REG_EDI) continue;
               if (indexreg != XED_REG_INVALID) continue;
               const unsigned dwidth =
                  xed_decoded_inst_get_memory_displacement_width(ops, i);
               if (dwidth != sizeof(uint32_t)) continue;
               auto anchor_it = anchors.find(basereg);
               if (anchor_it == anchors.end()) continue;

               const ssize_t disp =
                  xed_decoded_inst_get_memory_displacement(ops, i);
               const std::size_t target = anchor_it->second + disp;
               if (std::getenv("MACHO_TRACE_ANCHOR")) {
                  fprintf(stderr, "[anchor] inst=0x%zx base=%s anchor=0x%zx disp=0x%zx target=0x%zx iform=%s\n",
                          (size_t)inst->loc.vmaddr,
                          xed_reg_enum_t2str(basereg),
                          (size_t)anchor_it->second, (size_t)disp, (size_t)target,
                          xed_iform_enum_t2str(iform));
               }
               SectionBlob<bits> *target_blob = env.add_placeholder(target);
               if (target_blob == nullptr) continue;

               /* Cancel the absolute-table path's deferred resolve(raw_disp)
                * aimed at this memdisp, or it fires in do_resolve() and
                * overwrites target_blob with the raw-disp blob (__eh_frame
                * garbage). No-op if the resolve already fired (no todo entry);
                * the assignment below then wins outright. */
               env.vmaddr_resolver.cancel(
                  (std::size_t)disp,
                  (const SectionBlob<bits> **)&inst->memdisp);

               inst->memidx = i;
               inst->memdisp = target_blob;
               inst->pic_anchored = true;
               /* pic_anchored implies neither absolute nor existing
                * rip-relative — transform consumes the flag and
                * synthesises a new rip-relative encoding. */
               inst->memdisp_absolute = false;
               break;
            }
         }

         /* (2b) Track the anchor through a frame-slot spill/reload so a
          *      reloaded copy in another register is also recognized.
          *        spill:  mov %anchor_reg, disp(%ebp|%esp)
          *        reload: mov disp(%ebp|%esp), %reg   => %reg is an anchor
          *      Frame base must be EBP/ESP with no index. Works for disp8 or
          *      disp32 (the disp width the existing read-rewrite requires does
          *      not apply here — spills use disp8). */
         {
            const xed_operand_values_t* ops2 =
               xed_decoded_inst_operands_const(&xedd);
            const xed_reg_enum_t mbase = xed_decoded_inst_get_base_reg(ops2, 0);
            const xed_reg_enum_t midx  = xed_decoded_inst_get_index_reg(ops2, 0);
            if ((mbase == XED_REG_EBP || mbase == XED_REG_ESP) &&
                midx == XED_REG_INVALID &&
                xed_decoded_inst_number_of_memory_operands(&xedd) == 1) {
               const ssize_t sdisp =
                  xed_decoded_inst_get_memory_displacement(ops2, 0);
               const auto slot = std::make_pair(mbase, sdisp);
               if (iform == XED_IFORM_MOV_MEMv_GPRv) {
                  /* store reg -> slot */
                  const xed_reg_enum_t src =
                     xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
                  auto a = anchors.find(src);
                  if (a != anchors.end()) {
                     anchor_slots[slot] = a->second;   /* slot now holds anchor */
                  } else {
                     anchor_slots.erase(slot);          /* overwritten -> not anchor */
                  }
               } else if (iform == XED_IFORM_MOV_GPRv_MEMv) {
                  /* load slot -> reg */
                  auto s = anchor_slots.find(slot);
                  const xed_reg_enum_t dst =
                     xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
                  if (s != anchor_slots.end() &&
                      dst >= XED_REG_EAX && dst <= XED_REG_EDI) {
                     anchors[dst] = s->second;
                  }
               }
            }
         }

         /* (3) Skipped: REG0-write clearing. Linear walk reaches the
          *     function-exit register restore (`mov %ebx, -0xc(%rbp)`)
          *     BEFORE the slow-path anchored reads (which sit at higher
          *     addresses but are reached via a fast/slow-path branch).
          *     Clearing on the restore would disable the rewrite for the
          *     slow path. Anchor instead lives until RET. See header
          *     comment for the tradeoff. */
         (void)is_anchor_pop;

         /* (4) Prune every pending forward-branch target we have now reached
          *     OR walked past. Exact-match-only retention (erase the current
          *     vmaddr alone) let a single target that the linear walk never
          *     lands on EXACTLY — a misdecoded branch, or a target that falls
          *     mid-instruction relative to the linear decode — stay pending
          *     forever. With it pending, the RET-clear guard below
          *     (`pending_forward_targets.empty()`) never fires, so the PIC
          *     anchor leaks across every subsequent function in the section
          *     (observed: anchor 0x367b1 leaking ~25 KB → 7251 stores resolved
          *     into read-only __text → iPhoto write-fault). Pruning <= current
          *     bounds the anchor to the real function extent. The legitimate
          *     fast-path-RET/slow-path case is unaffected: its slow-path target
          *     is AHEAD of the early RET (> current vmaddr), so it is not
          *     pruned and still keeps the anchor alive for the slow path. */
         pending_forward_targets.erase(
            pending_forward_targets.begin(),
            pending_forward_targets.upper_bound(inst->loc.vmaddr));

         /* (5) Record forward branches so a subsequent mid-function RET
          *     can know there's still reachable code ahead. Only targets that
          *     land within THIS section count: a misdecoded data byte parsed as
          *     a branch yields a garbage far target that the walk would never
          *     reach, which would (again) poison RET-clearing permanently.
          *
          *     CALL is intentionally EXCLUDED: a call transfers to ANOTHER
          *     function and returns, so its (often far-forward) target is not
          *     part of the current function's extent. Tracking call targets
          *     kept pending_forward_targets non-empty across every intervening
          *     RET, so the PIC anchor never cleared and leaked across the rest
          *     of the section (anchor 0x367b1 → ~25 KB → 7251 stores into
          *     read-only __text → iPhoto write-fault). Only intra-procedural
          *     conditional/unconditional branches extend a function. */
         const xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
         if (cat == XED_CATEGORY_COND_BR ||
             cat == XED_CATEGORY_UNCOND_BR) {
            const ssize_t brdisp =
               xed_decoded_inst_get_branch_displacement(&xedd);
            if (brdisp > 0) {
               const std::size_t after =
                  inst->loc.vmaddr + xed_decoded_inst_get_length(&xedd);
               const std::size_t tgt = after + brdisp;
               const std::size_t sect_end = sect.addr + sect.size;
               /* Only a branch landing within a PLAUSIBLE function span counts
                * as intra-function flow that keeps a RET from being a real
                * function boundary. A forward `jmp`/`jcc` whose target is far
                * ahead is a tail call or cross-function transfer (observed:
                * iPhoto's `jmp 0xc91fc0` tail calls, megabytes away). Tracking
                * those kept pending_forward_targets non-empty across every
                * intervening RET, so the PIC anchor never cleared and leaked
                * across the whole section (anchor 0x367b1 → entire binary →
                * 7251 normal [reg+disp] accesses mis-rewritten into read-only
                * __text → iPhoto write-fault). 64 KB comfortably covers any
                * real function's internal branches. */
               static const std::size_t MAX_FUNC_SPAN = 0x10000;
               if (tgt > inst->loc.vmaddr && tgt < sect_end &&
                   tgt - inst->loc.vmaddr <= MAX_FUNC_SPAN) {
                  pending_forward_targets.insert(tgt);
               }
            }
         }

         /* (6) Control transfers. Three classes of behavior:
          *     - RET: function exit ONLY IF no forward branch targets
          *       are pending. Otherwise this is a mid-function RET (the
          *       slow path follows, reached via the pending branch).
          *     - INTERRUPT / SYSCALL / SYSRET: system transition; clear.
          *     - CALL (not `call $+0`): the callee may clobber caller-
          *       saved regs (EAX/ECX/EDX in i386 sysv). Clear those;
          *       keep EBX/ESI/EDI/EBP (callee preserves them).
          *     - COND_BR / UNCOND_BR: intra-procedural, anchors survive. */
         const bool is_pic_call_zero =
            iform == XED_IFORM_CALL_NEAR_RELBRz &&
            xed_decoded_inst_get_branch_displacement(&xedd) == 0;
         if (cat == XED_CATEGORY_RET) {
            if (pending_forward_targets.empty()) {
               anchors.clear();
               anchor_slots.clear();
            }
         } else if (cat == XED_CATEGORY_INTERRUPT ||
                    cat == XED_CATEGORY_SYSCALL ||
                    cat == XED_CATEGORY_SYSRET) {
            anchors.clear();
            anchor_slots.clear();
         } else if (cat == XED_CATEGORY_CALL && !is_pic_call_zero) {
            anchors.erase(XED_REG_EAX);
            anchors.erase(XED_REG_ECX);
            anchors.erase(XED_REG_EDX);
         }

         prev_inst = inst;
      }
   }

   template <Bits bits>
   std::string Section<bits>::name() const {
      return std::string(sect.sectname, strnlen(sect.sectname, sizeof(sect.sectname)));
   }

   template <Bits bits>
   void Section<bits>::Build(BuildEnv<bits>& env) {
      env.align(sect.align);
      loc(env.loc);

      for (SectionBlob<bits> *elem : content) {
         elem->Build(env);
      }

      sect.size = env.loc.offset - loc().offset;

      Location relloc;
      sect.nreloc = relocs.size();
      env.allocate(sect.nreloc * RelocationInfo<bits>::size(), relloc);
      sect.reloff = relloc.offset;
   }

   template <Bits bits>
   void Section<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<section_t<bits>>(offset) = sect;

      std::size_t sect_offset = sect.offset;
      
      for (const SectionBlob<bits> *elem : content) {
         if (elem->active) {
            elem->Emit(img, sect_offset);
            sect_offset += elem->size();
         }
      }

      for (const RelocationInfo<bits> *reloc : relocs) {
         reloc->Emit(img, sect_offset, loc());
         sect_offset += RelocationInfo<bits>::size();
      }
   }

   template <Bits bits>
   void Section<bits>::Insert(const SectionLocation<bits>& loc, SectionBlob<bits> *blob) {
      SectionBlob<bits> *elem = dynamic_cast<SectionBlob<bits> *>(blob);
      if (elem == nullptr) {
         throw std::invalid_argument(std::string(__FUNCTION__) + ": blob is of incorrect type");
      }
      auto it = content.begin();
      std::advance(it, loc.index);
      content.insert(it, elem);
      // content.insert(content.begin() + loc.index, elem);
   }

   template <Bits bits>
   Section<bits>::Section(const Section<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      /* relocs(other.relocs.Transform(env)), */ id(other.id)
   {
      env.add(&other, this);
      env(other.sect, sect);
      env.resolve(other.segment, &segment);

      /* DBG_CONTENT: dump M32 content entries near the crash vmaddr 0x3fd4b6
       * right before transform, to see what object actually occupies that
       * slot in the content list (object A movb vs object B mov-edx). */
      if (std::getenv("DBG_CONTENT")) {
         for (const auto elem : other.content) {
            if (elem->loc.vmaddr >= 0x3fd4a0 && elem->loc.vmaddr <= 0x3fd4d0) {
               std::fprintf(stderr, "[content] obj=%p loc.offset=0x%zx loc.vmaddr=0x%zx",
                            (const void *)elem, (std::size_t)elem->loc.offset,
                            (std::size_t)elem->loc.vmaddr);
               auto *inst = dynamic_cast<const Instruction<opposite<bits>> *>(elem);
               if (inst) {
                  std::fprintf(stderr, " bytes=");
                  for (uint8_t b : inst->instbuf) std::fprintf(stderr, "%02x ", b);
               } else {
                  std::fprintf(stderr, " (not an Instruction: %s)", typeid(*elem).name());
               }
               std::fprintf(stderr, "\n");
            }
         }
      }

      /* transform content */
      for (const auto elem : other.content) {
         auto new_blobs = elem->Transform(env);
         if (!new_blobs.empty()) {
            env.add(elem, new_blobs.front());
         }
         content.splice(content.end(), new_blobs);
      }

      /* update alignment */
      const std::unordered_set<std::string> wordsize_align =
         {SECT_BSS, SECT_DATA, SECT_CONST};
      if (wordsize_align.find(name()) != wordsize_align.end()) {
         switch (bits) {
         case Bits::M32:
            sect.align = 2;
            break;
         case Bits::M64:
            sect.align = 3;
         }
      }
      /*
       * Pointer-array sections must be aligned to slot size. In M64 each entry
       * grows from 4 to 8 bytes (see S_MOD_INIT_FUNC_POINTERS dispatch above),
       * so the section base address must be 8-byte aligned or dyld walks
       * misaligned pointers — and on Apple Silicon Rosetta this manifests as
       * a SIGBUS in dyld4::prepare during init-func invocation. Apply by
       * SECTION_TYPE so all pointer-array sections (S_LAZY_SYMBOL_POINTERS,
       * S_NON_LAZY_SYMBOL_POINTERS, S_MOD_INIT_FUNC_POINTERS, S_MOD_TERM_FUNC_POINTERS)
       * pick this up uniformly. log2(8)=3 for M64, log2(4)=2 for M32.
       */
      const uint32_t stype = sect.flags & SECTION_TYPE;
      const bool is_ptr_array =
         stype == S_LAZY_SYMBOL_POINTERS ||
         stype == S_NON_LAZY_SYMBOL_POINTERS ||
         stype == S_MOD_INIT_FUNC_POINTERS ||
         stype == S_MOD_TERM_FUNC_POINTERS;
      if (is_ptr_array) {
         switch (bits) {
         case Bits::M32: if (sect.align < 2) sect.align = 2; break;
         case Bits::M64: if (sect.align < 3) sect.align = 3; break;
         }
      }
      
   }

   template <Bits bits>
   typename Section<bits>::Content::iterator Section<bits>::find(std::size_t vmaddr) {
      typename Content::iterator prev = content.end();
      for (typename Content::iterator it = content.begin();
           it != content.end() && (*it)->loc.vmaddr <= vmaddr;
           prev = it, ++it)
         {}
      return prev;
   }

   template <Bits bits>
   typename Section<bits>::Content::const_iterator Section<bits>::find(std::size_t vmaddr) const {
      typename Content::const_iterator prev = content.end();
      for (typename Content::const_iterator it = content.begin();
           it != content.end() && (*it)->loc.vmaddr <= vmaddr;
           prev = it, ++it)
         {}
      return prev;
   }

   template <Bits bits>
   void Section<bits>::Parse2(ParseEnv<bits>& env) {
      auto content_it = content.begin();

      /* for each placeholder, find blob in this section to insert it before */
      for (auto placeholder_it = env.placeholders.lower_bound(sect.addr);
           placeholder_it != env.placeholders.end() &&
              placeholder_it->first < sect.addr + sect.size;
           placeholder_it = env.placeholders.erase(placeholder_it))
         {
            /* Find first section blob with vmaddr ≥ placeholder. Was:
             *
             *   content_it = std::lower_bound(content_it, content.end(), ...);
             *
             * std::lower_bound on a std::list iterator (bidirectional, not
             * random-access) falls back to std::distance + std::advance, each
             * O(N) on a list. For each placeholder that's O(N); for M
             * placeholders that's O(M*N) — on iPhoto with M≈50K placeholders
             * and N≈500K blobs, ~25 billion ops, observed as 1h41min hang.
             *
             * Linear scan resuming from content_it across the sorted
             * placeholder stream is O(M+N) amortized. content_it never
             * rewinds (placeholders walk vmaddrs ascending; once content_it
             * passes a vmaddr it doesn't need to come back). */
            while (content_it != content.end() &&
                   (*content_it)->loc.vmaddr < placeholder_it->first) {
               ++content_it;
            }
            if (content_it == content.end()) {
               /* Placeholder vmaddr is inside [sect.addr, sect.addr+sect.size)
                * but past every blob in this section — i.e. it points into
                * trailing alignment padding after the last real content. Hard
                * abort here means a binary with one such reference never
                * translates (observed on iWeb's SFProofReader.framework).
                * Insert at end so downstream code that resolves through this
                * placeholder still has a target; the underlying address is
                * preserved in loc.vmaddr. */
               fprintf(stderr,
                       "warning: placeholder vmaddr 0x%zx past last blob in section "
                       "%s (sect ends at 0x%zx); inserting at end\n",
                       placeholder_it->first,
                       std::string(sect.sectname,
                                   strnlen(sect.sectname, sizeof(sect.sectname))).c_str(),
                       sect.addr + sect.size);
               placeholder_it->second->segment = env.current_segment;
               placeholder_it->second->section = this;
               content.insert(content.end(), placeholder_it->second);
               continue;
            }

            /*
             * Soft mismatch: the placeholder's vmaddr lands inside a blob
             * rather than at a blob boundary. This typically happens when
             * linear-sweep disassembly slipped (jump table interpreted as
             * code) so the "instruction" covering 4 bytes actually spans a
             * function-start that something else is referencing. We don't
             * try to retroactively split the blob — instead we insert the
             * placeholder just before the next blob and warn. Its
             * loc.vmaddr (set at construction) still holds the correct
             * target address for downstream consumers.
             */
            if ((*content_it)->loc.vmaddr != placeholder_it->first) {
               fprintf(stderr,
                       "warning: placeholder vmaddr 0x%zx falls inside a blob "
                       "(next blob at 0x%zx, section %s); inserting before next blob\n",
                       placeholder_it->first,
                       (*content_it)->loc.vmaddr,
                       std::string(sect.sectname,
                                   strnlen(sect.sectname, sizeof(sect.sectname))).c_str());
            }

            placeholder_it->second->segment = env.current_segment;
            placeholder_it->second->section = this;

            content.insert(content_it, placeholder_it->second);
         }
   }

   template <Bits bits>
   void Section<bits>::insert(SectionBlob<bits> *blob, const Location& loc, Relation rel) {
      blob->section = this;
      std::size_t Location::*locptr;
      std::size_t sectloc;
      if (loc.offset) {
         locptr = &Location::offset;
         sectloc = sect.offset;
      } else if (loc.vmaddr) {
         locptr = &Location::vmaddr;
         sectloc = sect.addr;
      } else {
         throw std::invalid_argument("location offset and vmaddr are both 0");
      }

      if (loc.*locptr >= sectloc && loc.*locptr < sectloc + sect.size) {
         /* find blob with given offset/address */
         auto it = std::upper_bound(content.begin(), content.end(), loc.*locptr,
                                    [=] (std::size_t loc, SectionBlob<bits> *blob) {
                                       return loc < blob->loc.*locptr;
                                    });
         switch (rel) {
         case Relation::BEFORE:
            --it;
            break;
         case Relation::AFTER:
            break;
         }
         content.insert(it, blob);
      } else {
         throw std::invalid_argument("location not in section");
      }
   }

   template <Bits bits>
   Section<bits>::Section(const Image& img, std::size_t offset, ParseEnv<bits>& env, Parser parser):
      sect(img.at<section_t<bits>>(offset)), segment(env.current_segment), parser(parser)
   {
      /* section resolver */
      env.section_resolver.add(this);
      
      /* read reloaction entries */
      std::size_t reloff = sect.reloff;
      for (uint32_t i = 0; i < sect.nreloc; ++i, reloff += RelocationInfo<bits>::size()) {
         relocs.push_back(RelocationInfo<bits>::Parse(img, reloff, env, loc()));
      }
   }

   template <Bits bits>
   RelocationInfo<bits>::RelocationInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                                        const Location& baseloc):
      info(img.at<relocation_info>(offset))
   {
      /* create reloc blob */
      const Location relocloc = baseloc + info.r_address;
      relocee = RelocBlob<bits>::Parse(img, relocloc, env, info.r_type);
      env.relocs.insert({relocloc.vmaddr, relocee});
   }

   template <Bits bits>
   void RelocationInfo<bits>::Emit(Image& img, std::size_t offset, const Location& baseloc) const {
      relocation_info info = this->info;
      info.r_address = relocee->loc.vmaddr - baseloc.vmaddr;
      img.at<relocation_info>(offset) = info;
   }

   template <Bits bits>
   bool Section<bits>::contains_offset(std::size_t offset) const {
      return offset >= sect.offset && offset < sect.offset + sect.size;
   }

   template <Bits bits>
   bool Section<bits>::contains_vmaddr(std::size_t vmaddr) const {
      return vmaddr >= sect.addr && vmaddr < sect.addr + sect.size;
   }

   template <Bits bits>
   void Section<bits>::AssignID(BuildEnv<bits>& env) {
      id = env.section_counter();
   }

   template <Bits bits>
   SectionBlob<bits> *Section<bits>::StubHelperParser(const Image& img, const Location& loc,
                                                      ParseEnv<bits>& env) {
      if (StubHelperBlob<bits>::can_parse(img, loc, env)) {
         return StubHelperBlob<bits>::Parse(img, loc, env);
      } else {
         return Instruction<bits>::Parse(img, loc, env);
      }
   }

   template class Section<Bits::M32>;
   template class Section<Bits::M64>;
   
}
