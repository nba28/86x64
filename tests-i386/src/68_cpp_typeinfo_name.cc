/* 68_cpp_typeinfo_name.cc — the i386 libstdc++ RTTI DATA surface bridged by
 * libabiconv's cxx_typeinfo.c (the ____ZTI* fundamental / pointer / class
 * std::type_info objects reached after static-interpose redirects each __ZTI*
 * DATA bind).
 *
 * A GCC-4 C++ target that uses RTTI (typeid / boost::python converter
 * registration) references the fundamental type_info objects in libstdc++
 * (__ZTIb, __ZTIw, __ZTIPKw, ...).  On modern macOS libstdc++ is absent, so the
 * weak DATA bind resolves to a truncated >4GB / garbage pointer; the i386 code
 * reads type_info::name() = *(u32*)(&ti + 4) (i386 std::type_info layout
 * {vtable@0, __name@4}) and faults (Civ IV s23: boost::python::type_info::
 * type_info reading a garbage &ti at +4).  libabiconv now provides each typeinfo
 * as an i386-layout object at a low-4GB address with a valid __name; this test
 * reads those names exactly as boost does and prints them — the end-to-end proof
 * the redirected binds land on complete, name-bearing objects.
 */
#include <cstdio>
#include <cstdint>
#include <cstdlib>

/* i386 std::type_info: { const void* __vtable @0; const char* __name @4 }. */
struct i386_type_info { const void *vt; const char *name; };

extern i386_type_info ti_b   __asm__("__ZTIb");
extern i386_type_info ti_w   __asm__("__ZTIw");
extern i386_type_info ti_x   __asm__("__ZTIx");
extern i386_type_info ti_i   __asm__("__ZTIi");
extern i386_type_info ti_d   __asm__("__ZTId");
extern i386_type_info ti_PKw __asm__("__ZTIPKw");
extern i386_type_info ti_PKc __asm__("__ZTIPKc");
extern i386_type_info ti_exc __asm__("__ZTISt9exception");
extern i386_type_info ti_re  __asm__("__ZTISt13runtime_error");

static void show(const char *tag, const i386_type_info &ti) {
   /* name() must be a valid low-4GB pointer to the mangled type string; a NULL
    * or garbage pointer is the s23 failure mode. */
   printf("%-18s name='%s' vtable=%s\n", tag, ti.name ? ti.name : "(NULL)",
          ti.vt ? "set" : "NULL");
}

int main() {
   show("__ZTIb", ti_b);
   show("__ZTIw", ti_w);
   show("__ZTIx", ti_x);
   show("__ZTIi", ti_i);
   show("__ZTId", ti_d);
   show("__ZTIPKw", ti_PKw);
   show("__ZTIPKc", ti_PKc);
   show("__ZTISt9exception", ti_exc);
   show("__ZTISt13runtime_error", ti_re);

   /* boost::python compares registrations by strcmp of these names; prove two
    * distinct typeinfos have distinct, non-null, comparable names. */
   int c = __builtin_strcmp(ti_b.name, ti_w.name);
   printf("strcmp(b,w) %s 0\n", c < 0 ? "<" : (c > 0 ? ">" : "=="));

   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
