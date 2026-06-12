#include <cstdlib>
#include <cassert>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

#include "lc.hh"
#include "segment.hh"
#include "image.hh"
#include "dyldinfo.hh"
#include "linkedit.hh"
#include "symtab.hh"
#include "transform.hh"
#include "section_blob.hh" // SectionBlob
#include "archive.hh" // Archive::Insert

namespace MachO {

   template <Bits bits>
   LoadCommand<bits> *LoadCommand<bits>::Parse(const Image& img, std::size_t offset,
                                               ParseEnv<bits>& env)
   {
      load_command lc = img.at<load_command>(offset);

      switch (lc.cmd) {
      case LC_SEGMENT:
         if constexpr (bits == Bits::M32) {
               return Segment<Bits::M32>::Parse(img, offset, env);
            } else {
            throw("32-bit segment command in 64-bit binary");
         }
         
      case LC_SEGMENT_64:
         if constexpr (bits == Bits::M64) {
               return Segment<Bits::M64>::Parse(img, offset, env);
            } else {
            throw("64-bit segment command in 32-bit binary");
         }

      case LC_DYLD_INFO:
      case LC_DYLD_INFO_ONLY:
         return DyldInfo<bits>::Parse(img, offset, env);

      case LC_SYMTAB:
         return Symtab<bits>::Parse(img, offset, env);

      case LC_DYSYMTAB:
         return Dysymtab<bits>::Parse(img, offset, env);

      case LC_LOAD_DYLINKER:
      case LC_RPATH:
         /* dylinker_command and rpath_command share layout (cmd/cmdsize/lc_str),
          * so DylinkerCommand parses/builds/emits both correctly; the stored
          * cmd field carries LC_RPATH through to emission. */
         return DylinkerCommand<bits>::Parse(img, offset, env);

      case LC_UUID:
         return UUID<bits>::Parse(img, offset, env);

      case LC_BUILD_VERSION:
         return BuildVersion<bits>::Parse(img, offset, env);

      case LC_SOURCE_VERSION:
         return SourceVersion<bits>::Parse(img, offset, env);

      case LC_VERSION_MIN_MACOSX:
      case LC_VERSION_MIN_IPHONEOS:
      case LC_VERSION_MIN_WATCHOS:
      case LC_VERSION_MIN_TVOS:
         /* All four share version_min_command layout; pre-10.13 toolchains
          * still emit LC_VERSION_MIN_MACOSX on i386 builds. */
         return VersionMin<bits>::Parse(img, offset, env);

      case LC_MAIN:
         return EntryPoint<bits>::Parse(img, offset, env);

      case LC_UNIXTHREAD:
         return UnixThread<bits>::Parse(img, offset, env);

      case LC_LOAD_DYLIB:
      case LC_LOAD_WEAK_DYLIB:
      case LC_REEXPORT_DYLIB:
      case LC_LOAD_UPWARD_DYLIB:
      case LC_LAZY_LOAD_DYLIB:
      case LC_ID_DYLIB:
         /* All six share the dylib_command layout. AssignID (Build phase)
          * and the parse-time dylib_resolver.add(this) (DylibCommand ctor)
          * both handle them uniformly, so any bind ordinal targeting a
          * weak/reexport/upward/lazy dylib resolves correctly. Was: throw
          * on weak — broke any i386 binary linking with -weak_framework. */
         return DylibCommand<bits>::Parse(img, offset, env);

      case LC_DATA_IN_CODE:
      case LC_FUNCTION_STARTS:
      case LC_CODE_SIGNATURE:
      case LC_SEGMENT_SPLIT_INFO:
      case LC_DYLIB_CODE_SIGN_DRS:
      case LC_LINKER_OPTIMIZATION_HINT:
         /* All five share linkedit_data_command layout: cmd/cmdsize/dataoff/datasize.
          * Parse generically; the data blob in __LINKEDIT is copied through verbatim
          * since none of it references vmaddrs that the M32→M64 transform shifts. */
         return LinkeditData<bits>::Parse(img, offset, env);
         
      default:
         throw error("load command 0x%x not supported", lc.cmd);
      }
   }


