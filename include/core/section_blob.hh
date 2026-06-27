#pragma once
#include <cassert>

#include <vector>

#include "types.hh"
#include "loc.hh"
#include "section.hh"

namespace MachO {

   template <Bits bits>
   class SectionBlob: public Node {
   public:
      using Iterator = typename Section<bits>::Content::iterator;
      using SectionBlobs = std::list<SectionBlob<opposite<bits>> *>;
      
      bool active = true;
      /*!< This blob is a function entry point (its parse-time vmaddr is in
       * env.func_syms and it lives in a text section).  At Build time such a
       * blob is aligned to an even address so the Itanium C++ ABI's
       * pointer-to-member-function low-bit tag survives translation: a pmf's
       * first word is the function address when bit0==0 (non-virtual) or a
       * vtable byte-offset+1 when bit0==1 (virtual).  If translation packs a
       * function at an ODD address, &Class::nonvirtual_method has bit0 set and
       * the dispatch code misreads it as a virtual thunk, dereferencing a bogus
       * vtable slot (f09_member_func_ptr).  Set in Section::Parse1; propagated
       * across the i386->x86_64 transform by the SectionBlob copy ctor. */
      bool func_entry = false;
      /*!< Original i386 vmaddr this blob was translated from (0 if synthetic /
       * unknown).  loc.vmaddr is reused for parse-time AND post-build addresses,
       * so it cannot retain the original once Build assigns the final x86_64
       * vmaddr; this field preserves the i386 source address across the
       * transform (set in the M32->M64 copy ctor) so Archive::inject_pcmap_section
       * can emit the original<->translated PC map the libabiconv C++ exception
       * unwinder needs (eh_shim.c). */
      std::size_t orig_vmaddr = 0;
      const Segment<bits> *segment = nullptr; /*!< containing segment */
      const Section<bits> *section = nullptr; /*!< containing section */
      Location loc; /*!< Post-build location, also used during parsing */
      Iterator iter;
      
      virtual std::size_t size() const = 0;
      virtual ~SectionBlob() {}
      virtual void Build(BuildEnv<bits>& env);
      virtual void Emit(Image& img, std::size_t offset) const = 0;

      virtual SectionBlobs Transform(TransformEnv<bits>& env) const { return {Transform_one(env)}; }
      virtual SectionBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const = 0;
      
   protected:
      SectionBlob(const Location& loc, ParseEnv<bits>& env, bool add_to_map = true);
      SectionBlob() {}
      SectionBlob(const SectionBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
   };

   template <Bits bits>
   class DataBlob: public SectionBlob<bits> {
   public:
      uint8_t data;
      virtual std::size_t size() const override { return 1; }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env) {
         return new DataBlob(img, loc, env);
      }

      /* Synthetic single-byte padding blob (value 0), not registered in any
       * parse-time resolver map.  Used by Section::Build to even-align function
       * entries (see SectionBlob::func_entry).  The byte sits between functions
       * and is never executed. */
      static DataBlob<bits> *Padding() { return new DataBlob<bits>((uint8_t)0); }

      virtual DataBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new DataBlob<opposite<bits>>(*this, env);
      }

   private:
      DataBlob(const Image& img, const Location& loc, ParseEnv<bits>& env);
      DataBlob(const DataBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      explicit DataBlob(uint8_t value): SectionBlob<bits>() { data = value; }

      template <Bits b> friend class DataBlob;
   };

   template <Bits bits>
   class ZeroBlob: public SectionBlob<bits> {
   public:
      virtual std::size_t size() const override { return 1; }
      virtual void Emit(Image& img, std::size_t offset) const override {}

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
      { return new ZeroBlob(img, loc, env); }
      virtual ZeroBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override
      { return new ZeroBlob<opposite<bits>>(*this, env); }
      
   private:
      ZeroBlob(const Image& img, const Location& loc, ParseEnv<bits>& env):
         SectionBlob<bits>(loc, env) {}
      ZeroBlob(const ZeroBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         SectionBlob<bits>(other, env) {}
      
      template <Bits b> friend class ZeroBlob;
   };

