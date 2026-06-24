#include <set>
#include <cstring>
#include <mach-o/stab.h>

#include "symtab.hh"
#include "segment.hh"
#include "transform.hh"
#include "section_blob.hh" // SectionBlob
#include "archive.hh" // Archive
#include "dyldinfo.hh" // DyldInfo (subcommand<> needs the complete type)

namespace MachO {

   template <Bits bits>
   Symtab<bits>::Symtab(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), symtab(img.at<symtab_command>(offset))
   {
      /* construct strings */
      const std::size_t strbegin = symtab.stroff;
      const std::size_t strend = strbegin + symtab.strsize;
      std::unordered_map<std::size_t, String<bits> *> off2str; // = {{0, nullptr}};
      for (std::size_t strit = strbegin /* + strnlen(&img.at<char>(strbegin), strend - strbegin) + 1 */;
           strit < strend; ) {
         const std::size_t strrem = strend - strit;

         String<bits> *str = String<bits>::Parse(img, strit, strrem);
         strs.push_back(str);
         off2str[strit - strbegin] = str;
         strit += str->size(); // NOTE: includes null byte
      }

      /* Some linkers COALESCE the string table: a symbol whose name is a
       * suffix of another's shares storage, so its n_strx points INTO the
       * middle of a longer string rather than at a string start. The linear
       * walk above only recorded string starts, so such an interior offset is
       * absent from off2str and Nlist::Parse would throw. Pre-register each
       * referenced interior offset as its own (suffix) string. The rebuild
       * emits every string separately (no coalescing), which is still valid —
       * just slightly larger. (Seen in iWork's ObjC1-fragile-ABI SFUtility.) */
      for (uint32_t i = 0; i < symtab.nsyms; ++i) {
         const auto& nl = img.template at<nlist_t<bits>>(
            symtab.symoff + i * Nlist<bits>::size());
         const std::size_t sx = nl.n_un.n_strx;
         if (off2str.find(sx) == off2str.end() && strbegin + sx < strend) {
            String<bits> *str = String<bits>::Parse(img, strbegin + sx, strend - (strbegin + sx));
            strs.push_back(str);
            off2str[sx] = str;
         }
      }

      /* construct symbols */
      for (uint32_t i = 0; i < symtab.nsyms; ++i) {
         syms.insert(Nlist<bits>::Parse(img, symtab.symoff + i * Nlist<bits>::size(), env,
                                        off2str));
      }

   }

   template <Bits bits>
   Nlist<bits>::Nlist(const Image& img, std::size_t offset, ParseEnv<bits>& env,
                      const std::unordered_map<std::size_t, String<bits> *>& off2str):
      value(nullptr)
   {
      nlist = img.at<nlist_t<bits>>(offset);
      if (off2str.find(nlist.n_un.n_strx) == off2str.end()) {
         throw error("nlist offset 0x%x does not point to beginning of string", nlist.n_un.n_strx);
      }
      string = off2str.at(nlist.n_un.n_strx);

      /*
       * n_value is a virtual address only when the symbol is defined in a
       * section (N_SECT) and not a stab/debug entry. For undefined symbols
       * (N_UNDF) it carries flags / common-block size; for absolute symbols
       * (N_ABS) it's an arbitrary constant; for stabs (N_STAB set) it's
       * debug-info specific. Adding a placeholder for any of these strands
       * a fake "address" in env.placeholders that never lines up with a
       * real blob and later fails archive-build.
       */
      const bool is_stab = (nlist.n_type & N_STAB) != 0;
      const bool is_sect = !is_stab && (nlist.n_type & N_TYPE) == N_SECT;
      if (Nlist<bits>::is_header_symbol(string->str)) {
         // __mh_{execute,dylib,bundle}_header: address points at the mach_header,
         // which has no corresponding parsed blob. Don't placeholder it.
      } else if (is_sect) {
         value = env.add_placeholder(nlist.n_value);
      }

      /* resolve section */
      if (type() == Type::SECT) {
         if (nlist.n_sect == NO_SECT) {
            throw error("symbol `%s' type is SECT but section number is NO_SECT (0)",
                        string->str.c_str());
         }
         env.section_resolver.resolve(nlist.n_sect, &section);
      }
   }

