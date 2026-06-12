/*
 * 11_sscanf — exercises the scanf-family vararg ABI shim.
 *
 * i386 sscanf is variadic, so abigen skips it; the lazy bind would otherwise
 * fall through to the host x86_64 sscanf, which reads the i386 cdecl stack
 * args under the register ABI and mismarshals every pointer. The
 * `___sscanf` vararg shim (printf-conv.cc scanf_conversion_f) converts each
 * 4-byte i386 output pointer to an 8-byte x86_64 pointer before the call.
 *
 * Two calls cover the cases that matter for the iPhoto UpgradeChecker path:
 *   - dotted "%d.%d.%d" version parse (3 output pointers)
 *   - assignment suppression "%*d" (consumes input but NO argument)
 *
 * Validation via exit code (not printf) to dodge the 4-arg printf vararg
 * bug. exit(42) iff every field parsed correctly.
 */
extern void exit(int status);
extern int sscanf(const char *str, const char *format, ...);

int main(void) {
   int a = 0, b = 0, c = 0;
   int n1 = sscanf("10.6.99", "%d.%d.%d", &a, &b, &c);

   int x = 0, y = 0;
   int n2 = sscanf("99 10 6", "%*d %d %d", &x, &y);

   int ok = (n1 == 3 && a == 10 && b == 6 && c == 99 &&
             n2 == 2 && x == 10 && y == 6);
   exit(ok ? 42 : 1);
   return 0;
}
