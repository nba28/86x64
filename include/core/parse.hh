#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <unordered_map>

#include "loc.hh"
#include "types.hh"
#include "resolve.hh"
#include "region.hh"

namespace MachO {
   
   template <Bits bits> class SectionBlob;
   template <Bits bits> class RelocBlob;
   
   template <typename T>
   class CountResolver {
   public:
      void add(T *pointee) { resolver.add(++id, pointee); }
      template <typename... Args>
      void resolve(Args&&... args) { return resolver.resolve(args...); }
      
      CountResolver(const std::string& name): resolver(name), id(0) {}
      
   private:
      Resolver<std::size_t, T, false> resolver;
      unsigned id;
   };

   template <Bits bits>
   class ParseEnv {
   public:
      Archive<bits>& archive;
      
      Resolver<std::size_t, SectionBlob<bits>, true> vmaddr_resolver;
      Resolver<std::size_t, SectionBlob<bits>, true> offset_resolver;
      Resolver<uint32_t, BindNode<bits, true>, false> lazy_bind_node_resolver;
      std::unordered_map<std::size_t, RelocBlob<bits> *> relocs;
      // Resolver<std::size_t, RelocBlob<bits>> reloc_resolver; /*!< resolves by vmaddr */
      CountResolver<DylibCommand<bits>> dylib_resolver;
      CountResolver<Segment<bits>> segment_resolver;
      CountResolver<Section<bits>> section_resolver;
      Segment<bits> *current_segment = nullptr;
      Section<bits> *current_section = nullptr;
      Regions data_in_code;

      /* vmaddr-to-placeholder map */
      using TodoPlaceholders = std::map<std::size_t, Placeholder<bits> *>;
      TodoPlaceholders placeholders;

      /* i386 PIC relative-offset switch jump-table slots: maps each 4-byte
       * table-entry vmaddr to its dispatch's PIC anchor vmaddr. Populated by
       * Section::DetectJumpTables (a pre-pass before the linear sweep) and
       * consumed by TextParser, which emits a JumpTableEntry blob (instead of
       * decoding the entry bytes as code) for any offset whose vmaddr is a key
       * here. See JumpTableEntry in section_blob.hh. */
      std::map<std::size_t, std::size_t> jump_table_slots;

      /* Function-symbol vmaddrs, populated from the LC_SYMTAB nlist table
       * (all N_SECT, non-stab entries with n_value != 0).  Used by the linear
       * code sweep (Section::Parse1) to detect when a decoded instruction would
       * SPAN a function boundary (i.e. a known entry point lands inside the
       * decoded range, not at the decode start).  Such a span means the bytes
       * preceding the entry point are inter-function padding (e.g. a 0x00 align
       * byte) that the sweep has mistakenly absorbed into the previous decode.
       * The sweep truncates to a 1-byte DataBlob and retries from the next byte,
       * re-syncing cleanly at the true function boundary.
       *
       * Universal: the check triggers only when a symbol boundary falls
       * STRICTLY INSIDE the decoded instruction (vmaddr < sym < vmaddr+len),
       * which is never true for well-aligned code and exclusively fires on
       * padding bytes before a labelled entry.  Gated on M32 only (no i386 PIC
       * confusion in M64 re-parses). */
      std::set<std::size_t> func_syms;

      /* GCC PIC thunks (`___i686.get_pc_thunk.<r>`), keyed by the thunk's
       * entry vmaddr (= its nlist n_value) and valued by the x86 GPR encoding
       * (0=EAX,1=ECX,2=EDX,3=EBX,5=EBP,6=ESI,7=EDI) the thunk loads with the
       * caller's PC.  Populated from the LC_SYMTAB by NAME in the Symtab ctor
       * (first parse phase), so it is GLOBAL and ready before any section's
       * Parse1 runs.  Section::DetectPicAnchoredDisps / DetectJumpTables each
       * additionally byte-scan their own section for unnamed thunks, then
       * seed from this map so a `call ___i686.get_pc_thunk.bx` whose thunk
       * lives in a DIFFERENT text section (Civ IV/GCC put them in
       * __textcoal_nt while callers are in __text) still establishes the PIC
       * anchor.  Without it the anchored `[ebx+disp32]` falls through to the
       * generic absolute-table rewrite, which keeps the anchor base and emits
       * `[anchor + lea(rip+target)]` — double-counting the base (SIGSEGV at
       * anchor+target, Civ IV crt `start`).  The uint8_t value avoids pulling
       * xed into this header; the consumers map it back to xed_reg_enum_t. */
      std::unordered_map<std::size_t, uint8_t> pic_thunks;

      Placeholder<bits> *add_placeholder(std::size_t vmaddr);
      void do_resolve();

      /* True iff `vmaddr` lands in a program-writable data segment (the
       * `__DATA` segment family: VM_PROT_WRITE set, not the ObjC fragile-ABI
       * `__OBJC` metadata segment which is also writable but whose blobs are
       * parsed structurally). Gates the mid-blob containing-fallback on the
       * LOAD/pointer paths: in opaque `__DATA`, a mid-blob offset into the
       * nearest containing DataBlob is meaningful; in `__OBJC` the "nearest
       * containing blob" guess corrupts category/method metadata (regressed
       * +[NSObject isLogEnabled] legacy category lookup). */
      bool vmaddr_in_writable_data(std::size_t vmaddr) const;

      /* True iff vmaddr falls in a constant/string/code section that is a
       * high-confidence pointer target (__cstring/__cfstring/__const/__text/
       * __objc* etc.). Used to disambiguate a `mov [reg+disp], imm32` whose
       * imm32 might be a baked-in absolute pointer vs. an integer constant. */
      bool vmaddr_in_const_section(std::size_t vmaddr) const;


      ParseEnv(Archive<bits>& archive):
         archive(archive),
         vmaddr_resolver("ParseEnv::vmaddr_resolver"), offset_resolver("ParseEnv::offset_resolver"),
         lazy_bind_node_resolver("ParseEnv::lazy_bind_node_resolver"),
         dylib_resolver("ParseEnv::dylib_resolver"), segment_resolver("ParseEnv::segment_resolver"),
         section_resolver("ParseEnv::section_resolver"),
         current_segment(nullptr), current_section(nullptr) {}
      
   private:
      
   };
   
}