   template <Bits bits>
   Dysymtab<bits>::Dysymtab(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), dysymtab(img.at<dysymtab_command>(offset)),
      indirectsyms(std::vector<uint32_t>(&img.at<uint32_t>(dysymtab.indirectsymoff),
                                         &img.at<uint32_t>(dysymtab.indirectsymoff) +
                                         dysymtab.nindirectsyms)) {}

   template <Bits bits>
   void Symtab<bits>::Build_LINKEDIT_symtab(BuildEnv<bits>& env) {
      symtab.nsyms = syms.size();
      symtab.symoff = env.allocate(Nlist<bits>::size() * symtab.nsyms);
      
      for (auto sym : syms) {
         sym->Build(env);
      }
   }

   template <Bits bits>
   void Symtab<bits>::Build_LINKEDIT_strtab(BuildEnv<bits>& env) {
      BuildEnv<bits> strtab_env(env.archive, Location(0, 0));
      for (String<bits> *str : strs) {
         str->Build(strtab_env);
      }
      
      symtab.strsize = strtab_env.loc.offset;
      symtab.stroff = env.allocate(symtab.strsize);
   }
   
   template <Bits bits>
   String<bits>::String(const Image& img, std::size_t offset, std::size_t maxlen) {
      std::size_t slen = strnlen(&img.at<char>(offset), maxlen);
      str = std::string(&img.at<char>(offset), slen);
   }

   template <Bits bits>
   void String<bits>::Build(BuildEnv<bits>& env) {
      offset = env.allocate(size());
   }

   template <Bits bits>
   void Dysymtab<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      /* compute counts of symbol types (local, ext, undef) */
      std::size_t symindex = 0;
      auto symtab = env.archive->template subcommand<Symtab>();
      dysymtab.nlocalsym = std::count_if(symtab->syms.begin(), symtab->syms.end(), [] (auto sym) {
            return sym->kind() == Nlist<bits>::Kind::LOCAL;
         });
      dysymtab.ilocalsym = symindex;
      symindex += dysymtab.nlocalsym;

      dysymtab.nextdefsym = std::count_if(symtab->syms.begin(), symtab->syms.end(), [] (auto sym) {
            return sym->kind() == Nlist<bits>::Kind::EXT;
         });
      dysymtab.iextdefsym = symindex;
      symindex += dysymtab.nextdefsym;

      dysymtab.nundefsym = std::count_if(symtab->syms.begin(), symtab->syms.end(), [] (auto sym) {
            return sym->kind() == Nlist<bits>::Kind::UNDEF; 
         });
      dysymtab.iundefsym = symindex;
      symindex += dysymtab.nundefsym;
      
      dysymtab.indirectsymoff = env.allocate(align<bits>(sizeof(uint32_t) * indirectsyms.size()));
      dysymtab.nindirectsyms = indirectsyms.size();

      /* Zero the classic-linker static-metadata tables (toc, modtab,
       * extrefsyms) and the external relocation table. The parser preserves
       * their *off/n* fields from the i386 input, but the translator doesn't
       * relocate the actual data into the new LINKEDIT layout — so the
       * original offsets point into garbage after transform, and
       * install_name_tool's validator reports them as "table of contents at
       * offset X overlaps section contents at Y".
       *
       * Safe to drop: dyld doesn't use any of these for image loading. toc/
       * modtab/extrefsyms are static-linker / nm metadata. External relocs are
       * binds — for classic images we handle those by renaming the undefined
       * nlist + retargeting its library ordinal (macho-tool classic_symbind),
       * so dyld binds the symbol-pointer slots via the indirect symbol table;
       * no external reloc entries are needed. */
      dysymtab.tocoff = 0;          dysymtab.ntoc = 0;
      dysymtab.modtaboff = 0;       dysymtab.nmodtab = 0;
      dysymtab.extrefsymoff = 0;    dysymtab.nextrefsyms = 0;
      dysymtab.extreloff = 0;       dysymtab.nextrel = 0;

