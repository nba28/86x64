#include <algorithm>
#include <iostream>
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
   symtab->remove("__mh_execute_header");

   /* Inject the synthesized `_main` symbol now that the symtab exists. We
    * pick n_sect=1 (the first section, conventionally __TEXT,__text).
    * If a `_main` is already present we don't double-add. */
   if (entry_placeholder != nullptr) {
      bool already_have_main = false;
      for (auto *sym : symtab->syms) {
         if (sym->string && sym->string->str == "_main"
             && sym->type() == MachO::Nlist<MachO::Bits::M64>::Type::SECT) {
            already_have_main = true;
            break;
         }
      }
      if (!already_have_main) {
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
}
