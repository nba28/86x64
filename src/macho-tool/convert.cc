#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <libgen.h>
#include <mach/machine.h>

#include "convert.hh"
#include "util.hh"
#include "core/macho.hh"
#include "core/archive.hh"
#include "core/symtab.hh"
#include "core/dyldinfo.hh"
#include "core/export_info.hh"
#include "core/section_blob.hh"
#include "core/segment.hh"
#include "core/section.hh"

int ConvertCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;
      
   case 'a':
      {
         auto it = mach_header_filetype_map.find(optarg);
         if (it != mach_header_filetype_map.end()) {
            filetype = it->second;
         } else {
            throw std::string("invalid Mach-O archive filetype");
         }
      }
      return 1;

   case 'D':
      synthesize_dyld_info = true;
      return 1;

   default: abort();
   }
}

int ConvertCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
      
   if (filetype) {
      auto archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho);
      if (archive == nullptr) {
         log("MachO binary is not an archive");
         return -1;
      }
         
      /* change filetype */
      archive->header.filetype = *filetype;
         
      switch (*filetype) {
      case MH_DYLIB:
         archive_EXECUTE_to_DYLIB(archive);
         break;
            
      default:
         log("unsupported Mach-O archive filetype for conversion");
         return -1;
      }
   }

   /* Opt-in: manufacture a modern LC_DYLD_INFO_ONLY for a classic image so the
    * output is a canonical modern dylib (install_name_tool/llvm accept it; binds
    * flow through the modern opcode path). Only meaningful for an M64 archive;
    * the synthesis itself no-ops for an already-modern or non-classic image. */
   if (synthesize_dyld_info) {
      if (auto *archive = dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho)) {
         archive->synthesize_dyld_info_enabled = true;
      } else {
         log("--synthesize-dyld-info: input is not an M64 archive; ignoring");
      }
   }

   macho->Build();
   macho->Emit(*out_img);

   return 0;
}

