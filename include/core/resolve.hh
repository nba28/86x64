#pragma once

#include <cassert>
#include <cstdlib>
#include <map>
#include <list>
#include <tuple>
#include <memory>
#include <iostream>
#include <typeinfo>
#include <type_traits>

#include "util.hh"

namespace MachO {

   /* Detect whether U exposes a size() method (only SectionBlob-keyed
    * resolvers use the containing-blob fallback; other U types — Node,
    * Segment, DylibCommand — are explicitly instantiated and must still
    * compile do_resolve_containing without a size()). */
   template <typename X, typename = void>
   struct resolver_has_size : std::false_type {};
   template <typename X>
   struct resolver_has_size<X, std::void_t<decltype(std::declval<X>().size())>>
      : std::true_type {};

   template <typename T, typename U, bool lazy>
   class Resolver {
   public:
      struct functor {
         virtual void operator()(U *val) = 0;
         virtual ~functor() {}
      };

      struct noop: public functor {
         virtual void operator()(U *val) override {}
      };
      
      using FoundMap = std::map<T, U *>;
      // typedef void (*Callback)(U *, Resolver<T, U>&);
      using TodoNode = std::pair<const U **, std::shared_ptr<functor>>;
      using TodoMap = std::map<T, std::list<TodoNode>>;
      
      void add(const T& key, U *pointee) {
         if constexpr (!lazy) {
               auto todo_it = todo.find(key);
               if (todo_it != todo.end()) {
                  for (const TodoNode& node : todo_it->second) {
                     *node.first = pointee;
                     (*node.second)(pointee);
                  }
                  todo.erase(todo_it);
               }
            }

         auto found_it = found.find(key);
         if (found_it != found.end() && found_it->second != pointee) {
            /*
             * Two different parsed blobs claiming the same address. Common
             * triggers: contiguous sections (e.g. __TEXT,__text ending at
             * the same vmaddr where __TEXT,__symbol_stub begins) producing
             * a one-past-end blob in section A and a starts-here blob in
             * section B; or overlapping ranges in the input. Keep the first
             * registration (likely the section that owns the address).
             */
            return;
         }
         found.insert({key, pointee});
      }
      
      /* Cancel a pending deferred resolve() for `key` whose destination is
       * `pointer`. Used when a later analysis pass (DetectPicAnchoredDisps)
       * reinterprets an operand: instruction.cc's `[base+disp32]` absolute-
       * table path eagerly registers resolve(raw_disp, &memdisp) before the
       * anchor pass knows `base` is a PIC anchor. Without cancellation that
       * deferred todo fires in do_resolve() and clobbers the anchor-corrected
       * placeholder (anchor+disp), so the selector/data load resolves to the
       * raw displacement's blob (lands in __eh_frame -> garbage). Only removes
       * todo entries aimed at exactly `pointer`; an already-fired resolve has
       * no todo entry, so the anchor pass's direct memdisp overwrite wins. */
      void cancel(const T& key, const U **pointer) {
         auto it = todo.find(key);
         if (it == todo.end()) { return; }
         it->second.remove_if([&](const TodoNode& n){ return n.first == pointer; });
         if (it->second.empty()) { todo.erase(it); }
      }

      void resolve(const T& key, const U **pointer,
                   std::shared_ptr<functor> callback = std::make_shared<noop>()) {
         auto found_it = found.find(key);
         if (found_it != found.end()) {
            *pointer = found_it->second;
            (*callback)(found_it->second);
         } else {
            todo[key].emplace_back(pointer, callback);
         }
      }

      /*
       * Containing-blob resolution. An absolute data reference can target an
       * address in the MIDDLE of a multi-byte blob (e.g. `movb $0, [&obj.field]`
       * where `obj` parsed as one DataBlob registered only at its start). Exact-
       * key resolve() then misses and leaves the pointer null → the store emits
       * unrelocated and lands in read-only __TEXT (iPhoto 8th-blocker class).
       * resolve_containing() defers like resolve(); do_resolve_containing()
       * (run after do_resolve) fills *pointer with the nearest registered blob
       * whose [start, start+size) range contains the key, and *offset_out with
       * (key - blob_start) so the emitter can target the exact byte.
       */
      /* override=true: replace an already-set *pointer (e.g. a placeholder from
       * add_placeholder) with a real containing blob when one exists. Used by
       * the M64 rip-relative re-parse path so mid-blob refs resolve to the
       * containing blob (which gets a correct rebuilt vmaddr) instead of a
       * stranded placeholder. override=false (default): only fill when *pointer
       * is still null (an exact resolve() hasn't already won). */
      using TodoContaining = std::list<std::tuple<const U **, std::size_t *, T, bool>>;
      TodoContaining todo_containing;

      void resolve_containing(const T& key, const U **pointer,
                              std::size_t *offset_out, bool override = false) {
         *offset_out = 0;
         todo_containing.emplace_back(pointer, offset_out, key, override);
      }

      /* Cancel a pending resolve_containing() for `key` whose destination is
       * `pointer` (the containing-fallback sibling of cancel() above). Used
       * when a later analysis pass reinterprets an operand as a non-pointer —
       * e.g. DetectPicAnchoredDisps recognising that a heuristic pointer-
       * immediate inside PIC-anchored code is really an anchor-relative
       * OFFSET that merely aliases a zerofill section's (huge) vmaddr span.
       * Without cancellation do_resolve_containing() would attach the
       * immediate to the spanning ZeroBlob and mis-relocate the constant. */
      void cancel_containing(const T& key, const U **pointer) {
         todo_containing.remove_if([&](const std::tuple<const U **, std::size_t *, T, bool>& n) {
            return std::get<0>(n) == pointer && std::get<2>(n) == key;
         });
      }

      void do_resolve_containing() {
       if constexpr (resolver_has_size<U>::value) {
         for (auto& node : todo_containing) {
            const U **pointer = std::get<0>(node);
            std::size_t *offset_out = std::get<1>(node);
            const T key = std::get<2>(node);
            const bool override = std::get<3>(node);
            if (!override && *pointer != nullptr) { continue; }  /* exact resolve won */
            auto exact = found.find(key);
            if (exact != found.end()) {
               *pointer = exact->second;
               *offset_out = 0;
               continue;
            }
            /* nearest registered blob with start <= key */
            auto it = found.upper_bound(key);
            if (it == found.begin()) { continue; }
            --it;
            U *blob = it->second;
            if (blob != nullptr && key < it->first + blob->size()) {
               *pointer = blob;
               *offset_out = (std::size_t)(key - it->first);
            }
         }
       }
         todo_containing.clear();
      }

      void do_resolve() {
         for (auto todo_it = todo.begin(); todo_it != todo.end(); todo_it = todo.erase(todo_it)) {
            // for (auto todo_it = todo.begin(); todo_it != todo.end(); ++todo_it) {
            if (!todo_it->second.empty()) {
               auto found_it = found.find(todo_it->first);
               if (found_it != found.end()) {
                  for (const TodoNode& node : todo_it->second) {
                     *node.first = found_it->second;
                     (*node.second)(found_it->second);
                  }
               }
            }
         }
      }

      Resolver(const std::string& name): name(name) {}
      
      ~Resolver();

      FoundMap found;
      TodoMap todo;

      const std::string name;

   };   
   
}
