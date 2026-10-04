#include <cassert>
#include <cstdint>
#include <cstdio>
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

/* ★A FILE* crossing from i386 is a HANDLE, not a pointer (Portal 2 2026-09-14).
 * typeconv.cc now classifies `struct __sFILE *` as a CF-style ref, so the normal
 * bridges wrap a >4GB FILE on the way out and unwrap it on the way in. The printf
 * family does NOT use those bridges -- it is variadic and hand-marshalled here --
 * so every entry point that takes a stream must resolve it itself, or it hands
 * native stdio an arena handle. x64_objc_unwrap returns an arena handle's real
 * 64-bit value and passes anything else through unchanged, so a low raw FILE*
 * (and NULL) still works. */
extern "C" uint64_t x64_objc_unwrap(uint32_t h);
extern "C" void x64_gap_hit(const char *kind, const char *sym, const char *who, uint32_t caller);
static inline FILE *stream_from_i386(uint32_t h) {
   return (FILE *)(uintptr_t)x64_objc_unwrap(h);
}

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

   /* Every args64/argtypes buffer holds at least this many slots
    * (vararg-conv-t.asm ARGS64_COUNT; the v* shims' VA_SLOTS_MAX is larger).
    * Past it, a slot is dropped loudly rather than written out of bounds. */
   constexpr unsigned VARARG_SLOTS = 64;
   bool slot_full(unsigned arg_count) {
      if (arg_count < VARARG_SLOTS) { return false; }
      static int hit;
      if (!__atomic_exchange_n(&hit, 1, __ATOMIC_RELAXED)) {
         x64_gap_hit("vararg-slots", "over-64-args", "printf-conv.cc", 0);
      }
      return true;
   }

   /* convert_arg<ptr32_t,ptr64_t> for a FILE* slot: resolve the handle instead
    * of widening it. Advances the cursors identically. */
   uint64_t convert_stream_arg(const void *& args32, void *& args64,
                               reg_width_t *& argtypes, unsigned& arg_count) {
      const uint64_t real = x64_objc_unwrap(*(const ptr32_t *)args32);
      if (slot_full(arg_count)) { args32 = (const char *) args32 + 4; return real; }
      *(ptr64_t *)args64 = real;
      args32 = (const char *) args32 + align_up<size_t>(sizeof(ptr32_t), 4);
      args64 = (char *) args64 + align_up<size_t>(sizeof(ptr64_t), 8);
      *argtypes++ = type_reg_width<ptr64_t>();
      ++arg_count;
      return real;
   }

   template <typename T32, typename T64>
   T64 convert_arg(const void *& args32, void *& args64, reg_width_t *& argtypes,
                          unsigned& arg_count) {
      if (slot_full(arg_count)) {
         const T64 v = * (T32 *) args32;
         args32 = (const char *) args32 + align_up<size_t>(sizeof(T32), 4);
         return v;
      }
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
          {"cC", printf_type::CHAR},          /* C = %lc (wint_t) */
          {"s", printf_type::STRING},
          {"pnS", printf_type::POINTER},      /* n = int *, S = %ls (wchar_t *) */
          {"%", printf_type::ESCAPE},
         };
      for (auto pair : conv) {
         if (pair.first.find(*format) != std::string::npos) {
            ++format;
            return pair.second;
         }
      }

      /* Unknown (e.g. %Lf: a 16-byte i386 long double is not converted).
       * Never throw: an exception out of this C-ABI bridge aborts the app
       * (Portal 2 single-player load). Speak once per character, consume no
       * argument. */
      static unsigned char seen[256];
      const unsigned char u = (unsigned char)*format;
      if (!__atomic_exchange_n(&seen[u], 1, __ATOMIC_RELAXED)) {
         const char sym[] = {'%', (char)(u ? u : '0'), '\0'};
         x64_gap_hit("printf-conversion", sym, "printf-conv.cc", 0);
      }
      if (u) { ++format; }
      return printf_type::ESCAPE;
   }

   enum class printf_modifier {H, HH, L, LL, J, T, Z};
   std::optional<printf_modifier> printf_parse_modifier(const char *& format) {
      const std::list<std::pair<const char *, printf_modifier>> conv =
         {{"hh", printf_modifier::HH},       /* longest match first: "l" used */
          {"h", printf_modifier::H},         /* to win over "ll", so %lld threw */
          {"ll", printf_modifier::LL},
          {"q", printf_modifier::LL},        /* BSD spelling of ll */
          {"l", printf_modifier::L},
          {"j", printf_modifier::J},
          {"t", printf_modifier::T},
          {"z", printf_modifier::Z},
         };
      /* Kill switch M64_NO_PRINTF_LONGEST_MOD: the old order ("h"/"l" first);
       * guard printf-length-mods. */
      static const bool old_order = getenv("M64_NO_PRINTF_LONGEST_MOD") != nullptr;
      if (old_order) {
         for (const char *m : {"h", "l"}) {
            if (*format == *m) { ++format; return *m == 'h' ? printf_modifier::H : printf_modifier::L; }
         }
      }
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
    * for `%%`. `star_args` counts the `*` field-width / `.*` precision specifiers
    * (0, 1, or 2): each `*` consumes an EXTRA leading int argument (the width or
    * precision value), which must be converted i386->x86_64 BEFORE the main
    * argument. `printf("%*.*f", w, p, x)` = 3 consumed args. */
   struct printf_directive {
      unsigned pos;
      printf_type type;
      std::optional<printf_modifier> mod;
      bool consumes;
      unsigned star_args;
   };

   /* Parse ONE directive (the char after '%'), advancing `format` past it.
    * Does NOT consume an argument — the driver decides ordering (sequential vs
    * positional). Handles the C / CFString positional prefix `%N$` (which MUST
    * precede flags/width; without this the leading digit was mis-read as a field
    * width and the trailing '$' threw "invalid conversion specifier", so a
    * `%1$.2g` double arg was never converted -> native read garbage, e.g. Civ
    * IV's "requires at least 1.2e-265 MB" disk-space alert). */
   printf_directive printf_parse_directive(const char *& format) {
      printf_directive d{0, printf_type::ESCAPE, std::nullopt, false, 0};

      /* positional argument prefix: <digits>'$' */
      if (isdigit((unsigned char)*format)) {
         const char *q = format;
         unsigned n = 0;
         while (isdigit((unsigned char)*q)) { n = n * 10 + (unsigned)(*q - '0'); ++q; }
         if (*q == '$') { d.pos = n; format = q + 1; }
      }

      /* parse flags (there may be several, e.g. "%-+08.3f") */
      for (;;) {
         switch (*format) {
         case '#': case '0': case '-': case ' ': case '+': case '\'':
            ++format;
            continue;
         }
         break;
      }

      /* parse minimum field width: either a literal digit string OR a '*' that
       * takes the width from an int argument (`printf("%*d", w, x)`). The '*'
       * consumes one EXTRA int arg the driver must convert before the main one.
       * A `%*N$d` positional-star form also exists (rare); consume its `N$`. */
      if (*format == '*') {
         ++format;
         if (isdigit((unsigned char)*format)) {
            const char *q = format;
            while (isdigit((unsigned char)*q)) { ++q; }
            if (*q == '$') { format = q + 1; }
         }
         ++d.star_args;
      } else {
         while (isdigit((unsigned char)*format)) { ++format; }
      }

      /* parse precision: '.' then either digits, a '*' (int arg), or empty (== .0) */
      if (*format == '.') {
         ++format;
         if (*format == '*') {
            ++format;
            if (isdigit((unsigned char)*format)) {
               const char *q = format;
               while (isdigit((unsigned char)*q)) { ++q; }
               if (*q == '$') { format = q + 1; }
            }
            ++d.star_args;
         } else {
            while (isdigit((unsigned char)*format)) { ++format; }
         }
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
                                 {printf_modifier::Z, convert_arg_s<i32_t, i64_t>}
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
          {printf_type::STRING, {{std::nullopt, convert_arg_s<uint32_t, uint64_t>},
                                 {printf_modifier::L, convert_arg_s<uint32_t, uint64_t>}}},
          {printf_type::FLOAT, {{std::nullopt, convert_arg_s<double, double>},
                                {printf_modifier::L, convert_arg_s<double, double>}
             }
          },
          {printf_type::CHAR, {{std::nullopt, convert_arg_s<int32_t, int32_t>},
                               {printf_modifier::L, convert_arg_s<int32_t, int32_t>}}},
          {printf_type::POINTER, {{std::nullopt, convert_arg_s<uint32_t, uint64_t>}}},
          {printf_type::ESCAPE, {{std::nullopt, nullptr}}}
         };

      /* A modifier this type has no entry for (%hs, %jc, ...): convert as the
       * bare type, loudly, rather than throw out of the bridge. */
      const auto& by_mod = converter.at(type);
      auto it = by_mod.find(modifier);
      if (it == by_mod.end()) {
         static int hit;
         if (!__atomic_exchange_n(&hit, 1, __ATOMIC_RELAXED)) {
            x64_gap_hit("printf-conversion", "modifier", "printf-conv.cc", 0);
         }
         it = by_mod.find(std::nullopt);
      }
      if (it->second) { it->second(args32, args64, argtypes, arg_count); }
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
         /* `*` width/precision each take an int arg that PRECEDES the main
          * argument in the i386 stream: `printf("%*.*f", w, p, x)` pushes
          * w, p, x. Convert those ints first (i386 4-byte -> x86_64), then
          * the main conversion. */
         for (unsigned s = 0; s < d.star_args; ++s) {
            printf_do_convert(printf_type::SIGNED, std::nullopt,
                              args32, args64, argtypes, arg_count);
         }
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
            /* A positional star form (`%1$*2$d`) references its width/precision
             * by their own positional slots, so the star ints are already
             * accounted for as separate slots — do NOT double-consume here.
             * (Mixed positional+star is vanishingly rare; the common star use
             * `%*.*f` is non-positional and handled above.) */
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
   convert_stream_arg(args32, args64, argtypes, arg_count);   /* FILE * */
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
   convert_stream_arg(args32, args64, argtypes, arg_count);   /* FILE * */
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
#include <wchar.h>

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
            /* `*` width/`.*` precision each take an int arg PRECEDING the main
             * one in the i386 stream (`"%*.*f", w, p, x`): convert those ints
             * first, then the main conversion. (Same as printf_conversion_f.) */
            for (unsigned s = 0; s < d.star_args; ++s) {
               printf_do_convert(printf_type::SIGNED, std::nullopt,
                                 a32, a64, at, arg_count);
            }
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
               /* positional-star (`%1$*2$d`) references star ints by their own
                * slots, already counted — do not double-consume (see the sibling
                * comment in printf_conversion_f). */
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

   /* Wide formats: every directive is ASCII and wchar_t is 4 bytes in both
    * ABIs, so parse an ASCII copy (non-ASCII -> 'x') and hand the native call
    * the ORIGINAL wide format. %s and %c in a wide format take char pointers and ints, exactly
    * the narrow conversions. Directives past the buffer are ignored (their
    * args are then not converted -- formats that long do not exist). */
   void narrow_fmt(const wchar_t *w, char *out, size_t n) {
      size_t i = 0;
      for (; w && w[i] && i + 1 < n; ++i) {
         out[i] = (w[i] > 0 && w[i] < 0x80) ? (char)w[i] : 'x';
      }
      out[i] = 0;
   }

   /* scanf family: every consumed argument is a pointer (scanf_parse_directive). */
   void build_native_scanf_va_list(const char *format, const uint32_t *ap,
                                   sysv_va_list_tag *va,
                                   uint64_t *args64, reg_width_t *argtypes) {
      unsigned arg_count = 0;
      const void *a32 = (const void *) ap;
      void *a64 = (void *) args64;
      reg_width_t *at = argtypes;
      scanf_convert_format(a32, a64, at, format, arg_count);
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
   FILE        *stream = stream_from_i386(a[0]);
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
   FILE *fp = stream_from_i386(a[0]);
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

/* Wide family (Portal 2 tier1 V_snwprintf -> vswprintf: a raw i386 va_list
 * reached native vswprintf -> SIGSEGV opening Options > Video). Guard
 * wide-printf. */
#define WVA_BUILD(wfmt_, ap_, builder_)                                     \
   const wchar_t *wfmt = (wfmt_);                                           \
   char nfmt[2048];                                                         \
   narrow_fmt(wfmt, nfmt, sizeof nfmt);                                     \
   alignas(16) uint64_t args64[VA_SLOTS_MAX];                               \
   reg_width_t argtypes[VA_SLOTS_MAX];                                      \
   va_list va;                                                              \
   builder_(nfmt, (ap_), (sysv_va_list_tag *)(void *)va, args64, argtypes)

/* int vswprintf(wchar_t *s, size_t n, const wchar_t *fmt, va_list ap) */
int vswprintf_vshim(const uint32_t *a) {
   WVA_BUILD((const wchar_t *)(uintptr_t)a[2], (const uint32_t *)(uintptr_t)a[3],
             build_native_va_list);
   return vswprintf((wchar_t *)(uintptr_t)a[0], (size_t)a[1], wfmt, va);
}
/* int swprintf(wchar_t *s, size_t n, const wchar_t *fmt, ...) */
int swprintf_vshim(const uint32_t *a) {
   WVA_BUILD((const wchar_t *)(uintptr_t)a[2], &a[3], build_native_va_list);
   return vswprintf((wchar_t *)(uintptr_t)a[0], (size_t)a[1], wfmt, va);
}
/* int vsscanf(const char *s, const char *fmt, va_list ap) — the i386 va_list
 * is a plain pointer to 4-byte pointer slots. */
int vsscanf_vshim(const uint32_t *a) {
   const char *fmt = (const char *)(uintptr_t)a[1];
   alignas(16) uint64_t args64[VA_SLOTS_MAX];
   reg_width_t argtypes[VA_SLOTS_MAX];
   va_list va;
   build_native_scanf_va_list(fmt, (const uint32_t *)(uintptr_t)a[2],
                              (sysv_va_list_tag *)(void *)va, args64, argtypes);
   return vsscanf((const char *)(uintptr_t)a[0], fmt, va);
}
/* int swscanf(const wchar_t *s, const wchar_t *fmt, ...) */
int swscanf_vshim(const uint32_t *a) {
   WVA_BUILD((const wchar_t *)(uintptr_t)a[1], &a[2], build_native_scanf_va_list);
   return vswscanf((const wchar_t *)(uintptr_t)a[0], wfmt, va);
}

} /* extern "C" */
