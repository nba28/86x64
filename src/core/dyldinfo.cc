#include <cstring>

#include "dyldinfo.hh"
#include "image.hh"
#include "leb.hh"
#include "archive.hh"
#include "section_blob.hh" // Immediate
#include "export_info.hh" // ExportInfo
#include "rebase_info.hh" // RebaseInfo

namespace MachO {

   template <Bits bits>
   DyldInfo<bits>::DyldInfo(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditCommand<bits>(img, offset, env), dyld_info(img.at<dyld_info_command>(offset)),
      rebase(RebaseInfo<bits>::Parse(img, dyld_info.rebase_off, dyld_info.rebase_size, env)),
      bind(BindInfo<bits, false>::Parse(img, dyld_info.bind_off, dyld_info.bind_size, env)),
      weak_bind(BindInfo<bits, false>::Parse(img, dyld_info.weak_bind_off,
                                             dyld_info.weak_bind_size, env, /*weak=*/true)),
      lazy_bind(BindInfo<bits, true>::Parse(img, dyld_info.lazy_bind_off, dyld_info.lazy_bind_size,
                                            env)),
      export_info(ExportInfo<bits>::Parse(img, dyld_info.export_off, dyld_info.export_size, env))
   {}


   template <Bits bits, bool lazy>
   BindInfo<bits, lazy>::BindInfo(const Image& img, std::size_t offset, std::size_t size,
                                  ParseEnv<bits>& env, bool weak): weak(weak) {
      const std::size_t begin = offset;
      const std::size_t end = begin + size;
      std::size_t it = begin;
      std::size_t vmaddr = 0;
      uint8_t type = 0;
      std::size_t dylib = 0;
      int8_t dylib_special = 0;
      const char *sym = nullptr;
      uint8_t flags = 0;
      std::size_t addend = 0;
      uint32_t index = 0;
               
      while (it != end) {
         const uint8_t byte = img.at<uint8_t>(it);
         const uint8_t opcode = byte & BIND_OPCODE_MASK;
         const uint8_t imm = byte & BIND_IMMEDIATE_MASK;

         ++it;

         std::size_t uleb, uleb2;

         switch (opcode) {
         case BIND_OPCODE_DONE:
         case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
         case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
         case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
         case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
         case BIND_OPCODE_SET_TYPE_IMM:
         case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
         case BIND_OPCODE_DO_BIND:
            break;
            
         case BIND_OPCODE_THREADED:
         case BIND_OPCODE_SET_ADDEND_SLEB:
         case BIND_OPCODE_ADD_ADDR_ULEB:
         case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
         case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
         case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            if constexpr (lazy) {
            throw error("%s: invalid lazy bind opcode 0x%hhx", __FUNCTION__, opcode);
               } else {
               break;
            }
            
         default:
            throw error("%s: invalid bind opcode 0x%x", __FUNCTION__, opcode);
         }

         switch (opcode) {
         case BIND_OPCODE_DONE:
            dylib = 0;
            dylib_special = 0;
            addend = 0;
            flags = 0;
            type = 0;
            index = it - begin;
            break;
            // return;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_IMM:
            dylib = imm;
            dylib_special = 0;
            break;

         case BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB:
            /* Ordinal is the ULEB value, NOT the imm bits — imm is 0 for the
             * ULEB form (encoder switches to ULEB only when ordinal exceeds
             * the 4-bit imm range, i.e. > 15). The original `dylib = imm`
             * silently dropped every bind targeting dylibs with ordinals
             * 16+: CountResolver::resolve(0,&dylib) found no key 0, dylib
             * stayed nullptr, BindNode::emittable() returned false, the
             * bind was omitted from the M64 output. iPhoto with 53 LC_LOAD
             * entries lost roughly half its binds to this. */
            it += leb128_decode(img, it, uleb);
            dylib = uleb;
            dylib_special = 0;
            break;

         case BIND_OPCODE_SET_DYLIB_SPECIAL_IMM:
            /* Sign-extend the 4-bit imm: -1 = main_executable, -2 = flat_lookup,
             * -3 = weak_lookup, 0 = self. Stored in dylib_special; dylib stays 0. */
            dylib = 0;
            dylib_special = (imm & 0x8) ? (int8_t)(imm | 0xF0) : (int8_t)imm;
            break;

         case BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM:
            flags = imm;
            sym = &img.at<char>(it);
            it += strnlen(sym, end - it) + 1;
            break;

         case BIND_OPCODE_SET_TYPE_IMM:
            type = imm;
            break;

         case BIND_OPCODE_SET_ADDEND_SLEB:
            it += leb128_decode(img, it, addend);
            break;

         case BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            it += leb128_decode(img, it, vmaddr);
            vmaddr += env.archive.segment(imm)->segment_command.vmaddr;
            break;

         case BIND_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr += uleb;
            break;

         case BIND_OPCODE_DO_BIND:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, dylib_special, sym, flags, index);
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, dylib_special, sym, flags, index);
            it += leb128_decode(img, it, uleb);
            vmaddr += uleb;
            break;

         case BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED:
            vmaddr = do_bind(vmaddr, env, type, addend, dylib, dylib_special, sym, flags, index);
            vmaddr += imm * sizeof(ptr_t);
            break;

         case BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(img, it, uleb);   /* count   */
            it += leb128_decode(img, it, uleb2);  /* skipping */
            /* uleb2 is the per-iteration SKIP, and must be passed as the
             * `skipping` argument. It was previously passed positionally as
             * `index`, leaving `skipping` at its default 0 — so every
             * TIMES_SKIPPING bind advanced by only sizeof(ptr_t) and bound
             * EVERY pointer slot instead of every (ptr+skip)-th. For a
             * strided array like __cfstring (16-byte records, skip=12) this
             * bound ___CFConstantStringClassReference into all four 4-byte
             * slots of every record, smearing the whole section with the
             * class pointer and corrupting every constant @"..." string. */
            vmaddr = do_bind_times(uleb, vmaddr, env, type, addend, dylib, dylib_special, sym,
                                   flags, index, uleb2);
            break;

         case BIND_OPCODE_THREADED: {
            /* Chained-fixup sub-opcode follows in the imm bits. The imm
             * encodes a sub-action (set table size or apply). We don't
             * currently translate threaded binds — skip the sub-opcode
             * payload to keep parsing in sync rather than throw. Translated
             * binary won't have these binds applied; affects only frameworks
             * that genuinely use chained fixups, which most pre-Big Sur
             * binaries don't. */
            const uint8_t subopcode = imm;
            if (subopcode == BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB) {
               std::size_t uleb_skip;
               it += leb128_decode(img, it, uleb_skip);
            }
            /* BIND_SUBOPCODE_THREADED_APPLY consumes no extra bytes. */
            break;
         }
         }
      }
   }
      
   template <Bits bits, bool lazy>
   std::size_t BindInfo<bits, lazy>::do_bind(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type,
                                             ssize_t addend, std::size_t dylib,
                                             int8_t dylib_special, const char *sym,
                                             uint8_t flags, uint32_t index) {
      if (vmaddr != 0 && sym != nullptr) {
         bindees.push_back(BindNode<bits, lazy>::Parse(vmaddr, env, type, addend, dylib,
                                                       dylib_special, sym, flags, index, weak));
      }
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits, bool lazy>
   std::size_t BindInfo<bits, lazy>::do_bind_times(std::size_t count, std::size_t vmaddr,
                                                   ParseEnv<bits>& env, uint8_t type,
                                                   ssize_t addend, std::size_t dylib,
                                                   int8_t dylib_special, const char *sym,
                                                   uint8_t flags, uint32_t index,
                                                   ptr_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         vmaddr = do_bind(vmaddr, env, type, addend, dylib, dylib_special, sym, flags, index);
         vmaddr += skipping;
      }
      return vmaddr;
   }

   template <Bits bits, bool lazy>
   BindNode<bits, lazy>::BindNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type,
                                  ssize_t addend, std::size_t dylib, int8_t dylib_special,
                                  const char *sym, uint8_t flags, uint32_t index, bool weak):
      type(type), addend(addend), dylib(nullptr), dylib_special(dylib_special), sym(sym),
      flags(flags), blob(nullptr), index(index), weak(weak)
   {
      env.vmaddr_resolver.resolve(vmaddr, &blob);
      /* weak binds carry no dylib ordinal (implicit weak lookup), so there is
       * nothing to resolve and nothing to emit for the dylib. */
      if (dylib_special == 0 && !weak) {
         env.dylib_resolver.resolve(dylib, &this->dylib);
      }
      if constexpr (lazy) {
            env.lazy_bind_node_resolver.add(index, this);
         }
   }

   template <Bits bits>
   DyldInfo<bits> *DyldInfo<bits>::Create(RebaseInfo<bits> *rebase, BindInfo<bits, false> *bind,
                                          BindInfo<bits, false> *weak_bind,
                                          BindInfo<bits, true> *lazy_bind,
                                          ExportInfo<bits> *export_info) {
      auto *self = new DyldInfo();
      std::memset(&self->dyld_info, 0, sizeof(self->dyld_info));
      self->dyld_info.cmd = LC_DYLD_INFO_ONLY;
      self->dyld_info.cmdsize = sizeof(self->dyld_info);
      self->rebase = rebase;
      self->bind = bind;
      self->weak_bind = weak_bind;
      self->lazy_bind = lazy_bind;
      self->export_info = export_info;
      return self;
   }

   template <Bits bits>
   void DyldInfo<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      dyld_info.rebase_size = rebase->size();
      dyld_info.rebase_off = env.allocate(dyld_info.rebase_size);

      dyld_info.bind_size = bind->size();
      dyld_info.bind_off = env.allocate(dyld_info.bind_size);
      bind->Build(env);

      dyld_info.weak_bind_size = weak_bind->size();
      dyld_info.weak_bind_off = env.allocate(dyld_info.weak_bind_size);
      weak_bind->Build(env);

      dyld_info.lazy_bind_size = align<bits>(lazy_bind->size());
      dyld_info.lazy_bind_off = env.allocate(dyld_info.lazy_bind_size);
      lazy_bind->Build(env);

      dyld_info.export_size = align<bits>(export_info->size());
      dyld_info.export_off = env.allocate(dyld_info.export_size);
   }

   template <Bits bits, bool lazy>
   std::size_t BindInfo<bits, lazy>::size() const {
      std::size_t size = 0;
      for (const BindNode<bits, lazy> *node : bindees) {
         size += node->size();
      }
      if (size > 0) {
         ++size;
      }
      return align<bits>(size);
   }

   template <Bits bits, bool lazy>
   bool BindNode<bits, lazy>::emittable() const {
      /* All preconditions for safely emitting this bind opcode. size()
       * and Emit() must agree (any mismatch silently corrupts every
       * later LC), so they share this single predicate. */
      if (!active()) return false;
      if (blob->segment == nullptr) return false;
      /* weak binds have neither a real dylib nor a special ordinal; they are
       * still emittable (implicit weak lookup, no SET_DYLIB opcode). */
      if (!weak && dylib_special == 0 && dylib == nullptr) return false;
      if constexpr (bits == Bits::M64) {
         /* x86_64 W^X: __TEXT pages are RX, dyld can't write a bind
          * target there ("KERN_PROTECTION_FAILURE" SIGBUS during
          * dyld4::Loader::applyFixupsGeneric). The i386 binaries we
          * translate sometimes have bind entries targeting __TEXT
          * (e.g. __unwind_info pointer slots, lazy-stub patch sites);
          * the M32→M64 rewriter has already eliminated the actual
          * dependency on writing there, so drop the bind. Parallel
          * to RebaseNode::emittable(). */
         if (strcmp(blob->segment->segment_command.segname, SEG_TEXT) == 0) {
            return false;
         }
      }
      return true;
   }

   template <Bits bits, bool lazy>
   std::size_t BindNode<bits, lazy>::dylib_opcode_size() const {
      /* SPECIAL ordinals (main_exec/flat/weak/self) fit in the 4-bit imm,
       * so 1 byte. Real dylib ordinals: 1-15 fit in the IMM form (1 byte);
       * 16+ require SET_DYLIB_ORDINAL_ULEB (1 opcode byte + ULEB payload).
       * Failing to switch encoding form would OR the ordinal into the
       * opcode bits, corrupting it into SET_DYLIB_SPECIAL_IMM with an
       * invalid signed-extended ordinal (dyld_info reports "unknown library
       * special ordinal (-15)" etc.). */
      if (weak) return 0; /* weak binds emit no SET_DYLIB opcode (dylib is null) */
      if (dylib_special != 0) return 1;
      if (dylib->id <= 15) return 1;
      return 1 + leb128_size(dylib->id);
   }

   template <Bits bits, bool lazy>
   std::size_t BindNode<bits, lazy>::size() const {
      if (!emittable()) { return 0; }

      if (!lazy) {
         /* NON-LAZY
          * d   BIND_OPCODE_SET_DYLIB_{ORDINAL_IMM, ORDINAL_ULEB, SPECIAL_IMM}
          * 1   BIND_OPCODE_SET_TYPE_IMM
          * 1+a BIND_OPCODE_SET_ADDEND_SLEB
          * 1+b BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
          * 1+c BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
          * 1   BIND_OPCODE_DO_BIND
          */
         return
            dylib_opcode_size() +
            1 +
            (1 + leb128_size(addend)) +
            /* vmaddr delta, NOT file-offset delta: Emit() encodes
             * `loc.vmaddr - segment->loc().vmaddr`, and the two diverge
             * in segments with zerofill content. A mismatch that
             * straddles a LEB128 length boundary would corrupt every
             * later LC. */
            (1 + leb128_size(blob->loc.vmaddr - blob->segment->loc().vmaddr)) +
            (1 + (sym.size() + 1)) +
            1;
      } else {
         /* LAZY
          * 1+a BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
          * d   BIND_OPCODE_SET_DYLIB_{ORDINAL_IMM, ORDINAL_ULEB, SPECIAL_IMM}
          * 1+c BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
          * 1   BIND_OPCODE_DO_BIND
          * 1   BIND_OPCODE_DONE
          */
         return
            /* vmaddr delta to match Emit() — see non-lazy case above. */
            (1 + leb128_size(blob->loc.vmaddr - blob->segment->loc().vmaddr)) +
            dylib_opcode_size() +
            (1 + (sym.size() + 1)) +
            1 +
            1;
      }
   }

   template <Bits bits>
   void DyldInfo<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<dyld_info_command>(offset) = dyld_info;

      rebase->Emit(img, dyld_info.rebase_off);
      bind->Emit(img, dyld_info.bind_off);
      weak_bind->Emit(img, dyld_info.weak_bind_off);
      lazy_bind->Emit(img, dyld_info.lazy_bind_off);
      export_info->Emit(img, dyld_info.export_off);
   }

   template <Bits bits, bool lazy>
   void BindInfo<bits, lazy>::Emit(Image& img, std::size_t offset) const {
      for (BindNode<bits, lazy> *bindee : bindees) {
         bindee->Emit(img, offset);
         offset += bindee->size();
      }
      if (!bindees.empty()) {
         img.at<uint8_t>(offset) = BIND_OPCODE_DONE;
      }
   }

   template <Bits bits, bool lazy>
   std::size_t BindNode<bits, lazy>::emit_dylib_opcode(Image& img, std::size_t offset) const {
      /* Three valid encodings for "which dylib to bind against":
       *   SPECIAL_IMM:   opcode 0x30, imm = sign-extended 4-bit special ordinal
       *                  (-1=main_exec, -2=flat, -3=weak, 0=self). Always 1 byte.
       *   ORDINAL_IMM:   opcode 0x10, imm = ordinal in [1, 15]. 1 byte.
       *   ORDINAL_ULEB:  opcode 0x20, followed by ULEB(ordinal). For ordinals 16+.
       * Picking the wrong form for ordinal 16+ (i.e. OR-ing into IMM) silently
       * corrupts the opcode byte — high bits leak into next opcode's nibble,
       * dyld_info reports "unknown library special ordinal (-15)" etc. */
      if (weak) return 0; /* weak binds emit no SET_DYLIB opcode (dylib is null) */
      if (dylib_special != 0) {
         img.at<uint8_t>(offset) = (uint8_t)(BIND_OPCODE_SET_DYLIB_SPECIAL_IMM |
                                             (dylib_special & 0xF));
         return 1;
      }
      if (dylib->id <= 15) {
         img.at<uint8_t>(offset) = (uint8_t)(BIND_OPCODE_SET_DYLIB_ORDINAL_IMM |
                                             (dylib->id & 0xF));
         return 1;
      }
      img.at<uint8_t>(offset) = (uint8_t)BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB;
      return 1 + leb128_encode(img, offset + 1, dylib->id);
   }

   template <Bits bits, bool lazy>
   void BindNode<bits, lazy>::Emit(Image& img, std::size_t offset) const {
      /* emittable() folds active(), null-guards, and the M64 __TEXT
       * filter into one predicate that size() and Emit() share. Any
       * divergence here corrupts byte counts and zeroes later LCs. */
      if (!emittable()) {
         return;
      }

      if (!lazy) {
         /* BIND_OPCODE_SET_DYLIB_{ORDINAL_IMM, ORDINAL_ULEB, SPECIAL_IMM}
          * BIND_OPCODE_SET_TYPE_IMM
          * BIND_OPCODE_SET_ADDEND_SLEB
          * BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
          * BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM
          * BIND_OPCODE_DO_BIND
          */
         offset += emit_dylib_opcode(img, offset);
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_TYPE_IMM | type;

         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_ADDEND_SLEB;
         offset += leb128_encode(img, offset, addend);

         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
         const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
         offset += leb128_encode(img, offset, segoff);

         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | flags;
         img.copy(offset, sym.c_str(), sym.size() + 1);
         offset += sym.size() + 1;
         img.at<uint8_t>(offset++) = BIND_OPCODE_DO_BIND;
      } else {
         /* LAZY */
         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
         const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
         offset += leb128_encode(img, offset, segoff);

         offset += emit_dylib_opcode(img, offset);

         img.at<uint8_t>(offset++) = BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | flags;
         img.copy(offset, sym.c_str(), sym.size() + 1);
         offset += sym.size() + 1;

         img.at<uint8_t>(offset++) = BIND_OPCODE_DO_BIND;
         img.at<uint8_t>(offset++) = BIND_OPCODE_DONE;
      }
   }

   template <Bits bits>
   std::size_t DyldInfo<bits>::content_size() const {
      return rebase->size() + bind->size() +
         weak_bind->size() + lazy_bind->size() + export_info->size();
   }

   template <Bits bits, bool lazy>
   BindInfo<bits, lazy>::BindInfo(const BindInfo<opposite<bits>, lazy>& other,
                            TransformEnv<opposite<bits>>& env): weak(other.weak) {
      for (const auto other_bindee : other.bindees) {
         bindees.push_back(other_bindee->Transform(env));
      }
   }

   template <Bits bits, bool lazy>
   BindNode<bits, lazy>::BindNode(const BindNode<opposite<bits>, lazy>& other,
                            TransformEnv<opposite<bits>>& env):
      type(other.type), addend(other.addend), dylib(nullptr),
      dylib_special(other.dylib_special), sym(other.sym), flags(other.flags),
      blob(nullptr), weak(other.weak)
   {
      if constexpr (lazy) { env.template add<LazyBindNode>(&other, this); }
      /* weak nodes have a null dylib (TransformEnv::resolve null-guards the
       * key, but be explicit and parallel to the parse ctor). */
      if (dylib_special == 0 && !weak) {
         env.resolve(other.dylib, &dylib);
      }
      env.resolve(other.blob, &blob);
   }

   template <Bits bits, bool lazy>
   void BindInfo<bits, lazy>::print(std::ostream& os) const {
      os << "segment section address dylib symbol" << std::endl;
      for (auto bindee : bindees) {
         bindee->print(os);
         os << std::endl;
      }
   }
   
   template <Bits bits, bool lazy>
   void BindNode<bits, lazy>::print(std::ostream& os) const {
      os << blob->segment->name() << " " << blob->section->name() << " 0x" << std::hex
         << blob->loc.vmaddr << " " << (weak ? "weak" : dylib_special ? "special" : dylib->name)
         << " " << sym;
   }

   template <Bits bits, bool lazy>
   void BindNode<bits, lazy>::Build(BuildEnv<bits>& env) {
      if constexpr (lazy) {
            index = env.lazy_bind_index(size());
         } else {
         index = 0;
      }
   }

   template <Bits bits, bool lazy>
   void BindInfo<bits, lazy>::Build(BuildEnv<bits>& env) {
      for (auto bindee : bindees) {
         bindee->Build(env);
      }
   }

   template class DyldInfo<Bits::M32>;
   template class DyldInfo<Bits::M64>;

   template class BindInfo<Bits::M32, true>;
   template class BindInfo<Bits::M32, false>;
   template class BindInfo<Bits::M64, true>;
   template class BindInfo<Bits::M64, false>;

}
