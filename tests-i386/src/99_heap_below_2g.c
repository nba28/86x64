/* 99_heap_below_2g — malloc must hand i386 code pointers that are POSITIVE as
 * a signed int, as real 32-bit Darwin did. PvZ's reanim image lookup takes an
 * Image* or a small id and range-checks it signed (`id <= 1000`); a heap
 * pointer at 0x8xxxxxxx passed as an id and crashed Adventure mode.
 * ON: exit 42. OFF (M64_HEAP_HIGH_FIRST=1 at run time): exit 1. */
#include <stdlib.h>

static int lookup_is_id(int id) { return id != 0 && id <= 1000; }   /* PvZ's shape */

int main(void)
{
   void *p = malloc(64);
   exit(lookup_is_id((int)(long)p) ? 1 : 42);
}
