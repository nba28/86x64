#include <mach-o/x86_64/reloc.h>
#include <typeinfo>
#include <algorithm>

#include "section_blob.hh"
#include "segment.hh"
#include "image.hh"
#include "build.hh"
#include "parse.hh"
#include "transform.hh"
#include "archive.hh"   /* PcmapBlob/EhlsdaBlob::Parse walk env.archive.segments() */
#include "section.hh"

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
          * Gated to OPAQUE data: writable __DATA plus read-only non-code
          * sections (__TEXT,__const / __cstring / literals — Quinn Preferences:
          * `movl $_pieceMatrix2,(%esp)` with the table interior at 2 mod 4 in
          * __TEXT,__const stayed a stale raw 0xb3a36 -> EXC_BAD_ACCESS in the
          * native callee, while its 4-aligned siblings relocated). NOT __OBJC
          * (writable, but the fragile-ABI metadata is parsed structurally, so
          * the containing-blob guess corrupts category/method lists — regressed
          * +[NSObject isLogEnabled]) and NOT instruction sections (mid-
          * instruction offsets are meaningless after the M32->M64 rewrite). */
         if (env.vmaddr_in_writable_data(value) ||
             env.vmaddr_in_readonly_opaque_data(value)) {
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
   ZeroBlob<bits>::ZeroBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
      SectionBlob<bits>(loc, env)
   {
      /* ONE extent spanning from loc.vmaddr to the section end. Never read
       * file bytes: a zerofill section has none (input header offset=0), so
       * the historical per-byte sweep fabricated blobs from other sections'
       * bytes / past EOF. Section::Parse1's linear sweep then steps by
       * size() and terminates after this single blob. */
      size_ = 1;
      if (env.current_section != nullptr) {
         const auto& cs = env.current_section->sect;
         const std::size_t sect_end = (std::size_t)cs.addr + (std::size_t)cs.size;
         if (loc.vmaddr < sect_end) {
            size_ = sect_end - loc.vmaddr;
         }
      }
   }

   template <Bits bits>
   void ZeroBlob<bits>::Build(BuildEnv<bits>& env) {
      /* Reserve VMADDR space only. Zerofill occupies no file bytes, so the
       * file-offset cursor must not advance (a zerofill section contributes
       * to segment vmsize, not filesize — advancing the offset bloated every
       * translated image by the zerofill span and skewed the offset-delta
       * section size at Section::Build). */
      this->loc = env.loc;
      env.loc.vmaddr += this->active ? size_ : 0;
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
      /* Carry forward the source instruction's DISK-FAITHFUL i386 address that
       * Section::Parse1 recorded during the initial sweep (section base + sum of
       * raw decoded lengths).  We must NOT use other.loc.vmaddr here: by the time
       * Transform runs the M32 archive has already been Built (transform.cc), so
       * loc.vmaddr reflects the re-laid-out layout (even-alignment/padding) and
       * drifts from the i386 layout the LSDA is keyed in.  orig_vmaddr is the
       * pre-Build sweep value, untouched by Build/DetectPicAnchoredDisps. */
      orig_vmaddr = other.orig_vmaddr;
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
          * OPAQUE data like Immediate: writable __DATA plus read-only non-code
          * sections (a slot holding an interior __TEXT,__const pointer has the
          * same stale/NULL failure) — never structurally-parsed __OBJC
          * metadata, never instruction sections. */
         if (env.vmaddr_in_writable_data(value) ||
             env.vmaddr_in_readonly_opaque_data(value)) {
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
      heuristic = other.heuristic;
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
          * 32-bit field suffices; the runtime adds the load slide). slot_offset
          * is non-zero only for a reparse slot that fell inside a multi-byte blob
          * (resolve_containing); for a freshly-lifted exact slot it is 0. */
         const uint32_t slot_vmaddr =
            e.slot ? static_cast<uint32_t>(e.slot->loc.vmaddr + e.slot_offset) : 0;
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
   SectionBlob<bits> *XrelBlob<bits>::Parse(const Image& img, const Location& loc,
                                            ParseEnv<bits>& env) {
      /* One blob for the WHOLE section (Section::Parse1 calls us once: size()
       * spans the entire payload). Reconstruct ents/strtab from the on-disk
       * table and RE-RESOLVE each slot by its baked vmaddr, so this build's
       * Emit writes the slot's post-re-layout address. */
      auto *blob = new XrelBlob<bits>(loc, env);
      const uint32_t magic = img.at<uint32_t>(loc.offset + 0);
      if (magic != MAGIC) {
         throw error("__86x64_xrel: bad magic 0x%08x on reparse", magic);
      }
      const uint32_t count = img.at<uint32_t>(loc.offset + 4);
      blob->ents.reserve(count); /* keep &ents.back() stable for deferred resolve */
      for (uint32_t i = 0; i < count; ++i) {
         const std::size_t eo = loc.offset + 8 + static_cast<std::size_t>(i) * 12;
         const uint32_t slot_vmaddr = img.at<uint32_t>(eo + 0);
         const int32_t  addend      = img.at<int32_t>(eo + 4);
         const uint32_t name_off    = img.at<uint32_t>(eo + 8); /* from section base */
         Ent ent;
         ent.addend = addend;
         ent.name_off = static_cast<uint32_t>(blob->strtab.size()); /* intra; rebased below */
         const char *nm = &img.at<char>(loc.offset + name_off);
         blob->strtab.append(nm);
         blob->strtab.push_back('\0');
         blob->ents.push_back(ent);
         /* resolve_containing handles a slot that lands at a blob start (offset 0)
          * AND one inside a multi-byte blob; fires in do_resolve_containing once
          * every section's blobs are registered. */
         env.vmaddr_resolver.resolve_containing(
            slot_vmaddr, &blob->ents.back().slot, &blob->ents.back().slot_offset);
      }
      /* Rebase name_off to the section base (header precedes the packed names),
       * now that the entry count is final — matches inject_xrel_section. */
      const uint32_t hdr = static_cast<uint32_t>(blob->header_size());
      for (auto& e : blob->ents) { e.name_off += hdr; }
      return blob;
   }

   template <Bits bits>
   void Abs32Blob<bits>::Emit(Image& img, std::size_t offset) const {
      /* Resolve each field's final vmaddr (blob locs are set during Build) and
       * sort so the runtime walks/patches in ascending address order. */
      std::vector<uint32_t> rows;
      rows.reserve(ents.size());
      for (const Ent& e : ents) {
         if (e.blob == nullptr) { continue; }
         rows.push_back(static_cast<uint32_t>(e.blob->loc.vmaddr + e.off));
      }
      std::sort(rows.begin(), rows.end());
      img.at<uint32_t>(offset + 0) = MAGIC;
      img.at<uint32_t>(offset + 4) = static_cast<uint32_t>(rows.size());
      std::size_t p = offset + 8;
      for (const uint32_t r : rows) {
         img.at<uint32_t>(p) = r;
         p += 4;
      }
      /* dropped (unresolved) entries leave a short tail: zero-fill it so the
       * section stays well-formed (count above is the emitted row count). */
      const std::size_t end = offset + size();
      for (; p < end; p += 4) { img.at<uint32_t>(p) = 0; }
   }

   template <Bits bits>
   SectionBlob<bits> *Abs32Blob<bits>::Parse(const Image& img, const Location& loc,
                                             ParseEnv<bits>& env) {
      /* One blob for the WHOLE section. Reconstruct the field list and
       * RE-RESOLVE each field vmaddr to the blob currently containing it, so
       * this build's Emit writes the field's post-re-layout address (the
       * __86x64_xrel pattern — a DataBlob round-trip would freeze the baked
       * vmaddrs and go stale when a later pass shifts the layout). */
      auto *blob = new Abs32Blob<bits>(loc, env);
      const uint32_t magic = img.at<uint32_t>(loc.offset + 0);
      if (magic != MAGIC) {
         throw error("__86x64_abs32: bad magic 0x%08x on reparse", magic);
      }
      const uint32_t count = img.at<uint32_t>(loc.offset + 4);
      blob->ents.reserve(count); /* keep &ents.back() stable for deferred resolve */
      for (uint32_t i = 0; i < count; ++i) {
         const uint32_t field_vmaddr =
            img.at<uint32_t>(loc.offset + 8 + static_cast<std::size_t>(i) * 4);
         if (field_vmaddr == 0) { continue; } /* zero-filled tail */
         Ent ent;
         blob->ents.push_back(ent);
         /* resolve_containing handles a field at a blob start (offset 0) AND
          * one inside a multi-byte blob (a disp32 mid-instruction); fires in
          * do_resolve_containing once every section's blobs are registered. */
         env.vmaddr_resolver.resolve_containing(
            field_vmaddr, &blob->ents.back().blob, &blob->ents.back().off);
      }
      return blob;
   }

   template <Bits bits>
   void PcmapBlob<bits>::Emit(Image& img, std::size_t offset) const {
      const uint32_t anchor_vmaddr =
         anchor ? static_cast<uint32_t>(anchor->loc().vmaddr) : 0;
      /* Resolve each instruction's final translated vmaddr (set at Build) and
       * sort by the section-relative offset so the runtime can binary-search.
       * A parse-lifted row whose re-resolution missed keeps its on-disk
       * trans_off verbatim (raw_trans_off fallback) — better a frozen row than
       * a dropped one, since the runtime interpolates between rows. */
      std::vector<std::pair<int32_t, uint32_t>> rows;
      rows.reserve(ents.size());
      for (const Ent& e : ents) {
         int32_t trans_off;
         if (e.trans != nullptr) {
            trans_off = static_cast<int32_t>(
               static_cast<uint32_t>(e.trans->loc.vmaddr + e.off) - anchor_vmaddr);
         } else if (e.raw_trans_off != INT32_MIN) {
            trans_off = e.raw_trans_off;
         } else {
            continue;
         }
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
      /* dropped (unresolvable inject-time) entries leave a short tail: zero it
       * so the section stays well-formed (count above is the emitted count). */
      const std::size_t end = offset + size();
      for (; p < end; p += 4) { img.at<uint32_t>(p) = 0; }
   }

   template <Bits bits>
   SectionBlob<bits> *PcmapBlob<bits>::Parse(const Image& img, const Location& loc,
                                             ParseEnv<bits>& env) {
      /* One blob for the WHOLE section (the __86x64_xrel/__86x64_abs32
       * pattern). Reconstruct the rows and RE-RESOLVE each translated
       * instruction (old __text vmaddr + trans_off) to the blob currently
       * containing it, so this build's Emit re-emits the row against the
       * post-re-layout addresses. `orig` stays verbatim: it lives in the
       * frozen ORIGINAL-i386 address space, which the generic DataParser
       * round-trip this replaces used to pointer-detect and corrupt whenever
       * the input image's base aliased the M64 layout window (guard
       * pcmap_stage_shift_test.sh). */
      auto *blob = new PcmapBlob<bits>(loc, env);
      const uint32_t magic = img.at<uint32_t>(loc.offset + 0);
      if (magic != MAGIC) {
         throw error("__86x64_pcmap: bad magic 0x%08x on reparse", magic);
      }
      /* Anchor on __text, exactly like inject_pcmap_section: the stored
       * trans_offs are relative to its CURRENT (on-disk) vmaddr; Emit
       * re-derives them against its post-re-layout vmaddr. __TEXT precedes
       * __DATA in every image we emit, so the section object already exists. */
      const Section<bits> *text = nullptr;
      for (Segment<bits> *seg : env.archive.segments()) {
         for (Section<bits> *s : seg->sections) {
            if (s->name() == "__text") { text = s; break; }
         }
         if (text != nullptr) { break; }
      }
      if (text == nullptr) {
         throw error("__86x64_pcmap reparse: no __text section to anchor on");
      }
      blob->anchor = text;
      const std::size_t text_addr = text->sect.addr;
      const uint32_t count = img.at<uint32_t>(loc.offset + 4);
      blob->ents.reserve(count); /* keep &ents.back() stable for deferred resolve */
      for (uint32_t i = 0; i < count; ++i) {
         const std::size_t eo = loc.offset + 8 + static_cast<std::size_t>(i) * 8;
         Ent ent;
         ent.raw_trans_off = img.at<int32_t>(eo + 0);
         ent.orig          = img.at<uint32_t>(eo + 4);
         blob->ents.push_back(ent);
         env.vmaddr_resolver.resolve_containing(
            text_addr + static_cast<std::size_t>(
               static_cast<int64_t>(blob->ents.back().raw_trans_off)),
            &blob->ents.back().trans, &blob->ents.back().off);
      }
      return blob;
   }

   template <Bits bits>
   void EhlsdaBlob<bits>::Emit(Image& img, std::size_t offset) const {
      const uint32_t text_vmaddr =
         text_anchor ? static_cast<uint32_t>(text_anchor->loc().vmaddr) : 0;
      struct Row { int32_t func_off; uint32_t orig_func; int32_t lsda_off; };
      std::vector<Row> rows;
      rows.reserve(ents.size());
      for (const Ent& e : ents) {
         Row r;
         if (e.func != nullptr) {
            r.func_off = static_cast<int32_t>(
               static_cast<uint32_t>(e.func->loc.vmaddr + e.off) - text_vmaddr);
         } else if (e.raw_func_off != INT32_MIN) {
            r.func_off = e.raw_func_off;   /* parse-lifted frozen fallback */
         } else {
            continue;
         }
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
      const std::size_t end = offset + size();
      for (; p < end; p += 4) { img.at<uint32_t>(p) = 0; }
   }

   template <Bits bits>
   SectionBlob<bits> *EhlsdaBlob<bits>::Parse(const Image& img, const Location& loc,
                                              ParseEnv<bits>& env) {
      /* Same re-parse contract as PcmapBlob::Parse: re-resolve the translated
       * function entry (old __text vmaddr + trans_func_off); orig_func (frozen
       * i386 space) and lsda_off (__gcc_except_tab-relative, copied verbatim)
       * stay untouched. */
      auto *blob = new EhlsdaBlob<bits>(loc, env);
      const uint32_t magic = img.at<uint32_t>(loc.offset + 0);
      if (magic != MAGIC) {
         throw error("__86x64_ehlsda: bad magic 0x%08x on reparse", magic);
      }
      const Section<bits> *text = nullptr;
      for (Segment<bits> *seg : env.archive.segments()) {
         for (Section<bits> *s : seg->sections) {
            if (s->name() == "__text") { text = s; break; }
         }
         if (text != nullptr) { break; }
      }
      if (text == nullptr) {
         throw error("__86x64_ehlsda reparse: no __text section to anchor on");
      }
      blob->text_anchor = text;
      const std::size_t text_addr = text->sect.addr;
      const uint32_t count = img.at<uint32_t>(loc.offset + 4);
      blob->ents.reserve(count); /* keep &ents.back() stable for deferred resolve */
      for (uint32_t i = 0; i < count; ++i) {
         const std::size_t eo = loc.offset + 8 + static_cast<std::size_t>(i) * 12;
         Ent ent;
         ent.raw_func_off = img.at<int32_t>(eo + 0);
         ent.orig_func    = img.at<uint32_t>(eo + 4);
         ent.lsda_off     = img.at<int32_t>(eo + 8);
         blob->ents.push_back(ent);
         env.vmaddr_resolver.resolve_containing(
            text_addr + static_cast<std::size_t>(
               static_cast<int64_t>(blob->ents.back().raw_func_off)),
            &blob->ents.back().func, &blob->ents.back().off);
      }
      return blob;
   }

   template <Bits bits>
   JumpStubBlob<bits>::JumpStubBlob(const JumpStubBlob<opposite<bits>>& other,
                                    TransformEnv<opposite<bits>>& env):
      SectionBlob<bits>(other, env), slot(nullptr), name(other.name)
   {
      env.resolve(other.slot, &slot);
   }

   template <Bits bits>
   void JumpStubBlob<bits>::Emit(Image& img, std::size_t offset) const {
      if constexpr (bits == Bits::M32) {
         /* Intermediate image (never executed): classic 6-byte absolute
          * `jmp dword ptr [slot_vmaddr]` (`ff 25` + absolute disp32). */
         img.at<uint8_t>(offset + 0) = 0xff;
         img.at<uint8_t>(offset + 1) = 0x25;
         img.at<uint32_t>(offset + 2) =
            slot ? static_cast<uint32_t>(slot->loc.vmaddr) : 0;
         return;
      } else {
         /* Fixed 17-byte NULL-checking diagnostic trampoline (PURE CODE -- no
          * inline data, so it survives the convert/modify reparse that decodes
          * __jt_tramp as instructions and SHIFTS its layout). The slot may be
          * bound to NULL (weak import of a removed symbol), in which case the
          * classic `jmp *slot` would fault at 0x0 with no attribution. Instead
          * load the slot into r11 (PLT scratch -- not an argument register, not
          * the SysV varargs AL count, so the resolved fast path is register-
          * faithful to the old jmp) and trap with ud2 if NULL.
          *
          *   +0  4c 8b 1d <disp32>   mov  r11, [rip+slot]   (rip = self+7)
          *   +7  4d 85 db            test r11, r11
          *  +10  74 03               jz   .trap (+15)
          *  +12  41 ff e3            jmp  r11               ; resolved import
          *  +15  0f 0b               ud2                    ; .trap (NULL slot)
          *
          * Both references re-resolve on reparse: the rip-relative mov to its
          * __jt_ptrs slot (NonLazySymbolPointer), and the jz to the ud2. */
         const int64_t slot_va = slot ? static_cast<int64_t>(slot->loc.vmaddr) : 0;
         const int64_t self_va = static_cast<int64_t>(this->loc.vmaddr);
         /* mov r11, [rip+disp32] */
         img.at<uint8_t>(offset + 0) = 0x4c;
         img.at<uint8_t>(offset + 1) = 0x8b;
         img.at<uint8_t>(offset + 2) = 0x1d;
         img.at<int32_t>(offset + 3) = static_cast<int32_t>(slot_va - (self_va + 7));
         /* test r11, r11 */
         img.at<uint8_t>(offset + 7) = 0x4d;
         img.at<uint8_t>(offset + 8) = 0x85;
         img.at<uint8_t>(offset + 9) = 0xdb;
         /* jz +3 -> .trap */
         img.at<uint8_t>(offset + 10) = 0x74;
         img.at<uint8_t>(offset + 11) = 0x03;
         /* jmp r11 (resolved tail-call) */
         img.at<uint8_t>(offset + 12) = 0x41;
         img.at<uint8_t>(offset + 13) = 0xff;
         img.at<uint8_t>(offset + 14) = 0xe3;
         /* .trap: ud2 */
         img.at<uint8_t>(offset + 15) = 0x0f;
         img.at<uint8_t>(offset + 16) = 0x0b;
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

   template class ZeroBlob<Bits::M32>;
   template class ZeroBlob<Bits::M64>;

   template class CFStringBlob<Bits::M32>;
   template class CFStringBlob<Bits::M64>;

   template class Immediate<Bits::M32>;
   template class Immediate<Bits::M64>;

   template class JumpTableEntry<Bits::M32>;
   template class JumpTableEntry<Bits::M64>;

   template class XrelBlob<Bits::M32>;
   template class XrelBlob<Bits::M64>;

   template class Abs32Blob<Bits::M32>;
   template class Abs32Blob<Bits::M64>;

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