void ConvertCommand::archive_EXECUTE_to_DYLIB(MachO::Archive<MachO::Bits::M64> *archive) {
   /* add LC_ID_DYLIB.
    *
    * Prefix the basename with @rpath so the wrapper executable that links
    * against this dylib can find it via -rpath, rather than dyld looking
    * for an unqualified filename in the current working directory only. */
   char *out_path = strdup(this->out_path);
   std::string install_name = std::string("@rpath/") + basename(out_path);
   auto id_dylib = MachO::DylibCommand<MachO::Bits::M64>::Create(LC_ID_DYLIB, install_name);
   archive->load_commands.push_back(id_dylib);
   free(out_path);

   /* check whether MH_NO_REEXPORTED_DYLIBS should be added */
   if (archive->template subcommands<MachO::LoadCommand, LC_REEXPORT_DYLIB>().empty()) {
      archive->header.flags |= MH_NO_REEXPORTED_DYLIBS;
   }

   /*
    * Snapshot the entry-point Placeholder before we drop the load command
    * that owns it. The wrapper exe links against this dylib expecting an
    * external `_main`, and many real-world i386 executables (anything
    * shipped with `strip`) have *no* symbol at the entry — we have to
    * synthesize one. If a defined symbol with that name already exists we
    * leave it alone.
    */
   const MachO::Placeholder<MachO::Bits::M64> *entry_placeholder = nullptr;
   if (auto *main_lc = archive->template subcommand<MachO::EntryPoint>()) {
      entry_placeholder = main_lc->entry;
   } else if (auto *thr_lc = archive->template subcommand<MachO::UnixThread>()) {
      entry_placeholder = thr_lc->entry;
   }

   /* remove PAGEZERO — using erase's return value so we don't double-advance
    * past the element that took the erased slot (and so we don't run off the
    * end into UB if __PAGEZERO was the last command). */
   for (auto it = archive->load_commands.begin(); it != archive->load_commands.end(); /**/) {
      auto segment = dynamic_cast<MachO::Segment<MachO::Bits::M64> *>(*it);
      if (segment && strcmp(SEG_PAGEZERO, segment->segment_command.segname) == 0) {
         it = archive->load_commands.erase(it);
      } else {
         ++it;
      }
   }

   /* remove __mh_execute_header */
   auto symtab = archive->template subcommand<MachO::Symtab>();
   if (symtab == nullptr) {
      throw std::string("missing symtab");
   }

   /* Snapshot the symbol order BEFORE we mutate the symbol table.
    *
    * LC_DYSYMTAB's indirect symbol table is an array of raw INDICES into the
    * symbol table (one per __la/__nl/__IMPORT pointer slot and stub). The
    * symbol removals (__mh_execute_header, a stale private _main) and the
    * _main insertion below shift every subsequent symbol's index, but the
    * indirect indices were carried verbatim from the i386 input — so without
    * fix-up they would point at the WRONG symbols afterward. Record the
    * pre-mutation order so we can re-map the indirect table by symbol
    * IDENTITY once all edits are done (see the re-index pass below). */
   std::vector<const MachO::Nlist<MachO::Bits::M64> *> old_sym_order(
      symtab->syms.begin(), symtab->syms.end());

   symtab->remove("__mh_execute_header");

   /* Inject the synthesized `_main` symbol now that the symtab exists. We
    * pick n_sect=1 (the first section, conventionally __TEXT,__text).
    *
    * The wrapper exe links `extern _main` against this dylib and needs an
    * *external* defined `_main` at the ENTRY point. Two subtleties:
    *   - We only treat _main as "already present" if it is N_EXT. A private
    *     (non-external) `_main` does not satisfy the link and must not block
    *     synthesis. (Real-world i386 execs from gcc/ld carry a private SECT
    *     `_main` — the C `main()` — at a DIFFERENT address than the entry,
    *     which is the crt `start` stub. The wrapper wants the entry.)
    *   - A defined symbol named `_main` may already exist but be private and
    *     at the wrong address; drop it so the synthesized external entry
    *     `_main` is the sole, unambiguous one. */
   if (entry_placeholder != nullptr) {
      bool already_have_main = false;
      for (auto *sym : symtab->syms) {
         if (sym->string && sym->string->str == "_main"
             && sym->type() == MachO::Nlist<MachO::Bits::M64>::Type::SECT
             && (sym->nlist.n_type & N_EXT)) {
            already_have_main = true;
            break;
         }
      }
      if (!already_have_main) {
         /* Drop any pre-existing private `_main` (wrong address / not
          * external) so we don't emit two `_main` nlists. */
         symtab->remove("_main");
         /* Locate __TEXT,__text — the entry point lives there. */
         const MachO::Section<MachO::Bits::M64> *text_section = nullptr;
         if (auto *text_seg = archive->segment(SEG_TEXT)) {
            for (auto *s : text_seg->sections) {
               if (std::string(s->sect.sectname) == SECT_TEXT) {
                  text_section = s;
                  break;
               }
            }
         }
         if (text_section == nullptr) {
            throw std::string("can't find __TEXT,__text to anchor synthesized _main");
         }
         auto *name_str = MachO::String<MachO::Bits::M64>::Create("_main");
         symtab->strs.push_back(name_str);
         auto *main_sym = MachO::Nlist<MachO::Bits::M64>::CreateDefinedExt(
            name_str, entry_placeholder, text_section);
         symtab->syms.insert(main_sym);
      }

      /*
       * The runtime linker resolves `extern _main` against the dyld export
       * trie, not the classic nlist symtab — so we ALSO need the entry to
       * show up in LC_DYLD_INFO_ONLY's exports. Without this nm finds the
       * symbol but `ld` linking against the dylib does not.
       */
      if (auto *dyld = archive->template subcommand<MachO::DyldInfo>()) {
         if (dyld->export_info != nullptr) {
            auto& trie = dyld->export_info->trie;
            /* Drop the now-bogus __mh_execute_header entry if it's still in
             * the trie (we removed it from the nlist symtab above but the
             * trie is a separate data structure). */
            auto mh_it = std::find_if(
               trie.begin(), trie.end(),
               [] (const auto& p) { return p.first == "__mh_execute_header"; });
            if (mh_it != trie.end()) {
               trie.erase(mh_it);
            }
            auto *node = MachO::RegularExportNode<MachO::Bits::M64>::Create(
               /*flags=*/EXPORT_SYMBOL_FLAGS_KIND_REGULAR,
               /*value=*/entry_placeholder);
            trie.insert(std::string("_main"), node);
         }
      }
   }

   /*
    * Re-index the indirect symbol table to track the symbol-table edits above.
    *
    * The edits (remove __mh_execute_header, remove a stale private _main,
    * insert the synthesized external _main) change every later symbol's index.
    * The indirect symbol table holds raw indices, carried verbatim from the
    * i386 input, so they now reference the wrong symbols. For a CLASSIC image
    * (LC_DYSYMTAB only, NO LC_DYLD_INFO bind opcode stream — e.g. a GCC-built
    * game executable) dyld resolves the __la/__nl/__IMPORT pointer slots purely
    * through this table, so a stale index makes EVERY slot bind to a
    * neighbouring symbol. Concretely, Civilization IV's __IMPORT,__pointers has
    * ~25k self-referential non-lazy pointers (FDE `.eh` markers, vtables,
    * statics); an off-by-one retargets ~18k of them onto N_ABS value-0 `.eh`
    * markers, which aren't exportable, so dyld aborts at load with
    * "Symbol not found: ..._GetClassInfo_StaticEv.eh".
    *
    * (Modern images bind via the LC_DYLD_INFO opcode stream, which names
    * symbols directly, so the index shift was harmless there — but re-indexing
    * is still correct for them, leaving the table consistent for any tool that
    * reads it.)
    *
    * Re-map each entry by symbol IDENTITY: old index -> Nlist* (snapshot taken
    * before the edits) -> new index (post-edit multiset order, which is exactly
    * the Build/Emit order). Leave the INDIRECT_SYMBOL_LOCAL/_ABS sentinels and
    * any out-of-range entry untouched. A slot whose referenced symbol was
    * dropped (e.g. a stub to __mh_execute_header / the stale _main — these
    * don't normally occur) becomes INDIRECT_SYMBOL_ABS so dyld leaves it as-is
    * rather than binding a stale index. */
   if (auto *dysymtab = archive->template subcommand<MachO::Dysymtab>()) {
      std::unordered_map<const MachO::Nlist<MachO::Bits::M64> *, uint32_t> new_index;
      new_index.reserve(symtab->syms.size());
      uint32_t idx = 0;
      for (const auto *sym : symtab->syms) {
         new_index[sym] = idx++;
      }
      for (uint32_t& entry : dysymtab->indirectsyms) {
         if ((entry & (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)) != 0) {
            continue; /* sentinel: a local/absolute pointer, not a symbol ref */
         }
         if (entry >= old_sym_order.size()) {
            continue; /* out of range (shouldn't happen) — leave untouched */
         }
         auto it = new_index.find(old_sym_order[entry]);
         entry = (it != new_index.end()) ? it->second
                                         : (uint32_t) INDIRECT_SYMBOL_ABS;
      }
   }

   /*
    * Remove every load command that only makes sense for an executable.
    * LC_UNIXTHREAD is the pre-LC_MAIN entry-point form; both must be gone
    * for the result to be a valid dylib.
    */
   archive->remove_commands(LC_LOAD_DYLINKER);
   archive->remove_commands(LC_MAIN);
   archive->remove_commands(LC_UNIXTHREAD);

   /* tweak archive header */
   archive->header.cpusubtype &= ~ (uint32_t) CPU_SUBTYPE_LIB64;
   archive->header.flags &= ~ (uint32_t) MH_PIE;

   /*
    * Preserve the low-32-bit vmaddr base chosen by `transform` (0x10000000).
    * The dylib must live entirely below 4GB so the i386 absolute pointers
    * baked into __data fit in their 4-byte slots after transform-time
    * remapping; the slide must be 0 at runtime (wrapper disables ASLR via
    * posix_spawn) so the patched values are also the real load addresses.
    */
   archive->vmaddr = 0x10000000;

   /* Define ___dso_handle at the image base.
    *
    * The i386 MH_EXECUTE carries ___dso_handle as an N_ABS symbol valued at the
    * mach header (image base), provided by crt1.o. C++ static (de)structor
    * registration passes its ADDRESS (&__dso_handle) to __cxa_atexit to
    * identify the owning image. Converting to a DYLIB keeps the reference (a
    * self-referential __IMPORT,__pointers / symbol-pointer bind) but dyld cannot
    * satisfy a bind to an N_ABS symbol — only N_SECT defined symbols are yielded
    * as a dylib's exports — so it aborts at load with
    * "Symbol not found: ___dso_handle, Expected in: <this dylib>".
    *
    * A normal dylib gets ___dso_handle from dylib1.o/ld, defined as a section-
    * relative symbol at the image base. Mirror that: retype the existing N_ABS
    * ___dso_handle to a DEFINED N_SECT external symbol anchored at __TEXT,__text
    * and valued at the image base, so the self-bind resolves to a stable,
    * image-unique address. It must be THIS image's own defined symbol — never
    * imported from a shared shim — because &__dso_handle is what tells
    * __cxa_finalize which image an atexit registration belongs to. (The slide is
    * 0 at runtime, so the pinned base is also the real load address.) */
   {
      const MachO::Section<MachO::Bits::M64> *text_section = nullptr;
      if (auto *text_seg = archive->segment(SEG_TEXT)) {
         for (auto *s : text_seg->sections) {
            if (std::string(s->sect.sectname) == SECT_TEXT) {
               text_section = s;
               break;
            }
         }
      }
      if (text_section != nullptr) {
         for (auto *sym : symtab->syms) {
            if (sym->string && sym->string->str == "___dso_handle"
                && sym->type() == MachO::Nlist<MachO::Bits::M64>::Type::ABS) {
               sym->nlist.n_type = N_SECT | N_EXT;
               sym->nlist.n_desc = 0;
               sym->section = text_section;          /* Build sets n_sect from this */
               sym->value = nullptr;                 /* keep our n_value through Emit */
               sym->nlist.n_value = archive->vmaddr; /* image base (pinned, slide 0) */
            }
         }
      }
   }
}
