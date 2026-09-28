#include <cstdlib>

#include "resolve.hh"
#include "parse.hh"
#include "section_blob.hh"
#include "dyldinfo.hh"

namespace MachO {

      template <typename T, typename U, bool lazy>
      Resolver<T, U, lazy>::~Resolver() {}

   template class Resolver<const Node *, Node, false>;
   template class Resolver<std::size_t, SectionBlob<Bits::M32>, true>;
   template class Resolver<std::size_t, SectionBlob<Bits::M64>, true>;
   template class Resolver<uint32_t, BindNode<Bits::M32, true>, false>;
   template class Resolver<uint32_t, BindNode<Bits::M64, true>, false>;

   template class Resolver<std::size_t, DylibCommand<Bits::M32>, false>;
   template class Resolver<std::size_t, DylibCommand<Bits::M64>, false>;
   template class Resolver<std::size_t, Segment<Bits::M32>, false>;
   template class Resolver<std::size_t, Segment<Bits::M64>, false>;
   template class Resolver<std::size_t, Section<Bits::M32>, false>;
   template class Resolver<std::size_t, Section<Bits::M64>, false>;


   
}
