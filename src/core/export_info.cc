#include "export_info.hh"
#include <cstdlib>
#include "parse.hh"
#include "transform.hh"
#include "section_blob.hh"
#include "archive.hh"

namespace MachO {

   template <Bits bits>
   ExportInfo<bits>::ExportInfo(const Image& img, std::size_t offset, std::size_t size,
                                ParseEnv<bits>& env) {
      if (size > 0) {
         trie = ExportTrie<bits>::Parse(img, offset, env);
      }
   }

   /* Image base of the archive being emitted (ExportInfo::base, set at
    * Build_LINKEDIT); export values are vmaddr - base. Emission is serial. */
   static std::size_t g_export_emit_base = 0;

   template <Bits bits>
   RegularExportNode<bits>::RegularExportNode(const Image& img, std::size_t offset,
                                              std::size_t flags, ParseEnv<bits>& env):
      ExportNode<bits>(flags)
   {
      std::size_t value_offset;
      offset += leb128_decode(img, offset, value_offset);
      if (value_offset > 0) {
         /* The export's value_offset is a file offset from the dylib base. Most
          * exports point at __TEXT or __DATA sections, but some can point into
          * sectionless segments (notably __LINKEDIT). offset_to_vmaddr throws on
          * the latter; use try_ and skip placeholdering when the offset lands
          * outside any parsed section. The export still emits — its leb128 value
          * just stays 0 instead of tracking a moved blob. Affects exports that
          * reference __LINKEDIT data (rare; seen in iLife framework dylibs). */
         /* ★An export value is an ADDRESS offset from the image base (the
          * mach header = first non-__PAGEZERO segment), not a file offset. The
          * two only coincide where a segment's fileoff equals its vmaddr
          * delta, and NEVER for zero-fill (__common/__bss: no file bytes).
          * Portal 2 libtier0: all 12 __common exports (g_VProfCurrentProfile,
          * g_ClockSpeed, ...) were emitted as the section start, so every
          * importer aliased one zero page -> server VProf m_pCurNode NULL.
          * Kill M64_NO_EXPORT_VMADDR (both directions); guard export-zerofill. */
         static const bool by_vmaddr = std::getenv("M64_NO_EXPORT_VMADDR") == nullptr;
         std::optional<std::size_t> maybe_vmaddr;
         if (by_vmaddr) {
            for (Segment<bits> *seg : env.archive.segments()) {
               if (std::strcmp(seg->segment_command.segname, SEG_PAGEZERO) == 0) { continue; }
               const std::size_t va = seg->segment_command.vmaddr + value_offset;
               for (Segment<bits> *s2 : env.archive.segments()) {
                  if (s2->contains_vmaddr(va)) { maybe_vmaddr = va; break; }
               }
               break;
            }
         }
         if (!maybe_vmaddr) { maybe_vmaddr = env.archive.try_offset_to_vmaddr(value_offset); }
         if (maybe_vmaddr) {
            value = env.add_placeholder(*maybe_vmaddr);
         }
      }
   }

   template <Bits bits>
   ReexportNode<bits>::ReexportNode(const Image& img, std::size_t offset, std::size_t flags,
                                    ParseEnv<bits>& env): ExportNode<bits>(flags) {
      offset += leb128_decode(img, offset, libordinal);
      name = std::string(&img.at<char>(offset));
      offset += name.size() + 1;
   }

   template <Bits bits>
   StubExportNode<bits>::StubExportNode(const Image& img, std::size_t offset, std::size_t flags,
                                        ParseEnv<bits>& env): ExportNode<bits>(flags) {
      offset += leb128_decode(img, offset, stuboff);
      offset += leb128_decode(img, offset, resolveroff);
   }


   template <Bits bits>
   std::size_t RegularExportNode<bits>::derived_size() const {
      return leb128_size(std::numeric_limits<std::size_t>::max());
   }

   template <Bits bits>
   std::size_t ReexportNode<bits>::derived_size() const {
      return leb128_size(std::numeric_limits<std::size_t>::max()) + (name.size() + 1);
   }

