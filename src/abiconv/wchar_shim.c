// wchar_shim.c — the C <wchar.h> wide-string family as i386->x86_64 marshalling
// shims (wired through wchar_tramp.asm's WCHAR_MTSHIM* -> ___wcs* exports).
//
// WHY: these are PRESENT native functions (wcslen/wcscpy/... live in libSystem),
// NOT removed symbols. But static-interpose renames the translated app's
// `_wcs*` imports to `___wcs*` (the libabiconv-shim spelling), and abigen's
// modern pass did not emit shims for them, so the imports bound NULL ->
// jt-weak-import `ud2` the moment the app called one (Civ IV: `wcslen` during
// its Python/boost wide-string handling, Civilization IV.dylib jt_ptrs[319]).
// Provide the family here as a targeted stopgap; when the modern import-manifest
// covers them abigen would generate the identical shims (custom.syms lists the
// family so there is no duplicate export).
//
// wchar_t is `int` (4 bytes) on BOTH i386 and x86_64 macOS, so wchar_t VALUES
// need no conversion; only wchar_t POINTERS widen (i386 4-byte low-4GB -> native
// 8-byte, a zero-extend since the app's buffers live below 4GB). A `wchar_t**`
// out-param (endptr/saveptr) points at a 4-byte i386 slot, so it is BOUNCED
// through a native pointer temp and truncated back. Return-register conventions
// (eax / edx:eax / st0) are handled by wchar_tramp.asm.
//
// MTSHIM convention: `args` -> &i386 args[0] (4-byte cdecl slots).

#include <wchar.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define WP(n)  ((wchar_t *)(uintptr_t)args[(n)])
#define CWP(n) ((const wchar_t *)(uintptr_t)args[(n)])

/* --- pointer args in, 32-bit (int / size_t / wchar_t*) result in eax --- */
uint32_t shim_wcslen(uint32_t *args)  { return (uint32_t)wcslen(CWP(0)); }
uint32_t shim_wcscmp(uint32_t *args)  { return (uint32_t)wcscmp(CWP(0), CWP(1)); }
uint32_t shim_wcsncmp(uint32_t *args) { return (uint32_t)wcsncmp(CWP(0), CWP(1), (size_t)args[2]); }
uint32_t shim_wcscpy(uint32_t *args)  { return (uint32_t)(uintptr_t)wcscpy(WP(0), CWP(1)); }
uint32_t shim_wcsncpy(uint32_t *args) { return (uint32_t)(uintptr_t)wcsncpy(WP(0), CWP(1), (size_t)args[2]); }
uint32_t shim_wcscat(uint32_t *args)  { return (uint32_t)(uintptr_t)wcscat(WP(0), CWP(1)); }
uint32_t shim_wcschr(uint32_t *args)  { return (uint32_t)(uintptr_t)wcschr(CWP(0), (wchar_t)args[1]); }
uint32_t shim_wcsrchr(uint32_t *args) { return (uint32_t)(uintptr_t)wcsrchr(CWP(0), (wchar_t)args[1]); }
uint32_t shim_wcsstr(uint32_t *args)  { return (uint32_t)(uintptr_t)wcsstr(CWP(0), CWP(1)); }

/* wcstok: saveptr (wchar_t**) points at a 4-byte i386 slot holding a wchar_t*.
 * Read+widen it into a native temp, call, truncate the updated temp back. */
uint32_t shim_wcstok(uint32_t *args) {
   wchar_t *str = WP(0);
   const wchar_t *delim = CWP(1);
   uint32_t sp = args[2];
   wchar_t *save = sp ? (wchar_t *)(uintptr_t)(*(uint32_t *)(uintptr_t)sp) : NULL;
   wchar_t *r = wcstok(str, delim, sp ? &save : NULL);
   if (sp) { *(uint32_t *)(uintptr_t)sp = (uint32_t)(uintptr_t)save; }
   return (uint32_t)(uintptr_t)r;
}

/* wcsto{l,ll,d,f}: endptr (wchar_t**) -> 4-byte i386 slot; bounce + truncate. */
uint32_t shim_wcstol(uint32_t *args) {
   uint32_t ep = args[1];
   wchar_t *end = NULL;
   long r = wcstol(CWP(0), ep ? &end : NULL, (int)args[2]);
   if (ep) { *(uint32_t *)(uintptr_t)ep = (uint32_t)(uintptr_t)end; }
   return (uint32_t)r;
}
uint64_t shim_wcstoll(uint32_t *args) {   /* edx:eax via WCHAR_MTSHIM64 */
   uint32_t ep = args[1];
   wchar_t *end = NULL;
   long long r = wcstoll(CWP(0), ep ? &end : NULL, (int)args[2]);
   if (ep) { *(uint32_t *)(uintptr_t)ep = (uint32_t)(uintptr_t)end; }
   return (uint64_t)r;
}
double shim_wcstod(uint32_t *args) {      /* st0 via WCHAR_MTSHIM_FP */
   uint32_t ep = args[1];
   wchar_t *end = NULL;
   double r = wcstod(CWP(0), ep ? &end : NULL);
   if (ep) { *(uint32_t *)(uintptr_t)ep = (uint32_t)(uintptr_t)end; }
   return r;
}
double shim_wcstof(uint32_t *args) {      /* return double; trampoline fld's st0 */
   uint32_t ep = args[1];
   wchar_t *end = NULL;
   float r = wcstof(CWP(0), ep ? &end : NULL);
   if (ep) { *(uint32_t *)(uintptr_t)ep = (uint32_t)(uintptr_t)end; }
   return (double)r;
}

/* wcsftime: the `struct tm*` arg has a DIFFERENT i386 layout (tm_gmtoff is a
 * 4-byte long, tm_zone a 4-byte pointer), so marshal it field-by-field. */
struct i386_tm {
   int32_t  tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst;
   int32_t  tm_gmtoff;   /* i386 long */
   uint32_t tm_zone;     /* i386 char* */
};
uint32_t shim_wcsftime(uint32_t *args) {
   const struct i386_tm *it = (const struct i386_tm *)(uintptr_t)args[3];
   struct tm nt;
   memset(&nt, 0, sizeof nt);
   if (it) {
      nt.tm_sec = it->tm_sec; nt.tm_min = it->tm_min; nt.tm_hour = it->tm_hour;
      nt.tm_mday = it->tm_mday; nt.tm_mon = it->tm_mon; nt.tm_year = it->tm_year;
      nt.tm_wday = it->tm_wday; nt.tm_yday = it->tm_yday; nt.tm_isdst = it->tm_isdst;
      nt.tm_gmtoff = it->tm_gmtoff;
      nt.tm_zone = (char *)(uintptr_t)it->tm_zone;
   }
   return (uint32_t)wcsftime(WP(0), (size_t)args[1], CWP(2), it ? &nt : NULL);
}
