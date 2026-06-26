#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include "archive.hh"
#include "parse.hh"
#include "build.hh"
#include "segment.hh"
#include "types.hh"
#include "section_blob.hh"
#include "symtab.hh"   // Dysymtab (inject_xrel_section)
#include "dyldinfo.hh" // DyldInfo (classic-image gate)

namespace MachO {

   template <Bits b>
   Archive<b>::Archive(const Image& img, std::size_t offset):
      header(img.at<mach_header_t<b>>(offset))
   {
      ParseEnv<b> env(*this);
      offset += sizeof(header);
      for (int i = 0; i < header.ncmds; ++i) {
         /* Advance by the LC header's stored cmdsize, not by cmd->size().
          * size() recomputes from the parsed string length and may disagree
          * with the file's cmdsize if a tool (e.g. an in-place install_name
          * patch) shortened a path but kept the same cmdsize via null
          * padding. Trusting the file's cmdsize keeps subsequent LCs at
          * their actual on-disk offsets. */
         const std::size_t lc_start = offset;
         const auto& lc_hdr = img.at<load_command>(lc_start);
         const std::size_t lc_cmdsize = lc_hdr.cmdsize;
         LoadCommand<b> *cmd = LoadCommand<b>::Parse(img, offset, env);
         if (cmd) { load_commands.push_back(cmd); }   /* nullptr = intentionally dropped (e.g. LC_TWOLEVEL_HINTS) */
         offset = lc_start + lc_cmdsize;
      }

      /*
       * Adopt the input binary's existing vmaddr base instead of forcing
       * the default `vmaddr_start<b>`. Picking it from the first non-
       * __PAGEZERO segment preserves the base across multi-step toolchain
       * passes (transform → modify → convert) so internal pointer values
       * baked into __data by the transform step don't get truncated when a
       * later step rebuilds at a different base.
       */
      for (LoadCommand<b> *cmd : load_commands) {
         auto seg = dynamic_cast<Segment<b> *>(cmd);
         if (seg == nullptr) continue;
         if (strcmp(seg->segment_command.segname, SEG_PAGEZERO) == 0) continue;
         vmaddr = seg->segment_command.vmaddr;
         break;
      }

      for (LoadCommand<b> *cmd : load_commands) {
         cmd->Parse1(img, env);
      }

      env.do_resolve();

      for (LoadCommand<b> *cmd : load_commands) {
         cmd->Parse2(env);
      }

      /* remaining placeholder should go at end of sections */
      for (Section<b> *section : sections()) {
         auto placeholder_it = env.placeholders.begin();
         while (placeholder_it != env.placeholders.end()) {
            if (section->sect.addr + section->sect.size == placeholder_it->first) {
               section->content.push_back(placeholder_it->second);
               placeholder_it = env.placeholders.erase(placeholder_it);
            } else {
               ++placeholder_it;
            }
         }
      }

      if (!env.placeholders.empty()) {
         /* Stranded placeholders represent addresses the parser thought might
          * be pointers (instruction memdisp targets, nlist values, etc.) but
          * for which no parsed blob exists at that vmaddr. Common origins:
          *   - XED-decoded instruction bytes that are actually data
          *     (jump tables, constants between functions) → garbage memdisp
          *   - Values that land in __LINKEDIT (opaque metadata, no section
          *     blobs)
          *   - Cross-segment addresses computed from translator-emitted code
          *     that no longer correspond to a real blob after layout shift
          * The originating Immediate keeps its raw value (pointee=nullptr)
          * and emits the bytes verbatim. dyld doesn't resolve via parsed
          * blobs; it uses LC_DYLD_INFO bind/rebase, which is unaffected.
          * So stranded placeholders are non-fatal — warn and continue. Was:
          * threw std::logic_error, which blocked modify on iPhoto run23. */
         static const bool verbose = std::getenv("MACHO_PARSE_VERBOSE") != nullptr;
         if (verbose) {
            for (const auto& p : env.placeholders) {
               std::size_t vmaddr = p.first;
               const char *where = "outside any segment";
               for (const Segment<b> *seg : segments()) {
                  if (vmaddr >= seg->segment_command.vmaddr &&
                      vmaddr <  seg->segment_command.vmaddr + seg->segment_command.vmsize) {
                     where = seg->segment_command.segname;
                     break;
                  }
               }
               fprintf(stderr, "stranded placeholder vmaddr=0x%zx (%s)\n", vmaddr, where);
            }
         }
         if (env.placeholders.size() > 0) {
            fprintf(stderr,
                    "warning: %zu stranded placeholder(s) at parse; "
                    "originating Immediate values preserved verbatim "
                    "(set MACHO_PARSE_VERBOSE=1 for per-placeholder addresses).\n",
                    env.placeholders.size());
         }
         env.placeholders.clear();
      }

      env.do_resolve();
      /* Containing-blob fallback for absolute data refs that land mid-blob
       * (exact-key resolve missed). Must run after do_resolve (so exact wins
       * where possible) and after all blobs are registered. */
      env.vmaddr_resolver.do_resolve_containing();
   }