   template <Bits bits>
   class SymbolPointer: public SectionBlob<bits> {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      static std::size_t Size() { return sizeof(ptr_t); }
      virtual std::size_t size() const override { return sizeof(ptr_t); }
      virtual void Emit(Image& img, std::size_t offset) const override;
      
   protected:
      template <typename... Args>
      SymbolPointer(Args&&... args): SectionBlob<bits>(args...) {}
      virtual ptr_t raw_data() const = 0;
   };

   template <Bits bits>
   class LazySymbolPointer: public SymbolPointer<bits> {
   public:
      const SectionBlob<bits> *pointee; /*!< initial pointee */

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc,
                                            ParseEnv<bits>& env) {
         return new LazySymbolPointer(img, loc, env);
      }

      virtual LazySymbolPointer<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const
         override { return new LazySymbolPointer<opposite<bits>>(*this, env);      }
     
   private:
      LazySymbolPointer(const Image& img, const Location& loc, ParseEnv<bits>& env);
      LazySymbolPointer(const LazySymbolPointer<opposite<bits>>& other,
                        TransformEnv<opposite<bits>>& env);
      virtual typename SymbolPointer<bits>::ptr_t raw_data() const override {
         /* Match NonLazySymbolPointer's null guard: a lazy slot whose pointee
          * doesn't resolve to a known blob means dyld will bind it at first
          * call via the lazy_bind opcodes — emit 0 and let dyld fill it in.
          * Was unconditional pointee->loc.vmaddr; crashed in QuickTime's
          * __la_symbol_ptr where the dylib has lazy pointers to undefined
          * external symbols (no internal blob to point at). */
         return pointee ? pointee->loc.vmaddr : 0x0;
      }
      template <Bits> friend class LazySymbolPointer;
   };

   template <Bits bits>
   class NonLazySymbolPointer: public SymbolPointer<bits> {
   public:
      /*
       * `pointee` is non-null when the parsed value is a compile-time
       * pointer the linker baked into the slot (typical for LOCAL
       * indirect-symbol entries that don't get a dyld bind). null when
       * the slot is normally bound by dyld at load time (e.g.
       * `___stderrp`) — in that case raw_data() returns 0 and dyld
       * overwrites the slot. */
      const SectionBlob<bits> *pointee = nullptr;
      std::size_t pointee_offset = 0;  /*!< intra-blob byte offset when the slot
                                            value is an interior pointer resolved
                                            via the containing-blob fallback */

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc,
                                               ParseEnv<bits>& env) {
         return new NonLazySymbolPointer(img, loc, env);
      }

      /* Synthetic dyld-bound non-lazy pointer slot (pointee stays null, so
       * raw_data() emits 0 and the slot is bound at load: route-C
       * synthesize_dyld_info auto-emits a BIND for every slot of an
       * S_NON_LAZY_SYMBOL_POINTERS section via reserved1 + slot_idx ->
       * indirect symbol table). Not registered in any parse resolver; the
       * caller places it in a synthesized section's content. Used to build the
       * __DATA,__jt_ptrs slots that back the classic __IMPORT,__jump_table
       * UNDEFINED-stub trampolines (see JumpStubBlob +
       * Dysymtab::lift_jump_table_targets). */
      static NonLazySymbolPointer<bits> *CreateBound() {
         return new NonLazySymbolPointer();
      }

      virtual NonLazySymbolPointer<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const
         override { return new NonLazySymbolPointer<opposite<bits>>(*this, env); }

