#pragma once

#include <mach-o/loader.h>
#include <vector>
#include <list>

#include "lc.hh"
#include "segment.hh"
#include "parse.hh"
#include "types.hh"

namespace MachO {

   template <Bits bits>
   class DyldInfo: public LinkeditCommand<bits> {
   public:
      dyld_info_command dyld_info;

      RebaseInfo<bits> *rebase;
      BindInfo<bits, false> *bind;
      /* weak_bind is now translated like bind/lazy_bind (was a raw opcode
       * blob copied verbatim, whose i386 4-byte segment offsets/strides were
       * applied to the 8-byte-widened x86_64 layout → misaligned writes →
       * a C++ weak-coalesced slot held def<<32 → SIGBUS in dyld applyFixups).
       * A weak bind carries no dylib ordinal (implicit BIND_SPECIAL_DYLIB_
       * WEAK_LOOKUP, -3) so it is a BindInfo in `weak` mode: no SET_DYLIB. */
      BindInfo<bits, false> *weak_bind;
      // std::vector<uint8_t> lazy_bind;
      BindInfo<bits, true> *lazy_bind;
      ExportInfo<bits> *export_info;

      virtual uint32_t cmd() const override { return dyld_info.cmd; }
      virtual std::size_t size() const override { return sizeof(dyld_info); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual std::size_t content_size() const override;
      
      static DyldInfo<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new DyldInfo(img, offset, env);
      }

      /* Synthesize an LC_DYLD_INFO_ONLY from already-built stream objects. Used
       * to manufacture a modern bind/rebase/export representation for a CLASSIC
       * (LC_DYSYMTAB-only) image — see Archive::synthesize_dyld_info. The
       * dataoff/size fields are filled in at Build_LINKEDIT. */
      static DyldInfo<bits> *Create(RebaseInfo<bits> *rebase, BindInfo<bits, false> *bind,
                                    BindInfo<bits, false> *weak_bind,
                                    BindInfo<bits, true> *lazy_bind,
                                    ExportInfo<bits> *export_info);

      virtual DyldInfo<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new DyldInfo<opposite<bits>>(*this, env);
      }

   private:
      DyldInfo() {}
      DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      DyldInfo(const DyldInfo<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditCommand<bits>(other, env),
         dyld_info(other.dyld_info),
         rebase(other.rebase->Transform(env)),
         bind(other.bind->Transform(env)),
         weak_bind(other.weak_bind->Transform(env)),
         lazy_bind(other.lazy_bind->Transform(env)),
         export_info(other.export_info->Transform(env)) {}

      template <Bits b> friend class DyldInfo;
   };

   template <Bits bits, bool lazy>
   class BindNode: public Node {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      
      uint8_t type;
      ssize_t addend;
      const DylibCommand<bits> *dylib;
      /* Non-zero ⇒ this bind targets a "special" pseudo-dylib (flat lookup,
       * main executable, weak lookup, self). Holds the 4-bit imm field of
       * BIND_OPCODE_SET_DYLIB_SPECIAL_IMM. When non-zero, `dylib` is null
       * and Emit uses SET_DYLIB_SPECIAL_IMM instead of SET_DYLIB_ORDINAL_IMM.
       * Used by Carbon/QuickTime-era frameworks (NavigationServices) for
       * flat-namespace symbol resolution. */
      int8_t dylib_special;
      std::string sym;
      uint8_t flags;
      const SectionBlob<bits> *blob;
      uint32_t index;
      /* weak_bind entry: no dylib ordinal (implicit weak lookup, -3); emit no
       * SET_DYLIB opcode and skip dylib resolution (dylib stays null). */
      bool weak = false;

      std::size_t size() const;
      std::size_t dylib_opcode_size() const;
      std::size_t emit_dylib_opcode(Image& img, std::size_t offset) const;
      void Emit(Image& img, std::size_t offset) const;
      bool active() const { return blob == nullptr ? false : blob->active; }
      bool emittable() const;

