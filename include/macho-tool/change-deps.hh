#pragma once

#include <getopt.h>
#include <string>
#include <vector>
#include <utility>
#include <optional>

#include "command.hh"
#include "core/types.hh"

/* `macho-tool change-deps` — rewrite dependency-path strings in a Mach-O's
 * load commands, in a way that is safe for CLASSIC translated dylibs (which
 * cctools install_name_tool rejects with "local relocation entries out of
 * place" and llvm-install_name_tool punts on with "shared library not yet
 * supported").
 *
 * Rewrites the lc_str of every dependency-loading command
 *   LC_LOAD_DYLIB / LC_LOAD_WEAK_DYLIB / LC_REEXPORT_DYLIB /
 *   LC_LOAD_UPWARD_DYLIB / LC_LAZY_LOAD_DYLIB / LC_ID_DYLIB
 * and every LC_RPATH whose current value equals a --change OLD, plus an
 * optional --id to set LC_ID_DYLIB outright. The whole binary is re-emitted
 * through macho-tool's model (which rebuilds __LINKEDIT from scratch, so it
 * never trips cctools' reloc-placement constraints), while the original
 * header-region size is preserved so no code/data vmaddr moves.
 *
 *   macho-tool change-deps \
 *     --change /System/.../QuickTime.framework/Versions/A/QuickTime=@rpath/QuickTime.framework/Versions/A/QuickTime \
 *     --change /usr/lib/libcrypto.0.9.7.dylib=@rpath/libcrypto.0.9.7.dylib \
 *     [--id @rpath/foo.dylib] \
 *     in.dylib [out.dylib]
 *
 * OLD=NEW is split on the FIRST '=' (Mach-O install paths never contain '=').
 */
struct ChangeDepsCommand: InOutCommand {
   std::vector<std::pair<std::string, std::string>> changes; /* OLD -> NEW */
   std::optional<std::string> new_id;                        /* --id */
   bool quiet = false;

   virtual std::string optusage() const override {
      return "[-h] [--change OLD=NEW]... [--id NEW] [-q]";
   }
   virtual const char *optstring() const override { return "hc:I:q"; }

   virtual std::vector<option> longopts() const override {
      return {{"help",   no_argument,       nullptr, 'h'},
              {"change", required_argument, nullptr, 'c'},
              {"id",     required_argument, nullptr, 'I'},
              {"quiet",  no_argument,       nullptr, 'q'},
              {0}};
   }

   virtual int opthandler(int optchar) override;
   virtual int work() override;

   ChangeDepsCommand(): InOutCommand("change-deps") {}

private:
   template <MachO::Bits b>
   int workT(MachO::Archive<b> *archive);
};
