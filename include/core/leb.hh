#pragma once

#include <cassert>
#include <limits>
#include <stdexcept>
#include <type_traits>

#include "leb.h"
#include "image.hh"

namespace MachO {

template <typename T>
size_t leb128_decode(const Image& img, std::size_t offset, T& n) {
   static_assert(std::is_integral<T>());
   size_t count;
   std::string name;

   if (offset > img.size()) { throw std::out_of_range(__FUNCTION__); }
   const void *buf = &img.at<char>(offset);
   const std::size_t buflen = img.size() - offset;
   if constexpr (std::is_unsigned<T>()) {
         count = uleb128_decode(buf, buflen, &n);
         name = "uleb";
      } else {
      count = sleb128_decode(buf, buflen, &n);
      name = "sleb";
   }

   if (count == 0) {
      throw std::overflow_error(name + "128 overflow");
   } else if (count > buflen) {
      throw std::invalid_argument(name + "128 runs past end of buffer");
   }

   return count;
}

template <typename T>
size_t leb128_size(T n) {
   static_assert(std::is_integral<T>());
   if constexpr (std::is_unsigned<T>()) {
         return uleb128_encode(nullptr, std::numeric_limits<size_t>::max(), n);
      } else {
      return sleb128_encode(nullptr, std::numeric_limits<size_t>::max(), n);
   }
}

template <typename T>
size_t leb128_encode(Image& img, std::size_t offset, T n) {
   static_assert(std::is_integral<T>());
   
   std::string name;
   const size_t buflen = leb128_size(n);
   char *buf = new char[buflen];
   
   /* NB: the encode call must NOT live inside assert() -- assert() expands to
    * nothing under -DNDEBUG (every -O Release/RelWithDebInfo/MinSizeRel build),
    * which would skip the encode and copy an UNINITIALIZED buffer into the
    * image, corrupting every LEB value (export-trie edge offsets, bind/rebase
    * deltas, ...). Run the encode unconditionally; assert only its result. */
   size_t encoded;
   if constexpr (std::is_unsigned<T>()) {
         encoded = uleb128_encode(buf, buflen, n);
      } else {
      encoded = sleb128_encode(buf, buflen, n);
   }
   assert(encoded);
   (void) encoded;

   img.copy(offset, buf, buflen);
   delete[] buf;
                  

   return buflen;
}

}
