/* 79_star_format — `*` field-width / `.*` precision vararg conversion
 * (printf-conv.cc). A `*` in the width or precision takes an EXTRA int argument
 * from the vararg stream (`printf("%*.*f", w, p, x)` consumes w, p, x). The
 * i386->x86_64 marshaller parsed only literal-digit widths/precisions, so the
 * '*' fell through to printf_parse_type and threw std::invalid_argument
 * ("invalid conversion specifier") -> uncaught -> std::terminate -> abort.
 * Civ IV's "Launch in Window" path hit exactly `%*.*f` (formatting a numeric
 * field) and aborted. This exercises star-width, star-precision, and both,
 * plus a plain literal-width control, through printf (stdout). */
#include <stdio.h>
extern void exit(int status);
int main(void){
   printf("[%*.*f]\n", 8, 2, 3.14159);   /* both: width 8, prec 2 -> [    3.14] */
   printf("[%*d]\n", 5, 42);             /* star width only        -> [   42]   */
   printf("[%.*g]\n", 3, 2.71828);       /* star precision only    -> [2.72]    */
   printf("[%-*.*s]\n", 6, 3, "abcdef"); /* star w/p on string     -> [abc   ]  */
   printf("[%8.2f]\n", 3.14159);         /* literal-width control  -> [    3.14] */
   exit(0);
}
