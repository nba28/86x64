/* 99_atomic_bit_volatile — OSAtomicTestAndSet/ClearBarrier take a
 * `volatile void *`; abigen matched void pointees by NAME, so the volatile one
 * was deep-copied and the bit flipped in a temporary. PvZ: every sound's
 * play-request bit stayed set, no sound effect ever started, quit hung.
 * ON: exit 42. OFF arm = a libabiconv from before the fix (verified by hand:
 * exit 1, the byte never changes). */
#include <libkern/OSAtomic.h>
#include <stdlib.h>

int main(void)
{
   static volatile unsigned char b[4];
   if (OSAtomicTestAndSetBarrier(0, b) != 0) { exit(2); }      /* was clear */
   if (b[0] != 0x80) { exit(1); }                              /* bit 0 = MSB of byte 0 */
   if (OSAtomicTestAndClearBarrier(0, b) != 1) { exit(3); }
   if (b[0] != 0) { exit(4); }
   exit(42);
}
