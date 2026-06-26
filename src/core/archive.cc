#include <cstdlib>
#include <sstream>
#include <stdexcept>

#include <algorithm>
#include <vector>
#include <mach-o/nlist.h>

#include "archive.hh"
#include "parse.hh"
#include "build.hh"
#include "segment.hh"
#include "types.hh"
#include "section_blob.hh"
#include "symtab.hh"   // Dysymtab (inject_xrel_section)
#include "dyldinfo.hh" // DyldInfo (classic-image gate)
#include "rebase_info.hh"  // RebaseInfo (synthesize_dyld_info)
#include "export_info.hh"  // ExportInfo / RegularExportNode (synthesize_dyld_info)
#include "lc.hh"           // DylibCommand (synthesize_dyld_info)

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

      /* Manufacture a modern LC_DYLD_INFO_ONLY for a classic image (opt-in via
       * convert --synthesize-dyld-info). Runs AFTER inject_xrel_section so the
       * 4-byte external-reloc RTTI slots still get their runtime __86x64_xrel
       * binding (that section is bound by libabiconv, not dyld, and covers a
       * disjoint slot set from the 8-byte symbol-pointer binds synthesized
       * here). Adds the DyldInfo load command, so it must precede the ncmds /
       * sizeofcmds accounting below. */
      synthesize_dyld_info();

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
   void Archive<b>::synthesize_dyld_info() {
      if constexpr (b != Bits::M64) {
         return; /* M32 builds are intermediate; we only emit modern output */
      } else {
         if (!synthesize_dyld_info_enabled) { return; }

         /* Already modern (binds live in a real LC_DYLD_INFO stream): nothing to
          * do. This is exactly the inverse of the classic-image set. */
         if (this->template subcommand<DyldInfo>() != nullptr) { return; }

         auto *symtab   = this->template subcommand<Symtab>();
         auto *dysymtab = this->template subcommand<Dysymtab>();
         if (symtab == nullptr || dysymtab == nullptr) { return; }

         const bool dbg = std::getenv("MACHO_BUILD_DEBUG") != nullptr;

         /* (1) ordinal -> DylibCommand. dyld's two-level library ordinal counts
          * every dylib-loading command in load-command order, 1-based (matching
          * BuildEnv::dylib_counter and classic_symbind). */
         std::vector<const DylibCommand<b> *> ord_dylibs;
         for (LoadCommand<b> *lc : load_commands) {
            auto *dc = dynamic_cast<DylibCommand<b> *>(lc);
            if (dc == nullptr) { continue; }
            switch (dc->cmd()) {
            case LC_LOAD_DYLIB: case LC_LOAD_WEAK_DYLIB: case LC_REEXPORT_DYLIB:
            case LC_LOAD_UPWARD_DYLIB: case LC_LAZY_LOAD_DYLIB:
               ord_dylibs.push_back(dc); break;
            default: break; /* LC_ID_DYLIB et al. don't get an ordinal */
            }
         }

         /* (2) symbol-table index -> Nlist*. The multiset iteration order is the
          * Build/Emit order and is exactly the index space the indirect symbol
          * table entries reference (convert.cc re-indexes the indirect table
          * into this same order after its symtab edits). */
         std::vector<const Nlist<b> *> syms_by_index(symtab->syms.begin(),
                                                     symtab->syms.end());

         auto *rebase    = RebaseInfo<b>::Create();
         auto *bind      = BindInfo<b, false>::Create();
         auto *weak_bind = BindInfo<b, false>::Create(/*weak=*/true);
         auto *lazy_bind = BindInfo<b, true>::Create();
         auto *export_info = ExportInfo<b>::Create();

         auto add_rebase = [&] (const SectionBlob<b> *blob) {
            auto *rn = RebaseNode<b>::Create(REBASE_TYPE_POINTER);
            rn->blob = blob;
            rebase->rebasees.push_back(rn);
         };

         /* A symbol-pointer slot holds a sliding internal pointer only when its
          * blob resolved to an internal pointee; a null-valued LOCAL slot must
          * NOT be rebased (dyld would add the slide to 0). */
         auto sp_has_pointee = [] (const SectionBlob<b> *blob) -> bool {
            if (auto *n = dynamic_cast<const NonLazySymbolPointer<b> *>(blob)) {
               return n->pointee != nullptr;
            }
            if (auto *l = dynamic_cast<const LazySymbolPointer<b> *>(blob)) {
               return l->pointee != nullptr;
            }
            return false;
         };

         std::size_t n_bind = 0, n_lazy = 0, n_rebase = 0, n_export = 0;

         /* (3) Symbol-pointer sections (S_{NON_,}LAZY_SYMBOL_POINTERS): each slot
          * maps through the indirect symbol table to a symbol or a local/abs
          * sentinel. External undef -> BIND (eager: we route lazy imports
          * through the non-lazy bind stream too, which is always correct and
          * avoids depending on the translated stub_helper / dyld_stub_binder
          * path; true LAZY_BIND is a future optimization). Defined symbol or
          * LOCAL sentinel -> REBASE (internal sliding pointer). These slots are
          * 8 bytes wide in the M64 output, so a modern 8-byte bind/rebase is
          * size-correct. */
         for (Segment<b> *seg : segments()) {
            for (Section<b> *sect : seg->sections) {
               const uint32_t stype = sect->sect.flags & SECTION_TYPE;
               if (stype != S_NON_LAZY_SYMBOL_POINTERS &&
                   stype != S_LAZY_SYMBOL_POINTERS) { continue; }
               const uint32_t reserved1 = sect->sect.reserved1;

               uint32_t slot = 0;
               for (SectionBlob<b> *blob : sect->content) {
                  if (dynamic_cast<SymbolPointer<b> *>(blob) == nullptr) {
                     continue; /* skip trailing placeholders etc. */
                  }
                  const uint32_t isym_idx = reserved1 + slot;
                  ++slot;
                  if (isym_idx >= dysymtab->indirectsyms.size()) { continue; }
                  const uint32_t isym = dysymtab->indirectsyms[isym_idx];

                  if (isym & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) {
                     /* LOCAL = baked internal pointer (slides) -> rebase, but
                      * only if it actually holds an internal pointer (skip
                      * null-valued slots). ABS = absolute (never slides). */
                     if ((isym & INDIRECT_SYMBOL_ABS) == 0 && sp_has_pointee(blob)) {
                        add_rebase(blob); ++n_rebase;
                     }
                     continue;
                  }
                  if (isym >= syms_by_index.size()) { continue; }
                  const Nlist<b> *nl = syms_by_index[isym];
                  if (nl->string == nullptr) { continue; }

                  if (nl->kind() == Nlist<b>::Kind::UNDEF) {
                     const uint8_t ord = GET_LIBRARY_ORDINAL(nl->nlist.n_desc);
                     int8_t special = 0;
                     const DylibCommand<b> *dylib = nullptr;
                     if (ord == DYNAMIC_LOOKUP_ORDINAL) {
                        special = BIND_SPECIAL_DYLIB_FLAT_LOOKUP;       /* -2 */
                     } else if (ord == EXECUTABLE_ORDINAL) {
                        special = BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE;   /* -1 */
                     } else if (ord >= 1 && ord <= ord_dylibs.size()) {
                        dylib = ord_dylibs[ord - 1];
                     } else {
                        /* SELF (0) can't be represented in this codebase's
                         * dylib_special sentinel scheme, and an unknown ordinal
                         * has no dylib; skip rather than emit a corrupt bind. */
                        continue;
                     }
                     uint8_t flags = 0;
                     if (nl->nlist.n_desc & N_WEAK_REF) {
                        flags |= BIND_SYMBOL_FLAGS_WEAK_IMPORT;
                     }
                     bind->bindees.push_back(
                        BindNode<b, false>::Create(BIND_TYPE_POINTER, 0, dylib, special,
                                                   nl->string->str, flags, blob));
                     ++n_bind;
                  } else {
                     /* Defined symbol referenced by a non-lazy pointer = an
                      * internal sliding pointer. */
                     add_rebase(blob); ++n_rebase;
                  }
               }
            }
         }

         /* (4) Absolute pointer arrays outside the symbol-pointer sections:
          * __mod_init_func / __mod_term_func (NonLazySymbolPointer blobs with a
          * resolved internal pointee). Mirrors Dysymtab::regenerate_local_relocs
          * but emits REBASE opcodes instead of a classic local-reloc table. Only
          * 8-byte NonLazySymbolPointer slots are rebased; 4-byte Immediate
          * pointers stay un-rebased (the translated dylib loads at a pinned base
          * with slide 0, so their baked values are already correct — same as the
          * classic path; an 8-byte rebase would clobber the neighbouring 4-byte
          * slot). */
         for (Segment<b> *seg : segments()) {
            const std::string sn = seg->name();
            if (sn == SEG_PAGEZERO || sn == SEG_LINKEDIT) { continue; }
            for (Section<b> *sect : seg->sections) {
               const uint32_t stype = sect->sect.flags & SECTION_TYPE;
               if (stype == S_NON_LAZY_SYMBOL_POINTERS ||
                   stype == S_LAZY_SYMBOL_POINTERS) { continue; } /* handled in (3) */
               for (SectionBlob<b> *blob : sect->content) {
                  auto *nlp = dynamic_cast<NonLazySymbolPointer<b> *>(blob);
                  if (nlp == nullptr || nlp->pointee == nullptr) { continue; }
                  add_rebase(blob); ++n_rebase;
               }
            }
         }

         /* (5) Exports: every defined external (N_SECT | N_EXT) symbol. N_ABS
          * externals have no `value` placeholder (Nlist parse only placeholders
          * N_SECT symbols) and are skipped — which also avoids re-exporting GCC
          * C++ `.eh` FDE markers (N_ABS, value 0) that dyld can't satisfy. Weak
          * definitions carry EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION. */
         for (const Nlist<b> *nl : symtab->syms) {
            if (nl->kind() != Nlist<b>::Kind::EXT) { continue; }
            if (nl->string == nullptr || nl->string->str.empty()) { continue; }
            if (Nlist<b>::is_header_symbol(nl->string->str)) { continue; }
            if (nl->value == nullptr) { continue; } /* N_ABS / no address */
            std::size_t eflags = EXPORT_SYMBOL_FLAGS_KIND_REGULAR;
            if (nl->nlist.n_desc & N_WEAK_DEF) {
               eflags |= EXPORT_SYMBOL_FLAGS_WEAK_DEFINITION;
            }
            auto *node = RegularExportNode<b>::Create(eflags, nl->value);
            export_info->trie.insert(nl->string->str, node);
            ++n_export;
         }

         auto *dyld = DyldInfo<b>::Create(rebase, bind, weak_bind, lazy_bind, export_info);

         /* Insert the LC_DYLD_INFO_ONLY immediately before LC_SYMTAB (its
          * conventional position, right after the segment commands). */
         auto pos = std::find(load_commands.begin(), load_commands.end(),
                              static_cast<LoadCommand<b> *>(symtab));
         load_commands.insert(pos, dyld);

         if (dbg) {
            fprintf(stderr,
                    "synthesize_dyld_info: %zu bind, %zu lazy, %zu rebase, "
                    "%zu export\n", n_bind, n_lazy, n_rebase, n_export);
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
