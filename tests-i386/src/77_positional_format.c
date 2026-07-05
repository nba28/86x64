/* 77_positional_format — POSITIONAL (%N$) vararg conversion (printf-conv.cc).
 * The i386->x86_64 vararg marshaller parsed flags/width BEFORE the positional
 * `%N$` prefix, so `%1$d` mis-read the '1' as a field width and threw on '$';
 * out-of-order positional (%2$ before %1$) also requires converting arg SLOTS
 * in index order, not format order. Civ IV's ASL disk-space alert uses the
 * positional double `%1$.2g` on the CFStringCreateWithFormat path; this
 * exercises the same parse + slot-order logic via printf (stdout). */
#include <stdio.h>
extern void exit(int status);
int main(void){
   printf("%1$s\n", "hello");        /* lone positional -> hello */
   printf("%2$s|%1$d\n", 7, "X");    /* out-of-order, slots (int,str) -> X|7 */
   exit(0);
}
