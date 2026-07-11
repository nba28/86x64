#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <list>
#include <string>
#include <optional>
#include <vector>
#include <map>
#include <unordered_map>

typedef uint32_t ptr32_t;
typedef uint64_t ptr64_t;

typedef uint32_t size32_t;
typedef uint64_t size64_t;

typedef int32_t i32_t;
typedef int64_t i64_t;

/* f32_t/f64_t are unused at present: every FLOAT specifier (e/E/f/F/g/G/a/A)
 * routes through convert_arg_s<double, double> explicitly. Kept for
 * symmetry with the other size pairs in case a future ABI variant
 * (e.g. `%f` with explicit `l` on a long-double target) needs them. */
typedef float f32_t;
typedef float f64_t;

typedef int32_t l32_t;
typedef int64_t l64_t;

typedef int64_t ll32_t;
typedef int64_t ll64_t;

typedef int16_t s32_t;
typedef int16_t s64_t;

typedef signed char sc32_t;
typedef signed char sc64_t;

typedef int64_t j32_t;
typedef int64_t j64_t;

typedef int32_t t32_t;
typedef int64_t t64_t;

enum class reg_width_t {B, W, D, Q};

namespace {

   template <typename T>
   constexpr T div_up(T a, T b) { return (a + b - 1) / b; }

   template <typename T>
   constexpr T align_up(T a, T b) { return div_up(a, b) * b; }

   template <typename T>
   constexpr reg_width_t type_reg_width() {
      static_assert((sizeof(T) & (sizeof(T) - 1)) == 0);
      int i = 0;
      size_t size = sizeof(T);
      while (size > 1) {
         size >>= 1;
         ++i;
      }
      return (reg_width_t) i;
   }

   template <typename T32, typename T64>
   T64 convert_arg(const void *& args32, void *& args64, reg_width_t *& argtypes,
                          unsigned& arg_count) {
      const T64 result = * (T64 *) args64 = * (T32 *) args32;
      args32 = (const char *) args32 + align_up<size_t>(sizeof(T32), 4);
      args64 = (char *) args64 + align_up<size_t>(sizeof(T64), 8);
      *argtypes++ = type_reg_width<T64>();
      ++arg_count;
      return result;
   }

   /* convert arg, silently */
   template <typename T32, typename T64>
   void convert_arg_s(const void *& args32, void *& args64, reg_width_t *& argtypes,
                      unsigned& arg_count) {
      convert_arg<T32, T64>(args32, args64, argtypes, arg_count);
   }

   static bool printf_is_length_modifier(char c) {
      switch (c) {
      case 'h':
      case 'l':
      case 'j':
      case 't':
      case 'z':
         return true;
      default:
         return false;
      }
   }

   enum class printf_type {SIGNED, UNSIGNED, STRING, FLOAT, CHAR, POINTER, ESCAPE};
   printf_type printf_parse_type(const char *& format) {
      const std::list<std::pair<std::string, printf_type>> conv =
         {{"di", printf_type::SIGNED},
          {"ouxX", printf_type::UNSIGNED},
          {"eEfFgGaA", printf_type::FLOAT},
          {"c", printf_type::CHAR},
          {"s", printf_type::STRING},
          {"p", printf_type::POINTER},
          {"%", printf_type::ESCAPE},
         };
      for (auto pair : conv) {
         if (pair.first.find(*format) != std::string::npos) {
            ++format;
            return pair.second;
         }
      }

      throw std::invalid_argument("invalid conversion specifier");
   }

   enum class printf_modifier {H, HH, L, LL, J, T, Z};
   std::optional<printf_modifier> printf_parse_modifier(const char *& format) {
      const std::list<std::pair<const char *, printf_modifier>> conv =
         {{"h", printf_modifier::H},
          {"hh", printf_modifier::HH},
          {"l", printf_modifier::L},
          {"ll", printf_modifier::LL},
          {"j", printf_modifier::J},
          {"t", printf_modifier::T},
          {"z", printf_modifier::Z},
         };
      for (auto pair : conv) {
         auto len = strlen(pair.first);
         if (strncmp(pair.first, format, len) == 0) {
            format += len;
            return pair.second;
         }
      }
      return std::nullopt;
   }

