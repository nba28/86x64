/* 80_wchar_family.c — regression for the <wchar.h> wide-string family shims
 * (libabiconv wchar_shim.c / wchar_tramp.asm; Civ IV `___wcslen` ud2).
 *
 * static-interpose renames the translated i386 `_wcs*` imports to the
 * libabiconv-shim spelling `___wcs*`; before the fix libabiconv exported none
 * of them (except wcstombs) so the imports bound NULL -> jt-weak-import `ud2`
 * the moment the app called one. This exercises the family — including the
 * tricky endptr bounce (wcstol), the 64-bit edx:eax return (wcstoll), the st0
 * FP returns (wcstod/wcstof) and the wchar_t** saveptr (wcstok) — and asserts
 * correct results, so a broken marshalling shim (wrong pointer widening / wrong
 * return register / bad endptr write-back) fails the test. exit 42 == all pass.
 */

#include <wchar.h>
extern void exit(int);

int main(void) {
   int ok = 1;
   wchar_t buf[32];
   wchar_t *end;

   ok &= (wcslen(L"hello") == 5);
   ok &= (wcscmp(L"abc", L"abc") == 0);
   ok &= (wcscmp(L"abc", L"abd") < 0);
   ok &= (wcsncmp(L"abcd", L"abce", 3) == 0);

   const wchar_t *s = L"a.b.c";
   ok &= (wcschr(s, L'.') == s + 1);
   ok &= (wcsrchr(s, L'.') == s + 3);
   ok &= (wcsstr(L"foobar", L"bar") != 0);
   ok &= (wcsstr(L"foobar", L"xyz") == 0);

   wcscpy(buf, L"foo");
   wcscat(buf, L"bar");
   ok &= (wcscmp(buf, L"foobar") == 0);

   wchar_t b2[8];
   wcsncpy(b2, L"hello", 3); b2[3] = 0;
   ok &= (wcscmp(b2, L"hel") == 0);

   /* endptr bounce (wchar_t** -> i386 4-byte slot) */
   long v = wcstol(L"123abc", &end, 10);
   ok &= (v == 123 && *end == L'a');

   /* 64-bit edx:eax return */
   long long vll = wcstoll(L"9999999999", 0, 10);
   ok &= (vll == 9999999999LL);

   /* st0 FP returns */
   ok &= (wcstod(L"3.5", 0) == 3.5);
   ok &= (wcstof(L"2.5", 0) == 2.5f);

   /* saveptr (wchar_t**) round-trip */
   wchar_t tks[] = L"a,b,c";
   wchar_t *save;
   wchar_t *t1 = wcstok(tks, L",", &save);
   wchar_t *t2 = wcstok(0, L",", &save);
   ok &= (t1 && wcscmp(t1, L"a") == 0 && t2 && wcscmp(t2, L"b") == 0);

   exit(ok ? 42 : 1);
   return 0;
}
