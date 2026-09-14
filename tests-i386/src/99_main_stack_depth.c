/*
 * 99_main_stack_depth — the translated program's main thread needs a REAL stack.
 *
 * The wrapper (src/86x64/wrapper_setup.c) mmaps a 16 MB low-4GB region and hands
 * the translated _main an esp inside it; everything BELOW that esp is the i386
 * main stack. The argv frame used to be placed at base + 1 MB, so the stack was
 * 1 MB and the 15 MB above it was never used. A real i386 main thread gets 8 MB.
 *
 * Portal 2 (2026-09-14) ran off the bottom of that 1 MB: a native callee's
 * `push rbp` faulted at rsp-8, one page below the region base, with rbp still
 * near the top of the region — 1.08 MB of stack consumed, no recursion needed.
 *
 * The frame now sits directly below the argv strings, so the region size IS the
 * stack size, and the bottom page is PROT_NONE.
 *
 * Two arms, so the guard proves the fix rather than the test:
 *   fixed                                -> 42
 *   M64_NO_FULL_MAIN_STACK=1 (kill sw)   -> SIGSEGV (139), on the guard page
 *
 * WARNING: ends with exit(), never `return`. These tests link `-e _main` with no
 * crt0 and the 86x64.sh wrapper enters _main via `jmp`, so there is no return
 * address to `ret` to.
 */
#include <stdlib.h>
#include <stdio.h>

/* 64 frames x 64 KiB = 4 MiB: far past the old 1 MB, far short of the 16 MB
 * region. Sized in the middle on purpose — the point is the LAYOUT, not a
 * fight with the exact limit. */
#define FRAME_BYTES (64u * 1024u)
#define FRAMES      64

/* volatile + a returned checksum so nothing here can be optimised away, and
 * every frame is actually TOUCHED (an untouched page never faults). */
static unsigned long __attribute__((noinline)) burn(int depth)
{
   volatile unsigned char buf[FRAME_BYTES];
   buf[0]               = (unsigned char)depth;
   buf[FRAME_BYTES / 2] = (unsigned char)depth;
   buf[FRAME_BYTES - 1] = (unsigned char)depth;

   unsigned long sum = (unsigned long)buf[0] + buf[FRAME_BYTES / 2] +
                       buf[FRAME_BYTES - 1];
   if (depth > 0) { sum += burn(depth - 1); }
   return sum;
}

int main(void)
{
   /* sum over depth 64..0 of 3*depth = 3 * (64*65/2) = 6240 */
   unsigned long got = burn(FRAMES);
   if (got != 6240ul) {
      printf("checksum %lu, expected 6240\n", got);
      exit(7);
   }
   printf("burned %u KiB of main stack\n", (FRAMES + 1) * (FRAME_BYTES / 1024));
   exit(42);
}
