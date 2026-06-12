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
         if constexpr (std::is_integral_v<T>) {
            if ((std::size_t)key == 0xf602d1 && std::getenv("DBG_RES602")) {
               auto fit = found.find(key);
               std::cerr << "[res602] add(" << name << ") key=0xf602d1 pointee="
                         << (const void*)pointee << " lazy=" << (int)lazy
                         << " preexisting_found="
                         << (fit!=found.end() ? (const void*)fit->second : (const void*)nullptr)
                         << " todo_present=" << (todo.find(key)!=todo.end()) << "\n";
            }
         }
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
             * section B; or overlapping ranges in the input. The assert
             * was unconditional which is brittle for real-world binaries
             * — keep the first registration (likely the section that owns
             * the address) and warn instead of crashing.
             */
            /*
             * iPhoto Parse hit this ~18K times; the per-call cerr was real
             * I/O cost without diagnostic value (the policy is fixed:
             * keep first, drop new). Gate behind RESOLVER_VERBOSE so the
             * info is available when actually investigating, silent
             * otherwise.
             */
            static const bool verbose = std::getenv("RESOLVER_VERBOSE") != nullptr;
            if (verbose) {
               std::cerr << "Resolver(" << name << "): duplicate key with different pointee — keeping first; "
                         << "key=0x" << std::hex << key << std::dec
                         << " old=" << static_cast<const void *>(found_it->second)
                         << " new=" << static_cast<const void *>(pointee)
                         << std::endl;
            }
            return;
         }
         found.insert({key, pointee});
      }
      
      void resolve(const T& key, const U **pointer,
                   std::shared_ptr<functor> callback = std::make_shared<noop>()) {
         if constexpr (std::is_integral_v<T>) {
            if ((std::size_t)key == 0xf602d1 && std::getenv("DBG_RES602")) {
               auto fit = found.find(key);
               std::cerr << "[res602] resolve(" << name << ") key=0xf602d1 found="
                         << (fit!=found.end() ? (const void*)fit->second : (const void*)nullptr)
                         << "\n";
            }
         }
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
            if constexpr (std::is_integral_v<T>) {
               if ((std::size_t)todo_it->first == 0xf602d1 && std::getenv("DBG_RES602")) {
                  auto fit = found.find(todo_it->first);
                  std::cerr << "[res602] do_resolve(" << name << ") key=0xf602d1 todo_empty="
                            << todo_it->second.empty() << " found="
                            << (fit!=found.end() ? (const void*)fit->second : (const void*)nullptr)
                            << "\n";
               }
            }
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
