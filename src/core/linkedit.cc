#include <cassert>
#include "linkedit.hh"
#include "leb.hh"
#include "data_in_code.hh"
#include "archive.hh"
#include "segment.hh"
#include "section_blob.hh" // SectionBlob

namespace MachO {

   template <Bits bits>
   LinkeditData<bits> *LinkeditData<bits>::Parse(const Image& img, std::size_t offset,
                                                 ParseEnv<bits>& env) {
      const linkedit_data_command& linkedit = img.at<linkedit_data_command>(offset);
      switch (linkedit.cmd) {
      case LC_DATA_IN_CODE:
         return DataInCode<bits>::Parse(img, offset, env);

      case LC_FUNCTION_STARTS:
         return FunctionStarts<bits>::Parse(img, offset, env);

      case LC_CODE_SIGNATURE:
         return CodeSignature<bits>::Parse(img, offset, env);

      case LC_SEGMENT_SPLIT_INFO:
      case LC_DYLIB_CODE_SIGN_DRS:
      case LC_LINKER_OPTIMIZATION_HINT:
         /* Opaque byte blobs, but distinct from CodeSignature so that
          * subcommand<CodeSignature>() lookups don't pick one of these up by
          * mistake (QuickTime has LC_SEGMENT_SPLIT_INFO; it would otherwise
          * win the dynamic_cast and steal CodeSignature's __LINKEDIT slot
          * during Segment_LINKEDIT::Build). The bytes pass through unchanged. */
         return OpaqueLinkeditBlob<bits>::Parse(img, offset, env);

      default:
         throw error("%s: unknown linkedit command type 0x%x", __FUNCTION__, linkedit.cmd);
      }
   }
   
   template <Bits bits>
   void LinkeditData<bits>::Build_LINKEDIT(BuildEnv<bits>& env) {
      /* datasize records the actual content length. Allocate space rounded up to
       * pointer alignment so the next linkedit blob starts aligned (dyld requires
       * pointer-aligned blob starts in __LINKEDIT). LC_SEGMENT_SPLIT_INFO and
       * other linker-emitted blobs aren't always pointer-aligned themselves
       * (the old assert here only held for CodeSignature). The padding bytes are
       * left zero by Image's default init. */
      linkedit.datasize = this->content_size();
      const std::size_t alloc_size =
         (linkedit.datasize + sizeof(ptr_t<bits>) - 1) & ~(sizeof(ptr_t<bits>) - 1);
      linkedit.dataoff = env.allocate(alloc_size);
   }

   template <Bits bits>
   void LinkeditData<bits>::Emit(Image& img, std::size_t offset) const {
      img.at<linkedit_data_command>(offset) = linkedit;
      Emit_content(img, linkedit.dataoff);
      /* Build_LINKEDIT padded the allocation up to pointer alignment but
       * datasize records the unpadded content length, so Emit_content writes
       * datasize bytes. The Image grows on-demand to the highest byte ever
       * written — if we never touch the trailing 0..7 padding bytes, the file
       * ends short of where __LINKEDIT's filesize says it does, and dyld_info
       * flags: "segment '__LINKEDIT' load command content extends beyond end
       * of file". Touch the last padded byte to extend the file. */
      const std::size_t alloc_size =
         (linkedit.datasize + sizeof(ptr_t<bits>) - 1) & ~(sizeof(ptr_t<bits>) - 1);
      if (alloc_size > linkedit.datasize) {
         img.at<uint8_t>(linkedit.dataoff + alloc_size - 1) = 0;
      }
   }

   template <Bits bits>
   FunctionStarts<bits>::FunctionStarts(const Image& img, std::size_t offset, ParseEnv<bits>& env):
      LinkeditData<bits>(img, offset, env), segment(env.archive.segment(SEG_TEXT))
   {
      const std::size_t begin = this->linkedit.dataoff;
      const std::size_t end = begin + this->linkedit.datasize;
      std::size_t it = begin;
      std::size_t refaddr = segment->loc().offset;

      while (it != end) {
         std::size_t uleb;
         it += leb128_decode(img, it, uleb);
         // it += leb128_decode(&img.at<uint8_t>(it), img.size() - it, uleb);
         if (uleb != 0 || refaddr == segment->loc().offset) {
            refaddr += uleb;
            std::optional<std::size_t> vmaddr = env.archive.try_offset_to_vmaddr(refaddr);
            if (vmaddr) {
               entries.push_back(env.add_placeholder(*vmaddr));
            } else {
               fprintf(stderr, "warning: function start with offset 0x%zx not in __text segment\n",
                       refaddr);
            }
         }
      }
   }

   template <Bits bits>
   std::size_t FunctionStarts<bits>::content_size() const {
      std::size_t size = 0;
      std::size_t refaddr = segment->loc().offset;
      
      for (const SectionBlob<bits> *entry : entries) {
         size += leb128_size(entry->loc.offset - refaddr);
         refaddr = entry->loc.offset;
      }
      
      return align<bits>(size);
   }

   template <Bits bits>
   void FunctionStarts<bits>::Emit_content(Image& img, std::size_t offset) const {
      std::size_t refaddr = segment->loc().offset;
      const std::size_t begin = offset;
      const std::size_t end = begin + content_size();
      std::size_t it = begin;
      for (const SectionBlob<bits> *entry : entries) {
         it += leb128_encode(img, it, entry->loc.offset - refaddr);
         refaddr = entry->loc.offset;
      }
      img.memset(it, 0, end - it);
   }

   template <Bits bits>
   FunctionStarts<bits>::FunctionStarts(const FunctionStarts<opposite<bits>>& other,
                                        TransformEnv<opposite<bits>>& env):
      LinkeditData<bits>(other, env)
   {
      for (const auto entry : other.entries) {
         entries.push_back(nullptr);
         const Placeholder<bits> **entryptr = &entries.back();
         env.resolve(entry, entryptr);
      }
      env.resolve(other.segment, &segment);
   }

   template <Bits bits>
   void CodeSignature<bits>::Emit_content(Image& img, std::size_t offset) const {
      img.copy(offset, cs.begin(), cs.size());
   }

   template <Bits bits>
   void OpaqueLinkeditBlob<bits>::Emit_content(Image& img, std::size_t offset) const {
      img.copy(offset, bytes.begin(), bytes.size());
   }

   template class LinkeditData<Bits::M32>;
   template class LinkeditData<Bits::M64>;
   template class OpaqueLinkeditBlob<Bits::M32>;
   template class OpaqueLinkeditBlob<Bits::M64>;

}