   /* One parsed conversion directive. `pos` is the 1-based positional argument
    * index from a `%N$...` specifier (0 = non-positional). `consumes` is false
    * for `%%`. */
   struct printf_directive {
      unsigned pos;
      printf_type type;
      std::optional<printf_modifier> mod;
      bool consumes;
   };

   /* Parse ONE directive (the char after '%'), advancing `format` past it.
    * Does NOT consume an argument — the driver decides ordering (sequential vs
    * positional). Handles the C / CFString positional prefix `%N$` (which MUST
    * precede flags/width; without this the leading digit was mis-read as a field
    * width and the trailing '$' threw "invalid conversion specifier", so a
    * `%1$.2g` double arg was never converted -> native read garbage, e.g. Civ
    * IV's "requires at least 1.2e-265 MB" disk-space alert). */
   printf_directive printf_parse_directive(const char *& format) {
      printf_directive d{0, printf_type::ESCAPE, std::nullopt, false};

      /* positional argument prefix: <digits>'$' */
      if (isdigit((unsigned char)*format)) {
         const char *q = format;
         unsigned n = 0;
         while (isdigit((unsigned char)*q)) { n = n * 10 + (unsigned)(*q - '0'); ++q; }
         if (*q == '$') { d.pos = n; format = q + 1; }
      }

      /* parse flags */
      switch (*format) {
      case '#':
      case '0':
      case '-':
      case ' ':
      case '+':
      case '\'':
         ++format;
         break;
      }

      /* parse minimum field width */
      while (isdigit((unsigned char)*format)) {
         ++format;
      }

      /* parse precision */
      if (*format == '.') {
         while (isdigit(*++format)) {}
      }

      /* parse length modifier */
      d.mod = printf_parse_modifier(format);

      /* parse format specifier */
      d.type = printf_parse_type(format);
      d.consumes = (d.type != printf_type::ESCAPE);
      return d;
   }

   /* Convert one i386 argument of `type`/`modifier` into the x86_64 arg stream
    * (advances args32/args64). */
   void printf_do_convert(printf_type type, std::optional<printf_modifier> modifier,
                          const void *& args32, void *& args64,
                          reg_width_t *& argtypes, unsigned& arg_count) {
      const std::unordered_map<printf_type,
                               std::unordered_map<std::optional<printf_modifier>,
                                                  void (*)(const void*&, void*&, reg_width_t*&,
                                                           unsigned&)
                                                  >
                               > converter
         {{printf_type::SIGNED, {{std::nullopt, convert_arg_s<i32_t, i64_t>},
                                 {printf_modifier::HH, convert_arg_s<sc32_t, sc64_t>},
                                 {printf_modifier::H, convert_arg_s<s32_t, s64_t>},
                                 {printf_modifier::L, convert_arg_s<l32_t, l64_t>},
                                 {printf_modifier::LL, convert_arg_s<ll32_t, ll64_t>},
                                 {printf_modifier::J, convert_arg_s<j32_t, j64_t>},
                                 {printf_modifier::T, convert_arg_s<t32_t, t64_t>},
                                 {printf_modifier::Z, convert_arg_s<int8_t, int8_t>}
               }
           },
          {printf_type::UNSIGNED, {{std::nullopt, convert_arg_s<i32_t, i64_t>},
                                   {printf_modifier::HH, convert_arg_s<uint8_t, uint8_t>},
                                   {printf_modifier::H, convert_arg_s<uint16_t, uint16_t>},
                                   {printf_modifier::L, convert_arg_s<uint32_t, uint64_t>},
                                   {printf_modifier::LL, convert_arg_s<uint64_t, uint64_t>},
                                   {printf_modifier::J, convert_arg_s<uint64_t, uint64_t>},
                                   {printf_modifier::T, convert_arg_s<uint32_t, uint64_t>},
                                   {printf_modifier::Z, convert_arg_s<uint32_t, uint64_t>}
             }
          },
          {printf_type::STRING, {{std::nullopt, convert_arg_s<uint32_t, uint64_t>}}},
          {printf_type::FLOAT, {{std::nullopt, convert_arg_s<double, double>},
                                {printf_modifier::L, convert_arg_s<double, double>}
             }
          },
          {printf_type::CHAR, {{std::nullopt, convert_arg_s<int32_t, int32_t>}}},
          {printf_type::POINTER, {{std::nullopt, convert_arg_s<uint32_t, uint64_t>}}},
          {printf_type::ESCAPE, {{std::nullopt, nullptr}}}
         };

      auto fn = converter.at(type).at(modifier);
      if (fn) { fn(args32, args64, argtypes, arg_count); }
   }