   template <Bits b>
   Archive<b>::~Archive() {
      for (LoadCommand<b> *lc : load_commands) {
         delete lc;
      }
   }

   template <Bits b>
   Segment<b> *Archive<b>::segment(const std::string& name) {
      for (Segment<b> *seg : segments()) {
         if (name == seg->name()) {
            return seg;
         }
      }
      return nullptr;
   }

   template <Bits b>
   std::size_t Archive<b>::Build(std::size_t offset) {
      /* Inject our runtime-bind metadata section (classic external relocs that
       * dyld can't process) before laying out, so it shares __DATA's layout and
       * its XrelBlob::Emit can read each slot blob's resolved vmaddr. M64-only,
       * no-op when there are no lifted relocs. */
      inject_xrel_section();

      /* Enforce the "__LINKEDIT is the last segment" invariant before laying
       * out file offsets. codesign/dyld append the code-signature superblob at
       * the end of __LINKEDIT and require it to cover the whole image, so
       * __LINKEDIT must hold the highest file offset of any segment. dyld's
       * strict validation additionally rejects a binary whose segment
       * load-command order disagrees with its file/vmaddr order ("segment load
       * commands out of order with respect to layout", MachOFile::validSegments
       * / Malformed::segmentOrder). So we move the __LINKEDIT *load command*
       * itself to sit after every other segment -- not merely bump its file
       * offset -- so the Build cursor below and Emit both place it last in
       * load-command, file, and vmaddr order. Without this, a binary that
       * carries a custom segment trailing __LINKEDIT (e.g. Civ IV's Firaxis
       * __OINK, a 93740-byte opaque sectionless payload) is unsignable:
       * codesign reports "main executable failed strict validation".
       *
       * Universal: triggers on the structural property (a non-__LINKEDIT
       * segment laid out after __LINKEDIT), never on an app name. Idempotent --
       * a no-op once __LINKEDIT is already the last segment, so binaries with
       * the normal layout are unaffected (and byte-stable). Moving only the
       * sectionless __LINKEDIT past trailing segments leaves every section's
       * index (n_sect) unchanged, since section IDs are assigned in
       * load-command order and the section-bearing segments keep their relative
       * positions. */
      if (Segment<b> *linkedit = segment(SEG_LINKEDIT)) {
         Segment<b> *last_seg = nullptr;
         for (LoadCommand<b> *lc : load_commands) {
            if (auto *seg = dynamic_cast<Segment<b> *>(lc)) { last_seg = seg; }
         }
         if (last_seg != linkedit) {
            /* unlink __LINKEDIT from its current slot */
            for (auto it = load_commands.begin(); it != load_commands.end(); ++it) {
               if (*it == linkedit) { load_commands.erase(it); break; }
            }
            /* re-insert immediately after the (now) last segment command */
            std::size_t insert_idx = 0;
            for (std::size_t i = 0; i < load_commands.size(); ++i) {
               if (dynamic_cast<Segment<b> *>(load_commands[i])) { insert_idx = i + 1; }
            }
            load_commands.insert(load_commands.begin() + insert_idx, linkedit);
         }
      }

      BuildEnv<b> env(this, Location(offset, vmaddr));
      
      env.allocate(sizeof(header));
      
      /* count number of load commands */
      header.ncmds = load_commands.size();

      /* compute size of commands */
      header.sizeofcmds = 0;
      for (LoadCommand<b> *lc : load_commands) {
         header.sizeofcmds += lc->size();
      }

      env.allocate(header.sizeofcmds);

      /* Header padding between the end of the load commands and the first
       * section. Two sources, in priority order:
       *
       *  1. header_size_target (set by the `change-deps` dependency-path
       *     rewriter): reserve exactly enough padding that the first section
       *     keeps its ORIGINAL file offset, so no code/data vmaddr moves. This
       *     makes a load-command string edit byte-stable everywhere except the
       *     rewritten strings -- required because re-parsing a translated binary
       *     leaves some absolute __data pointers as verbatim values that would
       *     be stale if the layout shifted. Since such a rewrite only ever
       *     shrinks dependency paths (/System/... -> @rpath/...), the freed
       *     load-command bytes are absorbed back into this pad.
       *
       *  2. MACHO_HEADERPAD=N env var (legacy): reserve N bytes of slack so a
       *     later install_name_tool -add_rpath / -change can grow a path without
       *     "load commands do not fit". Apple's linker reserves 32 bytes by
       *     default (1024 with -headerpad_max_install_names). 0 by default to
       *     keep regressions byte-stable with prior outputs. */
      std::size_t headerpad = 0;
      const std::size_t header_used = sizeof(header) + header.sizeofcmds;
      if (header_size_target > 0) {
         if (header_size_target >= header_used) {
            headerpad = header_size_target - header_used;
         } else {
            fprintf(stderr,
                    "warning: rewritten load commands (%zu bytes) exceed the original "
                    "header region (%zu bytes); first section will shift and absolute "
                    "data pointers may go stale\n",
                    header_used, header_size_target);
         }
      } else if (const char *hp_env = std::getenv("MACHO_HEADERPAD")) {
         headerpad = std::strtoull(hp_env, nullptr, 0);
      }
      if (headerpad > 0) {
         env.allocate(headerpad);
      }

      /* assign IDs */
      for (LoadCommand<b> *lc : load_commands) {
         lc->AssignID(env);
      }
      
      /* build each command */
      static const bool build_debug = std::getenv("MACHO_BUILD_DEBUG") != nullptr;
      for (size_t i = 0; i < load_commands.size(); ++i) {
         LoadCommand<b> *lc = load_commands[i];
         if (build_debug) {
            fprintf(stderr,
                    "Build: lc[%zu] cmd=0x%x size=%u\n",
                    i, (unsigned)lc->cmd(), (unsigned)lc->size());
         }
         lc->Build(env);
      }

      total_size = env.loc.offset - offset;
      return total_size;
   }