   private:
      NonLazySymbolPointer(): SymbolPointer<bits>() {}
      NonLazySymbolPointer(const Image& img, const Location& loc, ParseEnv<bits>& env);
      NonLazySymbolPointer(const NonLazySymbolPointer<opposite<bits>>& other,
                           TransformEnv<opposite<bits>>& env);
      virtual typename SymbolPointer<bits>::ptr_t raw_data() const override {
         return pointee ? pointee->loc.vmaddr + pointee_offset : 0x0;
      }
      template <Bits> friend class NonLazySymbolPointer;
   };
   
   template <Bits bits>
   class Immediate: public SectionBlob<bits> {
   public:
      uint32_t value;
      const SectionBlob<bits> *pointee = nullptr; /*!< optional -- only if deemed to be pointer */
      std::size_t pointee_offset = 0;             /*!< intra-blob byte offset when
                                                      pointee came from a
                                                      containing-blob fallback */
      virtual std::size_t size() const override { return sizeof(uint32_t); }
      
      static Immediate<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env,
                                    bool is_pointer)
      { return new Immediate(img, loc, env, is_pointer); }
      static Immediate<bits> *Create(uint32_t value) { return new Immediate(value); }
      
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual Immediate<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new Immediate<opposite<bits>>(*this, env);
      }
      
   private:
      Immediate(const Image& img, const Location& loc, ParseEnv<bits>& env, bool is_pointer);
      Immediate(const Immediate<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      Immediate(uint32_t value): value(value) {}
      template <Bits> friend class Immediate;
   };

   /*
    * One entry of an i386 PIC relative-offset switch jump table. GCC/Clang
    * i386 -fPIC switch dispatch is `mov reg,[table + idx*4]; add reg, anchor;
    * jmp reg`, where `anchor` is the PIC base (`call $+0; pop reg`) and each
    * 4-byte table entry holds `(case_target_vmaddr - anchor_vmaddr)`. Linear
    * sweep would disassemble the table bytes AS CODE, and the raw i386 offsets
    * are meaningless in the translated (non-linear) layout — so the first
    * indirect jmp lands in garbage. Section::DetectJumpTables recognises the
    * dispatch and the section's parse emits these blobs for the table range
    * instead. Each resolves its case-body target and the anchor blob; Emit
    * writes `(target_vmaddr - anchor_vmaddr)` in the NEW layout, so at runtime
    * `entry + reg(anchor)` again lands on the translated case body — the
    * load-time slide cancels because both terms carry it. Always 4 bytes (the
    * dispatch strides idx*4 and reads 4-byte entries in both M32 and M64).
    */
   template <Bits bits>
   class JumpTableEntry: public SectionBlob<bits> {
   public:
      const SectionBlob<bits> *target = nullptr; /*!< case-body blob (anchor+raw) */
      const SectionBlob<bits> *anchor = nullptr; /*!< PIC base blob (== runtime reg) */
      uint32_t raw = 0;                          /*!< original i386 offset (fallback) */

      virtual std::size_t size() const override { return sizeof(uint32_t); }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static JumpTableEntry<bits> *Parse(const Image& img, const Location& loc,
                                         ParseEnv<bits>& env, std::size_t anchor_vmaddr)
      { return new JumpTableEntry(img, loc, env, anchor_vmaddr); }

      virtual JumpTableEntry<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new JumpTableEntry<opposite<bits>>(*this, env);
      }

   private:
      JumpTableEntry(const Image& img, const Location& loc, ParseEnv<bits>& env,
                     std::size_t anchor_vmaddr);
      JumpTableEntry(const JumpTableEntry<opposite<bits>>& other,
                     TransformEnv<opposite<bits>>& env);
      template <Bits> friend class JumpTableEntry;
   };

   /*
    * One CFConstantString record from __DATA,__cfstring (the @"..." literals
    * the compiler bakes in). The i386 record is 16 bytes
    * {isa:4, flags:4, str:4, length:4}; the x86_64 record is 32 bytes
    * {isa:8, flags:8, str:8, length:8}. Parsing each record as a single blob
    * (instead of four 4-byte Immediates) lets us EXPAND it on transform so the
    * real x86_64 CoreFoundation can read it. `isa` is left 0 and bound at load
    * by the record's existing BIND (which targets this blob's start vmaddr);
    * `str` is an internal pointer we resolve to the __cstring blob and re-emit
    * pointer-width. Without this the i386 16-byte layout reaches real CF, which
    * reads str@16/length@24 and faults (e.g. in __CFStringHash).
    */
   template <Bits bits>
   class CFStringBlob: public SectionBlob<bits> {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      /* 16 bytes for the i386 source, 32 for the expanded x86_64 record. */
      static constexpr std::size_t record_size = 4 * sizeof(ptr_t);

      uint64_t flags = 0;
      const SectionBlob<bits> *str = nullptr; /*!< pointee in __cstring */
      uint64_t length = 0;

      virtual std::size_t size() const override { return 4 * sizeof(ptr_t); }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static SectionBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
      { return new CFStringBlob(img, loc, env); }

      virtual CFStringBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new CFStringBlob<opposite<bits>>(*this, env);
      }

   private:
      CFStringBlob(const Image& img, const Location& loc, ParseEnv<bits>& env);
      CFStringBlob(const CFStringBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class CFStringBlob;
   };

   /*
    * The whole `__DATA,__86x64_xrel` section payload as one synthesized blob.
    * Holds the classic external relocations macho-tool lifted (so dyld never
    * sees the unloadable 4-byte-slot extreloff table) for libabiconv to bind at
    * image load (objc_slide.c bind_external_relocs). Created post-transform by
    * Archive::inject_xrel_section; never parsed or transformed. Emit reads each
    * slot blob's resolved loc.vmaddr (set during Build).
    *
    * On-disk layout (little-endian, all 4-byte fields — translated images live
    * below 4GB):
    *   u32 magic = MAGIC ("xrel")
    *   u32 count
    *   count * { u32 slot_vmaddr; i32 addend; u32 name_off }   // 12 bytes each
    *   packed NUL-terminated symbol names
    * name_off is the byte offset of the name from the SECTION base (so the
    * runtime resolves it as section_base+slide + name_off).
    */
   template <Bits bits>
   class XrelBlob: public SectionBlob<bits> {
   public:
      static constexpr uint32_t MAGIC = 0x6c657278u; /* "xrel" */
      struct Ent {
         const SectionBlob<bits> *slot = nullptr; /*!< reloc'd slot (its loc.vmaddr) */
         int32_t addend = 0;
         uint32_t name_off = 0;                    /*!< from section base */
      };
      std::vector<Ent> ents;
      std::string strtab;                          /*!< packed NUL-terminated names */

      std::size_t header_size() const { return 8 + ents.size() * 12; }
      virtual std::size_t size() const override { return header_size() + strtab.size(); }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static XrelBlob<bits> *Create() { return new XrelBlob(); }
      virtual XrelBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         throw error("XrelBlob is synthesized post-transform and is never transformed");
      }

   private:
      XrelBlob() {}
      template <Bits> friend class XrelBlob;
   };

   /*
    * `__DATA,__86x64_pcmap`: the original-i386 <-> translated-x86_64 instruction
    * address map the libabiconv C++ exception unwinder (eh_shim.c) needs.  The
    * translated binary carries the ORIGINAL i386 __eh_frame/__gcc_except_tab
    * verbatim (every offset/range inside is in the i386 address space), but the
    * code now lives at different x86_64 addresses; only the blob graph knows the
    * correspondence, so it must be serialized here.
    *
    * Synthesized post-transform by Archive::inject_pcmap_section; never parsed or
    * transformed.  trans offsets are stored RELATIVE TO THE __text SECTION (not
    * the image base) because the later EXECUTE->DYLIB convert shifts segment
    * vmaddrs by the header-size delta while preserving each section's internal
    * byte layout — a section-relative offset is convert-invariant, an
    * image-relative one is not.  Emit resolves each instruction blob's final
    * loc.vmaddr (set during Build) and subtracts the anchor section's vmaddr.
    *
    * On-disk (little-endian, 4-byte fields; translated images live <4GB):
    *   u32 magic = MAGIC ("pcm6")
    *   u32 count
    *   count * { i32 trans_off; u32 orig_vmaddr }   // sorted ascending by trans_off
    * trans_off = blob.loc.vmaddr - anchor(__text).vmaddr; the runtime adds the
    * actual loaded __text base (getsectiondata) so the load slide cancels.
    */
   template <Bits bits>
   class PcmapBlob: public SectionBlob<bits> {
   public:
      static constexpr uint32_t MAGIC = 0x366d6370u; /* "pcm6" */
      struct Ent {
         const SectionBlob<bits> *trans = nullptr;   /*!< translated insn blob */
         uint32_t orig = 0;                          /*!< original i386 vmaddr */
      };
      std::vector<Ent> ents;
      const Section<bits> *anchor = nullptr;         /*!< __text (trans base) */

      virtual std::size_t size() const override { return 8 + ents.size() * 8; }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static PcmapBlob<bits> *Create() { return new PcmapBlob(); }
      virtual PcmapBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         throw error("PcmapBlob is synthesized post-transform and is never transformed");
      }
   private:
      PcmapBlob() {}
      template <Bits> friend class PcmapBlob;
   };

   /*
    * `__DATA,__86x64_ehlsda`: per-EH-function {translated start, original start,
    * LSDA} so the unwinder can, for a translated frame, find the function's
    * original i386 region start (the LSDA call-site/landing-pad offsets are
    * relative to it) and the translated address of its __gcc_except_tab LSDA.
    * The function<->LSDA association is read from the ORIGINAL i386 __eh_frame
    * FDEs during transform (Archive::collect_eh_lsda_pairs, where the addresses
    * are still correct) and resolved to translated blobs here.
    *
    * trans_func is __text-section-relative and lsda_off is
    * __gcc_except_tab-section-relative (each anchored on its own section, so the
    * convert segment-shift cancels regardless of inter-section repadding).
    *
    * On-disk (little-endian, 4-byte fields):
    *   u32 magic = MAGIC ("ehl6")
    *   u32 count
    *   count * { i32 trans_func_off; u32 orig_func; i32 lsda_off }  // sorted by trans_func_off
    */
   template <Bits bits>
   class EhlsdaBlob: public SectionBlob<bits> {
   public:
      static constexpr uint32_t MAGIC = 0x366c6865u; /* "ehl6" */
      struct Ent {
         const SectionBlob<bits> *func = nullptr;    /*!< translated func entry */
         uint32_t orig_func = 0;                     /*!< original i386 func start */
         int32_t lsda_off = 0;                       /*!< LSDA offset within
                                                          __gcc_except_tab (the
                                                          section is copied
                                                          verbatim, so its
                                                          intra-section offset is
                                                          transform/convert-
                                                          invariant) */
      };
      std::vector<Ent> ents;
      const Section<bits> *text_anchor = nullptr;    /*!< __text (func base) */

      virtual std::size_t size() const override { return 8 + ents.size() * 12; }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static EhlsdaBlob<bits> *Create() { return new EhlsdaBlob(); }
      virtual EhlsdaBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         throw error("EhlsdaBlob is synthesized post-transform and is never transformed");
      }
   private:
      EhlsdaBlob() {}
      template <Bits> friend class EhlsdaBlob;
   };

   /*
    * One x86_64 trampoline for a classic i386 `__IMPORT,__jump_table` UNDEFINED
    * external stub (the ~8% of self-modifying S_SYMBOL_STUBS that resolve to an
    * import, not an in-image function -- libstdc++ `__cxa_*`/`__ZSt*`, keymgr,
    * soft-float). The original 5-byte stub ships all-`0xf4` (hlt) and modern
    * dyld never fills it, and the __IMPORT segment is mapped non-exec on Apple
    * Silicon (its rwx initprot loses X), so the dead stub can't carry the jump.
    * Instead Dysymtab::lift_jump_table_targets synthesizes, per undefined stub,
    * a JumpStubBlob in a fresh executable `__TEXT,__jt_tramp` section plus a
    * paired `__DATA,__jt_ptrs` NonLazySymbolPointer slot; the blob is registered
    * at the ORIGINAL stub vmaddr so the `call <stub>` site resolves straight to
    * it (no call-site rewrite needed -- the dead stub is simply bypassed). The
    * slot is bound at load (route-C synthesize_dyld_info), so the trampoline is
    * `jmp *slot` -> the real import.
    *
    * Emit is `ff 25` + a 4-byte operand:
    *   M32 (noop/rebasify rebuild): absolute `jmp dword ptr [slot_vmaddr]`.
    *   M64 (transform output): rip-relative `jmp qword ptr [rip+disp32]`,
    *        disp = slot_vmaddr - (this_vmaddr + 6).
    * On any later reparse (modify/convert) the `__jt_tramp` bytes decode through
    * the TextParser whitelist as an ordinary `jmp [mem]` instruction and the
    * existing FF25/symbol-stub handling recomputes the displacement -- so this
    * blob is purely the one-time synthesis vehicle (idempotent: re-synthesis is
    * skipped once the section exists).
    */
   template <Bits bits>
   class JumpStubBlob: public SectionBlob<bits> {
   public:
      const SectionBlob<bits> *slot = nullptr; /*!< the __jt_ptrs slot to jump through */

      virtual std::size_t size() const override { return 6; } /* ff 25 + disp32 */
      virtual void Emit(Image& img, std::size_t offset) const override;

      static JumpStubBlob<bits> *Create(const Location& loc, ParseEnv<bits>& env,
                                        const SectionBlob<bits> *slot) {
         return new JumpStubBlob(loc, env, slot);
      }

      virtual JumpStubBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new JumpStubBlob<opposite<bits>>(*this, env);
      }

   private:
      /* add_to_map=false: the trampoline is reached via the branch handler's
       * direct brdisp assignment (ParseEnv::jump_table_undef_tramps), never via
       * a vmaddr resolve, so it must NOT register at the stub vmaddr (that slot
       * belongs to the dead __jump_table bytes). */
      JumpStubBlob(const Location& loc, ParseEnv<bits>& env, const SectionBlob<bits> *slot):
         SectionBlob<bits>(loc, env, /*add_to_map=*/false), slot(slot) {}
      JumpStubBlob(const JumpStubBlob<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      template <Bits> friend class JumpStubBlob;
   };

   template <Bits bits>
   class Placeholder: public SectionBlob<bits> {
   public:
      static Placeholder<bits> *Parse(const Location& loc, ParseEnv<bits>& env) {
         return new Placeholder(loc, env);
      }

      virtual Placeholder<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const override {
         return new Placeholder<opposite<bits>>(*this, env);
      }

      virtual std::size_t size() const override { return 0; }
      virtual void Emit(Image& img, std::size_t offset) const override {}

      static Placeholder<bits> *Create() { return new Placeholder(); }
      
   private:
      Placeholder(const Location& loc, ParseEnv<bits>& env): SectionBlob<bits>(loc, env, false) {}
      Placeholder(const Placeholder<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         SectionBlob<bits>(other, env) {}
      Placeholder() {}
      template <Bits> friend class Placeholder;
   };

   template <Bits bits>
   class RelocBlob: public SectionBlob<bits> {
   public:
      virtual RelocBlob<opposite<bits>> *Transform_one(TransformEnv<bits>& env) const {
         throw error("transforming relocation blobs unsupported");
      }
      
      static RelocBlob<bits> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env,
                                    uint8_t type);
      
   protected:
      RelocBlob(const Location& loc, ParseEnv<bits>& env): SectionBlob<bits>(loc, env) {}
   };

   template <Bits bits, typename T>
   class RelocBlobT: public RelocBlob<bits> {
   public:
      static_assert(std::is_integral<T>());

      T data;

      virtual std::size_t size() const override { return sizeof(T); }

      virtual void Emit(Image& img, std::size_t offset) const override;

      static RelocBlobT<bits, T> *Parse(const Image& img, const Location& loc, ParseEnv<bits>& env)
      { return new RelocBlobT(img, loc, env); }
      
   private:
      RelocBlobT(const Image& img, const Location& loc, ParseEnv<bits>& env);
   };

   template <Bits bits>
   using RelocUnsignedBlob = RelocBlobT<bits, macho_addr_t<bits>>;

   template <Bits bits>
   using RelocBranchBlob = RelocBlobT<bits, int32_t>;

}
