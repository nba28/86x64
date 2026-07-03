/*
 * cxx_typeinfo.c — i386-layout libstdc++ RTTI DATA surface (fundamental /
 * pointer / std-class std::type_info objects + the __cxxabiv1 type_info
 * vtable sentinels) resident in libabiconv's low-4GB image.
 *
 * WHY THIS EXISTS (Civ IV s23)
 * ---------------------------
 * A GCC-4 i386 C++ target that uses RTTI (typeid / dynamic_cast / boost::python
 * converter registration) references the fundamental type_info objects that
 * live in libstdc++:  __ZTIb (bool), __ZTIw (wchar_t), __ZTIx (long long),
 * __ZTIPKw (const wchar_t*), the std exception classes, etc.  On modern macOS
 * libstdc++ is absent, so those weak external DATA binds resolve to nothing /
 * a truncated >4GB address.  The i386 code loads `&typeid(T)` as a 4-byte
 * pointer and dereferences it — e.g. boost::python::type_info::type_info(
 * std::type_info const& ti) reads `ti.name()` = *(u32*)(&ti + 4) — SIGSEGV on
 * a garbage pointer (Civ IV static-init: CyTranslator's std::wstring converter
 * registration, &ti = 0x4279AB70, fault at ti+4).
 *
 * Native x86_64 libstdc++'s type_info is ALSO layout-incompatible (8-byte
 * fields vs the i386 4-byte {vtable,__name}), so the objects cannot be
 * forwarded; they are reimplemented here in i386 layout at a LOW-4GB address
 * (libabiconv itself loads low-4GB), and static-interpose redirects each
 * __ZTI<code> / __ZTVN10__cxxabiv1*<...> DATA bind to the ____-prefixed export
 * below.
 *
 * The 4-byte pointer FIELDS (vtable, __name) can't be a static initializer
 * (x86_64 Mach-O has no 4-byte absolute reloc), so they are filled at load by
 * a HIGH-priority constructor — priority 101 runs before objc_slide_init
 * (default priority) registers the add-image callback that runs the app's own
 * static initializers, so every typeinfo is complete before any app RTTI use.
 *
 * boost::python (the reachable path) reads only __name @ +4 and compares by
 * string, so a valid low-4GB __name is the load-bearing field; the vtable @ +0
 * points at the matching __cxxabiv1 sentinel (address point +8) so a virtual
 * type_info call or cxx_shim's __dynamic_cast kind-probe never reads garbage.
 * Universal: keyed on the RTTI ABI, no app-specific behavior.
 */

#include <stdint.h>

/* __cxxabiv1 type_info vtable sentinels (16B, zero; the typeinfo stores
 * sentinel+8, the i386 ABI address point, which lands inside the block).  The
 * 17class / 20si_class / 21vmi_class sentinels are defined in
 * maptable_tramp.asm; we add the enum / pointer / function / fundamental ones
 * the app also imports (and our fundamental/pointer objects reference). */
uint8_t g_vt_enum[16] __asm__("____ZTVN10__cxxabiv116__enum_type_infoE");
uint8_t g_vt_ptr[16]  __asm__("____ZTVN10__cxxabiv119__pointer_type_infoE");
uint8_t g_vt_func[16] __asm__("____ZTVN10__cxxabiv120__function_type_infoE");
uint8_t g_vt_fund[16] __asm__("____ZTVN10__cxxabiv123__fundamental_type_infoE");
/* class_type_info sentinel: reuse the one exported by maptable_tramp.asm. */
extern uint8_t g_vt_class[] __asm__("____ZTVN10__cxxabiv117__class_type_infoE");

enum { VT_FUND, VT_PTR, VT_CLASS };

/* AUTO-CONTENT: objects + table (edit gen_ti.py, not here). */
/* --- fundamental type_info objects --- */
uint32_t ti_v[4]          __asm__("____ZTIv");
uint32_t ti_w[4]          __asm__("____ZTIw");
uint32_t ti_b[4]          __asm__("____ZTIb");
uint32_t ti_c[4]          __asm__("____ZTIc");
uint32_t ti_a[4]          __asm__("____ZTIa");
uint32_t ti_h[4]          __asm__("____ZTIh");
uint32_t ti_s[4]          __asm__("____ZTIs");
uint32_t ti_t[4]          __asm__("____ZTIt");
uint32_t ti_i[4]          __asm__("____ZTIi");
uint32_t ti_j[4]          __asm__("____ZTIj");
uint32_t ti_l[4]          __asm__("____ZTIl");
uint32_t ti_m[4]          __asm__("____ZTIm");
uint32_t ti_x[4]          __asm__("____ZTIx");
uint32_t ti_y[4]          __asm__("____ZTIy");
uint32_t ti_n[4]          __asm__("____ZTIn");
uint32_t ti_o[4]          __asm__("____ZTIo");
uint32_t ti_f[4]          __asm__("____ZTIf");
uint32_t ti_d[4]          __asm__("____ZTId");
uint32_t ti_e[4]          __asm__("____ZTIe");
uint32_t ti_g[4]          __asm__("____ZTIg");
uint32_t ti_z[4]          __asm__("____ZTIz");
/* --- pointer type_info objects --- */
uint32_t ti_Pc[4]         __asm__("____ZTIPc");
uint32_t ti_Pi[4]         __asm__("____ZTIPi");
uint32_t ti_Pv[4]         __asm__("____ZTIPv");
uint32_t ti_Pw[4]         __asm__("____ZTIPw");
uint32_t ti_Ph[4]         __asm__("____ZTIPh");
uint32_t ti_PKc[4]        __asm__("____ZTIPKc");
uint32_t ti_PKw[4]        __asm__("____ZTIPKw");
uint32_t ti_PKv[4]        __asm__("____ZTIPKv");
uint32_t ti_PKi[4]        __asm__("____ZTIPKi");
/* --- std class type_info objects --- */
uint32_t ti_St9exception[4] __asm__("____ZTISt9exception");
uint32_t ti_St9bad_alloc[4] __asm__("____ZTISt9bad_alloc");
uint32_t ti_St8bad_cast[4] __asm__("____ZTISt8bad_cast");
uint32_t ti_St13bad_exception[4] __asm__("____ZTISt13bad_exception");
uint32_t ti_St13runtime_error[4] __asm__("____ZTISt13runtime_error");
uint32_t ti_St11logic_error[4] __asm__("____ZTISt11logic_error");

