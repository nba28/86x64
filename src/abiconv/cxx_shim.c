/*
 * Hand-written i386->x86_64 shims for the C++ runtime entry points that
 * legacy i386 binaries import from /usr/lib/libstdc++.6.dylib (iWeb's iWork
 * SF* frameworks were the first to hit them — __cxa_guard_acquire reached
 * the NATIVE libc++abi entry with i386 stack args, so it dereferenced a
 * stale rdi).
 *
 * Families covered (trampolines in maptable_tramp.asm, binds redirected by
 * static-interpose at translate time):
 *
 *  - __cxa_guard_{acquire,release,abort}: implemented LOCALLY on the i386
 *    guard object (first byte = initialized flag, per Itanium ABI) with one
 *    process-wide recursive mutex. Not forwarded: native libc++abi treats
 *    the guard's tail bytes as its own lock words sized for x86_64.
 *
 *  - __cxa_atexit: forwarded to native __cxa_atexit with the i386 destructor
 *    wrapped as a native callback (x64_cb_wrap) so it can fire at exit on a
 *    low-4GB stack. MUST be interposed so its classic lazy symbol pointer
 *    binds to libabiconv instead of the broken translated stub_helper path.
 *
 *  - operator new/delete (__Znwm/__Znam/__ZdlPv/__ZdaPv + nothrow forms):
 *    libabiconv's malloc/free, which IS the low-4GB heap the i386 code
 *    lives on. All paired forms are interposed together, so allocations
 *    never cross heaps. On Darwin/i386 size_t mangles as 'm' (unsigned
 *    long), hence the 64-bit-looking names with 4-byte sizes.
 *
 *  - std::_Rb_tree_{increment,decrement,insert_and_rebalance,
 *    rebalance_for_erase}: reimplemented for the i386 node layout
 *    {color,parent,left,right} x 4 bytes. The native versions would write
 *    8-byte pointers through nodes the i386 code allocated with 4-byte
 *    fields. Classic CLRS red-black algorithm, matching the documented
 *    libstdc++ tree semantics (header node: parent=root, left=leftmost,
 *    right=rightmost; root.parent=&header).
 *
 *  - __ZSt*__throw_{bad_alloc,length_error,out_of_range}: loud abort. A
 *    native C++ throw could never unwind through translated frames anyway;
 *    failing fast with the message beats corrupting the unwinder.
 *
 * NOT shimmed (deliberate): __cxa_pure_virtual (zero-arg noreturn — calling
 * convention is irrelevant), __gxx_personality_v0 + __cxa_begin/end_catch/
 * rethrow (only reachable from unwinding, which cannot enter translated
 * frames), __ZTV*__class_type_infoE vtable DATA binds (RTTI metadata;
 * truncation hazard noted but inert until something dynamic_casts).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* ---------------- __cxa_guard_* ---------------- */

static pthread_mutex_t g_guard_mu = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;

uint32_t shim_cxa_guard_acquire(uint32_t *a) {
   volatile uint8_t *g = (volatile uint8_t *)(uintptr_t)a[0];
   if (!g) { return 0; }
   if (*g) { return 0; }                  /* already initialized */
   pthread_mutex_lock(&g_guard_mu);
   if (*g) { pthread_mutex_unlock(&g_guard_mu); return 0; }
   return 1;                              /* caller runs the initializer */
}

uint32_t shim_cxa_guard_release(uint32_t *a) {
   volatile uint8_t *g = (volatile uint8_t *)(uintptr_t)a[0];
   if (g) { *g = 1; }
   pthread_mutex_unlock(&g_guard_mu);
   return 0;
}

uint32_t shim_cxa_guard_abort(uint32_t *a) {
   (void)a;
   pthread_mutex_unlock(&g_guard_mu);
   return 0;
}

/* ---------------- __cxa_atexit ---------------- */
/*
 * int __cxa_atexit(void (*func)(void*), void* arg, void* dso) — register a
 * C++ destructor (Itanium ABI). Reached from translated static initializers
 * (e.g. Source-engine libtier0's GetGlobalLoggingSystem registering
 * ~CLoggingSystem). `func` is TRANSLATED i386 code, so we cannot hand its raw
 * 4-byte address to the native libc++abi __cxa_atexit: at exit the native
 * runtime would call it with the x86_64 ABI on a >4GB stack and fault. Wrap it
 * as a native callback (x64_cb_wrap, 1 pointer arg, void return — the SysV
 * shape of `void dtor(void*)`); the trampoline marshals back to the i386 cdecl
 * frame on a low-4GB stack when the runtime fires it. `arg` and `dso` are
 * opaque low-4GB cookies passed straight through.
 *
 * This is the C++-runtime analogue of the __cxa_guard_* shims: an i386 import
 * that MUST be interposed (not bound to native) so its classic lazy symbol
 * pointer resolves to libabiconv. (Without the shim the slot never binds — on
 * modern dyld a classic LAZY pointer is only serviced by the original i386
 * stub_helper/dyld_stub_binder path, which translation breaks.)
 */
