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
 * zero high-32 bits into offset 4 — zeroing __name. typeid(x).name() then reads
 * a NULL/garbage pointer, and boost::python's strcmp-keyed converter registry
 * SIGSEGVs on registration (Civ IV: _platform_strcmp(rdi=0)).
 *
 * The fix routes any bind whose target is a 4-byte slot to __DATA,__86x64_xrel,
 * which libabiconv binds with a 4-byte write (bind_external_relocs) — leaving
 * the adjacent __name field intact. This test defines a polymorphic hierarchy
 * (forcing __class_type_info objects into __const) and reads each type's name
 * via typeid; a leaked 8-byte bind prints a NULL / garbage name or crashes,
 * the routed 4-byte bind prints the correct mangled names.
 *
 * Needs `make sysroot-cpp` (uses <typeinfo> / RTTI). See tests-i386/Makefile.
 */
#include <cstdio>
#include <cstdlib>
#include <typeinfo>

struct Base {
   virtual int tag() const { return 0; }
   virtual ~Base() {}
};
struct Derived : Base {
   int v;
   Derived(int x) : v(x) {}
   int tag() const override { return v; }
};
struct Other : Base {
   double d;
   Other(double x) : d(x) {}
   int tag() const override { return (int)d; }
};

/* Keep the objects and typeid calls from being constant-folded away. */
static Base *make(int which) {
   switch (which) {
   case 0:  return new Base();
   case 1:  return new Derived(7);
   default: return new Other(3.5);
   }
}

int main() {
   Base *b = make(0);
   Base *d = make(1);
   Base *o = make(2);

   /* typeid(*p).name() reads the __class_type_info __name field at i386 +4 —
    * the exact field an 8-byte vtable bind at +0 would clobber to NULL. */
   const char *nb = typeid(*b).name();
   const char *nd = typeid(*d).name();
   const char *no = typeid(*o).name();

   printf("Base    name='%s'\n", nb ? nb : "(NULL)");
   printf("Derived name='%s'\n", nd ? nd : "(NULL)");
   printf("Other   name='%s'\n", no ? no : "(NULL)");

   /* boost::python compares registrations by strcmp of these names — prove the
    * names are non-null and distinct (a clobbered name is NULL -> would crash
    * or mis-compare here). */
   int c1 = __builtin_strcmp(nb, nd);
   int c2 = __builtin_strcmp(nd, no);
   printf("distinct: %s %s\n", c1 != 0 ? "yes" : "no", c2 != 0 ? "yes" : "no");

   /* Dynamic dispatch + dynamic_cast exercise the vtable/RTTI pointers too. */
   printf("tags: %d %d %d\n", b->tag(), d->tag(), o->tag());
   Derived *dc = dynamic_cast<Derived *>(d);
   printf("dynamic_cast<Derived>(d): %s\n", dc ? "ok" : "null");

   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
