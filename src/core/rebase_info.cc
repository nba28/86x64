#include <cassert>
#include <string>
#include <iostream>
#include <mach-o/loader.h>
#include <typeinfo>

#include "rebase_info.hh"
#include "image.hh"
#include "leb.hh"
#include "instruction.hh" // Immediate
#include "parse.hh"
#include "archive.hh"

namespace MachO {

   template <Bits bits>
   RebaseInfo<bits>::RebaseInfo(const Image& img, std::size_t offset, std::size_t size,
                                ParseEnv<bits>& env) {
      const std::size_t begin = offset;
      const std::size_t end = begin + size;
      std::size_t it = begin;
      uint8_t type = 0;
      std::size_t vmaddr = 0;
      while (it != end) {
         const uint8_t byte = img.at<uint8_t>(it);
         const uint8_t opcode = byte & REBASE_OPCODE_MASK;
         const uint8_t imm = byte & REBASE_IMMEDIATE_MASK;
         ++it;

         std::size_t uleb, uleb2;
         
         switch (opcode) {
         case REBASE_OPCODE_DONE:
            return;
            
         case REBASE_OPCODE_SET_TYPE_IMM:
            type = imm;
            break;
            
         case REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB:
            it += leb128_decode(img, it, vmaddr);
            vmaddr += env.archive.segment(imm)->vmaddr();
            break;

         case REBASE_OPCODE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr += uleb;
            break;

         case REBASE_OPCODE_ADD_ADDR_IMM_SCALED:
            vmaddr += imm * sizeof(ptr_t);
            break;

         case REBASE_OPCODE_DO_REBASE_IMM_TIMES:
            vmaddr = do_rebase_times(imm, vmaddr, env, type);
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES:
            it += leb128_decode(img, it, uleb);
            vmaddr = do_rebase_times(uleb, vmaddr, env, type);
            break;

         case REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB:
            it += leb128_decode(img, it, uleb);
            vmaddr = do_rebase(vmaddr, env, type);
            vmaddr += uleb;
            break;
            
         case REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB:
            it += leb128_decode(img, it, uleb);
            it += leb128_decode(img, it, uleb2);
            vmaddr = do_rebase_times(uleb, vmaddr, env, type, uleb2);
            break;
            
         default:
            throw error("%s: invalid rebase opcode", __FUNCTION__);
         }
         
      }
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type) {
      // std::cerr << "[PARSE] REBASE @ 0x" << std::hex << vmaddr << std::endl;
      rebasees.push_back(RebaseNode<bits>::Parse(vmaddr, env, type));
      /* every rebased slot -- TEXT relocations included -- is the linker's
       * statement that it holds an in-image address (ParseEnv::rebase_slot_addrs) */
      if constexpr (bits == Bits::M32) { env.rebase_slot_addrs.insert(vmaddr); }
      return vmaddr + sizeof(ptr_t);
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::do_rebase_times(std::size_t count, std::size_t vmaddr,
                                                 ParseEnv<bits>& env, uint8_t type,
                                                 std::size_t skipping) {
      for (std::size_t i = 0; i < count; ++i) {
         do_rebase(vmaddr, env, type);
         vmaddr += sizeof(ptr_t) + skipping;
      }
      return vmaddr;
   }

   template <Bits bits>
   std::size_t RebaseInfo<bits>::size() const {
      std::size_t size = 0;
      for (const RebaseNode<bits> *node : rebasees) {
         size += node->size();
      }
      if (size > 0) {
         ++size; /* REBASE_OPCODE_DONE */
      }
      return align<bits>(size);
   }

   template <Bits bits>
   RebaseNode<bits>::RebaseNode(std::size_t vmaddr, ParseEnv<bits>& env, uint8_t type):
      type(type), blob(nullptr)
   {
      struct callback: decltype(env.vmaddr_resolver)::functor {
         ParseEnv<bits>& env;

         callback(ParseEnv<bits>& env): env(env) {}
         
         virtual void operator()(SectionBlob<bits> *blob2) override {
            auto imm = dynamic_cast<Immediate<bits> *>(blob2);
            if (imm) {
               imm->pointee = env.add_placeholder(imm->value);
            }
         }
      };
      
      env.vmaddr_resolver.resolve(vmaddr, &blob, std::make_shared<callback>(env));
   }

   template <Bits bits>
   bool RebaseNode<bits>::emittable() const {
      /* All preconditions for safely emitting this rebase opcode. Used by
       * both size() and Emit() so the byte counts agree (any mismatch
       * silently corrupts every later LC). */
      if (!active()) return false;
      if (blob->segment == nullptr) return false;
      if constexpr (bits == Bits::M64) {
         /* x86_64 only allows REBASE_TYPE_POINTER, and disallows rebases
          * into __TEXT (W^X — __TEXT pages are RX, dyld can't apply slide).
          * The i386 binaries we translate have REBASE_TYPE_TEXT_{ABSOLUTE32,
          * PCREL32} entries that the M32→M64 instruction rewriter has
          * already replaced with RIP-relative addressing, so the rebase
          * entries themselves are now superfluous and must be filtered out
          * or dyld rejects the load: "REBASE_OPCODE_DO_REBASE_IMM_TIMES
          * text rebase not supported for architecture". */
         if (type != REBASE_TYPE_POINTER) return false;
         if (strcmp(blob->segment->segment_command.segname, SEG_TEXT) == 0) {
            return false;
         }
         /* Sections containing 4-byte pointer slots (legacy i386 ObjC1
          * __OBJC,__message_refs / __cls_refs and any S_LITERAL_POINTERS)
          * cannot be safely rebased by dyld. REBASE_TYPE_POINTER in x86_64
          * reads + writes 8 bytes per entry; one rebase clobbers two
          * adjacent 4-byte slots, and 494 cascading entries in __message_refs
          * corrupt the entire section. A runtime shim (DYLD_INSERT) must
          * walk these sections and apply the binary's slide to each 4-byte
          * slot instead. The translator already embeds post-translate-time
          * (unslid) M64 vmaddrs in the slots, so the shim just adds slide. */
         if (blob->section != nullptr) {
            const uint32_t stype =
               blob->section->sect.flags & SECTION_TYPE;
            if (stype == S_LITERAL_POINTERS) {
               return false;
            }
            /* __OBJC segment is S_REGULAR but holds 4-byte ObjC1 slots
             * (legacy metadata). Filter those too. */
            if (strcmp(blob->segment->segment_command.segname, SEG_OBJC) == 0) {
               return false;
            }
         }
      }
      return true;
   }

   template <Bits bits>
   std::size_t RebaseNode<bits>::size() const {
      if (!emittable()) {
         return 0;
      }

      /* 1   REBASE_OPCODE_SET_TYPE_IMM
       * 1+a REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * 1   REBASE_OPCODE_DO_REBASE_IMM_TIMES
       * 3+a total
       */
      assert(blob->segment->id < 16);
      /* vmaddr delta, NOT file-offset delta: Emit() encodes
       * `loc.vmaddr - segment->loc().vmaddr`, and the two diverge in
       * segments with zerofill content. A size()/Emit() length mismatch
       * across a LEB128 boundary would corrupt the opcode stream. */
      return 3 + leb128_size(blob->loc.vmaddr - blob->segment->loc().vmaddr);
   }

   template <Bits bits>
   RebaseNode<bits>::RebaseNode(const RebaseNode<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env):
      type(other.type), blob(nullptr)
   {
      env.resolve(other.blob, &blob);
   }

   template <Bits bits>
   RebaseInfo<bits>::RebaseInfo(const RebaseInfo<opposite<bits>>& other,
                                TransformEnv<opposite<bits>>& env) {
      for (const auto other_rebasee : other.rebasees) {
         rebasees.push_back(other_rebasee->Transform(env));
      }
   }

   template <Bits bits>
   void RebaseInfo<bits>::Emit(Image& img, std::size_t offset) const {
      for (RebaseNode<bits> *rebasee : rebasees) {
         rebasee->Emit(img, offset);
         offset += rebasee->size();
      }
      if (!rebasees.empty()) {
         img.at<uint8_t>(offset) = REBASE_OPCODE_DONE;
      }
   }

   template <Bits bits>
   void RebaseNode<bits>::Emit(Image& img, std::size_t offset) const {
      if (!emittable()) {
         return;
      }

      /* REBASE_OPCODE_SET_TYPE_IMM
       * REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB
       * REBASE_OPCODE_DO_REBASE_IMM_TIMES
       */
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_TYPE_IMM | type;
      img.at<uint8_t>(offset++) = REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | blob->segment->id;
      const std::size_t segoff = blob->loc.vmaddr - blob->segment->loc().vmaddr;
      offset += leb128_encode(img, offset, segoff);
      // offset += leb128_encode(&img.at<uint8_t>(offset), img.size() - offset, segoff);
      img.at<uint8_t>(offset++) = REBASE_OPCODE_DO_REBASE_IMM_TIMES | 0x1;
   }

   template <Bits bits>
   void RebaseInfo<bits>::print(std::ostream& os) const {
      os << "segment\tsection\taddress\ttype" << std::endl;
      for (auto rebasee : rebasees) {
         rebasee->print(os);
         os << std::endl;
      }
   }

   template <Bits bits>
   void RebaseNode<bits>::print(std::ostream& os) const {
      if (blob) {
         os << blob->segment->name() << "\t" << blob->section->name() << "\t"
            << blob->loc.vmaddr << "\t";
      } else {
         os << "?" << "\t" << "?" << "\t" << "?" << "\t";
      }

      std::unordered_map<uint8_t, std::string> types =
         {{REBASE_TYPE_POINTER, "POINTER"},
          {REBASE_TYPE_TEXT_ABSOLUTE32, "TEXT_ABSOLUTE32"},
          {REBASE_TYPE_TEXT_PCREL32, "TEXT_PCREL32"}};
      auto type_it = types.find(type);
      if (type_it != types.end()) {
         os << type_it->second;
      } else {
         os << "(invalid)";
      }
   }

   template class RebaseInfo<Bits::M32>;
   template class RebaseInfo<Bits::M64>;

}
