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
 *  - RTTI (__dynamic_cast, std::type_info::operator==/!=/before,
 *    __cxa_bad_typeid/__cxa_bad_cast/__cxa_pure_virtual + the three
 *    __cxxabiv1 type_info vtable DATA symbols): reimplemented on the i386
 *    Itanium-ABI typeinfo layout (4-byte pointer fields). See the RTTI block
 *    at the bottom of this file. The native libstdc++ __dynamic_cast walks
 *    8-byte typeinfo and would read garbage from the i386 structures.
 *
 * NOT shimmed (deliberate): __gxx_personality_v0 + __cxa_throw/begin/end_catch/
 * rethrow + _Unwind_Resume (only reachable from C++ EXCEPTION UNWINDING, which
 * cannot enter translated frames without a libabiconv-resident translated-frame
 * unwinder + an original<->translated PC map — the big remaining C++ gap).
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

/* std::set_new_handler state — the handler is an i386 FUNCTION POINTER and
 * stays entirely in the i386 world: translated C++ allocation goes through
 * cxx_alloc below (the native new-handler chain is never consulted). Bound
 * RAW, __ZSt15set_new_handlerPFvvE over-popped the 4-byte i386 frame (the
 * fused-PC family; Halo audio startup at receive_samples). */
static uint32_t cxx_new_handler32 = 0;

uint32_t shim_ZSt15set_new_handler(uint32_t *a) {
   uint32_t prev = cxx_new_handler32;
   cxx_new_handler32 = a[0];
   return prev;                    /* contract: return the PREVIOUS handler */
}

