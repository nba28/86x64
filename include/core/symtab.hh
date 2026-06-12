#pragma once

#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <set>

#include "lc.hh"
#include "types.hh"

namespace MachO {

   template <Bits bits>
   class String: public Node {
   public:
      std::string str;
      std::size_t offset; /*!< offset inside string table */

      std::size_t size() const { return str.size() + 1; }

      static String<bits> *Parse(const Image& img, std::size_t offset, std::size_t maxlen) {
         return new String(img, offset, maxlen);
      }

      /* Synthesize from a literal — used when the tool needs to inject a
       * symbol name that wasn't in the input binary (e.g. an `_main` export
       * on a stripped executable being converted to a dylib). */
      static String<bits> *Create(const std::string& s) { return new String(s); }

      void Build(BuildEnv<bits>& env);
      void Emit(Image& img, std::size_t offset) const;

      String<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new String<opposite<bits>>(*this, env);
      }

   private:
      String(const Image& img, std::size_t offset, std::size_t maxlen);
      String(const String<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);
      explicit String(const std::string& s): str(s), offset(0) {}

      template <Bits b> friend class String;
   };

   template <Bits bits>
   class Nlist: public Node {
   public:
      enum class Kind {LOCAL, EXT, UNDEF}; /*!< keep ordering! */
      enum class Type {UNDF = N_UNDF, ABS = N_ABS, SECT = N_SECT, PBUD = N_PBUD, INDR = N_INDR};
      static constexpr char MH_EXECUTE_HEADER[] = "__mh_execute_header";
      static constexpr char MH_DYLIB_HEADER[] = "__mh_dylib_header";
      static constexpr char MH_BUNDLE_HEADER[] = "__mh_bundle_header";
      static constexpr char DYLD_PRIVATE[] = "__dyld_private";

      static bool is_header_symbol(const std::string& s) {
         return s == MH_EXECUTE_HEADER || s == MH_DYLIB_HEADER || s == MH_BUNDLE_HEADER;
      }
      
      static std::size_t size() { return sizeof(nlist_t<bits>); }

      nlist_t<bits> nlist;
      const String<bits> *string = nullptr;
      const Placeholder<bits> *value = nullptr;
      const Section<bits> *section = nullptr; /* indexed by nlist.n_sect */

      static Nlist<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                                const std::unordered_map<std::size_t, String<bits> *>& off2str) {
         return new Nlist(img, offset, env, off2str);
      }

      /*
       * Synthesize a defined external symbol pointing at an existing
       * Placeholder (e.g. the program entry). `section` must be the
       * Section<bits> the symbol lives in — Nlist::Build overwrites
       * nlist.n_sect from `section->id` at emit time, so passing nullptr
       * would erase the section assignment. Defined out-of-line so we
       * don't need Placeholder's / Section's full definition here.
       */
      static Nlist<bits> *CreateDefinedExt(String<bits> *name,
                                           const Placeholder<bits> *value,
                                           const Section<bits> *section);

      void Build(BuildEnv<bits>& env);
      void Emit(Image& img, std::size_t offset) const;
      Nlist<opposite<bits>> *Transform(TransformEnv<bits>& env) const {
         return new Nlist<opposite<bits>>(*this, env);
      }

      Kind kind() const;
      Type type() const { return (Type) (nlist.n_type & N_TYPE); }

      void print(std::ostream& os) const;
      
   private:
      Nlist() : nlist{} {}
      Nlist(const Image& img, std::size_t offset, ParseEnv<bits>& env,
            const std::unordered_map<std::size_t, String<bits> *>& off2str);
      Nlist(const Nlist<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      template <Bits> friend class Nlist;
   };

   template <Bits bits>
   class Symtab: public LinkeditCommand<bits> {
   public:
      struct NlistCompare {
         bool operator()(const Nlist<bits> *lhs, const Nlist<bits> *rhs) const;
      };
      using Nlists = std::multiset<Nlist<bits> *, NlistCompare>;
      using Strings = std::list<String<bits> *>;
      
      symtab_command symtab;
      Nlists syms;
      Strings strs;
      
      virtual uint32_t cmd() const override { return symtab.cmd; }
      virtual std::size_t size() const override { return sizeof(symtab); }
      virtual std::size_t content_size() const override;
      
      virtual void Emit(Image& img, std::size_t offset) const override;

      template <typename... Args>
      static Symtab<bits> *Parse(Args&&... args) { return new Symtab(args...); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override { abort(); }
      void Build_LINKEDIT_symtab(BuildEnv<bits>& env);
      void Build_LINKEDIT_strtab(BuildEnv<bits>& env);
      
      virtual Symtab<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Symtab<opposite<bits>>(*this, env);
      }

      void remove(const std::string& name);

      void print(std::ostream& os) const;

   private:
      Symtab(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      Symtab(const Symtab<opposite<bits>>& other, TransformEnv<opposite<bits>>& env);

      template <Bits b> friend class Symtab;
   };


   template <Bits bits>
   class Dysymtab: public LinkeditCommand<bits> {
   public:
      dysymtab_command dysymtab;
      std::vector<uint32_t> indirectsyms;

      virtual uint32_t cmd() const override { return dysymtab.cmd; }      
      virtual std::size_t size() const override { return sizeof(dysymtab_command); }

      static Dysymtab<bits> *Parse(const Image& img, std::size_t offset, ParseEnv<bits>& env) {
         return new Dysymtab(img, offset, env);
      }
      
      template <typename... Args>
      static Dysymtab<bits> *Parse(Args&&... args) { return new Dysymtab(args...); }
      virtual void Build_LINKEDIT(BuildEnv<bits>& env) override;
      virtual void Emit(Image& img, std::size_t offset) const override;
      virtual std::size_t content_size() const override;
      
      virtual Dysymtab<opposite<bits>> *Transform(TransformEnv<bits>& env) const override {
         return new Dysymtab<opposite<bits>>(*this, env);
      }

   private:
      Dysymtab(const Image& img, std::size_t offset, ParseEnv<bits>& env);
      Dysymtab(const Dysymtab<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
         LinkeditCommand<bits>(other, env), dysymtab(other.dysymtab),
         indirectsyms(other.indirectsyms) {}
      template <Bits> friend class Dysymtab;
   };   
   
}