   template <Bits bits>
   DylinkerCommand<bits>::DylinkerCommand(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), dylinker(img.at<dylinker_command>(offset))
   {
      if (dylinker.name.offset > dylinker.cmdsize) {
         throw error("dylinker name starts past end of command");
      }
      const std::size_t maxstrlen = dylinker.cmdsize - dylinker.name.offset;
      const std::size_t slen = strnlen(&img.at<char>(offset + dylinker.name.offset), maxstrlen);
      const char *strbegin = &img.at<char>(offset + dylinker.name.offset);
      name = std::string(strbegin, strbegin + slen);
   }

   template <Bits bits>
   std::size_t DylinkerCommand<bits>::size() const {
      return align_up(sizeof(dylinker_command) + name.size() + 1, sizeof(macho_addr_t<bits>));
   }

   template <Bits bits>
   BuildVersion<bits>::BuildVersion(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), build_version(img.at<build_version_command>(offset))
   {
      for (int i = 0; i < build_version.ntools; ++i) {
         tools.emplace_back(img, offset + sizeof(build_version) + i * BuildToolVersion::size());
      }
   }

   template <Bits bits>
   std::size_t BuildVersion<bits>::size() const {
      return sizeof(build_version) + BuildToolVersion::size() * tools.size();
   }

   template <Bits bits>
   EntryPoint<bits>::EntryPoint(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), entry_point(img.at<entry_point_command>(offset))
   {
      // env.offset_resolver.resolve(entry_point.entryoff, &entry);
   }

   template <Bits bits>
   DylibCommand<bits>::DylibCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env), dylib_cmd(img.at<dylib_command>(offset))
   {
      const std::size_t stroff = dylib_cmd.dylib.name.offset;
      if (stroff > dylib_cmd.cmdsize) {
         throw error("dynamic library name starts past end of dylib command");
      }
      const std::size_t strrem = dylib_cmd.cmdsize - stroff;
      const std::size_t len = strnlen(&img.at<char>(offset + stroff), strrem);
      name = std::string(&img.at<char>(offset + stroff), &img.at<char>(offset + stroff + len));

      /* Every dylib-loading LC contributes to the ordinal counter that
       * BindNode references via SET_DYLIB_ORDINAL_{IMM,ULEB}. AssignID
       * already handles all five (LOAD/WEAK/REEXPORT/UPWARD/LAZY); the
       * parse-time resolver must mirror that or any bind targeting a
       * WEAK/REEXPORT/etc. dylib would resolve to nullptr and be silently
       * dropped from the output (same failure mode as the ULEB-ordinal bug
       * in dyldinfo.cc). LC_ID_DYLIB is excluded — it identifies *this*
       * library, not a dependency, and never receives a bind ordinal. */
      if (dylib_cmd.cmd != LC_ID_DYLIB) {
         env.dylib_resolver.add(this);
      }
   }

   template <Bits bits>
   std::size_t DylibCommand<bits>::size() const {
      return align_up(sizeof(dylib_cmd) + name.size() + 1, sizeof(macho_addr_t<bits>));
   }

   template <Bits bits>
   void DylinkerCommand<bits>::Build(BuildEnv<bits>& env) {
      dylinker.cmdsize = size();
      dylinker.name.offset = sizeof(dylinker);
   }

   template <Bits bits>
   void BuildVersion<bits>::Build(BuildEnv<bits>& env) {
      build_version.cmdsize = size();
      build_version.ntools = tools.size();
   }

   template <Bits bits>
   void EntryPoint<bits>::Build(BuildEnv<bits>& env) {
      entry_point.cmdsize = size();
      entry_point.entryoff = entry->loc.offset;
   }

   template <Bits bits>
   void DylinkerCommand<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dylinker_command>(offset) = dylinker;
      memcpy(&img.at<char>(offset + dylinker.name.offset), name.c_str(), name.size() + 1);
   }

   template <Bits bits>
   void UUID<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<uuid_command>(offset) = uuid;
   }

   template <Bits bits>
   void BuildVersion<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<build_version_command>(offset) = build_version;
      offset += sizeof(build_version_command);
      for (const BuildToolVersion& tool : tools) {
         tool.Emit(img, offset);
         offset += tool.size();
      }
   }

   void BuildToolVersion::Emit(Image& img, std::size_t offset) const {
      img.at<build_tool_version>(offset) = tool;
   }

   template <Bits bits>
   void SourceVersion<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<source_version_command>(offset) = source_version;
   }

   template <Bits bits>
   void EntryPoint<bits>::Emit(Image& img, std::size_t offset) const {
      assert(strcmp(entry->segment->segment_command.segname, SEG_TEXT) == 0);
      entry_point_command entry_point = this->entry_point;
      entry_point.entryoff = entry->loc.vmaddr - entry->segment->segment_command.vmaddr;
      img.at<entry_point_command>(offset) = entry_point;
   }

   template <Bits bits>
   void DylibCommand<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dylib_command>(offset) = dylib_cmd;
      offset += sizeof(dylib_command);
      memcpy(&img.at<char>(offset), name.c_str(), name.size() + 1);
      offset += name.size() + 1;
   }

   template <Bits bits>
   LoadCommand<bits>::LoadCommand(const LoadCommand<opposite<bits>>& other,
                                  TransformEnv<opposite<bits>>& env) {
      env.add(&other, this);
   }
   
   template <Bits bits>
   EntryPoint<bits>::EntryPoint(const EntryPoint<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env):
      LoadCommand<bits>(other, env), entry_point(other.entry_point), entry(nullptr)
   {
      env.resolve(other.entry, &entry);
   }

   template <Bits bits>
   void EntryPoint<bits>::Parse1(const Image& img, ParseEnv<bits>& env) {
      const std::size_t vmaddr = env.archive.offset_to_vmaddr(entry_point.entryoff);
      entry = env.add_placeholder(vmaddr);
   }

