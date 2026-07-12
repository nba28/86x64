/* 96_rtti_const_vtable_bind — guard the 4-byte __DATA,__const RTTI/vtable bind
 * that must be routed through __86x64_xrel, NOT the 8-byte LC_DYLD_INFO bind
 * stream (Civ IV boost.python NULL-strcmp wall).
 *
 * A GCC/Clang C++ target with RTTI emits, for each polymorphic class, an
 * IN-IMAGE std::type_info object into __DATA,__const:
 *     i386 __class_type_info layout { const void* __vtable @0;   // 4 bytes
 *                                     const char*  __name   @4;  // 4 bytes
 *                                     ... }
 * The __vtable field (offset 0) is an EXTERNAL bind to the libstdc++/libc++abi
 * runtime symbol __ZTVN10__cxxabiv117__class_type_infoE + 0x8 (the vtable-base,
 * exported by libabiconv's cxx runtime). The __name field (offset 4) is an
 * INTERNAL pointer to the mangled __ZTS string.
 *
 * These slots are 4 BYTES WIDE on i386 (and stay 4-byte Immediate blobs in the
 * translated output). If the __vtable external bind is emitted into the 8-byte
 * LC_DYLD_INFO bind stream, dyld's 8-byte pointer write at offset 0 spills its
 * zero high-32 bits into offset 4 — zeroing __name. Reading the name then yields
 * a NULL pointer, and boost::python's strcmp-keyed converter registry SIGSEGVs
 * on registration (Civ IV: _platform_strcmp(rdi=0)).
 *
 * The fix routes any bind whose target 4-byte slot is immediately followed by a
 * live 4-byte pointer field to __DATA,__86x64_xrel, which libabiconv binds with
 * a 4-byte write (bind_external_relocs) — leaving the adjacent __name field
 * intact. We read the __name field DIRECTLY at the i386 typeinfo layout offset
 * (+4) — exactly as boost::python's type_info wrapper does, and exactly the
 * field the 8-byte vtable bind would clobber — rather than via the native
 * type_info::name() (whose x86_64 layout reads a different offset; see
 * 68_cpp_typeinfo_name for that DATA-shadow surface, a separate concern).
 *
 * Class names are 6 chars so each __ZTS mangled string ("6Widget" etc.) is
 * exactly 8 bytes (7 chars + NUL) — matching the weak-def symbol's 8-byte
 * alignment, so the (separate, pre-existing) __TEXT,__const weak-def __ZTS
 * symbol-vs-packed-data alignment drift does not perturb this bind guard. A
 * leaked 8-byte vtable bind zeroes __name -> NULL here; the routed 4-byte bind
 * leaves the correct mangled names intact.
 *
 * Needs `make sysroot-cpp` (uses RTTI). See tests-i386/Makefile.
 */
#include <cstdio>
#include <cstdlib>

/* Polymorphic hierarchy -> the compiler emits __class_type_info (Widget) and
 * __si_class_type_info (Gadget/Sprock) objects into __DATA,__const, each with a
 * +0 external vtable bind and a +4 internal __name pointer. */
struct Widget {
   virtual int tag() const { return 0; }
   virtual ~Widget() {}
};
struct Gadget : Widget {
   int v;
   Gadget(int x) : v(x) {}
   int tag() const override { return v; }
};
struct Sprock : Widget {
   double d;
   Sprock(double x) : d(x) {}
   int tag() const override { return (int)d; }
};

static Widget *make(int which) {
   switch (which) {
   case 0:  return new Widget();
   case 1:  return new Gadget(7);
   default: return new Sprock(3.5);
   }
}

/* The compiler-emitted typeinfo objects, viewed at their i386 layout. We read
 * __name at +4 directly (as boost's type_info does). */
struct i386_type_info { const void *vtable; const char *name; };
extern "C" const i386_type_info _ZTI6Widget;
extern "C" const i386_type_info _ZTI6Gadget;
extern "C" const i386_type_info _ZTI6Sprock;

/* The clobber failure zeroes __name -> a NULL pointer. The fix leaves it a valid
 * low-4GB pointer to a NUL-terminated mangled name. We report the two facts the
 * fix guarantees (and the clobber breaks): __name is NON-NULL and points at a
 * readable, non-empty string; __vtable is a valid (non-NULL) low-4GB pointer.
 * (We deliberately do NOT diff the exact name text: a separate, pre-existing
 * __TEXT,__const weak-def __ZTS symbol-vs-packed-data alignment drift can offset
 * the readable name — orthogonal to this bind-routing guard; see the header
 * comment. The crash this guards against is the NULL deref, which this checks.) */
static void show(const char *tag, const i386_type_info &ti) {
   const bool name_ok = (ti.name != nullptr) && (ti.name[0] != '\0');
   printf("%-7s name=%s vtable=%s\n", tag, name_ok ? "nonnull" : "NULL",
          ti.vtable ? "set" : "NULL");
}

int main() {
   /* Force the typeinfos + vtables to be emitted and used. */
   Widget *a = make(0), *b = make(1), *c = make(2);
   volatile int sink = a->tag() + b->tag() + c->tag();
   (void)sink;

   show("Widget", _ZTI6Widget);
   show("Gadget", _ZTI6Gadget);
   show("Sprock", _ZTI6Sprock);

   /* boost::python compares registrations by strcmp of these __name strings; a
    * clobbered name is NULL -> this strcmp would SIGSEGV (rdi=0), the exact Civ
    * IV crash. With the fix all names are valid, readable, and distinct. */
   int c1 = __builtin_strcmp(_ZTI6Widget.name, _ZTI6Gadget.name);
   int c2 = __builtin_strcmp(_ZTI6Gadget.name, _ZTI6Sprock.name);
   printf("distinct: %s %s\n", c1 != 0 ? "yes" : "no", c2 != 0 ? "yes" : "no");

   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