   template <Bits bits>
   std::size_t StubExportNode<bits>::derived_size() const {
      return leb128_size(std::numeric_limits<std::size_t>::max()) * 2;
   }
   
   template <Bits bits>
   ExportTrie<bits> ExportTrie<bits>::Parse(const Image& img, std::size_t offset,
                                            ParseEnv<bits>& env) {
      ExportTrie<bits> t;
      t.root = ParseNode(img, offset, offset, env);
      return t;
   }

   template <Bits bits>
   ExportNode<bits> *ExportNode<bits>::Parse(const Image& img, std::size_t offset,
                                             ParseEnv<bits>& env) {
      std::size_t flags;
      offset += leb128_decode(img, offset, flags);
      
      if ((flags & EXPORT_SYMBOL_FLAGS_REEXPORT)) {
         return ReexportNode<bits>::Parse(img, offset, flags, env);
      } else if ((flags & EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER)) {
         return StubExportNode<bits>::Parse(img, offset, flags, env);
      } else {
         return RegularExportNode<bits>::Parse(img, offset, flags, env);
      }
   }

   template <Bits bits> typename ExportTrie<bits>::node
   ExportTrie<bits>::ParseNode(const Image& img, std::size_t offset, std::size_t start,
                               ParseEnv<bits>& env) {

      /* decode info */
      std::size_t size;
      offset += leb128_decode(img, offset, size);
 
      node curnode;
      if (size > 0) {
         curnode.value = ExportNode<bits>::Parse(img, offset, env);
      }
      offset += size;
      
      
      const uint8_t nedges = img.at<uint8_t>(offset++);
      for (uint8_t i = 0; i < nedges; ++i) {
         const char *sym = &img.at<char>(offset);
         offset += strlen(sym) + 1;

         std::size_t edge_diff;
         offset += leb128_decode(img, offset, edge_diff);

         node *subnode = &curnode;
         while (*sym) {
            auto result = subnode->children.emplace(*sym++, node());
            subnode = &result.first->second;
         }

         /* Apple's export trie format allows a zero-length edge label that
          * points at this node's own terminal info. When that's the case the
          * while loop above didn't move `subnode` away from `curnode`, so a
          * blind `*subnode = ParseNode(...)` would clobber whatever children
          * other edges of `curnode` had already populated — observed on iWeb's
          * SFDrawables where `_SFDPropertiesChangedNotification` has both a
          * multi-char "PropertiesKey" edge and an empty-label edge to its own
          * terminal, and the empty-label edge wiped the PropertiesKey child.
          * For the empty-label case copy the value only, preserving children.
          * For non-empty labels keep the original whole-node overwrite. */
         auto parsed = ParseNode(img, edge_diff + start, start, env);
         if (subnode == &curnode) {
            if (parsed.value) { subnode->value = parsed.value; }
            for (auto& kv : parsed.children) {
               subnode->children.emplace(kv.first, std::move(kv.second));
            }
         } else {
            *subnode = std::move(parsed);
         }
      }
      
      return curnode;
   }

   template <Bits bits>
   std::size_t ExportInfo<bits>::size() const {
      return trie.content_size();
   }

   template <Bits bits>
   std::size_t ExportTrie<bits>::content_size() const {
      return NodeSize(this->root);
   }

   template <Bits bits>
   std::size_t ExportTrie<bits>::NodeSize(const node& node) {
      std::size_t size = 0;

      if (node.value) {
         size += (*node.value)->size(); /* size of info */
      }
      size += leb128_size(size); /* size of size of info */
      ++size; /* nedges */

      for (auto& child : node.children) {
         std::string label;
         const auto& tail = EdgeTail(child, label);
         size += label.size() + 1;
         size += leb128_size(std::numeric_limits<std::size_t>::max()); /* max size of offset */
         size += NodeSize(tail);
      }

      return size;
   }