   template <Bits b>
   void Archive<b>::inject_xrel_section() {
      if constexpr (b != Bits::M64) {
         return; /* M32 builds are intermediate; XrelBlob is M64-output only */
      } else {
         /* Classic images only (no LC_DYLD_INFO) — exactly the set whose
          * external relocs we drop (symtab.cc) and whose pointer slots are
          * 4-byte. A modern image's binds live in the dyld_info stream and its
          * relocs are 8-byte; we must never write our 4-byte entries there. */
         if (this->template subcommand<DyldInfo>() != nullptr) { return; }

         auto *dysymtab = this->template subcommand<Dysymtab>();
         if (dysymtab == nullptr || dysymtab->xrel_entries.empty()) { return; }

         Segment<b> *data_seg = segment(SEG_DATA);
         if (data_seg == nullptr) { return; }

         /* idempotent: a reparse (modify/convert) of an already-translated dylib
          * carries the section as data but has no lifted relocs — and we never
          * want two. */
         for (Section<b> *s : data_seg->sections) {
            if (s->name() == "__86x64_xrel") { return; }
         }

         auto *blob = XrelBlob<b>::Create();
         std::size_t skipped = 0;
         for (const auto& e : dysymtab->xrel_entries) {
            if (e.slot == nullptr) { ++skipped; continue; } /* slot didn't resolve */
            typename XrelBlob<b>::Ent ent;
            ent.slot = e.slot;
            ent.addend = e.addend;
            ent.name_off = static_cast<uint32_t>(blob->strtab.size()); /* intra-strtab */
            blob->strtab.append(e.name);
            blob->strtab.push_back('\0');
            blob->ents.push_back(ent);
         }
         if (blob->ents.empty()) { delete blob; return; }

         /* Rebase name_off to the section start (header + entry table precede the
          * packed names), now that the entry count (hence header size) is final. */
         const uint32_t hdr = static_cast<uint32_t>(blob->header_size());
         for (auto& ent : blob->ents) { ent.name_off += hdr; }

         auto *sect = Section<b>::Synthetic(SEG_DATA, "__86x64_xrel",
                                            S_REGULAR, /*align=*/2);
         sect->segment = data_seg;
         sect->content.push_back(blob);
         blob->section = sect;
         blob->segment = data_seg;
         data_seg->sections.push_back(sect);
         invalidate_segments_cache();

         if (std::getenv("MACHO_BUILD_DEBUG")) {
            fprintf(stderr, "inject_xrel_section: %zu entries (%zu unresolved "
                    "skipped) -> __DATA,__86x64_xrel\n", blob->ents.size(), skipped);
         }
      }
   }