   /* scanf-family directive. Unlike printf, EVERY consumed scanf argument is a
    * pointer (int*, char*, float*, ...), so the i386→x86_64 conversion is
    * uniform: zero-extend one 4-byte pointer to 8 bytes. We only parse the
    * directive far enough to decide whether it consumes an argument:
    *   - '%%'      consumes nothing
    *   - '*' flag  (assignment suppression) consumes nothing
    *   - otherwise consumes exactly one pointer
    */
   void scanf_parse_directive(const void *& args32, void *& args64, reg_width_t *& argtypes,
                              const char *& format, unsigned& arg_count) {
      /* assignment-suppression flag */
      bool suppress = false;
      if (*format == '*') {
         suppress = true;
         ++format;
      }

      /* maximum field width */
      while (isdigit(*format)) {
         ++format;
      }

      /* length modifiers (h, hh, l, ll, L, j, t, z, q) — irrelevant to us
       * since the argument is a pointer regardless. */
      while (printf_is_length_modifier(*format) || *format == 'L' || *format == 'q') {
         ++format;
      }

      /* conversion specifier */
      const char conv = *format++;

      if (conv == '%') {
         /* literal '%' — consumes no argument */
         return;
      }

      if (conv == '[') {
         /* scanset: skip to the closing ']'. A ']' immediately after '[' or
          * '[^' is a literal member, not the terminator. */
         if (*format == '^') { ++format; }
         if (*format == ']') { ++format; }
         while (*format != '\0' && *format != ']') { ++format; }
         if (*format == ']') { ++format; }
      }

      if (suppress) {
         return;
      }

      convert_arg_s<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   }

   void scanf_convert_format(const void *& args32, void *& args64, reg_width_t *& argtypes,
                             const char *format, unsigned& arg_count) {
      char c;
      while ((c = *format++)) {
         if (c == '%') {
            scanf_parse_directive(args32, args64, argtypes, format, arg_count);
         }
      }
   }

}

extern "C" unsigned printf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned idx_64 = 0;
   unsigned idx_32 = 0;
   unsigned arg_count = 0;

   const char *format = (const char *) convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes,
                                                                     arg_count);
   /* Scan every directive first (arguments are NOT consumed during the scan) so
    * we can honor POSITIONAL args: a `%N$` format references arguments by index,
    * and (per C/CFString) if ANY specifier is positional they all are. The i386
    * argument stream is laid out in slot order (arg1, arg2, ...) regardless of
    * the order the format mentions them, so we must convert slots 1..max in
    * INDEX order with each slot's declared type. Non-positional formats keep the
    * original scan-order (== argument-order) sequential conversion. */
   std::vector<printf_directive> specs;
   bool positional = false;
   {
      const char *scan = format;
      char c;
      while ((c = *scan++)) {
         if (c != '%') { continue; }
         printf_directive d = printf_parse_directive(scan);
         if (d.consumes) {
            if (d.pos) { positional = true; }
            specs.push_back(d);
         }
      }
   }

   if (!positional) {
      for (const printf_directive &d : specs) {
         printf_do_convert(d.type, d.mod, args32, args64, argtypes, arg_count);
      }
   } else {
      std::map<unsigned, printf_directive> slot;   /* 1-based index -> spec */
      unsigned maxp = 0;
      for (const printf_directive &d : specs) {
         if (d.pos) { slot[d.pos] = d; if (d.pos > maxp) { maxp = d.pos; } }
      }
      for (unsigned i = 1; i <= maxp; ++i) {
         auto it = slot.find(i);
         if (it != slot.end()) {
            printf_do_convert(it->second.type, it->second.mod,
                              args32, args64, argtypes, arg_count);
         } else {
            /* gap (undefined per C): consume a pointer-width slot to keep the
             * remaining i386 stream aligned rather than desync. */
            convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
         }
      }
   }

   (void)idx_64; (void)idx_32;
   return arg_count;
}

extern "C" unsigned sprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned fprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned snprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   convert_arg<size32_t, size64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned asprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   return sprintf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned dprintf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<i32_t, i64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned __sprintf_chk_conversion_f(const void *args32, void *args64,
                                               reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count);
   convert_arg<i32_t, i64_t>(args32, args64, argtypes, arg_count);
   convert_arg<size32_t, size64_t>(args32, args64, argtypes, arg_count);
   return printf_conversion_f(args32, args64, argtypes) + 3;
}


