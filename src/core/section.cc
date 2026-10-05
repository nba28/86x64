#include <typeinfo>
#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <iterator>
#include <algorithm>
#include <vector>
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

   /*
    * Is this section nothing but NUL-terminated printable C strings and NOP
    * padding?  If so it is a STRING POOL, whatever its section attributes claim.
    *
    * GCC-era i386 toolchains put the C++ RTTI type-NAME strings (__ZTS*) in
    * __TEXT,__const_coal and align each one; because the section lives in an
    * EXECUTABLE segment the filler they emit is NOP padding (0x90, or the
    * multi-byte `0f 1f ...` forms), so the linker stamps the section
    * S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS. Those attributes
    * describe the PADDING, not the payload.
    *
    * Taking them at face value routes a pure string pool through TextParser,
    * which fails two ways: every mangled name gets "translated" as machine code
    * (corrupting type_info::name() — the Civ IV s24 failure mode the __DATA
    * typeinfo routing below exists to prevent), and a pool whose last string
    * does not happen to end on an instruction boundary aborts the parse outright
    * ("blob ... overruns section end": Portal 2's libsteam.dylib, whose 809-name
    * CryptoPP/common pool ends mid-string 5 bytes shy of the section end, where
    * the trailing `inkE\0` decodes as a 7-byte instruction).
    *
    * So: the attribute is evidence, the content is proof. Real code cannot
    * satisfy this test — a non-trivial instruction stream is full of opcode
    * bytes outside 0x20..0x7e that are not NOPs (the synthesized `__jt_tramp`
    * `ff 25` stubs and a `e8 00 00 00 00 / 58` PIC thunk both fail it on their
    * first byte). Structural and universal: it triggers on the byte content of
    * any NOP-padded literal pool, not on a section name or an app.
    */
   template <Bits bits>
   static bool SectionIsNopPaddedCstringPool(const Image& img,
                                             std::size_t fileoff,
                                             std::size_t size) {
      if (size == 0 || fileoff + size > img.size()) { return false; }

      std::size_t strings = 0;
      std::size_t i = 0;
      while (i < size) {
         const uint8_t b = img.at<uint8_t>(fileoff + i);
         if (b == 0x00) {
            ++i;                      /* inter-string / alignment zero fill */
            continue;
         }
         /* Try NOP padding FIRST, before treating the byte as string content.
          * A multi-byte NOP may open with a PRINTABLE prefix byte — the
          * operand-size prefix 0x66 is 'f', and the segment-override prefixes
          * 0x2e/0x3e/0x26/0x36/0x64/0x65 are './>/&/6/d/e' — so `66 0f 1f 44 00
          * 00` (nopw) reads as a string starting "f" and then trips over the
          * 0x0f. Deciding NOP-first is safe in the other direction too: every
          * NOP encoding is either exactly 0x90 or contains 0x0f, and neither
          * byte is printable, so a genuine C string can never be consumed here.
          */
         {
            xed_decoded_inst_t xd;
            xed_decoded_inst_zero_set_mode(&xd, &Instruction<bits>::dstate());
            xed_decoded_inst_set_input_chip(&xd, XED_CHIP_INVALID);
            if (xed_decode(&xd, &img.at<uint8_t>(fileoff + i),
                           size - i) == XED_ERROR_NONE &&
                xed_decoded_inst_get_iclass(&xd) == XED_ICLASS_NOP) {
               const unsigned len = xed_decoded_inst_get_length(&xd);
               if (len > 0) { i += len; continue; }
            }
         }
         if (b >= 0x20 && b < 0x7f) {
            /* A printable run only counts if it is NUL-terminated INSIDE the
             * section; an unterminated tail is not a string. */
            std::size_t j = i;
            while (j < size) {
               const uint8_t c = img.at<uint8_t>(fileoff + j);
               if (c == 0x00) { break; }
               if (!(c >= 0x20 && c < 0x7f)) { return false; }   /* nonprintable in run */
               ++j;
            }
            if (j == size) { return false; }   /* unterminated tail run */
            ++strings;
            i = j + 1;                /* step over the terminating NUL */
            continue;
         }
         return false;   /* neither NOP padding nor printable string */
      }
      return strings > 0;
   }

   template <Bits bits>
   Section<bits> *Section<bits>::Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
      section_t<bits> sect = img.at<section_t<bits>>(offset);
      const uint32_t flags = sect.flags;
      const std::string segname(sect.segname, strnlen(sect.segname, sizeof(sect.segname)));
      const std::string sectname(sect.sectname, strnlen(sect.sectname, sizeof(sect.sectname)));

      /* Code sections by name: __StaticInit and the coalesced-code sections
       * do not reliably carry S_ATTR_PURE_INSTRUCTIONS; __jt_tramp holds our
       * synthesized import trampolines (re-decoded on every re-parse). */
      static const char *const text_sectnames[] = {
         SECT_TEXT, SECT_STUBS, SECT_SYMBOL_STUB,
         "__StaticInit", "__textcoal_nt", "__coalesced", "__jt_tramp",
      };
      for (const char *name : text_sectnames) {
         if (sectname == name) {
            return new Section<bits>(img, offset, env, TextParser);
         }
      }
      if (sectname == SECT_STUB_HELPER) {
         return new Section<bits>(img, offset, env, StubHelperParser);
      }

      /* Our own metadata sections (M64 only): each is lifted back into a live
       * blob whose Parse re-resolves the addresses it records, so a later
       * stage's re-layout re-emits them current. Through DataParser their
       * address-valued entries would be "rebased" or frozen stale. */
      static const std::pair<const char *, Parser> synthetic[] = {
         {"__86x64_xrel",   XrelBlob<bits>::Parse},
         {"__86x64_abs32",  Abs32Blob<bits>::Parse},
         {"__86x64_dptr",   Abs32Blob<bits>::Parse},
         {"__86x64_cpin",   ConstPinBlob<bits>::Parse},
         {"__86x64_pcmap",  PcmapBlob<bits>::Parse},
         {"__86x64_ehlsda", EhlsdaBlob<bits>::Parse},
      };
      for (const auto& syn : synthetic) {
         if (sectname == syn.first) {
            return new Section<bits>(img, offset, env,
                                     bits == Bits::M64 ? syn.second : DataBlob<bits>::Parse);
         }
      }

      const uint32_t stype = flags & SECTION_TYPE;
      switch (stype) {
      case S_LAZY_SYMBOL_POINTERS:
         return new Section<bits>(img, offset, env, LazySymbolPointer<bits>::Parse);
      case S_NON_LAZY_SYMBOL_POINTERS:
      case S_MOD_INIT_FUNC_POINTERS:     /* pointer-width slots: 4 -> 8 bytes */
      case S_MOD_TERM_FUNC_POINTERS:
         return new Section<bits>(img, offset, env, NonLazySymbolPointer<bits>::Parse);
      case S_CSTRING_LITERALS:
         return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
      case S_LITERAL_POINTERS:           /* ObjC1 __message_refs / __cls_refs */
         return new Section<bits>(img, offset, env, DataParser);
      case S_ZEROFILL:
      case S_GB_ZEROFILL:
      case S_THREAD_LOCAL_ZEROFILL:
         return new Section<bits>(img, offset, env, ZeroBlob<bits>::Parse);
      case S_REGULAR:
      case S_COALESCED:                  /* weak C++ data: typeinfo, vtables */
         {
            /* The instruction attribute in an executable segment means code,
             * unless the content proves a NOP-padded string pool. */
            const bool exec_seg = env.current_segment != nullptr &&
               (env.current_segment->segment_command.initprot & VM_PROT_EXECUTE);
            if ((flags & S_ATTR_PURE_INSTRUCTIONS) != 0 && exec_seg &&
                !SectionIsNopPaddedCstringPool<bits>(img, sect.offset, sect.size)) {
               return new Section<bits>(img, offset, env, TextParser);
            }
            const bool is_data = segname.rfind("__DATA", 0) == 0;
            if (is_data && sectname == "__cfstring" &&
                (sect.size % CFStringBlob<bits>::record_size) == 0) {
               return new Section<bits>(img, offset, env, CFStringBlob<bits>::Parse);
            }
            /* Pointer-bearing data: __DATA, __TEXT,__const (switch tables),
             * and the ObjC1 __OBJC metadata. Everything else (__eh_frame,
             * __TEXT,__const_coal strings, ...) is opaque. */
            if (is_data || (segname == SEG_TEXT && sectname == "__const") || segname == SEG_OBJC) {
               return new Section<bits>(img, offset, env, DataParser);
            }
            return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
         }
      default:
         /* Unknown types (S_INTERPOSING, S_DTRACE_DOF, ...) pass through verbatim. */
         return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
      }
   }

   /*
    * A 4-byte aligned data word: pointer or constant?
    *
    * A classic image's local relocation table (when present) is the truth,
    * both ways. Otherwise the word is a pointer candidate if its value lands in
    * the image, and a chain of evidence-based gates demotes the candidates that
    * are really integers. Most gates are M32-only (they need the original
    * image); the M64 re-parses instead honour the M32 verdicts carried in
    * __86x64_cpin. Each gate names the guard that proves it.
    */
   template <Bits bits>
   SectionBlob<bits> *Section<bits>::DataParser(const Image& img, const Location& loc,
                                                ParseEnv<bits>& env) {
      /* Unaligned bytes and a section's sub-word tail stay bytes; aligned
       * words are always Immediates so later placeholders find a blob. */
      if ((loc.vmaddr & 3) != 0 || loc.offset + 4 > img.size()) {
         return DataBlob<bits>::Parse(img, loc, env);
      }
      if (env.current_section != nullptr) {
         const auto& cs = env.current_section->sect;
         if (loc.offset + 4 > cs.offset + cs.size) {
            return DataBlob<bits>::Parse(img, loc, env);
         }
      }
      const uint32_t value = img.at<uint32_t>(loc.offset);

      bool in_objc_symbols = false;    /* ObjC1 objc_symtab: never points at code */
      bool is_text_const_sect = false; /* switch tables target unsymboled blocks */
      bool in_objc_methods = false;    /* ObjC1 method lists: an IMP is an entry */
      if (env.current_section != nullptr) {
         const auto& cs = env.current_section->sect;
         const bool objc = strncmp(cs.segname, SEG_OBJC, sizeof(cs.segname)) == 0;
         const std::string sn(cs.sectname, strnlen(cs.sectname, sizeof(cs.sectname)));
         in_objc_symbols = objc && sn == "__symbols";
         static const bool objc_imp_off = std::getenv("M64_NO_OBJC_IMP_ENTRY") != nullptr;
         in_objc_methods = objc && !objc_imp_off &&
            (sn == "__inst_meth" || sn == "__cls_meth" ||
             sn == "__cat_inst_meth" || sn == "__cat_cls_meth");
         is_text_const_sect = strncmp(cs.segname, SEG_TEXT, sizeof(cs.segname)) == 0;
      }

      /* M64 re-parse: the M32 pass already called this slot a constant. */
      static const bool no_const_pin = std::getenv("M64_NO_CONST_PIN") != nullptr;
      if (bits == Bits::M64 && env.have_const_pins && !no_const_pin &&
          env.const_pin_slots.count(loc.vmaddr) != 0) {
         return Immediate<bits>::Parse(img, loc, env, /*is_pointer=*/false);
      }

      return Immediate<bits>::Parse(img, loc, env,
         data_word_is_pointer(img, loc, env, value, in_objc_symbols,
                              is_text_const_sect, in_objc_methods));
   }

   template <Bits bits>
   bool Section<bits>::data_word_is_pointer(const Image& img, const Location& loc,
                                            ParseEnv<bits>& env, uint32_t value,
                                            bool in_objc_symbols, bool is_text_const_sect,
                                            bool in_objc_methods) {
      /* The floor is the image's lowest section, not 0x1000: a dylib's __TEXT
       * starts at 0 and its first functions may sit below one page (Portal 2
       * inputsystem vtable slots 0xed0...). */
      const std::size_t ptr_floor = std::min<std::size_t>(0x1000, env.min_section_vmaddr());
      if (value < ptr_floor || value >= 0x80000000U) { return false; }
      Segment<bits> *seg = nullptr;
      for (Segment<bits> *s : env.archive.segments()) {
         const std::string name(s->segment_command.segname,
                                strnlen(s->segment_command.segname, sizeof(s->segment_command.segname)));
         if (name == SEG_PAGEZERO || name == SEG_LINKEDIT) continue;
         if (s->contains_vmaddr(value)) { seg = s; break; }
      }
      if (seg == nullptr) { return false; }
      const bool m32 = bits == Bits::M32;

      const bool exec = (seg->segment_command.initprot & VM_PROT_EXECUTE) != 0;

      /* A slot the linker recorded a local reloc for IS a pointer (PvZ libbass
       * fn tables; guard classic-reloc-accept) ... */
      static const bool reloc_accept_off =
         std::getenv("M64_NO_CLASSIC_RELOC_ACCEPT") != nullptr;
      if (m32 && env.have_classic_local_relocs && !reloc_accept_off &&
          env.local_reloc_addrs.count(loc.vmaddr) != 0) {
         return true;
      }
      /* ... and so does a slot the LC_DYLD_INFO rebase stream rebases; in a
       * slidable image every other word is a constant, however well its value
       * aliases the image (ParseEnv::have_rebase_stream). minimp3's
       * g_hz {44100, 48000, 32000} in a small dylib: 44100 = 0xac44 landed in
       * __DATA and was "rebased", so every MP3 frame failed to sync (the Miles
       * MP3 provider). Kill M64_NO_REBASE_EXACT; guard rebase-exact-const. */
      static const bool rebase_exact_off = std::getenv("M64_NO_REBASE_EXACT") != nullptr;
      if (m32 && env.have_rebase_stream && !rebase_exact_off) {
         return env.rebase_slot_addrs.count(loc.vmaddr) != 0;
      }

      /* A misaligned pointer into data is plausible only as a writable
       * pointer FIELD aimed at file-backed data (a byte-record table: Halo
       * 0x379ec6); into zero-fill or from read-only const data it is a
       * constant (photocd's 0x30a91 YCC coefficient). */
      if (!exec && (value & 3) != 0) {
         const Section<bits> *tsec = env.section_at(value);
         const uint32_t tst = tsec ? (tsec->sect.flags & SECTION_TYPE) : 0;
         const bool tgt_filebacked = tsec && tst != S_ZEROFILL && tst != S_GB_ZEROFILL;
         bool slot_writable = false;
         for (Segment<bits> *sl : env.archive.segments()) {
            if (!sl->contains_vmaddr(loc.vmaddr)) { continue; }
            slot_writable = (sl->segment_command.initprot & VM_PROT_WRITE) != 0 &&
                            (sl->segment_command.initprot & VM_PROT_EXECUTE) == 0;
            break;
         }
         if (!(tgt_filebacked && slot_writable)) { return false; }
      }

      /* Zero-fill target with nothing to corroborate it: a (small,small)
       * integer pair spells an address in a small image (Halo tag records;
       * guard zerofill-target-pair). */
      if (m32 && !exec && !env.have_classic_local_relocs &&
          env.zerofill_target_unattested(img, value)) {
         return false;
      }
      if (exec && in_objc_symbols) {
         return false;   /* objc_symtab count word aliasing __text */
      }
      /* A `char *` targets a string START; an interior hit is a byte table,
       * unless the slot's neighbours are string pointers (a tail-merged
       * literal; guard cstring-tailmerge-ptr). */
      if (m32 && exec && env.cstring_interior_alias(img, value) &&
          !env.cstring_slot_has_string_neighbour(img, loc.vmaddr)) {
         return false;
      }
      /* With local text symbols, a code pointer in data targets a symbol. */
      if (m32 && exec && !is_text_const_sect && env.code_alias_is_constant(value)) {
         return false;
      }
      /* A code pointer targets an instruction boundary (Civ IV " ._";
       * guard code-interior-alias). */
      if (m32 && exec && env.code_interior_alias(value)) {
         return false;
      }
      /* A jump table is a run of code targets; a neighbour that aliases code
       * mid-instruction contradicts it (guard jt-cohesion). */
      if (m32 && exec && env.code_alias_run_contradicted(img, loc, value)) {
         return false;
      }
      /* Stripped image: a data-resident code pointer targets a function entry
       * (symbol, prologue, thunk); switch tables in __TEXT,__const and ObjC1
       * IMPs are exempt (guards code-entry-alias, objc-imp-entry). */
      if (m32 && exec && !is_text_const_sect && !in_objc_methods &&
          env.code_alias_lacks_entry_evidence(img, value)) {
         return false;
      }
      /* Last: siblings of this slot at the record stride are integers, so
       * this field is one too (Halo #35; guard record-field-pair). */
      if (m32 && !env.have_classic_local_relocs &&
          (!exec || !env.code_target_has_entry_evidence(img, value)) &&
          (env.record_field_neighbours_are_integers(img, loc.vmaddr) ||
           env.packed_pair_family(img, loc.vmaddr, value))) {
         return false;
      }
      /* ... and, in a classic image, a slot with no reloc is a constant
       * (Portal 2 engine.dylib's 637 packed int16 pairs). */
      if (m32 && env.have_classic_local_relocs && env.local_reloc_addrs.count(loc.vmaddr) == 0) {
         return false;
      }
      return true;
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
      /* A recognised i386 PIC relative-offset jump-table entry (detected by
       * DetectJumpTables): emit a relocatable 4-byte JumpTableEntry instead of
       * decoding the table bytes as code. The linear sweep lands on each slot
       * exactly (entries are 4 bytes and the dispatch precedes the table). */
      auto jt = env.jump_table_slots.find(loc.vmaddr);
      if (jt != env.jump_table_slots.end()) {
         return JumpTableEntry<bits>::Parse(img, loc, env, jt->second);
      }
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

      /* claim PIC switch tables before the sweep decodes them as code */
      DetectJumpTables(img, env);

      const bool text = parser == TextParser;
      const std::size_t sect_end = sect.addr + sect.size;
      const std::size_t end = sect.offset + sect.size;
      std::size_t it = sect.offset;
      std::size_t vmaddr = sect.addr;
      bool prev_no_fallthrough = false;   /* last blob was ret/jmp (or padding) */
      /* A stripped image has no symbol for most entries, so align_even (the
       * even-alignment the Itanium pmf low bit needs) marks the first
       * real instruction at an even i386 address after ret/jmp/noreturn call +
       * alignment padding: nothing falls into it, so a pad there is never executed
       * (Portal 2 libcef: half its entries odd, a non-virtual RunnableMethod
       * pmf dispatched as virtual -> call 0 at quit). i386 source only: a
       * re-parse of our own x86_64 output decodes claimed tables as code, and
       * the transform already carries the flags over. Kill
       * M64_NO_DEAD_ENTRY_EVEN; guard stripped-pmf-even. */
      static const bool dead_entry_even = std::getenv("M64_NO_DEAD_ENTRY_EVEN") == nullptr;
      bool dead_run = true;   /* section start, or ret/jmp then only nop/padding */
      /* After a noreturn call the next instruction is still the call's return
       * address, which the EH unwinder looks up (51_eh_throw_int: a pad after
       * `call ___cxa_throw` hid its call site): mark only past alignment padding. */
      bool dead_needs_gap = false;

      /* An instruction that would straddle a function symbol swallowed the
       * inter-function padding before it (Portal 2 libtogl: `a1 00 00 00 55`).
       * A claimed jump-table slot is data of known length and never straddles. */
      auto straddles_symbol = [&]() {
         if (env.jump_table_slots.count(vmaddr) != 0) { return false; }
         auto ns = env.func_syms.upper_bound(vmaddr);
         if (ns == env.func_syms.end() || *ns >= sect_end) { return false; }
         xed_decoded_inst_t xd;
         xed_decoded_inst_zero_set_mode(&xd, &Instruction<bits>::dstate());
         xed_decoded_inst_set_input_chip(&xd, XED_CHIP_INVALID);
         if (xed_decode(&xd, &img.at<uint8_t>(it), img.size() - it) != XED_ERROR_NONE) {
            return false;
         }
         const unsigned len = xed_decoded_inst_get_length(&xd);
         return len > 1 && *ns < vmaddr + len;
      };

      while (it != end) {
         SectionBlob<bits> *elem;
         bool emitted_padding = false;
         /* Resync at 0x00 alignment padding after a terminator (stripped
          * images have no symbols to straddle): no compiler starts code with
          * `add [r/m8],r8`, and decoding `00 55 89 ...` would swallow the next
          * function's `push %ebp` (Halo static-init crash). */
         if (text && prev_no_fallthrough && img.at<uint8_t>(it) == 0x00 &&
             env.jump_table_slots.count(vmaddr) == 0) {
            elem = DataBlob<bits>::Parse(img, Location(it, vmaddr), env);
            emitted_padding = true;
         } else if (text && straddles_symbol()) {
            elem = DataBlob<bits>::Parse(img, Location(it, vmaddr), env);
         } else {
            elem = parser(img, Location(it, vmaddr), env);
         }

         const auto *elem_in = text ? dynamic_cast<Instruction<bits> *>(elem) : nullptr;
         const xed_category_enum_t elem_cat =
            elem_in ? xed_decoded_inst_get_category(&elem_in->xedd) : XED_CATEGORY_INVALID;
         const bool elem_nop = elem_cat == XED_CATEGORY_NOP || elem_cat == XED_CATEGORY_WIDENOP;
         if (text && env.func_syms.count(vmaddr)) {
            elem->func_entry = true;   /* even-aligned at Build (pmf low bit) */
         } else if (text && bits == Bits::M32 && dead_entry_even && dead_run && !dead_needs_gap && elem_in &&
                    !elem_nop && (vmaddr & 1) == 0) {
            elem->align_even = true;
         }
         if (!elem_in && !emitted_padding && !dynamic_cast<JumpTableEntry<bits> *>(elem)) {
            dead_run = false;   /* undecodable bytes: a pad there would re-decode them */
         } else if (elem_in) {   /* padding and claimed table slots keep the state */
            const bool noreturn_call =   /* call ___stack_chk_fail etc. */
               elem_cat == XED_CATEGORY_CALL &&
               xed_decoded_inst_get_iform_enum(&elem_in->xedd) == XED_IFORM_CALL_NEAR_RELBRz &&
               env.noreturn_stubs.count(vmaddr + elem->size() +
                                        xed_decoded_inst_get_branch_displacement(&elem_in->xedd)) != 0;
            dead_run = elem_cat == XED_CATEGORY_RET || elem_cat == XED_CATEGORY_UNCOND_BR ||
                       noreturn_call || (dead_run && elem_nop);
            dead_needs_gap = noreturn_call;
         } else if (emitted_padding) {
            dead_needs_gap = false;
         }
         elem->iter = content.insert(content.end(), elem);
         /* the i386 address, before Build re-lays out: keys the EH PC map */
         elem->orig_vmaddr = vmaddr;

         if (text) {
            if (emitted_padding) {
               prev_no_fallthrough = true;
            } else if (auto *in = dynamic_cast<Instruction<bits> *>(elem)) {
               const xed_category_enum_t cat = xed_decoded_inst_get_category(&in->xedd);
               prev_no_fallthrough = cat == XED_CATEGORY_RET || cat == XED_CATEGORY_UNCOND_BR;
            } else {
               prev_no_fallthrough = false;
            }
         }

         const std::size_t step = elem->size();
         if (step == 0 || it + step > end) {
            throw error("section %s: blob at offset 0x%zx (size %zu) overruns section end 0x%zx",
                        name().c_str(), it, step, end);
         }
         it += step;
         vmaddr += step;
      }

      DetectPicAnchoredDisps(env);
      env.current_section = nullptr;
   }

   /*
    * PIC-anchored operands (i386 input only).
    *
    * i386 PIC code materialises its own PC into a register — `call $+0; pop
    * %reg` (clang) or `call ___i686.get_pc_thunk.<r>` (GCC) — and reaches every
    * global as `disp(%reg)` with disp = target - anchor in the i386 layout.
    * Translation changes the layout, so each such operand is resolved here to
    * the blob at anchor+disp (memdisp, pic_anchored=true) and the transform
    * re-encodes it rip-relative (Instruction::lower_mem).
    *
    * The walk is LINEAR over the section, carrying reg -> anchor state. A
    * linear walk reaches a function's early epilogue before the blocks that
    * are only entered by a branch taken earlier, so the rules are tuned so
    * that KEEPING an anchor is the safe error (a missed rewrite is a wild
    * access; a kept anchor is only stale until the next definition):
    *
    *  - Any definition of a register ends its anchor, EXCEPT `pop` (epilogue
    *    restores precede branch-reached blocks and EH landing pads; measured
    *    276 lost sites on Portal 2) and the restore of the callee-saved
    *    anchor register from its prologue save slot.
    *  - Anchors follow register copies and frame-slot spill/reload.
    *  - Every forward branch (and every case body of a claimed PIC jump
    *    table) snapshots the state for its target; at the target the snapshot
    *    is intersected with the fall-through state, or ADOPTED when the
    *    fall-through is dead (after ret/jmp, or a noreturn call placed after
    *    one). An intersection can only recover an anchor, never invent one.
    *  - RET ends the function only when no forward branch target within
    *    64 KB is still pending; a CALL clobbers eax/ecx/edx; interleaved data
    *    clears everything.
    *
    * Inside a PIC-anchored region the absolute-address heuristics are
    * cancelled: PIC code never embeds an absolute address, so a heuristic
    * pointer immediate or `disp32(%base)` table capture there is an integer.
    * Guards: tests-i386 04_pic_data_read and the pic_anchor_* tests.
    */

   /* x86 GPR encoding (0..7, as stored in ParseEnv::pic_thunks) -> xed reg */
   static xed_reg_enum_t gpr32_from_enc(uint8_t enc) {
      static const xed_reg_enum_t regs[8] = {
         XED_REG_EAX, XED_REG_ECX, XED_REG_EDX, XED_REG_EBX,
         XED_REG_ESP, XED_REG_EBP, XED_REG_ESI, XED_REG_EDI,
      };
      return enc < 8 ? regs[enc] : XED_REG_INVALID;
   }

   /* Keep only the entries of `dst` that `src` maps to the same value. */
   template <typename Map>
   static void intersect_into(Map& dst, const Map& src) {
      for (auto it = dst.begin(); it != dst.end(); ) {
         auto s = src.find(it->first);
         it = (s == src.end() || s->second != it->second) ? dst.erase(it) : std::next(it);
      }
   }

   /* Record `state` for branch target `tgt`, intersecting with any earlier
    * source of the same target. */
   template <typename Map>
   static void snapshot_for(std::map<std::size_t, Map>& snaps, std::size_t tgt, const Map& state) {
      auto it = snaps.find(tgt);
      if (it == snaps.end()) { snaps[tgt] = state; }
      else { intersect_into(it->second, state); }
   }

   static bool is_gpr32(xed_reg_enum_t r) { return r >= XED_REG_EAX && r <= XED_REG_EDI; }

   /* Does the instruction read GPR32 `r` as a register operand? (Not as an
    * address base/index: `leal 0x10c4c9(%esi),%esi` is an anchor's last use.) */
   static bool reads_gpr32(const xed_decoded_inst_t& xedd, xed_reg_enum_t r) {
      const xed_inst_t *xi = xed_decoded_inst_inst(&xedd);
      for (unsigned i = 0; i < xed_inst_noperands(xi); ++i) {
         const xed_operand_t *op = xed_inst_operand(xi, i);
         const xed_operand_enum_t nm = xed_operand_name(op);
         if (!xed_operand_read(op) || !xed_operand_is_register(nm)) continue;
         const xed_reg_enum_t raw = xed_decoded_inst_get_reg(&xedd, nm);
         if (raw != XED_REG_INVALID && xed_get_largest_enclosing_register32(raw) == r) { return true; }
      }
      return false;
   }

   /* Every GPR32 the instruction writes, sub-register writes included. */
   template <typename F>
   static void for_each_written_gpr32(const xed_decoded_inst_t& xedd, F f) {
      const xed_inst_t *xi = xed_decoded_inst_inst(&xedd);
      for (unsigned i = 0; i < xed_inst_noperands(xi); ++i) {
         const xed_operand_t *op = xed_inst_operand(xi, i);
         if (!xed_operand_written(op)) continue;
         const xed_operand_enum_t nm = xed_operand_name(op);
         if (!xed_operand_is_register(nm)) continue;
         const xed_reg_enum_t raw = xed_decoded_inst_get_reg(&xedd, nm);
         if (raw == XED_REG_INVALID) continue;
         const xed_reg_enum_t r = xed_get_largest_enclosing_register32(raw);
         if (is_gpr32(r)) { f(r); }
      }
   }

   /* A backward branch to `tgt` from `from` stays in its function: no function
    * symbol in (tgt, from], and `tgt` is not itself an entry (a jump there is a
    * tail call or a self-loop through the prologue). */
   static bool pic_same_function(const std::set<std::size_t>& fs, std::size_t tgt, std::size_t from) {
      auto f = fs.upper_bound(tgt);
      return fs.count(tgt) == 0 && (f == fs.end() || *f > from);
   }

   template <typename Snaps>
   static bool pic_any_nonempty(const Snaps& snaps) {
      for (const auto& kv : snaps) { if (!kv.second.empty()) { return true; } }
      return false;
   }

   template <Bits bits>
   void Section<bits>::DetectPicAnchoredDisps(ParseEnv<bits>& env) {
      if constexpr (bits != Bits::M32) {
         return;
      }
      if (parser != TextParser) {
         return;
      }

      using AnchorMap = std::unordered_map<xed_reg_enum_t, std::size_t>;
      using Slot = std::pair<xed_reg_enum_t, ssize_t>;   /* (frame base, disp) */
      using SlotMap = std::map<Slot, std::size_t>;

      AnchorMap anchors;              /* reg -> anchor vmaddr */
      SlotMap anchor_slots;           /* frame slot -> anchor it holds */
      /* frame slots holding a register's FUNCTION-ENTRY value: stores seen
       * before the anchor is established are candidates, promoted when the
       * anchor pop follows; any later store to the slot demotes it. */
      std::map<Slot, xed_reg_enum_t> pre_anchor_saves, entry_save_slots;
      /* sticky "this function is PIC codegen"; outlives the last live anchor */
      bool anchored_region = false;
      std::set<std::size_t> pending_forward_targets;
      std::map<std::size_t, AnchorMap> branch_anchor_snap;
      std::map<std::size_t, SlotMap> branch_slot_snap;
      const bool entry_save_gate =
         std::getenv("M64_NO_PIC_ANCHOR_ENTRY_SAVE") == nullptr;   /* guard OFF arm */
      const bool trace = std::getenv("MACHO_TRACE_ANCHOR") != nullptr;

      auto clear_all = [&]() {
         anchors.clear();
         anchor_slots.clear();
         entry_save_slots.clear();
         pre_anchor_saves.clear();
         anchored_region = false;
      };

      /* GCC PIC thunks `mov (%esp),%reg; ret`: entry vmaddr -> %reg. Thunks
       * in other sections come from the symbol table (env.pic_thunks). */
      std::unordered_map<std::size_t, xed_reg_enum_t> pic_thunks;
      {
         Instruction<bits> *mov = nullptr;
         for (SectionBlob<bits> *blob : content) {
            auto *cur = dynamic_cast<Instruction<bits> *>(blob);
            if (mov && cur &&
                xed_decoded_inst_get_category(&cur->xedd) == XED_CATEGORY_RET) {
               const xed_reg_enum_t dst =
                  xed_decoded_inst_get_reg(&mov->xedd, XED_OPERAND_REG0);
               if (is_gpr32(dst)) {
                  pic_thunks[mov->loc.vmaddr] = dst;
               }
            }
            mov = nullptr;
            if (cur &&
                xed_decoded_inst_get_iform_enum(&cur->xedd) == XED_IFORM_MOV_GPRv_MEMv) {
               const xed_operand_values_t *mops = xed_decoded_inst_operands_const(&cur->xedd);
               if (xed_decoded_inst_number_of_memory_operands(&cur->xedd) == 1 &&
                   xed_decoded_inst_get_base_reg(mops, 0) == XED_REG_ESP &&
                   xed_decoded_inst_get_index_reg(mops, 0) == XED_REG_INVALID &&
                   xed_decoded_inst_get_memory_displacement(mops, 0) == 0) {
                  mov = cur;
               }
            }
         }
      }
      for (const auto& kv : env.pic_thunks) {
         const xed_reg_enum_t r = gpr32_from_enc(kv.second);
         if (r != XED_REG_INVALID) { pic_thunks.emplace(kv.first, r); }
      }

      /* Backward branches (a loop back-edge; a dispatch block placed BEFORE
       * the code that reloads its anchor and branches back to it) snapshot
       * too. The linear walk has already passed their targets, so a second
       * walk joins them there exactly like forward snapshots; it runs only
       * when one of them carries an anchor, and can only add rewrites. Portal
       * 2 server CUtlBuffer::VaScanf (anchor reloaded from -0x64(%ebp), then
       * `jbe` back to the table dispatch). Kill M64_NO_PIC_BACK_EDGE (also
       * DetectJumpTables); guard 99_jt_back_edge_join. */
      static const bool back_edge = std::getenv("M64_NO_PIC_BACK_EDGE") == nullptr;
      std::map<std::size_t, AnchorMap> back_anchor_snap;
      std::map<std::size_t, SlotMap> back_slot_snap;
      std::map<std::size_t, std::size_t> back_src_max;   /* target -> last source */
      int pass = 0;
      auto record_back = [&](std::size_t tgt, std::size_t from) {
         if (pass == 0 && back_edge && tgt >= sect.addr && from - tgt <= 0x10000 &&
             pic_same_function(env.func_syms, tgt, from)) {
            snapshot_for(back_anchor_snap, tgt, anchors);
            snapshot_for(back_slot_snap, tgt, anchor_slots);
            std::size_t& m = back_src_max[tgt];
            m = std::max(m, from);
            if (trace) {
               fprintf(stderr, "[anchor] back-edge 0x%zx <- 0x%zx regs=%zu slots=%zu\n",
                       tgt, from, anchors.size(), anchor_slots.size());
            }
         }
      };
      /* Targets of backward relative branches: the only places an ORPHAN block
       * may open (see `orphan` below). A block no edge reaches at all is an EH
       * landing pad (entered by the unwinder with the callee-saved anchors of
       * the throw site) or code reached indirectly: the state before it is the
       * function's own and stays right. Treating pads as orphan dropped their
       * anchors and their jmps' snapshots: Angry Birds 0x28bd, a pad's
       * `cmpl 0x30480e(%edi)` behind `call <noreturn>` lowered as
       * edi+translated(0x30480e) (corpus: 4442 such hunks; Civ IV 811).
       * Kill M64_NO_PIC_ORPHAN_BACK_ONLY; guard 99_pic_anchor_unreached_pad. */
      static const bool orphan_back_only = std::getenv("M64_NO_PIC_ORPHAN_BACK_ONLY") == nullptr;
      std::unordered_set<std::size_t> back_targets;
      for (SectionBlob<bits> *blob : content) {
         const auto *bi = dynamic_cast<Instruction<bits> *>(blob);
         if (!bi) { continue; }
         const xed_category_enum_t bc = xed_decoded_inst_get_category(&bi->xedd);
         if (bc != XED_CATEGORY_COND_BR && bc != XED_CATEGORY_UNCOND_BR) { continue; }
         const ssize_t bd = xed_decoded_inst_get_branch_displacement(&bi->xedd);
         if (bd < 0) {
            back_targets.insert(bi->loc.vmaddr + xed_decoded_inst_get_length(&bi->xedd) + bd);
         }
      }
      /* Pass-0 rewrites made inside an ORPHAN, with the stale state left
       * before its jmp: vmaddr -> (anchor reg, anchor). See the revert below. */
      static const bool orphan_revert = std::getenv("M64_NO_PIC_ORPHAN_REVERT") == nullptr;
      struct OrphanRewrite { xed_reg_enum_t reg; std::size_t anchor, top; };
      std::unordered_map<std::size_t, OrphanRewrite> orphan_rewrites;
      std::size_t orphan_top = 0;                  /* where the current orphan opened */
      std::set<xed_reg_enum_t> orphan_written;     /* regs written since then */
      /* (orphan top, reg) pairs the loop [top, last back-edge source] writes */
      std::set<std::pair<std::size_t, xed_reg_enum_t>> loop_carried;
      for (; pass < 2; ++pass) {
         if (pass == 1) {
            if (!pic_any_nonempty(back_anchor_snap) && !pic_any_nonempty(back_slot_snap)) { break; }
            clear_all();
            pending_forward_targets.clear();
            branch_anchor_snap.clear();
            branch_slot_snap.clear();
            /* One linear scan for every candidate loop range (content is a list). */
            std::map<std::size_t, std::set<xed_reg_enum_t>> want;   /* top -> regs */
            for (const auto& kv : orphan_rewrites) { want[kv.second.top].insert(kv.second.reg); }
            auto next = want.begin();
            std::vector<std::pair<std::size_t, std::size_t>> active;   /* (top, end) */
            for (SectionBlob<bits> *blob : content) {
               if (next == want.end() && active.empty()) { break; }
               const auto *ci = dynamic_cast<Instruction<bits> *>(blob);
               if (!ci) { continue; }
               const std::size_t vm = ci->loc.vmaddr;
               for (; next != want.end() && next->first <= vm; ++next) {
                  auto e = back_src_max.find(next->first);
                  if (e != back_src_max.end()) { active.emplace_back(next->first, e->second); }
               }
               active.erase(std::remove_if(active.begin(), active.end(),
                                           [&](const auto& a) { return vm > a.second; }),
                            active.end());
               for (const auto& a : active) {
                  const auto& regs = want[a.first];
                  /* a self-update (`addl $4,%ebx`), not a callee-saved restore,
                   * a reload or a last-use lea (libsteam: the linear range of an
                   * EH selector dispatch spans `popl %esi` and
                   * `leal 0x10c4c9(%esi),%esi`) */
                  for_each_written_gpr32(ci->xedd, [&](xed_reg_enum_t r) {
                     if (regs.count(r) && reads_gpr32(ci->xedd, r)) { loop_carried.emplace(a.first, r); }
                  });
               }
            }
         }
         Instruction<bits> *prev_inst = nullptr;
         /* category of the last non-nop instruction, and whether the linear
          * fall-through is dead (after ret/jmp, sticky across a noreturn call) */
         xed_category_enum_t last_flow_cat = XED_CATEGORY_INVALID;
         bool ft_dead = false;
         /* After a call that never returns (env.noreturn_stubs) the code is
          * reached only by branches: its state is unknown, so the next branch
          * target ADOPTS its snapshot. Portal 2 server CServerGameDLL::DLLInit: an
          * EH pad (`mov %eax,%esi ... call _Unwind_Resume`) falls into a loop head
          * whose only real entry is a forward jmp carrying the %esi anchor; the
          * dead fall-through's intersection dropped it and every later PIC store
          * kept its raw disp (SIGBUS writing __TEXT). Needs function symbols, so a
          * function entry after the call is never taken for such a join. Kill
          * M64_NO_PIC_NORETURN; guard 99_pic_anchor_noreturn_join. */
         static const bool noreturn_gate = std::getenv("M64_NO_PIC_NORETURN") == nullptr;
         bool unreached = false;
         /* ORPHAN: code past a dead fall-through that no recorded edge reaches
          * (yet) and that a backward branch targets (joined on the second
          * walk; see back_targets for why "not at all" is excluded). Its state
          * is unknown, so its branches record nothing
          * and the next join ADOPTS its snapshot. It used to run on the stale
          * state left before the jmp and constrain joins with it. Portal 2 engine
          * Mod_LoadNodes: `jmp L1; L0: <calls>; jmp L2` (L0 only by a backward
          * jae) intersected L2 with the stale state, dropping the %ebx anchor
          * copy; the loop head's back-edge snapshot inherited the loss on both
          * walks -> raw `0x30aaa3(%ecx)` -> NULL deref. Kill
          * M64_NO_PIC_ORPHAN_TOP; guard 99_pic_anchor_orphan_block. */
         static const bool orphan_gate = std::getenv("M64_NO_PIC_ORPHAN_TOP") == nullptr;
         bool orphan = false;

         for (SectionBlob<bits> *blob : content) {
            auto *inst = dynamic_cast<Instruction<bits> *>(blob);
            if (!inst) {
               /* An inline PIC jump table shares its function's anchor (several
                * dispatches may follow each other); any other data ends tracking. */
               if (dynamic_cast<JumpTableEntry<bits> *>(blob) == nullptr) {
                  clear_all();
               }
               prev_inst = nullptr;
               continue;
            }

            const xed_decoded_inst_t& xedd = inst->xedd;
            const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);
            const xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);

            /* Branch target: join the snapshots recorded by its sources. */
            {
               if (pass == 1) {
                  auto b = back_anchor_snap.find(inst->loc.vmaddr);
                  if (b != back_anchor_snap.end()) {
                     snapshot_for(branch_anchor_snap, b->first, b->second);
                     snapshot_for(branch_slot_snap, b->first, back_slot_snap[b->first]);
                  }
               }
               if (env.func_syms.count(inst->loc.vmaddr) != 0) { unreached = false; }
               /* the linear predecessor cannot fall through here */
               const bool dead_edge = prev_inst != nullptr &&
                  (last_flow_cat == XED_CATEGORY_RET ||
                   last_flow_cat == XED_CATEGORY_UNCOND_BR || ft_dead);
               const bool no_fallthrough = unreached || orphan || dead_edge;
               /* `unreached` is the dead edge from the noreturn call into the
                * first real instruction after it, nothing more: kept sticky, it
                * leaked through a stripped image's next function (no symbol to
                * reset it) and made every join there adopt instead of
                * intersect (Angry Birds 0x1a9f0..). Kill
                * M64_NO_PIC_UNREACHED_ONCE; guard 99_pic_anchor_unreached_pad. */
               static const bool unreached_once = std::getenv("M64_NO_PIC_UNREACHED_ONCE") == nullptr;
               if (unreached_once && cat != XED_CATEGORY_NOP && cat != XED_CATEGORY_WIDENOP) {
                  unreached = false;
               }
               auto snap = branch_anchor_snap.find(inst->loc.vmaddr);
               if (orphan_gate && !env.func_syms.empty()) {
                  const bool was_orphan = orphan;
                  orphan = no_fallthrough && snap == branch_anchor_snap.end() &&
                           prev_inst != nullptr && env.func_syms.count(inst->loc.vmaddr) == 0 &&
                           (orphan || !orphan_back_only || back_targets.count(inst->loc.vmaddr) != 0);
                  if (orphan && !was_orphan) {
                     orphan_top = inst->loc.vmaddr;
                     orphan_written.clear();
                  }
                  if (trace && orphan && !was_orphan) {
                     fprintf(stderr, "[anchor] orphan 0x%zx pass=%d regs=%zu\n",
                             (size_t)inst->loc.vmaddr, pass, anchors.size());
                  }
               }
               if (snap != branch_anchor_snap.end()) {
                  unreached = false;
                  /* a recorded edge reaches here: the code is live, so a CALL
                   * opening this block must not leave ft_dead sticky (that made
                   * its fall-through ORPHAN: Portal 2 engine
                   * CClientState::SetSignonState, a switch case opening with a
                   * call lost the `je` edge carrying the %edi anchor). */
                  ft_dead = false;
                  if (trace) {
                     fprintf(stderr, "[anchor] join 0x%zx pass=%d %s snap-regs=%zu\n",
                             (size_t)inst->loc.vmaddr, pass,
                             no_fallthrough ? "adopt" : "intersect", snap->second.size());
                  }
                  if (no_fallthrough) { anchors = snap->second; }
                  else {
                     intersect_into(anchors, snap->second);
                     /* After a CALL the fall-through edge holds nothing in the
                      * caller-saved eax/ecx/edx, so a caller-saved anchor the
                      * target uses can only come from the branch: the call never
                      * returns (Portal 2 client.dylib: EH landing pad ending in
                      * `call _Unwind_Resume`, then a `je` target that stores via
                      * the %eax anchor -> raw disp, a write into __text). */
                     static const bool call_adopt =
                        std::getenv("M64_NO_PIC_ANCHOR_CALL_ADOPT") == nullptr;
                     if (call_adopt && prev_inst != nullptr && last_flow_cat == XED_CATEGORY_CALL) {
                        for (xed_reg_enum_t r : {XED_REG_EAX, XED_REG_ECX, XED_REG_EDX}) {
                           auto it = snap->second.find(r);
                           if (it != snap->second.end()) { anchors[r] = it->second; }
                        }
                     }
                  }
                  branch_anchor_snap.erase(snap);
               }
               auto ssnap = branch_slot_snap.find(inst->loc.vmaddr);
               if (ssnap != branch_slot_snap.end()) {
                  if (no_fallthrough) { anchor_slots = ssnap->second; }
                  else { intersect_into(anchor_slots, ssnap->second); }
                  branch_slot_snap.erase(ssnap);
               }
            }

            /* `call $+0; pop %reg` establishes an anchor. Slots spilled before it
             * cannot hold it; they are the prologue's entry saves. */
            bool is_anchor_pop = false;
            if (iform == XED_IFORM_POP_GPRv_58 && prev_inst &&
                prev_inst->instbuf == opcode_t{0xe8, 0x00, 0x00, 0x00, 0x00}) {
               const xed_reg_enum_t reg = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               if (is_gpr32(reg)) {
                  anchor_slots.clear();
                  entry_save_slots = pre_anchor_saves;
                  pre_anchor_saves.clear();
                  anchors[reg] = inst->loc.vmaddr;
                  anchored_region = true;
                  is_anchor_pop = true;
                  orphan = false;   /* a fresh anchor: the state is known again */
               }
            }

            /* `call get_pc_thunk.<r>` leaves %r = the next instruction; applied
             * after the call-clobber below so a caller-saved %r survives. */
            xed_reg_enum_t thunk_anchor_reg = XED_REG_INVALID;
            std::size_t thunk_anchor_vm = 0;
            if (cat == XED_CATEGORY_CALL) {
               const ssize_t brdisp = xed_decoded_inst_get_branch_displacement(&xedd);
               if (brdisp != 0) {
                  const std::size_t after = inst->loc.vmaddr + xed_decoded_inst_get_length(&xedd);
                  auto t = pic_thunks.find(after + brdisp);
                  if (t != pic_thunks.end()) {
                     thunk_anchor_reg = t->second;
                     thunk_anchor_vm = after;
                  }
               }
            }

            /* Resolve an anchored operand, before this instruction's own writes
             * are applied (so `mov disp(%ebx),%ebx` still resolves). The anchor
             * may be the SIB base, or — at scale 1, the fields being symmetric —
             * the index (`lea 0xa4b0(%eax,%edi),%edx`; Portal 2 libsteam_api).
             * An anchored target overrides the parser's absolute-table guess. */
            /* An orphan runs pass 0 on the stale state left before its jmp.
             * Undo a pass-0 rewrite there when (1) its register came straight
             * from that stale state, (2) the loop [orphan top, last back-edge
             * source] updates the register from itself — an anchor is
             * loop-invariant, a loop variable is not — and (3) the second walk's real entry
             * state does not hold that anchor. The normal path below then
             * redoes it from the real state if it still applies. Portal 2
             * client CHudCloseCaption::Process: `xorl %ebx,%ebx` turns the
             * anchor into a loop index; the loop body (reached only by a
             * backward jae) still saw %ebx anchored and
             * `movl %eax,-0xab74(%ebp,%ebx)` became a store into the image
             * (SIGSEGV on a tagged caption). (3) alone is not enough: a
             * back-edge snapshot can merely have LOST an invariant anchor
             * (libsteam 0x499cd %esi; engine EH pad 0x16a9ad reloading it from
             * a frame slot). Only reverts, and only where the second walk
             * already runs. Kill M64_NO_PIC_ORPHAN_REVERT; guard
             * 99_pic_anchor_orphan_revert. */
            if (pass == 1 && !orphan) {
               auto o = orphan_rewrites.find(inst->loc.vmaddr);
               if (o != orphan_rewrites.end() &&
                   loop_carried.count({o->second.top, o->second.reg}) != 0) {
                  auto a = anchors.find(o->second.reg);
                  if (a == anchors.end() || a->second != o->second.anchor) {
                     if (trace) {
                        fprintf(stderr, "[anchor] orphan-revert 0x%zx %s\n",
                                (size_t)inst->loc.vmaddr, xed_reg_enum_t2str(o->second.reg));
                     }
                     inst->memdisp = nullptr;
                     inst->memdisp_offset = 0;
                     inst->pic_anchored = false;
                     inst->pic_anchor_in_index = false;
                  }
               }
            }
            if (!anchors.empty() && (inst->memdisp == nullptr || inst->memdisp_absolute)) {
               const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
               const unsigned nops = xed_decoded_inst_noperands(&xedd);
               for (unsigned i = 0; i < nops; ++i) {
                  const xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(ops, i);
                  const xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(ops, i);
                  if (!is_gpr32(basereg)) continue;
                  bool anchor_is_index = false;
                  xed_reg_enum_t anchor_reg = basereg;
                  if (anchors.find(basereg) == anchors.end()) {
                     if (!is_gpr32(indexreg) || anchors.find(indexreg) == anchors.end()) continue;
                     if (xed_decoded_inst_get_scale(ops, i) != 1) continue;
                     if (basereg == XED_REG_ESP) continue;   /* can't become a SIB index */
                     anchor_is_index = true;
                     anchor_reg = indexreg;
                  } else if (indexreg != XED_REG_INVALID) {
                     /* a second anchor in the index is ambiguous */
                     if (!is_gpr32(indexreg) || anchors.find(indexreg) != anchors.end()) continue;
                  }
                  if (xed_decoded_inst_get_memory_displacement_width(ops, i) != sizeof(uint32_t)) continue;

                  const ssize_t disp = xed_decoded_inst_get_memory_displacement(ops, i);
                  const std::size_t target = anchors[anchor_reg] + disp;
                  if (trace) {
                     fprintf(stderr, "[anchor] inst=0x%zx %s=%s anchor=0x%zx disp=0x%zx target=0x%zx iform=%s\n",
                             (size_t)inst->loc.vmaddr, anchor_is_index ? "index" : "base",
                             xed_reg_enum_t2str(anchor_reg), (size_t)anchors[anchor_reg],
                             (size_t)disp, (size_t)target, xed_iform_enum_t2str(iform));
                  }
                  SectionBlob<bits> *target_blob = env.add_placeholder(target);
                  if (target_blob == nullptr) continue;

                  /* the parser's pending resolves of the raw disp would overwrite us */
                  env.vmaddr_resolver.cancel((std::size_t)disp,
                                             (const SectionBlob<bits> **)&inst->memdisp);
                  env.vmaddr_resolver.cancel_containing((std::size_t)disp,
                                                        (const SectionBlob<bits> **)&inst->memdisp);
                  inst->memidx = i;
                  inst->memdisp = target_blob;
                  inst->pic_anchored = true;
                  inst->pic_anchor_in_index = anchor_is_index;
                  inst->memdisp_absolute = false;
                  if (orphan_revert && orphan && pass == 0 &&
                      orphan_written.count(anchor_reg) == 0) {   /* straight from the stale state */
                     orphan_rewrites[inst->loc.vmaddr] = {anchor_reg, anchors[anchor_reg], orphan_top};
                  }
                  /* an interior byte of opaque data binds to blob + offset (guard
                   * 97_pic_const_interior_field) */
                  if (env.vmaddr_in_writable_data(target) ||
                      env.vmaddr_in_readonly_opaque_data(target)) {
                     env.vmaddr_resolver.resolve_containing(
                        target, (const SectionBlob<bits> **)&inst->memdisp,
                        &inst->memdisp_offset, /*override=*/true);
                  }
                  break;
               }
            }

            /* PIC region: cancel the absolute-address heuristics (a pointer
             * immediate, or a `disp32(%base)` table capture on a non-anchor base).
             * Portal 2 `movl $0x1000,4(%esp)` (a size) aliased low __TEXT; guard
             * 96_zerofill_common_interior. */
            if (!anchors.empty() || anchored_region) {
               if (inst->imm != nullptr && inst->imm->heuristic) {
                  env.vmaddr_resolver.cancel((std::size_t)inst->imm->value,
                                             (const SectionBlob<bits> **)&inst->imm->pointee);
                  env.vmaddr_resolver.cancel_containing((std::size_t)inst->imm->value,
                                                        (const SectionBlob<bits> **)&inst->imm->pointee);
                  inst->imm->pointee = nullptr;
               }
               if (inst->memdisp_absolute && !inst->pic_anchored) {
                  const xed_operand_values_t *mops = xed_decoded_inst_operands_const(&xedd);
                  if (xed_decoded_inst_get_base_reg(mops, inst->memidx) != XED_REG_INVALID) {
                     const ssize_t mdisp = xed_decoded_inst_get_memory_displacement(mops, inst->memidx);
                     env.vmaddr_resolver.cancel((std::size_t)mdisp,
                                                (const SectionBlob<bits> **)&inst->memdisp);
                     env.vmaddr_resolver.cancel_containing((std::size_t)mdisp,
                                                           (const SectionBlob<bits> **)&inst->memdisp);
                     inst->memdisp = nullptr;
                     inst->memdisp_offset = 0;
                     inst->memdisp_absolute = false;
                  }
               }
            }

            /* Frame-slot spill/reload (`mov %reg,disp(%ebp|%esp)` and back). */
            xed_reg_enum_t anchor_keep = XED_REG_INVALID;   /* (re)established here */
            {
               const xed_operand_values_t *ops2 = xed_decoded_inst_operands_const(&xedd);
               const xed_reg_enum_t mbase = xed_decoded_inst_get_base_reg(ops2, 0);
               if ((mbase == XED_REG_EBP || mbase == XED_REG_ESP) &&
                   xed_decoded_inst_get_index_reg(ops2, 0) == XED_REG_INVALID &&
                   xed_decoded_inst_number_of_memory_operands(&xedd) == 1) {
                  const Slot slot{mbase, (ssize_t)xed_decoded_inst_get_memory_displacement(ops2, 0)};
                  const xed_reg_enum_t reg = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
                  if (iform == XED_IFORM_MOV_MEMv_GPRv) {               /* spill */
                     entry_save_slots.erase(slot);
                     auto a = anchors.find(reg);
                     if (a != anchors.end()) {
                        anchor_slots[slot] = a->second;
                        pre_anchor_saves.erase(slot);
                     } else {
                        anchor_slots.erase(slot);
                        if (is_gpr32(reg)) { pre_anchor_saves[slot] = reg; }
                        else { pre_anchor_saves.erase(slot); }
                     }
                  } else if (iform == XED_IFORM_MOV_GPRv_MEMv && is_gpr32(reg)) {   /* reload */
                     auto s = anchor_slots.find(slot);
                     auto es = entry_save_slots.find(slot);
                     /* the epilogue restore of the callee-saved anchor register:
                      * keep the anchor for blocks after the epilogue (Civ IV
                      * Python 2.6 post-epilogue case body) */
                     const bool entry_restore =
                        entry_save_gate && es != entry_save_slots.end() && es->second == reg &&
                        (reg == XED_REG_EBX || reg == XED_REG_ESI || reg == XED_REG_EDI);
                     if (s != anchor_slots.end()) {
                        anchors[reg] = s->second;
                        anchor_keep = reg;
                     } else if (entry_restore) {
                        anchor_keep = reg;
                     } else {
                        anchors.erase(reg);
                     }
                  }
               }
            }

            /* Any other definition of a register ends its anchor — including
             * sub-register writes — except the pops described above. */
            if (!is_anchor_pop && cat != XED_CATEGORY_POP && !anchors.empty()) {
               for_each_written_gpr32(xedd, [&](xed_reg_enum_t r) {
                  if (r != anchor_keep) { anchors.erase(r); }
               });
            }
            if (orphan && pass == 0) {
               for_each_written_gpr32(xedd, [&](xed_reg_enum_t r) { orphan_written.insert(r); });
            }

            /* `mov %src,%dst` copies the anchor (the kill above already cleared %dst). */
            if (iform == XED_IFORM_MOV_GPRv_GPRv_89 || iform == XED_IFORM_MOV_GPRv_GPRv_8B) {
               const xed_reg_enum_t mdst = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               const xed_reg_enum_t msrc = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
               if (is_gpr32(mdst) && mdst != msrc) {
                  auto a = anchors.find(msrc);
                  if (a != anchors.end()) { anchors[mdst] = a->second; }
                  else { anchors.erase(mdst); }
               }
            }

            /* Forward targets at or behind us are reached (or were never real). */
            pending_forward_targets.erase(pending_forward_targets.begin(),
                                          pending_forward_targets.upper_bound(inst->loc.vmaddr));

            /* Record intra-function forward branches: in-section, within 64 KB (a
             * farther jmp is a tail call; tracking it leaked an anchor across all
             * of iPhoto's __text). Calls leave the function and are not tracked. */
            if (cat == XED_CATEGORY_COND_BR || cat == XED_CATEGORY_UNCOND_BR) {
               const ssize_t brdisp = xed_decoded_inst_get_branch_displacement(&xedd);
               const std::size_t tgt =
                  inst->loc.vmaddr + xed_decoded_inst_get_length(&xedd) + brdisp;
               if (orphan) {
                  /* unknown state: constrains no join */
               } else if (brdisp > 0 && tgt < sect.addr + sect.size &&
                   tgt - inst->loc.vmaddr <= 0x10000) {
                  pending_forward_targets.insert(tgt);
                  snapshot_for(branch_anchor_snap, tgt, anchors);
                  snapshot_for(branch_slot_snap, tgt, anchor_slots);
               } else if (brdisp < 0) {
                  record_back(tgt, inst->loc.vmaddr);
               }
            }
            /* The case bodies of a claimed PIC jump table are branch targets too. */
            auto jt = env.pic_switch_targets.find(inst->loc.vmaddr);
            if (jt != env.pic_switch_targets.end() && !orphan) {
               for (const std::size_t tgt : jt->second) {
                  if (tgt <= inst->loc.vmaddr) { record_back(tgt, inst->loc.vmaddr); continue; }
                  snapshot_for(branch_anchor_snap, tgt, anchors);
                  snapshot_for(branch_slot_snap, tgt, anchor_slots);
               }
            }

            /* Control transfers. int3 is transparent (its fall-through is dead
             * code and an empty snapshot from it would poison the join). */
            const bool is_pic_call_zero =
               iform == XED_IFORM_CALL_NEAR_RELBRz &&
               xed_decoded_inst_get_branch_displacement(&xedd) == 0;
            if (cat == XED_CATEGORY_RET) {
               if (pending_forward_targets.empty()) { clear_all(); }
            } else if ((cat == XED_CATEGORY_INTERRUPT &&
                        xed_decoded_inst_get_iclass(&xedd) != XED_ICLASS_INT3) ||
                       cat == XED_CATEGORY_SYSCALL || cat == XED_CATEGORY_SYSRET) {
               clear_all();
            } else if (cat == XED_CATEGORY_CALL && !is_pic_call_zero) {
               anchors.erase(XED_REG_EAX);
               anchors.erase(XED_REG_ECX);
               anchors.erase(XED_REG_EDX);
            }

            if (thunk_anchor_reg != XED_REG_INVALID) {
               anchors[thunk_anchor_reg] = thunk_anchor_vm;
               anchored_region = true;
               orphan = false;
            }
            if (noreturn_gate && cat == XED_CATEGORY_CALL && !env.func_syms.empty() &&
                env.noreturn_stubs.count(inst->loc.vmaddr + xed_decoded_inst_get_length(&xedd) +
                                         xed_decoded_inst_get_branch_displacement(&xedd)) != 0) {
               unreached = true;
            }

            if (cat != XED_CATEGORY_NOP && cat != XED_CATEGORY_WIDENOP) {
               last_flow_cat = cat;
               if (cat == XED_CATEGORY_RET || cat == XED_CATEGORY_UNCOND_BR) {
                  ft_dead = true;
               } else if (cat != XED_CATEGORY_CALL) {
                  ft_dead = false;
               }
            }
            prev_inst = inst;
         }
      }
   }

   /* Canonical 32-bit name of a GPR, so `jmp rax` and `add eax,ebx` compare
    * equal; r11d (the transform's scratch table base) maps to r11. */
   static xed_reg_enum_t jt_norm32(xed_reg_enum_t r) {
      switch (r) {
      case XED_REG_RAX: case XED_REG_EAX: return XED_REG_EAX;
      case XED_REG_RBX: case XED_REG_EBX: return XED_REG_EBX;
      case XED_REG_RCX: case XED_REG_ECX: return XED_REG_ECX;
      case XED_REG_RDX: case XED_REG_EDX: return XED_REG_EDX;
      case XED_REG_RSI: case XED_REG_ESI: return XED_REG_ESI;
      case XED_REG_RDI: case XED_REG_EDI: return XED_REG_EDI;
      case XED_REG_RBP: case XED_REG_EBP: return XED_REG_EBP;
      case XED_REG_RSP: case XED_REG_ESP: return XED_REG_ESP;
      case XED_REG_R11D: return XED_REG_R11;
      default: return r;
      }
   }

   /* Does the instruction WRITE its REG0? (For a store, REG0 is the source.) */
   static bool jt_reg0_written(const xed_decoded_inst_t *xedd) {
      const xed_inst_t *xi = xed_decoded_inst_inst(xedd);
      if (xi == nullptr) { return true; }
      for (unsigned i = 0; i < xed_inst_noperands(xi); ++i) {
         const xed_operand_t *op = xed_inst_operand(xi, i);
         if (xed_operand_name(op) == XED_OPERAND_REG0) {
            return xed_operand_written(op);
         }
      }
      return true;
   }

   /*
    * PIC relative-offset switch tables (see JumpTableEntry), found before the
    * linear sweep so their slots are emitted as relocatable entries instead of
    * being decoded as code. Matches the dispatch
    *
    *     lea  %tbl,[anchor+d]          (or folded: [anchor + idx*4 + d])
    *     mov  %t,[%tbl + idx*4]
    *     add  %t,%anchor               (or fused: add %anchor,[%anchor+idx*4+d])
    *     jmp  *%t
    *
    * on both the i386 input and the translated x86_64 image the later stages
    * re-parse, where the anchor is the `lea r11,[rip+ret]; ...; mov %reg,[rsp]`
    * sequence call_op emits and the table base is `lea r11,[rip+d]`.
    *
    * Anchor tracking follows DetectPicAnchoredDisps' rules (register copies,
    * frame-slot spills, branch snapshots, RET ends a function only with no
    * pending forward target) plus a reset at every function symbol. A table is
    * auto-sized: it ends at its first case body, the next function symbol, or
    * the next table; every entry must land inside the section.
    */
   template <Bits bits>
   void Section<bits>::DetectJumpTables(const Image& img, ParseEnv<bits>& env) {
      if (parser != TextParser) {
         return;
      }

      const std::size_t sect_lo = sect.addr;
      const std::size_t sect_hi = sect.addr + sect.size;
      const bool trace = std::getenv("MACHO_TRACE_JUMPTABLE") != nullptr;
      if (trace) {
         fprintf(stderr, "[jumptable] pass bits=%d sect=0x%zx\n",
                 bits == Bits::M32 ? 32 : 64, (size_t)sect_lo);
      }
      const bool jt_spill = std::getenv("M64_NO_JT_SPILL_SLOTS") == nullptr;   /* guard OFF arm */
      const bool jt_esp_track = std::getenv("M64_NO_JT_ESP_TRACK") == nullptr; /* guard OFF arm */

      using RegMap = std::unordered_map<xed_reg_enum_t, std::size_t>;
      RegMap anchors;                                   /* reg -> anchor vmaddr */
      RegMap tbl_addr;                                  /* reg -> table base */
      /* reg -> (table base, anchor); anchor 0 until the `add` supplies it */
      std::unordered_map<xed_reg_enum_t, std::pair<std::size_t, std::size_t>> tbl_val;
      /* Frame slots holding a table base / an anchor, keyed (base reg, disp);
       * an %esp slot is keyed by disp + esp_off so it survives the pushes
       * around a call (PvZ libbass). */
      using SlotKey = std::pair<int, ssize_t>;
      std::map<SlotKey, std::size_t> stack_tbl, stack_anchor;
      ssize_t esp_off = 0;
      const auto slot_key = [&](const xed_operand_values_t *o) {
         const xed_reg_enum_t b = jt_norm32(xed_decoded_inst_get_base_reg(o, 0));
         const ssize_t d = xed_decoded_inst_get_memory_displacement(o, 0);
         return SlotKey{(int)b, b == XED_REG_ESP ? d + esp_off : d};
      };
      auto drop_slots_of = [](std::map<SlotKey, std::size_t>& m, xed_reg_enum_t base) {
         for (auto i = m.begin(); i != m.end(); ) {
            i = (i->first.first == (int)base) ? m.erase(i) : std::next(i);
         }
      };
      auto frame_operand = [&](const xed_operand_values_t *o) {
         const xed_reg_enum_t b = jt_norm32(xed_decoded_inst_get_base_reg(o, 0));
         return xed_decoded_inst_get_index_reg(o, 0) == XED_REG_INVALID &&
                (b == XED_REG_EBP || b == XED_REG_ESP);
      };

      std::set<std::size_t> pending_targets;
      std::map<std::size_t, RegMap> anchor_snap;
      /* %ebp-keyed frame slots per forward-branch target, joined like the
       * registers: an early epilogue's `pop %ebp` drops every slot on the
       * linear walk, but a block entered by a branch from before it still has
       * them (Portal 2 client vgui::Panel::OnMessage: anchor spilled to
       * -0x10(%ebp), `je` past a mid-function ret to a dispatch that reloads
       * it). %esp-keyed slots depend on the path's esp_off: not snapshotted.
       * Kill M64_NO_JT_SLOT_SNAP; guard 99_jt_slot_snap_epilogue. */
      static const bool slot_snap_on = std::getenv("M64_NO_JT_SLOT_SNAP") == nullptr;
      using SlotMap = std::map<SlotKey, std::size_t>;
      struct SlotSnap { SlotMap tbl, anchor; };
      std::map<std::size_t, SlotSnap> slot_snap;
      const auto ebp_only = [](const SlotMap& m) {
         SlotMap r;
         for (const auto& kv : m) { if (kv.first.first == (int)XED_REG_EBP) { r.insert(kv); } }
         return r;
      };
      /* table base -> (entry count, anchor), and -> its dispatch vmaddr */
      std::map<std::size_t, std::pair<std::size_t, std::size_t>> tables;
      std::map<std::size_t, std::size_t> table_dispatch;
      xed_category_enum_t prev_cat = XED_CATEGORY_INVALID;
      static const bool nop_keeps_dead = std::getenv("M64_NO_DEAD_ENTRY_EVEN") == nullptr;
      bool prev_call0 = false;     /* previous insn was `call $+0` */
      std::size_t pend_r11 = 0;    /* target of the last `lea r11,[rip+d]` (M64 call dance) */

      auto decode_at = [&](std::size_t off, xed_decoded_inst_t& xd) {
         xed_decoded_inst_zero_set_mode(&xd, &Instruction<bits>::dstate());
         xed_decoded_inst_set_input_chip(&xd, XED_CHIP_INVALID);
         return xed_decode(&xd, &img.at<uint8_t>(off), img.size() - off) == XED_ERROR_NONE;
      };
      /* An instruction that straddles a function symbol is padding decoded
       * into the next entry (the TextParser sweep resyncs the same way). */
      auto straddles_symbol = [&](std::size_t vm, unsigned len) {
         if (len <= 1) { return false; }
         auto ns = env.func_syms.upper_bound(vm);
         return ns != env.func_syms.end() && *ns < vm + len;
      };

      /* GCC PIC thunks: `mov (%esp),%reg; ret` — or, translated, the
       * `mov r11d,[rsp]` that begins the translated return. */
      std::unordered_map<std::size_t, xed_reg_enum_t> pic_thunks;
      {
         std::size_t pit = sect.offset, pvm = sect.addr;
         const std::size_t pend = sect.offset + sect.size;
         std::size_t mvm = 0;
         xed_reg_enum_t mreg = XED_REG_INVALID;
         while (pit < pend) {
            xed_decoded_inst_t xd;
            if (!decode_at(pit, xd)) { mreg = XED_REG_INVALID; ++pit; ++pvm; continue; }
            const unsigned l = xed_decoded_inst_get_length(&xd);
            if (straddles_symbol(pvm, l)) { mreg = XED_REG_INVALID; ++pit; ++pvm; continue; }
            const xed_operand_values_t *o = xed_decoded_inst_operands_const(&xd);
            const bool rsp0 =
               xed_decoded_inst_get_iform_enum(&xd) == XED_IFORM_MOV_GPRv_MEMv &&
               xed_decoded_inst_number_of_memory_operands(&xd) == 1 &&
               (xed_decoded_inst_get_base_reg(o, 0) == XED_REG_ESP ||
                xed_decoded_inst_get_base_reg(o, 0) == XED_REG_RSP) &&
               xed_decoded_inst_get_index_reg(o, 0) == XED_REG_INVALID &&
               xed_decoded_inst_get_memory_displacement(o, 0) == 0;
            const bool thunk_ret =
               xed_decoded_inst_get_category(&xd) == XED_CATEGORY_RET ||
               (bits == Bits::M64 && rsp0 &&
                xed_decoded_inst_get_reg(&xd, XED_OPERAND_REG0) == XED_REG_R11D);
            if (mreg != XED_REG_INVALID && thunk_ret) {
               pic_thunks[mvm] = mreg;
            }
            mreg = XED_REG_INVALID;
            if (rsp0) {
               const xed_reg_enum_t d = xed_decoded_inst_get_reg(&xd, XED_OPERAND_REG0);
               if (is_gpr32(d)) { mvm = pvm; mreg = d; }
            }
            pit += l; pvm += l;
         }
      }
      /* Cross-section named thunks (i386 only: the keys are i386 vmaddrs). */
      if constexpr (bits == Bits::M32) {
         for (const auto& kv : env.pic_thunks) {
            const xed_reg_enum_t r = gpr32_from_enc(kv.second);
            if (r != XED_REG_INVALID) { pic_thunks.emplace(kv.first, r); }
         }
      }

      /* Backward branches are joined on a second walk (see DetectPicAnchoredDisps):
       * Portal 2 server CUtlBuffer::VaScanf dispatches off an anchor that only
       * a later block reloads, then `jbe`s back. Re-claiming a table is
       * idempotent. Kill M64_NO_PIC_BACK_EDGE; guard 99_jt_back_edge_join. */
      static const bool back_edge = std::getenv("M64_NO_PIC_BACK_EDGE") == nullptr;
      std::map<std::size_t, RegMap> back_snap;
      for (int pass = 0; pass < 2; ++pass) {
         if (pass == 1) {
            if (!pic_any_nonempty(back_snap)) { break; }
            anchors.clear(); tbl_addr.clear(); tbl_val.clear(); pend_r11 = 0;
            stack_tbl.clear(); stack_anchor.clear(); esp_off = 0;
            pending_targets.clear(); anchor_snap.clear(); slot_snap.clear();
            prev_cat = XED_CATEGORY_INVALID; prev_call0 = false;
         }
         std::size_t it = sect.offset;
         std::size_t vmaddr = sect.addr;
         const std::size_t end = sect.offset + sect.size;
         while (it < end) {
            xed_decoded_inst_t xedd;
            const bool ok = decode_at(it, xedd);
            if (!ok || straddles_symbol(vmaddr, xed_decoded_inst_get_length(&xedd))) {
               tbl_addr.clear(); tbl_val.clear(); prev_call0 = false;
               prev_cat = XED_CATEGORY_INVALID;
               ++it; ++vmaddr;
               continue;
            }
            const unsigned len = xed_decoded_inst_get_length(&xedd);
            const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);
            const xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
            const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
            const xed_reg_enum_t reg0raw = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            const xed_reg_enum_t reg0 = jt_norm32(reg0raw);

            /* A function symbol starts with no live anchor. */
            if (env.func_syms.count(vmaddr) != 0) {
               anchors.clear(); tbl_addr.clear(); tbl_val.clear(); pend_r11 = 0;
               stack_tbl.clear(); stack_anchor.clear(); pending_targets.clear();
               anchor_snap.clear(); slot_snap.clear();
               prev_call0 = false;
            }
            if (pass == 1) {
               auto b = back_snap.find(vmaddr);
               if (b != back_snap.end()) { snapshot_for(anchor_snap, vmaddr, b->second); }
            }
            if (!anchor_snap.empty()) {
               auto sn = anchor_snap.find(vmaddr);
               if (sn != anchor_snap.end()) {
                  if (prev_cat == XED_CATEGORY_RET || prev_cat == XED_CATEGORY_UNCOND_BR) {
                     anchors = sn->second;
                  } else {
                     intersect_into(anchors, sn->second);
                  }
               }
               anchor_snap.erase(anchor_snap.begin(), anchor_snap.upper_bound(vmaddr));
            }
            if (!slot_snap.empty()) {
               auto sn = slot_snap.find(vmaddr);
               if (sn != slot_snap.end()) {
                  if (prev_cat == XED_CATEGORY_RET || prev_cat == XED_CATEGORY_UNCOND_BR) {
                     drop_slots_of(stack_tbl, XED_REG_EBP);
                     drop_slots_of(stack_anchor, XED_REG_EBP);
                     stack_tbl.insert(sn->second.tbl.begin(), sn->second.tbl.end());
                     stack_anchor.insert(sn->second.anchor.begin(), sn->second.anchor.end());
                  } else {
                     /* the fall-through keeps an %ebp slot only if the branch agrees */
                     for (SlotMap *m : {&stack_anchor, &stack_tbl}) {
                        const SlotMap& b = (m == &stack_anchor) ? sn->second.anchor : sn->second.tbl;
                        for (auto i = m->begin(); i != m->end(); ) {
                           auto f = b.find(i->first);
                           const bool keep = i->first.first != (int)XED_REG_EBP ||
                                             (f != b.end() && f->second == i->second);
                           i = keep ? std::next(i) : m->erase(i);
                        }
                     }
                  }
               }
               slot_snap.erase(slot_snap.begin(), slot_snap.upper_bound(vmaddr));
            }

            bool sets_state = false;
            bool spill_write = false;

            if (iform == XED_IFORM_POP_GPRv_58 && prev_call0 && is_gpr32(reg0)) {
               /* i386 anchor: `call $+0; pop %reg` */
               anchors[reg0] = vmaddr;
               tbl_addr.erase(reg0); tbl_val.erase(reg0);
               sets_state = true;
            } else if (iform == XED_IFORM_MOV_GPRv_MEMv && pend_r11 != 0 && vmaddr == pend_r11 &&
                       xed_decoded_inst_get_base_reg(ops, 0) == XED_REG_RSP &&
                       xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
                       is_gpr32(reg0)) {
               /* translated anchor: the `mov %reg,[rsp]` the translated
                * `call $+0` returns to */
               anchors[reg0] = pend_r11;
               tbl_addr.erase(reg0); tbl_val.erase(reg0);
               sets_state = true;
            } else if ((iform == XED_IFORM_MOV_GPRv_GPRv_89 ||
                        iform == XED_IFORM_MOV_GPRv_GPRv_8B) && is_gpr32(reg0)) {
               /* the anchor follows a copy (an overwrite does not clear it here) */
               const xed_reg_enum_t src = jt_norm32(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1));
               auto a = anchors.find(src);
               if (src != reg0 && a != anchors.end()) { anchors[reg0] = a->second; }
            } else if (iform == XED_IFORM_LEA_GPRv_AGEN &&
                       (is_gpr32(reg0) || reg0raw == XED_REG_R11)) {
               /* table base: `lea [rip+d]` (M64) or `lea [anchor+d]` landing in
                * this section — an anchor-relative lea usually reaches a string
                * or global, and recording that as a table poisons the load that
                * follows (Portal 2 libtogl GLMDecode) */
               const xed_reg_enum_t base = xed_decoded_inst_get_base_reg(ops, 0);
               const ssize_t disp = xed_decoded_inst_get_memory_displacement(ops, 0);
               if (xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID) {
                  if (base == XED_REG_RIP) {
                     tbl_addr[reg0] = vmaddr + len + disp;
                     tbl_val.erase(reg0);
                     sets_state = true;
                  } else {
                     auto a = anchors.find(jt_norm32(base));
                     const std::size_t cand = a != anchors.end() ? (std::size_t)((ssize_t)a->second + disp) : 0;
                     if (a != anchors.end() && cand >= sect_lo && cand < sect_hi) {
                        tbl_addr[reg0] = cand;
                        tbl_val.erase(reg0);
                        sets_state = true;
                     }
                  }
               }
            } else if (jt_spill && iform == XED_IFORM_MOV_MEMv_GPRv && frame_operand(ops)) {
               /* spill of a table base or anchor to a frame slot */
               const SlotKey key = slot_key(ops);
               auto tb = tbl_addr.find(reg0);
               if (tb != tbl_addr.end()) { stack_tbl[key] = tb->second; }
               else { stack_tbl.erase(key); }
               auto an = anchors.find(reg0);
               if (an != anchors.end()) { stack_anchor[key] = an->second; }
               else { stack_anchor.erase(key); }
               spill_write = true;
            } else if (jt_spill && iform == XED_IFORM_MOV_GPRv_MEMv && is_gpr32(reg0) &&
                       frame_operand(ops) &&
                       (stack_tbl.count(slot_key(ops)) || stack_anchor.count(slot_key(ops)))) {
               /* reload from such a slot */
               const SlotKey key = slot_key(ops);
               auto tb = stack_tbl.find(key);
               if (tb != stack_tbl.end()) { tbl_addr[reg0] = tb->second; }
               else { tbl_addr.erase(reg0); }
               auto an = stack_anchor.find(key);
               if (an != stack_anchor.end()) { anchors[reg0] = an->second; }
               else { anchors.erase(reg0); }
               tbl_val.erase(reg0);
               sets_state = true;
            } else if (iform == XED_IFORM_MOV_GPRv_MEMv && is_gpr32(reg0)) {
               /* `mov %t,[%tbl + idx*s]`, or the folded `[anchor + idx*4 + d]` */
               const xed_reg_enum_t base = jt_norm32(xed_decoded_inst_get_base_reg(ops, 0));
               const xed_reg_enum_t index = jt_norm32(xed_decoded_inst_get_index_reg(ops, 0));
               /* The table base may sit in the index field only at scale 1 (the
                * fields are then symmetric); a register scaled by 4 is the case
                * index. Portal 2 engine MXR_LoadAllSoundMixers: a stale anchor
                * copy in %esi (the loop counter) made `lea -1(%esi),%eax` look
                * like a table base, so `mov 0x82b(%ebx,%eax,4)` took %eax as the
                * table and the real dispatch was never claimed (SIGILL
                * mid-instruction). Kill M64_NO_JT_SCALED_INDEX_GUARD; guard
                * 99_jt_scaled_index_not_base. */
               static const bool scaled_index_any =
                  std::getenv("M64_NO_JT_SCALED_INDEX_GUARD") != nullptr;
               auto tb = tbl_addr.find(base);
               if (tb == tbl_addr.end() &&
                   (scaled_index_any || xed_operand_values_get_scale(ops) == 1)) {
                  tb = tbl_addr.find(index);
               }
               if (tb != tbl_addr.end()) {
                  tbl_val[reg0] = { tb->second, 0 };
                  tbl_addr.erase(reg0);
                  sets_state = true;
               } else if (index != XED_REG_INVALID && xed_operand_values_get_scale(ops) == 4) {
                  auto a = anchors.find(base);
                  if (a != anchors.end()) {
                     tbl_val[reg0] = { a->second + xed_decoded_inst_get_memory_displacement(ops, 0), 0 };
                     tbl_addr.erase(reg0);
                     sets_state = true;
                  }
               }
            } else if (iform == XED_IFORM_ADD_GPRv_MEMv && is_gpr32(reg0)) {
               /* fused load+add: `add %anchor,[%anchor + idx*4 + d]` (i386), or
                * `add %anchor,[%tbl + idx*4]` (translated, table in r11) */
               const xed_reg_enum_t mbase = jt_norm32(xed_decoded_inst_get_base_reg(ops, 0));
               const xed_reg_enum_t midx = jt_norm32(xed_decoded_inst_get_index_reg(ops, 0));
               auto a = anchors.find(reg0);
               const bool shape = a != anchors.end() && midx != XED_REG_INVALID && midx != reg0 &&
                                  xed_operand_values_get_scale(ops) == 4;
               if (shape && mbase == reg0) {
                  const std::size_t tbl =
                     (std::size_t)((ssize_t)a->second + xed_decoded_inst_get_memory_displacement(ops, 0));
                  if (tbl >= sect_lo && tbl < sect_hi) {
                     tbl_val[reg0] = { tbl, a->second };
                     tbl_addr.erase(reg0);
                     sets_state = true;
                  }
               } else if (shape && xed_decoded_inst_get_memory_displacement(ops, 0) == 0) {
                  auto tb = tbl_addr.find(mbase);
                  if (tb != tbl_addr.end()) {
                     tbl_val[reg0] = { tb->second, a->second };
                     tbl_addr.erase(reg0);
                     sets_state = true;
                  }
               }
            } else if (iform == XED_IFORM_ADD_GPRv_GPRv_01 || iform == XED_IFORM_ADD_GPRv_GPRv_03) {
               /* `add %t,%anchor`: the entry becomes a case target */
               auto tv = tbl_val.find(reg0);
               auto a = anchors.find(jt_norm32(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1)));
               if (tv != tbl_val.end() && a != anchors.end()) {
                  tv->second.second = a->second;
                  sets_state = true;
               }
            } else if (iform == XED_IFORM_JMP_GPRv) {
               auto tv = tbl_val.find(reg0);
               if (tv != tbl_val.end() && tv->second.second != 0) {
                  const std::size_t table_base = tv->second.first;
                  const std::size_t anchor = tv->second.second;
                  std::size_t min_target = sect_hi;
                  auto ns = env.func_syms.upper_bound(table_base);
                  if (ns != env.func_syms.end() && *ns < min_target) { min_target = *ns; }
                  /* A case body lies inside the dispatching function. Without
                   * this, the padding after a table read as one more entry: Portal
                   * 2 server `cmpl $3 ; ja` table (dispatch 0x24aed5) gained a 5th
                   * "case" from its `nopl` alignment, 4 MB away, and that target's
                   * empty-anchor snapshot erased %esi mid-function at 0x64c800 ->
                   * raw PIC stores into __TEXT. Bounded by symbols; a stripped
                   * image keeps the section bounds. Kill M64_NO_JT_CASE_IN_FUNC;
                   * guard 99_jt_case_in_func. */
                  static const bool case_anywhere = std::getenv("M64_NO_JT_CASE_IN_FUNC") != nullptr;
                  std::size_t fn_lo = sect_lo, fn_hi = sect_hi;
                  if (!case_anywhere) {
                     auto nx = env.func_syms.upper_bound(vmaddr);
                     if (nx != env.func_syms.end()) { fn_hi = std::min(fn_hi, *nx); }
                     if (nx != env.func_syms.begin()) { fn_lo = std::max(fn_lo, *std::prev(nx)); }
                  }
                  std::size_t i = 0;
                  for (; ; ++i) {
                     const std::size_t slot = table_base + i * 4;
                     if (slot + 4 > sect_hi || slot >= min_target) { break; }
                     const int32_t raw = (int32_t)img.at<uint32_t>(sect.offset + (slot - sect.addr));
                     const std::size_t target = anchor + raw;
                     if (target < sect_lo || target >= sect_hi) { break; }
                     if (target < fn_lo || target >= fn_hi) { break; }
                     if (target > table_base && target < min_target) {
                        min_target = target;
                     }
                  }
                  if (i >= 2) {
                     for (std::size_t k = 0; k < i; ++k) {
                        env.jump_table_slots[table_base + k * 4] = anchor;
                     }
                     auto& tr = tables[table_base];
                     if (i > tr.first) { tr = { i, anchor }; }
                     table_dispatch[table_base] = vmaddr;
                     if (trace) {
                        fprintf(stderr, "[jumptable] dispatch@0x%zx anchor=0x%zx table=0x%zx count=%zu\n",
                                (size_t)vmaddr, (size_t)anchor, (size_t)table_base, (size_t)i);
                     }
                     /* An inline table right after the jmp: step over it so the
                      * walk stays aligned for the rest of the function. */
                     const std::size_t table_end = table_base + i * 4;
                     if (table_base >= vmaddr && table_base - vmaddr < 64 && table_end > vmaddr) {
                        it = sect.offset + (table_end - sect.addr);
                        vmaddr = table_end;
                        tbl_addr.erase(reg0);
                        tbl_val.erase(reg0);
                        prev_call0 = false;
                        prev_cat = cat;
                        continue;
                     }
                  }
               }
            }

            /* A write that did not (re)define table state clears it. */
            if (!sets_state && is_gpr32(reg0) && (!jt_spill || jt_reg0_written(&xedd))) {
               tbl_addr.erase(reg0);
               tbl_val.erase(reg0);
            }

            if (jt_spill && (!stack_tbl.empty() || !stack_anchor.empty())) {
               /* a slot dies when something else writes it ... */
               if (!spill_write && xed_decoded_inst_number_of_memory_operands(&xedd) > 0 &&
                   xed_decoded_inst_mem_written(&xedd, 0) && frame_operand(ops)) {
                  const SlotKey dead = slot_key(ops);
                  stack_tbl.erase(dead);
                  stack_anchor.erase(dead);
               }
               /* ... when the frame pointer is redefined ... */
               if (iform == XED_IFORM_LEAVE || (reg0 == XED_REG_EBP && jt_reg0_written(&xedd))) {
                  drop_slots_of(stack_tbl, XED_REG_EBP);
                  drop_slots_of(stack_anchor, XED_REG_EBP);
               }
               /* ... and %esp slots shift with pushes/pops/`add|sub $imm,%esp`
                * (a returning CALL is net-zero; only `call $+0` leaves a push). */
               const xed_iclass_enum_t ic = xed_decoded_inst_get_iclass(&xedd);
               const bool call0 = iform == XED_IFORM_CALL_NEAR_RELBRz &&
                                  xed_decoded_inst_get_branch_displacement(&xedd) == 0;
               const bool esp_written = reg0 == XED_REG_ESP && jt_reg0_written(&xedd);
               bool esp_kill;
               if (!jt_esp_track) {
                  esp_kill = cat == XED_CATEGORY_PUSH || cat == XED_CATEGORY_POP ||
                             cat == XED_CATEGORY_CALL || cat == XED_CATEGORY_RET ||
                             iform == XED_IFORM_LEAVE || esp_written;
               } else if (ic == XED_ICLASS_PUSH || call0) {
                  esp_off -= 4; esp_kill = false;
               } else if (ic == XED_ICLASS_POP) {
                  esp_off += 4; esp_kill = false;
               } else if (ic == XED_ICLASS_LEA && reg0 == XED_REG_ESP &&
                          jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)) == XED_REG_ESP &&
                          xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID) {
                  /* translated push/pop: lea rsp,[rsp -/+ 4] */
                  esp_off += xed_decoded_inst_get_memory_displacement(ops, 0);
                  esp_kill = false;
               } else if (bits == Bits::M64 && cat == XED_CATEGORY_UNCOND_BR &&
                          xed_decoded_inst_get_branch_displacement(&xedd) != 0 &&
                          pend_r11 == vmaddr + len) {
                  /* translated call: the callee pops the return address */
                  esp_off += 4;
                  esp_kill = false;
               } else if ((ic == XED_ICLASS_ADD || ic == XED_ICLASS_SUB) && reg0 == XED_REG_ESP &&
                          xed_decoded_inst_get_immediate_width(&xedd) != 0) {
                  const ssize_t imm = (ssize_t)xed_decoded_inst_get_signed_immediate(&xedd);
                  esp_off += (ic == XED_ICLASS_ADD) ? imm : -imm;
                  esp_kill = false;
               } else {
                  esp_kill = cat == XED_CATEGORY_PUSH || cat == XED_CATEGORY_POP ||
                             cat == XED_CATEGORY_RET || iform == XED_IFORM_LEAVE || esp_written;
               }
               if (esp_kill) {
                  esp_off = 0;
                  drop_slots_of(stack_tbl, XED_REG_ESP);
                  drop_slots_of(stack_anchor, XED_REG_ESP);
               }
            }

            /* Anchor lifetime (DetectPicAnchoredDisps' rules, int3 transparent). */
            pending_targets.erase(pending_targets.begin(), pending_targets.upper_bound(vmaddr));
            if (cat == XED_CATEGORY_COND_BR || cat == XED_CATEGORY_UNCOND_BR) {
               const ssize_t bd = xed_decoded_inst_get_branch_displacement(&xedd);
               const std::size_t tgt = vmaddr + len + bd;
               if (bd > 0 && tgt < sect_hi && tgt - vmaddr <= 0x10000) {
                  pending_targets.insert(tgt);
                  snapshot_for(anchor_snap, tgt, anchors);
                  if (slot_snap_on && jt_spill) {
                     SlotSnap cur{ ebp_only(stack_tbl), ebp_only(stack_anchor) };
                     auto ex = slot_snap.find(tgt);
                     if (ex == slot_snap.end()) { slot_snap.emplace(tgt, std::move(cur)); }
                     else {
                        intersect_into(ex->second.tbl, cur.tbl);
                        intersect_into(ex->second.anchor, cur.anchor);
                     }
                  }
               } else if (pass == 0 && back_edge && bd < 0 && tgt >= sect_lo &&
                          vmaddr - tgt <= 0x10000 && pic_same_function(env.func_syms, tgt, vmaddr)) {
                  snapshot_for(back_snap, tgt, anchors);
               }
            }
            const bool is_pic_call0 =
               iform == XED_IFORM_CALL_NEAR_RELBRz &&
               xed_decoded_inst_get_branch_displacement(&xedd) == 0;
            if ((cat == XED_CATEGORY_RET && pending_targets.empty()) ||
                (cat == XED_CATEGORY_INTERRUPT &&
                 xed_decoded_inst_get_iclass(&xedd) != XED_ICLASS_INT3) ||
                cat == XED_CATEGORY_SYSCALL || cat == XED_CATEGORY_SYSRET) {
               anchors.clear(); tbl_addr.clear(); tbl_val.clear(); pend_r11 = 0;
               stack_tbl.clear(); stack_anchor.clear();
            } else if (cat == XED_CATEGORY_CALL && !is_pic_call0) {
               anchors.erase(XED_REG_EAX);
               anchors.erase(XED_REG_ECX);
               anchors.erase(XED_REG_EDX);
               tbl_addr.clear(); tbl_val.clear();
            }

            /* `call get_pc_thunk` (or its translated jmp) leaves %r = return address */
            const ssize_t bd = xed_decoded_inst_get_branch_displacement(&xedd);
            const bool is_call = cat == XED_CATEGORY_CALL ||
               (bits == Bits::M64 && cat == XED_CATEGORY_UNCOND_BR && pend_r11 == vmaddr + len);
            if (is_call && bd != 0) {
               auto t = pic_thunks.find(vmaddr + len + bd);
               if (t != pic_thunks.end()) { anchors[t->second] = vmaddr + len; }
            }

            prev_call0 = iform == XED_IFORM_CALL_NEAR_RELBRz && len == 5 && bd == 0;
            if (iform == XED_IFORM_LEA_GPRv_AGEN && reg0raw == XED_REG_R11 &&
                xed_decoded_inst_get_base_reg(ops, 0) == XED_REG_RIP) {
               pend_r11 = vmaddr + len + xed_decoded_inst_get_memory_displacement(ops, 0);
            }
            /* alignment nops keep a ret/jmp's dead fall-through, so a branch
             * target behind them still ADOPTS its snapshot (an align_even pad
             * before a block after an early epilogue; M64_NO_DEAD_ENTRY_EVEN) */
            if (!nop_keeps_dead || (cat != XED_CATEGORY_NOP && cat != XED_CATEGORY_WIDENOP)) {
               prev_cat = cat;
            }
            it += len;
            vmaddr += len;
         }
      }

      /* Two tables sharing an anchor often sit back to back below every case
       * body; clamp each at the next table's base and re-emit its slots with
       * its own anchor (shaderapidx9 D3DFormatToImageFormat). */
      for (auto t = tables.begin(); t != tables.end(); ++t) {
         auto nx = std::next(t);
         if (nx == tables.end()) { break; }
         const std::size_t room = (nx->first - t->first) / 4;
         if (t->second.first > room) {
            if (trace) {
               fprintf(stderr, "[jumptable] clamp table=0x%zx count=%zu->%zu (next table 0x%zx)\n",
                       (size_t)t->first, (size_t)t->second.first, room, (size_t)nx->first);
            }
            for (std::size_t k = room; k < t->second.first; ++k) {
               env.jump_table_slots.erase(t->first + k * 4);
            }
            t->second.first = room;
         }
      }
      for (const auto& t : tables) {
         for (std::size_t k = 0; k < t.second.first; ++k) {
            env.jump_table_slots[t.first + k * 4] = t.second.second;
         }
      }

      /* Publish each table's case bodies keyed by its dispatch, for the
       * branch snapshots in DetectPicAnchoredDisps. */
      for (const auto& t : tables) {
         auto d = table_dispatch.find(t.first);
         if (d == table_dispatch.end() || t.second.first == 0) { continue; }
         std::vector<std::size_t>& tgts = env.pic_switch_targets[d->second];
         for (std::size_t k = 0; k < t.second.first; ++k) {
            const std::size_t slot = t.first + k * 4;
            if (slot + 4 > sect_hi) { break; }
            const int32_t raw = (int32_t)img.at<uint32_t>(sect.offset + (slot - sect.addr));
            tgts.push_back(t.second.second + raw);
         }
      }
   }

   template <Bits bits>
   std::string Section<bits>::name() const {
      return std::string(sect.sectname, strnlen(sect.sectname, sizeof(sect.sectname)));
   }

   template <Bits bits>
   Section<bits> *Section<bits>::Synthetic(const std::string& segname,
                                           const std::string& sectname,
                                           uint32_t flags, uint32_t align) {
      auto *self = new Section<bits>();
      std::memset(&self->sect, 0, sizeof(self->sect));
      std::strncpy(self->sect.segname, segname.c_str(), sizeof(self->sect.segname));
      std::strncpy(self->sect.sectname, sectname.c_str(), sizeof(self->sect.sectname));
      self->sect.flags = flags;
      self->sect.align = align;
      /* addr/offset/size/reloff/nreloc are assigned by Build/Emit. */
      return self;
   }

   template <Bits bits>
   void Section<bits>::Build(BuildEnv<bits>& env) {
      /* Zero-fill reserves vmaddr space only: no file bytes, offset 0, size
       * from the vmaddr delta. */
      {
         const uint32_t stype = sect.flags & SECTION_TYPE;
         if (stype == S_ZEROFILL || stype == S_GB_ZEROFILL ||
             stype == S_THREAD_LOCAL_ZEROFILL) {
            env.loc.vmaddr = align_up(env.loc.vmaddr,
                                      (std::size_t)1 << sect.align);
            sect.addr = env.loc.vmaddr;
            sect.offset = 0;
            for (SectionBlob<bits> *elem : content) {
               elem->Build(env);
            }
            sect.size = env.loc.vmaddr - (std::size_t)sect.addr;
            sect.nreloc = relocs.size();
            if (relocs.empty()) {
               sect.reloff = 0;
            } else {
               Location relloc;
               env.allocate(sect.nreloc * RelocationInfo<bits>::size(), relloc);
               sect.reloff = relloc.offset;
            }
            return;
         }
      }

      env.align(sect.align);
      loc(env.loc);

      /* Function entries go at even addresses: an odd non-virtual member
       * function pointer reads as virtual (Itanium pmf low bit; guard
       * f09_member_func_ptr). Decided against the live cursor (instruction
       * sizes settle in Build), and the pad goes before the whole zero-size
       * run at that vmaddr so the function's symbol moves with its code. */
      for (auto it = content.begin(); it != content.end(); ++it) {
         SectionBlob<bits> *elem = *it;
         if (env.loc.vmaddr & 1) {
            bool starts_func_entry = false;
            for (auto pk = it; pk != content.end(); ++pk) {
               SectionBlob<bits> *b = *pk;
               if (!b->active) { continue; }
               if (b->func_entry || b->align_even) { starts_func_entry = true; break; }
               if (b->size() != 0) { break; } /* real blob -> vmaddr advances */
            }
            if (starts_func_entry) {
               DataBlob<bits> *pad = DataBlob<bits>::Padding();
               it = content.insert(it, pad);  /* before this run; it -> pad */
               pad->Build(env);               /* allocate the pad byte */
               ++it;                          /* it -> elem again */
               elem = *it;
            }
         }
         elem->Build(env);
      }

      sect.size = env.loc.offset - loc().offset;

      Location relloc;
      sect.nreloc = relocs.size();
      env.allocate(sect.nreloc * RelocationInfo<bits>::size(), relloc);
      /* ld rejects a non-zero reloff with nreloc 0 */
      sect.reloff = relocs.empty() ? 0 : relloc.offset;
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
   }

   template <Bits bits>
   Section<bits>::Section(const Section<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      id(other.id)
   {
      env.add(&other, this);
      env(other.sect, sect);
      env.resolve(other.segment, &segment);

      /* transform content */
      static const bool no_entry_carry = std::getenv("M64_NO_DEAD_ENTRY_EVEN") != nullptr;
      for (const auto elem : other.content) {
         auto new_blobs = elem->Transform(env);
         if (!new_blobs.empty()) {
            env.add(elem, new_blobs.front());
            /* an expansion (push %ebp -> lea+mov) is built fresh: keep the
             * entry's even-alignment on its first blob */
            if (!no_entry_carry) {
               new_blobs.front()->func_entry |= elem->func_entry;
               new_blobs.front()->align_even |= elem->align_even;
            }
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
      /* pointer arrays grow to 8-byte slots: align them (dyld SIGBUSes on
       * misaligned init-func arrays under Rosetta) */
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

      /* Zero-fill is one ZeroBlob extent; a placeholder inside it (every
       * variable after the first) SPLITS the extent there, so it gets an exact
       * blob boundary while interior lookups still find extent + offset. */
      {
         const uint32_t stype = sect.flags & SECTION_TYPE;
         if (stype == S_ZEROFILL || stype == S_GB_ZEROFILL ||
             stype == S_THREAD_LOCAL_ZEROFILL) {
            for (auto placeholder_it = env.placeholders.lower_bound(sect.addr);
                 placeholder_it != env.placeholders.end() &&
                    placeholder_it->first < sect.addr + sect.size;
                 placeholder_it = env.placeholders.erase(placeholder_it))
               {
                  const std::size_t ph_vmaddr = placeholder_it->first;
                  Placeholder<bits> *placeholder = placeholder_it->second;
                  placeholder->segment = env.current_segment;
                  placeholder->section = this;

                  /* placeholders ascend, so the cursor never rewinds */
                  while (content_it != content.end() &&
                         (*content_it)->loc.vmaddr + (*content_it)->size() <=
                            ph_vmaddr) {
                     ++content_it;
                  }
                  if (content_it == content.end() ||
                      (*content_it)->loc.vmaddr >= ph_vmaddr) {
                     /* at a boundary already (or past every extent) */
                     content.insert(content_it, placeholder);
                     continue;
                  }

                  auto *extent = dynamic_cast<ZeroBlob<bits> *>(*content_it);
                  if (extent == nullptr) {
                     fprintf(stderr,
                             "warning: placeholder vmaddr 0x%zx inside "
                             "non-extent blob in zerofill section %s\n",
                             ph_vmaddr, name().c_str());
                     auto next_it = std::next(content_it);
                     content.insert(next_it, placeholder);
                     continue;
                  }

                  const std::size_t head = ph_vmaddr - extent->loc.vmaddr;
                  const std::size_t tail = extent->size_ - head;
                  extent->size_ = head;
                  ZeroBlob<bits> *tail_extent = ZeroBlob<bits>::Create(
                     Location(extent->loc.offset + head, ph_vmaddr), env, tail);
                  tail_extent->segment = env.current_segment;
                  tail_extent->section = this;
                  auto next_it = std::next(content_it);
                  content.insert(next_it, placeholder);
                  content.insert(next_it, tail_extent);
               }
            return;
         }
      }

      /* for each placeholder, find blob in this section to insert it before */
      for (auto placeholder_it = env.placeholders.lower_bound(sect.addr);
           placeholder_it != env.placeholders.end() &&
              placeholder_it->first < sect.addr + sect.size;
           placeholder_it = env.placeholders.erase(placeholder_it))
         {
            /* linear resume, not std::lower_bound: a list makes that O(N) per
             * placeholder (iPhoto: 1h41m) */
            while (content_it != content.end() &&
                   (*content_it)->loc.vmaddr < placeholder_it->first) {
               ++content_it;
            }
            if (content_it == content.end()) {
               /* into trailing padding: append so it still resolves */
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

            /* inside a blob (usually a linear-sweep slip): insert before the
             * next blob; the placeholder keeps its exact vmaddr */
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
