#include <optional>
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
#include <memory>

#include "section.hh"
#include "segment.hh"
#include "parse.hh"
#include "build.hh"
#include "transform.hh"
#include "section_blob.hh" // LazySymbolPointer
#include "instruction.hh"
#include "stub_helper.hh"
#include "archive.hh"
#include "eh_frame.hh"
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
           env.packed_pair_family(img, loc.vmaddr, value) ||
           env.record_array_column_integers(img, loc.vmaddr, value))) {
         return false;
      }
      /* A single-bit "entry" in a column of single-bit flags is a flag (Call of
       * Duty 4's caps mask 0x20000 = a function entry; guard flag-column-entry). */
      if (m32 && exec && !is_text_const_sect && !env.have_classic_local_relocs &&
          env.flag_column_power_of_two(img, loc.vmaddr, value)) {
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
      /* Data at the end of __text (an unclaimed table) can decode as an
       * instruction running past the section: keep it a data byte rather than
       * abort the translation. Kill M64_NO_TEXT_END_DATA; guard text-end-data. */
      static const bool end_data = std::getenv("M64_NO_TEXT_END_DATA") == nullptr;
      auto overruns_end = [&]() {
         if (!end_data || env.jump_table_slots.count(vmaddr) != 0) { return false; }
         xed_decoded_inst_t xd;
         xed_decoded_inst_zero_set_mode(&xd, &Instruction<bits>::dstate());
         xed_decoded_inst_set_input_chip(&xd, XED_CHIP_INVALID);
         if (xed_decode(&xd, &img.at<uint8_t>(it), img.size() - it) != XED_ERROR_NONE) {
            return false;
         }
         return it + xed_decoded_inst_get_length(&xd) > end;
      };

      while (it != end) {
         SectionBlob<bits> *elem;
         bool emitted_padding = false;
         /* Resync at 0x00 alignment padding after a terminator (stripped
          * images have no symbols to straddle): no compiler starts code with
          * `add [r/m8],r8`, and decoding `00 55 89 ...` would swallow the next
          * function's `push %ebp` (Halo static-init crash). A byte something
          * already names -- a function symbol, or the target of an earlier
          * branch -- is code: a block after a `ret` may start `addb %r8,%r8`
          * (`00 c0`), and padding it desyncs the rest of the block. Kill
          * M64_NO_PAD_KNOWN_CODE; guard pad-known-code. */
         static const bool pad_known_code = std::getenv("M64_NO_PAD_KNOWN_CODE") == nullptr;
         const bool known_code = pad_known_code &&
            (env.func_syms.count(vmaddr) != 0 || env.placeholders.count(vmaddr) != 0);
         if (text && prev_no_fallthrough && img.at<uint8_t>(it) == 0x00 && !known_code &&
             env.jump_table_slots.count(vmaddr) == 0) {
            elem = DataBlob<bits>::Parse(img, Location(it, vmaddr), env);
            emitted_padding = true;
         } else if (text && (straddles_symbol() || overruns_end())) {
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

      DetectPicAnchoredDisps(img, env);
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

   /* The assembler's in-function alignment fillers rewrite a register with
    * itself (`lea 0(%esi),%esi`, `lea 0(%esi,%eiz),%esi`, `mov %esi,%esi`):
    * no write, or a PIC anchor in %esi dies at every aligned loop head.
    * Kill M64_NO_IDENTITY_NOP; guard identity-nop-anchor. */
   static bool identity_filler(const xed_decoded_inst_t& xedd) {
      static const bool identity_nop = std::getenv("M64_NO_IDENTITY_NOP") == nullptr;
      if (!identity_nop) { return false; }
      const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);
      const xed_reg_enum_t r0 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
      const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
      if (is_gpr32(r0) && iform == XED_IFORM_LEA_GPRv_AGEN &&
          xed_decoded_inst_get_base_reg(ops, 0) == r0 &&
          xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
          xed_decoded_inst_get_memory_displacement(ops, 0) == 0) {
         return true;
      }
      return is_gpr32(r0) && xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1) == r0 &&
             (iform == XED_IFORM_MOV_GPRv_GPRv_89 || iform == XED_IFORM_MOV_GPRv_GPRv_8B ||
              iform == XED_IFORM_XCHG_GPRv_GPRv);
   }

   /* Every GPR32 the instruction writes, sub-register writes included. */
   template <typename F>
   static void for_each_written_gpr32(const xed_decoded_inst_t& xedd, F f) {
      if (identity_filler(xedd)) { return; }
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

   /* GCC PIC thunks `mov (%esp),%reg; ret`: entry vmaddr -> %reg. Thunks
    * in other sections come from the symbol table (env.pic_thunks). */
   template <Bits bits>
   static std::unordered_map<std::size_t, xed_reg_enum_t>
   pic_find_thunks(const std::list<SectionBlob<bits> *>& content, const ParseEnv<bits>& env) {
      std::unordered_map<std::size_t, xed_reg_enum_t> pic_thunks;
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
      for (const auto& kv : env.pic_thunks) {
         const xed_reg_enum_t r = gpr32_from_enc(kv.second);
         if (r != XED_REG_INVALID) { pic_thunks.emplace(kv.first, r); }
      }
      return pic_thunks;
   }

   /* Resolve an anchored operand, before the instruction's own writes are
    * applied (so `mov disp(%ebx),%ebx` still resolves). The anchor may be
    * the SIB base, or — at scale 1, the fields being symmetric — the index
    * (`lea 0xa4b0(%eax,%edi),%edx`; Portal 2 libsteam_api). An anchored
    * target overrides the parser's absolute-table guess. `anchor_of(r)` is
    * the anchor %r holds, 0 for none. Returns the anchor register used. */
   template <Bits bits, typename AnchorOf>
   static xed_reg_enum_t pic_rewrite_operand(Instruction<bits> *inst, ParseEnv<bits>& env,
                                             AnchorOf anchor_of, bool trace) {
      if (inst->memdisp != nullptr && !inst->memdisp_absolute) { return XED_REG_INVALID; }
      const xed_decoded_inst_t& xedd = inst->xedd;
      const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
      const unsigned nops = xed_decoded_inst_noperands(&xedd);
      for (unsigned i = 0; i < nops; ++i) {
         const xed_reg_enum_t basereg = xed_decoded_inst_get_base_reg(ops, i);
         const xed_reg_enum_t indexreg = xed_decoded_inst_get_index_reg(ops, i);
         if (!is_gpr32(basereg)) continue;
         bool anchor_is_index = false;
         xed_reg_enum_t anchor_reg = basereg;
         if (anchor_of(basereg) == 0) {
            if (!is_gpr32(indexreg) || anchor_of(indexreg) == 0) continue;
            if (xed_decoded_inst_get_scale(ops, i) != 1) continue;
            if (basereg == XED_REG_ESP) continue;   /* can't become a SIB index */
            anchor_is_index = true;
            anchor_reg = indexreg;
         } else if (indexreg != XED_REG_INVALID) {
            /* a second anchor in the index is ambiguous */
            if (!is_gpr32(indexreg) || anchor_of(indexreg) != 0) continue;
         }
         if (xed_decoded_inst_get_memory_displacement_width(ops, i) != sizeof(uint32_t)) continue;

         const ssize_t disp = xed_decoded_inst_get_memory_displacement(ops, i);
         const std::size_t target = anchor_of(anchor_reg) + disp;
         if (trace) {
            fprintf(stderr, "[anchor] inst=0x%zx %s=%s anchor=0x%zx disp=0x%zx target=0x%zx iform=%s\n",
                    (size_t)inst->loc.vmaddr, anchor_is_index ? "index" : "base",
                    xed_reg_enum_t2str(anchor_reg), (size_t)anchor_of(anchor_reg),
                    (size_t)disp, (size_t)target,
                    xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&xedd)));
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
         /* an interior byte of opaque data binds to blob + offset (guard
          * 97_pic_const_interior_field) */
         if (env.vmaddr_in_writable_data(target) ||
             env.vmaddr_in_readonly_opaque_data(target)) {
            env.vmaddr_resolver.resolve_containing(
               target, (const SectionBlob<bits> **)&inst->memdisp,
               &inst->memdisp_offset, /*override=*/true);
         }
         return anchor_reg;
      }
      return XED_REG_INVALID;
   }

   /* PIC region: cancel the absolute-address heuristics (a pointer
    * immediate, or a `disp32(%base)` table capture on a non-anchor base).
    * Portal 2 `movl $0x1000,4(%esp)` (a size) aliased low __TEXT; guard
    * 96_zerofill_common_interior. */
   template <Bits bits>
   static void pic_cancel_absolute(Instruction<bits> *inst, ParseEnv<bits>& env) {
      if (inst->imm != nullptr && inst->imm->heuristic) {
         env.vmaddr_resolver.cancel((std::size_t)inst->imm->value,
                                    (const SectionBlob<bits> **)&inst->imm->pointee);
         env.vmaddr_resolver.cancel_containing((std::size_t)inst->imm->value,
                                               (const SectionBlob<bits> **)&inst->imm->pointee);
         inst->imm->pointee = nullptr;
      }
      if (inst->memdisp_absolute && !inst->pic_anchored) {
         const xed_operand_values_t *mops = xed_decoded_inst_operands_const(&inst->xedd);
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

   /* ANCHOR-DERIVED BASES: clang selects between two addresses off one
    * anchor and folds the common displacement into the use:
    *    lea 0x38(%edi),%eax ; cmovge %edi,%eax ; cvttsd2si 0x267ff(%eax),%eax
    * %eax is "anchor + c" for a c known only per path, so no single
    * rip-relative target exists for the use, and its raw disp32 reached the
    * wrong data (the translated anchor sits at another distance from it).
    * Each PRODUCER of the register (the `lea c(%anchor)`, the anchor source
    * of the cmov) becomes the exact address its first use reaches,
    * T(anchor + c + D), and every use's disp D becomes D - D_first. All rip
    * operands then land on real data, so later re-parses rebind them.
    * Straight-line only (the caller clears it at joins, calls and dead
    * flow); the first walk of an i386 section only. Kill
    * M64_NO_PIC_DERIVED_BASE; guard pic-derived-base. */
   template <Bits bits>
   struct PicDerivedBases {
      struct Producer { Instruction<bits> *inst; ssize_t c; bool cmov; };
      struct Derived { std::size_t anchor; std::vector<Producer> prods; ssize_t dref = 0; bool bound = false; };
      std::unordered_map<xed_reg_enum_t, std::shared_ptr<Derived>> derived;
      std::shared_ptr<Derived> derived_new;
      xed_reg_enum_t derived_dst = XED_REG_INVALID;

      static bool on() {
         static const bool derived_on = std::getenv("M64_NO_PIC_DERIVED_BASE") == nullptr;
         return derived_on;
      }

      void clear() { derived.clear(); }

      /* The derived base `inst` produces, judged on the state before its
       * own write (applied by after()), and the rewrite of a derived use. */
      template <typename AnchorOf>
      void before(Instruction<bits> *inst, ParseEnv<bits>& env, AnchorOf anchor_of, bool trace) {
         derived_new = nullptr;
         derived_dst = XED_REG_INVALID;
         const xed_decoded_inst_t& xedd = inst->xedd;
         const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);
         const xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
         {
            const xed_reg_enum_t r0 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            const xed_reg_enum_t r1 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
            const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
            if (iform == XED_IFORM_LEA_GPRv_AGEN && is_gpr32(r0) && inst->instbuf[0] == 0x8D &&
                inst->memdisp == nullptr &&
                xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
                xed_decoded_inst_get_memory_displacement_width(ops, 0) <= 1) {
               const std::size_t a = anchor_of(xed_decoded_inst_get_base_reg(ops, 0));
               if (a != 0) {
                  derived_new = std::make_shared<Derived>();
                  derived_new->anchor = a;
                  derived_new->prods.push_back(
                     {inst, (ssize_t)xed_decoded_inst_get_memory_displacement(ops, 0), false});
                  derived_dst = r0;
               }
            } else if (cat == XED_CATEGORY_CMOV && is_gpr32(r0) && is_gpr32(r1) &&
                       inst->instbuf.size() == 3 && inst->instbuf[0] == 0x0F) {
               auto d = derived.find(r0);
               const std::size_t a = anchor_of(r1);
               auto ds = derived.find(r1);
               if (d != derived.end() && !d->second->bound) {
                  if (a != 0 && a == d->second->anchor) {
                     derived_new = d->second;   /* the anchor source joins its producers */
                     derived_new->prods.push_back({inst, 0, true});
                     derived_dst = r0;
                  } else if (ds != derived.end() && !ds->second->bound &&
                             ds->second->anchor == d->second->anchor) {
                     derived_new = std::make_shared<Derived>(*d->second);
                     derived_new->prods.insert(derived_new->prods.end(),
                                               ds->second->prods.begin(), ds->second->prods.end());
                     derived_dst = r0;
                  }
               }
            } else if ((iform == XED_IFORM_MOV_GPRv_GPRv_89 || iform == XED_IFORM_MOV_GPRv_GPRv_8B) &&
                       is_gpr32(r0) && r0 != r1 && derived.count(r1) != 0) {
               derived_new = derived[r1];
               derived_dst = r0;
            }
         }

         /* A use of an anchor-derived base. */
         if (derived.empty() ||
             xed_decoded_inst_number_of_memory_operands(&xedd) < 1 ||   /* call/push add the stack */
             !(inst->memdisp == nullptr || inst->memdisp_absolute) || inst->pic_anchored) {
            return;
         }
         const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
         const xed_reg_enum_t b = xed_decoded_inst_get_base_reg(ops, 0);
         const xed_reg_enum_t x = xed_decoded_inst_get_index_reg(ops, 0);
         auto d = derived.find(b);
         if (d == derived.end() || anchor_of(b) != 0 ||
             !(x == XED_REG_INVALID || (derived.count(x) == 0 && anchor_of(x) == 0)) ||
             xed_decoded_inst_get_memory_displacement_width(ops, 0) != sizeof(uint32_t)) {
            return;
         }
         Derived& dv = *d->second;
         const ssize_t D = xed_decoded_inst_get_memory_displacement(ops, 0);
         const ssize_t dref = dv.bound ? dv.dref : D;
         bool ok = true;
         for (const auto& p : dv.prods) {
            const std::size_t t = (std::size_t)((ssize_t)dv.anchor + p.c + D);
            const std::size_t tr = (std::size_t)((ssize_t)dv.anchor + p.c + dref);
            ok = ok && (env.vmaddr_in_writable_data(t) || env.vmaddr_in_readonly_opaque_data(t)) &&
                 (env.vmaddr_in_writable_data(tr) || env.vmaddr_in_readonly_opaque_data(tr));
         }
         if (ok && !dv.bound) {
            for (auto& p : dv.prods) {
               const std::size_t t = (std::size_t)((ssize_t)dv.anchor + p.c + D);
               SectionBlob<bits> *tb = env.add_placeholder(t);
               if (tb == nullptr) { ok = false; break; }
               p.inst->memdisp = tb;
               p.inst->memdisp_offset = 0;
               p.inst->memdisp_absolute = false;
               env.vmaddr_resolver.resolve_containing(
                  t, (const SectionBlob<bits> **)&p.inst->memdisp,
                  &p.inst->memdisp_offset, /*override=*/true);
               if (p.cmov) {
                  p.inst->select_anchor_src = true;
               } else {
                  p.inst->memidx = 0;
                  p.inst->pic_anchored = true;
                  p.inst->pic_anchor_in_index = false;
               }
            }
            dv.bound = ok;
            dv.dref = D;
         }
         if (ok && dv.bound) {
            if (inst->memdisp != nullptr) {
               env.vmaddr_resolver.cancel((std::size_t)D, (const SectionBlob<bits> **)&inst->memdisp);
               env.vmaddr_resolver.cancel_containing((std::size_t)D,
                                                     (const SectionBlob<bits> **)&inst->memdisp);
            }
            inst->memdisp = nullptr;
            inst->memdisp_offset = 0;
            inst->memdisp_absolute = false;
            inst->set_mem_disp32((int32_t)(D - dv.dref));
            if (trace) {
               fprintf(stderr, "[anchor] derived use 0x%zx %s anchor=0x%zx disp 0x%zx -> 0x%zx\n",
                       (size_t)inst->loc.vmaddr, xed_reg_enum_t2str(b), dv.anchor,
                       (size_t)D, (size_t)(D - dv.dref));
            }
         } else if (!ok) {
            fprintf(stderr, "warning: anchor-derived base %s at 0x%zx reaches no data "
                    "for every producer; its disp stays raw\n",
                    xed_reg_enum_t2str(b), (size_t)inst->loc.vmaddr);
         }
      }

      /* After the instruction's register writes. */
      void after(const Instruction<bits> *inst) {
         if (derived.empty() && !derived_new) { return; }
         for_each_written_gpr32(inst->xedd, [&](xed_reg_enum_t r) { derived.erase(r); });
         if (derived_new) { derived[derived_dst] = derived_new; }
         if (xed_decoded_inst_get_category(&inst->xedd) == XED_CATEGORY_CALL) { derived.clear(); }
         derived_new = nullptr;
      }
   };

   template <Bits bits>
   void Section<bits>::DetectPicAnchoredDisps(const Image& img, ParseEnv<bits>& env) {
      if constexpr (bits != Bits::M32) {
         return;
      }
      if (parser != TextParser) {
         return;
      }
      /* The CFG walk below replaces this linear one. Kill M64_NO_PIC_CFG;
       * guard pic-cfg-fixpoint. */
      static const bool cfg_walk = std::getenv("M64_NO_PIC_CFG") == nullptr;
      if (cfg_walk && DetectPicAnchoredDispsCfg(img, env)) {
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
      static const bool slots_past_ret = std::getenv("M64_NO_PIC_SLOTS_PAST_RET") == nullptr;
      /* Past the final ret (landing pads), -O0 code first re-spills the PIC
       * register into the anchor's frame slot: the unwinder restored the
       * callee-saved registers of the throwing call, where that register is
       * the anchor. Such a store keeps the slot (and names the register) an
       * anchor instead of killing it (Portal 2 launcher `mov %ebx,-0x18(%ebp)`
       * then `mov -0x18(%ebp),%ecx; mov 0x20152(%ecx),%ecx`). Kill
       * M64_NO_PIC_PAD_RESPILL; guard 99_pic_anchor_pad_respill. */
      static const bool pad_respill = std::getenv("M64_NO_PIC_PAD_RESPILL") == nullptr;
      bool past_ret = false;

      PicDerivedBases<bits> derived;   /* cleared at joins, calls and dead flow */
      auto anchor_of = [&](xed_reg_enum_t r) -> std::size_t {
         auto it = anchors.find(r);
         return it == anchors.end() ? 0 : it->second;
      };

      auto clear_all = [&]() {
         derived.clear();
         anchors.clear();
         anchor_slots.clear();
         entry_save_slots.clear();
         pre_anchor_saves.clear();
         anchored_region = false;
      };

      const auto pic_thunks = pic_find_thunks(content, env);

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
      static const bool post_ret_body = std::getenv("M64_NO_PIC_POST_RET_BODY") == nullptr;
      static const bool slot_write_kill = std::getenv("M64_NO_PIC_SLOT_WRITE_KILL") == nullptr;
      AnchorMap post_ret_keep;   /* callee-saved anchors live at the last `ret` */
      AnchorMap post_ret_live;   /* ...as handed to the code after it */
      /* Only symbols inside this section bound its functions: Halo has data
       * symbols but no code ones, and every function after a ret inherited
       * the previous one's anchors. */
      const auto sect_sym = env.func_syms.lower_bound(sect.addr);
      const bool sect_func_syms = sect_sym != env.func_syms.end() && *sect_sym < sect.addr + sect.size;
      struct OrphanRewrite { xed_reg_enum_t reg; std::size_t anchor, top; };
      std::unordered_map<std::size_t, OrphanRewrite> orphan_rewrites;
      std::size_t orphan_top = 0;                  /* where the current orphan opened */
      std::set<xed_reg_enum_t> orphan_written;     /* regs written since then */
      /* (orphan top, reg) pairs the loop [top, last back-edge source] writes */
      std::set<std::pair<std::size_t, xed_reg_enum_t>> loop_carried;
      for (; pass < 2; ++pass) {
         post_ret_keep.clear();
         post_ret_live.clear();
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
               if (env.func_syms.count(inst->loc.vmaddr) != 0) {
                  unreached = false;
                  /* The body's anchors end with the body: a landing pad that
                   * ends in _Unwind_Resume leaves them set, and the next
                   * function's prologue spill of %ebx then read as an anchor
                   * slot, not an entry save (Portal 2 libsteam
                   * CryptoPP::FileSink::~FileSink pad lost %ebx). */
                  for (const auto& [r, a] : post_ret_live) {
                     auto it = anchors.find(r);
                     if (it != anchors.end() && it->second == a) { anchors.erase(it); }
                  }
                  post_ret_live.clear();
                  if (slots_past_ret) { anchor_slots.clear(); }   /* a new frame */
                  past_ret = false;
               }
               /* the linear predecessor cannot fall through here */
               const bool dead_edge = prev_inst != nullptr &&
                  (last_flow_cat == XED_CATEGORY_RET ||
                   last_flow_cat == XED_CATEGORY_UNCOND_BR || ft_dead);
               const bool no_fallthrough = unreached || orphan || dead_edge;
               if (no_fallthrough || env.func_syms.count(inst->loc.vmaddr) != 0) { derived.clear(); }
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
                  derived.clear();   /* straight-line state only */
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

            /* Code after a `ret` that no new function starts is the same
             * function's body, entered by branches whose state derives from
             * here: it holds the body's callee-saved anchors, not the empty set
             * the epilogue leaves (Portal 2009 filesystem_stdio
             * CSearchPathsIterator::GetNext: a block reached only by branches
             * from after the ret kept `0x5af18(%ebx)` raw -> NULL deref).
             * Padding keeps it pending; a function start drops it. Kill
             * M64_NO_PIC_POST_RET_BODY; guard 99_pic_anchor_post_ret. */
            if (!post_ret_keep.empty() && cat != XED_CATEGORY_NOP && cat != XED_CATEGORY_WIDENOP) {
               /* `push %ebp` opens a frame: an unnamed function, not a landing
                * pad (pvz/civ4 static functions lost their absolute pointers). */
               if (env.func_syms.count(inst->loc.vmaddr) == 0 && inst->instbuf[0] != 0x55) {
                  for (const auto& [r, a] : post_ret_keep) {
                     if (anchors.find(r) == anchors.end()) { anchors[r] = a; post_ret_live[r] = a; }
                  }
                  anchored_region = true;
                  orphan = false;
                  if (trace) {
                     fprintf(stderr, "[anchor] post-ret 0x%zx pass=%d kept=%zu\n",
                             (size_t)inst->loc.vmaddr, pass, post_ret_keep.size());
                  }
               }
               post_ret_keep.clear();
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
                  past_ret = false;
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
            if (!anchors.empty()) {
               const xed_reg_enum_t anchor_reg = pic_rewrite_operand(inst, env, anchor_of, trace);
               if (anchor_reg != XED_REG_INVALID && orphan_revert && orphan && pass == 0 &&
                   orphan_written.count(anchor_reg) == 0) {   /* straight from the stale state */
                  orphan_rewrites[inst->loc.vmaddr] = {anchor_reg, anchors[anchor_reg], orphan_top};
               }
            }

            if (PicDerivedBases<bits>::on() && pass == 0) {
               derived.before(inst, env, anchor_of, trace);
            }

            if (!anchors.empty() || anchored_region) {
               pic_cancel_absolute(inst, env);
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
                     auto pad_slot = anchor_slots.find(slot);
                     if (a != anchors.end()) {
                        anchor_slots[slot] = a->second;
                        pre_anchor_saves.erase(slot);
                     } else if (pad_respill && past_ret && pad_slot != anchor_slots.end() &&
                                (reg == XED_REG_EBX || reg == XED_REG_ESI || reg == XED_REG_EDI)) {
                        anchors[reg] = pad_slot->second;
                        anchor_keep = reg;
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
                  } else if (slot_write_kill && xed_decoded_inst_mem_written(&xedd, 0)) {
                     /* Any other write ends what the slot held, as in
                      * DetectJumpTables: Portal server reused its anchor slot
                      * as a loop counter (`movl $0,-0x18(%ebp)`), and the
                      * counter reloaded into %ecx was taken for the anchor in
                      * `leal 0x874(%eax,%ecx),%edx`. Kill
                      * M64_NO_PIC_SLOT_WRITE_KILL; guard 99_pic_anchor_slot_reuse. */
                     anchor_slots.erase(slot);
                     entry_save_slots.erase(slot);
                     pre_anchor_saves.erase(slot);
                  }
               }
            }

            /* `xchg %a,%b` swaps the anchors (the kill below clears both): the
             * cpuid idiom `mov %ebx,%esi; cpuid; xchg %ebx,%esi` hands the PIC
             * register back (Portal 2009 libtier0 GetProcessorVendorId stored
             * VendorID through a raw i386 disp into __TEXT). Kill
             * M64_NO_PIC_ANCHOR_XCHG; guard 99_pic_anchor_xchg. */
            static const bool xchg_swap = std::getenv("M64_NO_PIC_ANCHOR_XCHG") == nullptr;
            const xed_reg_enum_t xa = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            const xed_reg_enum_t xb = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
            const bool is_xchg = xchg_swap && xed_decoded_inst_get_iclass(&xedd) == XED_ICLASS_XCHG &&
                                 is_gpr32(xa) && is_gpr32(xb) && xa != xb;
            std::optional<std::size_t> xa_anchor, xb_anchor;
            if (is_xchg) {
               if (auto i = anchors.find(xa); i != anchors.end()) { xa_anchor = i->second; }
               if (auto i = anchors.find(xb); i != anchors.end()) { xb_anchor = i->second; }
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

            if (is_xchg) {
               if (xb_anchor) { anchors[xa] = *xb_anchor; }
               if (xa_anchor) { anchors[xb] = *xa_anchor; }
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

            derived.after(inst);

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
               if (pending_forward_targets.empty()) {
                  /* Code after the `ret` that no new function starts keeps the
                   * callee-saved anchors (see post_ret_keep above). */
                  post_ret_keep.clear();
                  if (post_ret_body && sect_func_syms) {
                     for (const xed_reg_enum_t r : {XED_REG_EBX, XED_REG_ESI, XED_REG_EDI}) {
                        auto it = anchors.find(r);
                        if (it != anchors.end()) { post_ret_keep[r] = it->second; }
                     }
                  }
                  /* The EH landing pads GCC places after the final `ret` are
                   * entered by the unwinder in this function's frame, and -O0
                   * code reloads the anchor from its frame slot there (Portal
                   * 2 launcher `mov -0x10(%ebp),%ecx; mov 0x24812(%ecx),%ecx`
                   * left raw: a wild read in every cleanup path). With
                   * function symbols to bound them, the frame slots survive
                   * the `ret` until the next function.
                   * Kill M64_NO_PIC_SLOTS_PAST_RET; guard 99_pic_anchor_pad_slot. */
                  if (slots_past_ret && !env.func_syms.empty()) {
                     const SlotMap keep = anchor_slots;
                     clear_all();
                     anchor_slots = keep;
                     past_ret = true;
                  } else {
                     clear_all();
                  }
               }
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

   /* The exception call sites of the functions in [lo, hi), from the image's
    * __eh_frame and __gcc_except_tab (libsteam has two of the latter): range
    * start -> (range end, landing pad). */
   template <Bits bits>
   static std::map<std::size_t, std::pair<std::size_t, std::size_t>>
   pic_eh_call_sites(const Image& img, ParseEnv<bits>& env, std::size_t lo, std::size_t hi) {
      std::map<std::size_t, std::pair<std::size_t, std::size_t>> sites;
      const section_t<bits> *ehf = nullptr;
      std::vector<const section_t<bits> *> gxts;
      for (Segment<bits> *seg : env.archive.segments()) {
         for (Section<bits> *s : seg->sections) {
            if (s->name() == "__eh_frame") { ehf = &s->sect; }
            else if (s->name() == "__gcc_except_tab") { gxts.push_back(&s->sect); }
         }
      }
      auto bytes = [&](const section_t<bits> *sc) {
         if (sc->offset + sc->size > img.size()) { return std::vector<uint8_t>(); }
         const uint8_t *p = &img.at<uint8_t>(sc->offset);
         return std::vector<uint8_t>(p, p + sc->size);
      };
      if (ehf == nullptr || gxts.empty()) { return sites; }
      const std::vector<uint8_t> ebuf = bytes(ehf);
      std::vector<std::vector<uint8_t>> gbufs;
      for (const auto *g : gxts) { gbufs.push_back(bytes(g)); }
      eh_for_each_fde(ebuf, (uint32_t)ehf->addr, [&](uint32_t func, bool has_lsda, uint32_t lsda) {
         if (!has_lsda || func < lo || func >= hi) { return; }
         for (std::size_t i = 0; i < gxts.size(); ++i) {
            const section_t<bits>& g = *gxts[i];
            if (lsda < g.addr || lsda >= g.addr + gbufs[i].size()) { continue; }
            eh_for_each_call_site(gbufs[i], lsda - g.addr, (uint32_t)g.addr, func,
                                  [&](uint32_t start, uint32_t len, uint32_t lp) {
               if (lp != 0) { sites[start] = {(std::size_t)start + len, lp}; }
            });
         }
      });
      return sites;
   }

   /*
    * The anchor walk as a dataflow fixpoint over the section's control-flow
    * graph (the default; M64_NO_PIC_CFG=1 runs the linear two-pass walk
    * above, whose comments describe the per-instruction rules shared here).
    *
    * The linear walk ran each block on the state of the code physically
    * before it, so a block entered only by a branch the walk had not reached
    * yet, or a loop body whose back edge was snapshotted on the first pass,
    * ran on a state missing an anchor, and the loss reached every later join:
    * clang -O2 spilled the anchor on one path, reloaded it before a join and
    * carried it around a loop, and every anchored access in the loop kept its
    * raw i386 disp.
    *
    * Here the basic blocks (leaders: function symbols, branch and claimed
    * jump-table case targets, the instruction after a branch, ret or
    * noreturn call, code after interleaved data) carry MUST states (reg ->
    * anchor, frame slot -> anchor, entry-save slots), start optimistic
    * (unvisited = no constraint) and iterate to the greatest fixpoint; one
    * final walk then rewrites each block from its fixpoint entry state.
    *
    *  - Edges stay inside a function: never into a function symbol (a tail
    *    call), no back edge over one, branches within 64 KB. Function
    *    entries nothing falls into, the section start and code after data
    *    start empty. Calls are not edges.
    *  - A call's fall-through holds nothing in eax/ecx/edx, so at a join
    *    those registers take the other edges' anchors (CALL_ADOPT).
    *  - A block no edge reaches (EH landing pads, indirect targets) starts
    *    from the state of the code before it: after a `ret`, the
    *    callee-saved anchors the unwinder restores and the frame slots (pad
    *    respill); after a jmp or noreturn call, all of it. Alignment fillers
    *    and trap calls after a ret/jmp feed no fall-through.
    *
    * Returns false, before any rewrite, if the iteration does not settle
    * (the caller then runs the linear walk).
    */
   template <Bits bits>
   bool Section<bits>::DetectPicAnchoredDispsCfg(const Image& img, ParseEnv<bits>& env) {
      using Slot = std::pair<xed_reg_enum_t, ssize_t>;   /* (frame base, disp) */
      struct State {
         std::map<xed_reg_enum_t, std::size_t> anchors;   /* reg -> anchor vmaddr */
         std::map<xed_reg_enum_t, std::size_t> plus;      /* reg = anchor + a non-address */
         std::map<Slot, std::size_t> slots;                /* frame slot -> anchor */
         /* frame slots holding a register's FUNCTION-ENTRY value: stores
          * before the anchor pop, promoted by it, demoted by a later store */
         std::map<Slot, xed_reg_enum_t> pre_saves, entry_saves;
         unsigned wild = 0;       /* eax/ecx/edx after a call: no constraint at a join */
         bool region = false;     /* PIC codegen seen: cancels the absolute heuristics */
         bool past_ret = false;   /* a landing pad after the final ret */
         bool operator==(const State& o) const {
            return anchors == o.anchors && plus == o.plus && slots == o.slots && pre_saves == o.pre_saves &&
                   entry_saves == o.entry_saves && wild == o.wild && region == o.region &&
                   past_ret == o.past_ret;
         }
      };
      auto wbit = [](xed_reg_enum_t r) { return 1u << (r - XED_REG_EAX); };
      auto meet = [&](State& a, const State& b) {
         for (auto it = a.anchors.begin(); it != a.anchors.end(); ) {
            auto o = b.anchors.find(it->first);
            const bool keep = o != b.anchors.end() ? o->second == it->second
                                                   : (b.wild & wbit(it->first)) != 0;
            it = keep ? std::next(it) : a.anchors.erase(it);
         }
         for (const auto& [r, v] : b.anchors) {
            if ((a.wild & wbit(r)) != 0) { a.anchors.emplace(r, v); }
         }
         a.wild &= b.wild;
         intersect_into(a.plus, b.plus);
         intersect_into(a.slots, b.slots);
         intersect_into(a.pre_saves, b.pre_saves);
         intersect_into(a.entry_saves, b.entry_saves);
         a.region = a.region || b.region;
         a.past_ret = a.past_ret && b.past_ret;
      };

      const bool trace = std::getenv("MACHO_TRACE_ANCHOR") != nullptr;
      static const bool entry_save_gate = std::getenv("M64_NO_PIC_ANCHOR_ENTRY_SAVE") == nullptr;
      static const bool slots_past_ret = std::getenv("M64_NO_PIC_SLOTS_PAST_RET") == nullptr;
      static const bool pad_respill = std::getenv("M64_NO_PIC_PAD_RESPILL") == nullptr;
      static const bool post_ret_body = std::getenv("M64_NO_PIC_POST_RET_BODY") == nullptr;
      static const bool slot_write_kill = std::getenv("M64_NO_PIC_SLOT_WRITE_KILL") == nullptr;
      static const bool xchg_swap = std::getenv("M64_NO_PIC_ANCHOR_XCHG") == nullptr;
      static const bool call_adopt = std::getenv("M64_NO_PIC_ANCHOR_CALL_ADOPT") == nullptr;
      static const bool noreturn_gate = std::getenv("M64_NO_PIC_NORETURN") == nullptr;
      /* ANCHOR + IMMEDIATE (below). Kill M64_NO_PIC_ANCHOR_IMM; guard pic-anchor-imm. */
      static const bool anchor_imm = std::getenv("M64_NO_PIC_ANCHOR_IMM") == nullptr;

      std::vector<Instruction<bits> *> ins;
      std::vector<char> after_data;   /* interleaved data precedes ins[k] */
      {
         bool data = false;
         for (SectionBlob<bits> *blob : content) {
            if (auto *in = dynamic_cast<Instruction<bits> *>(blob)) {
               ins.push_back(in);
               after_data.push_back(data);
               data = false;
            } else if (dynamic_cast<JumpTableEntry<bits> *>(blob) == nullptr) {
               data = true;   /* an inline PIC jump table shares its function's anchor */
            }
         }
      }
      const std::size_t n = ins.size();
      const auto pic_thunks = pic_find_thunks(content, env);
      const std::set<std::size_t>& fs = env.func_syms;
      const auto sect_sym = fs.lower_bound(sect.addr);
      const bool sect_func_syms = sect_sym != fs.end() && *sect_sym < sect.addr + sect.size;

      auto cat_of = [&](std::size_t k) { return xed_decoded_inst_get_category(&ins[k]->xedd); };
      auto is_func = [&](std::size_t k) { return fs.count(ins[k]->loc.vmaddr) != 0; };
      auto is_pad = [&](std::size_t k) {
         const xed_category_enum_t c = cat_of(k);
         return c == XED_CATEGORY_NOP || c == XED_CATEGORY_WIDENOP || identity_filler(ins[k]->xedd);
      };
      /* `call $+0; pop %reg` establishes an anchor */
      auto anchor_pop = [&](std::size_t k) {
         return k > 0 && !after_data[k] &&
                xed_decoded_inst_get_iform_enum(&ins[k]->xedd) == XED_IFORM_POP_GPRv_58 &&
                ins[k - 1]->instbuf == opcode_t{0xe8, 0x00, 0x00, 0x00, 0x00} &&
                is_gpr32(xed_decoded_inst_get_reg(&ins[k]->xedd, XED_OPERAND_REG0));
      };
      /* `call get_pc_thunk.<r>` leaves %r = the next instruction */
      auto thunk_of = [&](std::size_t k, std::size_t *after) {
         const xed_decoded_inst_t& x = ins[k]->xedd;
         if (cat_of(k) != XED_CATEGORY_CALL) { return XED_REG_INVALID; }
         const ssize_t d = xed_decoded_inst_get_branch_displacement(&x);
         if (d == 0) { return XED_REG_INVALID; }
         *after = ins[k]->loc.vmaddr + xed_decoded_inst_get_length(&x);
         auto t = pic_thunks.find(*after + d);
         return t == pic_thunks.end() ? XED_REG_INVALID : t->second;
      };
      {
         bool any = false;
         std::size_t vm;
         for (std::size_t k = 0; k < n && !any; ++k) {
            any = anchor_pop(k) || thunk_of(k, &vm) != XED_REG_INVALID;
         }
         if (!any) { return true; }
      }

      /* ---- the graph ---- */
      std::unordered_map<std::size_t, std::size_t> at;   /* vmaddr -> instruction */
      at.reserve(n);
      for (std::size_t k = 0; k < n; ++k) { at.emplace(ins[k]->loc.vmaddr, k); }
      auto edge_to = [&](std::size_t from, std::size_t tgt, bool ranged) -> std::optional<std::size_t> {
         if (tgt < sect.addr || tgt >= sect.addr + sect.size) { return std::nullopt; }
         if (ranged && (tgt > from ? tgt - from : from - tgt) > 0x10000) { return std::nullopt; }
         if (fs.count(tgt) != 0) { return std::nullopt; }   /* a tail call */
         if (tgt < from && !pic_same_function(fs, tgt, from)) { return std::nullopt; }
         auto it = at.find(tgt);
         if (it == at.end()) { return std::nullopt; }
         return it->second;
      };
      auto for_targets = [&](std::size_t k, auto f) {
         const xed_decoded_inst_t& x = ins[k]->xedd;
         const xed_category_enum_t c = cat_of(k);
         const std::size_t vm = ins[k]->loc.vmaddr;
         if ((c == XED_CATEGORY_COND_BR || c == XED_CATEGORY_UNCOND_BR) &&
             xed_decoded_inst_get_branch_displacement_width(&x) != 0) {
            const std::size_t tgt = vm + xed_decoded_inst_get_length(&x) +
                                    xed_decoded_inst_get_branch_displacement(&x);
            if (auto t = edge_to(vm, tgt, true)) { f(*t); }
         }
         auto jt = env.pic_switch_targets.find(vm);
         if (jt != env.pic_switch_targets.end()) {
            for (const std::size_t tgt : jt->second) {
               if (auto t = edge_to(vm, tgt, false)) { f(*t); }
            }
         }
      };
      auto call_target = [&](std::size_t k) {
         const xed_decoded_inst_t& x = ins[k]->xedd;
         return (std::size_t)(ins[k]->loc.vmaddr + xed_decoded_inst_get_length(&x) +
                              xed_decoded_inst_get_branch_displacement(&x));
      };
      /* A local function that runs straight into a noreturn call never
       * returns either: clang's __clang_call_terminate, unnamed in a stripped
       * image (Angry Birds 0x7840 `call ___cxa_begin_catch; call
       * std::terminate`), ends EH pads that would fall into the code after. */
      std::unordered_map<std::size_t, bool> local_noreturn;
      auto noreturn_fn = [&](std::size_t tgt) {
         if (env.noreturn_stubs.count(tgt) != 0) { return true; }
         auto memo = local_noreturn.find(tgt);
         if (memo != local_noreturn.end()) { return memo->second; }
         bool nr = false;
         auto it = at.find(tgt);
         for (std::size_t k = it == at.end() ? n : it->second, i = 0; k < n && i < 32; ++k, ++i) {
            const xed_category_enum_t c = cat_of(k);
            if (c == XED_CATEGORY_CALL && env.noreturn_stubs.count(call_target(k)) != 0) { nr = true; break; }
            if (c == XED_CATEGORY_RET || c == XED_CATEGORY_UNCOND_BR || c == XED_CATEGORY_COND_BR ||
                (k > it->second && is_func(k))) {
               break;
            }
         }
         return local_noreturn[tgt] = nr;
      };
      auto stops = [&](std::size_t k) {   /* no fall-through */
         const xed_category_enum_t c = cat_of(k);
         if (c == XED_CATEGORY_RET || c == XED_CATEGORY_UNCOND_BR) { return true; }
         return noreturn_gate && c == XED_CATEGORY_CALL && !fs.empty() && noreturn_fn(call_target(k));
      };

      /* A call inside an exception call-site range has an edge to its
       * landing pad: the unwinder enters the pad with the call's frame and
       * callee-saved registers. Kill M64_NO_PIC_EH_EDGES; guard pic-eh-edges. */
      static const bool eh_edges = std::getenv("M64_NO_PIC_EH_EDGES") == nullptr;
      std::vector<std::size_t> eh_pad(n, SIZE_MAX);   /* call -> its landing pad instruction */
      if (eh_edges) {
         const auto sites = pic_eh_call_sites(img, env, sect.addr, sect.addr + sect.size);
         std::size_t bad = 0;
         for (std::size_t k = 0; k < n && !sites.empty(); ++k) {
            const std::size_t vm = ins[k]->loc.vmaddr;
            auto r = sites.upper_bound(vm);
            if (cat_of(k) != XED_CATEGORY_CALL || r == sites.begin()) { continue; }
            /* a C library call, a PIC thunk or `call $+0` never unwinds; clang
             * merges the call-site ranges around them (libtogl
             * D3DToGL::Handle_TEX: %ebx = strchr's result at one) */
            const xed_decoded_inst_t& x = ins[k]->xedd;
            std::size_t thunk_after;
            if (env.nothrow_stubs.count(vm + xed_decoded_inst_get_length(&x) +
                                        xed_decoded_inst_get_branch_displacement(&x)) != 0 ||
                thunk_of(k, &thunk_after) != XED_REG_INVALID ||
                (k + 1 < n && anchor_pop(k + 1))) {
               continue;
            }
            --r;
            if (vm >= r->second.first) { continue; }
            auto lp = at.find(r->second.second);
            if (lp == at.end()) { ++bad; continue; }
            eh_pad[k] = lp->second;
         }
         if (bad != 0) {
            fprintf(stderr, "warning: section %s: %zu exception call sites name a landing pad "
                    "that is no instruction; their pads keep the stale-state rule\n",
                    name().c_str(), bad);
         }
      }

      std::vector<char> lead(n, 0), filler(n, 0);
      if (n != 0) { lead[0] = 1; }
      for (std::size_t k = 0; k < n; ++k) {
         if (after_data[k] || is_func(k)) { lead[k] = 1; }
         for_targets(k, [&](std::size_t t) { lead[t] = 1; });
         const xed_category_enum_t c = cat_of(k);
         if (k + 1 < n && (c == XED_CATEGORY_COND_BR || stops(k) || eh_pad[k] != SIZE_MAX ||
                           env.pic_switch_targets.count(ins[k]->loc.vmaddr) != 0)) {
            lead[k + 1] = 1;
         }
         if (eh_pad[k] != SIZE_MAX) { lead[eh_pad[k]] = 1; }
         if (k + 1 < n && stops(k)) {
            /* alignment, and the noreturn-trap idiom `ret; call
             * ___stack_chk_fail` (a call does not revive a dead fall-through;
             * guard 99_pic_anchor_ret_call_dead) */
            std::size_t j = k + 1;
            for (; j < n && !after_data[j] && !is_func(j) && (is_pad(j) || cat_of(j) == XED_CATEGORY_CALL); ++j) {
               filler[j] = 1;
            }
            if (j < n) { lead[j] = 1; }
         }
      }
      std::vector<std::size_t> bstart, blk(n);
      for (std::size_t k = 0; k < n; ++k) {
         if (lead[k]) { bstart.push_back(k); }
         blk[k] = bstart.size() - 1;
      }
      const std::size_t nb = bstart.size();
      bstart.push_back(n);
      constexpr std::size_t NONE = SIZE_MAX;
      std::vector<std::vector<std::size_t>> succs(nb), preds(nb), eh_succs(nb), eh_preds(nb);
      std::vector<char> root(nb, 0);
      for (std::size_t b = 0; b < nb; ++b) {
         const std::size_t k = bstart[b];
         root[b] = b == 0 || after_data[k];
         for_targets(bstart[b + 1] - 1, [&](std::size_t t) {
            succs[b].push_back(blk[t]);
            preds[blk[t]].push_back(b);
         });
         if (eh_pad[bstart[b + 1] - 1] != SIZE_MAX) {
            const std::size_t pb = blk[eh_pad[bstart[b + 1] - 1]];
            eh_succs[b].push_back(pb);
            eh_preds[pb].push_back(b);
         }
      }
      for (std::size_t b = 0; b + 1 < nb; ++b) {
         const std::size_t last = bstart[b + 1] - 1;
         /* a filler no edge reaches leads nowhere (the next block ADOPTS) */
         const bool dead_filler = filler[bstart[b]] && preds[b].empty() && eh_preds[b].empty() && !root[b];
         /* A function symbol is an entry, unless code runs straight into it
          * (a global label inside a function): after alignment or a call
          * (a noreturn helper the image's own code defines) it is not. */
         const std::size_t nk = bstart[b + 1];
         if (is_func(nk) && (is_pad(last) || cat_of(last) == XED_CATEGORY_CALL)) { root[b + 1] = 1; }
         if (!stops(last) && !dead_filler && !root[b + 1]) {
            succs[b].push_back(b + 1);
            preds[b + 1].push_back(b);
         }
      }
      for (std::size_t b = 0; b < nb; ++b) {
         if (is_func(bstart[b]) && preds[b].empty() && eh_preds[b].empty()) { root[b] = 1; }
      }
      /* blocks no edge from a root reaches start from the code before them,
       * except an unnamed function: `push %ebp` (after alignment) opens a frame */
      auto opens_frame = [&](std::size_t b) {
         std::size_t k = bstart[b];
         while (k < n && is_pad(k)) { ++k; }
         return k < n && ins[k]->instbuf[0] == 0x55;
      };
      std::vector<std::size_t> stale(nb, NONE);
      /* live: reached from a root. A live block ignores dead preds (the `jmp`
       * after a throw that rejoins a landing pad's code). Kill
       * M64_NO_PIC_DEAD_PREDS; guard 99_pic_cfg_dead_pred. */
      static const bool dead_preds = std::getenv("M64_NO_PIC_DEAD_PREDS") == nullptr;
      std::vector<char> live(nb, 0);
      {
         std::vector<char> reached(nb, 0);
         std::vector<std::size_t> stack;
         auto flood = [&](std::vector<char>& seen, std::size_t b0) {
            seen[b0] = 1;
            stack.push_back(b0);
            while (!stack.empty()) {
               const std::size_t b = stack.back();
               stack.pop_back();
               for (const auto *ss : {&succs[b], &eh_succs[b]}) {
                  for (const std::size_t s : *ss) {
                     if (!seen[s]) { seen[s] = 1; stack.push_back(s); }
                  }
               }
            }
         };
         for (std::size_t b = 0; b < nb; ++b) { if (root[b]) { flood(reached, b); } }
         for (std::size_t b = 1; b < nb; ++b) {
            if (reached[b]) { continue; }
            if (opens_frame(b)) { root[b] = 1; } else { stale[b] = b - 1; }
            flood(reached, b);
         }
         if (dead_preds) {
            for (std::size_t b = 0; b < nb; ++b) { if (root[b] && !live[b]) { flood(live, b); } }
         }
      }
      auto dead_pred = [&](std::size_t b, std::size_t p) { return live[b] && !live[p]; };
      /* Code after a `ret` that no new function starts is the same
       * function's: a landing pad the exception tables do not name keeps the
       * frame slots and the callee-saved anchors (`push %ebp` opens an unnamed
       * function instead). */
      auto stale_in = [&](std::size_t b, const State& src) {
         std::size_t k = bstart[b];
         while (k < n && is_pad(k)) { ++k; }
         const bool same_fn = sect_func_syms && k < n && !is_func(k) && ins[k]->instbuf[0] != 0x55;
         State t;
         if (cat_of(bstart[b] - 1) != XED_CATEGORY_RET) {
            t = src;
         } else {
            if (slots_past_ret && !fs.empty()) {
               t.slots = src.slots;
               t.past_ret = true;
            }
            if (post_ret_body && same_fn) {   /* the body's callee-saved anchors */
               for (const xed_reg_enum_t r : {XED_REG_EBX, XED_REG_ESI, XED_REG_EDI}) {
                  auto it = src.anchors.find(r);
                  if (it != src.anchors.end()) { t.anchors.insert(*it); }
               }
            }
         }
         if (!t.anchors.empty()) { t.region = true; }
         return t;
      };

      /* ---- the transfer (and, on the final walk, the rewrites) ---- */
      PicDerivedBases<bits> derived;
      auto step = [&](State& s, std::size_t k, bool final_walk) {
         Instruction<bits> *inst = ins[k];
         const xed_decoded_inst_t& xedd = inst->xedd;
         const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);
         const xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
         if (cat != XED_CATEGORY_NOP && cat != XED_CATEGORY_WIDENOP) { s.wild = 0; }

         /* Slots spilled before the anchor pop cannot hold it; they are the
          * prologue's entry saves. */
         const bool is_anchor_pop = anchor_pop(k);
         if (is_anchor_pop) {
            s.slots.clear();
            s.entry_saves = s.pre_saves;
            s.pre_saves.clear();
            s.anchors[xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0)] = inst->loc.vmaddr;
            s.region = true;
            s.past_ret = false;
         }
         std::size_t thunk_vm = 0;
         const xed_reg_enum_t thunk_reg = thunk_of(k, &thunk_vm);

         auto anchor_of = [&](xed_reg_enum_t r) -> std::size_t {
            auto it = s.anchors.find(r);
            return it == s.anchors.end() ? 0 : it->second;
         };
         if (final_walk) {
            if (!s.anchors.empty()) { pic_rewrite_operand(inst, env, anchor_of, trace); }
            if (PicDerivedBases<bits>::on()) { derived.before(inst, env, anchor_of, trace); }
            if (!s.anchors.empty() || s.region) { pic_cancel_absolute(inst, env); }
         }

         /* Frame-slot spill/reload (`mov %reg,disp(%ebp|%esp)` and back). */
         xed_reg_enum_t anchor_keep = XED_REG_INVALID;   /* (re)established here */
         {
            const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
            const xed_reg_enum_t mbase = xed_decoded_inst_get_base_reg(ops, 0);
            if ((mbase == XED_REG_EBP || mbase == XED_REG_ESP) &&
                xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
                xed_decoded_inst_number_of_memory_operands(&xedd) == 1) {
               const Slot slot{mbase, (ssize_t)xed_decoded_inst_get_memory_displacement(ops, 0)};
               const xed_reg_enum_t reg = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
               const bool callee_saved = reg == XED_REG_EBX || reg == XED_REG_ESI || reg == XED_REG_EDI;
               if (iform == XED_IFORM_MOV_MEMv_GPRv) {               /* spill */
                  s.entry_saves.erase(slot);
                  auto a = s.anchors.find(reg);
                  auto pad_slot = s.slots.find(slot);
                  if (a != s.anchors.end()) {
                     s.slots[slot] = a->second;
                     s.pre_saves.erase(slot);
                  } else if (pad_respill && s.past_ret && pad_slot != s.slots.end() && callee_saved) {
                     /* a landing pad re-spilling the unwinder-restored anchor */
                     s.anchors[reg] = pad_slot->second;
                     anchor_keep = reg;
                  } else {
                     s.slots.erase(slot);
                     if (is_gpr32(reg)) { s.pre_saves[slot] = reg; }
                     else { s.pre_saves.erase(slot); }
                  }
               } else if (iform == XED_IFORM_MOV_GPRv_MEMv && is_gpr32(reg)) {   /* reload */
                  auto sl = s.slots.find(slot);
                  auto es = s.entry_saves.find(slot);
                  /* the epilogue restore of the callee-saved anchor register
                   * keeps the anchor for the landing pads after the ret */
                  const bool entry_restore = entry_save_gate && es != s.entry_saves.end() &&
                                             es->second == reg && callee_saved;
                  if (sl != s.slots.end()) {
                     s.anchors[reg] = sl->second;
                     anchor_keep = reg;
                  } else if (entry_restore) {
                     anchor_keep = reg;
                  } else {
                     s.anchors.erase(reg);
                  }
               } else if (slot_write_kill && xed_decoded_inst_mem_written(&xedd, 0)) {
                  /* any other write ends what the slot held */
                  s.slots.erase(slot);
                  s.entry_saves.erase(slot);
                  s.pre_saves.erase(slot);
               }
            }
         }

         /* `xchg %a,%b` swaps the anchors (the kill below clears both). */
         const xed_reg_enum_t xa = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
         const xed_reg_enum_t xb = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
         const bool is_xchg = xchg_swap && xed_decoded_inst_get_iclass(&xedd) == XED_ICLASS_XCHG &&
                              is_gpr32(xa) && is_gpr32(xb) && xa != xb;
         const std::size_t xa_anchor = is_xchg ? anchor_of(xa) : 0;
         const std::size_t xb_anchor = is_xchg ? anchor_of(xb) : 0;

         /* `anchor + X` (see ANCHOR + IMMEDIATE below), judged before the writes */
         xed_reg_enum_t plus_dst = XED_REG_INVALID;
         std::size_t plus_anchor = 0;
         if (anchor_imm) {
            auto pa = [&](xed_reg_enum_t r) -> std::size_t {
               if (auto a = s.anchors.find(r); a != s.anchors.end()) { return a->second; }
               auto p = s.plus.find(r);
               return p == s.plus.end() ? 0 : p->second;
            };
            const xed_reg_enum_t r0 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            const xed_reg_enum_t r1 = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
            const xed_operand_values_t *ops = xed_decoded_inst_operands_const(&xedd);
            if (iform == XED_IFORM_LEA_GPRv_AGEN && inst->instbuf[0] == 0x8D && is_gpr32(r0) &&
                xed_decoded_inst_get_memory_displacement_width(ops, 0) < sizeof(uint32_t)) {
               /* lea d8(%a,%i,s): a disp32 form is anchored itself, and an
                * index-less one is an anchor-derived base (PicDerivedBases) */
               const xed_reg_enum_t b = xed_decoded_inst_get_base_reg(ops, 0);
               const xed_reg_enum_t i = xed_decoded_inst_get_index_reg(ops, 0);
               if (is_gpr32(b) && pa(b) != 0 && is_gpr32(i) && pa(i) == 0) {
                  plus_dst = r0;
                  plus_anchor = pa(b);
               }
            } else if ((iform == XED_IFORM_ADD_GPRv_GPRv_01 || iform == XED_IFORM_ADD_GPRv_GPRv_03) &&
                       is_gpr32(r0) && is_gpr32(r1) && (pa(r0) == 0) != (pa(r1) == 0)) {
               plus_dst = r0;
               plus_anchor = pa(r0) != 0 ? pa(r0) : pa(r1);
            } else if ((iform == XED_IFORM_MOV_GPRv_GPRv_89 || iform == XED_IFORM_MOV_GPRv_GPRv_8B) &&
                       is_gpr32(r0) && s.plus.count(r1) != 0) {
               plus_dst = r0;
               plus_anchor = s.plus.at(r1);
            }
            for_each_written_gpr32(xedd, [&](xed_reg_enum_t r) { s.plus.erase(r); });
         }
         /* Any other definition of a register ends its anchor, except a pop
          * (the epilogue restores that leave the landing pads their anchors). */
         if (!is_anchor_pop && cat != XED_CATEGORY_POP && !s.anchors.empty()) {
            for_each_written_gpr32(xedd, [&](xed_reg_enum_t r) {
               if (r != anchor_keep) { s.anchors.erase(r); }
            });
         }
         if (xb_anchor != 0) { s.anchors[xa] = xb_anchor; }
         if (xa_anchor != 0) { s.anchors[xb] = xa_anchor; }

         /* `mov %src,%dst` copies the anchor (the kill already cleared %dst). */
         if (iform == XED_IFORM_MOV_GPRv_GPRv_89 || iform == XED_IFORM_MOV_GPRv_GPRv_8B) {
            const xed_reg_enum_t mdst = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            const xed_reg_enum_t msrc = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
            if (is_gpr32(mdst) && mdst != msrc) {
               const std::size_t a = anchor_of(msrc);
               if (a != 0) { s.anchors[mdst] = a; }
               else { s.anchors.erase(mdst); }
            }
         }
         if (plus_dst != XED_REG_INVALID && s.anchors.count(plus_dst) == 0) { s.plus[plus_dst] = plus_anchor; }
         if (final_walk) { derived.after(inst); }

         /* int3 is transparent; other traps end everything. */
         const bool is_pic_call_zero = iform == XED_IFORM_CALL_NEAR_RELBRz &&
                                       xed_decoded_inst_get_branch_displacement(&xedd) == 0;
         if ((cat == XED_CATEGORY_INTERRUPT && xed_decoded_inst_get_iclass(&xedd) != XED_ICLASS_INT3) ||
             cat == XED_CATEGORY_SYSCALL || cat == XED_CATEGORY_SYSRET) {
            s = State{};
            derived.clear();
         } else if (cat == XED_CATEGORY_CALL && !is_pic_call_zero) {
            for (const xed_reg_enum_t r : {XED_REG_EAX, XED_REG_ECX, XED_REG_EDX}) {
               s.anchors.erase(r);
               s.plus.erase(r);
               if (call_adopt) { s.wild |= wbit(r); }
            }
         }
         if (thunk_reg != XED_REG_INVALID) {
            s.anchors[thunk_reg] = thunk_vm;
            s.wild &= ~wbit(thunk_reg);
            s.region = true;
         }
      };

      /* ---- the fixpoint ---- */
      std::vector<std::optional<State>> out(nb);
      auto block_in = [&](std::size_t b) -> std::optional<State> {
         if (root[b]) { return State{}; }
         std::optional<State> in;
         auto add = [&](const State& st) {
            if (!in) { in = st; } else { meet(*in, st); }
         };
         for (const std::size_t p : preds[b]) {
            if (out[p] && !dead_pred(b, p)) { add(*out[p]); }
         }
         for (const std::size_t p : eh_preds[b]) {
            if (!out[p] || dead_pred(b, p)) { continue; }
            State t = *out[p];   /* after the call: its caller-saved registers are gone */
            t.wild = 0;
            t.past_ret = true;   /* a landing pad (pad respill) */
            add(t);
         }
         if (stale[b] != NONE && out[stale[b]]) { add(stale_in(b, *out[stale[b]])); }
         return in;
      };
      std::size_t visits = 0;
      auto solve = [&]() {
         out.assign(nb, std::nullopt);
         std::set<std::size_t> work;
         for (std::size_t b = 0; b < nb; ++b) { work.insert(work.end(), b); }
         const std::size_t cap = visits + 64 * nb + 1024;
         while (!work.empty()) {
            const std::size_t b = *work.begin();
            work.erase(work.begin());
            if (++visits > cap) {
               fprintf(stderr, "warning: section %s: PIC anchor dataflow did not settle after %zu "
                       "block visits (%zu blocks); using the linear walk\n",
                       name().c_str(), visits, nb);
               return false;
            }
            std::optional<State> s = block_in(b);
            if (!s) { continue; }   /* nothing reaches it yet */
            for (std::size_t k = bstart[b]; k < bstart[b + 1]; ++k) { step(*s, k, false); }
            if (out[b] && *out[b] == *s) { continue; }
            out[b] = std::move(s);
            for (const std::size_t t : succs[b]) { work.insert(t); }
            for (const std::size_t t : eh_succs[b]) { work.insert(t); }
            if (b + 1 < nb && stale[b + 1] == b) { work.insert(b + 1); }
         }
         return true;
      };
      if (!solve()) { return false; }

      /* ANCHOR + IMMEDIATE: LLVM folds the anchor-relative offset of the data
       * into an ALU immediate, so no displacement names it:
       *    lea (%anchor,%i,4),%t ; add $T-anchor,%t           &table[i] (fuzz _t26)
       *    mov %anchor,%t        ; adc $T-anchor,%t           &g[cond]  (fuzz _t38)
       *    mov %anchor,%t ; add %i,%t ; add $T-anchor,%t      &g[i]     (fuzz _t20)
       * %t holds `anchor + X` (State::plus; X no address), and the translated
       * anchor register holds the translated return address, so the immediate
       * becomes T' - anchor' (Immediate::minus) and %t = T' + X as in i386.
       * Only when T is data and nothing reads the arithmetic's flags. */
      constexpr unsigned STATUS = 0x8D5;   /* CF PF AF ZF SF OF */
      auto flags_dead = [&](std::size_t j, std::size_t end) {
         for (; j < end; ++j) {
            const xed_simple_flag_t *f = xed_decoded_inst_get_rflags_info(&ins[j]->xedd);
            if (f != nullptr && (xed_flag_set_mask(xed_simple_flag_get_read_flag_set(f)) & STATUS)) {
               return false;
            }
            const xed_category_enum_t c = cat_of(j);
            if (c == XED_CATEGORY_CALL || c == XED_CATEGORY_RET) { return true; }
            if (f != nullptr && xed_simple_flag_get_must_write(f) &&
                ((xed_flag_set_mask(xed_simple_flag_get_written_flag_set(f)) |
                  xed_flag_set_mask(xed_simple_flag_get_undefined_flag_set(f))) & STATUS) == STATUS) {
               return true;
            }
         }
         return false;   /* the successor may read them */
      };
      auto anchor_relative_imm = [&](const State& s, std::size_t k, std::size_t end) {
         Instruction<bits> *op = ins[k];
         const xed_decoded_inst_t& x = op->xedd;
         switch (xed_decoded_inst_get_iform_enum(&x)) {
         case XED_IFORM_ADD_GPRv_IMMz: case XED_IFORM_ADD_OrAX_IMMz:
         case XED_IFORM_ADC_GPRv_IMMz: case XED_IFORM_ADC_OrAX_IMMz:
            break;
         default:
            return;
         }
         const xed_reg_enum_t t = xed_decoded_inst_get_reg(&x, XED_OPERAND_REG0);
         if (!is_gpr32(t) || xed_decoded_inst_get_operand_width(&x) != 32 ||
             xed_decoded_inst_get_immediate_width(&x) != sizeof(uint32_t)) {
            return;
         }
         std::size_t a = 0;
         if (auto it = s.anchors.find(t); it != s.anchors.end()) { a = it->second; }
         else if (auto it = s.plus.find(t); it != s.plus.end()) { a = it->second; }
         const std::size_t target = a + (std::size_t)(ssize_t)xed_decoded_inst_get_signed_immediate(&x);
         if (a == 0 || !(env.vmaddr_in_writable_data(target) || env.vmaddr_in_readonly_opaque_data(target)) ||
             !flags_dead(k + 1, end)) {
            return;
         }
         SectionBlob<bits> *tb = env.add_placeholder(target);
         if (tb == nullptr) { return; }
         if (op->imm == nullptr) {
            uint32_t raw;
            std::memcpy(&raw, &op->instbuf[op->instbuf.size() - sizeof raw], sizeof raw);
            op->imm = Immediate<bits>::Create(raw);
         } else if (op->imm->pointee != nullptr) {   /* the parser's pointer guess */
            env.vmaddr_resolver.cancel((std::size_t)op->imm->value,
                                       (const SectionBlob<bits> **)&op->imm->pointee);
            env.vmaddr_resolver.cancel_containing((std::size_t)op->imm->value,
                                                  (const SectionBlob<bits> **)&op->imm->pointee);
         }
         op->imm->heuristic = false;
         op->imm->pointee = tb;
         op->imm->pointee_offset = 0;
         env.vmaddr_resolver.resolve_containing(target, (const SectionBlob<bits> **)&op->imm->pointee,
                                                &op->imm->pointee_offset, /*override=*/true);
         env.vmaddr_resolver.resolve(a, (const SectionBlob<bits> **)&op->imm->minus);
         if (trace) {
            fprintf(stderr, "[anchor] inst=0x%zx imm=%s anchor=0x%zx target=0x%zx iform=%s\n",
                    (size_t)op->loc.vmaddr, xed_reg_enum_t2str(t), a, target,
                    xed_iform_enum_t2str(xed_decoded_inst_get_iform_enum(&x)));
         }
      };

      /* ---- the rewrite ---- */
      for (std::size_t b = 0; b < nb; ++b) {
         State s = block_in(b).value_or(State{});
         if (trace) {
            fprintf(stderr, "[anchor] block 0x%zx preds=%zu%s%s eh=%zu regs=%zu slots=%zu <-",
                    (size_t)ins[bstart[b]]->loc.vmaddr, preds[b].size(),
                    root[b] ? " root" : "", stale[b] != NONE ? " stale" : "", eh_preds[b].size(),
                    s.anchors.size(), s.slots.size());
            for (const auto& [r, a] : s.anchors) { fprintf(stderr, " %s", xed_reg_enum_t2str(r)); }
            for (const auto& [sl, a] : s.slots) { fprintf(stderr, " [%zd]", (ssize_t)sl.second); }
            fprintf(stderr, " |");
            for (const auto *ps : {&preds[b], &eh_preds[b]}) {
               for (const std::size_t p : *ps) {
                  fprintf(stderr, " %s0x%zx/%zu", ps == &eh_preds[b] ? "eh:" : "",
                          (size_t)ins[bstart[p]]->loc.vmaddr, out[p] ? out[p]->anchors.size() : (size_t)-1);
               }
            }
            fprintf(stderr, "\n");
         }
         /* derived bases are straight-line: only a plain fall-through keeps them */
         if (!(preds[b].size() == 1 && preds[b][0] + 1 == b && stale[b] == NONE && eh_preds[b].empty())) {
            derived.clear();
         }
         for (std::size_t k = bstart[b]; k < bstart[b + 1]; ++k) {
            if (anchor_imm) { anchor_relative_imm(s, k, bstart[b + 1]); }
            step(s, k, true);
         }
      }
      if (trace) {
         fprintf(stderr, "[anchor] cfg %s: %zu blocks, %zu visits\n", name().c_str(), nb, visits);
      }
      return true;
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
      static const bool lea_sum = std::getenv("M64_NO_JT_LEA_SUM") == nullptr;

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
      /* A dispatch whose table base the walk cannot see: GCC hoists a loop's
       * table base below the switch, so it reaches the dispatch only through
       * a back edge (register) or a frame slot written there (expat in
       * iMovie's MobileMe, 63 dispatches). Such a dispatch is claimed after
       * the walk when an anchor-relative lea elsewhere computes the address
       * just past its jmp. Kill M64_NO_JT_BASE_EVIDENCE; guard
       * 99_jt_base_after_dispatch. */
      static const bool base_evidence = std::getenv("M64_NO_JT_BASE_EVIDENCE") == nullptr;
      constexpr std::size_t unknown_base = SIZE_MAX;
      struct Deferred { std::size_t dispatch, end, anchor; };
      std::vector<Deferred> deferred;
      std::set<std::size_t> lea_bases;   /* every in-section lea [anchor+d] / [rip+d] target */
      std::size_t case_bound = 0, case_bound_at = 0;   /* last `cmp $N,%reg`: N+1 at vmaddr */
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

      /* Claim the table at `table_base` for the dispatch at `dispatch`: it is
       * auto-sized and every entry must land in the dispatching function.
       * Returns the entry count (claimed when >= 2). */
      auto claim = [&](std::size_t table_base, std::size_t anchor, std::size_t dispatch) {
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
            auto nx = env.func_syms.upper_bound(dispatch);
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
            table_dispatch[table_base] = dispatch;
            if (trace) {
               fprintf(stderr, "[jumptable] dispatch@0x%zx anchor=0x%zx table=0x%zx count=%zu\n",
                       (size_t)dispatch, (size_t)anchor, (size_t)table_base, (size_t)i);
            }
         }
         return i;
      };

      /* GCC PIC thunks: `mov (%esp),%reg; ret` — or, translated, the
       * `mov r11d,[rsp]` that begins the translated return. */
      std::unordered_map<std::size_t, xed_reg_enum_t> pic_thunks;
      const auto scan_thunks = [&](std::size_t soff, std::size_t saddr, std::size_t ssize) {
         std::size_t pit = soff, pvm = saddr;
         const std::size_t pend = soff + ssize;
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
      };
      /* Translated code calls a thunk wherever it lives: GCC puts
       * ___i686.get_pc_thunk.bx in __textcoal_nt, so a __text-only scan found
       * no anchor and the x86_64 re-parse decoded every GCC PIC table as code;
       * a table byte that read as a short jcc re-encoded wider and shifted the
       * entries after it (Halo's digit-key switch at 0x2ee904: SIGSEGV). Kill
       * M64_NO_JT_XSECT_THUNK; guard jt-xsect-thunk. */
      static const bool xsect_thunk = std::getenv("M64_NO_JT_XSECT_THUNK") == nullptr;
      if (bits == Bits::M64 && xsect_thunk) {
         for (Segment<bits> *seg : env.archive.segments()) {
            for (Section<bits> *other : seg->sections) {
               if (other->sect.offset == 0 ||
                   !(other->sect.flags & (S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS))) {
                  continue;
               }
               scan_thunks(other->sect.offset, other->sect.addr, other->sect.size);
            }
         }
      } else {
         scan_thunks(sect.offset, sect.addr, sect.size);
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
      /* ORPHAN code (DetectPicAnchoredDisps' rule): after a ret/jmp, a block
       * no recorded edge reaches yet (a loop entered by a forward jmp to its
       * test, whose body sits after an early epilogue). It runs on the stale
       * linear state, so its branches record nothing and the next join ADOPTS
       * its snapshot instead of intersecting with the stale state. Portal 2
       * engine: two dispatches after such a loop lost their spilled anchor.
       * Kill M64_NO_JT_ORPHAN; guard 99_jt_orphan_loop. */
      static const bool orphan_on = std::getenv("M64_NO_JT_ORPHAN") == nullptr;
      bool orphan = false;
      bool prev_tcall = false;   /* prev_cat is the jmp of a translated call: it returns */
      /* dispatch -> table base claimed from lea evidence after a walk. Until
       * then the walk decoded that table as instructions, which can drop the
       * anchor for every later dispatch of the function, so the walk is
       * repeated, stepping over these tables, until it claims nothing new. */
      std::map<std::size_t, std::size_t> evidence_claims;
      for (int round = 0; round < 4; ++round) {
      const std::size_t claims_before = evidence_claims.size();
      back_snap.clear();
      deferred.clear();
      for (int pass = 0; pass < 2; ++pass) {
         if (pass == 1 && !pic_any_nonempty(back_snap)) { break; }
         anchors.clear(); tbl_addr.clear(); tbl_val.clear(); pend_r11 = 0;
         stack_tbl.clear(); stack_anchor.clear(); esp_off = 0;
         pending_targets.clear(); anchor_snap.clear(); slot_snap.clear();
         prev_cat = XED_CATEGORY_INVALID; prev_call0 = false; orphan = false; prev_tcall = false;
         /* An intra-function edge from `at`: a forward target stays pending
          * and joins this state there; a backward one joins on the second walk. */
         /* An orphan's forward edge records no snapshot, but its target is
          * still this function: a `ret` before it must not end the walk's
          * state (`jmp *%esi` into a block that jumps over a `ret` to a
          * dispatch lost its anchor). Kill M64_NO_JT_ORPHAN_PENDING; guard
          * jt-orphan-pending. */
         static const bool orphan_pending = std::getenv("M64_NO_JT_ORPHAN_PENDING") == nullptr;
         auto join_edge = [&](std::size_t at, std::size_t tgt, bool forward) {
            if (orphan) {
               if (orphan_pending && forward && tgt < sect_hi && tgt - at <= 0x10000) {
                  pending_targets.insert(tgt);
               }
               return;
            }
            if (forward && tgt < sect_hi && tgt - at <= 0x10000) {
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
            } else if (!forward && pass == 0 && back_edge && tgt >= sect_lo &&
                       at - tgt <= 0x10000 && pic_same_function(env.func_syms, tgt, at)) {
               snapshot_for(back_snap, tgt, anchors);
            }
         };
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
               prev_call0 = false; orphan = false;
            }
            if (pass == 1) {
               auto b = back_snap.find(vmaddr);
               if (b != back_snap.end()) { snapshot_for(anchor_snap, vmaddr, b->second); }
            }
            const bool dead_ft = prev_cat == XED_CATEGORY_RET || prev_cat == XED_CATEGORY_UNCOND_BR;
            const bool joined = anchor_snap.count(vmaddr) != 0 || slot_snap.count(vmaddr) != 0;
            const bool adopt = dead_ft || orphan;
            if (orphan_on && env.func_syms.count(vmaddr) == 0) {
               orphan = joined ? false : (orphan || (dead_ft && !prev_tcall));
            }
            if (!anchor_snap.empty()) {
               auto sn = anchor_snap.find(vmaddr);
               if (sn != anchor_snap.end()) {
                  if (adopt) {
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
                  if (adopt) {
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
               orphan = false;
               tbl_addr.erase(reg0); tbl_val.erase(reg0);
               sets_state = true;
            } else if (iform == XED_IFORM_MOV_GPRv_MEMv && pend_r11 != 0 && vmaddr == pend_r11 &&
                       xed_decoded_inst_get_base_reg(ops, 0) == XED_REG_RSP &&
                       xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
                       is_gpr32(reg0)) {
               /* translated anchor: the `mov %reg,[rsp]` the translated
                * `call $+0` returns to */
               anchors[reg0] = pend_r11;
               orphan = false;
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
                     lea_bases.insert(vmaddr + len + disp);
                  } else {
                     auto a = anchors.find(jt_norm32(base));
                     const std::size_t cand = a != anchors.end() ? (std::size_t)((ssize_t)a->second + disp) : 0;
                     if (a != anchors.end() && cand >= sect_lo && cand < sect_hi) {
                        tbl_addr[reg0] = cand;
                        tbl_val.erase(reg0);
                        sets_state = true;
                        lea_bases.insert(cand);
                     }
                  }
               } else if (lea_sum && is_gpr32(reg0) && disp == 0 &&
                          xed_operand_values_get_scale(ops) == 1) {
                  /* `lea %d,(%t,%anchor)`: the -O0 spelling of `add %t,%anchor`
                   * (Portal's libSDL2 SDL_GL_SetAttribute). Scale 1 makes the
                   * fields symmetric, so either one may hold the entry. Kill
                   * M64_NO_JT_LEA_SUM; guard 99_jt_lea_sum_dispatch. */
                  const xed_reg_enum_t index = jt_norm32(xed_decoded_inst_get_index_reg(ops, 0));
                  const xed_reg_enum_t b32 = jt_norm32(base);
                  for (const auto& [t, an] : {std::pair{b32, index}, std::pair{index, b32}}) {
                     auto tv = tbl_val.find(t);
                     auto a = anchors.find(an);
                     if (tv != tbl_val.end() && tv->second.second == 0 && a != anchors.end()) {
                        const std::size_t table = tv->second.first;
                        tbl_val[reg0] = { table, a->second };
                        tbl_addr.erase(reg0);
                        sets_state = true;
                        break;
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
                  } else if (base_evidence && is_gpr32(base) &&
                             xed_decoded_inst_get_memory_displacement(ops, 0) == 0) {
                     tbl_val[reg0] = { unknown_base, 0 };
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
               /* `add %t,%anchor`: the entry becomes a case target. clang -O0
                * adds the other way, `add %anchor,%t` then `jmp *%anchor`.
                * Kill M64_NO_JT_ADD_SWAP; guard jt-add-swap. */
               static const bool add_swap = std::getenv("M64_NO_JT_ADD_SWAP") == nullptr;
               const xed_reg_enum_t src = jt_norm32(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1));
               auto tv = tbl_val.find(reg0);
               auto a = anchors.find(src);
               auto ts = tbl_val.find(src);
               auto ad = anchors.find(reg0);
               if (tv != tbl_val.end() && a != anchors.end()) {
                  tv->second.second = a->second;
                  sets_state = true;
               } else if (add_swap && ts != tbl_val.end() && ts->second.second == 0 &&
                          ad != anchors.end()) {
                  tbl_val[reg0] = { ts->second.first, ad->second };
                  tbl_addr.erase(reg0);
                  sets_state = true;
               }
            } else if (iform == XED_IFORM_JMP_GPRv) {
               auto tv = tbl_val.find(reg0);
               if (tv != tbl_val.end() && tv->second.second != 0) {
                  std::size_t table_base = tv->second.first;
                  if (table_base == unknown_base) {
                     auto ec = evidence_claims.find(vmaddr);
                     if (ec == evidence_claims.end()) {
                        deferred.push_back({ vmaddr, vmaddr + len, tv->second.second });
                     } else {
                        table_base = ec->second;
                     }
                  }
                  const std::size_t i =
                     table_base == unknown_base ? 0 : claim(table_base, tv->second.second, vmaddr);
                  /* Its case bodies are branch targets: a forward one stays
                   * pending (a `ret` before it does not end the function) and
                   * joins this state, like any forward branch; a backward one
                   * joins on the second walk. Portal 2 libtier0: an inner
                   * switch in a case body placed after the early epilogue lost
                   * the anchor and was never claimed. Kill M64_NO_JT_CASE_SNAP;
                   * guard 99_jt_case_after_ret. */
                  static const bool case_snap = std::getenv("M64_NO_JT_CASE_SNAP") == nullptr;
                  /* The auto-sized count can run on into the next table of the
                   * function (clamped only after the walk), whose targets
                   * belong to another dispatch: stop at a known table base,
                   * and at the bound of a range check just before (`cmp
                   * $N,%idx; ja default` = N+1 cases). */
                  std::size_t ncase = i;
                  if (case_bound != 0 && vmaddr - case_bound_at <= 32) { ncase = std::min(ncase, case_bound); }
                  for (std::size_t k = 0; case_snap && k < ncase; ++k) {
                     const std::size_t slot = table_base + k * 4;
                     if (k > 0 && tables.count(slot) != 0) { break; }
                     const std::size_t tgt =
                        tv->second.second + (int32_t)img.at<uint32_t>(sect.offset + (slot - sect.addr));
                     join_edge(vmaddr, tgt, tgt > vmaddr);
                  }
                  if (i >= 2) {
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

            /* `cmp $N,%idx` before a dispatch's `ja default` bounds its cases */
            if ((iform == XED_IFORM_CMP_GPRv_IMMb || iform == XED_IFORM_CMP_GPRv_IMMz ||
                 iform == XED_IFORM_CMP_OrAX_IMMz) && is_gpr32(reg0)) {
               const xed_int64_t n = xed_decoded_inst_get_signed_immediate(&xedd);
               case_bound = n >= 0 && n < 0x10000 ? (std::size_t)n + 1 : 0;
               case_bound_at = vmaddr;
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
               if (bd != 0) { join_edge(vmaddr, tgt, bd > 0); }
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
               if (t != pic_thunks.end()) { anchors[t->second] = vmaddr + len; orphan = false; }
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
               prev_tcall = bits == Bits::M64 && is_call;   /* the translated call's jmp */
            }
            it += len;
            vmaddr += len;
         }
      }

      /* The deferred dispatches: an inline table starts within the alignment
       * padding after its jmp, at an address some lea computes. */
      for (const Deferred& d : deferred) {
         auto b = lea_bases.lower_bound(d.end);
         if (b == lea_bases.end() || *b - d.end >= 16 || evidence_claims.count(d.dispatch) != 0) {
            continue;
         }
         if (tables.count(*b) != 0 || claim(*b, d.anchor, d.dispatch) >= 2) {
            evidence_claims[d.dispatch] = *b;
         }
      }
      if (evidence_claims.size() == claims_before) { break; }
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
                       (size_t)(sect.addr + sect.size));
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
