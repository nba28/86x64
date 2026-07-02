/* 63_cpp_static_map_rbtree.cc — the SL i386 libstdc++'s OUT-OF-LINE
 * red-black-tree entry points, bridged by libabiconv's rb shims (cxx_shim.c
 * shim_rb_{increment,decrement,insert_rebalance,rebalance_for_erase} via the
 * MTSHIM trampolines).
 *
 * Calls the mangled libstdc++ exports DIRECTLY on a hand-built
 * _Rb_tree_node_base tree (the host SDK's std::map is libc++'s header-inlined
 * __tree, which would never import these), exactly the binding shape a real
 * GCC-4-built game (Civ IV) has.
 *
 * Written while root-causing Civ IV's _Rb_tree_decrement SIGSEGV at 0x4: the
 * shim itself proved FAITHFUL to GNU libstdc++ (the crash came from a
 * never-constructed static set — the translator had mangled the GCC
 * static-init priority immediate, see 62_imm_const_text_alias). This test
 * pins the shim semantics anyway:
 *   - insert_and_rebalance: 5 ascending inserts (forces recolor + rotation),
 *     incl. the first-node p==&header header-hookup path;
 *   - increment: sorted forward walk incl. the rightmost->header step that
 *     terminates iteration;
 *   - decrement ON THE HEADER NODE (= --end()/rbegin() of a NON-empty tree):
 *     the `color==RED && parent->parent==this` header check must route to
 *     header->right (rightmost), NOT the generic predecessor walk;
 *   - rebalance_for_erase of an interior key, then re-walk.
 */
#include <cstdio>
#include <cstdlib>

struct rb_node {
   unsigned color;               /* 0 = red, 1 = black */
   rb_node *parent, *left, *right;
   int key;                      /* payload (std::_Rb_tree_node<int> value) */
};

extern "C" rb_node *rb_inc(rb_node *)
   asm("__ZSt18_Rb_tree_incrementPSt18_Rb_tree_node_base");
extern "C" rb_node *rb_dec(rb_node *)
   asm("__ZSt18_Rb_tree_decrementPSt18_Rb_tree_node_base");
extern "C" void rb_insert(bool insert_left, rb_node *x, rb_node *p,
                          rb_node &header)
   asm("__ZSt29_Rb_tree_insert_and_rebalancebPSt18_Rb_tree_node_baseS0_RS_");
extern "C" rb_node *rb_erase(rb_node *z, rb_node &header)
   asm("__ZSt28_Rb_tree_rebalance_for_erasePSt18_Rb_tree_node_baseRS_");

static rb_node header;           /* std::_Rb_tree_impl::_M_header */
static rb_node nodes[5];

int main() {
   /* _Rb_tree_impl::_M_initialize(): the empty-tree header. */
   header.color = 0;             /* _S_red */
   header.parent = 0;
   header.left = &header;
   header.right = &header;

   /* Ascending inserts, exactly as _M_insert would place them: the first
    * node hooks up under the header (insert_left, p==&header); every later
    * (larger) key becomes the RIGHT child of the current rightmost. */
   for (int i = 0; i < 5; i++) {
      nodes[i].key = i * 10;
      if (header.parent == 0) {
         rb_insert(true, &nodes[i], &header, header);
      } else {
         rb_insert(false, &nodes[i], header.right, header);
      }
   }

   /* Forward: begin() = header.left; ++ via _Rb_tree_increment until the
    * rightmost increments ONTO the header (end()). */
   for (rb_node *n = header.left; n != &header; n = rb_inc(n)) {
      printf("%d\n", n->key);
   }

   /* Reverse: --end() = _Rb_tree_decrement(&header) — the header-node
    * special case — then keep decrementing down to leftmost. */
   for (rb_node *n = rb_dec(&header);; n = rb_dec(n)) {
      printf("r %d\n", n->key);
      if (n == header.left) { break; }
   }

   /* Erase the interior key 20, then re-walk. */
   rb_erase(&nodes[2], header);
   for (rb_node *n = header.left; n != &header; n = rb_inc(n)) {
      printf("e %d\n", n->key);
   }

   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