/* scanf family. The variadic arguments are all output pointers; the format
 * string tells us how many. The leading fixed argument differs per function:
 *   sscanf(str, fmt, ...)  — str then fmt
 *   fscanf(FILE*, fmt, ...) — stream then fmt
 *   scanf(fmt, ...)        — fmt only
 */
extern "C" unsigned scanf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   const char *format = (const char *) convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes,
                                                                     arg_count);
   scanf_convert_format(args32, args64, argtypes, format, arg_count);
   return arg_count;
}

extern "C" unsigned sscanf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count); /* input string */
   return scanf_conversion_f(args32, args64, argtypes) + 1;
}

extern "C" unsigned fscanf_conversion_f(const void *args32, void *args64, reg_width_t *argtypes) {
   unsigned arg_count = 0;
   convert_arg<ptr32_t, ptr64_t>(args32, args64, argtypes, arg_count); /* FILE * */
   return scanf_conversion_f(args32, args64, argtypes) + 1;
}

/* ===================================================================
 * v-printf family (vsnprintf, vsprintf, vfprintf, vprintf, vasprintf,
 * __vsnprintf_chk) — the va_list variants.
 *
 * THE BUG these replace: an i386 `va_list` is a plain `char *` pointing
 * directly at the first variadic argument on the i386 stack (4-byte slots).
 * An x86_64 `va_list` is a 4-field `__va_list_tag` struct
 * {gp_offset, fp_offset, overflow_arg_area, reg_save_area}. The two are NOT
 * interchangeable, yet abigen's auto-generated v-shim copied the i386 va_list
 * bytes into an x86_64 tag as though they had the same shape — so native
 * vfprintf read *ap (the first vararg value) as gp_offset and dereferenced a
 * fused-garbage `overflow_arg_area` -> SIGSEGV (Portal 2 boot).
 *
 * THE FIX: drive the existing printf conversion machinery from the i386
 * va_list to build a flat x86_64 overflow buffer (one 8-byte slot per
 * conversion, exactly what `convert_arg` already produces), then synthesize a
 * real x86_64 va_list pointing at it with the register areas marked exhausted
 * (gp_offset/fp_offset past their reg-save windows) so native va_arg pulls
 * every argument from our buffer. The leading FIXED args (str/size/stream/fmt)
 * are passed directly. Universal: fixes any i386 binary calling a v* printf.
 * =================================================================== */

#include <cstdio>
#include <cstdarg>

/* Not always exposed by <cstdio> depending on feature macros. */
extern "C" int vasprintf(char **, const char *, va_list);
extern "C" int __vsnprintf_chk(char *, size_t, int, size_t, const char *, va_list);
extern "C" int __vsprintf_chk(char *, int, size_t, const char *, va_list);

namespace {
   /* x86_64 System V va_list element — layout-compatible with __va_list_tag. */
   struct sysv_va_list_tag {
      unsigned int gp_offset;
      unsigned int fp_offset;
      void        *overflow_arg_area;
      void        *reg_save_area;
   };

   /* Plenty for any sane format string; one 8-byte slot per conversion. */
   constexpr unsigned VA_SLOTS_MAX = 128;