static const struct { uint32_t *obj; const char *name; int kind; } g_ti[] = {
   { ti_v,          "v", VT_FUND },
   { ti_w,          "w", VT_FUND },
   { ti_b,          "b", VT_FUND },
   { ti_c,          "c", VT_FUND },
   { ti_a,          "a", VT_FUND },
   { ti_h,          "h", VT_FUND },
   { ti_s,          "s", VT_FUND },
   { ti_t,          "t", VT_FUND },
   { ti_i,          "i", VT_FUND },
   { ti_j,          "j", VT_FUND },
   { ti_l,          "l", VT_FUND },
   { ti_m,          "m", VT_FUND },
   { ti_x,          "x", VT_FUND },
   { ti_y,          "y", VT_FUND },
   { ti_n,          "n", VT_FUND },
   { ti_o,          "o", VT_FUND },
   { ti_f,          "f", VT_FUND },
   { ti_d,          "d", VT_FUND },
   { ti_e,          "e", VT_FUND },
   { ti_g,          "g", VT_FUND },
   { ti_z,          "z", VT_FUND },
   { ti_Pc,         "Pc", VT_PTR  },
   { ti_Pi,         "Pi", VT_PTR  },
   { ti_Pv,         "Pv", VT_PTR  },
   { ti_Pw,         "Pw", VT_PTR  },
   { ti_Ph,         "Ph", VT_PTR  },
   { ti_PKc,        "PKc", VT_PTR  },
   { ti_PKw,        "PKw", VT_PTR  },
   { ti_PKv,        "PKv", VT_PTR  },
   { ti_PKi,        "PKi", VT_PTR  },
   { ti_St9exception, "St9exception", VT_CLASS},
   { ti_St9bad_alloc, "St9bad_alloc", VT_CLASS},
   { ti_St8bad_cast, "St8bad_cast", VT_CLASS},
   { ti_St13bad_exception, "St13bad_exception", VT_CLASS},
   { ti_St13runtime_error, "St13runtime_error", VT_CLASS},
   { ti_St11logic_error, "St11logic_error", VT_CLASS},
};

/* Fill each typeinfo's i386 {vtable@0, __name@4} before any app initializer.
 * Mach-O does not reliably honor constructor PRIORITIES, so a plain constructor
 * here could run AFTER objc_slide.c's objc_slide_init (linked earlier) — which,
 * on registering its add-image callback, synchronously runs the translated
 * app's static initializers (boost::python converter registration reads these
 * typeinfos' __name).  objc_slide_init therefore CALLS this explicitly before
 * registering; the constructor is a belt-and-suspenders fallback.  Idempotent
 * (guarded) so the double invocation is a harmless no-op. */
void _86x64_cxx_typeinfo_init(void);
__attribute__((constructor))
void _86x64_cxx_typeinfo_init(void) {
   static int done = 0;
   if (done) { return; }
   done = 1;
   uint8_t *vt[3];
   vt[VT_FUND]  = g_vt_fund;
   vt[VT_PTR]   = g_vt_ptr;
   vt[VT_CLASS] = g_vt_class;
   for (unsigned i = 0; i < sizeof(g_ti) / sizeof(g_ti[0]); i++) {
      /* +8 = the Itanium i386 ABI vtable address point (2 leading words). */
      g_ti[i].obj[0] = (uint32_t)(uintptr_t)(vt[g_ti[i].kind] + 8);
      g_ti[i].obj[1] = (uint32_t)(uintptr_t)g_ti[i].name;
      g_ti[i].obj[2] = 0;   /* __pointer_type_info __flags (unused by name cmp) */
      g_ti[i].obj[3] = 0;   /* __pointer_type_info __pointee                    */
   }
}