      static BindNode<bits, lazy> *Parse(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type,
                                         ssize_t addend, std::size_t dylib, int8_t dylib_special,
                                         const char *sym, uint8_t flags, uint32_t index,
                                         bool weak = false) {
         return new BindNode(vmaddr, env, type, addend, dylib, dylib_special, sym, flags, index,
                             weak);
      }

      /* Synthesize a bind node pointing at an already-resolved slot blob (used
       * when manufacturing an LC_DYLD_INFO bind stream for a classic image —
       * Archive::synthesize_dyld_info). `dylib` is the resolved DylibCommand
       * (null for special/weak); `blob` is the slot whose post-Build loc.vmaddr
       * is the bind site. */
      static BindNode<bits, lazy> *Create(uint8_t type, ssize_t addend,
                                          const DylibCommand<bits> *dylib, int8_t dylib_special,
                                          const std::string& sym, uint8_t flags,
                                          const SectionBlob<bits> *blob, bool weak = false) {
         auto *node = new BindNode();
         node->type = type;
         node->addend = addend;
         node->dylib = dylib;
         node->dylib_special = dylib_special;
         node->sym = sym;
         node->flags = flags;
         node->blob = blob;
         node->index = 0;
         node->weak = weak;
         return node;
      }

      void Build(BuildEnv<bits>& env);

      BindNode<opposite<bits>, lazy> *Transform(TransformEnv<bits>& env) const {
         return new BindNode<opposite<bits>, lazy>(*this, env);
      }

      void print(std::ostream& os) const;
      
   private:
      BindNode() {} /*!< for Create (synthesized binds) */
      BindNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
               std::size_t dylib, int8_t dylib_special, const char *sym, uint8_t flags,
               uint32_t index, bool weak);
      BindNode(const BindNode<opposite<bits>, lazy>& other, TransformEnv<opposite<bits>>& env);
      template <Bits, bool> friend class BindNode;
   };

   template <Bits bits, bool lazy>
   class BindInfo {
   public:
      using ptr_t = select_type<bits, uint32_t, uint64_t>;
      using Bindees = std::list<BindNode<bits, lazy> *>;

      Bindees bindees;
      /* weak_bind table: emit no SET_DYLIB opcodes for any node. */
      bool weak = false;

      std::size_t size() const;
      void Emit(Image& img, std::size_t offset) const;

      static BindInfo<bits, lazy> *Parse(const Image& img, std::size_t offset, std::size_t size,
                                   ParseEnv<bits>& env, bool weak = false)
      { return new BindInfo(img, offset, size, env, weak); }

      /* Synthesize an empty bind table (caller appends BindNode::Create nodes).
       * Used to manufacture a classic image's LC_DYLD_INFO bind streams. */
      static BindInfo<bits, lazy> *Create(bool weak = false) {
         auto *info = new BindInfo();
         info->weak = weak;
         return info;
      }

      BindInfo<opposite<bits>, lazy> *Transform(TransformEnv<bits>& env) const {
         return new BindInfo<opposite<bits>, lazy>(*this, env);
      }

      void Build(BuildEnv<bits>& env);

      typename Bindees::iterator find(const std::string& sym) {
         return std::find_if(begin(), end(), [&] (auto node) { return node->sym == sym; });
      }
      typename Bindees::iterator begin() { return bindees.begin(); }
      typename Bindees::iterator end() { return bindees.end(); }

      void print(std::ostream& os) const;

   private:
      BindInfo() {} /*!< for Create (synthesized bind table) */
      BindInfo(const Image& img, std::size_t offset, std::size_t size, ParseEnv<bits>& env,
               bool weak = false);
      BindInfo(const BindInfo<opposite<bits>, lazy>& other, TransformEnv<opposite<bits>>& env);
      
      std::size_t do_bind(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type, ssize_t addend,
                          std::size_t dylib, int8_t dylib_special, const char *sym, uint8_t flags,
                          uint32_t index);
      std::size_t do_bind_times(std::size_t count, std::size_t vmaddr, ParseEnv<bits>& env,
                                uint8_t type, ssize_t addend, std::size_t dylib,
                                int8_t dylib_special, const char *sym, uint8_t flags,
                                uint32_t index, ptr_t skipping = 0);
      template <Bits, bool> friend class BindInfo;
   };

}