struct cxa_cb_sig { uint32_t nargs; uint32_t ret_kind; uint8_t arg_kinds[16]; };
extern uint64_t x64_cb_wrap(uint32_t fn32, const struct cxa_cb_sig *sig);
extern int __cxa_atexit(void (*func)(void *), void *arg, void *dso);

uint32_t shim_cxa_atexit(uint32_t *a) {
   const uint32_t func32 = a[0], arg32 = a[1], dso32 = a[2];
   if (!func32) { return 0; }
   /* CBA_PTR=2, CBR_VOID=0 (see cb_bridge.c). */
   static const struct cxa_cb_sig sig = { 1, 0, { 2 } };
   uint64_t tramp = x64_cb_wrap(func32, &sig);
   if (!tramp) { return 0; }   /* slots exhausted -> skip (no crash at exit) */
   return (uint32_t) __cxa_atexit((void (*)(void *))(uintptr_t) tramp,
                                  (void *)(uintptr_t) arg32,
                                  (void *)(uintptr_t) dso32);
}

/* ---------------- operator new / delete ---------------- */

static uint32_t cxx_alloc(uint32_t size, int may_abort, const char *which) {
   void *p = malloc(size ? size : 1);     /* shim malloc -> low-4GB heap */
   if (!p && may_abort) {
      fprintf(stderr, "[cxx] %s(%u) failed — legacy operator new cannot "
              "throw; aborting\n", which, size);
      fflush(stderr);
      abort();
   }
   return (uint32_t)(uintptr_t)p;
}

uint32_t shim_Znwm(uint32_t *a)  { return cxx_alloc(a[0], 1, "operator new"); }
uint32_t shim_Znam(uint32_t *a)  { return cxx_alloc(a[0], 1, "operator new[]"); }
uint32_t shim_Znwm_nothrow(uint32_t *a) { return cxx_alloc(a[0], 0, "new(nothrow)"); }
uint32_t shim_Znam_nothrow(uint32_t *a) { return cxx_alloc(a[0], 0, "new[](nothrow)"); }

uint32_t shim_ZdlPv(uint32_t *a) { free((void *)(uintptr_t)a[0]); return 0; }
uint32_t shim_ZdaPv(uint32_t *a) { free((void *)(uintptr_t)a[0]); return 0; }
uint32_t shim_ZdlPv_nothrow(uint32_t *a) { free((void *)(uintptr_t)a[0]); return 0; }

/* ---------------- std:: __throw_* helpers ---------------- */

static void cxx_throw_abort(const char *what, uint32_t msg32) {
   const char *m = msg32 ? (const char *)(uintptr_t)msg32 : "";
   fprintf(stderr, "[cxx] std::%s(\"%s\") from translated code — C++ "
           "exceptions cannot unwind translated frames; aborting\n", what, m);
   fflush(stderr);
   abort();
}

uint32_t shim_throw_bad_alloc(uint32_t *a)    { (void)a; cxx_throw_abort("__throw_bad_alloc", 0); return 0; }
uint32_t shim_throw_length_error(uint32_t *a) { cxx_throw_abort("__throw_length_error", a[0]); return 0; }
uint32_t shim_throw_out_of_range(uint32_t *a) { cxx_throw_abort("__throw_out_of_range", a[0]); return 0; }

/* ---------------- std::_Rb_tree_* (i386 node layout) ---------------- */

#define RB_RED   0u
#define RB_BLACK 1u

struct rb32 {                  /* std::_Rb_tree_node_base, i386 */
   uint32_t color;
   uint32_t parent;
   uint32_t left;
   uint32_t right;
};

static struct rb32 *RB(uint32_t p) { return (struct rb32 *)(uintptr_t)p; }
static uint32_t RP(const struct rb32 *n) { return (uint32_t)(uintptr_t)n; }

static uint32_t rb_minimum(uint32_t x) {
   while (RB(x)->left) { x = RB(x)->left; }
   return x;
}
static uint32_t rb_maximum(uint32_t x) {
   while (RB(x)->right) { x = RB(x)->right; }
   return x;
}

/* increment: header is recognized only via the decrement special case in
 * libstdc++; increment of rightmost lands on the header, as upstream. */
static uint32_t rb_increment(uint32_t x) {
   if (RB(x)->right) {
      return rb_minimum(RB(x)->right);
   }
   uint32_t y = RB(x)->parent;
   while (x == RB(y)->right) { x = y; y = RB(y)->parent; }
   if (RB(x)->right != y) { x = y; }
   return x;
}

