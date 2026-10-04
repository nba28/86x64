/* 99_wide_printf — the wide printf/scanf family across the ABI: vswprintf takes
 * an i386 va_list (a plain pointer to 4-byte slots), swprintf/swscanf are
 * variadic, vsscanf takes an i386 va_list of pointers. Portal 2 tier1 V_snwprintf -> vswprintf crashed opening
 * Options > Video (native vswprintf read the i386 va_list as an x86_64
 * __va_list_tag). Exit 42 = every result exact. No kill switch (the old path
 * is the raw abigen bridge); proven non-inert against the pre-fix library. */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int my_snwprintf(wchar_t *buf, int n, const wchar_t *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   int r = vswprintf(buf, n, fmt, ap);
   va_end(ap);
   return r;
}

static int my_sscanf(const char *s, const char *fmt, ...) {
   va_list ap;
   va_start(ap, fmt);
   int r = vsscanf(s, fmt, ap);
   va_end(ap);
   return r;
}

int main(void) {
   wchar_t b[128];
   int bad = 0;
   my_snwprintf(b, 128, L"%d x %d @ %ls %s %.1f", 1280, 720, L"wide", "narrow", 2.5);
   if (wcscmp(b, L"1280 x 720 @ wide narrow 2.5") != 0) bad |= 1;
   swprintf(b, 128, L"[%5d|%c|%lld]", 42, 'Q', 76561197960287930LL);
   if (wcscmp(b, L"[   42|Q|76561197960287930]") != 0) bad |= 2;
   int w = 0, h = 0; float f = 0;
   if (swscanf(L"1920 1080 0.75", L"%d %d %f", &w, &h, &f) != 3 || w != 1920 || h != 1080 || f != 0.75f) bad |= 4;
   int x = 0, y = 0; char word[16] = "";
   if (my_sscanf("7 8 hello", "%d %d %15s", &x, &y, word) != 3 || x != 7 || y != 8 || strcmp(word, "hello")) bad |= 8;
   exit(bad ? bad : 42);
}