   template <Bits b>
   void Archive<b>::Emit(Image& img) const {
      /* emit header */
      img.at<mach_header_t<b>>(0) = header;

      /* emit load commands */
      static const bool emit_debug = std::getenv("MACHO_EMIT_DEBUG") != nullptr;
      std::size_t offset = sizeof(header);
      std::size_t i = 0;
      for (LoadCommand<b> *lc : load_commands) {
         if (emit_debug) {
            fprintf(stderr,
                    "Archive::Emit lc[%zu] cmd=0x%x size=%u @ offset=0x%zx\n",
                    i, (unsigned)lc->cmd(), (unsigned)lc->size(), offset);
         }
         lc->Emit(img, offset);
         offset += lc->size();
         ++i;
      }
      if (emit_debug) {
         fprintf(stderr, "Archive::Emit completed all %zu LCs\n", load_commands.size());
      }
   }

   template <Bits b>
   Archive<b>::Archive(const Archive<opposite<b>>& other, TransformEnv<opposite<b>>& env)
   {
      env(other.header, header);
      for (const auto lc : other.load_commands) {
         /* Drop LC_SEGMENT_SPLIT_INFO, LC_DYLIB_CODE_SIGN_DRS, and
          * LC_LINKER_OPTIMIZATION_HINT from the translated output. These are
          * hints for Apple's static linker / dyld shared cache builder, not
          * required for runtime image loading. install_name_tool has strict
          * ordering checks for them that depend on absolute file offsets, and
          * our M32→M64 layout shifts (especially dyld_info data growth) push
          * them off the expected positions — install_name_tool then refuses
          * to mutate with "X data out of place". Dropping them silences the
          * error, lets later -change/-add_rpath calls succeed, and costs
          * nothing at runtime since dyld doesn't consult these blobs. */
         const uint32_t cmd = lc->cmd();
         if (cmd == LC_SEGMENT_SPLIT_INFO ||
             cmd == LC_DYLIB_CODE_SIGN_DRS ||
             cmd == LC_LINKER_OPTIMIZATION_HINT) {
            continue;
         }
         load_commands.push_back(lc->Transform(env));
      }
   }