static uint32_t rb_decrement(uint32_t x) {
   if (RB(x)->color == RB_RED && RB(RB(RB(x)->parent)->parent) == RB(x)) {
      return RB(x)->right;                /* x is the header: rightmost */
   }
   if (RB(x)->left) {
      return rb_maximum(RB(x)->left);
   }
   uint32_t y = RB(x)->parent;
   while (x == RB(y)->left) { x = y; y = RB(y)->parent; }
   return y;
}

uint32_t shim_rb_increment(uint32_t *a) { return a[0] ? rb_increment(a[0]) : 0; }
uint32_t shim_rb_decrement(uint32_t *a) { return a[0] ? rb_decrement(a[0]) : 0; }

static void rb_rotate_left(uint32_t x, uint32_t *root) {
   uint32_t y = RB(x)->right;
   RB(x)->right = RB(y)->left;
   if (RB(y)->left) { RB(RB(y)->left)->parent = x; }
   RB(y)->parent = RB(x)->parent;
   if (x == *root) { *root = y; }
   else if (x == RB(RB(x)->parent)->left) { RB(RB(x)->parent)->left = y; }
   else { RB(RB(x)->parent)->right = y; }
   RB(y)->left = x;
   RB(x)->parent = y;
}

static void rb_rotate_right(uint32_t x, uint32_t *root) {
   uint32_t y = RB(x)->left;
   RB(x)->left = RB(y)->right;
   if (RB(y)->right) { RB(RB(y)->right)->parent = x; }
   RB(y)->parent = RB(x)->parent;
   if (x == *root) { *root = y; }
   else if (x == RB(RB(x)->parent)->right) { RB(RB(x)->parent)->right = y; }
   else { RB(RB(x)->parent)->left = y; }
   RB(y)->right = x;
   RB(x)->parent = y;
}

/* void _Rb_tree_insert_and_rebalance(bool insert_left, node* x, node* p,
 *                                    node_base& header)
 * i386 frame: insert_left[0] x[1] p[2] header[3]. */
uint32_t shim_rb_insert_rebalance(uint32_t *a) {
   const int insert_left = (a[0] & 0xFF) != 0;
   uint32_t x = a[1], p = a[2], header = a[3];
   uint32_t *root = &RB(header)->parent;

   RB(x)->parent = p;
   RB(x)->left = 0;
   RB(x)->right = 0;
   RB(x)->color = RB_RED;

   if (insert_left) {
      RB(p)->left = x;                    /* also makes leftmost ok below */
      if (p == header) {
         RB(header)->parent = x;
         RB(header)->right = x;
      } else if (p == RB(header)->left) {
         RB(header)->left = x;
      }
   } else {
      RB(p)->right = x;
      if (p == RB(header)->right) {
         RB(header)->right = x;
      }
   }

   while (x != *root && RB(RB(x)->parent)->color == RB_RED) {
      uint32_t xpp = RB(RB(x)->parent)->parent;
      if (RB(x)->parent == RB(xpp)->left) {
         uint32_t y = RB(xpp)->right;
         if (y && RB(y)->color == RB_RED) {
            RB(RB(x)->parent)->color = RB_BLACK;
            RB(y)->color = RB_BLACK;
            RB(xpp)->color = RB_RED;
            x = xpp;
         } else {
            if (x == RB(RB(x)->parent)->right) {
               x = RB(x)->parent;
               rb_rotate_left(x, root);
            }
            RB(RB(x)->parent)->color = RB_BLACK;
            RB(xpp)->color = RB_RED;
            rb_rotate_right(xpp, root);
         }
      } else {
         uint32_t y = RB(xpp)->left;
         if (y && RB(y)->color == RB_RED) {
            RB(RB(x)->parent)->color = RB_BLACK;
            RB(y)->color = RB_BLACK;
            RB(xpp)->color = RB_RED;
            x = xpp;
         } else {
            if (x == RB(RB(x)->parent)->left) {
               x = RB(x)->parent;
               rb_rotate_right(x, root);
            }
            RB(RB(x)->parent)->color = RB_BLACK;
            RB(xpp)->color = RB_RED;
            rb_rotate_left(xpp, root);
         }
      }
   }
   RB(*root)->color = RB_BLACK;
   return 0;
}

/* node* _Rb_tree_rebalance_for_erase(node* z, node_base& header)
 * i386 frame: z[0] header[1]. Returns z. */
