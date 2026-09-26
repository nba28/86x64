#include <typeinfo>
#include <cassert>
#include <cstring>
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
      const bool trace = std::getenv("MACHO_TRACE_CSTRPOOL") != nullptr;
#define CSTRPOOL_REJECT(why, POS)                                           \
      do {                                                                  \
         if (trace) {                                                       \
            const std::size_t pos_ = (POS);                                 \
            fprintf(stderr, "[cstrpool] reject @0x%zx: %s [", pos_, (why)); \
            for (std::size_t k = 0; k < 12 && pos_ + k < fileoff + size; ++k) \
               fprintf(stderr, "%02x ", img.template at<uint8_t>(pos_ + k)); \
            fprintf(stderr, "]\n");                                         \
         }                                                                  \
         return false;                                                      \
      } while (0)

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
               if (!(c >= 0x20 && c < 0x7f)) { CSTRPOOL_REJECT("nonprintable in run", fileoff + j); }
               ++j;
            }
            if (j == size) { CSTRPOOL_REJECT("unterminated tail run", fileoff + i); }
            ++strings;
            i = j + 1;                /* step over the terminating NUL */
            continue;
         }
         CSTRPOOL_REJECT("neither NOP padding nor printable string", fileoff + i);
      }
      return strings > 0;
#undef CSTRPOOL_REJECT
   }

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
         /* Synthesized x86_64 trampolines for classic __IMPORT,__jump_table
          * UNDEFINED stubs (see JumpStubBlob / Dysymtab::lift_jump_table_targets):
          * each is `ff 25 disp32` = `jmp [rip+slot]`. Decoding them as code on
          * any reparse (modify/convert) re-resolves the rip-relative slot
          * reference through the existing FF25 handling so the displacement
          * tracks the final layout. */
         "__jt_tramp",
      };
      for (const std::string& sectname : text_sectnames) {
         if (sectname == sect.sectname) {
            return new Section<bits>(img, offset, env, TextParser);
         }
      }

      if (std::string(sect.sectname) == SECT_STUB_HELPER) {
         return new Section<bits>(img, offset, env, StubHelperParser);
      }

      /* Our own runtime-bind metadata section (slot vmaddrs + symbol names; see
       * Archive::inject_xrel_section / objc_slide.c). On a later modify/convert
       * reparse, parse it back into a live XrelBlob that RE-RESOLVES each slot to
       * the blob now at that vmaddr — so this build's Emit re-emits the slot's
       * post-re-layout address. (A plain DataBlob round-trip froze the baked
       * absolute slot vmaddrs; when the EXECUTE->DYLIB convert then shifts every
       * section by the header-size delta, the binds went stale — e.g. the
       * cfstring isa binds missed their records, staying isa=0 -> __CF_IS_OBJC
       * trap. We read the structured table explicitly, so unlike DataParser we
       * never pointer-detect a mangled-name byte.) Only M64 carries this section;
       * the M32 input never has one. */
      if (std::string(sect.sectname,
                      strnlen(sect.sectname, sizeof(sect.sectname))) == "__86x64_xrel") {
         if constexpr (bits == Bits::M64) {
            return new Section<bits>(img, offset, env, XrelBlob<bits>::Parse);
         } else {
            return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
         }
      }

      /* The runtime abs32-slide site table (Archive::inject_abs32_section /
       * objc_slide.c patch_text_abs32). Same re-parse contract as __86x64_xrel:
       * lift it back into a live Abs32Blob that RE-RESOLVES each field to the
       * blob now containing it, so a later stage's re-layout re-emits correct
       * addresses instead of freezing stale ones (the __86x64_pcmap drift
       * lesson). Only M64 carries this section. */
      if (std::string(sect.sectname,
                      strnlen(sect.sectname, sizeof(sect.sectname))) == "__86x64_abs32") {
         if constexpr (bits == Bits::M64) {
            return new Section<bits>(img, offset, env, Abs32Blob<bits>::Parse);
         } else {
            return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
         }
      }

      /* The M32 constant-classification provenance table
       * (Archive::inject_cpin_section; consumed by ParseEnv::const_pin_slots via
       * Archive's pre-parse lift). Same re-parse contract as __86x64_xrel /
       * __86x64_abs32: lift it into a live ConstPinBlob that RE-RESOLVES each
       * pinned slot to the blob now there, so a later stage re-emits current
       * addresses. Routing it away from DataParser is doubly required: its
       * entries ARE in-image vmaddrs, so the generic pointer detection would
       * "rebase" the table describing what must not be rebased. Only M64
       * carries this section. */
      if (std::string(sect.sectname,
                      strnlen(sect.sectname, sizeof(sect.sectname))) == "__86x64_cpin") {
         if constexpr (bits == Bits::M64) {
            return new Section<bits>(img, offset, env, ConstPinBlob<bits>::Parse);
         } else {
            return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
         }
      }

      /* The C++ exception PC map + per-function LSDA table
       * (Archive::inject_pcmap_section / inject_ehlsda_section; consumed by
       * libabiconv eh_shim.c). Same re-parse contract as __86x64_xrel /
       * __86x64_abs32: lift each back into a live blob whose Parse re-resolves
       * the TRANSLATED side of every row (old __text vmaddr + trans_off -> the
       * instruction blob now there) and keeps the ORIGINAL-i386 side verbatim.
       * The generic DataParser route these used to take pointer-detects any
       * 4-byte word aliasing a segment vmaddr — for an input image based inside
       * the M64 layout window the frozen i386 `orig` fields alias, so a later
       * stage's layout shift re-emitted them "rebased": the deployed pcmap
       * mapped translated PCs to WRONG original PCs and every C++ throw found
       * the wrong LSDA/landing pad (guard pcmap_stage_shift_test.sh). Only M64
       * carries these sections. */
      if (std::string(sect.sectname,
                      strnlen(sect.sectname, sizeof(sect.sectname))) == "__86x64_pcmap") {
         if constexpr (bits == Bits::M64) {
            return new Section<bits>(img, offset, env, PcmapBlob<bits>::Parse);
         } else {
            return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
         }
      }
      if (std::string(sect.sectname,
                      strnlen(sect.sectname, sizeof(sect.sectname))) == "__86x64_ehlsda") {
         if constexpr (bits == Bits::M64) {
            return new Section<bits>(img, offset, env, EhlsdaBlob<bits>::Parse);
         } else {
            return new Section<bits>(img, offset, env, DataBlob<bits>::Parse);
         }
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
      } else if (stype == S_REGULAR || stype == S_COALESCED) {
         /*
          * S_COALESCED (0xb) is the linker-dedup type for WEAK symbols — C++
          * template/inline instantiations. Its *content* is ordinary
          * initialized data, identical to S_REGULAR, so it must take the same
          * path: __DATA,__const_coal holds RTTI typeinfo objects (i386 layout
          * {vtable@0, __name@4, __base_type@8}) and __DATA,__datacoal_nt holds
          * coalesced globals — both carry INTERNAL pointers (typeinfo __name ->
          * a __ZTS string in __TEXT,__const_coal, __base_type -> another __ZTI,
          * vtable slots -> code) that are baked absolute in a non-PIE i386 exec
          * and must be pointer-detected + rebased to the shifted x86_64 layout.
          * Falling through to the opaque DataBlob default (below) left every
          * typeinfo __name a STALE i386 vmaddr; the runtime slide_data_fnptrs
          * then skips it (value is below the translated image base), so
          * type_info::name() returns a wild/NULL pointer and boost::python's
          * strcmp-keyed converter registry SIGSEGVs on registration (Civ IV
          * s24: strcmp(0,0) in _Rb_tree<...registration>::insert_unique reading
          * std::string / std::wstring typeinfo names).  The is_data /
          * is_text_const gate inside this block keeps the treatment SECTION-
          * granular: __DATA,__const_coal / __datacoal_nt -> DataParser (fixed),
          * while __TEXT,__const_coal (the __ZTS cstrings) and __TEXT,__eh_frame
          * (both S_COALESCED, in the executable segment) stay opaque as before.
          * __TEXT,__textcoal_nt (S_COALESCED + instruction attrs, real code) is
          * routed to TextParser by the NAME whitelist above and never reaches
          * here.  Universal: any C++ binary with weak typeinfos/vtables/statics.
          *
          * Structural code-section detection, complementing the NAME
          * whitelist above (which exists because __StaticInit doesn't always
          * SET the attribute — a false-NEGATIVE concern). The attribute being
          * PRESENT is definitive the other way: S_ATTR_PURE_INSTRUCTIONS on an
          * S_REGULAR section in an EXECUTABLE segment means "only true machine
          * instructions" (Mach-O semantics) — translate it as code. Without
          * this, a code section under a name outside the whitelist passes
          * through as raw bytes: its i386 `ret` executes as an x86_64 8-byte
          * pop and control lands on a FUSED return address (observed:
          * tests-i386/62, a get_pc_thunk in a custom __TEXT,__picthunk
          * section). Applies at both bits — the M64 reparse (modify/convert)
          * must re-decode such sections so rip-relative disps re-resolve
          * against the final layout, exactly as __text does. Synthesized
          * metadata sections (__86x64_pcmap/__86x64_xrel, flags 0) and
          * __eh_frame (S_COALESCED, no instr attrs) are unaffected.
          */
         if (std::getenv("MACHO_TRACE_SECTROUTE")) {
            fprintf(stderr, "[sectroute] %.16s,%.16s flags=0x%x off=0x%x size=0x%x "
                            "pure=%d exec=%d cstrpool=%d\n",
                    sect.segname, sect.sectname, flags,
                    (unsigned)sect.offset, (unsigned)sect.size,
                    (flags & S_ATTR_PURE_INSTRUCTIONS) != 0,
                    env.current_segment != nullptr &&
                       (env.current_segment->segment_command.initprot & VM_PROT_EXECUTE) ? 1 : 0,
                    (int)SectionIsNopPaddedCstringPool<bits>(img, sect.offset, sect.size));
         }
         if ((flags & S_ATTR_PURE_INSTRUCTIONS) != 0 &&
             env.current_segment != nullptr &&
             (env.current_segment->segment_command.initprot & VM_PROT_EXECUTE) &&
             /* ...unless the CONTENT proves it is a NOP-padded literal pool
              * rather than code, in which case the instruction attributes
              * describe the linker's alignment filler. See
              * SectionIsNopPaddedCstringPool. */
             !SectionIsNopPaddedCstringPool<bits>(img, sect.offset, sect.size)) {
            return new Section<bits>(img, offset, env, TextParser);
         }
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
      /* __TEXT-resident data (i.e. __TEXT,__const) hosts switch jump tables
       * whose entries point at MID-function basic blocks — no func_syms entry —
       * so the exec-target function-entry gate below must not apply there. */
      bool is_text_const_sect = false;
      /* ObjC1 method lists (__OBJC,__inst_meth/__cls_meth/__cat_*_meth) hold
       * {name, types, imp} triples after a small header, so a code-target
       * word there IS a method IMP — a function entry by construction, the
       * positive evidence the code-entry gate asks for. iPhoto's locals-
       * stripped methods have no nlist and many no `55 89 e5` prologue, so
       * 1.7k IMPs stayed raw i386 addresses. Guard objc-imp-entry; OFF arm
       * M64_NO_OBJC_IMP_ENTRY=1. */
      bool in_objc_methods = false;
      if (env.current_section != nullptr) {
         const auto& cs = env.current_section->sect;
         in_objc_symbols =
            strncmp(cs.segname, SEG_OBJC, sizeof(cs.segname)) == 0 &&
            strncmp(cs.sectname, "__symbols", sizeof(cs.sectname)) == 0;
         static const bool objc_imp_off =
            std::getenv("M64_NO_OBJC_IMP_ENTRY") != nullptr;
         if (!objc_imp_off &&
             strncmp(cs.segname, SEG_OBJC, sizeof(cs.segname)) == 0) {
            const std::string sn(cs.sectname, strnlen(cs.sectname, sizeof(cs.sectname)));
            in_objc_methods = sn == "__inst_meth" || sn == "__cls_meth" ||
                              sn == "__cat_inst_meth" || sn == "__cat_cls_meth";
         }
         is_text_const_sect =
            strncmp(cs.segname, SEG_TEXT, sizeof(cs.segname)) == 0;
      }

      /* Debug probe: M64_DBG_DATAPTR=<hex> traces every gate decision for
       * __data words holding that exact value. Inert unless set. */
      static const char *dbgenv = std::getenv("M64_DBG_DATAPTR");
      static const uint32_t dbgval =
         dbgenv ? (uint32_t)strtoul(dbgenv, nullptr, 0) : 0u;
      const bool dbg = (dbgenv != nullptr && value == dbgval);
      if (dbg) {
         fprintf(stderr, "[dbgptr] value=%#x at vmaddr=%#zx sect=%.16s,%.16s "
                 "bits=%s locals=%d classicrelocs=%d objcsym=%d textconst=%d\n",
                 value, (std::size_t)loc.vmaddr,
                 env.current_section ? env.current_section->sect.segname : "?",
                 env.current_section ? env.current_section->sect.sectname : "?",
                 bits == Bits::M32 ? "M32" : "M64",
                 (int)env.have_local_text_syms,
                 (int)env.have_classic_local_relocs,
                 (int)in_objc_symbols, (int)is_text_const_sect);
      }

      /* M32 CONSTANT-PROVENANCE PIN (M64 re-parses only, and only when the
       * image carries the table). Every discriminator below is M32-gated
       * because each one needs the ORIGINAL i386 image; the M64 re-parse runs
       * the bare in-range heuristic, and the translated image is based at
       * 0x10000000, so an ordinary constant like 0x10080808 — far ABOVE the
       * small i386 image, hence correctly left alone by the M32 pass — reads as
       * a perfectly good translated __text address here and gets "rebased" by
       * the next layout shift.
       *
       * No value test can separate the two: Halo's 0x10080808 resolves to an
       * EXACT instruction boundary in the translated __text, so even
       * code_interior_alias is blind to it, and 79% of the measured corruptions
       * target __TEXT,__eh_frame, which no code/cstring/zerofill gate covers.
       * But the answer is already KNOWN — the M32 pass, holding the original
       * image, its relocs and its symbols, already classified this exact slot.
       * __DATA,__86x64_cpin carries that verdict across the file boundary, so
       * membership here is EXACT rather than heuristic.
       *
       * ★MEASURED (2026-08-05, same-offset i386-vs-translated diff): Civ IV
       * Steam 738 reclassified constants, iPhoto 143, iMovie 10, Halo CE 1,
       * Quinn/Pages/Numbers/iWeb 0. iPhoto's 13 __DATA,__gcc_except_tab hits
       * are round LSDA constants shifted by -0xF0 (corrupted C++ exception
       * tables); 14 words were "relocated" into __DATA,__86x64_pcmap, a section
       * WE synthesize.
       *
       * Self-disarming: an image with no table (produced before this mechanism,
       * or any native binary passed through `macho-tool modify`) keeps the
       * legacy behavior. Only ever REMOVES false positives — a genuine pointer
       * is never listed. Kill switch M64_NO_CONST_PIN=1 (A/B harness). */
      static const bool no_const_pin = std::getenv("M64_NO_CONST_PIN") != nullptr;
      if (bits == Bits::M64 && env.have_const_pins && !no_const_pin &&
          env.const_pin_slots.count(loc.vmaddr) != 0) {
         if (dbg) {
            fprintf(stderr, "[dbgptr]   pinned CONSTANT by M32 provenance "
                    "(__86x64_cpin) => is_pointer=0\n");
         }
         return Immediate<bits>::Parse(img, loc, env, /*is_pointer=*/false);
      }

      bool is_pointer = false;
      /* Reject obvious non-pointers up-front so we don't waste resolver
       * traffic on integer constants or float bit patterns.
       *
       * ⚠ The floor is the image's OWN lowest section, not a hardcoded 0x1000.
       * A non-PIE executable puts __PAGEZERO in the first page, so "below
       * 0x1000 cannot be a pointer" holds there — but a DYLIB's __TEXT is based
       * at vmaddr 0, so its __text routinely starts a couple of KiB in and the
       * magic number rejected genuine code pointers. 16 of Portal 2's 42 i386
       * modules have __text below 0x1000. inputsystem.dylib's CInputSystem
       * vtable (i386 __DATA,__const 0x10168) had 9 of 12 slots rebased and
       * three left RAW — 0xed0, 0xef0, 0xf50, every one of them below the
       * threshold — so the launcher's `mov (%ecx),%eax; call *0x14(%eax)`
       * jumped to 0xed0 and took SIGSEGV on the instruction fetch.
       *
       * std::min keeps 0x1000 wherever it was already the tighter bound, so
       * this can only ever ADD candidates, and only for an image that really
       * does have a section down there. The false-positive side is unchanged and
       * still handled by the evidence-based gates below plus __86x64_cpin. */
      static const bool ptr_floor_page =
         std::getenv("M64_PTR_FLOOR_PAGE") != nullptr;
      const std::size_t ptr_floor =
         ptr_floor_page ? 0x1000
                        : std::min<std::size_t>(0x1000,
                                                env.min_section_vmaddr());
      if (value >= ptr_floor && value < 0x80000000U) {
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
               /* ★NARROWED to ZERO-FILL targets (M32). The rule above — "a real
                * pointer into data points at an ALIGNED global" — is FALSE for a
                * pointer into a packed BYTE-RECORD table, which legitimately
                * starts at any offset. It produced a FALSE NEGATIVE that crashed
                * Halo CE (measured 2026-08-20): the versioned-schema pointer at
                * i386 0x3798ec holds 0x00379ec6 (& 3 == 2), a genuine pointer to
                * a table of 10-byte records. Left unrebased, it survived into
                * __DATA,__data of the translated image as a raw i386 address
                * while every other pointer in the same struct was rebased. The
                * scan at 0x19f0a4 walks that table for a tag-9 terminator, so it
                * ran off the end of whatever happened to be mapped low and took
                * SIGSEGV at exactly 0x379ec6 — reproducible, on the Campaign
                * "load existing profile" path.
                *
                * Every false positive this gate was written for lands in
                * ZERO-FILL space, and each is ALREADY covered by a gate that
                * reasons from evidence rather than from alignment:
                *   photocd's 0x30a91          -> __DATA,__bss    (zero-fill gate)
                *   Halo's 0x0048021C/0x5802D0 -> __DATA,__common (zero-fill gate)
                *   Civ's 0x01000100           -> __cstring interior (cstring gate)
                *   Quinn's {4,4}              -> __text alias   (func-entry gate)
                * A zero-fill target has no file content to corroborate, so
                * alignment is the only signal left there and the gate keeps its
                * bite. A target with real file content is a different situation,
                * and rejecting it on alignment alone discards a true pointer.
                * Kill switch M64_NO_MISALIGN_ZF_NARROW=1 restores the old
                * unconditional gate — the OFF arm reproduces the crash. */
               static const bool misalign_narrow_off =
                  std::getenv("M64_NO_MISALIGN_ZF_NARROW") != nullptr;
               if (!exec && (value & 3) != 0) {
                  bool relax = false;
                  if (!misalign_narrow_off) {
                     /* (a) the TARGET must have file content. A zero-fill
                      * pointee can be corroborated by nothing, so alignment
                      * stays the only signal there. */
                     bool tgt_filebacked = false;
                     for (Section<bits> *sec : seg->sections) {
                        if (!sec->contains_vmaddr(value)) { continue; }
                        const uint32_t st = sec->sect.flags & SECTION_TYPE;
                        tgt_filebacked = (st != S_ZEROFILL && st != S_GB_ZEROFILL);
                        break;
                     }
                     /* (b) ★the SLOT must live in WRITABLE data — a genuine
                      * pointer FIELD. This half was missing in the first cut and
                      * it cost a regression. `exec` above describes the TARGET's
                      * segment, so a slot sitting in read-only __TEXT,__const
                      * whose value merely ALIASES a __DATA address was relaxed
                      * too. That is precisely where const tables of small
                      * integers live — including the VERTEX-STRIDE table at
                      * i386 0x34e280 whose corruption-by-rebasing was the
                      * graphics bug fixed on 2026-08-18. MEASURED: without this
                      * condition a Halo retranslate changed 753 bytes of
                      * __TEXT,__const, against 144 of __DATA,__data, and the
                      * graphics-settings dialog stopped responding.
                      * A compile-time pointer into a byte-record table is a
                      * FIELD in writable data; a small-integer table is not. */
                     bool slot_writable = false;
                     for (Segment<bits> *sl : env.archive.segments()) {
                        if (!sl->contains_vmaddr(loc.vmaddr)) { continue; }
                        slot_writable =
                           (sl->segment_command.initprot & VM_PROT_WRITE) != 0 &&
                           (sl->segment_command.initprot & VM_PROT_EXECUTE) == 0;
                        break;
                     }
                     relax = tgt_filebacked && slot_writable;
                  }
                  if (!relax) {
                     break;   /* misaligned, unrelaxed -> treat as constant */
                  }
               }
               /* ZERO-FILL TARGET gate (M32, reloc-less images). The gate above
                * accepts that a data-range hit is weak evidence and rejects the
                * misaligned ones. A ZERO-FILL target is weaker still: the
                * pointee has no file content to inspect, the image carries no
                * relocation naming the slot, and in a locals-stripped image
                * there is no symbol either — literally nothing can corroborate
                * it. A false positive there is never inert; it rewrites a live
                * integer.
                * ★MEASURED (Halo CE): tag-class descriptor records carry three
                * u16 fields at +0xa/+0xc/+0xe. In the 'scen' and 'lifi' records
                * the PAIR at +0xc spells 0x0048021C / 0x005802D0, both landing
                * in __DATA,__common, so both were "rebased" — (540,72) became
                * (27356,4272) and (720,88) became (27536,4288). The 'bipd'
                * record survived only because its pair spells 0x00780234, above
                * the image. The consuming loop processes exactly the two
                * corrupted records, so `base + 564` became `base - 9508`,
                * landing in the tag block's name strings, and the deref took a
                * wild address every run.
                * Because the image is SMALL, every valid address has a small
                * high half — which is exactly what makes a (small, small) u16
                * pair look like an address, and why no value-based test can
                * separate them. Same family as the memdisp-code-alias gate.
                * Narrow by construction: zero-fill targets only, M32 only, and
                * only when no local reloc table can speak authoritatively; an
                * exact nlist hit passes as positive evidence. */
               if (bits == Bits::M32 && !exec && !env.have_classic_local_relocs &&
                   env.zerofill_target_unattested(value)) {
                  break;   /* unattested zero-fill target -> constant */
               }
               if (exec && in_objc_symbols) {
                  break;   /* objc_symtab count word aliasing __text -> constant */
               }
               /* CSTRING-INTERIOR ALIAS gate (M32). A __data word whose value
                * lands in the INTERIOR of a __cstring (not at a string start)
                * is an integer/byte-table constant that merely aliases the
                * cstring vmaddr range, never a genuine `char *` (which targets
                * a string START). Unlike the func-entry and classic-reloc gates
                * this needs no symbols or reloc table, so it is the ONLY
                * discriminator left for a locals-stripped, reloc-less
                * fixed-address exec (Civ IV STEAM: the byte-classification word
                * 0x01000100 = "00 01 00 01" aliased a __cstring interior and was
                * falsely rebased to a translated __text address, corrupting
                * GCompactDeclInfoNodeArray's table -> NULL name deref crash). */
               if (dbg) {
                  fprintf(stderr, "[dbgptr]   cstr_interior=%d str_neighbour=%d\n",
                          (int)env.cstring_interior_alias(img, value),
                          (int)env.cstring_slot_has_string_neighbour(img, loc.vmaddr));
               }
               if (bits == Bits::M32 && exec &&
                   env.cstring_interior_alias(img, value) &&
                   !env.cstring_slot_has_string_neighbour(img, loc.vmaddr)) {
                  break;   /* mid-cstring alias -> constant */
               }
               /* CODE-target FUNCTION-ENTRY gate (M32, symboled binaries).
                * A genuine pointer into an INSTRUCTIONS section baked into
                * __DATA/__OBJC data is a function entry: a fn-pointer table
                * slot, a C++ vtable slot, or an ObjC1 method-list IMP — all of
                * which carry an N_SECT nlist (func_syms) when the binary keeps
                * its local symbols (have_local_text_syms). A data word that
                * merely ALIASES a mid-function __text address is a constant:
                * small-struct packs like Quinn's IGSize {4,4} = 0x00040004,
                * shape/bitmask tables (0x10000/0x20200/...), fixed-point
                * values. Rebasing those "+segment delta" corrupts them with a
                * slide-dependent value — Quinn's piece matrix became
                * {w=8156,h=657} and -[QuinnMatrix usedRect] scanned 1.7MB off
                * a 16-byte piece shape (SIGBUS, black board).
                * SECTION-granular, not segment-: __TEXT also hosts DATA
                * sections (__cstring/__const/literals) whose interior addrs
                * selector-ref slots and string pointers legitimately target
                * with NO symbol — only targets inside a section marked
                * S_ATTR_(PURE|SOME)_INSTRUCTIONS are gated. Pointers to
                * IMPORTED functions never take this path (the linker emits
                * external relocs / symbol-ptr sections for them), and switch
                * jump tables target mid-function basic blocks but live in
                * __TEXT,__const or inline __text — never __DATA/__OBJC — so
                * requiring a func_syms hit here is exact, not heuristic, for
                * locals-symboled binaries. Locals-stripped binaries keep the
                * legacy permissive detection (gate disarmed). M32-only: the
                * M64 convert re-parse must keep its established behavior.
                * The predicate lives in ParseEnv::code_alias_is_constant and
                * is SHARED with the instruction-IMMEDIATE heuristics in
                * instruction.cc (same family: Civ IV's `mov $0xffff,%edx`
                * static-init priority aliasing __text). */
               if (exec && bits == Bits::M32 && !is_text_const_sect &&
                   env.code_alias_is_constant(value)) {
                  break;   /* mid-function code alias -> constant */
               }
               /* CODE-INTERIOR ALIAS gate (M32). The gate above needs local
                * text symbols; the classic-reloc gate below needs a local
                * reloc table; cstring_interior_alias only covers __cstring
                * targets. A locals-stripped, reloc-less fixed-address exec
                * disarms all three, and then ANY 4-byte constant landing in
                * __text is falsely rebased. But a genuine code pointer always
                * targets an instruction BOUNDARY (function entry, or a
                * basic-block head for a switch table) — never the middle of an
                * instruction. So a value that lands strictly INSIDE a decoded
                * instruction is a constant, whatever it aliases. Symbol-free,
                * reloc-free and exact; see ParseEnv::code_interior_alias.
                * (Civ IV STEAM: the __DATA,__data 4-byte string constant
                * " ._" = 0x005F2E20 — the strtok delimiter of the engine's
                * name-registry path lookup — aliased a mid-instruction __text
                * address and was rebased to 0x10A9546D = "mT\xa9\x10"; strtok
                * then split the path "Game" on 'm' and the failed lookup
                * dereferenced a NULL node at +0x88 during static init.)
                * Applies to __TEXT,__const too (is_text_const_sect is NOT
                * excluded): its switch tables target basic-block heads, which
                * ARE boundaries, so they pass this gate — unlike the
                * func-entry gate, which is why that one excludes them. */
               if (dbg) {
                  fprintf(stderr, "[dbgptr]   in seg %.16s exec=%d "
                          "cstring_interior=%d code_alias_const=%d "
                          "code_interior=%d lacks_entry=%d\n",
                          seg->segment_command.segname, (int)exec,
                          (int)env.cstring_interior_alias(img, value),
                          (int)env.code_alias_is_constant(value),
                          (int)env.code_interior_alias(value),
                          (int)env.code_alias_lacks_entry_evidence(img, value));
               }
               if (exec && bits == Bits::M32 && env.code_interior_alias(value)) {
                  break;   /* mid-INSTRUCTION alias -> constant */
               }
               /* JUMP-TABLE COHESION gate (M32). The func-entry and code-entry
                * gates below/above are deliberately disarmed for __TEXT-
                * resident data because switch jump tables live in
                * __TEXT,__const and target unsymboled basic-block heads. That
                * leaves only the interior gate above, so an integer aliasing an
                * exact instruction BOUNDARY passes everything and is rebased.
                * A jump table is a RUN of valid code targets; an integer table
                * is not. An immediate neighbour that itself aliases code at a
                * NON-boundary proves this run is not a table (see
                * ParseEnv::code_alias_run_contradicted for the full argument
                * and the Halo #45 measurement). Placed here, after the interior
                * gate, so it only ever judges words every cheaper
                * discriminator already waved through. */
               if (exec && bits == Bits::M32 &&
                   env.code_alias_run_contradicted(img, loc, value)) {
                  break;   /* neighbour contradicts a jump table -> constant */
               }
               /* CODE-ENTRY gate (M32, locals-STRIPPED images). The boundary
                * gate above only catches values that are not instruction
                * starts at all. A data constant can easily alias a perfectly
                * valid instruction boundary that is nonetheless MID-FUNCTION,
                * and then nothing above it fires: code_alias_is_constant needs
                * local text symbols, the classic-reloc gate needs a local
                * reloc table, cstring_interior_alias only covers __cstring.
                * A genuine DATA-resident code pointer targets a function
                * ENTRY, so demand positive entry evidence — an nlist at the
                * value (globals survive `strip -x`) or the `55 89 e5`
                * frame-setup prologue. This is the same positive-evidence rule
                * instruction.cc already applies to code-aliasing imm32s.
                * (Civ IV STEAM: " ._" = 0x005F2E20 is `sub $0x18,%esp`, 3
                * bytes into the function entered at 0x5F2E1D — a real
                * boundary, no symbol, no prologue -> constant.)
                * __TEXT,__const excluded: its switch jump tables target
                * basic-block heads, which have neither symbol nor prologue. */
               if (exec && bits == Bits::M32 && !is_text_const_sect &&
                   !in_objc_methods &&
                   env.code_alias_lacks_entry_evidence(img, value)) {
                  break;   /* mid-function alias, no entry evidence -> constant */
               }
               /* RECORD-FIELD gate (M32) — the LAST heuristic, deliberately, so
                * it only ever sees the words every cheaper discriminator has
                * already waved through.
                *
                * Same (small,small) u16-pair corruption as the zero-fill gate
                * above, but for targets that ARE mapped, where the zero-fill
                * anchor argument does not apply. Two blind spots, both MEASURED
                * on Halo CE (#35), both in ONE 8-byte record array:
                *   - a __DATA,__data target (0x00380000) — no discriminator
                *     existed for mapped data targets at all;
                *   - a __TEXT,__text target (0x00030002) held in a slot that
                *     lives in __TEXT,__const, where the func-entry and
                *     code-entry gates are DELIBERATELY disarmed (switch jump
                *     tables live there and target unsymboled basic-block
                *     heads), leaving only code_interior_alias — which passes
                *     this value because it happens to be a decoded instruction
                *     BOUNDARY.
                * Both were rebased to 0x104a2000 / 0x1003ac81, making a u16
                * index the low half of a relocated pointer; proven by the index
                * tracking ASLR across runs and by an i386-vs-translated diff.
                *
                * ★The evidence is POSITIONAL, which is exactly what separates a
                * record array in __TEXT,__const from the switch jump table that
                * disarmed the entry gate: a jump table is a contiguous run of
                * code pointers, so its slots' stride-siblings are pointers too
                * and the gate stays silent. See
                * ParseEnv::record_field_neighbours_are_integers.
                *
                * For a CODE target, additionally demand that the target carries
                * no positive function-entry evidence — the same positive test
                * the code-entry gate uses. So this never demotes a word that
                * points at a symbol, a `55 89 e5` prologue or a C++ adjustor
                * thunk; it re-arms that gate for __TEXT,__const only where the
                * positional evidence says "record array", never for a value
                * that looks like a real function entry. */
               if (bits == Bits::M32 && !env.have_classic_local_relocs &&
                   (!exec || !env.code_target_has_entry_evidence(img, value)) &&
                   env.record_field_neighbours_are_integers(img, loc.vmaddr)) {
                  break;   /* integer field of a record array -> constant */
               }
               /* CLASSIC-RELOC AUTHORITATIVE GATE (M32 classic images). A
                * slidable classic image's genuine absolute internal pointers
                * ALL carry LC_DYSYMTAB local relocations — dyld could not
                * slide the image otherwise — so when the image has a live
                * local-reloc table (and no LC_DYLD_INFO rebase stream: see
                * Dysymtab::lift_local_relocs) membership is EXACT: a slot
                * with no reloc is a constant, whatever its value aliases.
                * Kills the mass false positives the in-range heuristic
                * produces in packed-constant const data (Portal 2
                * engine.dylib __TEXT,__const: all 637 in-range hits were
                * packed int16 pairs / ASCII / floats with zero relocs —
                * g_SideVertCorners {1,0} = 0x00010000 became a slid text
                * address, wild SIGBUS store in InitPowerInfo_R). This gate
                * covers __TEXT,__const, where the func-entry gate above is
                * deliberately disarmed, AND locals-stripped classic dylibs,
                * where it can't arm. Slots WITH a reloc fall through to the
                * existing detection unchanged, so this only ever REMOVES
                * false positives. Reloc-less fixed-address execs stay on the
                * heuristics. M32-only: the M64 convert re-parse keeps its
                * established behavior. */
               if (bits == Bits::M32 && env.have_classic_local_relocs &&
                   env.local_reloc_addrs.count(loc.vmaddr) == 0) {
                  break;   /* linker recorded no reloc here -> constant */
               }
               is_pointer = true;
               break;
            }
         }
      }
      if (dbg) {
         fprintf(stderr, "[dbgptr]   => is_pointer=%d\n", (int)is_pointer);
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

      /*
       * Progress: per-section banner so the long Section::Parse1 loop on
       * a 17 MB binary is not silent. Cheap (one line per section), but gated
       * behind MACHO_TOOL_DEBUG: m64 drives macho-tool through a pipe, and the
       * unconditional banner (× every section × every macho-tool invocation in
       * the pipeline) floods that pipe; under Rosetta the flooded write can
       * abort the tool, which surfaces downstream as a spurious
       * "cp <file>: No such file" retranslate failure (the output never landed).
       */
      if (std::getenv("MACHO_TOOL_DEBUG")) {
         fprintf(stderr, "parse: %.16s,%.16s vmaddr=0x%zx size=%zu\n",
                 sect.segname, sect.sectname,
                 (size_t)sect.addr, (size_t)sect.size);
      }

      /* Recognise PIC relative-offset switch jump tables BEFORE the linear
       * sweep so TextParser emits relocatable JumpTableEntry blobs for their
       * slots instead of disassembling the table bytes as code. */
      DetectJumpTables(img, env);

      /* For text sections, build a cursor into the func_syms set so the
       * sweep can cheaply check whether the NEXT symbol boundary falls inside
       * the current decoded instruction.  An instruction that spans a symbol
       * boundary means the bytes BEFORE the boundary are inter-function padding
       * (e.g. a 0x00 alignment byte) that the decoder absorbed.  We truncate to
       * a 1-byte DataBlob and retry: this re-syncs the sweep at the true entry.
       *
       * Applies to BOTH M32 (initial sweep) and M64 (convert re-parse): the
       * M32 sweep emits a DataBlob for the padding byte, but the M64 re-parse
       * still sees the raw bytes and needs the same guard to avoid absorbing
       * the padding into a multi-byte instruction that crosses the symbol.
       *
       * Only active when func_syms is non-empty (i.e. the binary has a symtab
       * AND the Symtab LC was parsed before this section's Parse1 ran). */
      std::set<std::size_t>::const_iterator next_sym_it;
      bool check_sym_boundary = false;
      if (parser == TextParser && !env.func_syms.empty()) {
         /* Advance past any symbols that are at or before the section start
          * (they can't split an instruction decoded at the section start). */
         next_sym_it = env.func_syms.upper_bound(sect.addr);
         check_sym_boundary = (next_sym_it != env.func_syms.end() &&
                               *next_sym_it < sect.addr + sect.size);
      }

      const std::size_t begin = sect.offset;
      const std::size_t end = begin + sect.size;
      std::size_t it = begin;
      std::size_t vmaddr = sect.addr;
      /* Tracks whether the previously emitted text blob ended control flow with
       * NO fall-through (RET or unconditional JMP). After such a terminator the
       * linker inserts 0x00 alignment padding before the next function entry;
       * see the padding-resync guard below. Meaningful for TextParser only. */
      bool prev_no_fallthrough = false;
      while (it != end) {
         SectionBlob<bits> *elem;
         bool emitted_padding = false;

         /* Re-sync the linear sweep at inter-function 0x00 alignment padding in
          * STRIPPED binaries, where func_syms is empty so the boundary guard
          * below is inert. After a no-fall-through terminator the linker pads
          * with 0x00 before the next function; the sweep would otherwise decode
          * e.g. `00 55 89` as `add [ebp-0x77],dl`, ABSORBING the next function's
          * entry byte (0x55 = push ebp) and emitting the whole function body as
          * translation-invariant garbage (eax/ebp-based misdecodes need no PIC
          * fixup, so they survive M32->M64 byte-identical). A direct `call` to
          * that swallowed entry then resolves to a mid-instruction address ->
          * executes raw i386 bytes as x86_64 -> runtime SIGSEGV (Halo CE
          * static-init ctor crash, Halo.dylib+0x37fd95, rax=1 from the raw
          * `mov eax,1`). A 0x00 byte reached with no fall-through is never the
          * first byte of a real instruction (no compiler starts a function or
          * basic block at `add [r/m8],r8`), so emit it as a 1-byte DataBlob and
          * stay in padding mode until a non-zero byte: the true entry. Skip
          * jump-table slots (an indirect JMP can be followed by a relocated
          * table that DetectJumpTables already claimed). Universal: triggers on
          * the structural pattern (terminator + 0x00 fill), not an app name;
          * complements the func_syms guard for symtab'd binaries and applies to
          * the M64 convert re-parse too (the translated `jmpq *r11` ret idiom is
          * an UNCOND_BR terminator). */
         if (parser == TextParser && prev_no_fallthrough &&
             img.at<uint8_t>(it) == 0x00 &&
             env.jump_table_slots.find(vmaddr) == env.jump_table_slots.end()) {
            elem = DataBlob<bits>::Parse(img, Location(it, vmaddr), env);
            emitted_padding = true;
         }
         /* Check whether the decoded instruction would STRADDLE the next known
          * function-symbol boundary.  If so, the bytes at [vmaddr, sym) are
          * inter-function padding — force a 1-byte DataBlob here and let the
          * next iteration start fresh at vmaddr+1 (closer to or at the symbol).
          * Apply only when the parser is TextParser (code sections) and only
          * for M32 (the constexpr guard above already ensures this path is only
          * compiled for M32 parses). */
         else if (check_sym_boundary) {
            /* Advance next_sym_it past any symbol already consumed. */
            while (next_sym_it != env.func_syms.end() && *next_sym_it <= vmaddr) {
               ++next_sym_it;
               if (next_sym_it == env.func_syms.end() ||
                   *next_sym_it >= sect.addr + sect.size) {
                  check_sym_boundary = false;
                  break;
               }
            }
            if (check_sym_boundary) {
               /* Try a quick-decode to get the instruction length; if it
                * crosses the next symbol boundary, fall back to DataBlob. */
               xed_decoded_inst_t xd;
               xed_decoded_inst_zero_set_mode(&xd, &Instruction<bits>::dstate());
               xed_decoded_inst_set_input_chip(&xd, XED_CHIP_INVALID);
               bool boundary_straddle = false;
               /* ★A CLAIMED JUMP-TABLE SLOT IS NOT A DECODE — it is known DATA
                * of known length (4), so it cannot straddle anything and this
                * heuristic must not run on it. The padding-resync branch above
                * already carries exactly this exemption; without it here, the
                * LAST entry of a table that ends a function is quietly
                * preempted: the straddle test decodes the slot bytes as code,
                * sees them run into the next function symbol, emits a 1-byte
                * DataBlob, and so never calls `parser` — which is the only
                * place jump_table_slots is consulted. The entry then ships its
                * RAW i386 anchor-relative delta, and `translated_anchor +
                * i386_delta` is a mid-instruction address.
                *
                * This is the NORMAL PIC-switch layout, not a corner case: the
                * table sits inline at the end of its function, so its last slot
                * always butts against the next function symbol.
                *
                * MEASURED, Portal 2 wall 7 — libtogl
                * D3DToGL::WriteGLSLSamplerDefinitions (i386 0x11880, anchor
                * 0x1188e, table 0x11960, DetectJumpTables count=4):
                *   slot 3 @0x1196c = a1 00 00 00, next func symbol 0x11970
                *   decode `a1 00 00 00 55` = movl 0x55000000,%eax, len 5,
                *   0x11970 falls inside [0x1196d,0x11971) -> straddle
                * so only slots 0..2 were re-anchored (0x3d/0x57/0x69 ->
                * 0x60/0x81/0x96) and slot 3 kept 0xa1. At run time
                * anchor 0x10019ae7 + 0xa1 = 0x10019b88 = ONE BYTE INTO the
                * `leal 0x32515(%rip),%ecx` of another case, which decodes as
                * `orl $0x32515,%eax; jmp <shared tail>` and falls into
                * `movl %ecx,0x4(%rsp)` WITHOUT EVER SETTING %ecx -> PrintToBuf
                * got fmt = 0 -> SIGSEGV in the printf-format walk.
                * Kill switch M64_NO_JT_SLOT_BEATS_FUNCSYM=1. */
               static const bool jt_slot_wins =
                  std::getenv("M64_NO_JT_SLOT_BEATS_FUNCSYM") == nullptr;
               const bool jt_claimed =
                  jt_slot_wins &&
                  env.jump_table_slots.find(vmaddr) != env.jump_table_slots.end();
               if (!jt_claimed &&
                   xed_decode(&xd, &img.at<uint8_t>(it), img.size() - it) == XED_ERROR_NONE) {
                  const unsigned len = xed_decoded_inst_get_length(&xd);
                  /* straddles if the sym falls INSIDE [vmaddr+1, vmaddr+len) */
                  if (len > 1 && *next_sym_it > vmaddr && *next_sym_it < vmaddr + len) {
                     boundary_straddle = true;
                  }
               }
               if (boundary_straddle) {
                  /* Emit the padding byte as DataBlob and re-sync. */
                  if (std::getenv("MACHO_TRACE_FUNCSYM")) {
                     fprintf(stderr, "[funcsym-boundary] vmaddr=0x%zx sym=0x%zx len=%u -> DataBlob\n",
                             vmaddr, (size_t)*next_sym_it,
                             xed_decoded_inst_get_length(&xd));
                  }
                  elem = DataBlob<bits>::Parse(img, Location(it, vmaddr), env);
               } else {
                  elem = parser(img, Location(it, vmaddr), env);
               }
            } else {
               elem = parser(img, Location(it, vmaddr), env);
            }
         } else {
            elem = parser(img, Location(it, vmaddr), env);
         }

         /* Mark text function entries so Build aligns them to an even address
          * (preserves the Itanium pmf low-bit tag; see SectionBlob::func_entry).
          * Only code sections: a func_syms vmaddr that coincides with this blob's
          * start in a text section is a function entry. */
         if (parser == TextParser && env.func_syms.count(vmaddr)) {
            elem->func_entry = true;
         }

         elem->iter = content.insert(content.end(), elem);

         /* Record this blob's DISK-FAITHFUL i386 address (the linear-sweep vmaddr,
          * i.e. section base + the sum of raw decoded instruction lengths) for the
          * C++ exception PC map.  Captured HERE, during the initial sweep, BEFORE
          * Archive::Build (transform.cc Build()s the M32 archive before Transform)
          * re-lays-out the blobs (even-alignment, padding) and DetectPicAnchoredDisps
          * runs — both of which drift loc.vmaddr from the i386 layout the LSDA is
          * keyed in.  Pure observation: nothing reads orig_vmaddr except the EH
          * sections, so this does not change translation output. */
         elem->orig_vmaddr = vmaddr;

         /* Update no-fall-through state for the next iteration (TextParser): an
          * emitted padding byte keeps us in the padding gap; otherwise consult
          * the just-decoded instruction's category (RET / unconditional JMP end
          * control flow with no fall-through). Non-instruction blobs fall
          * through. Matches the terminator convention used elsewhere in this
          * file (DetectPicAnchoredDisps). */
         if (parser == TextParser) {
            if (emitted_padding) {
               prev_no_fallthrough = true;
            } else if (auto *in = dynamic_cast<Instruction<bits> *>(elem)) {
               const xed_category_enum_t cat =
                  xed_decoded_inst_get_category(&in->xedd);
               prev_no_fallthrough = (cat == XED_CATEGORY_RET ||
                                      cat == XED_CATEGORY_UNCOND_BR);
            } else {
               prev_no_fallthrough = false;
            }
         }

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

   /* Map the x86 GPR encoding (0..7) carried in ParseEnv::pic_thunks back to
    * its 32-bit xed register, so the section-local thunk byte-scan maps can be
    * seeded from the global symbol-table thunk map. */
   static xed_reg_enum_t gpr32_from_enc(uint8_t enc) {
      switch (enc) {
      case 0: return XED_REG_EAX;
      case 1: return XED_REG_ECX;
      case 2: return XED_REG_EDX;
      case 3: return XED_REG_EBX;
      case 4: return XED_REG_ESP;
      case 5: return XED_REG_EBP;
      case 6: return XED_REG_ESI;
      case 7: return XED_REG_EDI;
      default: return XED_REG_INVALID;
      }
   }

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
      /* ★Sticky "this region is PIC codegen", set when an anchor is ESTABLISHED
       * and cleared only where the whole tracking state is (interleaved data,
       * RET, interrupt). Steps (2c)/(2d) cancel an absolute-address heuristic
       * because PIC code reaches globals anchor-relative or through slots, and
       * that is a property of the CODE, not of whether an anchor is still LIVE.
       * Gating them on `!anchors.empty()` conflated the two: the last anchor
       * dying — e.g. `mov 0x28c(%eax),%eax` re-purposing the only anchor
       * register, step (2b') — made a PIC function look non-PIC, and the
       * heuristic then claimed a plain pointer OFFSET as an absolute table
       * address (guard 96_zerofill_common_interior: `0x124f88(%eax)` =
       * &freqstruct[150001], mis-resolved into __common's interior). */
      bool anchored_region = false;

      /* frame-slot displacement → anchor vmaddr. Compilers spill the PIC
       * register to the stack (`mov %esi, -0x2c(%ebp)`) and later reload it
       * into a DIFFERENT register (`mov -0x2c(%ebp), %eax; mov 0x2d6(%eax),..`)
       * — common once %esi is needed for something else. Track the anchor
       * through the spill slot so the reloaded register is recognized as an
       * anchor too. Keyed by (base_reg, disp) of the frame slot. */
      std::map<std::pair<xed_reg_enum_t, ssize_t>, std::size_t> anchor_slots;

      /* Frame slots holding a register's FUNCTION-ENTRY value — the callee-save
       * store a prologue emits BEFORE the PIC anchor exists:
       *
       *     push %ebp; mov %esp,%ebp; sub $N,%esp
       *     mov  %ebx,-0xc(%ebp)          <- entry save, %ebx not yet an anchor
       *     call $+0 ; pop %ebx           <- anchor established here
       *     ...
       *     mov  -0xc(%ebp),%ebx          <- EPILOGUE RESTORE (see below)
       *
       * `pre_anchor_saves` collects candidate saves seen since the last anchor
       * pop / function boundary; the pop promotes them to `entry_save_slots`
       * (a save can only be an ENTRY save if it precedes the anchor — after the
       * pop the register may legitimately be re-purposed and re-spilled, and
       * such a slot must keep the ordinary "reload = redefinition" semantics).
       * Any later store to a promoted slot demotes it: it no longer holds the
       * entry value. Cleared with `anchors` at a function-boundary RET. */
      std::map<std::pair<xed_reg_enum_t, ssize_t>, xed_reg_enum_t>
         pre_anchor_saves, entry_save_slots;
      /* Kill switch: restore the pre-fix behaviour (an epilogue restore erases
       * the anchor) so the guard can A/B the gate at TRANSLATE time. */
      const bool entry_save_gate =
         std::getenv("M64_NO_PIC_ANCHOR_ENTRY_SAVE") == nullptr;

      /* Forward branch targets we've seen but not yet reached in the
       * linear walk. Used to recognize "mid-function" RETs: if there's
       * pending forward-branch flow that hasn't been resolved by visiting
       * the target, we're still inside the function and shouldn't clear
       * anchors at this RET. Models the Tessera/cxa_guard pattern where
       * the fast path returns early (RET in middle of byte stream) and
       * the slow path lives at higher addresses, reached via the
       * fast-path JE around it. */
      std::set<std::size_t> pending_forward_targets;

      /* Anchor-map snapshots captured at forward-branch SOURCES, keyed by the
       * branch TARGET vmaddr. A caller-saved PIC anchor (e.g. ECX from an inline
       * `call $+0; pop %ecx`) can be CLOBBERED on the linear fall-through path by
       * a CALL (step 6 erases EAX/ECX/EDX) or a register write, yet a slow-path
       * block reached by an earlier forward branch (taken BEFORE that clobber)
       * still holds the anchor at runtime. The linear walk would then carry the
       * clobbered state into that block and fail to rewrite its anchored
       * `disp(%reg)` ref. Observed: Portal 2 CUtlMemory<int,int>::Grow — the
       * fresh-alloc `je` slow path's `movl 0x22f1e(%ecx),%eax` was left
       * un-rewritten after the realloc fast path's `call *0x4(%edi)` erased ECX,
       * so at runtime it read [stale_i386_disp] → garbage ptr → SIGSEGV. On
       * reaching a target we restore/JOIN this snapshot so the anchor is
       * recognized on the branch path too. The JOIN is an INTERSECTION (an entry
       * survives only if every recorded predecessor agrees on the same anchor
       * vmaddr), so it can never INTRODUCE a spurious anchor — only recover one
       * the linear walk dropped. */
      std::map<std::size_t, std::unordered_map<xed_reg_enum_t, std::size_t>>
         branch_anchor_snap;

      /* Intersect `dst` with `src` in place: keep only entries present in both
       * and mapping to the same anchor vmaddr. */
      auto anchor_intersect =
         [](std::unordered_map<xed_reg_enum_t, std::size_t>& dst,
            const std::unordered_map<xed_reg_enum_t, std::size_t>& src) {
         for (auto it = dst.begin(); it != dst.end(); ) {
            auto s = src.find(it->first);
            if (s == src.end() || s->second != it->second) {
               it = dst.erase(it);
            } else {
               ++it;
            }
         }
      };

      /* The SAME snapshot/JOIN for the anchor SPILL SLOTS (step 2b's
       * anchor_slots), because a slot dies the same way a register does — and
       * for the same reason the linear walk cannot see.
       *
       * ★MEASURED (Portal 2 libtogl `CGLMFBO::TexAttach`, i386 0x2260):
       *     0x226e  pop %eax                 ; anchor
       *     0x226f  mov %eax,-0x10(%ebp)     ; anchor_slots[(EBP,-0x10)] = anchor
       *     0x2309  jne 0x23b5               ; forward branch
       *     0x230f  mov -0x10(%ebp),%eax     ; reload (rewritten OK)
       *     0x2312  mov 0x44daa(%eax),%eax
       *     0x2318  mov (%eax),%eax
       *     0x231a  mov %eax,-0x10(%ebp)     ; NON-anchor stored -> slot erased
       *     0x23b0  jmp 0x25a3               ; (no fall-through into 0x23b5)
       *     0x23b5: ... 0x23ce mov -0x10(%ebp),%eax ; reload -> NOT an anchor
       *             0x23d1 mov 0x44daa(%eax),%eax   ; kept the raw i386 disp
       * The two blocks are MUTUALLY EXCLUSIVE at runtime, but the linear walk
       * sees the kill first and applies it to everything after. At runtime
       * translated_anchor + i386_disp landed inside translated __text and the
       * loaded "pointer" was code bytes (0xc700) -> SIGSEGV in CGLMFBO::TexAttach
       * with the GL dispatch slots 0x1c8/0x1d4/0x1d8 one deref away.
       * Register-level branch_anchor_snap already repairs exactly this shape;
       * the slot map was simply never included.
       *
       * The JOIN is the same INTERSECTION, so it can only RECOVER a slot the
       * linear walk dropped, never invent one.
       * KILL SWITCH M64_NO_PIC_ANCHOR_SLOT_SNAP=1 (restores the drop). */
      using anchor_slot_map = decltype(anchor_slots);
      std::map<std::size_t, anchor_slot_map> branch_slot_snap;
      const bool slot_snap_on =
         std::getenv("M64_NO_PIC_ANCHOR_SLOT_SNAP") == nullptr;
      /* Extend the SAME snapshot to the case-body targets of an indirect PIC
       * jump-table dispatch — see step (5b). */
      const bool jt_target_snap_on =
         std::getenv("M64_NO_PIC_ANCHOR_JT_TARGETS") == nullptr;

      auto slot_intersect = [](anchor_slot_map& dst, const anchor_slot_map& src) {
         for (auto it = dst.begin(); it != dst.end(); ) {
            auto s = src.find(it->first);
            if (s == src.end() || s->second != it->second) {
               it = dst.erase(it);
            } else {
               ++it;
            }
         }
      };

      /* (0) Pre-scan for GCC-style PIC thunks. `___i686.get_pc_thunk.<r>` is the
       *     two-instruction leaf `mov %reg,(%esp); ret` — it copies the return
       *     address (the caller's PC) into %reg. A `call` to such a thunk is the
       *     SEPARATE-thunk form of the PIC anchor (vs the inline `call $+0;
       *     pop %reg` clang form): after the call, %reg holds the vmaddr of the
       *     instruction following the call. Portal 2's i386 binaries use this
       *     form, which the inline-only detector above missed — leaving every
       *     `disp(%reg)` PIC data ref carrying its stale i386 displacement
       *     (observed: portal2_osx main's dlopen path `lea eax,[ebx+0xd0]`
       *     pointing into __text instead of the relocated __cstring). Map each
       *     thunk's entry vmaddr -> destination register for the call sites. */
      std::unordered_map<std::size_t, xed_reg_enum_t> pic_thunks;
      {
         Instruction<bits> *mov = nullptr;   /* candidate `mov %reg,(%esp)` */
         for (SectionBlob<bits> *blob : content) {
            auto *cur = dynamic_cast<Instruction<bits> *>(blob);
            if (mov && cur &&
                xed_decoded_inst_get_category(&cur->xedd) == XED_CATEGORY_RET) {
               const xed_reg_enum_t dst =
                  xed_decoded_inst_get_reg(&mov->xedd, XED_OPERAND_REG0);
               if (dst >= XED_REG_EAX && dst <= XED_REG_EDI) {
                  pic_thunks[mov->loc.vmaddr] = dst;
               }
            }
            /* Is `cur` a `mov %reg,(%esp)` (the thunk's first instruction)? */
            mov = nullptr;
            if (cur &&
                xed_decoded_inst_get_iform_enum(&cur->xedd) == XED_IFORM_MOV_GPRv_MEMv) {
               const xed_operand_values_t* mops =
                  xed_decoded_inst_operands_const(&cur->xedd);
               if (xed_decoded_inst_number_of_memory_operands(&cur->xedd) == 1 &&
                   xed_decoded_inst_get_base_reg(mops, 0) == XED_REG_ESP &&
                   xed_decoded_inst_get_index_reg(mops, 0) == XED_REG_INVALID &&
                   xed_decoded_inst_get_memory_displacement(mops, 0) == 0) {
                  mov = cur;
               }
            }
         }
      }

      /* Seed the section-local thunk map from the GLOBAL symbol-table thunk map
       * (ParseEnv::pic_thunks) so a `call ___i686.get_pc_thunk.<r>` whose thunk
       * lives in another text section (Civ IV: thunks in __textcoal_nt, callers
       * in __text) is recognised here too.  The byte-scan above already covers
       * thunks within THIS section (incl. unnamed/stripped ones); the symbol map
       * adds the cross-section named thunks.  Same key (entry vmaddr). */
      for (const auto& kv : env.pic_thunks) {
         const xed_reg_enum_t r = gpr32_from_enc(kv.second);
         if (r != XED_REG_INVALID) { pic_thunks.emplace(kv.first, r); }
      }

      Instruction<bits> *prev_inst = nullptr;
      /* Category of the last NON-nop instruction on the linear walk. Block (0b)
       * uses it (not prev_inst alone) to tell whether a branch target has a LIVE
       * fall-through predecessor: a slow-path target frequently sits after
       * `ret` + alignment nops, so prev_inst is a nop while the real predecessor
       * (the RET) ends control flow. Seeing through the nops lets the target
       * ADOPT its branch anchor snapshot instead of intersecting it with the
       * dead post-RET state — which otherwise dropped the PIC anchor and left a
       * `disp(%ebx)` data store carrying its stale i386 displacement (iPhoto
       * UpgradeChecker +[checkOSVersion:] `osVersion = SystemVersion()` -> store
       * into read-only __TEXT -> SIGBUS). */
      xed_category_enum_t last_flow_cat = XED_CATEGORY_INVALID;
      xed_category_enum_t prev2_flow_cat = XED_CATEGORY_INVALID;
      /* ★A CALL DOES NOT REVIVE A DEAD FALL-THROUGH. (0b)/(0c) ask whether the
       * instruction before a branch target ended control flow (RET / uncond
       * JMP); if it did, the target's only predecessors are its branches and
       * the JOIN ADOPTs their snapshot instead of intersecting the linear
       * walk's dead post-epilogue state. That test looked at ONE instruction,
       * so the compiler's NORETURN-TRAP idiom defeated it:
       *
       *     0x79a7e  ret                        <- function exit
       *     0x79a7f  call ___stack_chk_fail     <- branch-reachable, NEVER returns
       *     0x79a84  mov 0xc(%ebp),%ebx         <- branch target (3 branches)
       *
       * `last_flow_cat` is CALL at 0x79a84, so the join INTERSECTED the target's
       * (correct, non-empty) snapshot with the epilogue's empty state and lost
       * the anchor for the whole rest of the function.
       *
       * A call sitting between a RET and a branch target is unreachable except
       * by a branch, and whatever the linear walk carries into it is the
       * EPILOGUE's state — which describes no execution path that reaches the
       * target. So the deadness is STICKY ACROSS CALLS: set at RET/uncond JMP,
       * carried through CALL (and, as before, through alignment NOPs), cleared
       * by any other instruction. Nothing else changes: a target whose real
       * predecessor is an ordinary instruction still intersects.
       * KILL SWITCH M64_NO_PIC_ANCHOR_RET_CALL_DEAD=1. */
      const bool ret_call_dead =
         std::getenv("M64_NO_PIC_ANCHOR_RET_CALL_DEAD") == nullptr;
      bool ft_dead = false;
      for (SectionBlob<bits> *blob : content) {
         auto *inst = dynamic_cast<Instruction<bits> *>(blob);
         if (!inst) {
            /* A JumpTableEntry is the inline switch table emitted right after
             * a PIC dispatch (see DetectJumpTables). It is NOT anchor-
             * invalidating data: control-flow-flattened functions routinely
             * place several dispatches back-to-back, each followed by its own
             * inline table, all sharing the one PIC anchor (ebx). Clearing
             * anchors here dropped the anchor across the first table so every
             * LATER dispatch's table-base `lea %eax,[%ebx+disp]` went
             * un-rewritten — it kept the stale i386 displacement and pointed
             * past the relocated table at runtime (observed: iPhoto FairPlay
             * white-box-crypto callees, 2nd+ dispatch per function). The
             * entries themselves are relocated by DetectJumpTables; the anchor
             * must survive so the dispatch that READS them is fixed too. Any
             * OTHER non-instruction blob is genuine interleaved data and still
             * invalidates the anchor. */
            if (dynamic_cast<JumpTableEntry<bits> *>(blob) != nullptr) {
               prev_inst = nullptr;
               continue;
            }
            /* Data interleaved in __text — clear tracking, anchors
             * almost certainly don't survive past it. */
            if (std::getenv("MACHO_TRACE_ANCHORSLOT") && !anchor_slots.empty()) {
               fprintf(stderr, "[aslot] CLEAR-datablob vmaddr=0x%zx type=%s nslots=%zu\n",
                       (size_t)blob->loc.vmaddr, typeid(*blob).name(),
                       anchor_slots.size());
            }
            anchors.clear();
            anchor_slots.clear();
            entry_save_slots.clear();
            pre_anchor_saves.clear();
            anchored_region = false;
            prev_inst = nullptr;
            continue;
         }

         const xed_decoded_inst_t& xedd = inst->xedd;
         const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);

         /* (0b) Restore/JOIN any anchor snapshot recorded by a forward branch
          *      that targets this instruction (see branch_anchor_snap). If the
          *      previous instruction was an unconditional transfer (RET or
          *      unconditional JMP) there is NO fall-through predecessor, so the
          *      linear-carried state is dead code — adopt the snapshot (the JOIN
          *      of all branch predecessors) outright. Otherwise JOIN the snapshot
          *      with the carried fall-through state by intersection. This is what
          *      lets a caller-saved anchor survive into a slow-path block whose
          *      fast-path sibling clobbered it (Portal 2 Grow). For the common
          *      case (a target whose only predecessor is the immediately
          *      preceding instruction) the snapshot equals the carried state, so
          *      this is a no-op. */
         {
            auto snap_it = branch_anchor_snap.find(inst->loc.vmaddr);
            if (snap_it != branch_anchor_snap.end()) {
               /* Is the linear fall-through into here DEAD? Consult the last
                * NON-nop instruction (last_flow_cat), not just prev_inst: a
                * branch target commonly sits after `ret` + alignment nops, so
                * prev_inst is a nop and the carried `anchors` is the dead
                * post-RET state. If the real predecessor ended flow (RET /
                * uncond JMP) the only way in is the branch -> ADOPT its snapshot
                * outright; otherwise the target also has a live fall-through ->
                * JOIN by intersection. (prev_inst==nullptr after interleaved
                * data keeps the old conservative intersect.) */
               bool no_fallthrough = false;
               if (prev_inst != nullptr) {
                  no_fallthrough = (last_flow_cat == XED_CATEGORY_RET ||
                                    last_flow_cat == XED_CATEGORY_UNCOND_BR ||
                                    (ret_call_dead && ft_dead));
               }
               if (no_fallthrough) {
                  anchors = snap_it->second;
               } else {
                  anchor_intersect(anchors, snap_it->second);
               }
               branch_anchor_snap.erase(snap_it);
            }
            /* The same adopt/JOIN for the anchor SPILL SLOTS. Kept in its own
             * lookup because a target may have a slot snapshot even when the
             * register snapshot was already consumed/absent. */
            if (slot_snap_on) {
               auto ssnap = branch_slot_snap.find(inst->loc.vmaddr);
               if (ssnap != branch_slot_snap.end()) {
                  bool no_fallthrough = false;
                  if (prev_inst != nullptr) {
                     no_fallthrough = (last_flow_cat == XED_CATEGORY_RET ||
                                       last_flow_cat == XED_CATEGORY_UNCOND_BR ||
                                       (ret_call_dead && ft_dead));
                  }
                  const size_t before = anchor_slots.size();
                  if (no_fallthrough) {
                     anchor_slots = ssnap->second;
                  } else {
                     slot_intersect(anchor_slots, ssnap->second);
                  }
                  if (std::getenv("MACHO_TRACE_ANCHORSLOT")) {
                     fprintf(stderr, "[aslot] join  tgt=0x%zx %s cur=%zu snap=%zu -> %zu prevcat=%s prev2=%s\n",
                             (size_t)inst->loc.vmaddr,
                             no_fallthrough ? "ADOPT" : "isect", before,
                             ssnap->second.size(), anchor_slots.size(),
                             xed_category_enum_t2str(last_flow_cat),
                             xed_category_enum_t2str(prev2_flow_cat));
                  }
                  branch_slot_snap.erase(ssnap);
               }
            }
         }

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
               /* The stores cleared above are exactly the function's ENTRY
                * saves — promote them so the epilogue restore is recognisable
                * (see entry_save_slots). */
               entry_save_slots = pre_anchor_saves;
               pre_anchor_saves.clear();
               anchors[reg] = inst->loc.vmaddr;
               anchored_region = true;
               is_anchor_pop = true;
            }
         }

         /* (1b) Separate-thunk PIC anchor: `call ___i686.get_pc_thunk.<r>`
          *      leaves %r = the return address = the following instruction's
          *      vmaddr. Apply the anchor AFTER the step-6 call-clobber clear
          *      (which erases EAX/ECX/EDX on any CALL) so it survives even for
          *      a caller-saved target register. */
         xed_reg_enum_t thunk_anchor_reg = XED_REG_INVALID;
         std::size_t thunk_anchor_vm = 0;
         if (xed_decoded_inst_get_category(&xedd) == XED_CATEGORY_CALL) {
            const ssize_t brdisp =
               xed_decoded_inst_get_branch_displacement(&xedd);
            if (brdisp != 0) {
               const std::size_t after =
                  inst->loc.vmaddr + xed_decoded_inst_get_length(&xedd);
               auto t = pic_thunks.find(after + brdisp);
               if (t != pic_thunks.end()) {
                  thunk_anchor_reg = t->second;
                  thunk_anchor_vm = after;
               }
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
               /* SIB-indexed forms `[anchor + idx*scale + disp]` are allowed
                * when the BASE is a known PIC anchor — the instruction.cc
                * transform will emit `lea r11,[rip+table]; op [r11+idx*scale]`
                * replacing the anchor base with the resolved table pointer.
                *
                * ★But the anchor is not always the BASE. SIB is symmetric for
                * scale 1, and clang emits the operands in whatever order its
                * addressing-mode matcher produced, so a loop over a static
                * buffer routinely comes out as
                *
                *     lea edx, [eax + edi + 0xa4b0]   # edi = get_pc_thunk anchor
                *                                     # eax = a live counter
                *
                * which is the SAME access as `[anchor + idx + disp]` with the
                * two SIB fields swapped. Skipping it left the RAW i386
                * displacement in the translated instruction, and ★a raw disp
                * is NEVER safe: translated code is bigger, so the translated
                * section layout differs from the i386 one and `anchor + disp`
                * reaches a DIFFERENT section than it did originally. Portal 2's
                * libsteam_api (i386 0x61f6) then scanned and stored through
                * `anchor + 0xa4b0`, which in the translated image lands inside
                * its own read-only __TEXT — SIGBUS, err=0x6, on a write-
                * protected r-x page. Three instructions earlier the sibling
                * `lea esi,[edi + 0xa4b1]` (anchor in the BASE, no index) was
                * rewritten CORRECTLY, which is why audits read the region clean.
                *
                * So: accept EITHER slot as the anchor. The index slot requires
                * scale 1 (`anchor*2` is not an anchor) and a base that can be
                * re-encoded AS a SIB index — field 100 means "no index", so an
                * ESP base cannot make the swap. Both slots anchored stays
                * ambiguous and still bails. Kill switch
                * M64_NO_PIC_ANCHOR_INDEX=1 restores the old skip. */
               static const bool index_anchor =
                  std::getenv("M64_NO_PIC_ANCHOR_INDEX") == nullptr;
               bool anchor_is_index = false;
               xed_reg_enum_t anchor_reg = basereg;
               if (anchors.find(basereg) == anchors.end()) {
                  if (!index_anchor) continue;
                  if (indexreg < XED_REG_EAX || indexreg > XED_REG_EDI) continue;
                  if (anchors.find(indexreg) == anchors.end()) continue;
                  if (xed_decoded_inst_get_scale(ops, i) != 1) continue;
                  if (basereg == XED_REG_ESP) continue;
                  anchor_is_index = true;
                  anchor_reg = indexreg;
               } else if (indexreg != XED_REG_INVALID) {
                  /* Index register must be a general GP (EAX..EDI) and must
                   * NOT itself be an anchor (if it is, the two-anchor form is
                   * ambiguous — bail and leave it unhandled). */
                  if (indexreg < XED_REG_EAX || indexreg > XED_REG_EDI) continue;
                  if (anchors.find(indexreg) != anchors.end()) continue;
               }
               const unsigned dwidth =
                  xed_decoded_inst_get_memory_displacement_width(ops, i);
               if (dwidth != sizeof(uint32_t)) continue;
               auto anchor_it = anchors.find(anchor_reg);
               if (anchor_it == anchors.end()) continue;

               const ssize_t disp =
                  xed_decoded_inst_get_memory_displacement(ops, i);
               const std::size_t target = anchor_it->second + disp;
               if (std::getenv("MACHO_TRACE_ANCHOR")) {
                  fprintf(stderr, "[anchor] inst=0x%zx %s=%s anchor=0x%zx disp=0x%zx target=0x%zx iform=%s\n",
                          (size_t)inst->loc.vmaddr,
                          anchor_is_index ? "index" : "base",
                          xed_reg_enum_t2str(anchor_reg),
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
               /* ... and its CONTAINING fallback (instruction.cc registers
                * both): the raw disp of an anchored access routinely ALIASES
                * a zerofill span (an array offset like 0x124f80 lands inside
                * the 1.5MB __common ZeroBlob extent), so the pending
                * containing-resolve would fire in do_resolve_containing()
                * and clobber the authoritative anchor+disp target with
                * extent+aliasing-offset (guard 96_zerofill_common_interior).
                * Mirrors the (2c) heuristic-immediate cancel below. */
               env.vmaddr_resolver.cancel_containing(
                  (std::size_t)disp,
                  (const SectionBlob<bits> **)&inst->memdisp);

               inst->memidx = i;
               inst->memdisp = target_blob;
               inst->pic_anchored = true;
               inst->pic_anchor_in_index = anchor_is_index;
               /* pic_anchored implies neither absolute nor existing
                * rip-relative — transform consumes the flag and
                * synthesises a new rip-relative encoding. */
               inst->memdisp_absolute = false;

               /* Mid-blob containing fallback (mirrors instruction.cc's
                * [rip+disp] / [disp32] absolute paths). A PIC-anchored read of
                * an INTERIOR byte/short of a multi-byte __DATA word — e.g.
                * `movswl kSize+2(%anchor)` on a `{short,short}` global — makes
                * `target` a vmaddr INSIDE a blob, not at its start.
                * add_placeholder() parks that mid-blob placeholder just BEFORE
                * the next blob in Parse2, so the emitted rip-relative disp
                * skews to the neighbouring datum (+offset). Prefer the
                * containing blob + byte offset so the emit targets the exact
                * interior byte and survives the modify/convert re-layout.
                * Admits writable __DATA AND read-only opaque const/literal
                * data, mirroring the M64 [rip+disp] re-parse gate in
                * instruction.cc (the Quinn _pieceSize1+2 fix): an anchored
                * access to the INTERIOR of a packed __TEXT,__const struct —
                * clang -O0 PIC copies a {u16,u16,u16} initializer template
                * via `movl 0x6b(%anchor); movw 0x6f(%anchor)`, and when a
                * preceding odd-length __cstring leaves __const misaligned
                * mod 4, the +4 field lands mid-Immediate — otherwise strands
                * its placeholder, and Parse2 parks it at the SECTION END: the
                * translated load reads end-of-section padding instead of the
                * field (RGBColor c={0,0,0xFFFF} printed blue=0; guard
                * 97_pic_const_interior_field, the 6-byte odd-struct
                * aggregate-init field report). An anchored target is a
                * DEREFERENCED address, never an integer constant, so the
                * containing-blob guess is always valid for opaque program
                * data; __OBJC (fragile parsed metadata) and instruction
                * sections stay excluded by the predicates. override=true
                * replaces the placeholder when a containing blob exists, else
                * the placeholder resolution stands (e.g. a __bss zerofill
                * target with no blob). The M32→M64 transform propagates
                * memdisp_offset onto the synthesised rip-relative
                * instruction. */
               if (env.vmaddr_in_writable_data(target) ||
                   env.vmaddr_in_readonly_opaque_data(target)) {
                  env.vmaddr_resolver.resolve_containing(
                     target,
                     (const SectionBlob<bits> **)&inst->memdisp,
                     &inst->memdisp_offset, /*override=*/true);
               }
               break;
            }
         }

         /* (2c) Cancel HEURISTIC pointer-immediates that alias a ZEROFILL
          *      span inside PIC-anchored code. The bare-immediate heuristics
          *      (instruction.cc PUSH/MOV/ADD/CMP/c7-stack families) classify
          *      an imm32 as a pointer when its VALUE lands in a segment's
          *      vmaddr range — an accepted false-positive risk for compact
          *      file-backed sections, but catastrophic for zerofill:
          *      __DATA,__common/__bss legitimately spans megabytes (Halo:
          *      0x15cf10), so ordinary array-offset arithmetic aliases it
          *      constantly (`addl $0x124f80, %edx` computing &array[150000]
          *      against a base loaded from a non-lazy slot). Relocating such
          *      an offset ADDS the translated section base into pure pointer
          *      arithmetic -> garbage address -> SIGBUS/SIGFPE class.
          *
          *      Context the parse-time probe lacks, this pass has: a LIVE PIC
          *      anchor means the enclosing function is PIC codegen, and PIC
          *      code NEVER embeds absolute-address immediates (globals are
          *      reached anchor-relative / through slots — the same reasoning
          *      as the heuristic's own MH_PIE image-level gate, applied at
          *      function granularity). So inside an anchored region, an
          *      immediate aliasing zerofill is an OFFSET: cancel its pending
          *      exact + containing resolutions and drop any already-attached
          *      pointee so it emits verbatim. Non-PIC functions (no anchor)
          *      keep the heuristic — fixed-address images genuinely bake
          *      absolute zerofill pointers into immediates. Structural
          *      pointer operands (bare `[disp32]`, non-lazy slots) are NOT
          *      Immediate::heuristic and are never cancelled. */
         /*      ★2026-09-13: the ZEROFILL restriction was pure conservatism and
          *      it let the same bug through on __TEXT. Portal 2 engine.dylib
          *      i386 0x2e4d5f, CVoxelTree::CVoxelTree:
          *          movl $0x1000, 0x4(%esp)        # 4096 -- a page size
          *      The immediate 0x1000 aliases the image's OWN early __TEXT, so
          *      the heuristic relocated it and the argument arrived as
          *          lea r11,[rip-0x475d43]; mov %r11d,0x4(%rsp)   # 0x1000145a
          *      i.e. a CODE ADDRESS where an allocation size belongs. Two of
          *      those per run reached the allocator as ~300 MB requests that
          *      tracked ASLR (size == engine_base + 0x1460, low 12 bits always
          *      0x460) and were SERVED, so nothing ever failed and no
          *      signal-based tool could see them.
          *      The governing argument does not mention zerofill at all: PIC
          *      code NEVER embeds an absolute-address immediate, whatever
          *      section the value happens to alias. Small constants (0x1000,
          *      0x2000, a struct size) alias low __TEXT as readily as large
          *      ones alias a megabyte __common. So cancel on ANY aliased
          *      section. An immediate that aliases NOTHING was never heuristic,
          *      so this is exactly "all of them"; genuine absolute immediates
          *      live in fixed-address NON-PIC functions, which have no anchor
          *      and never reach here (guards 95_abs32_imm_const,
          *      98_abs32_imm_group, 87_alu_absdest_imm_ptr). */
         static const bool imm_cancel_zerofill_only =
            std::getenv("M64_PIC_ANCHOR_IMM_ZEROFILL_ONLY") != nullptr;
         if ((!anchors.empty() || anchored_region) &&
             inst->imm != nullptr && inst->imm->heuristic &&
             (!imm_cancel_zerofill_only ||
              env.vmaddr_in_zerofill(inst->imm->value))) {
            env.vmaddr_resolver.cancel(
               (std::size_t)inst->imm->value,
               (const SectionBlob<bits> **)&inst->imm->pointee);
            env.vmaddr_resolver.cancel_containing(
               (std::size_t)inst->imm->value,
               (const SectionBlob<bits> **)&inst->imm->pointee);
            inst->imm->pointee = nullptr;
         }

         /* (2d) Cancel the `[base+disp32]` absolute-table heuristic inside
          *      PIC-anchored code when the base is NOT an anchor. Same
          *      function-granularity reasoning as (2c): PIC codegen reaches
          *      globals anchor-relative or through slot-loaded pointers, so
          *      a `[reg+disp32]` whose base is an ORDINARY pointer register
          *      is pointer+OFFSET arithmetic — its raw disp32 aliasing a
          *      segment (routinely a megabyte zerofill span: 0x124f80 =
          *      &array[150000]-&array inside the __common extent) must NOT
          *      resolve as an absolute table base. The parse-time probe
          *      (instruction.cc, gated on a non-PIE MH_EXECUTE IMAGE) can't
          *      see function granularity — and a non-PIE LINK routinely
          *      contains -fPIC objects (guard 96_zerofill_common_interior's
          *      main: `movl $imm32, 0x124f80(%slotbase)` was mis-lea'd once
          *      the containing fallback admitted extent interiors).
          *      Anchor-based forms were taken over by (2) above
          *      (pic_anchored=true, excluded here); genuine absolute
          *      `[base+disp32]` sites live in fixed-address non-PIC
          *      functions — no live anchors — and keep the heuristic
          *      (Halo's C6 82 zerofill store, guard 98_abs32_imm_group). */
         if ((!anchors.empty() || anchored_region) &&
             inst->memdisp_absolute && !inst->pic_anchored) {
            const xed_operand_values_t* mops =
               xed_decoded_inst_operands_const(&xedd);
            const xed_reg_enum_t mbase =
               xed_decoded_inst_get_base_reg(mops, inst->memidx);
            if (mbase != XED_REG_INVALID) {
               const ssize_t mdisp =
                  xed_decoded_inst_get_memory_displacement(mops, inst->memidx);
               env.vmaddr_resolver.cancel(
                  (std::size_t)mdisp,
                  (const SectionBlob<bits> **)&inst->memdisp);
               env.vmaddr_resolver.cancel_containing(
                  (std::size_t)mdisp,
                  (const SectionBlob<bits> **)&inst->memdisp);
               inst->memdisp = nullptr;
               inst->memdisp_offset = 0;
               inst->memdisp_absolute = false;
            }
         }

         /* A register whose anchor step (2b) deliberately (re-)established on
          * THIS instruction, and which the generalised definition-kill below
          * must therefore leave alone. */
         xed_reg_enum_t anchor_keep = XED_REG_INVALID;

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
                  if (std::getenv("MACHO_TRACE_ANCHORSLOT")) {
                     fprintf(stderr, "[aslot] store inst=0x%zx %s->(%s,%zd) anchor=%d\n",
                             (size_t)inst->loc.vmaddr, xed_reg_enum_t2str(src),
                             xed_reg_enum_t2str(mbase), (ssize_t)sdisp,
                             a != anchors.end());
                  }
                  /* Any store to a slot invalidates its ENTRY-save status: the
                   * slot no longer holds the register's function-entry value. */
                  entry_save_slots.erase(slot);
                  if (a != anchors.end()) {
                     anchor_slots[slot] = a->second;   /* slot now holds anchor */
                     pre_anchor_saves.erase(slot);
                  } else {
                     anchor_slots.erase(slot);          /* overwritten -> not anchor */
                     /* Candidate entry save. Only promoted if an anchor pop
                      * follows (i.e. this store was in the prologue). */
                     if (src >= XED_REG_EAX && src <= XED_REG_EDI) {
                        pre_anchor_saves[slot] = src;
                     } else {
                        pre_anchor_saves.erase(slot);
                     }
                  }
               } else if (iform == XED_IFORM_MOV_GPRv_MEMv) {
                  /* load slot -> reg */
                  auto s = anchor_slots.find(slot);
                  const xed_reg_enum_t dst =
                     xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
                  if (std::getenv("MACHO_TRACE_ANCHORSLOT")) {
                     fprintf(stderr, "[aslot] load  inst=0x%zx (%s,%zd)->%s hit=%d nslots=%zu\n",
                             (size_t)inst->loc.vmaddr, xed_reg_enum_t2str(mbase),
                             (ssize_t)sdisp, xed_reg_enum_t2str(dst),
                             s != anchor_slots.end(), anchor_slots.size());
                  }
                  if (dst >= XED_REG_EAX && dst <= XED_REG_EDI) {
                     auto es = entry_save_slots.find(slot);
                     const bool entry_restore =
                        entry_save_gate &&
                        es != entry_save_slots.end() && es->second == dst &&
                        /* Only a CALLEE-SAVED i386 SysV register is restored
                         * from its entry save; EAX/ECX/EDX are caller-saved and
                         * never carry an entry value across the function. */
                        (dst == XED_REG_EBX || dst == XED_REG_ESI ||
                         dst == XED_REG_EDI);
                     if (s != anchor_slots.end()) {
                        anchors[dst] = s->second;    /* reload of a spilled anchor */
                        anchor_keep = dst;
                     } else if (entry_restore) {
                        anchor_keep = dst;
                        /* EPILOGUE RESTORE of the callee-saved PIC register from
                         * the very slot the PROLOGUE saved it into. At RUNTIME
                         * this really does end the anchor's life — but the walk
                         * is LINEAR, not a CFG traversal, and an epilogue sits
                         * lexically BEFORE every basic block that the function
                         * only reaches by a branch taken earlier. Erasing here
                         * therefore disarms the rewrite for all of them, and
                         * they keep their stale i386 displacements.
                         *
                         * branch_anchor_snap repairs that for blocks reached by
                         * a DIRECT forward branch, but it is keyed on branch
                         * displacements — so it cannot see a block reached only
                         * through an INDIRECT `jmp %reg` PIC switch dispatch.
                         * That combination (entry save + early epilogue +
                         * jump-table case bodies) leaves the anchor
                         * unrecoverable (Civ IV "Init Python": Python 2.6
                         * `PyString_Format`-class helper at i386 0x96fd1 — its
                         * post-epilogue case body's `movl 0x51033(%ebx),%eax`
                         * kept the raw i386 GOT displacement, so at runtime
                         * translated_anchor + i386_disp landed 0x1d87 into
                         * __TEXT,__cstring and the following `movl (%eax),%eax`
                         * dereferenced the ASCII "e AS" -> SIGSEGV at
                         * 0x53412065).
                         *
                         * Keeping the anchor is the correct linear-walk
                         * approximation, and matches what step (3) already does
                         * for the register-only form of the same restore.
                         * Structural trigger: a prologue store of THIS register
                         * to THIS slot, seen BEFORE the anchor pop. A register
                         * genuinely re-purposed mid-function is loaded from an
                         * argument/temporary slot with no matching pre-anchor
                         * save (Portal 2 CVProfile ctor: `mov 0x8(%ebp),%esi` =
                         * `this`, a positive-displacement ARGUMENT slot) and
                         * still erases below. */
                     } else {
                        /* Loading NON-anchor data (e.g. a function argument
                         * `mov %esi, 0x8(%ebp)` = `this`) into the register
                         * overwrites whatever it held — including a STALE anchor
                         * leaked from a prior function (anchors live until RET,
                         * and a suppressed RET-clear can leak one across the
                         * function boundary). Without this, the leaked anchor
                         * makes a subsequent object-field store `disp(%esi)`
                         * (esi=this) get misrewritten as a rip-relative GLOBAL
                         * store into __TEXT -> SIGBUS (Portal 2 CVProfile ctor).
                         * A genuine spilled anchor is reloaded via the
                         * anchor_slots branch above, so clearing here only
                         * affects non-anchor loads. */
                        anchors.erase(dst);
                     }
                  }
               }
            }
         }

         /* (2b') ANY DEFINITION OF A REGISTER ENDS THAT REGISTER'S ANCHOR.
          *      Step (2b) above only watches EBP/ESP frame slots, because that
          *      is where an anchor is legitimately spilled and reloaded. But a
          *      function re-purposes its anchor register in every other way
          *      too, and each of those left the dead anchor in place:
          *
          *        mov  0x64(%ebx),%esi      # an object FIELD, arbitrary base
          *        movzx 0x6bfa(%esi),%ebx   # a byte load, not the plain `mov`
          *        pop  %esi                 # a restore that is not the anchor pop
          *        xor  %ebx,%ebx            # no memory operand at all
          *
          *      Enumerating iforms is how this bug keeps coming back: the rule
          *      is about the DEFINITION, not about which opcode performed it.
          *      So ask XED which operands this instruction WRITES and erase the
          *      anchor of every 32-bit GPR among them. A sub-register write
          *      (%bl, %bx) counts — the value is no longer the anchor even
          *      though the upper bytes survive — hence the normalisation to the
          *      enclosing 32-bit register.
          *
          *      ⚠ ONE STALE ANCHOR HAS TWO OPPOSITE FAILURE MODES, and this
          *      family has produced both:
          *
          *      - on the BASE, a FALSE rewrite: an ADDRESS is produced where a
          *        VALUE belongs. Portal 2 CKeyValuesSystem::CKeyValuesSystem
          *        (libvstdlib i386 0x11fb1): %esi is the get_pc_thunk anchor,
          *        `mov 0x64(%ebx),%esi` loads m_HashTable.m_Size into it, and
          *        the next instruction `lea 0x7ff(%esi),%edi` became
          *        `lea rip+...,%edi` — an image address where a count belongs.
          *        CUtlMemory::Grow then asked for next_pow2(load_base)*8, i.e.
          *        1-2 GiB depending purely on ASLR; the NULL made Source
          *        _exit(0) silently.
          *
          *      - on the INDEX, a SUPPRESSED rewrite: step (2)'s two-anchor
          *        form is ambiguous and bails, leaving the RAW i386
          *        displacement against a TRANSLATED anchor. Portal 2
          *        localize.dylib i386 0xbc8f, in a static initializer:
          *            movzx 0x6bfa(%esi),%ebx        # %ebx := a small count
          *            mov   %eax,0x791e(%esi,%ebx,8) # store into a global table
          *        %ebx had held a stale anchor, so the store kept `0x791e` and
          *        landed at translated_anchor+0x791e inside its OWN read-only
          *        __TEXT -> SIGBUS. Every sibling access in that same block
          *        (seven of them, all without an index) WAS rewritten correctly,
          *        which is exactly why the audit tools read the function as
          *        clean: only the two indexed forms were left raw.
          *
          *      Exemptions are narrow and explicit: the `call $+0; pop %reg`
          *      that ESTABLISHES an anchor (step 1, which already recorded it),
          *      and the frame-slot reload/entry-restore that step (2b) just
          *      recognised (anchor_keep). A reg->reg move re-establishes the
          *      anchor in step (2c) below, which runs after this. A separate
          *      get_pc_thunk call establishes its anchor after the call-clobber
          *      clear further down. So nothing that genuinely re-creates an
          *      anchor is lost here.
          *
          *      ⛔ POP IS EXEMPT ENTIRELY, and that is a measurement, not caution.
          *      `pop %reg` is how a function EXIT restores its callee-saved
          *      registers, and a LINEAR walk reaches every function exit BEFORE
          *      the blocks that are only entered from somewhere else. Killing on
          *      pop therefore disarms the rewrite for all of them and they keep
          *      their raw i386 displacements — the very suppressed-rewrite failure
          *      this whole family produces, just introduced from the other side.
          *      Two distinct shapes were measured on Portal 2, and TOGETHER they
          *      rule out a gate:
          *
          *        engine.dylib — exit by RET, then a slow-path block reached by a
          *        branch taken earlier (182 sites across five images):
          *          0x2d1be  add esp,0x1c
          *          0x2d1c1  pop esi ; pop edi ; pop ebx ; pop ebp ; ret
          *          0x2d1c6  dec 0x5f67ce(%esi)   <- %esi IS still the anchor
          *
          *        libsteam.dylib — exit by TAIL CALL, then an EH LANDING PAD (94
          *        sites). The unwinder enters 0x1112 having restored the callee
          *        -saved registers per the CFI, so %esi holds the anchor exactly
          *        as the main path's identical access at 0x10fb does:
          *          0x001109  pop esi ; pop edi ; pop ebx ; pop ebp
          *          0x00110d  jmp 0x101c                   <- tail call, not RET
          *          0x001112  mov ebx,eax                  <- landing pad
          *          0x001114  mov 0x28bf24(%esi),%esi      <- %esi IS the anchor
          *
          *      Gating on "a forward branch target is still pending above here"
          *      fixes the first shape and CANNOT fix the second: a landing pad is
          *      reached through __eh_frame/LSDA, not through a branch
          *      displacement, so pending_forward_targets never contains it. And
          *      nothing distinguishes an epilogue pop from a data pop locally.
          *      ⇒ Do not kill on pop. This restores the long-standing baseline
          *      (before this rule, pop never killed an anchor) and matches the
          *      file's standing approximation: for a linear walk, KEEPING an
          *      anchor is the safe error, because a lost rewrite is a wild access
          *      while a kept one is merely stale until the next real definition.
          *      A mid-function data pop can still leak an anchor; no bug has ever
          *      been attributed to that, whereas killing cost 276 real sites. */
         static const bool memload_kill =
            std::getenv("M64_NO_PIC_ANCHOR_MEMLOAD_KILL") == nullptr;
         const bool is_pop = (xed_decoded_inst_get_category(&xedd) ==
                              XED_CATEGORY_POP);
         if (memload_kill && !is_anchor_pop && !is_pop && !anchors.empty()) {
            const xed_inst_t *xi = xed_decoded_inst_inst(&xedd);
            const unsigned nop = xed_inst_noperands(xi);
            for (unsigned i = 0; i < nop; ++i) {
               const xed_operand_t *op = xed_inst_operand(xi, i);
               if (!xed_operand_written(op)) continue;
               const xed_operand_enum_t nm = xed_operand_name(op);
               if (!xed_operand_is_register(nm)) continue;
               const xed_reg_enum_t raw = xed_decoded_inst_get_reg(&xedd, nm);
               if (raw == XED_REG_INVALID) continue;
               const xed_reg_enum_t r = xed_get_largest_enclosing_register32(raw);
               if (r < XED_REG_EAX || r > XED_REG_EDI) continue;
               if (r == anchor_keep) continue;
               anchors.erase(r);
            }
         }

         /* (2c) Track the anchor through a register-to-register move
          *      `mov %src, %dst` (MOV_GPRv_GPRv, both 0x89 and 0x8B
          *      encodings). The destination takes on the source's contents:
          *      if the source carries an anchor, propagate it; otherwise the
          *      destination's prior value — possibly a STALE PIC anchor (e.g.
          *      a get_pc_thunk base popped into %edi at function entry) — is
          *      overwritten by non-anchor data and the anchor must be cleared.
          *      Step 2b already does this for the stack spill/reload form
          *      (`mov disp(%ebp), %reg`); a reg->reg move is the other way a
          *      function reuses an anchor register for unrelated data. Without
          *      it a later base+index access off the reassigned register,
          *      `mov disp(%dst,%idx), ...`, is misrewritten as a rip-relative
          *      absolute reference into read-only __TEXT (Portal 2
          *      CLoggingSystem::LogDirect: %edi = PIC anchor at entry, then
          *      `mov %eax, %edi` reassigns it from a function argument, then
          *      `mov 0x745c(%edi,%eax), %eax` -> rip-relative load of a garbage
          *      vtable ptr -> SIGSEGV). For XED both MOV_GPRv_GPRv encodings
          *      present operand 0 = destination, operand 1 = source. The
          *      fast/slow-path branch JOIN (branch_anchor_snap) preserves the
          *      anchor for any slow-path block reached by a forward branch
          *      taken BEFORE the move, so clearing here is safe. */
         if (iform == XED_IFORM_MOV_GPRv_GPRv_89 ||
             iform == XED_IFORM_MOV_GPRv_GPRv_8B) {
            const xed_reg_enum_t mdst =
               xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
            const xed_reg_enum_t msrc =
               xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1);
            if (mdst >= XED_REG_EAX && mdst <= XED_REG_EDI && mdst != msrc) {
               auto a = anchors.find(msrc);
               if (a != anchors.end()) {
                  anchors[mdst] = a->second;   /* anchor copied src -> dst */
               } else {
                  anchors.erase(mdst);          /* overwritten with non-anchor */
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
                  /* Snapshot the live anchor state at this branch SOURCE for the
                   * target's JOIN (see branch_anchor_snap). `anchors` here is the
                   * pre-branch state (a COND/UNCOND branch neither pops an anchor
                   * nor clobbers a reg). If another branch already targets `tgt`,
                   * intersect so only anchors agreed on by ALL sources survive. */
                  auto sit = branch_anchor_snap.find(tgt);
                  if (sit == branch_anchor_snap.end()) {
                     branch_anchor_snap[tgt] = anchors;
                  } else {
                     anchor_intersect(sit->second, anchors);
                  }
                  /* Same snapshot for the anchor SPILL SLOTS (see
                   * branch_slot_snap): a slot killed by a store on the
                   * fall-through path is still live on this branch path. */
                  if (slot_snap_on) {
                     auto slit = branch_slot_snap.find(tgt);
                     if (slit == branch_slot_snap.end()) {
                        branch_slot_snap[tgt] = anchor_slots;
                     } else {
                        slot_intersect(slit->second, anchor_slots);
                     }
                     if (std::getenv("MACHO_TRACE_ANCHORSLOT")) {
                        fprintf(stderr, "[aslot] snap  br=0x%zx tgt=0x%zx cur=%zu -> snap=%zu\n",
                                (size_t)inst->loc.vmaddr, (size_t)tgt,
                                anchor_slots.size(), branch_slot_snap[tgt].size());
                     }
                  }
               }
            }
         }

         /* (5b) The SAME snapshot for an INDIRECT dispatch of a CLAIMED PIC
          *      jump table. `jmp *%reg` is a branch whose target set is known
          *      statically — DetectJumpTables resolved every case body while
          *      auto-sizing the table (env.pic_switch_targets, keyed by this
          *      dispatch's vmaddr) — but step (5) only sees a branch
          *      DISPLACEMENT, so the linear walk carried no state into any case
          *      body except the one that happens to follow the dispatch.
          *
          *      ★MEASURED (Portal 2 shaderapidx9 `CShaderShadowDX8::DepthFunc`,
          *      i386 0x35a60, anchor `pop %ecx` at 0x35a6a):
          *          0x35a91  mov 0xb2(%ecx,%edx,4),%edx   ; fused PIC table
          *          0x35a9a  jmp *%edx
          *          0x35a9c  mov 0x3c5c2(%ecx),%eax       ; case 1 - rewritten
          *          0x35abc/0x35ad8/0x35af8  same insn    ; cases 2-4 - NOT
          *      Case 1 is the walk's next instruction so it still holds the
          *      anchor; a CALL in it erases ECX (caller-saved), and cases 2-4,
          *      reachable ONLY through the table, then kept their raw i386
          *      displacement. At runtime `mov (%eax),%eax` dereferenced
          *      translated __text read as a pointer (0x76654472, ASCII "rDev")
          *      -> SIGSEGV.
          *
          *      A case body is a plain intra-function block, so the same
          *      ADOPT/INTERSECT join at (0b) applies unchanged, and the join
          *      being an INTERSECTION means this can only RECOVER an anchor the
          *      linear walk dropped, never invent one.
          *      KILL SWITCH M64_NO_PIC_ANCHOR_JT_TARGETS=1. */
         if (jt_target_snap_on) {
            auto jt = env.pic_switch_targets.find(inst->loc.vmaddr);
            if (jt != env.pic_switch_targets.end()) {
               for (const std::size_t tgt : jt->second) {
                  if (tgt <= inst->loc.vmaddr) { continue; }  /* forward only */
                  auto sit = branch_anchor_snap.find(tgt);
                  if (sit == branch_anchor_snap.end()) {
                     branch_anchor_snap[tgt] = anchors;
                  } else {
                     anchor_intersect(sit->second, anchors);
                  }
                  if (slot_snap_on) {
                     auto slit = branch_slot_snap.find(tgt);
                     if (slit == branch_slot_snap.end()) {
                        branch_slot_snap[tgt] = anchor_slots;
                     } else {
                        slot_intersect(slit->second, anchor_slots);
                     }
                  }
               }
            }
         }

         /* (6) Control transfers. Three classes of behavior:
          *     - RET: function exit ONLY IF no forward branch targets
          *       are pending. Otherwise this is a mid-function RET (the
          *       slow path follows, reached via the pending branch).
          *     - INTERRUPT / SYSCALL / SYSRET: system transition; clear.
          *       EXCEPT int3 (0xCC): an abort/breakpoint trap whose fall-through
          *       is dead code — live successors reach any shared target via a
          *       branch that BYPASSES the int3. Clearing at int3 poisons the
          *       branch_anchor_snap intersection at that target (pattern
          *       `je L; int3; jmp L` — the dead jmp records an empty snapshot
          *       which intersects the live path's anchors to {}), losing the
          *       PIC anchor. So int3 is transparent to anchor tracking.
          *       (Root cause: Portal 2 engine.dylib CVoxelTree::CVoxelTree.)
          *     - CALL (not `call $+0`): the callee may clobber caller-
          *       saved regs (EAX/ECX/EDX in i386 sysv). Clear those;
          *       keep EBX/ESI/EDI/EBP (callee preserves them).
          *     - COND_BR / UNCOND_BR: intra-procedural, anchors survive. */
         const bool is_pic_call_zero =
            iform == XED_IFORM_CALL_NEAR_RELBRz &&
            xed_decoded_inst_get_branch_displacement(&xedd) == 0;
         if (cat == XED_CATEGORY_RET) {
            if (pending_forward_targets.empty()) {
               if (std::getenv("MACHO_TRACE_ANCHORSLOT") && !anchor_slots.empty()) {
                  fprintf(stderr, "[aslot] CLEAR-ret vmaddr=0x%zx nslots=%zu\n",
                          (size_t)inst->loc.vmaddr, anchor_slots.size());
               }
               anchors.clear();
               anchor_slots.clear();
               entry_save_slots.clear();
               pre_anchor_saves.clear();
               anchored_region = false;
            }
         } else if ((cat == XED_CATEGORY_INTERRUPT &&
                     xed_decoded_inst_get_iclass(&xedd) != XED_ICLASS_INT3) ||
                    cat == XED_CATEGORY_SYSCALL ||
                    cat == XED_CATEGORY_SYSRET) {
            anchors.clear();
            anchor_slots.clear();
            entry_save_slots.clear();
            pre_anchor_saves.clear();
            anchored_region = false;
         } else if (cat == XED_CATEGORY_CALL && !is_pic_call_zero) {
            anchors.erase(XED_REG_EAX);
            anchors.erase(XED_REG_ECX);
            anchors.erase(XED_REG_EDX);
         }

         /* Establish a separate-thunk anchor now (post call-clobber clear). */
         if (thunk_anchor_reg != XED_REG_INVALID) {
            anchors[thunk_anchor_reg] = thunk_anchor_vm;
            anchored_region = true;
         }

         /* Track the last non-nop category for block (0b)'s dead-fall-through
          * test. Alignment nops between a RET/uncond-JMP and a branch target are
          * transparent: skip them so the target still sees the RET that precedes
          * the padding. */
         if (cat != XED_CATEGORY_NOP && cat != XED_CATEGORY_WIDENOP) {
            prev2_flow_cat = last_flow_cat;
            last_flow_cat = cat;
            /* Sticky dead-fall-through (see ft_dead's declaration): a RET or an
             * unconditional JMP ends the fall-through, a CALL placed after one
             * is the noreturn-trap idiom and keeps it dead, anything else is a
             * real fall-through again. */
            if (cat == XED_CATEGORY_RET || cat == XED_CATEGORY_UNCOND_BR) {
               ft_dead = true;
            } else if (cat != XED_CATEGORY_CALL) {
               ft_dead = false;
            }
         }
         prev_inst = inst;
      }
   }

   /* Map a GPR to its canonical 32-bit name so 64-bit (`jmp rax`) and 32-bit
    * (`add eax,ebx`) references to the same architectural register compare
    * equal. Non-GPRs pass through unchanged. */
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
      /* the transform's scratch table base: `lea r11,[rip+d]` then an
       * addr32 `[r11d + idx*s]` indexed load (EA-wrap fidelity) */
      case XED_REG_R11D: return XED_REG_R11;
      default: return r;
      }
   }
   static bool jt_is_gpr32(xed_reg_enum_t r) {
      return r >= XED_REG_EAX && r <= XED_REG_EDI;
   }

   /* True iff this instruction actually WRITES its XED_OPERAND_REG0 operand.
    * A memory-destination MOV (`mov [ebp-0x1c],%eax`, XED_IFORM_MOV_MEMv_GPRv)
    * has REG0 = the SOURCE register: it is read, never written. DetectJumpTables'
    * "this insn overwrote %reg0, so drop its stale table state" rule must not
    * fire on such a store, or a spilled table base is killed by its own spill. */
   static bool jt_reg0_written(const xed_decoded_inst_t *xedd) {
      const xed_inst_t *xi = xed_decoded_inst_inst(xedd);
      if (xi == nullptr) { return true; }   /* conservative */
      const unsigned n = xed_inst_noperands(xi);
      for (unsigned i = 0; i < n; ++i) {
         const xed_operand_t *op = xed_inst_operand(xi, i);
         if (xed_operand_name(op) == XED_OPERAND_REG0) {
            return xed_operand_written(op);
         }
      }
      return true;                          /* conservative */
   }

   /* Recognise PIC relative-offset switch jump tables (see JumpTableEntry).
    * A pre-pass over the raw section bytes (the linear sweep hasn't run yet):
    * decode each instruction, track the PIC anchor, then match the dispatch
    *     lea  %tbl,[anchor]            ; table base
    *     mov  %t,[%tbl + idx*4]        ; t = table[idx]
    *     add  %t,%anchor              ; t = case_target
    *     jmp  %t
    * On a match, auto-size the table (it ends at the first case body = the
    * lowest entry target above the table base) and record each 4-byte slot's
    * vmaddr -> anchor vmaddr in env.jump_table_slots, so TextParser emits a
    * relocatable JumpTableEntry for it. The anchor is recovered both for the
    * i386 input (`call $+0; pop %reg` sets %reg = the pop's vmaddr) AND for the
    * already-translated x86_64 form the `convert` stage re-parses (the
    * `lea r11,[rip+d]; push r11d; jmp; mov %reg,[rsp]` sequence that call_op
    * synthesises leaves %reg = the rip-relative anchor) — so the table is kept
    * relocatable across BOTH parses of __text. Conservative: every entry must
    * resolve to an in-section code address or the scan stops; a partial/garbage
    * match simply yields no slots (parse proceeds exactly as before). */
   template <Bits bits>
   void Section<bits>::DetectJumpTables(const Image& img, ParseEnv<bits>& env) {
      if (parser != TextParser) {
         return;
      }

      const std::size_t sect_lo = sect.addr;
      const std::size_t sect_hi = sect.addr + sect.size;
      const bool trace = std::getenv("MACHO_TRACE_JUMPTABLE") != nullptr;
      if (trace) {   /* one header per pass, so per-pass table sets can be compared */
         fprintf(stderr, "[jumptable] pass bits=%d sect=0x%zx\n",
                 bits == Bits::M32 ? 32 : 64, (size_t)sect_lo);
      }

      std::unordered_map<xed_reg_enum_t, std::size_t> anchors;  /* reg -> anchor vmaddr */
      std::unordered_map<xed_reg_enum_t, std::size_t> tbl_addr;  /* reg -> table base vmaddr */
      /* reg -> (table_base, anchor); anchor 0 until the `add` resolves it */
      std::unordered_map<xed_reg_enum_t, std::pair<std::size_t, std::size_t>> tbl_val;

      /* Table bases SPILLED TO A FRAME SLOT. Under register pressure GCC computes
       * the PIC table base once, parks it in a local (`mov [ebp-0x1c],%eax`) and
       * reloads it into a DIFFERENT register at each dispatch
       * (`mov %edi,[ebp-0x1c]; mov %eax,[%edi+%eax*4]; add %eax,%ebx; jmp *%eax`).
       * Tracking only registers loses the base across the spill, the dispatch is
       * never recognised, and the linear sweep then disassembles the inline table
       * AS CODE — re-encoding e.g. the entry byte 0x53 (`pushl %ebx`) into the
       * 7-byte x86_64 push sequence, which both destroys the entries and shifts
       * every later one. (Civ IV "Init Python": Python 2.6 marshal r_object's
       * 126-entry switch, table 0x9b268; the shifted read yields an entry whose
       * low 16 bits are 0x0000, so `add %anchor` leaves the anchor's low half
       * intact and only its high half wrong -> jump to unmapped memory, SIGBUS.)
       * Keyed by (normalised frame base reg, displacement); invalidated whenever
       * that slot is rewritten, the base register is redefined, or %esp moves. */
      std::map<std::pair<int, ssize_t>, std::size_t> stack_tbl;

      /* The SAME thing happens to the PIC ANCHOR, and losing it costs more.
       * `mov [ebp-0x14],%edi` parks the anchor in a local and a later dispatch
       * reloads it into a different register: `mov %ecx,[ebp-0x14];
       * add %ecx,(%ecx,%eax,4),0x236; jmp *%ecx`. The anchor REWRITE follows the
       * spill and retargets that load at the translated table, but this detector
       * did not, so the table was never claimed and its ENTRIES kept their i386
       * anchor-relative offsets -> `translated_anchor + i386_offset` = a
       * mid-instruction address. (Portal 2 KeyValues::MakeCopy: the resulting
       * runaway ate the entire 16 MB i386 stack.) Same key, same lifetime rules
       * as stack_tbl. */
      std::map<std::pair<int, ssize_t>, std::size_t> stack_anchor;
      const bool jt_spill = std::getenv("M64_NO_JT_SPILL_SLOTS") == nullptr;
      const bool jt_anchor_spill =
         jt_spill && std::getenv("M64_NO_JT_ANCHOR_SPILL") == nullptr;

      /* Forward intra-function branch targets not yet reached; decides whether
       * a RET ends the function (see "Anchor lifetime" below). */
      std::set<std::size_t> pending_targets;

      /* Anchor state at each forward branch SOURCE, keyed by its target: the
       * linear walk drops an anchor a NOT-TAKEN arm clobbers (`jbe L; lea
       * %eax,[%eax+str]; call warn; jmp out; L: add %eax,[%eax+%ecx*4+d];
       * jmp *%eax` — the call erased the %eax anchor before the dispatch that
       * only the branch reaches). On reaching the target, ADOPT the snapshot if
       * the previous instruction has no fall-through, else INTERSECT it with
       * the carried state — DetectPicAnchoredDisps' branch_anchor_snap rule.
       * MEASURED, Portal 2 shaderapidx9 CShaderShadowDX8::BlendOp (i386
       * 0x3600e): the rewrite pass re-anchored the fused load, this pass never
       * claimed its table. Kill switch M64_NO_JT_BRANCH_SNAP=1. */
      std::map<std::size_t, std::unordered_map<xed_reg_enum_t, std::size_t>> anchor_snap;
      /* table base -> (entry count, anchor), for the next-table clamp below */
      std::map<std::size_t, std::pair<std::size_t, std::size_t>> tables;
      /* table base -> vmaddr of the `jmp %reg` that dispatches it, so the
       * post-pass below can publish each table's case-body targets keyed by
       * their dispatch (env.pic_switch_targets). */
      std::map<std::size_t, std::size_t> table_dispatch;
      const bool jt_table_bound = std::getenv("M64_NO_JT_TABLE_BOUND") == nullptr;
      const bool jt_branch_snap = std::getenv("M64_NO_JT_BRANCH_SNAP") == nullptr;
      xed_category_enum_t prev_cat = XED_CATEGORY_INVALID;
      const bool jt_midfn_ret = std::getenv("M64_NO_JT_MIDFN_RET") == nullptr;
      const bool jt_func_reset = std::getenv("M64_NO_JT_FUNC_RESET") == nullptr;
      const bool jt_x64_anchor_at = std::getenv("M64_NO_JT_X64_ANCHOR_AT") == nullptr;
      const bool jt_sym_resync = std::getenv("M64_NO_JT_SYM_RESYNC") == nullptr;
      /* M64: a translated call to a PIC thunk is `lea r11,[rip+ret]; …; jmp
       * thunk`, and the thunk leaves %reg = ret. Recognise it like the i386
       * `call ___i686.get_pc_thunk.<r>`. Before the anchor-at rule (a-x64) these
       * anchors were picked up by accident from the next unrelated `mov r,[rsp]`;
       * with it they were lost (bugreporter_filequeue: M32 15 tables, M64 4).
       * Kill switch M64_NO_JT_X64_THUNK=1. */
      const bool jt_x64_thunk = std::getenv("M64_NO_JT_X64_THUNK") == nullptr;
      const bool jt_anchor_copy = std::getenv("M64_NO_JT_ANCHOR_COPY") == nullptr;
      const bool jt_fused_add_x64 = std::getenv("M64_NO_JT_FUSED_ADD_X64") == nullptr;

      bool prev_call0 = false;     /* previous insn was `call $+0` (e8 00000000) */
      std::size_t pend_r11 = 0;    /* value of the last `lea r11,[rip+d]` (x86_64 anchor dance) */

      /* Pre-sweep for GCC PIC thunks (`mov %reg,(%esp); ret`): a `call` to one
       * is the separate-thunk PIC anchor (vs inline `call $+0; pop`). The thunk
       * usually sits AFTER its callers, so a forward pre-sweep is needed to
       * recognise the call when we reach it. Without this, GCC-built switch
       * dispatches anchored through get_pc_thunk keep their stale i386 table-
       * relative displacements and jump to garbage. (i386 input only; the M64
       * convert re-parse uses the lea-r11/mov-[rsp] dance, no `mov reg,[esp]`.) */
      std::unordered_map<std::size_t, xed_reg_enum_t> pic_thunks;
      {
         std::size_t pit = sect.offset, pvm = sect.addr;
         const std::size_t pend = sect.offset + sect.size;
         std::size_t mvm = 0; xed_reg_enum_t mreg = XED_REG_INVALID;
         while (pit < pend) {
            xed_decoded_inst_t xd;
            xed_decoded_inst_zero_set_mode(&xd, &Instruction<bits>::dstate());
            xed_decoded_inst_set_input_chip(&xd, XED_CHIP_INVALID);
            if (xed_decode(&xd, &img.at<uint8_t>(pit), img.size() - pit)
                != XED_ERROR_NONE) {
               mreg = XED_REG_INVALID; ++pit; ++pvm; continue;
            }
            const unsigned l = xed_decoded_inst_get_length(&xd);
            if (l > 1 && jt_sym_resync) {   /* same symbol re-sync as the main walk */
               auto ns = env.func_syms.upper_bound(pvm);
               if (ns != env.func_syms.end() && *ns < pvm + l) {
                  mreg = XED_REG_INVALID; ++pit; ++pvm; continue;
               }
            }
            /* The thunk's return: a RET (i386), or in a TRANSLATED image the
             * `mov r11d,[rsp]` that starts the translated return
             * (`mov ebx,[rsp]; mov r11d,[rsp]; lea rsp,[rsp+4]; jmp r11`). */
            const bool thunk_ret =
               xed_decoded_inst_get_category(&xd) == XED_CATEGORY_RET ||
               (bits == Bits::M64 && jt_x64_thunk &&
                xed_decoded_inst_get_iform_enum(&xd) == XED_IFORM_MOV_GPRv_MEMv &&
                xed_decoded_inst_get_reg(&xd, XED_OPERAND_REG0) == XED_REG_R11D &&
                xed_decoded_inst_get_base_reg(xed_decoded_inst_operands_const(&xd), 0) == XED_REG_RSP &&
                xed_decoded_inst_get_memory_displacement(xed_decoded_inst_operands_const(&xd), 0) == 0);
            if (mreg != XED_REG_INVALID && thunk_ret) {
               pic_thunks[mvm] = mreg;
            }
            mreg = XED_REG_INVALID;
            if (xed_decoded_inst_get_iform_enum(&xd) == XED_IFORM_MOV_GPRv_MEMv) {
               const xed_operand_values_t* o = xed_decoded_inst_operands_const(&xd);
               if (xed_decoded_inst_number_of_memory_operands(&xd) == 1 &&
                   (xed_decoded_inst_get_base_reg(o, 0) == XED_REG_ESP ||
                    xed_decoded_inst_get_base_reg(o, 0) == XED_REG_RSP) &&
                   xed_decoded_inst_get_index_reg(o, 0) == XED_REG_INVALID &&
                   xed_decoded_inst_get_memory_displacement(o, 0) == 0) {
                  const xed_reg_enum_t d =
                     xed_decoded_inst_get_reg(&xd, XED_OPERAND_REG0);
                  if (d >= XED_REG_EAX && d <= XED_REG_EDI) { mvm = pvm; mreg = d; }
               }
            }
            pit += l; pvm += l;
         }
      }

      /* Seed from the GLOBAL symbol-table thunk map so cross-section thunks
       * (Civ IV: __textcoal_nt) anchor switch dispatches in __text too. See
       * ParseEnv::pic_thunks / the matching seed in DetectPicAnchoredDisps.
       * M32 only: the named-thunk anchor is the i386 `call get_pc_thunk` idiom;
       * the M64 convert re-parse uses the lea-r11/mov-[rsp] dance and must not
       * adopt these i386-vmaddr-keyed entries. */
      if constexpr (bits == Bits::M32) {
         for (const auto& kv : env.pic_thunks) {
            const xed_reg_enum_t r = gpr32_from_enc(kv.second);
            if (r != XED_REG_INVALID) { pic_thunks.emplace(kv.first, r); }
         }
      }

      std::size_t it = sect.offset;
      std::size_t vmaddr = sect.addr;
      const std::size_t end = sect.offset + sect.size;
      while (it < end) {
         xed_decoded_inst_t xedd;
         xed_decoded_inst_zero_set_mode(&xedd, &Instruction<bits>::dstate());
         xed_decoded_inst_set_input_chip(&xedd, XED_CHIP_INVALID);
         if (xed_decode(&xedd, &img.at<uint8_t>(it), img.size() - it) != XED_ERROR_NONE) {
            tbl_addr.clear(); tbl_val.clear(); prev_call0 = false;
            prev_cat = XED_CATEGORY_INVALID;
            ++it; ++vmaddr;
            continue;
         }
         const unsigned len = xed_decoded_inst_get_length(&xedd);
         /* Re-sync at a function symbol the decoded instruction STRADDLES: the
          * bytes before it are inter-function padding, exactly the rule the
          * TextParser sweep applies (its func_syms boundary guard). Without it
          * this walk decoded `nop…nop 00 | 8b 4c 24 04 4c 8d 1d …` (a translated
          * entry after its 0x00 alignment byte) as `add [rbx+…],cl`, swallowed
          * the entry and the `lea r11` of the anchor dance, and so never saw the
          * function's anchor on any M64 re-parse. Kill switch M64_NO_JT_SYM_RESYNC=1. */
         if (len > 1 && jt_sym_resync) {
            auto ns = env.func_syms.upper_bound(vmaddr);
            if (ns != env.func_syms.end() && *ns < vmaddr + len) {
               tbl_addr.clear(); tbl_val.clear(); prev_call0 = false;
               prev_cat = XED_CATEGORY_INVALID;
               ++it; ++vmaddr;
               continue;
            }
         }
         const xed_iform_enum_t iform = xed_decoded_inst_get_iform_enum(&xedd);
         const xed_category_enum_t cat = xed_decoded_inst_get_category(&xedd);
         const xed_operand_values_t* ops = xed_decoded_inst_operands_const(&xedd);
         const xed_reg_enum_t reg0raw = xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG0);
         const xed_reg_enum_t reg0 = jt_norm32(reg0raw);

         /* A symbolled function entry starts with NO live anchor. The RET rule
          * below cannot guarantee that: it keeps anchors across a RET while a
          * forward branch target is pending, and a tail `jmp` to a later
          * function stays pending for up to 64 KB. The M64 re-parse never sees
          * a RET at all (the translated return is `jmp *%r11`). Either way a
          * previous function's anchor leaked in: a stale %eax anchor turned
          * `lea %esi,[%eax-4]` into a bogus table base and lost CTempMeshDX8::
          * RenderPass's table (shaderapidx9 0xcfc7), and the M64 re-parse
          * claimed ExecuteCommandBuffer's table with an earlier function's
          * anchor. Kill switch M64_NO_JT_FUNC_RESET=1. */
         if (jt_func_reset && env.func_syms.count(vmaddr) != 0) {
            anchors.clear(); tbl_addr.clear(); tbl_val.clear(); pend_r11 = 0;
            stack_tbl.clear(); stack_anchor.clear(); pending_targets.clear();
            anchor_snap.clear();
            prev_call0 = false;
         }
         if (jt_branch_snap && !anchor_snap.empty()) {
            auto sn = anchor_snap.find(vmaddr);
            if (sn != anchor_snap.end()) {
               if (prev_cat == XED_CATEGORY_RET || prev_cat == XED_CATEGORY_UNCOND_BR) {
                  anchors = sn->second;
               } else {
                  for (auto i = anchors.begin(); i != anchors.end(); ) {
                     auto o = sn->second.find(i->first);
                     i = (o == sn->second.end() || o->second != i->second)
                            ? anchors.erase(i) : std::next(i);
                  }
               }
            }
            anchor_snap.erase(anchor_snap.begin(), anchor_snap.upper_bound(vmaddr));
         }

         bool sets_state = false;
         bool spill_write = false;   /* this insn is the tracked frame-slot spill */

         /* (a-i386) PIC anchor: `pop %reg` right after `call $+0`. */
         if (iform == XED_IFORM_POP_GPRv_58 && prev_call0 && jt_is_gpr32(reg0)) {
            anchors[reg0] = vmaddr;
            tbl_addr.erase(reg0); tbl_val.erase(reg0);
            sets_state = true;
         }
         /* (a-x64) PIC anchor: the translated `lea r11,[rip+d]; …; mov %reg,[rsp]`
          *         dance (call_op) leaves %reg = the rip-relative value.
          *         Only the `mov` AT the dance's target is the anchor pop: the
          *         translated `call $+0` pushes the address of that very `mov`.
          *         Any later `mov %reg,[rsp]` (an epilogue `pop`, an argument
          *         read) was taking the stale return address of the last real
          *         CALL as an anchor, so every M64 re-parse claimed fused tables
          *         with a wrong anchor and a wrong size (shaderapidx9
          *         0x35d00: `pop %ebx` of the epilogue re-anchored %ebx at the
          *         return site of a `call`). Kill switch M64_NO_JT_X64_ANCHOR_AT. */
         else if (iform == XED_IFORM_MOV_GPRv_MEMv && pend_r11 != 0 &&
                  (vmaddr == pend_r11 || !jt_x64_anchor_at) &&
                  xed_decoded_inst_get_base_reg(ops, 0) == XED_REG_RSP &&
                  xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
                  jt_is_gpr32(reg0)) {
            anchors[reg0] = pend_r11;
            tbl_addr.erase(reg0); tbl_val.erase(reg0);
            sets_state = true;
         }
         /* (a-copy) `mov %dst,%src` carries the anchor along, as
          *     DetectPicAnchoredDisps step (2c) does. Copy only: an overwrite
          *     does not clear %dst (this walk has no branch-join snapshot, and
          *     keeping an anchor is the safe error for a linear walk).
          *     GCC parks the anchor in one register and copies it into another
          *     for the dispatch (`pop %esi; ... mov %edx,%esi; ... mov
          *     %eax,[%edx+%eax*4+d]; add %eax,%edx; jmp *%eax`). The rewrite
          *     pass followed the copy and re-anchored the load, this detector
          *     did not: the table was never claimed on the i386 pass, and the
          *     M64 re-parse then claimed it with whatever STALE anchor %edx
          *     still held from an earlier function. MEASURED, Portal 2
          *     shaderapidx9 CShaderAPIDx8::ExecuteCommandBuffer (i386 0x1e0f9).
          *     Kill switch M64_NO_JT_ANCHOR_COPY=1. */
         else if ((iform == XED_IFORM_MOV_GPRv_GPRv_89 ||
                   iform == XED_IFORM_MOV_GPRv_GPRv_8B) && jt_is_gpr32(reg0) &&
                  jt_anchor_copy) {
            const xed_reg_enum_t src =
               jt_norm32(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1));
            if (src != reg0) {
               auto a = anchors.find(src);
               if (a != anchors.end()) { anchors[reg0] = a->second; }
            }
         }
         /* (b) table base via lea: i386 `[anchor+disp]` or x86_64 `[rip+disp]`.
          *     The M64 `convert` re-parse sees the ALREADY-TRANSLATED dispatch,
          *     whose table base is loaded into the scratch reg r11
          *     (`lea r11,[rip+disp]`). r11 is outside jt_is_gpr32's EAX..EDI
          *     range, so accept it explicitly — otherwise the table is never
          *     re-detected on the convert pass and TextParser disassembles its
          *     relocated 4-byte entries as code (an entry like 0x174 = `74 01 ..`
          *     mis-decodes as `je rel8` and length-widening shifts/corrupts the
          *     whole table -> wild jump into __LINKEDIT; Portal2 CRC32 dispatch). */
         else if (iform == XED_IFORM_LEA_GPRv_AGEN &&
                  (jt_is_gpr32(reg0) || reg0raw == XED_REG_R11)) {
            const xed_reg_enum_t base = xed_decoded_inst_get_base_reg(ops, 0);
            const xed_reg_enum_t index = xed_decoded_inst_get_index_reg(ops, 0);
            const ssize_t disp = xed_decoded_inst_get_memory_displacement(ops, 0);
            if (index == XED_REG_INVALID && base == XED_REG_RIP) {
               tbl_addr[reg0] = vmaddr + len + disp;     /* rip-relative */
               tbl_val.erase(reg0);
               sets_state = true;
            } else if (index == XED_REG_INVALID) {
               auto a = anchors.find(jt_norm32(base));
               if (a != anchors.end()) {
                  /* ★A `lea %reg,[%anchor + disp]` is only a TABLE BASE if it
                   * lands in THIS section. Anchor-relative addressing is how
                   * i386 PIC reaches EVERYTHING — strings, globals, vtables —
                   * so most such leas are not table bases at all, and recording
                   * one poisons case (c): the indexed load finds a `tbl_addr`
                   * entry and takes the table-base branch instead of the
                   * (c-combined) anchor+disp branch that would compute the real
                   * table. The rest of this pass already requires the table and
                   * every case target to be inside the section (auto-size below
                   * tests exactly that), so gating here is the same rule applied
                   * one step earlier, not a new assumption.
                   *
                   * MEASURED, Portal 2 libtogl `GLMDecode` (i386 0x27890): the
                   * default arm of the switch does `lea eax,[eax+0x19fbf]` to
                   * get its "unknown" STRING, which recorded tbl_addr[EAX] =
                   * 0x41858 — a __cstring address, far outside __text
                   * [0x1630,0x2fb10). Two instructions later
                   * `mov edx,[eax+edx*4+0xb3]` therefore resolved its table to
                   * 0x41858 instead of 0x2794c, auto-size failed bounds at once
                   * (i = 0 < 2), and NO slots were recorded. The 11 table bytes
                   * were then parsed as CODE: entry 0x73 decoded as a short
                   * `jae` and was WIDENED to a 6-byte near jcc, 0x4d became
                   * `dec ebp`, 0x5d became the `pop ebp` idiom — so the emitted
                   * table was shifted and shot through with instructions.
                   * GLMDecode(10) jumped to anchor+0x24648d48. ⚠The linear walk
                   * is why a NOT-TAKEN arm can do this: at run time %eax is
                   * still the anchor when the indexed load executes.
                   *
                   * Falling through with sets_state = false lets the stale-drop
                   * below erase any tbl_addr/tbl_val for %reg0, which is what we
                   * want — the lea genuinely redefined it.
                   * Kill switch M64_NO_JT_LEA_BOUNDS=1. */
                  static const bool lea_bounds =
                     std::getenv("M64_NO_JT_LEA_BOUNDS") == nullptr;
                  const std::size_t cand =
                     (std::size_t)((ssize_t)a->second + disp);
                  if (!lea_bounds || (cand >= sect_lo && cand < sect_hi)) {
                     tbl_addr[reg0] = cand;                /* anchor-relative */
                     tbl_val.erase(reg0);
                     sets_state = true;
                  }
               }
            }
         }
         /* (b-spill) `mov [%ebp/%esp + disp],%reg` where %reg currently holds a
          *     table base -> remember the frame SLOT as holding that base, so the
          *     later reload into any register recovers it. A store defines no
          *     register, so sets_state stays false (the reg0-written guard below
          *     is what keeps %reg's own state alive across its spill). Storing
          *     anything else to a tracked slot invalidates it. */
         else if (jt_spill && iform == XED_IFORM_MOV_MEMv_GPRv &&
                  xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
                  (jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)) == XED_REG_EBP ||
                   jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)) == XED_REG_ESP)) {
            const std::pair<int, ssize_t> key {
               (int)jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)),
               xed_decoded_inst_get_memory_displacement(ops, 0) };
            auto tb = tbl_addr.find(reg0);
            if (tb != tbl_addr.end()) { stack_tbl[key] = tb->second; }
            else { stack_tbl.erase(key); }
            auto an = anchors.find(reg0);
            if (jt_anchor_spill && an != anchors.end()) {
               stack_anchor[key] = an->second;
            } else {
               stack_anchor.erase(key);
            }
            spill_write = true;
         }
         /* (b-reload) `mov %reg,[%ebp/%esp + disp]` from a slot holding a spilled
          *     table base -> %reg is a table base again. A plain (non-indexed)
          *     frame load, so it can never be the indexed table read of case (c). */
         else if (jt_spill && iform == XED_IFORM_MOV_GPRv_MEMv && jt_is_gpr32(reg0) &&
                  xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID &&
                  (jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)) == XED_REG_EBP ||
                   jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)) == XED_REG_ESP) &&
                  (stack_tbl.count({ (int)jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)),
                                     xed_decoded_inst_get_memory_displacement(ops, 0) }) ||
                   stack_anchor.count({ (int)jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)),
                                        xed_decoded_inst_get_memory_displacement(ops, 0) }))) {
            const std::pair<int, ssize_t> key {
               (int)jt_norm32(xed_decoded_inst_get_base_reg(ops, 0)),
               xed_decoded_inst_get_memory_displacement(ops, 0) };
            auto tb = stack_tbl.find(key);
            if (tb != stack_tbl.end()) { tbl_addr[reg0] = tb->second; }
            else                       { tbl_addr.erase(reg0); }
            auto an = stack_anchor.find(key);
            if (an != stack_anchor.end()) { anchors[reg0] = an->second; }
            else                          { anchors.erase(reg0); }
            tbl_val.erase(reg0);
            sets_state = true;
         }
         /* (c) mov %reg,[%tblbase + idx*scale] -> reg holds a table entry. */
         else if (iform == XED_IFORM_MOV_GPRv_MEMv && jt_is_gpr32(reg0)) {
            const xed_reg_enum_t base = jt_norm32(xed_decoded_inst_get_base_reg(ops, 0));
            const xed_reg_enum_t index = jt_norm32(xed_decoded_inst_get_index_reg(ops, 0));
            auto tb = tbl_addr.find(base);
            if (tb == tbl_addr.end()) { tb = tbl_addr.find(index); }
            if (tb != tbl_addr.end()) {
               tbl_val[reg0] = { tb->second, 0 };
               tbl_addr.erase(reg0);
               sets_state = true;
            }
            /* (c-combined) GCC folds the table-base LEA into the load's own
             * addressing: `mov %reg,[%anchor + idx*4 + disp]` — the live PIC
             * anchor IS the base register and there is no preceding
             * `lea %tbl,[anchor+disp]` (e.g. Civ IV's inflate switch dispatch,
             * `movl 0xf9(%ebx,%eax,4),%eax` with %ebx = get_pc_thunk anchor).
             * tbl_addr was therefore never populated. Recognise the combined
             * form directly from the anchor map: the table sits at anchor+disp
             * with a 4-byte entry stride, and the following `add %reg,%anchor;
             * jmp %reg` completes the chain. Conservative: case (e) still
             * validates that every entry resolves to an in-section code address
             * before recording slots, so a plain anchor-relative indexed array
             * load (not a switch) yields nothing. */
            else if (index != XED_REG_INVALID &&
                     xed_operand_values_get_scale(ops) == 4) {
               auto a = anchors.find(base);
               if (a != anchors.end()) {
                  const ssize_t disp =
                     xed_decoded_inst_get_memory_displacement(ops, 0);
                  tbl_val[reg0] = { a->second + disp, 0 };
                  tbl_addr.erase(reg0);
                  sets_state = true;
               }
            }
         }
         /* (c+d fused) `add %anchor,[%anchor + idx*4 + disp]` — clang emits the
          *     table LOAD and the anchor ADD as ONE instruction, with the anchor
          *     serving as both the table base and the addend, and the result
          *     landing back in the anchor register:
          *
          *       add edi,[edi + edx*4 + 0x2a7]   ; edi = anchor + table[idx]
          *       jmp edi
          *
          *     The (c) -> (d) chain below never matches it: (c) wants
          *     MOV_GPRv_MEMv and (d) wants a register-to-register ADD. So the
          *     dispatch went unrecognised, the table was never claimed, and its
          *     bytes were parsed as CODE — the same end state as the unbounded
          *     table-base lea, reached by a different route.
          *
          *     MEASURED, Portal 2 libtogl `IDirect3D9::CheckDeviceFormat`
          *     (i386 0x15b67, anchor 0x15a31, table 0x15cd8, 9 entries whose raw
          *     values 0x272/0x13f/0x20e/0x215/0x29d decode as the junk
          *     `jb`/`aas`/`add` stream the pcmap rows show). The jump then left
          *     %rip in the low-4GB arena at a `prot=rw-` page (err=0x15:
          *     instruction fetch + protection).
          *
          *     Both roles must be the SAME live anchor, and the scale must be 4;
          *     anything else is an ordinary indexed add, not a dispatch. Case (e)
          *     still validates every entry against the section before recording
          *     slots, so a non-switch load that happens to match this shape
          *     yields nothing. Kill switch M64_NO_JT_FUSED_ADD=1. */
         else if (iform == XED_IFORM_ADD_GPRv_MEMv && jt_is_gpr32(reg0)) {
            static const bool fused_add =
               std::getenv("M64_NO_JT_FUSED_ADD") == nullptr;
            const xed_reg_enum_t mbase =
               jt_norm32(xed_decoded_inst_get_base_reg(ops, 0));
            const xed_reg_enum_t midx =
               jt_norm32(xed_decoded_inst_get_index_reg(ops, 0));
            auto a = anchors.find(reg0);
            if (fused_add && a != anchors.end() && mbase == reg0 &&
                midx != XED_REG_INVALID && midx != reg0 &&
                xed_operand_values_get_scale(ops) == 4) {
               const ssize_t disp =
                  xed_decoded_inst_get_memory_displacement(ops, 0);
               const std::size_t tbl = (std::size_t)((ssize_t)a->second + disp);
               if (tbl >= sect_lo && tbl < sect_hi) {
                  tbl_val[reg0] = { tbl, a->second };   /* table AND anchor */
                  tbl_addr.erase(reg0);
                  sets_state = true;
               }
            }
            /* The same fused add with the table base in ANOTHER register:
             * `add %anchor,[%tbl + idx*4]`. This is what the TRANSLATED fused
             * dispatch looks like (`lea r11,[rip+d]; add edx,[r11d+rcx*4];
             * jmp rdx`), so without it every M64 re-parse (modify, strip-bind,
             * interpose, convert) missed each fused table and re-emitted its
             * entries as code: entry 0x175 = `75 01` (jne) was widened to a
             * 6-byte jcc, shifting every later entry by 4. MEASURED, Portal 2
             * shaderapidx9 ImageLoader::D3DFormatToImageFormat (i386 0x49b10,
             * two fused tables): M32 26 tables, M64 19.
             * Kill switch M64_NO_JT_FUSED_ADD_X64=1. */
            else if (fused_add && a != anchors.end() &&
                     jt_fused_add_x64 &&
                     midx != XED_REG_INVALID && midx != reg0 &&
                     xed_operand_values_get_scale(ops) == 4 &&
                     xed_decoded_inst_get_memory_displacement(ops, 0) == 0) {
               auto tb = tbl_addr.find(mbase);
               if (tb != tbl_addr.end()) {
                  tbl_val[reg0] = { tb->second, a->second };
                  tbl_addr.erase(reg0);
                  sets_state = true;
               }
            }
         }
         /* (d) add %reg,%anchor where reg holds a table entry -> reg = target.
          *     The anchor register's value resolves the table's anchor. */
         else if (iform == XED_IFORM_ADD_GPRv_GPRv_01 ||
                  iform == XED_IFORM_ADD_GPRv_GPRv_03) {
            const xed_reg_enum_t src =
               jt_norm32(xed_decoded_inst_get_reg(&xedd, XED_OPERAND_REG1));
            auto tv = tbl_val.find(reg0);
            auto a = anchors.find(src);
            if (tv != tbl_val.end() && a != anchors.end()) {
               tv->second.second = a->second;   /* fill anchor; keep for the jmp */
               sets_state = true;
            }
         }
         /* (e) jmp %reg where reg = a relocated table target -> a jump table. */
         else if (iform == XED_IFORM_JMP_GPRv) {
            auto tv = tbl_val.find(reg0);
            if (tv != tbl_val.end() && tv->second.second != 0) {
               const std::size_t table_base = tv->second.first;
               const std::size_t anchor = tv->second.second;
               /* Auto-size: the table ends at the lowest entry target above the
                * base (= first case body). Stop on any out-of-section target,
                * and at the next function symbol: a table never spans into
                * another function (the post-pass below also stops it at the
                * next table). */
               std::size_t min_target = sect_hi;
               if (jt_table_bound) {
                  auto ns = env.func_syms.upper_bound(table_base);
                  if (ns != env.func_syms.end() && *ns < min_target) { min_target = *ns; }
               }
               std::size_t i = 0;
               for (; ; ++i) {
                  const std::size_t slot = table_base + i * 4;
                  if (slot + 4 > sect_hi || slot >= min_target) { break; }
                  const std::size_t slot_off = sect.offset + (slot - sect.addr);
                  const int32_t raw = (int32_t)img.at<uint32_t>(slot_off);
                  const std::size_t target = anchor + raw;
                  if (target < sect_lo || target >= sect_hi) { break; }
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
                     fprintf(stderr, "[jumptable] dispatch@0x%zx anchor=0x%zx "
                             "table=0x%zx count=%zu\n",
                             (size_t)vmaddr, (size_t)anchor, (size_t)table_base,
                             (size_t)i);
                  }
                  /* Skip the table bytes so the linear sweep stays ALIGNED for
                   * the rest of the function. The PIC switch idiom emits the
                   * jump table inline immediately after the indirect jmp; if we
                   * keep decoding those 4-byte entries as instructions the sweep
                   * misaligns and silently drops every downstream dispatch in
                   * the same function. This bit M64 convert harder than M32
                   * transform (its relocated `target-anchor` entries are large
                   * values that misdecode into longer bogus instructions), which
                   * is exactly the 45-vs-51 detection gap. Only skip a table
                   * that sits just after the jmp (inline) so a far rip-relative
                   * table never causes us to step over real intervening code. */
                  const std::size_t table_end = table_base + i * 4;
                  if (table_base >= vmaddr && table_base - vmaddr < 64 &&
                      table_end > vmaddr) {
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

         /* Drop stale table state for a register this insn overwrote but didn't
          * redefine (keeps the lea->mov->add->jmp chain tight). Only when the
          * insn actually WRITES %reg0: for a memory-destination MOV, REG0 is the
          * source operand, and erasing on it kills a table base at the very
          * instruction that spills it (see jt_reg0_written). */
         if (!sets_state && jt_is_gpr32(reg0) &&
             (!jt_spill || jt_reg0_written(&xedd))) {
            tbl_addr.erase(reg0);
            tbl_val.erase(reg0);
         }

         if (jt_spill && (!stack_tbl.empty() || !stack_anchor.empty())) {
            /* A tracked frame slot dies when anything else writes it ... */
            if (!spill_write && xed_decoded_inst_number_of_memory_operands(&xedd) > 0 &&
                xed_decoded_inst_mem_written(&xedd, 0) &&
                xed_decoded_inst_get_index_reg(ops, 0) == XED_REG_INVALID) {
               const xed_reg_enum_t mb = jt_norm32(xed_decoded_inst_get_base_reg(ops, 0));
               if (mb == XED_REG_EBP || mb == XED_REG_ESP) {
                  const std::pair<int, ssize_t> dead {
                     (int)mb, xed_decoded_inst_get_memory_displacement(ops, 0) };
                  stack_tbl.erase(dead);
                  stack_anchor.erase(dead);
               }
            }
            /* ... when the frame pointer itself is redefined (epilogue/`leave`) ... */
            if (iform == XED_IFORM_LEAVE ||
                (jt_norm32(reg0raw) == XED_REG_EBP && jt_reg0_written(&xedd))) {
               for (auto i = stack_tbl.begin(); i != stack_tbl.end(); ) {
                  i = (i->first.first == (int)XED_REG_EBP) ? stack_tbl.erase(i)
                                                           : std::next(i);
               }
               for (auto i = stack_anchor.begin(); i != stack_anchor.end(); ) {
                  i = (i->first.first == (int)XED_REG_EBP) ? stack_anchor.erase(i)
                                                           : std::next(i);
               }
            }
            /* ... and every %esp-keyed slot dies whenever %esp moves, since the
             * displacement is then measured from a different place. */
            if (cat == XED_CATEGORY_PUSH || cat == XED_CATEGORY_POP ||
                cat == XED_CATEGORY_CALL || cat == XED_CATEGORY_RET ||
                iform == XED_IFORM_LEAVE ||
                (jt_norm32(reg0raw) == XED_REG_ESP && jt_reg0_written(&xedd))) {
               for (auto i = stack_tbl.begin(); i != stack_tbl.end(); ) {
                  i = (i->first.first == (int)XED_REG_ESP) ? stack_tbl.erase(i)
                                                           : std::next(i);
               }
               for (auto i = stack_anchor.begin(); i != stack_anchor.end(); ) {
                  i = (i->first.first == (int)XED_REG_ESP) ? stack_anchor.erase(i)
                                                           : std::next(i);
               }
            }
         }

         /* Anchor lifetime: clear on a function-ending RET / system transitions;
          * a real CALL clobbers caller-saved regs. Table-pointer state is purely
          * local.
          *
          * ★A RET ends the function only when no forward branch target is still
          * pending — the rule DetectPicAnchoredDisps already applies (its steps
          * 4-6: prune targets we have reached, record in-section forward
          * branches within 64 KB, keep anchors across a RET while any remain).
          * A LINEAR walk reaches an early-return epilogue before the code behind
          * it, so clearing at every RET dropped the anchor for every later
          * dispatch in the function. The rewrite pass kept it and re-anchored
          * those loads, but their tables were never claimed, so the entries kept
          * i386 offsets. MEASURED, Portal 2 libcef: anchor 0xc7823e, early return
          * 0xc7825f, dispatches 0xc78930 / 0xc78aa4 unclaimed; the entries decode
          * as BOUND and trip the width guard. Kill switch M64_NO_JT_MIDFN_RET=1. */
         pending_targets.erase(pending_targets.begin(),
                               pending_targets.upper_bound(vmaddr));
         if (cat == XED_CATEGORY_COND_BR || cat == XED_CATEGORY_UNCOND_BR) {
            const ssize_t bd = xed_decoded_inst_get_branch_displacement(&xedd);
            const std::size_t tgt = vmaddr + len + bd;
            if (bd > 0 && tgt < sect_hi && tgt - vmaddr <= 0x10000) {
               pending_targets.insert(tgt);
               if (jt_branch_snap) {
                  auto sit = anchor_snap.find(tgt);
                  if (sit == anchor_snap.end()) {
                     anchor_snap[tgt] = anchors;
                  } else {
                     for (auto i = sit->second.begin(); i != sit->second.end(); ) {
                        auto o = anchors.find(i->first);
                        i = (o == anchors.end() || o->second != i->second)
                               ? sit->second.erase(i) : std::next(i);
                     }
                  }
               }
            }
         }
         const bool midfn_ret =
            jt_midfn_ret && cat == XED_CATEGORY_RET && !pending_targets.empty();
         const bool is_pic_call0 =
            iform == XED_IFORM_CALL_NEAR_RELBRz &&
            xed_decoded_inst_get_branch_displacement(&xedd) == 0;
         if ((cat == XED_CATEGORY_RET && !midfn_ret) ||
             cat == XED_CATEGORY_INTERRUPT ||
             cat == XED_CATEGORY_SYSCALL || cat == XED_CATEGORY_SYSRET) {
            anchors.clear(); tbl_addr.clear(); tbl_val.clear(); pend_r11 = 0;
            stack_tbl.clear(); stack_anchor.clear();
         } else if (cat == XED_CATEGORY_CALL && !is_pic_call0) {
            anchors.erase(XED_REG_EAX);
            anchors.erase(XED_REG_ECX);
            anchors.erase(XED_REG_EDX);
            tbl_addr.clear(); tbl_val.clear();
         }

         /* Separate-thunk PIC anchor: `call ___i686.get_pc_thunk.<r>` leaves
          * %r = the return address. Apply after the call-clobber clear so it
          * survives for any target register. */
         if (cat == XED_CATEGORY_CALL) {
            const ssize_t bd = xed_decoded_inst_get_branch_displacement(&xedd);
            if (bd != 0) {
               auto t = pic_thunks.find(vmaddr + len + bd);
               if (t != pic_thunks.end()) { anchors[t->second] = vmaddr + len; }
            }
         } else if (bits == Bits::M64 && jt_x64_thunk && cat == XED_CATEGORY_UNCOND_BR && xed_decoded_inst_get_branch_displacement(&xedd) != 0 &&
                    pend_r11 == vmaddr + len) {
            /* translated call: the pushed return address is this jmp's successor */
            auto t = pic_thunks.find(vmaddr + len +
                                     xed_decoded_inst_get_branch_displacement(&xedd));
            if (t != pic_thunks.end()) { anchors[t->second] = vmaddr + len; }
         }

         prev_call0 = (iform == XED_IFORM_CALL_NEAR_RELBRz && len == 5 &&
                       xed_decoded_inst_get_branch_displacement(&xedd) == 0);
         /* Remember the x86_64 anchor-dance `lea r11,[rip+d]` for the following
          * `mov %reg,[rsp]`. */
         if (iform == XED_IFORM_LEA_GPRv_AGEN && reg0raw == XED_REG_R11 &&
             xed_decoded_inst_get_base_reg(ops, 0) == XED_REG_RIP) {
            pend_r11 = vmaddr + len + xed_decoded_inst_get_memory_displacement(ops, 0);
         }
         prev_cat = cat;
         it += len;
         vmaddr += len;
      }

      /* A run of entries stops at the next table. Two switches sharing one
       * anchor often place their tables back to back at the function's end,
       * below every case body, so "stop at the first case body above the base"
       * never fires and the first table's auto-size ran straight through the
       * second (shaderapidx9 ImageLoader::D3DFormatToImageFormat: 0x49ce4
       * sized 20 = its own 11 + the 9 of 0x49d10). Harmless while both share an
       * anchor, wrong as soon as they do not. The walk meets dispatches in code
       * order, not table order, so clamp here once every base is known and
       * re-emit each clamped table's slots with its OWN anchor.
       * Kill switch M64_NO_JT_TABLE_BOUND=1. */
      if (jt_table_bound) {
         for (auto t = tables.begin(); t != tables.end(); ++t) {
            auto nx = std::next(t);
            if (nx == tables.end()) { break; }
            const std::size_t room = (nx->first - t->first) / 4;
            if (t->second.first > room) {
               if (trace) {
                  fprintf(stderr, "[jumptable] clamp table=0x%zx count=%zu->%zu "
                          "(next table 0x%zx)\n", (size_t)t->first,
                          (size_t)t->second.first, room, (size_t)nx->first);
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
      }

      /* Publish each claimed table's CASE-BODY TARGETS keyed by its dispatch,
       * for DetectPicAnchoredDisps' indirect-branch anchor snapshot. Computed
       * here, after the clamp, so a table shortened above contributes only the
       * targets it still owns. Nothing else reads this map, so populating it
       * cannot change translation on its own. */
      for (const auto& t : tables) {
         auto d = table_dispatch.find(t.first);
         if (d == table_dispatch.end() || t.second.first == 0) { continue; }
         std::vector<std::size_t>& tgts = env.pic_switch_targets[d->second];
         for (std::size_t k = 0; k < t.second.first; ++k) {
            const std::size_t slot = t.first + k * 4;
            if (slot + 4 > sect_hi) { break; }
            const std::size_t slot_off = sect.offset + (slot - sect.addr);
            const int32_t raw = (int32_t)img.at<uint32_t>(slot_off);
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
      /* ZEROFILL sections (S_ZEROFILL / S_GB_ZEROFILL / S_THREAD_LOCAL_ZEROFILL
       * — __DATA,__bss/__common) reserve VMADDR space only: they occupy no
       * file bytes, so the file-offset cursor must not move (a zerofill
       * section contributes to segment vmsize, not filesize; Segment::Build's
       * filesize = offset delta then stays correct automatically). Align the
       * vmaddr only, pin the header's offset to 0 (the Mach-O convention for
       * zerofill), and derive the header size from the VMADDR delta — the
       * generic offset-delta below would report 0 once the blobs stop
       * advancing the offset cursor. Content is the spanning ZeroBlob
       * extent(s) plus zero-size placeholders (symbol/export/memdisp
       * anchors), whose Build assigns their final interior vmaddrs. */
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
            /* Zerofill carries no section relocations (there are no file
             * bytes to relocate); emit the conventional reloff=0/nreloc=0
             * instead of a junk cursor offset. */
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

      /* Even-align function entries by inserting a single 0-byte padding blob
       * before any func_entry blob that would otherwise land at an odd VMADDR.
       * This preserves the Itanium C++ ABI pointer-to-member-function low-bit
       * tag (see SectionBlob::func_entry): a non-virtual member function
       * pointer is just the function address, and an odd address sets bit0,
       * which the pmf dispatch code interprets as "virtual" -> bogus vtable
       * deref (f09_member_func_ptr).
       *
       * Two subtleties drive the implementation:
       *  (1) The decision MUST be made against the live build cursor: instruction
       *      sizes are not final until each Instruction::Build runs (it
       *      re-resolves relative branches, which can widen them), so a pre-pass
       *      over elem->size() would mis-predict parity.
       *  (2) The function's defined SYMBOL is a zero-size placeholder blob that
       *      sits in `content` at the SAME vmaddr, immediately before the entry
       *      instruction (its n_value is read from that blob's post-Build
       *      loc.vmaddr).  The pad must go BEFORE that whole same-vmaddr run, or
       *      the code would shift even while the symbol (and the pmf literal that
       *      references it) stays odd.
       * We therefore pad when the cursor is odd and the run of blobs starting
       * here -- skipping zero-size placeholders -- reaches a func_entry at this
       * vmaddr.  Align the VMADDR (not the file offset): the vmaddr is final once
       * assigned here, whereas env.loc.offset is still preliminary (header /
       * load-command sizes are fixed up later, shifting section file offsets).
       * Padding is a real blob in `content`, so the sequential Emit and the
       * EXECUTE->DYLIB convert re-parse (whose func-boundary sweep already
       * tolerates inter-function padding) stay consistent.  No-op when no blob
       * carries func_entry (data sections; stripped binaries; empty func_syms). */
      const bool trace = std::getenv("MACHO_TRACE_FUNCALIGN") != nullptr;
      std::size_t nfe = 0, npad = 0;
      for (auto it = content.begin(); it != content.end(); ++it) {
         SectionBlob<bits> *elem = *it;
         if (elem->active && elem->func_entry) { ++nfe; }
         if (env.loc.vmaddr & 1) {
            /* Does the same-vmaddr run starting at `it` begin a function entry? */
            bool starts_func_entry = false;
            for (auto pk = it; pk != content.end(); ++pk) {
               SectionBlob<bits> *b = *pk;
               if (!b->active) { continue; }
               if (b->func_entry) { starts_func_entry = true; break; }
               if (b->size() != 0) { break; } /* real blob -> vmaddr advances */
            }
            if (starts_func_entry) {
               DataBlob<bits> *pad = DataBlob<bits>::Padding();
               it = content.insert(it, pad);  /* before this run; it -> pad */
               pad->Build(env);               /* allocate the pad byte */
               ++it;                          /* it -> elem again */
               elem = *it;
               ++npad;
            }
         }
         elem->Build(env);
      }
      if (trace && nfe) {
         fprintf(stderr, "[funcalign] section %s: %zu func entries, %zu even-aligned\n",
                 name().c_str(), nfe, npad);
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

      /* ZEROFILL sections parse as ONE spanning ZeroBlob extent (no per-byte
       * blobs — see ZeroBlob), so a placeholder whose vmaddr lands INSIDE the
       * extent (an interior __bss/__common symbol value, export entry, or
       * instruction memdisp target — the common case: every zerofill variable
       * beyond the first) has no blob boundary to sit at. The generic path
       * below would bump it past the extent ("past last blob" -> section
       * end), so its Build-time vmaddr — and with it every nlist n_value /
       * export address / rip-relative disp resolved through it — would be
       * wrong. Instead SPLIT the extent at the placeholder's vmaddr:
       *   [start, end)  ->  [start, ph) + placeholder + [ph, end)
       * Both halves stay registered in the vmaddr resolver (the tail
       * registers on creation), so interior resolve_containing() lookups
       * still snap to a covering extent + exact byte offset, and Build lays
       * the pieces out back-to-back — vmaddr-identical to the unsplit span. */
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

                  /* First blob whose extent reaches (or starts at/past) the
                   * placeholder. Placeholders arrive in ascending vmaddr
                   * order, so the cursor never rewinds. */
                  while (content_it != content.end() &&
                         (*content_it)->loc.vmaddr + (*content_it)->size() <=
                            ph_vmaddr) {
                     ++content_it;
                  }
                  if (content_it == content.end() ||
                      (*content_it)->loc.vmaddr >= ph_vmaddr) {
                     /* At a blob boundary (split point of an earlier
                      * placeholder, or the section start) — insert before,
                      * exactly like the generic path. end() means past every
                      * extent (malformed span); append like the generic
                      * fallback so downstream resolution still has a target. */
                     content.insert(content_it, placeholder);
                     continue;
                  }

                  auto *extent = dynamic_cast<ZeroBlob<bits> *>(*content_it);
                  if (extent == nullptr) {
                     /* Interior of a non-extent blob (cannot happen in a
                      * zerofill section parsed by ZeroBlob::Parse; guard for
                      * synthetic content). Mirror the generic soft-mismatch:
                      * warn + insert before the NEXT blob. */
                     fprintf(stderr,
                             "warning: placeholder vmaddr 0x%zx inside "
                             "non-extent blob in zerofill section %s\n",
                             ph_vmaddr, name().c_str());
                     auto next_it = std::next(content_it);
                     content.insert(next_it, placeholder);
                     continue;
                  }

                  /* Split the extent. */
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
