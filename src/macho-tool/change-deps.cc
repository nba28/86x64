#include <cstdio>
#include <iostream>
#include <string>

#include "change-deps.hh"
#include "util.hh"
#include "core/macho.hh"
#include "core/archive.hh"
#include "core/lc.hh"
#include "core/segment.hh"
#include "core/section.hh"

int ChangeDepsCommand::opthandler(int optchar) {
   switch (optchar) {
   case 'h':
      usage(std::cout);
      return 0;

   case 'c':
      {
         /* OLD=NEW, split on the FIRST '=' (install paths never contain one). */
         std::string arg = optarg;
         auto eq = arg.find('=');
         if (eq == std::string::npos) {
            throw std::string("--change expects OLD=NEW");
         }
         changes.emplace_back(arg.substr(0, eq), arg.substr(eq + 1));
      }
      return 1;

   case 'I':
      new_id = optarg;
      return 1;

   case 'q':
      quiet = true;
      return 1;

   default: abort();
   }
}

int ChangeDepsCommand::work() {
   MachO::MachO *macho = MachO::MachO::Parse(*in_img);
   int rv;
   switch (macho->bits()) {
   case MachO::Bits::M32:
      rv = workT(dynamic_cast<MachO::Archive<MachO::Bits::M32> *>(macho));
      break;
   case MachO::Bits::M64:
      rv = workT(dynamic_cast<MachO::Archive<MachO::Bits::M64> *>(macho));
      break;
   default:
      abort();
   }
   if (rv < 0) { return rv; }

   macho->Build();
   macho->Emit(*out_img);
   return 0;
}

template <MachO::Bits b>
int ChangeDepsCommand::workT(MachO::Archive<b> *archive) {
   if (archive == nullptr) {
      log("Mach-O binary is not an archive");
      return -1;
   }

   unsigned n_changed = 0;

   for (MachO::LoadCommand<b> *lc : archive->load_commands) {
      /* Dependency-loading commands (and LC_ID_DYLIB) all share the
       * dylib_command layout and are modelled by DylibCommand. */
      if (auto *dylib = dynamic_cast<MachO::DylibCommand<b> *>(lc)) {
         if (new_id && dylib->cmd() == LC_ID_DYLIB && dylib->name != *new_id) {
            if (!quiet) {
               fprintf(stderr, "  id: %s -> %s\n", dylib->name.c_str(), new_id->c_str());
            }
            dylib->name = *new_id;
            ++n_changed;
         }
         for (const auto& [old_path, new_path] : changes) {
            if (dylib->name == old_path) {
               if (!quiet) {
                  fprintf(stderr, "  dylib: %s -> %s\n",
                          dylib->name.c_str(), new_path.c_str());
               }
               dylib->name = new_path;
               ++n_changed;
            }
         }
         continue;
      }
      /* LC_RPATH shares the dylinker_command layout and is modelled by
       * DylinkerCommand (which also covers LC_LOAD_DYLINKER / LC_ID_DYLINKER);
       * only rewrite the LC_RPATH variant. */
      if (auto *dyl = dynamic_cast<MachO::DylinkerCommand<b> *>(lc)) {
         if (dyl->cmd() != LC_RPATH) { continue; }
         for (const auto& [old_path, new_path] : changes) {
            if (dyl->name == old_path) {
               if (!quiet) {
                  fprintf(stderr, "  rpath: %s -> %s\n",
                          dyl->name.c_str(), new_path.c_str());
               }
               dyl->name = new_path;
               ++n_changed;
            }
         }
         continue;
      }
   }

   /* Preserve the original header-region size so the re-emit keeps every
    * section (and thus every code/data vmaddr) at its current address. The
    * header region ends at the lowest file offset of any section with file
    * content (normally __TEXT,__text). See Archive::header_size_target. */
   std::size_t first_sect_off = 0;
   for (const MachO::Section<b> *sect : archive->sections()) {
      const std::size_t off = sect->sect.offset;
      if (off > 0 && (first_sect_off == 0 || off < first_sect_off)) {
         first_sect_off = off;
      }
   }
   archive->header_size_target = first_sect_off;

   if (!quiet) {
      fprintf(stderr, "change-deps: rewrote %u load-command path(s)\n", n_changed);
   }
   return 0;
}
