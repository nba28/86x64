#pragma once

#include <optional>

#include "modify.hh"

struct ModifyCommand::Update: Subcommand {
   struct LoadDylib;
   struct BindNode;
   struct StripBind; 

   virtual std::vector<char *> keylist() const override {
      return {"load_dylib", "load-dylib",
              "bind",
              "strip-bind",
              nullptr};
   }

   virtual Operation *getop(int index) override;
};

struct ModifyCommand::Update::LoadDylib: Operation {
   std::optional<unsigned> lc;
   std::optional<std::string> name;
   std::optional<uint32_t> timestamp;
   std::optional<uint32_t> current_version;
   std::optional<uint32_t> compatibility_version;

   virtual std::vector<char *> keylist() const override {
      return {"name", "timestamp", "current_version", "compatibility_version", "lc", nullptr};
   }
   virtual int subopthandler(int index, char *value) override;
   virtual void validate() const override;
   virtual void operator()(MachO::MachO *macho) override;
   template <MachO::Bits b> void workT(MachO::Archive<b> *archive);
};

struct ModifyCommand::Update::BindNode: Operation {
   std::optional<std::string> old_sym;
   bool lazy = false;
   bool optional = false;   /* if set, silently skip when old_sym is absent
                             * from the selected (lazy/non-lazy) bind table,
                             * instead of throwing. Lets a caller redirect a
                             * symbol that lives in only one of the two tables
                             * by issuing both updates. */
   bool weak = false;       /* if set, operate on the weak_bind table instead
                             * of the regular bind table (mutually exclusive
                             * with `lazy' — there is no lazy weak_bind table).
                             * A weak bind carries no dylib ordinal (implicit
                             * BIND_SPECIAL_DYLIB_WEAK_LOOKUP), so only new_sym
                             * is meaningful — new_dylib is ignored. C++ runtime
                             * weak-coalesced externals (operator new/delete:
                             * __Znwm/__Znam/__ZdlPv/__ZdaPv) are bound via this
                             * table; without rewriting it dyld's weak coalescing
                             * OVERWRITES the regular-bind redirect with the
                             * NATIVE definition (the __la_symbol_ptr slot the
                             * regular bind aimed at libabiconv's ____Znwm gets
                             * re-resolved to native libstdc++ __Znwm) -> the
                             * call reaches native code UNROUTED -> the i386
                             * 4-byte ret is over-popped by the native 8-byte
                             * ret -> fused PC crash. Renaming the weak_bind
                             * symbol to the shim name keeps the coalesced
                             * lookup resolving to libabiconv (its only
                             * definition). */

   std::optional<uint8_t> new_type;
   std::optional<ssize_t> new_addend;
   std::optional<unsigned> new_dylib_ord;
   std::optional<std::string> new_sym;
   std::optional<uint8_t> new_flags;
   // std::optional<std::size_t> new_vmaddr;

   virtual std::vector<char *> keylist() const override {
      return {"old_sym", "old-sym",
              "new_type", "new-type",
              "new_dylib", "new-dylib",
              "new_sym", "new-sym",
              "new_flags", "new-flags",
              "lazy",
              "optional",
              "weak",
              nullptr};
   }
   virtual int subopthandler(int index, char *value) override;
   virtual void validate() const override;
   virtual void operator()(MachO::MachO *macho) override;
   template <MachO::Bits b, bool lazy> void workT(MachO::Archive<b> *archive);

   /* Classic (pre-10.6, LC_DYSYMTAB-only) Mach-O has no dyld_info bind
    * opcode stream — undefined imports are resolved by dyld from the symbol
    * table + indirect symbol table, keyed by (name, two-level library
    * ordinal). Redirect such a bind by renaming the undefined nlist to
    * `new_sym` and retargeting its library ordinal to `new_dylib_ord`;
    * every la/nl_symbol_ptr slot indexing that nlist then binds to the
    * shim. Used when no DyldInfo is present. */
   template <MachO::Bits b> void classic_symbind(MachO::Archive<b> *archive);
};

struct ModifyCommand::Update::StripBind: Operation {
   std::list<std::string> suffixes;
   
   virtual std::vector<char *> keylist() const override {
      return {"suffix", nullptr};
   }
   virtual int subopthandler(int index, char *value) override;
   virtual void validate() const override {}
   virtual void operator()(MachO::MachO *macho) override;
   template <MachO::Bits b> void workT(MachO::Archive<b> *archive);
   template <MachO::Bits b, bool lazy> void workT(MachO::BindInfo<b, lazy> *bind_info);
   void strip(std::string& s) const;
};