   template <Bits b>
   void Archive<b>::insert(SectionBlob<b> *blob, const Location& loc, Relation rel) {
      macho_addr_t<b> segment_command_t<b>::*segloc;
      macho_addr_t<b> segment_command_t<b>::*segsize;
      std::size_t locval;
      if (loc.offset) {
         segloc = &segment_command_t<b>::fileoff;
         segsize = &segment_command_t<b>::filesize;
         locval = loc.offset;
      } else if (loc.vmaddr) {
         segloc = &segment_command_t<b>::vmaddr;
         segsize = &segment_command_t<b>::vmsize;
         locval = loc.vmaddr;
      } else {
         throw std::invalid_argument("location offset and vmaddr are both 0");
      }

      for (Segment<b> *segment : segments()) {
         std::size_t segloc_ = segment->segment_command.*segloc;
         if (locval >= segloc_ && locval < segloc_ + segment->segment_command.*segsize) {
            segment->insert(blob, loc, rel);
            return;
         }
      }

      throw std::invalid_argument("location not in any segment");
   }

   template <Bits b>
   std::size_t Archive<b>::offset_to_vmaddr(std::size_t offset) const {
      for (Segment<b> *segment : segments()) {
         if (offset >= segment->segment_command.fileoff &&
             offset < segment->segment_command.fileoff + segment->segment_command.filesize) {
            return segment->offset_to_vmaddr(offset);
         }
      }
      throw std::invalid_argument(std::string("offset" ) + std::to_string(offset) +
                                  " not in any segment");
   }

   template <Bits b>
   std::optional<std::size_t> Archive<b>::try_offset_to_vmaddr(std::size_t offset) const {
      for (Segment<b> *segment : segments()) {
         if (segment->contains_offset(offset)) {
            return segment->try_offset_to_vmaddr(offset);
         }
      }
      return std::nullopt;
   }

   AbstractArchive *AbstractArchive::Parse(const Image& img, std::size_t offset) {
      uint32_t magic = img.at<uint32_t>(offset);
      switch (magic) {
      case MH_CIGAM:
      case MH_CIGAM_64:
         throw std::invalid_argument("archive has opposite endianness");

      case MH_MAGIC:
         return Archive<Bits::M32>::Parse(img, offset);
         
      case MH_MAGIC_64:
         return Archive<Bits::M64>::Parse(img, offset);

      default:
         {
            std::stringstream ss;
            ss << "invalid magic number 0x" << std::hex << magic;
            throw std::invalid_argument(ss.str());
         }
      }
   }

   template <Bits b>
   void Archive<b>::remove_commands(uint32_t cmd) {
      /* erase-friendly loop: erase returns the iterator to the next element,
       * which we use directly instead of ++it'ing past it. The prior version
       * `it = erase(it); ++it;` skipped every second consecutive match. */
      for (auto it = load_commands.begin(); it != load_commands.end(); ) {
         if ((*it)->cmd() == cmd) {
            it = load_commands.erase(it);
         } else {
            ++it;
         }
      }
   }

   template <Bits b>
   std::vector<Section<b> *> Archive<b>::sections() const {
      std::vector<Section<b> *> acc;
      for (const auto segment : segments()) {
         acc.insert(acc.end(), segment->sections.begin(), segment->sections.end());
      }
      return acc;
   }

   template <Bits b>
   Section<b> *Archive<b>::section(uint8_t index) const {
      if (index == NO_SECT) {
         return nullptr;
      } else {
         return sections().at(index - 1);
      }
   }

   template <Bits b>
   Section<b> *Archive<b>::section(const std::string& name) const {
      for (auto section : sections()) {
         if (section->name() == name) {
            return section;
         }
      }
      return nullptr;
   }

   template class Archive<Bits::M32>;
   template class Archive<Bits::M64>;

}
