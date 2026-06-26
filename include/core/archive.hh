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

