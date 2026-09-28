#pragma once
#include <cassert>

#include <vector>
#include <sstream>
#include <mach-o/loader.h>

#include "macho.hh"
#include "transform.hh"
#include "types.hh"
#include "segment.hh"

namespace MachO {

   class AbstractArchive: public MachO {
   public:
      
      
      static AbstractArchive *Parse(const Image& img, std::size_t offset);
      virtual void Build() override { Build(0); }
      virtual std::size_t Build(std::size_t offset) = 0;
      // virtual std::size_t Build(std::size_t offset, std::size_t vmaddr) = 0;
   };
   
   template <Bits b>
   class Archive: public AbstractArchive {
   public:
      using LoadCommands = std::vector<LoadCommand<b> *>;

      mach_header_t<b> header;
      LoadCommands load_commands;
      std::size_t vmaddr = vmaddr_start<b>; /*!< start vmaddr */

      /*!< If non-zero, Build reserves header padding so the first section keeps
       * exactly this file offset (= the original header-region size). A
       * post-translation pass that only edits load-command strings (e.g. the
       * `change-deps` dependency-path rewriter) sets this to the input's
       * original first-section offset so NO code/data vmaddr moves: re-parsing
       * a translated binary leaves some absolute __data pointers as verbatim
       * values that would be stale if the layout shifted, so byte-stable layout
       * is required. 0 = legacy behaviour (MACHO_HEADERPAD env / no pad). */
      std::size_t header_size_target = 0;

      /*!< When set (by `macho-tool convert --synthesize-dyld-info`), Build
       * manufactures a modern LC_DYLD_INFO_ONLY (rebase/bind/lazy/weak/export
       * opcode streams) for a CLASSIC (LC_DYSYMTAB-only) M64 image, so the
       * output is a canonical modern dylib: install_name_tool (cctools + llvm)
       * accept it, and its binds flow through the same opcode-rewrite path as
       * natively-modern images. No-op for images that already carry a
       * DyldInfo, for M32 builds, and for non-classic outputs. See
       * Archive::synthesize_dyld_info. */
      bool synthesize_dyld_info_enabled = false;

      /* Built by Transform from the opposite-width archive (the M32 pass), so
       * it still holds that pass's constant/pointer verdicts. See
       * inject_cpin_section. */
      bool from_transform = false;

      virtual uint32_t magic() const override { return header.magic; }
      virtual uint32_t& magic() override { return header.magic; }
      virtual Bits bits() const override { return b; }

      template <template <Bits> class Subclass, int64_t cmdval = -1>
      Subclass<b> *subcommand() const {
         static_assert(std::is_base_of<LoadCommand<b>, Subclass<b>>());
         for (LoadCommand<b> *lc : load_commands) {
            Subclass<b> *subcommand = dynamic_cast<Subclass<b> *>(lc);
            if (subcommand) { return subcommand; }
         }
         return nullptr;
      }
      
      template <template <Bits> class Subclass, int64_t cmdval = -1>
      std::vector<Subclass<b> *> subcommands() const {
         static_assert(std::is_base_of<LoadCommand<b>, Subclass<b>>());
         std::vector<Subclass<b> *> cmds;
         for (LoadCommand<b> *lc : load_commands) {
            Subclass<b> *cmd = dynamic_cast<Subclass<b> *>(lc);
            if (cmd && (cmdval == -1 || cmdval == cmd->cmd())) {
               cmds.push_back(cmd);
            }
         }
         return cmds;
      }