#if 0
   template <Bits bits>
   void EntryPoint<bits>::Parse2(ParseEnv<bits>& env) {
      Location loc(entry_point.entryoff, 0);
      Placeholder<bits> *entry = Placeholder<bits>::Parse(loc, env);
      env.archive.insert(entry, loc, Relation::BEFORE);
      this->entry = entry;
   }
#endif

   /* ---- LC_UNIXTHREAD ---- */

   template <Bits bits>
   UnixThread<bits>::UnixThread(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LoadCommand<bits>(img, offset, env),
      thread_cmd(img.at<thread_command>(offset))
   {
      const std::size_t state_off = offset + sizeof(thread_command);
      flavor = img.at<uint32_t>(state_off);
      count = img.at<uint32_t>(state_off + sizeof(uint32_t));

      /* Sanity: cmdsize must hold the header + flavor + count + state words. */
      const std::size_t expect = sizeof(thread_command) + 2 * sizeof(uint32_t)
                                 + count * sizeof(uint32_t);
      if (thread_cmd.cmdsize < expect) {
         throw error("LC_UNIXTHREAD: cmdsize %u smaller than header+state %zu",
                     thread_cmd.cmdsize, expect);
      }

      /* Refuse flavors whose pc index we don't know. The pipeline targets
       * x86, so reject ppc/arm thread states up front rather than silently
       * mis-locating the entry point. */
      const uint32_t want_flavor = (bits == Bits::M32) ? 1u /* x86_THREAD_STATE32 */
                                                       : 4u /* x86_THREAD_STATE64 */;
      if (flavor != want_flavor) {
         throw error("LC_UNIXTHREAD: unexpected thread-state flavor %u for %d-bit binary",
                     flavor, (bits == Bits::M32) ? 32 : 64);
      }
      if (count <= pc_word_index()) {
         throw error("LC_UNIXTHREAD: thread state count %u too small to hold pc", count);
      }

      state.resize(count);
      const std::size_t words_off = state_off + 2 * sizeof(uint32_t);
      for (uint32_t i = 0; i < count; ++i) {
         state[i] = img.at<uint32_t>(words_off + i * sizeof(uint32_t));
      }
   }

   template <Bits bits>
   void UnixThread<bits>::Parse1(const Image& img, ParseEnv<bits>& env) {
      /* eip / rip is the virtual address the kernel sets up before jumping
       * into the binary. Wire it through the placeholder resolver like
       * EntryPoint does so it follows any later moves of the entry blob. */
      std::size_t pc;
      if constexpr (bits == Bits::M32) {
         pc = state[pc_word_index()];
      } else {
         pc = static_cast<uint64_t>(state[pc_word_index()])
            | (static_cast<uint64_t>(state[pc_word_index() + 1]) << 32);
      }
      entry = env.add_placeholder(pc);
   }

   template <Bits bits>
   void UnixThread<bits>::Build(BuildEnv<bits>& env) {
      thread_cmd.cmdsize = size();
   }

   template <Bits bits>
   void UnixThread<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<thread_command>(offset) = thread_cmd;
      offset += sizeof(thread_command);
      img.at<uint32_t>(offset) = flavor;                            offset += sizeof(uint32_t);
      img.at<uint32_t>(offset) = count;                             offset += sizeof(uint32_t);

      /* Rewrite the pc slot with the (possibly relocated) entry vmaddr. */
      std::vector<uint32_t> out = state;
      const std::size_t pc_vmaddr = entry ? entry->loc.vmaddr : 0;
      if constexpr (bits == Bits::M32) {
         out[pc_word_index()] = static_cast<uint32_t>(pc_vmaddr);
      } else {
         out[pc_word_index()    ] = static_cast<uint32_t>(pc_vmaddr & 0xffffffffu);
         out[pc_word_index() + 1] = static_cast<uint32_t>(pc_vmaddr >> 32);
      }
      for (uint32_t w : out) {
         img.at<uint32_t>(offset) = w;
         offset += sizeof(uint32_t);
      }
   }

   template <Bits bits>
   UnixThread<bits>::UnixThread(const UnixThread<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env):
      LoadCommand<bits>(other, env), entry(nullptr)
   {
      thread_cmd = other.thread_cmd;
      /* Switch flavor + extend the state buffer for the new bit-width. The
       * kernel ignores everything but pc for LC_UNIXTHREAD bring-up of a
       * userspace process, so the zero-extension of other registers is fine.
       */
      if constexpr (bits == Bits::M64) {
         flavor = 4;          /* x86_THREAD_STATE64 */
         count = 42;          /* x86_THREAD_STATE64_COUNT */
      } else {
         flavor = 1;          /* x86_THREAD_STATE32 */
         count = 16;          /* x86_THREAD_STATE32_COUNT */
      }
      state.assign(count, 0);

      env.resolve(other.entry, &entry);
   }

   /* ---- /LC_UNIXTHREAD ---- */

   template <Bits bits>
   void DylibCommand<bits>::Build(BuildEnv<bits>& env) {
      dylib_cmd.cmdsize = size();
      dylib_cmd.dylib.name.offset = sizeof(dylib_cmd);
   }

   template <Bits bits>
   void DylibCommand<bits>::AssignID(BuildEnv<bits>& env) {
      switch (dylib_cmd.cmd) {
      case LC_ID_DYLIB:
         id = 0;
         break;

      /*
       * All dylib-loading commands behave the same way for the link-order
       * counter (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB,
       * LC_LOAD_UPWARD_DYLIB, LC_LAZY_LOAD_DYLIB). iPhoto links some
       * frameworks weakly; was abort() on those, which crashed Build.
       */
      case LC_LOAD_DYLIB:
      case LC_LOAD_WEAK_DYLIB:
      case LC_REEXPORT_DYLIB:
      case LC_LOAD_UPWARD_DYLIB:
      case LC_LAZY_LOAD_DYLIB:
         id = env.dylib_counter();
         break;

      default:
         /* Unknown LC_*_DYLIB variant — best effort: allocate a counter
          * id (treat as load). Logs once if MACHO_BUILD_DEBUG is set. */
         if (std::getenv("MACHO_BUILD_DEBUG")) {
            fprintf(stderr,
                    "DylibCommand::AssignID: unknown cmd 0x%x; treating as LC_LOAD_DYLIB\n",
                    (unsigned)dylib_cmd.cmd);
         }
         id = env.dylib_counter();
         break;
      }
   }

   template <Bits bits>
   DylibCommand<bits>::DylibCommand(uint32_t cmd, const std::string& name, uint32_t timestamp,
                                    uint32_t current_version, uint32_t compatibility_version):
      name(name)
   {
      dylib_cmd.cmd = cmd;
      dylib_cmd.dylib.timestamp = timestamp;
      dylib_cmd.dylib.current_version = current_version;
      dylib_cmd.dylib.compatibility_version = compatibility_version;
   }

   template class LoadCommand<Bits::M32>;
   template class LoadCommand<Bits::M64>;

   template class DylibCommand<Bits::M32>;
   template class DylibCommand<Bits::M64>;

   template class UnixThread<Bits::M32>;
   template class UnixThread<Bits::M64>;
   
}
