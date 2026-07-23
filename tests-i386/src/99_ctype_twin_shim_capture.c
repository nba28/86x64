/* 99_ctype_twin_shim_capture — abigen shim DOUBLE-WRAP via libc's internal
 * underscore-twin namespace (the Civ IV "XML Load Error" root cause).
 *
 * abigen names the i386->x86_64 bridge shim for symbol S `"__" + S`, and the
 * shim's inner body does `call native_target(S)`. When the consider set holds
 * BOTH a public libc function (`_tolower`) AND its internal underscore twin
 * (`___tolower` = C `__tolower`, which old ctype.h inlines call directly), the
 * twin's shim `_____tolower` inner-calls the NAME `___tolower` — which is ALSO
 * a DEFINED sibling shim (the shim for public tolower) in the same link unit,
 * so ld binds the inner call to the SIBLING SHIM instead of libSystem:
 *
 *   app `call ___tolower` (i386, arg at [esp+4])
 *     -> OUTER shim _____tolower: marshals [rbp+0xc] -> edi = 'X'   (correct)
 *     -> `call ___tolower` = INNER SIBLING SHIM (reached via a NATIVE 8-byte
 *        call): re-runs `movl 0xc(%rbp),%edi`, but [rbp+0xc] is now the HIGH
 *        half of the 64-bit return address = 0
 *     -> real tolower(0) = 0.
 *
 * Result: __tolower/__toupper return 0 for EVERY char in the translated
 * process. Civ IV's path-builder CompareNoCase("xml\\") then compares
 * 0 == 0 for every character pair, spuriously reports "equal", skips
 * prepending the XML\ directory, and the first XML load dies in the Carbon
 * "XML Load Error" modal.
 *
 * This test pins the exact import surface with explicit externs (no ctype.h,
 * whose SL inlines would hide the calls): the public pair, the twin pair, and
 * __maskrune (a twin whose shim binds correctly today — a canary proving the
 * fix doesn't disturb healthy single-level twins).
 *
 * RED  (double-wrap): __tolower('X') == 0, __toupper('x') == 0.
 * GREEN (fixed):      __tolower('X') == 120, __toupper('x') == 88, and the
 *                     public pair + __maskrune keep working.
 * Exit 99 = all five checks passed. */

extern int printf(const char *, ...);
extern void exit(int);

/* Public libc pair: shimmed as ___tolower / ___toupper (the names that
 * shadow the twins' native targets). */
extern int tolower(int);
extern int toupper(int);

/* Internal libc ctype twins (asm ___tolower / ___toupper / ___maskrune):
 * exactly what old i386 binaries import via the SL ctype.h inlines. */
extern int __tolower(int);
extern int __toupper(int);
extern int __maskrune(int, unsigned long);

#define CTYPE_L_MASK 0x00001000UL /* BSD _CTYPE_L (lowercase) */

int main(void) {
   const int pub_lo  = tolower('x');            /* 120 */
   const int pub_up  = toupper('x');            /* 88  */
   const int twin_lo = __tolower('X');          /* 120; double-wrap -> 0 */
   const int twin_up = __toupper('x');          /* 88;  double-wrap -> 0 */
   const int mask_lo = __maskrune('x', CTYPE_L_MASK) != 0; /* 1 */

   printf("tolower(x)=%d\n", pub_lo);
   printf("toupper(x)=%d\n", pub_up);
   printf("__tolower(X)=%d\n", twin_lo);
   printf("__toupper(x)=%d\n", twin_up);
   printf("__maskrune_lower=%d\n", mask_lo);

   const int ok = (pub_lo == 120) && (pub_up == 88) &&
                  (twin_lo == 120) && (twin_up == 88) && (mask_lo == 1);
   exit(ok ? 99 : 1);
}
