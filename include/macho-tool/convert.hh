#pragma once

#include <optional>

#include "command.hh"

struct ConvertCommand: InOutCommand {
   std::optional<uint32_t> filetype;
   bool synthesize_dyld_info = false; /*!< --synthesize-dyld-info: manufacture a
                                       * modern LC_DYLD_INFO_ONLY for a classic
                                       * (LC_DYSYMTAB-only) image so stock tools
                                       * accept it. See
                                       * Archive::synthesize_dyld_info. */

   virtual std::string optusage() const override {
      return "[-h|-a <type>|--synthesize-dyld-info]";
   }
   virtual const char *optstring() const override { return "ha:D"; }

   virtual std::vector<option> longopts() const override {
      return {{"help", no_argument, nullptr, 'h'},
              {"archive", required_argument, nullptr, 'a'},
              {"synthesize-dyld-info", no_argument, nullptr, 'D'},
              {0}};
   }

   virtual int opthandler(int optchar) override;
   virtual int work() override;
   ConvertCommand(): InOutCommand("convert") {}
   void archive_EXECUTE_to_DYLIB(MachO::Archive<MachO::Bits::M64> *archive);
};