   /* An edge's full label: a run of value-less single-child nodes collapses
    * into one multi-character label (the standard compressed-trie encoding).
    * One character per edge makes the trie as deep as the longest symbol, and
    * ld rejects a trie deeper than 255 (long C++ mangled exports, Angry Birds). */
   template <Bits bits>
   const typename ExportTrie<bits>::node&
   ExportTrie<bits>::EdgeTail(const typename children_t::value_type& edge, std::string& label) {
      static const bool off = std::getenv("M64_NO_TRIE_EDGE_COMPRESS") != nullptr;
      label.assign(1, edge.first);
      const node *n = &edge.second;
      while (!off && !n->value && n->children.size() == 1) {
         label += n->children.begin()->first;
         n = &n->children.begin()->second;
      }
      return *n;
   }

   template <Bits bits>
   void ExportTrie<bits>::Emit(Image& img, std::size_t offset) const {
      EmitNode(this->root, img, offset, offset);
   }

   template <Bits bits>
   std::size_t ExportTrie<bits>::EmitNode(const node& node, Image& img, std::size_t offset,
                                          std::size_t start) {
      /* emit node info size */
      std::size_t info_size = node.value ? (*node.value)->size() : 0;
      std::size_t info_off;
      std::size_t info_actual_size = info_size;
      do {
         info_size = info_actual_size;
         info_off = offset;
         info_off += leb128_encode(img, info_off, info_size);
         if (node.value) {
            info_actual_size = (*node.value)->Emit(img, info_off);
         } else {
            info_actual_size = 0;
         }
         info_off += info_actual_size;
      } while (info_actual_size != info_size);
      offset = info_off;

      /* emit edge count */
      const uint8_t nedges = node.children.size();
      img.at<uint8_t>(offset++) = nedges;
      
      /* compute offset past edges */
      std::size_t offset_past_edges = offset;
      for (auto& child : node.children) {
         std::string label;
         EdgeTail(child, label);
         offset_past_edges += label.size() + 1 /* '\0' */ +
            leb128_size(std::numeric_limits<std::size_t>::max());
      }

      /* emit edges & children */
      for (auto& child : node.children) {
         std::string label;
         const auto& tail = EdgeTail(child, label);
         for (char c : label) { img.at<char>(offset++) = c; }
         img.at<char>(offset++) = '\0';
         offset += leb128_encode(img, offset, offset_past_edges - start);
         offset_past_edges = EmitNode(tail, img, offset_past_edges, start);
      }

      return offset_past_edges;
   }

   template <Bits bits>
   std::size_t ExportNode<bits>::Emit(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      offset += leb128_encode(img, offset, flags);
      offset += Emit_derived(img, offset);
      return offset - start;
   }

   template <Bits bits>
   void ExportInfo<bits>::Emit(Image& img, std::size_t offset) const {
      g_export_emit_base = base;   /* read by RegularExportNode::Emit_derived */
      return trie.Emit(img, offset);
   }

   template <Bits bits>
   std::size_t ExportNode<bits>::size() const {
      return leb128_size(flags) + derived_size();
   }

   template <Bits bits>
   std::size_t RegularExportNode<bits>::Emit_derived(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      static const bool by_vmaddr = std::getenv("M64_NO_EXPORT_VMADDR") == nullptr;
      offset += leb128_encode(img, offset,
                              value ? (by_vmaddr ? value->loc.vmaddr - g_export_emit_base
                                                 : value->loc.offset)
                                    : 0);
      return offset - start;
   }
   
   template <Bits bits>
   std::size_t ReexportNode<bits>::Emit_derived(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      offset += leb128_encode(img, offset, libordinal);
      img.copy(offset, name.c_str(), name.size() + 1);
      offset += name.size() + 1;
      return offset - start;
   }

   template <Bits bits>
   std::size_t StubExportNode<bits>::Emit_derived(Image& img, std::size_t offset) const {
      const std::size_t start = offset;
      offset += leb128_encode(img, offset, stuboff);
      offset += leb128_encode(img, offset, resolveroff);
      return offset - start;
   }

   template class ExportInfo<Bits::M32>;
   template class ExportInfo<Bits::M64>;

}