      /*
       * Cached segments view. The previous implementation rebuilt a
       * vector with dynamic_casts over every load command on each call;
       * the hot per-instruction "is this vmaddr in any segment?" loops
       * in instruction parsing and DataParser hit this millions of times
       * on a 17 MB binary like iPhoto. Now lazy-built once and reused.
       * Callers that mutate load_commands (modify --insert load-dylib,
       * remove_commands) must call invalidate_segments_cache().
       */
   private:
      mutable std::vector<Segment<b> *> segments_cache_;
      mutable bool segments_cache_valid_ = false;
   public:
      const std::vector<Segment<b> *>& segments() const {
         if (!segments_cache_valid_) {
            segments_cache_ = subcommands<Segment>();
            segments_cache_valid_ = true;
         }
         return segments_cache_;
      }
      void invalidate_segments_cache() const {
         segments_cache_valid_ = false;
         segments_cache_.clear();
      }
      Segment<b> *segment(std::size_t index) { return segments().at(index); }
      Segment<b> *segment(const std::string& name);

      std::vector<Section<b> *> sections() const;
      Section<b> *section(uint8_t index) const;
      Section<b> *section(const std::string& name) const;

      static Archive<b> *Parse(const Image& img, std::size_t offset = 0) {
         return new Archive(img, offset);
      }

      virtual std::size_t Build(std::size_t offset) override;

      virtual void Emit(Image& img) const override;
      virtual ~Archive() override;

      template <typename... Args>
      void Insert(const SectionLocation<b>& loc, Args&&... args) {
         loc.segment->Insert(loc, args...);
      }

      std::size_t offset_to_vmaddr(std::size_t offset) const;
      std::optional<size_t> try_offset_to_vmaddr(std::size_t offset) const;

      Archive<opposite<b>> *Transform(TransformEnv<b>& env) const {
         return new Archive<opposite<b>>(*this, env);
      }
      Archive<opposite<b>> *Transform() const {
         TransformEnv<b> env;
         return Transform(env);
      }

      void insert(SectionBlob<b> *blob, const Location& loc, Relation rel);
      void remove_commands(uint32_t cmd);

      /* Move every LC_DYLD_INFO bind (regular/weak/lazy) whose target slot is a
       * 4-byte Immediate (an i386 __DATA,__const RTTI/vtable pointer that stays
       * 4 bytes wide in the M64 output) OUT of the dyld bind stream and INTO the
       * Dysymtab's xrel_entries, so inject_xrel_section binds them 4-byte-wide at
       * load via libabiconv. dyld's 8-byte pointer write into such a 4-byte slot
       * spills its zero high-32 into the adjacent 4-byte field (the typeinfo
       * __name), zeroing it -> typeid().name()==NULL -> boost::python strcmp
       * SIGSEGV (Civ IV). Binds into real 8-byte SymbolPointer slots
       * (__nl_symbol_ptr/__la_symbol_ptr) are left in the dyld stream. M64 only;
       * runs BEFORE inject_xrel_section. Structural (triggers on slot width, not
       * an RTTI-name match) + universal for any C++ target with __const
       * external-vtable/RTTI binds. */
      void divert_narrow_const_binds_to_xrel();

      /* Replace every WEAK bind whose target slot is a SymbolPointer already
       * resolved to an in-image blob with a plain local REBASE (M64 only).
       * transform.cc clears MH_WEAK_DEFINES so a translated image cannot satisfy
       * a NATIVE image's coalesced bind (the cross-ABI weak-def coalesce bug) —
       * which also stops it satisfying its OWN, leaving C++ linkonce_odr slots
       * holding unslid preferred-base addresses. Runs after the narrow divert
       * (an 8-byte rebase would clobber a 4-byte typeinfo field's neighbour).
       * Kill switch M64_NO_SELF_WEAK_REBASE=1. */
      void resolve_self_weak_binds();

      /* Synthesize the __DATA,__86x64_xrel section from the Dysymtab's lifted
       * classic external relocations (M64 only; no-op otherwise / if empty / if
       * already present). Called at the top of Build so the section is laid out
       * with the rest of __DATA and its XrelBlob's Emit can read every slot
       * blob's resolved vmaddr. See the known-gaps list / objc_slide.c. */
      void inject_xrel_section();

