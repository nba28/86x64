#include <mach-o/x86_64/reloc.h>
#include <typeinfo>
#include <algorithm>

#include "section_blob.hh"
#include "segment.hh"
#include "image.hh"
#include "build.hh"
#include "parse.hh"
#include "transform.hh"

namespace MachO {

   template <Bits bits>
   Immediate<bits>::Immediate(const Image& img, const Location& loc, ParseEnv<bits>& env,
                              bool is_pointer):
      SectionBlob<bits>(loc, env), value(img.at<uint32_t>(loc.offset)), pointee(nullptr)
   {
      if (is_pointer) {
         /*
          * `value` is the absolute 32-bit address stored in the immediate
          * (e.g. the target of an i386 `jmp [abs32]` stub or a `mov r32, abs32`).
          * We need the blob residing at THAT vmaddr — not the blob at
          * loc.vmaddr (which would be the slot the immediate itself sits in).
          * The previous code resolved loc.vmaddr, leaving `pointee` pointing
          * at the immediate's own location and producing zero-displacement
          * RIP-relative jumps after the i386→x86_64 transform — which made
          * __symbol_stub entries jump to whatever followed them.
          */
         env.vmaddr_resolver.resolve(value, &pointee);
         /* mid-blob fallback: an absolute pointer (e.g. `mov [abs],reg` data
          * load) whose target sits inside a multi-byte blob misses exact-key
          * resolve; attach to the containing blob + offset so the load/pointer
          * relocates to the exact byte instead of being left unrelocated.
          * Gated to writable __DATA: the nearest-containing-blob guess is only
          * valid for opaque program data. In __OBJC (also writable) the
          * fragile-ABI metadata is parsed structurally, so the guess corrupts
          * category/method lists (regressed +[NSObject isLogEnabled]). */
         if (env.vmaddr_in_writable_data(value)) {
            env.vmaddr_resolver.resolve_containing(value, &pointee, &pointee_offset);
         }
      }
   }

