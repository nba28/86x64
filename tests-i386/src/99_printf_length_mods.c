/* 99_printf_length_mods — the i386 printf bridge (printf-conv.cc) must parse
 * every length modifier/conversion it forwards. The modifier list matched "h"
 * before "hh" and "l" before "ll", so %lld/%llu left a stray 'l' as the
 * conversion and THREW std::invalid_argument out of the bridge: Portal 2
 * single-player load aborted ("uncaught exception ... invalid conversion
 * specifier"). Also: %zd converted as int8, %ls/%lc/%S/%n were unknown.
 * Exit 42 = every line formats exactly. Kill switch M64_NO_PRINTF_LONGEST_MOD=1
 * (run time) restores the old order. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int bad;
static void check(const char *got, const char *want) {
   if (strcmp(got, want) != 0) { printf("BAD got [%s] want [%s]\n", got, want); bad = 1; }
}

int main(void) {
   char b[256];
   int n = 0;
   snprintf(b, sizeof b, "%lld|%llu|%d", -5000000000LL, 18446744073709551615ULL, 7);
   check(b, "-5000000000|18446744073709551615|7");
   snprintf(b, sizeof b, "%hhx|%hd|%qd|%d", 0x1ff, (short)-3, 76561197960287930LL, 9);
   check(b, "ff|-3|76561197960287930|9");
   snprintf(b, sizeof b, "%zd|%zu|%d", (long)-2, (unsigned long)3, 4);
   check(b, "-2|3|4");
   snprintf(b, sizeof b, "%ls|%lc|%d%n|%d", L"wide", (wint_t)L'w', 5, &n, 6);
   check(b, "wide|w|5|6");
   if (n != 8)  { printf("BAD %%n=%d\n", n); bad = 1; }
   puts(bad ? "printf_length_mods: FAIL" : "printf_length_mods: PASS");
   exit(bad ? 1 : 42);   /* -e _main: no crt to return to */
}