   /* Walk `format`, converting the i386 varargs at `ap` (a flat array of
    * 4-byte i386 stack slots — the i386 va_list value) into the caller's
    * 8-byte-slot overflow buffer, and populate `va` to read from it. The
    * buffers must outlive the native call. */
   void build_native_va_list(const char *format, const uint32_t *ap,
                             sysv_va_list_tag *va,
                             uint64_t *args64, reg_width_t *argtypes) {
      unsigned arg_count = 0;
      const void *a32 = (const void *) ap;
      void *a64 = (void *) args64;
      reg_width_t *at = argtypes;
      /* Same positional-aware conversion as printf_conversion_f: scan first, then
       * convert either in argument order (non-positional) or slot-index order
       * (%N$ positional). THIS is the CFStringCreateWithFormat path — Civ IV's
       * disk-space alert uses `%1$.2g`, whose double was previously misparsed. */
      std::vector<printf_directive> specs;
      bool positional = false;
      {
         const char *scan = format;
         char c;
         while ((c = *scan++)) {
            if (c != '%') { continue; }
            printf_directive d = printf_parse_directive(scan);
            if (d.consumes) {
               if (d.pos) { positional = true; }
               specs.push_back(d);
            }
         }
      }
      if (!positional) {
         for (const printf_directive &d : specs) {
            printf_do_convert(d.type, d.mod, a32, a64, at, arg_count);
         }
      } else {
         std::map<unsigned, printf_directive> slot;
         unsigned maxp = 0;
         for (const printf_directive &d : specs) {
            if (d.pos) { slot[d.pos] = d; if (d.pos > maxp) { maxp = d.pos; } }
         }
         for (unsigned i = 1; i <= maxp; ++i) {
            auto it = slot.find(i);
            if (it != slot.end()) {
               printf_do_convert(it->second.type, it->second.mod, a32, a64, at, arg_count);
            } else {
               convert_arg<ptr32_t, ptr64_t>(a32, a64, at, arg_count);
            }
         }
      }
      /* GP regs (6*8=48 bytes) and XMM regs (8*16, fp window ends at 176) are
       * both marked exhausted, so every va_arg falls through to the overflow
       * area we built. reg_save_area is never consulted but must be non-null. */
      va->gp_offset       = 48;
      va->fp_offset       = 176;
      va->overflow_arg_area = args64;
      va->reg_save_area     = args64;
   }
}