      /* Manufacture an LC_DYLD_INFO_ONLY (rebase/bind/lazy_bind/weak_bind/
       * export opcode streams) for a classic (no-LC_DYLD_INFO) M64 image from
       * its LC_DYSYMTAB indirect symbol table + relocations + nlist symtab, so
       * the output is a canonical modern dylib. Gated on
       * synthesize_dyld_info_enabled; no-op if a DyldInfo already exists, on
       * M32, or with no Dysymtab/Symtab. Called from Build after
       * inject_xrel_section. See the known-gaps list route C. */
      void synthesize_dyld_info();

      /* {original i386 function start, original i386 LSDA vmaddr} read from the
       * source image's __eh_frame FDEs during transform (collect_eh_lsda_pairs),
       * where the addresses are still correct; consumed by inject_ehlsda_section
       * at Build to emit __DATA,__86x64_ehlsda. Empty for non-EH binaries. */
      std::vector<std::pair<std::size_t, std::size_t>> eh_lsda_pairs;

      /* Parse the SOURCE (M32) __eh_frame to populate eh_lsda_pairs. Called from
       * the M32->M64 transform ctor (this == M64) with the original archive. */
      void collect_eh_lsda_pairs(const Archive<opposite<b>>& other);

      /* Synthesize __DATA,__86x64_pcmap (original<->translated instruction PC
       * map) and __DATA,__86x64_ehlsda (per-EH-function start + LSDA) for the
       * libabiconv C++ exception unwinder (eh_shim.c). M64 only; INERT (emits
       * nothing) when the image has no __eh_frame / __gcc_except_tab so non-C++
       * binaries are unperturbed; idempotent (skips if the section exists, e.g. a
       * convert reparse). Called from Build alongside inject_xrel_section. */
      void inject_pcmap_section();
      void inject_ehlsda_section();

      /* Synthesize __DATA,__86x64_abs32: the exact table of 4-byte __TEXT
       * fields holding pre-slide absolute intra-image addresses (abs
       * [disp32+idx*scale] operand disps + __TEXT data-section pointer slots)
       * for the runtime slide patchers (objc_slide.c patch_text_abs32 /
       * wrapper_setup.c), replacing their byte-pattern scan heuristics. M64
       * only; idempotent (a reparse lifts the section into a re-resolving
       * Abs32Blob, see section.cc). Called from Build alongside
       * inject_pcmap_section. */
      void inject_abs32_section();

      /* Synthesize __DATA,__86x64_cpin: the M32 pass's CONSTANT verdicts for
       * every 4-byte data slot whose value could alias the translated image, so
       * the later M64 re-parses (modify / strip-bind / static-interpose /
       * convert) — where every false-positive discriminator in
       * Section::DataParser is disarmed — cannot reclassify them as pointers and
       * "rebase" them on the next layout shift. M64 only; idempotent (a reparse
       * lifts the section into a re-resolving ConstPinBlob, see section.cc) —
       * and idempotency is REQUIRED for correctness here, not just efficiency:
       * only the first M64 Build still holds the M32 verdicts. Called from Build
       * alongside inject_abs32_section. See ConstPinBlob for the measurement
       * that motivated it. */
      void inject_cpin_section();

      template <template <Bits> class Blob>
      Blob<b> *find_blob(std::size_t vmaddr) const {
         for (Segment<b> *segment : segments()) {
            if (segment->contains_vmaddr(vmaddr)) {
               return segment->template find_blob<Blob>(vmaddr);
            }
         }
         std::stringstream ss;
         ss << "vmaddr " << std::hex << vmaddr << " not in any segment";
         throw std::invalid_argument(ss.str());
      }

   private:
      std::size_t total_size;

      Archive(const Image& img, std::size_t offset);
      Archive(const Archive<opposite<b>>& other, TransformEnv<opposite<b>>& env);
      
      template <Bits> friend class Archive;
   };   

}

