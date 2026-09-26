/*
 * iostream_shim.c — the output half of i386 libstdc++ iostreams and the
 * locale/numpunct surface, for translated C++ that binds them (PvZ's PopCap
 * framework: stringstream/ostringstream formatting, a global locale +
 * use_facet<numpunct<char>>). Bound raw they ran NATIVE libstdc++ code on
 * i386-sized objects (std::locale::locale(const char*) SIGSEGV in static init).
 *
 * The i386 object's bytes are never touched: each stream is a small native
 * state (a growable buffer) found by ADDRESS. A GCC 4.x i386 stringstream is
 * {istream @0 (vptr,gcount), ostream @8, stringbuf @0xc, ios virtual base};
 * an ostringstream is {ostream @0, stringbuf @4, ios}. Every address the app
 * can hand back (object, ostream subobject, stringbuf) maps to the state.
 * Output only: PvZ imports no operator>>. Unsupported manipulators log once.
 *
 * Trampolines: cow_string_tramp.asm (MTSHIM / MTSHIM_SRET for by-value strings).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void cow_str_init(uint32_t obj, uint32_t buf, uint32_t n);   /* cow_string_shim.c */

struct ss { uint32_t obj, os, sb; char *buf; uint32_t len, cap; };

/* ponytail: linear table of live streams; a hash if an app keeps thousands alive. */
#define MAXSS 256
static struct ss g_ss[MAXSS];

static struct ss *ss_find(uint32_t addr) {
   for (int i = 0; i < MAXSS; i++) {
      struct ss *s = &g_ss[i];
      if (s->obj && (s->obj == addr || s->os == addr || s->sb == addr)) { return s; }
   }
   return NULL;
}
static void ss_open(uint32_t obj, uint32_t os_off, uint32_t sb_off) {
   struct ss *s = ss_find(obj);
   for (int i = 0; !s && i < MAXSS; i++) { if (!g_ss[i].obj) { s = &g_ss[i]; } }
   if (!s) { fprintf(stderr, "iostream_shim: >%d live streams\n", MAXSS); abort(); }
   free(s->buf);
   *s = (struct ss){ obj, obj + os_off, obj + sb_off, NULL, 0, 0 };
}
static void ss_close(uint32_t obj) {
   struct ss *s = ss_find(obj);
   if (s) { free(s->buf); memset(s, 0, sizeof *s); }
}
static void ss_put(uint32_t os, const char *p, uint32_t n) {
   struct ss *s = ss_find(os);
   if (!s) { fwrite(p, 1, n, stderr); return; }   /* cout/cerr or unknown: stderr */
   if (s->len + n + 1 > s->cap) {
      s->cap = (s->len + n + 1) * 2;
      s->buf = realloc(s->buf, s->cap);   /* libabiconv malloc = low heap */
   }
   memcpy(s->buf + s->len, p, n);
   s->len += n;
}
static uint32_t ss_str(uint32_t sret, uint32_t addr) {
   struct ss *s = ss_find(addr);
   cow_str_init(sret, s ? (uint32_t)(uintptr_t)s->buf : 0, s ? s->len : 0);
   return sret;
}

/* ctors(this, openmode) / dtors(this) */
uint32_t shim_stringstream_ctor(uint32_t *a)  { ss_open(a[0], 8, 0xc); return a[0]; }
uint32_t shim_ostringstream_ctor(uint32_t *a) { ss_open(a[0], 0, 4); return a[0]; }
uint32_t shim_sstream_dtor(uint32_t *a)       { ss_close(a[0]); return 0; }
/* str() const — by value: a[0] = hidden sret slot, a[1] = this */
uint32_t shim_sstream_str(uint32_t *a)        { return ss_str(a[0], a[1]); }

/* ostream& operator<<(ostream&, const char*) / ostream::operator<<(int) */
uint32_t shim_ostream_cstr(uint32_t *a) {
   const char *p = (const char *)(uintptr_t)a[1];
   if (p) { ss_put(a[0], p, (uint32_t)strlen(p)); }
   return a[0];
}
uint32_t shim_ostream_int(uint32_t *a) {
   char t[16];
   int n = snprintf(t, sizeof t, "%d", (int32_t)a[1]);
   ss_put(a[0], t, (uint32_t)n);
   return a[0];
}
/* std::endl<char>(ostream&) — also what operator<<(manip) recognises. */
extern char ___ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_[];
uint32_t shim_ostream_endl(uint32_t *a) { ss_put(a[0], "\n", 1); return a[0]; }
uint32_t shim_ostream_manip(uint32_t *a) {   /* operator<<(ostream& (*)(ostream&)) */
   if (a[1] == (uint32_t)(uintptr_t)___ZSt4endlIcSt11char_traitsIcEERSt13basic_ostreamIT_T0_ES6_) {
      ss_put(a[0], "\n", 1);
   } else {
      static int once;
      if (!once++) { fprintf(stderr, "iostream_shim: unsupported manipulator %#x ignored\n", a[1]); }
   }
   return a[0];
}

/* std::locale: one _M_impl word. Every locale is the classic "C" one. */
static uint32_t g_locale_impl = 1;
uint32_t shim_locale_ctor(uint32_t *a) { *(uint32_t *)(uintptr_t)a[0] = (uint32_t)(uintptr_t)&g_locale_impl; return a[0]; }
uint32_t shim_locale_dtor(uint32_t *a) { (void)a; return 0; }
uint32_t shim_locale_assign(uint32_t *a) {
   *(uint32_t *)(uintptr_t)a[0] = *(uint32_t *)(uintptr_t)a[1];
   return a[0];
}

/* use_facet<numpunct<char>>: a static i386-layout facet {vptr, refcount,
 * _M_data} whose vtable slots are i386-callable trampolines:
 * D1, D0, do_decimal_point, do_thousands_sep, do_grouping, do_truename,
 * do_falsename (the last three return std::string by value). */
extern char __np_nop[], __np_decimal_point[], __np_thousands_sep[],
            __np_grouping[], __np_truename[], __np_falsename[];
uint32_t shim_np_nop(uint32_t *a)            { (void)a; return 0; }
uint32_t shim_np_decimal_point(uint32_t *a)  { (void)a; return '.'; }
uint32_t shim_np_thousands_sep(uint32_t *a)  { (void)a; return ','; }
uint32_t shim_np_grouping(uint32_t *a)  { cow_str_init(a[0], 0, 0); return a[0]; }
uint32_t shim_np_truename(uint32_t *a)  { static char t[] = "true";  cow_str_init(a[0], (uint32_t)(uintptr_t)t, 4); return a[0]; }
uint32_t shim_np_falsename(uint32_t *a) { static char f[] = "false"; cow_str_init(a[0], (uint32_t)(uintptr_t)f, 5); return a[0]; }

static uint32_t g_np_vtbl[2 + 7];        /* offset-to-top, typeinfo, 7 slots */
static uint32_t g_np_obj[3];
uint32_t shim_use_facet_numpunct(uint32_t *a) {
   (void)a;
   if (!g_np_obj[0]) {
      const char *slots[7] = { __np_nop, __np_nop, __np_decimal_point,
                               __np_thousands_sep, __np_grouping,
                               __np_truename, __np_falsename };
      for (int i = 0; i < 7; i++) { g_np_vtbl[2 + i] = (uint32_t)(uintptr_t)slots[i]; }
      g_np_obj[0] = (uint32_t)(uintptr_t)&g_np_vtbl[2];
      g_np_obj[1] = 1;                    /* refcount: never released */
   }
   return (uint32_t)(uintptr_t)g_np_obj;
}
