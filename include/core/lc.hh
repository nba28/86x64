#pragma once

#include <cstdint>
#include <cstdio>
#include <unordered_map>
#include <string>
#include <vector>
#include <mach-o/loader.h>

#include "util.hh"
#include "image.hh"
#include "parse.hh"
#include "build.hh"

namespace MachO {

   template <Bits bits>
   class LoadCommand: public Node {
   public:
      virtual uint32_t cmd() const = 0;
      virtual std::size_t size() const = 0;

      static LoadCommand<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      virtual void Parse1(const Image& img, ParseEnv<bits>& env) {}
      virtual void Parse2(ParseEnv<bits>& env) {}
      
      virtual void Build(BuildEnv<bits>& env) = 0;
      virtual ~LoadCommand() {}
      virtual void AssignID(BuildEnv<bits>& env) {} /*!< build pass 1 */
      virtual void Emit(Image& img, std::size_t offset) const = 0;

      virtual LoadCommand<opposite<bits>> *Transform(TransformEnv<bits>& env) const { abort(); }
      
   protected:
      LoadCommand() {}
      LoadCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env) {}
      LoadCommand(const LoadCommand<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
   };

   template <Bits bits>
   class LinkeditCommand: public LoadCommand<bits> {
   public:
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) = 0;
      virtual void Build(BuildEnv<bits>& env) override {}
      virtual std::size_t content_size() const = 0;