   template <Bits bits>
   SectionBlob<bits>::SectionBlob(const Location& loc, ParseEnv<bits>& env, bool add_to_map):
      segment(env.current_segment), section(env.current_section), loc(loc)
   {
      if (add_to_map) {
         env.vmaddr_resolver.add(loc.vmaddr, this);
         env.offset_resolver.add(loc.offset, this);
      }
   }

   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const Image& img, const Location& loc,
                                              ParseEnv<bits>& env):
      SymbolPointer<bits>(loc, env)
   {
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      std::size_t targetaddr = img.at<ptr_t>(loc.offset);
      env.vmaddr_resolver.resolve(targetaddr, (const SectionBlob<bits> **) &pointee);
   }

   template <Bits bits>
   void SectionBlob<bits>::Build(BuildEnv<bits>& env) {
      env.allocate(active ? size() : 0, loc);
   }

   template <Bits bits>
   void DataBlob<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uint8_t>(offset) = data;
   }

   template <Bits bits>
   void SymbolPointer<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<ptr_t>(offset) = raw_data();
   }

   template <Bits bits>
   SectionBlob<bits>::SectionBlob(const SectionBlob<opposite<bits>>& other,
                                  TransformEnv<opposite<bits>>& env): segment(nullptr) {
      env.add(&other, this);
      env.resolve(other.segment, &segment);
      func_entry = other.func_entry; /* preserve even-alignment intent i386->x86_64 */
      /* Preserve the i386 source address: loc.vmaddr will be overwritten with the
       * final x86_64 address at Build, so capture the original now for the
       * __86x64_pcmap / __86x64_ehlsda emission (C++ exception unwinding). */
      orig_vmaddr = other.loc.vmaddr;
   }


   template <Bits bits>
   LazySymbolPointer<bits>::LazySymbolPointer(const LazySymbolPointer<opposite<bits>>& other,
                                              TransformEnv<opposite<bits>>& env):
      SymbolPointer<bits>(other, env), pointee(nullptr)
   {
      env.resolve(other.pointee, &pointee);
   }

   template <Bits bits>
   NonLazySymbolPointer<bits>::NonLazySymbolPointer(const Image& img, const Location& loc,
                                                    ParseEnv<bits>& env):
      SymbolPointer<bits>(loc, env)
   {
      /*
       * Pick up the compile-time value the linker put in the slot. If
       * dyld is going to bind this symbol at load time it will overwrite
       * what we emit anyway; for LOCAL indirect-symbol slots (no bind)
       * the value is what callers actually use. Treat any non-zero
       * value that lands inside one of the binary's segments as a
       * pointer so transform/emit rewrites it to the new vmaddr after
       * we shift segments.
       */
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      const std::size_t value = img.template at<ptr_t>(loc.offset);
      if (value != 0) {
         env.vmaddr_resolver.resolve(value,
                                     (const SectionBlob<bits> **) &pointee);
         /* mid-blob fallback (mirrors Immediate's): a LOCAL indirect-symbol
          * slot can hold an INTERIOR pointer into a multi-byte blob — e.g.
          * the address of a single bool/char field inside __DATA,__data (read
          * back via `movl slot,%reg; movzbl (%reg)`). Exact-key resolve misses
          * (the blob is registered at the 4-byte-aligned start, not the odd
          * interior address), leaving pointee null → the slot emits 0 → the
          * translated byte-load dereferences NULL. Attach to the containing
          * blob + offset so the slot relocates to the exact byte. Gated to
          * writable __DATA like Immediate (the nearest-blob guess is only valid
          * for opaque program data, not structurally-parsed __OBJC metadata). */
         if (env.vmaddr_in_writable_data(value)) {
            env.vmaddr_resolver.resolve_containing(
               value, (const SectionBlob<bits> **) &pointee, &pointee_offset);
         }
      }
   }

   template <Bits bits>
   NonLazySymbolPointer<bits>::NonLazySymbolPointer(
      const NonLazySymbolPointer<opposite<bits>>& other,
      TransformEnv<opposite<bits>>& env):
      SymbolPointer<bits>(other, env), pointee(nullptr),
      pointee_offset(other.pointee_offset)
   {
      if (other.pointee) {
         env.resolve(other.pointee, &pointee);
      }
   }

   template <Bits bits>
   Immediate<bits>::Immediate(const Immediate<opposite<bits>>& other,
                              TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), value(other.value), pointee(nullptr),
      pointee_offset(other.pointee_offset)
   {
      env.resolve(other.pointee, &pointee);
   }

   template <Bits bits>
   JumpTableEntry<bits>::JumpTableEntry(const Image& img, const Location& loc,
                                        ParseEnv<bits>& env, std::size_t anchor_vmaddr):
      SectionBlob<bits>(loc, env), raw(img.template at<uint32_t>(loc.offset))
   {
      /* Entry value is `case_target - anchor`; recover the case target and
       * resolve both it and the anchor to their blobs (deferred — the target
       * case body is parsed later in the linear sweep; the anchor `pop` earlier).
       * Emit re-derives the offset from the rebuilt vmaddrs. */
      env.vmaddr_resolver.resolve(anchor_vmaddr + (int32_t)raw, &target);
      env.vmaddr_resolver.resolve(anchor_vmaddr, &anchor);
   }

   template <Bits bits>
   JumpTableEntry<bits>::JumpTableEntry(const JumpTableEntry<opposite<bits>>& other,
                                        TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), raw(other.raw)
   {
      env.resolve(other.target, &target);
      env.resolve(other.anchor, &anchor);
   }

   template <Bits bits>
   void JumpTableEntry<bits>::Emit(Image& img, std::size_t offset) const {
      uint32_t value;
      if (target && anchor) {
         value = (uint32_t)((int64_t)target->loc.vmaddr - (int64_t)anchor->loc.vmaddr);
      } else {
         /* unresolved (target outside any parsed blob) — keep the original
          * offset so behaviour is no worse than before the relocation. */
         value = raw;
      }
      img.at<uint32_t>(offset) = value;
   }

   template <Bits bits>
   void Immediate<bits>::Emit(Image& img, std::size_t offset) const {
      uint32_t value;
      if (pointee) {
         value = pointee->loc.vmaddr + pointee_offset;
      } else {
         value = this->value;
      }
      img.at<uint32_t>(offset) = value;
   }

   template <Bits bits>
   CFStringBlob<bits>::CFStringBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env)
   {
      /* Record layout (pointer-width fields): {isa, flags, str, length}.
       * isa is bound at load (a BIND already targets loc.vmaddr), so we don't
       * read it. Read at ptr_t stride so this works whether we're parsing the
       * 16-byte i386 source OR re-parsing an already-expanded 32-byte M64
       * record (later pipeline stages re-parse the M64 binary). Reading at
       * fixed i386 offsets would misread the wider M64 fields and zero them. */
      flags = img.at<ptr_t>(loc.offset + 1 * sizeof(ptr_t));
      const ptr_t str_vmaddr = img.at<ptr_t>(loc.offset + 2 * sizeof(ptr_t));
      env.vmaddr_resolver.resolve(str_vmaddr, &str);
      length = img.at<ptr_t>(loc.offset + 3 * sizeof(ptr_t));
   }

   template <Bits bits>
   CFStringBlob<bits>::CFStringBlob(const CFStringBlob<opposite<bits>>& other,
                                    TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), flags(other.flags), str(nullptr), length(other.length)
   {
      env.resolve(other.str, &str);
   }

   template <Bits bits>
   void CFStringBlob<bits>::Emit(Image& img, std::size_t offset) const {
      /* Emit a pointer-width record: isa=0 (bound at load), flags, str, length.
       * In M64 this widens the i386 16-byte record to the 32-byte layout real
       * CoreFoundation expects. */
      const ptr_t isa = 0;
      const ptr_t str_addr = str ? (ptr_t)str->loc.vmaddr : 0;
      img.at<ptr_t>(offset + 0 * sizeof(ptr_t)) = isa;
      img.at<ptr_t>(offset + 1 * sizeof(ptr_t)) = (ptr_t)flags;
      img.at<ptr_t>(offset + 2 * sizeof(ptr_t)) = str_addr;
      img.at<ptr_t>(offset + 3 * sizeof(ptr_t)) = (ptr_t)length;
   }

   template <Bits bits>
   DataBlob<bits>::DataBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env), data(img.at<uint8_t>(loc.offset)) {}

   template <Bits bits>
   DataBlob<bits>::DataBlob(const DataBlob<opposite<bits>>& other,
                            TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), data(other.data) {}

   template <Bits bits>
   RelocBlob<bits> *RelocBlob<bits>::Parse(const Image& img, const Location& loc,
                                           ParseEnv<bits>& env, uint8_t type) {
      switch (type) {
      case X86_64_RELOC_UNSIGNED: return RelocUnsignedBlob<bits>::Parse(img, loc, env);
      case X86_64_RELOC_BRANCH:	 return RelocBranchBlob<bits>::Parse(img, loc, env);

      case X86_64_RELOC_SIGNED:		
      case X86_64_RELOC_GOT_LOAD:		
      case X86_64_RELOC_GOT:			
      case X86_64_RELOC_SUBTRACTOR:	
      case X86_64_RELOC_SIGNED_1:		
      case X86_64_RELOC_SIGNED_2:		
      case X86_64_RELOC_SIGNED_4:		
      case X86_64_RELOC_TLV:
         throw error("unsupported relocation type");

      default:
         throw error("bad relocation type");
      }
   }

   template <Bits bits, typename T>
   RelocBlobT<bits, T>::RelocBlobT(const Image& img, const Location& loc, ParseEnv<bits>& env):
      RelocBlob<bits>(loc, env), data(img.at<T>(loc.offset)) {}

   template <Bits bits, typename T>
   void RelocBlobT<bits, T>::Emit(Image& img, std::size_t offset) const {
      img.at<T>(offset) = data;
   }

   template <Bits bits>
   void XrelBlob<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uint32_t>(offset + 0) = MAGIC;
      img.at<uint32_t>(offset + 4) = static_cast<uint32_t>(ents.size());
      std::size_t p = offset + 8;
      for (const Ent& e : ents) {
         /* The slot's resolved x86_64 vmaddr (translated images live <4GB, so a
          * 32-bit field suffices; the runtime adds the load slide). */
         const uint32_t slot_vmaddr =
            e.slot ? static_cast<uint32_t>(e.slot->loc.vmaddr) : 0;
         img.at<uint32_t>(p + 0) = slot_vmaddr;
         img.at<int32_t>(p + 4) = e.addend;
         img.at<uint32_t>(p + 8) = e.name_off;
         p += 12;
      }
      if (!strtab.empty()) {
         img.copy(p, strtab.data(), strtab.size());
      }
   }

   template <Bits bits>
   void PcmapBlob<bits>::Emit(Image& img, std::size_t offset) const {
      const uint32_t anchor_vmaddr =
         anchor ? static_cast<uint32_t>(anchor->loc().vmaddr) : 0;
      /* Resolve each instruction's final translated vmaddr (set at Build) and
       * sort by the section-relative offset so the runtime can binary-search. */
      std::vector<std::pair<int32_t, uint32_t>> rows;
      rows.reserve(ents.size());
      for (const Ent& e : ents) {
         if (e.trans == nullptr) { continue; }
         const int32_t trans_off =
            static_cast<int32_t>(static_cast<uint32_t>(e.trans->loc.vmaddr) - anchor_vmaddr);
         rows.emplace_back(trans_off, e.orig);
      }
      std::sort(rows.begin(), rows.end());
      img.at<uint32_t>(offset + 0) = MAGIC;
      img.at<uint32_t>(offset + 4) = static_cast<uint32_t>(rows.size());
      std::size_t p = offset + 8;
      for (const auto& r : rows) {
         img.at<int32_t>(p + 0) = r.first;
         img.at<uint32_t>(p + 4) = r.second;
         p += 8;
      }
   }

   template <Bits bits>
   void EhlsdaBlob<bits>::Emit(Image& img, std::size_t offset) const {
      const uint32_t text_vmaddr =
         text_anchor ? static_cast<uint32_t>(text_anchor->loc().vmaddr) : 0;
      struct Row { int32_t func_off; uint32_t orig_func; int32_t lsda_off; };
      std::vector<Row> rows;
      rows.reserve(ents.size());
      for (const Ent& e : ents) {
         if (e.func == nullptr) { continue; }
         Row r;
         r.func_off = static_cast<int32_t>(static_cast<uint32_t>(e.func->loc.vmaddr) - text_vmaddr);
         r.orig_func = e.orig_func;
         r.lsda_off = e.lsda_off;
         rows.push_back(r);
      }
      std::sort(rows.begin(), rows.end(),
                [] (const Row& a, const Row& b) { return a.func_off < b.func_off; });
      img.at<uint32_t>(offset + 0) = MAGIC;
      img.at<uint32_t>(offset + 4) = static_cast<uint32_t>(rows.size());
      std::size_t p = offset + 8;
      for (const auto& r : rows) {
         img.at<int32_t>(p + 0) = r.func_off;
         img.at<uint32_t>(p + 4) = r.orig_func;
         img.at<int32_t>(p + 8) = r.lsda_off;
         p += 12;
      }
   }

   template <Bits bits>
   JumpStubBlob<bits>::JumpStubBlob(const JumpStubBlob<opposite<bits>>& other,
                                    TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), slot(nullptr)
   {
      env.resolve(other.slot, &slot);
   }

   template <Bits bits>
   void JumpStubBlob<bits>::Emit(Image& img, std::size_t offset) const {
      /* `ff 25` = JMP r/m (ModRM /4): in 64-bit mode the operand is
       * rip-relative `[rip+disp32]`; in 32-bit mode it is absolute `[disp32]`. */
      img.at<uint8_t>(offset + 0) = 0xff;
      img.at<uint8_t>(offset + 1) = 0x25;
      const uint32_t slot_vmaddr =
         slot ? static_cast<uint32_t>(slot->loc.vmaddr) : 0;
      if constexpr (bits == Bits::M32) {
         img.at<uint32_t>(offset + 2) = slot_vmaddr; /* absolute pointer */
      } else {
         /* rip-relative displacement from the end of this 6-byte instruction */
         const int32_t disp = static_cast<int32_t>(
            static_cast<int64_t>(slot_vmaddr) -
            static_cast<int64_t>(this->loc.vmaddr + 6));
         img.at<int32_t>(offset + 2) = disp;
      }
   }

   template class SectionBlob<Bits::M32>;
   template class SectionBlob<Bits::M64>;

   template class LazySymbolPointer<Bits::M32>;
   template class LazySymbolPointer<Bits::M64>;

   template class NonLazySymbolPointer<Bits::M32>;
   template class NonLazySymbolPointer<Bits::M64>;

   template class DataBlob<Bits::M32>;
   template class DataBlob<Bits::M64>;

   template class CFStringBlob<Bits::M32>;
   template class CFStringBlob<Bits::M64>;

   template class Immediate<Bits::M32>;
   template class Immediate<Bits::M64>;

   template class JumpTableEntry<Bits::M32>;
   template class JumpTableEntry<Bits::M64>;

   template class XrelBlob<Bits::M32>;
   template class XrelBlob<Bits::M64>;

   template class PcmapBlob<Bits::M32>;
   template class PcmapBlob<Bits::M64>;

   template class EhlsdaBlob<Bits::M32>;
   template class EhlsdaBlob<Bits::M64>;

   template class JumpStubBlob<Bits::M32>;
   template class JumpStubBlob<Bits::M64>;

   template class RelocBlob<Bits::M32>;
   template class RelocBlob<Bits::M64>;
   
   template class RelocBlobT<Bits::M32, uint32_t>;
   template class RelocBlobT<Bits::M64, uint64_t>;
   template class RelocBlobT<Bits::M32, int32_t>;
   template class RelocBlobT<Bits::M64, int32_t>;

}