static uint32_t cxx_alloc(uint32_t size, int may_abort, const char *which) {
   void *p = malloc(size ? size : 1);     /* shim malloc -> low-4GB heap */
   if (!p && cxx_new_handler32) {
      /* libstdc++ contract: give the installed new-handler one chance to
       * release memory, then retry. Route through the generic i386-callback
       * trampoline (0 args, void return) so it runs on a low-4GB stack. */
      static const struct cxa_cb_sig nh_sig = { 0, 0, { 0 } };
      uint64_t tramp = x64_cb_wrap(cxx_new_handler32, &nh_sig);
      if (tramp) {
         ((void (*)(void))(uintptr_t)tramp)();
         p = malloc(size ? size : 1);
      }
   }
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

/* ==========================================================================
 * RTTI: __dynamic_cast / std::type_info compare / __cxa_bad_* / pure_virtual
 *
 * Reimplemented on the i386 Itanium-C++-ABI typeinfo layout (4-byte pointer
 * fields). Native libstdc++/libc++abi __dynamic_cast walks 8-byte typeinfo
 * structures, so handing it the i386 4-byte-field typeinfo objects the
 * translated binary carries would read garbage. We never call native; we walk
 * the i386 hierarchy ourselves (a faithful port of libsupc++'s class/si/vmi
 * __do_dyncast + __do_find_public_src).
 *
 * SELF-DESCRIBING TYPEINFO. Every typeinfo's first field (+0) points at one of
 * three ABI vtables. libabiconv EXPORTS those three vtable symbols (asm-global
 * sentinels in maptable_tramp.asm) under their exact libstdc++ names, so
 * static-interpose rebinds each typeinfo's vtable-ptr to a known low-4GB
 * sentinel (the i386 +8 address-point addend lands inside it). Binding instead
 * to the native 8-byte vtable would truncate to an unusable 32-bit value.
 * Reading +0 and matching the sentinel range tells us the typeinfo kind.
 *
 * i386 typeinfo layouts (measured from a translated test binary):
 *   __class_type_info     : {vtbl@0, name@4}                          (8 B)
 *   __si_class_type_info  : {vtbl@0, name@4, base@8}                  (12 B)
 *   __vmi_class_type_info : {vtbl@0, name@4, flags@8, base_count@12,
 *                            {base_type, offset_flags}[base_count] @16}
 *      base_info entry = 8 B (base_type@+0, offset_flags@+4, i386).
 *      offset_flags: bit0=virtual, bit1=public, (signed val>>8)=offset.
 * ======================================================================== */

/* The three ABI vtable sentinels (defined+exported in maptable_tramp.asm). We
 * only ever take their addresses — the bytes are inert. */
extern const uint8_t cxxabi_class_vtbl[] __asm__("____ZTVN10__cxxabiv117__class_type_infoE");
extern const uint8_t cxxabi_si_vtbl[]    __asm__("____ZTVN10__cxxabiv120__si_class_type_infoE");
extern const uint8_t cxxabi_vmi_vtbl[]   __asm__("____ZTVN10__cxxabiv121__vmi_class_type_infoE");

enum ti_kind { TI_UNKNOWN = 0, TI_CLASS, TI_SI, TI_VMI };

static inline uint32_t ld32(uint32_t p)  { return *(uint32_t *)(uintptr_t)p; }
static inline int32_t  ld32s(uint32_t p) { return *(int32_t  *)(uintptr_t)p; }

static enum ti_kind ti_kind_of(uint32_t ti) {
   if (!ti) return TI_UNKNOWN;
   uint32_t v  = ld32(ti);                            /* typeinfo vtable-ptr (+0) */
   uint32_t cb = (uint32_t)(uintptr_t)cxxabi_class_vtbl;
   uint32_t sb = (uint32_t)(uintptr_t)cxxabi_si_vtbl;
   uint32_t vb = (uint32_t)(uintptr_t)cxxabi_vmi_vtbl;
   if (v >= cb && v < cb + 16) return TI_CLASS;
   if (v >= sb && v < sb + 16) return TI_SI;
   if (v >= vb && v < vb + 16) return TI_VMI;
   return TI_UNKNOWN;                                 /* native/foreign typeinfo */
}

/* std::type_info equality on the i386 layout: same typeinfo, else same name
 * pointer, else strcmp (unless an '*'-prefixed address-only name). */
static int ti_equal(uint32_t a, uint32_t b) {
   if (a == b) return 1;
   if (!a || !b) return 0;
   uint32_t na = ld32(a + 4), nb = ld32(b + 4);
   if (na == nb) return 1;
   if (!na || !nb) return 0;
   const char *sa = (const char *)(uintptr_t)na;
   const char *sb = (const char *)(uintptr_t)nb;
   if (sa[0] == '*' || sb[0] == '*') return 0;
   return strcmp(sa, sb) == 0;
}

/* __sub_kind access-path flags (Itanium ABI / libsupc++). */
#define SK_unknown           0
#define SK_not_contained     1
#define SK_contained_ambig   2
#define SK_virtual_mask      0x1
#define SK_public_mask       0x2
#define SK_contained_mask    0x10
#define SK_contained_public  (SK_contained_mask | SK_public_mask)   /* 0x12 */
#define VMI_FLAGS_UNKNOWN    0x10   /* __flags_unknown_mask sentinel */

static int sk_contained_p(int k)        { return k >= SK_contained_mask; }
static int sk_public_p(int k)           { return (k & SK_public_mask) != 0; }
static int sk_contained_public_p(int k) { return (k & SK_contained_public) == SK_contained_public; }
static int sk_contained_nonvirtual_p(int k) {
   return (k & (SK_contained_mask | SK_virtual_mask)) == SK_contained_mask;
}

/* __vmi_class_type_info::__base_info[i] accessors (i386). */
static uint32_t bi_base_type(uint32_t ti, uint32_t i) { return ld32(ti + 16 + 8 * i); }
static int32_t  bi_offset(uint32_t ti, uint32_t i)    { return ld32s(ti + 16 + 8 * i + 4) >> 8; }
static int      bi_is_virtual(uint32_t ti, uint32_t i){ return (ld32(ti + 16 + 8 * i + 4) & SK_virtual_mask) != 0; }
static int      bi_is_public(uint32_t ti, uint32_t i) { return (ld32(ti + 16 + 8 * i + 4) & SK_public_mask) != 0; }

/* obj_ptr adjusted to a base subobject. Virtual bases read their offset from
 * the object's vtable at the (signed) in-vtable slot given by `offset`. */
static uint32_t to_base(uint32_t obj_ptr, int is_virtual, int32_t offset) {
   if (is_virtual) {
      uint32_t vtbl = ld32(obj_ptr);
      offset = ld32s((uint32_t)((int32_t)vtbl + offset));
   }
   return (uint32_t)((int32_t)obj_ptr + offset);
}

struct dyncast_result {
   uint32_t dst_ptr;       /* found dst_type subobject (0 = not found) */
   int      whole2dst;     /* access path most-derived -> dst */
   int      whole2src;     /* access path most-derived -> src */
   int      dst2src;       /* relationship dst -> src */
   int      whole_details; /* cached vmi __flags */
};

/* Search the dst object for a publicly-reachable src subobject (libsupc++
 * __do_find_public_src). Used when the static src2dst hint is unavailable. */
static int do_find_public_src(uint32_t ti, int32_t src2dst, uint32_t obj_ptr,
                              uint32_t src_type, uint32_t src_ptr) {
   if (obj_ptr == src_ptr && ti_equal(ti, src_type))
      return SK_contained_public;
   switch (ti_kind_of(ti)) {
   case TI_SI:
      return do_find_public_src(ld32(ti + 8), src2dst, obj_ptr, src_type, src_ptr);
   case TI_VMI: {
      uint32_t n = ld32(ti + 12);
      for (uint32_t i = n; i--; ) {
         if (!bi_is_public(ti, i)) continue;
         int is_virtual = bi_is_virtual(ti, i);
         if (is_virtual && src2dst == -3) continue;   /* src not a virtual base */
         uint32_t base = to_base(obj_ptr, is_virtual, bi_offset(ti, i));
         int k = do_find_public_src(bi_base_type(ti, i), src2dst, base, src_type, src_ptr);
         if (sk_contained_p(k)) {
            if (is_virtual) k |= SK_virtual_mask;
            return k;
         }
      }
      return SK_not_contained;
   }
   case TI_CLASS:
   default:
      return SK_not_contained;
   }
}

static int find_public_src(uint32_t ti, int32_t src2dst, uint32_t obj_ptr,
                           uint32_t src_type, uint32_t src_ptr) {
   if (src2dst >= 0)
      return ((uint32_t)((int32_t)obj_ptr + src2dst) == src_ptr)
             ? SK_contained_public : SK_not_contained;
   if (src2dst == -2)
      return SK_not_contained;
   return do_find_public_src(ti, src2dst, obj_ptr, src_type, src_ptr);
}

/* Recursive most-derived-object walk (libsupc++ class/si/vmi __do_dyncast).
 * Records, in *res, the dst_type subobject and the access paths to src/dst. */
static void do_dyncast(uint32_t ti, int32_t src2dst, int access_path,
                       uint32_t dst_type, uint32_t obj_ptr,
                       uint32_t src_type, uint32_t src_ptr,
                       struct dyncast_result *res) {
   if (obj_ptr == src_ptr && ti_equal(ti, src_type)) {
      res->whole2src = access_path;               /* this is the source subobject */
      return;
   }
   if (ti_equal(ti, dst_type)) {                  /* a dst_type subobject */
      res->dst_ptr = obj_ptr;
      res->whole2dst = access_path;
      if (src2dst >= 0)
         res->dst2src = ((uint32_t)((int32_t)obj_ptr + src2dst) == src_ptr)
                        ? SK_contained_public : SK_not_contained;
      else if (src2dst == -2)
         res->dst2src = SK_not_contained;
      return;
   }

   switch (ti_kind_of(ti)) {
   case TI_CLASS:
      return;                                      /* leaf: no bases */
   case TI_SI:                                     /* single public base @ off 0 */
      do_dyncast(ld32(ti + 8), src2dst, access_path, dst_type,
                 obj_ptr, src_type, src_ptr, res);
      return;
   case TI_VMI:
      break;
   default:
      return;
   }

   if (res->whole_details & VMI_FLAGS_UNKNOWN)
      res->whole_details = ld32(ti + 8);           /* vmi __flags */

   uint32_t n = ld32(ti + 12);
   for (uint32_t i = n; i--; ) {
      struct dyncast_result r2;
      memset(&r2, 0, sizeof r2);
      r2.dst2src = SK_unknown;
      r2.whole_details = res->whole_details;

      int is_virtual = bi_is_virtual(ti, i);
      uint32_t base = to_base(obj_ptr, is_virtual, bi_offset(ti, i));
      int base_access = access_path;
      if (!bi_is_public(ti, i))
         base_access &= ~SK_public_mask;

      do_dyncast(bi_base_type(ti, i), src2dst, base_access,
                 dst_type, base, src_type, src_ptr, &r2);

      res->whole2src |= r2.whole2src;              /* accumulate src reachability */

      if (r2.dst_ptr) {
         if (!res->dst_ptr) {                      /* first dst found */
            res->dst_ptr   = r2.dst_ptr;
            res->whole2dst = r2.whole2dst;
            res->dst2src   = r2.dst2src;
         } else if (res->dst_ptr == r2.dst_ptr) {  /* same subobject, 2nd path */
            res->whole2dst |= r2.whole2dst;
            if (r2.dst2src != SK_unknown)
               res->dst2src = (res->dst2src == SK_unknown)
                              ? r2.dst2src : (res->dst2src | r2.dst2src);
         } else {                                  /* distinct dst -> ambiguous */
            res->dst2src = SK_contained_ambig;
         }
      }
   }
}

/* void* __dynamic_cast(const void* sub, const __class_type_info* src,
 *                      const __class_type_info* dst, ptrdiff_t src2dst).
 * i386 frame: sub[0] src[1] dst[2] src2dst[3]. */
uint32_t shim_dynamic_cast(uint32_t *a) {
   uint32_t src_ptr  = a[0];
   uint32_t src_type = a[1];
   uint32_t dst_type = a[2];
   int32_t  src2dst  = (int32_t)a[3];
   if (!src_ptr) return 0;

   uint32_t vtable = ld32(src_ptr);
   if (!vtable) return 0;
   int32_t  off_to_top = ld32s(vtable - 8);          /* i386 vtable_prefix */
   uint32_t whole_type = ld32(vtable - 4);
   uint32_t whole_ptr  = (uint32_t)((int32_t)src_ptr + off_to_top);

   /* If the most-derived typeinfo isn't one we recognize (its vtable bind was
    * not redirected — a native/foreign type), we cannot walk it safely. Fail
    * the cast rather than risk a wild read. */
   if (ti_kind_of(whole_type) == TI_UNKNOWN) return 0;

   struct dyncast_result res;
   memset(&res, 0, sizeof res);
   res.whole2src = SK_unknown;
   res.whole2dst = SK_unknown;
   res.dst2src   = SK_unknown;
   res.whole_details = VMI_FLAGS_UNKNOWN;

   do_dyncast(whole_type, src2dst, SK_contained_public, dst_type,
              whole_ptr, src_type, src_ptr, &res);

   if (!res.dst_ptr) return 0;
   if (res.dst2src == SK_contained_ambig) return 0;
   if (sk_contained_public_p(res.dst2src))
      return res.dst_ptr;                            /* src is a public base of dst */
   if (sk_contained_public_p(res.whole2src & res.whole2dst))
      return res.dst_ptr;                            /* valid public cross-cast */
   if (sk_contained_nonvirtual_p(res.whole2src))
      return 0;                                      /* src uniquely found, not in dst */
   if (res.dst2src == SK_unknown)
      res.dst2src = find_public_src(dst_type, src2dst, res.dst_ptr, src_type, src_ptr);
   if (sk_contained_public_p(res.dst2src))
      return res.dst_ptr;
   return 0;
}

/* ---------------- std::type_info comparison operators ---------------- */
/* bool type_info::operator==(const type_info&) const  — i386: this[0] arg[1].
 * Out-of-line in GCC-era libstdc++ (the real i386 targets); modern libc++
 * headers inline it, so a libc++-headers test won't import these — they exist
 * for the GCC-built binaries (Halo/Civ IV). */
uint32_t shim_type_info_eq(uint32_t *a) { return ti_equal(a[0], a[1]) ? 1u : 0u; }
uint32_t shim_type_info_ne(uint32_t *a) { return ti_equal(a[0], a[1]) ? 0u : 1u; }

/* bool type_info::before(const type_info&) const — orders by mangled name. */
uint32_t shim_type_info_before(uint32_t *a) {
   uint32_t ta = a[0], tb = a[1];
   if (ta == tb) return 0;
   uint32_t na = ld32(ta + 4), nb = ld32(tb + 4);
   if (na == nb) return 0;
   const char *sa = na ? (const char *)(uintptr_t)na : "";
   const char *sb = nb ? (const char *)(uintptr_t)nb : "";
   if (sa[0] == '*' || sb[0] == '*') return na < nb ? 1u : 0u;  /* address-only */
   return strcmp(sa, sb) < 0 ? 1u : 0u;
}

/* ---------------- __cxa_bad_typeid / __cxa_bad_cast / __cxa_pure_virtual --- */
/* All three are noreturn diagnostics in normal libstdc++; they would throw
 * (bad_typeid / bad_cast) or trap (pure_virtual), and a C++ throw cannot unwind
 * translated frames. Abort loudly with a clear message (same discipline as the
 * __throw_* shims above). */
uint32_t shim_cxa_bad_typeid(uint32_t *a) {
   (void)a;
   fprintf(stderr, "[cxx] typeid applied to a null polymorphic pointer "
           "(std::bad_typeid) from translated code; aborting\n");
   fflush(stderr); abort();
}
uint32_t shim_cxa_bad_cast(uint32_t *a) {
   (void)a;
   fprintf(stderr, "[cxx] failed dynamic_cast<T&> (std::bad_cast) from "
           "translated code; aborting\n");
   fflush(stderr); abort();
}
uint32_t shim_cxa_pure_virtual(uint32_t *a) {
   (void)a;
   fprintf(stderr, "[cxx] pure virtual function called from translated code; "
           "aborting\n");
   fflush(stderr); abort();
}