      /* LOCAL relocations = rebases. For a MODERN image (LC_DYLD_INFO present)
       * the rebase stream's opcodes carry these, so the classic locrel table
       * is redundant — drop it (the layout shift would strand the original
       * offsets anyway). But for a CLASSIC image (LC_DYSYMTAB only, NO
       * LC_DYLD_INFO — e.g. GCC-built game dylibs like Portal 2's libtier0),
       * the local relocations are the ONLY rebase mechanism. Dropping them
       * left every absolute __DATA pointer (notably the __mod_init_func
       * constructor pointers) un-rebased: dyld never slid them, so the C++
       * static initializers never ran (CThreadLocalBase's pthread_key_create
       * never fired -> garbage TLS). REGENERATE them here from the already-
       * transformed pointer blobs so dyld rebases the x86_64 output when it
       * slides. dyld's x86_64 SUPPORT_CLASSIC_RELOCS path handles VANILLA
       * pointer-sized local relocs. */
      local_relocs.clear();
      if (env.archive->template subcommand<DyldInfo>() == nullptr) {
         regenerate_local_relocs(env);
      }
      if (local_relocs.empty()) {
         dysymtab.locreloff = 0;    dysymtab.nlocrel = 0;
      } else {
         dysymtab.nlocrel = local_relocs.size();
         dysymtab.locreloff =
            env.allocate(align<bits>(local_relocs.size() * sizeof(relocation_info)));
      }
   }

   /* Build classic VANILLA local relocations (rebases) for every absolute,
    * pointer-sized __DATA pointer in the (classic) x86_64 output. The pointer
    * VALUES are already correct — DataParser / NonLazySymbolPointer detected,
    * widened (4->8) and re-resolved each to the new preferred vmaddr; the only
    * missing piece is telling dyld to add the load slide, which a local reloc
    * does. Enumerate the transformed pointer blobs (NonLazySymbolPointer with a
    * resolved internal pointee = __mod_init_func / __mod_term_func entries and
    * baked compile-time data pointers); SKIP the indirect-pointer sections
    * (S_*_SYMBOL_POINTERS), which dyld rebases/binds via the indirect symbol
    * table — a locrel there would double-rebase. */
   template <Bits bits>
   void Dysymtab<bits>::regenerate_local_relocs(BuildEnv<bits>& env) {
      /* r_address is an offset from the reloc base. dyld bases 64-bit classic
       * local relocs on the FIRST WRITABLE segment (not __TEXT) — text relocs
       * aren't permitted in 64-bit, so the base is the first __DATA-like
       * segment (see dyld ImageLoaderMachOClassic::getRelocBase /
       * MachOAnalyzer::relocBaseAddress). Using __TEXT yields r_address values
       * dyld rejects as "out of range". */
      std::size_t base = 0;
      bool have_base = false;
      for (Segment<bits> *seg : env.archive->segments()) {
         const std::string sn(seg->segment_command.segname,
                              strnlen(seg->segment_command.segname,
                                      sizeof(seg->segment_command.segname)));
         if (sn == SEG_LINKEDIT) { continue; }
         if (seg->segment_command.initprot & VM_PROT_WRITE) {
            base = seg->segment_command.vmaddr;
            have_base = true;
            break;
         }
      }
      if (!have_base) { return; }

      for (Segment<bits> *seg : env.archive->segments()) {
         const std::string segname(seg->segment_command.segname,
                                   strnlen(seg->segment_command.segname,
                                           sizeof(seg->segment_command.segname)));
         if (segname == SEG_PAGEZERO || segname == SEG_LINKEDIT) { continue; }

         for (Section<bits> *sect : seg->sections) {
            const uint32_t stype = sect->sect.flags & SECTION_TYPE;
            if (stype == S_NON_LAZY_SYMBOL_POINTERS ||
                stype == S_LAZY_SYMBOL_POINTERS) {
               continue; /* dyld rebases these via the indirect symbol table */
            }

            for (SectionBlob<bits> *blob : sect->content) {
               auto *nlp = dynamic_cast<NonLazySymbolPointer<bits> *>(blob);
               if (nlp == nullptr || nlp->pointee == nullptr) { continue; }

               relocation_info ri;
               std::memset(&ri, 0, sizeof ri);
               ri.r_address = static_cast<int32_t>(blob->loc.vmaddr - base);
               ri.r_symbolnum = 0;
               ri.r_pcrel = 0;
               ri.r_length = (bits == Bits::M64) ? 3 : 2; /* pointer size */
               ri.r_extern = 0;
               ri.r_type = GENERIC_RELOC_VANILLA; /* == X86_64_RELOC_UNSIGNED == 0 */
               local_relocs.push_back(ri);
            }
         }
      }
   }

   template <Bits bits>
   std::size_t Dysymtab<bits>::content_size() const {
      return align<bits>(indirectsyms.size() * sizeof(uint32_t)) +
             align<bits>(local_relocs.size() * sizeof(relocation_info));
   }

   template <Bits bits>
   void Symtab<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<symtab_command>(offset) = symtab;

      std::size_t symoff = symtab.symoff;
      for (const Nlist<bits> *sym : syms) {
         sym->Emit(img, symoff);
         symoff += sym->size();
      }

      std::size_t stroff = symtab.stroff;
      for (const String<bits> *str : strs) {
         str->Emit(img, stroff);
         stroff += str->size();
      }
   }

   template <Bits bits>
   void Nlist<bits>::Build(BuildEnv<bits>& env) {
      /* get text address */
      if (Nlist<bits>::is_header_symbol(string->str)) {
         nlist.n_value = env.archive->segment(SEG_TEXT)->loc().vmaddr;
      }

      /* get section ID */
      nlist.n_sect = section ? section->id : NO_SECT;
   }

   template <Bits bits>
   void Nlist<bits>::Emit(Image& img, std::size_t offset) const {
      nlist_t<bits> nlist = this->nlist;
      /*
       * Guard against a null `string` pointer. The Nlist M32→M64 copy
       * ctor uses `env.resolve(other.string, &string)` async; if the
       * M32 String wasn't itself transformed (which can happen if the
       * Symtab transform skipped some strs, or env.resolve missed the
       * callback), `string` stays null and the deref below SIGSEGVs.
       * Mid-Emit SIGSEGV truncates the output file and leaves every
       * later LC header zeroed. Emit n_strx=0 (the empty string in
       * the string table) as a defensive fallback with a warning.
       */
      if (string) {
         nlist.n_un.n_strx = string->offset;
      } else {
         static int null_string_count = 0;
         if (null_string_count++ < 10) {
            fprintf(stderr,
                    "warning: Nlist::Emit: null string pointer, "
                    "n_type=0x%x n_sect=%u — using strx=0\n",
                    (unsigned)nlist.n_type, (unsigned)nlist.n_sect);
         }
         nlist.n_un.n_strx = 0;
      }

      if (value) {
         nlist.n_value = value->loc.vmaddr;
      }

      img.at<nlist_t<bits>>(offset) = nlist;
   }
   
   template <Bits bits>
   void String<bits>::Emit(Image& img, std::size_t offset) const {
      img.copy(offset, str.c_str(), str.size() + 1);
      // memcpy(&img.at<char>(offset), str.c_str(), str.size() + 1);
   }

   template <Bits bits>
   void Dysymtab<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dysymtab_command>(offset) = dysymtab;
      img.copy(dysymtab.indirectsymoff, indirectsyms.begin(), indirectsyms.size());
      if (!local_relocs.empty()) {
         img.copy(dysymtab.locreloff, local_relocs.begin(), local_relocs.size());
      }
   }

   template <Bits bits>
   std::size_t Symtab<bits>::content_size() const {
      std::size_t size = Nlist<bits>::size() * syms.size();
      for (const String<bits> *str : strs) {
         size += str->size();
      }
      return align<bits>(size);
   }

   template <Bits bits>
   Symtab<bits>::Symtab(const Symtab<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      LinkeditCommand<bits>(other, env), symtab(other.symtab)
   {
      for (const auto sym : other.syms) {
         if (sym->string->str != Nlist<bits>::DYLD_PRIVATE || 1) {
            syms.insert(sym->Transform(env));
         }
      }

      for (const auto str : other.strs) {
         if (str->str != Nlist<bits>::DYLD_PRIVATE || 1) {
            strs.push_back(str->Transform(env));
         }
      }
   }

   template <Bits bits>
   Nlist<bits>::Nlist(const Nlist<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      string(nullptr), value(nullptr)
   {
      env(other.nlist, nlist);
      env.resolve(other.string, &string);

      if (Nlist<bits>::is_header_symbol(other.string->str)) {
         // value remains null
      } else {
         env.resolve(other.value, &value);
      }

      /* resolve section */
      env.resolve(other.section, &section);
   }
   
   template <Bits bits>
   String<bits>::String(const String<opposite<bits>>& other, TransformEnv<opposite<bits>>& env):
      str(other.str), offset(0)
   {
      env.add(&other, this);
   }

   template <Bits bits>
   Nlist<bits> *Nlist<bits>::CreateDefinedExt(String<bits> *name,
                                              const Placeholder<bits> *value,
                                              const Section<bits> *section) {
      auto *self = new Nlist();
      self->string = name;
      self->value = value;
      self->section = section;             /* Build reads section->id at emit */
      self->nlist.n_un.n_strx = 0;         /* set at Build time */
      self->nlist.n_type = N_SECT | N_EXT;
      self->nlist.n_sect = 0;              /* overwritten by Build from section->id */
      self->nlist.n_desc = 0;
      self->nlist.n_value = value ? value->loc.vmaddr : 0;
      return self;
   }

   template <Bits bits>
   void Symtab<bits>::remove(const std::string& name) {
      /* The old loop did `++it` after `erase(it)`, which is UB once erase
       * has invalidated `it` (it crashes immediately on libc++ sets).
       * Use erase's return value to advance instead. */
      for (auto it = syms.begin(); it != syms.end(); /* see body */) {
         if ((*it)->string->str == name) {
            it = syms.erase(it);
         } else {
            ++it;
         }
      }
      strs.remove_if([&] (auto str) { return name == str->str; });
   }

   template <Bits bits>
   typename Nlist<bits>::Kind Nlist<bits>::kind() const {
      if ((nlist.n_type & N_EXT) == 0) {
         return Kind::LOCAL;
      } else if ((nlist.n_type & N_TYPE) == N_UNDF) {
         return Kind::UNDEF;
      } else {
         return Kind::EXT;
      }
   }
   
   template <Bits bits>
   bool Symtab<bits>::NlistCompare::operator()(const Nlist<bits> *lhs,
                                               const Nlist<bits> *rhs) const {
      return (int) lhs->kind() < (int) rhs->kind();
   }

   template <Bits bits>
   void Symtab<bits>::print(std::ostream& os) const {
      os << "nsyms=" << syms.size() << std::endl;
      
      /* print column names */
      os << "value type external desc section string" << std::endl;
      
      for (auto sym : syms) {
         sym->print(os);
         os << std::endl;
      }
   }

   template <Bits bits>
   void Nlist<bits>::print(std::ostream& os) const {
      /* value */
      os << (value ? value->loc.vmaddr : nlist.n_value) << " ";

      /* type */
      const std::unordered_map<uint8_t, const char *> type2str =
         {{N_UNDF, "UNDF"},
          {N_ABS,  "ABS"},
          {N_SECT, "SECT"},
          {N_PBUD, "PBUD"},
          {N_INDR, "INDR"}};
      const auto type_it = type2str.find(nlist.n_type & N_TYPE);
      os << (type_it != type2str.end() ? type_it->second : "?") << " ";

      /* external? */
      os << (nlist.n_type & N_EXT ? 'x' : '-') << " ";

      /* stab info? */
      if ((nlist.n_type & N_EXT)) {
         os << nlist.n_desc;
      } else {
         os << "-";
      }
      os << " ";

      /* section */
      switch (nlist.n_type & N_TYPE) {
      case N_UNDF:
      case N_ABS:
      case N_PBUD:
      case N_INDR:
         os << "-";
         break;
      case N_SECT:
         os << section->segment->name() << "," << section->name() << " ";
         break;
      default:
         os << "-";
      }
      os << " ";
      
      os << string->str;
   }
   
   template class Symtab<Bits::M32>;
   template class Symtab<Bits::M64>;
   
   template class Nlist<Bits::M32>;
   template class Nlist<Bits::M64>;

   template class Dysymtab<Bits::M32>;
   template class Dysymtab<Bits::M64>;
   
}