extern "C" {

int vsnprintf_vshim(const uint32_t *a) {
   char        *str  = (char *)(uintptr_t)a[0];
   size_t       size = (size_t)a[1];
   const char  *fmt  = (const char *)(uintptr_t)a[2];
   const uint32_t *ap = (const uint32_t *)(uintptr_t)a[3];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return vsnprintf(str, size, fmt, va);
}

int vsprintf_vshim(const uint32_t *a) {
   char        *str = (char *)(uintptr_t)a[0];
   const char  *fmt = (const char *)(uintptr_t)a[1];
   const uint32_t *ap = (const uint32_t *)(uintptr_t)a[2];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return vsprintf(str, fmt, va);
}

int vfprintf_vshim(const uint32_t *a) {
   FILE        *stream = (FILE *)(uintptr_t)a[0];
   const char  *fmt    = (const char *)(uintptr_t)a[1];
   const uint32_t *ap   = (const uint32_t *)(uintptr_t)a[2];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return vfprintf(stream, fmt, va);
}

int vprintf_vshim(const uint32_t *a) {
   const char  *fmt = (const char *)(uintptr_t)a[0];
   const uint32_t *ap = (const uint32_t *)(uintptr_t)a[1];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return vprintf(fmt, va);
}

int vasprintf_vshim(const uint32_t *a) {
   char       **strp = (char **)(uintptr_t)a[0];
   const char  *fmt  = (const char *)(uintptr_t)a[1];
   const uint32_t *ap = (const uint32_t *)(uintptr_t)a[2];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return vasprintf(strp, fmt, va);
}

/* __vsprintf_chk(str, flag, slen, fmt, ap) — the fortified vsprintf. Civ IV
 * (Steam) binds it; unshimmed it bound NATIVE libc and the 64-bit ret
 * over-popped the i386 4-byte return slot (fused-PC class). The i386
 * "unknown size" sentinel 0xffffffff maps to (size_t)-1 so the native check
 * is skipped rather than bounded at a bogus 4GB. */
int __vsprintf_chk_vshim(const uint32_t *a) {
   char        *str  = (char *)(uintptr_t)a[0];
   int          flag = (int)a[1];
   size_t       slen = a[2] == 0xffffffffU ? (size_t)-1 : (size_t)a[2];
   const char  *fmt  = (const char *)(uintptr_t)a[3];
   const uint32_t *ap = (const uint32_t *)(uintptr_t)a[4];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return __vsprintf_chk(str, flag, slen, fmt, va);
}

/* __vsnprintf_chk(str, size, flag, slen, fmt, ap) */
int __vsnprintf_chk_vshim(const uint32_t *a) {
   char        *str  = (char *)(uintptr_t)a[0];
   size_t       size = (size_t)a[1];
   int          flag = (int)a[2];
   size_t       slen = (size_t)a[3];
   const char  *fmt  = (const char *)(uintptr_t)a[4];
   const uint32_t *ap = (const uint32_t *)(uintptr_t)a[5];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return __vsnprintf_chk(str, size, flag, slen, fmt, va);
}

/* __snprintf_chk(str, maxlen, flag, os, fmt, ...) — the fortified snprintf clang
 * emits for a known-size buffer. Route it through the SAME va_list path as
 * __vsnprintf_chk so FLOATING-POINT varargs work: the inline-varargs register
 * trampoline (vararg-conv-t.asm) only distributes GP args into rdi..r9 with al=0
 * and never places a double into an xmm register, so `%g`/`%f` read garbage; and
 * without ANY shim __snprintf_chk bound native -> i386 cdecl over-pop. Here the
 * varargs are INLINE (not a passed va_list), so `ap` is the ADDRESS of the first
 * vararg slot (&a[5]); build_native_va_list lays them into the overflow area a
 * native va_arg(double) reads correctly. (tests-i386/78) */
int __snprintf_chk_vshim(const uint32_t *a) {
   char        *str  = (char *)(uintptr_t)a[0];
   size_t       maxlen = (size_t)a[1];
   int          flag = (int)a[2];
   size_t       os   = (size_t)a[3];
   const char  *fmt  = (const char *)(uintptr_t)a[4];
   const uint32_t *ap = &a[5];        /* inline i386 varargs */
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return __vsnprintf_chk(str, maxlen, flag, os, fmt, va);
}

/* The INLINE-varargs printf family, routed through the va_list path so FLOATING-
 * POINT arguments work. The register trampoline (vararg-conv-t.asm) only ever
 * distributes the converted args into the GP registers rdi..r9 with al=0 — it
 * never places a double into an xmm register — so `%g`/`%f` read garbage (a
 * translated `printf("%.2g", x)` printed 0 / a denormal). build_native_va_list
 * instead lays every arg into the overflow area (gp_offset/fp_offset exhausted),
 * which native va_arg(double) reads correctly. `ap` = &a[N] is the address of
 * the first INLINE vararg slot (after the fixed args), distinct from the v*
 * variants where a[N] is a passed va_list pointer. (tests-i386/78) */
extern int vprintf(const char *, va_list);
extern int vfprintf(FILE *, const char *, va_list);
extern int vsprintf(char *, const char *, va_list);
extern int vsnprintf(char *, size_t, const char *, va_list);
extern int vasprintf(char **, const char *, va_list);
extern int vdprintf(int, const char *, va_list);

#define VA_BUILD(fmt_expr, ap_expr)                                          \
   const char *fmt = (fmt_expr);                                            \
   const uint32_t *ap = (ap_expr);                                         \
   alignas(16) uint64_t args64[VA_SLOTS_MAX];                              \
   reg_width_t argtypes[VA_SLOTS_MAX];                                     \
   va_list va;                                                            \
   build_native_va_list(fmt, ap, (sysv_va_list_tag *)(void *)va, args64, argtypes)

int printf_vshim(const uint32_t *a) {
   VA_BUILD((const char *)(uintptr_t)a[0], &a[1]);
   return vprintf(fmt, va);
}
int fprintf_vshim(const uint32_t *a) {
   FILE *fp = (FILE *)(uintptr_t)a[0];
   VA_BUILD((const char *)(uintptr_t)a[1], &a[2]);
   return vfprintf(fp, fmt, va);
}
int sprintf_vshim(const uint32_t *a) {
   char *str = (char *)(uintptr_t)a[0];
   VA_BUILD((const char *)(uintptr_t)a[1], &a[2]);
   return vsprintf(str, fmt, va);
}
int snprintf_vshim(const uint32_t *a) {
   char *str = (char *)(uintptr_t)a[0];
   size_t size = (size_t)a[1];
   VA_BUILD((const char *)(uintptr_t)a[2], &a[3]);
   return vsnprintf(str, size, fmt, va);
}
int asprintf_vshim(const uint32_t *a) {
   char **strp = (char **)(uintptr_t)a[0];
   VA_BUILD((const char *)(uintptr_t)a[1], &a[2]);
   return vasprintf(strp, fmt, va);
}
int dprintf_vshim(const uint32_t *a) {
   int fd = (int)a[0];
   VA_BUILD((const char *)(uintptr_t)a[1], &a[2]);
   return vdprintf(fd, fmt, va);
}

} /* extern "C" */