   protected:
      LinkeditCommand() {} /*!< for synthesized linkedit commands (e.g. a
                            * classic image's manufactured LC_DYLD_INFO_ONLY) */
      LinkeditCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LoadCommand<bits>(img, offset, env) {}
      LinkeditCommand(const LinkeditCommand<opposite<bits>>& other,
                      TransformEnv<opposite<bits>>& env): LoadCommand<bits>(other, env) {}
      template <Bits b> friend class LinkeditCommand;
   };

   template <Bits bits>
   class DylinkerCommand: public LoadCommand<bits> {
   public:
      dylinker_command dylinker;
      std::string name;

      virtual uint32_t cmd() const override { return dylinker.cmd; }      
      virtual std::size_t size() const override;

      static DylinkerCommand<bits> *Parse(const Image& img, std::size_t offset,
                                          ParseEnv<bits>& env) {
         return new DylinkerCommand(img, offset, env);
      }

      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      virtual DylinkerCommand<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DylinkerCommand<opposite<bits>>(*this, env);
      }

   private:
      DylinkerCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      DylinkerCommand(const DylinkerCommand<opposite<bits>>& other,
                      TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), dylinker(other.dylinker), name(other.name) {}

      template <Bits b> friend class DylinkerCommand;
   };

   template <Bits bits>
   class UUID: public LoadCommand<bits> {
   public:
      uuid_command uuid;

      virtual uint32_t cmd() const override { return uuid.cmd; }
      virtual std::size_t size() const override { return sizeof(uuid_command); }

      static UUID<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new UUID(img, offset, env);
      }
      
      virtual void Build(BuildEnv<bits>& env) override {
         uuid.cmdsize = size();
      }
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      virtual UUID<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new UUID<opposite<bits>>(*this, env);
      }

   private:
      UUID(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LoadCommand<bits>(img, offset, env), uuid(img.at<uuid_command>(offset)) {}
      UUID(const UUID<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), uuid(other.uuid) {}

      template <Bits b> friend class UUID;
   };

   class BuildToolVersion {
   public:
      build_tool_version tool;

      static std::size_t size() { return sizeof(build_tool_version); }
      void Emit(Image& img, std::size_t offset) const;
      
      BuildToolVersion(const Image& img, std::size_t offset):
         tool(img.at<build_tool_version>(offset)) {}
      BuildToolVersion(const build_tool_version& tool): tool(tool) {}
   };

   template <Bits bits>
   class BuildVersion: public LoadCommand<bits> {
   public:
      using Tools = std::vector<BuildToolVersion>;
      build_version_command build_version;
      Tools tools;

      virtual uint32_t cmd() const override { return build_version.cmd; }      
      virtual std::size_t size() const override;
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;

      static BuildVersion<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new BuildVersion(img, offset, env);
      }
      
      virtual BuildVersion<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new BuildVersion<opposite<bits>>(*this, env);
      }

   private:
      BuildVersion(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      BuildVersion(const BuildVersion<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), build_version(other.build_version), tools(other.tools) {}

      template <Bits b> friend class BuildVersion;
   };

   template <Bits bits>
   class SourceVersion: public LoadCommand<bits> {
   public:
      source_version_command source_version;

      virtual uint32_t cmd() const override { return source_version.cmd; }      
      virtual std::size_t size() const override { return sizeof(source_version); }
      virtual void Build(BuildEnv<bits>& env) override {
         source_version.cmdsize = size();
      }
      virtual void Emit(Image& img, std::size_t offset) const override;

      static SourceVersion<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env)
      { return new SourceVersion(img, offset, env); }
      
      template <typename... Args>
      static SourceVersion<bits> *Parse(Args&&... args) {
         return new SourceVersion(args...);
      }
      
      virtual SourceVersion<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new SourceVersion<opposite<bits>>(*this, env);
      }

   private:
      SourceVersion(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LoadCommand<bits>(img, offset, env), source_version(img.at<source_version_command>(offset)) {}
      SourceVersion(const SourceVersion<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), source_version(other.source_version) {}

      template <Bits b> friend class SourceVersion;
   };

   /*
    * LC_VERSION_MIN_MACOSX / LC_VERSION_MIN_IPHONEOS / LC_VERSION_MIN_WATCHOS
    * / LC_VERSION_MIN_TVOS — all share `version_min_command` (cmd, cmdsize,
    * version, sdk). Pre-LC_BUILD_VERSION (macOS 10.13) toolchains emit these
    * on i386 binaries. They carry no vmaddr references so the M32->M64
    * transform is a verbatim copy; the stored cmd round-trips.
    */
   template <Bits bits>
   class VersionMin: public LoadCommand<bits> {
   public:
      version_min_command version_min;

      virtual uint32_t cmd() const override { return version_min.cmd; }
      virtual std::size_t size() const override { return sizeof(version_min); }
      virtual void Build(BuildEnv<bits>& env) override {
         version_min.cmdsize = size();
      }
      virtual void Emit(Image& img, std::size_t offset) const override {
         img.template at<version_min_command>(offset) = version_min;
      }

      static VersionMin<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env)
      { return new VersionMin(img, offset, env); }

      virtual VersionMin<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new VersionMin<opposite<bits>>(*this, env);
      }

   private:
      VersionMin(const Image& img, std::size_t offset, ParseEnv<bits>& env):
         LoadCommand<bits>(img, offset, env),
         version_min(img.template at<version_min_command>(offset)) {}
      VersionMin(const VersionMin<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), version_min(other.version_min) {}

      template <Bits b> friend class VersionMin;
   };

   template <Bits bits>
   class EntryPoint: public LoadCommand<bits> {
   public:
      entry_point_command entry_point;
      const Placeholder<bits> *entry = nullptr;

      virtual uint32_t cmd() const override { return entry_point.cmd; }
      virtual std::size_t size() const override { return sizeof(entry_point); }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;

      static EntryPoint<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new EntryPoint(img, offset, env);
      }
      virtual void Parse1(const Image& img, ParseEnv<bits>& env) override;

      virtual EntryPoint<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new EntryPoint<opposite<bits>>(*this, env);
      }

   private:
      EntryPoint(const EntryPoint<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      EntryPoint(const Image& img, std::size_t offset, ParseEnv<bits>& env);

      template <Bits b> friend class EntryPoint;
   };

   /*
    * LC_UNIXTHREAD — the pre-LC_MAIN form of "where do I start". The on-disk
    * layout is:
    *
    *     struct thread_command { uint32_t cmd; uint32_t cmdsize; };
    *     uint32_t flavor;       // x86_THREAD_STATE32 = 1 / x86_THREAD_STATE64 = 4
    *     uint32_t count;        // number of 32-bit words of state that follow
    *     uint32_t state[count]; // register state; eip/rip is the entry point
    *
    * We model the state as an array of 32-bit words and pull the entry point
    * (eip on i386, low half of rip on x86_64) out as a Placeholder so it
    * participates in the rebase / transform passes the same way LC_MAIN's
    * entryoff does.
    */
   template <Bits bits>
   class UnixThread: public LoadCommand<bits> {
   public:
      thread_command thread_cmd;
      uint32_t flavor;
      uint32_t count;            /* in 32-bit words */
      std::vector<uint32_t> state;
      const Placeholder<bits> *entry = nullptr;

      virtual uint32_t cmd() const override { return thread_cmd.cmd; }
      virtual std::size_t size() const override {
         return sizeof(thread_command) + 2 * sizeof(uint32_t) + state.size() * sizeof(uint32_t);
      }
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;

      static UnixThread<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new UnixThread(img, offset, env);
      }
      virtual void Parse1(const Image& img, ParseEnv<bits>& env) override;

      virtual UnixThread<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new UnixThread<opposite<bits>>(*this, env);
      }

      /* Indices into the state[] array where the program counter lives.
       *   i386 x86_THREAD_STATE32: eip is the 11th uint32 (index 10).
       *   x86_64 x86_THREAD_STATE64: rip is the 17th uint64 -> dwords 32..33.
       */
      static constexpr std::size_t pc_word_index() {
         return (bits == Bits::M32) ? 10 : 32;
      }

   private:
      UnixThread(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      UnixThread(const UnixThread<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      template <Bits b> friend class UnixThread;
   };

   template <Bits bits>
   class DylibCommand: public LoadCommand<bits> {
   public:
      dylib_command dylib_cmd;
      std::string name;
      unsigned id; /*!< dylib identifier assigned at build time */

      virtual uint32_t cmd() const override { return dylib_cmd.cmd; }            
      virtual std::size_t size() const override;
      virtual void Build(BuildEnv<bits>& env) override;
      virtual void AssignID(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      
      static DylibCommand<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new DylibCommand(img, offset, env);
      }
      
      virtual DylibCommand<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DylibCommand<opposite<bits>>(*this, env);
      }

      static DylibCommand<bits> *Create(uint32_t cmd, const std::string& name,
                                        uint32_t timestamp = 0, uint32_t current_version = 0,
                                        uint32_t compatibility_version = 0) {
         return new DylibCommand(cmd, name, timestamp, current_version, compatibility_version);
      }

   private:
      DylibCommand(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      DylibCommand(const DylibCommand<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LoadCommand<bits>(other, env), dylib_cmd(other.dylib_cmd), name(other.name), id(0) {}
      DylibCommand(uint32_t cmd, const std::string& name, uint32_t timestamp,
                   uint32_t current_version, uint32_t compatibility_version);
      template <Bits> friend class DylibCommand;
   };   
   
}