uint32_t shim_rb_rebalance_for_erase(uint32_t *a) {
   uint32_t z = a[0], header = a[1];
   uint32_t *root = &RB(header)->parent;
   uint32_t *leftmost = &RB(header)->left;
   uint32_t *rightmost = &RB(header)->right;
   uint32_t y = z, x = 0, x_parent = 0;

   if (RB(y)->left == 0) {                /* z has at most one non-null child */
      x = RB(y)->right;
   } else if (RB(y)->right == 0) {
      x = RB(y)->left;
   } else {                               /* two children: successor */
      y = rb_minimum(RB(y)->right);
      x = RB(y)->right;
   }

   if (y != z) {                          /* relink y in place of z */
      RB(RB(z)->left)->parent = y;
      RB(y)->left = RB(z)->left;
      if (y != RB(z)->right) {
         x_parent = RB(y)->parent;
         if (x) { RB(x)->parent = RB(y)->parent; }
         RB(RB(y)->parent)->left = x;
         RB(y)->right = RB(z)->right;
         RB(RB(z)->right)->parent = y;
      } else {
         x_parent = y;
      }
      if (*root == z) { *root = y; }
      else if (RB(RB(z)->parent)->left == z) { RB(RB(z)->parent)->left = y; }
      else { RB(RB(z)->parent)->right = y; }
      RB(y)->parent = RB(z)->parent;
      uint32_t tmp = RB(y)->color;
      RB(y)->color = RB(z)->color;
      RB(z)->color = tmp;
      y = z;                              /* y now points to the node to free */
   } else {                               /* y == z: one child at most */
      x_parent = RB(y)->parent;
      if (x) { RB(x)->parent = RB(y)->parent; }
      if (*root == z) { *root = x; }
      else if (RB(RB(z)->parent)->left == z) { RB(RB(z)->parent)->left = x; }
      else { RB(RB(z)->parent)->right = x; }
      if (*leftmost == z) {
         /* z is leftmost so z->left==0; if also no right child, parent (or
          * header when z was the root) becomes leftmost. */
         *leftmost = RB(z)->right == 0 ? RB(z)->parent : rb_minimum(x);
      }
      if (*rightmost == z) {
         *rightmost = RB(z)->left == 0 ? RB(z)->parent : rb_maximum(x);
      }
   }

   if (RB(y)->color != RB_RED) {          /* rebalance */
      while (x != *root && (x == 0 || RB(x)->color == RB_BLACK)) {
         if (x == RB(x_parent)->left) {
            uint32_t w = RB(x_parent)->right;
            if (RB(w)->color == RB_RED) {
               RB(w)->color = RB_BLACK;
               RB(x_parent)->color = RB_RED;
               rb_rotate_left(x_parent, root);
               w = RB(x_parent)->right;
            }
            if ((RB(w)->left == 0 || RB(RB(w)->left)->color == RB_BLACK) &&
                (RB(w)->right == 0 || RB(RB(w)->right)->color == RB_BLACK)) {
               RB(w)->color = RB_RED;
               x = x_parent;
               x_parent = RB(x_parent)->parent;
            } else {
               if (RB(w)->right == 0 ||
                   RB(RB(w)->right)->color == RB_BLACK) {
                  if (RB(w)->left) { RB(RB(w)->left)->color = RB_BLACK; }
                  RB(w)->color = RB_RED;
                  rb_rotate_right(w, root);
                  w = RB(x_parent)->right;
               }
               RB(w)->color = RB(x_parent)->color;
               RB(x_parent)->color = RB_BLACK;
               if (RB(w)->right) { RB(RB(w)->right)->color = RB_BLACK; }
               rb_rotate_left(x_parent, root);
               break;
            }
         } else {
            uint32_t w = RB(x_parent)->left;
            if (RB(w)->color == RB_RED) {
               RB(w)->color = RB_BLACK;
               RB(x_parent)->color = RB_RED;
               rb_rotate_right(x_parent, root);
               w = RB(x_parent)->left;
            }
            if ((RB(w)->right == 0 || RB(RB(w)->right)->color == RB_BLACK) &&
                (RB(w)->left == 0 || RB(RB(w)->left)->color == RB_BLACK)) {
               RB(w)->color = RB_RED;
               x = x_parent;
               x_parent = RB(x_parent)->parent;
            } else {
               if (RB(w)->left == 0 || RB(RB(w)->left)->color == RB_BLACK) {
                  if (RB(w)->right) { RB(RB(w)->right)->color = RB_BLACK; }
                  RB(w)->color = RB_RED;
                  rb_rotate_left(w, root);
                  w = RB(x_parent)->left;
               }
               RB(w)->color = RB(x_parent)->color;
               RB(x_parent)->color = RB_BLACK;
               if (RB(w)->left) { RB(RB(w)->left)->color = RB_BLACK; }
               rb_rotate_right(x_parent, root);
               break;
            }
         }
      }
      if (x) { RB(x)->color = RB_BLACK; }
   }
   (void)RP;
   return z;
}
