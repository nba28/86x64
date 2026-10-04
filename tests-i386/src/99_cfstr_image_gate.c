/* 99_cfstr_image_gate — a C bridge `void *` argument is a CFSTR constant only
 * if it lies inside some loaded image (static data; objc_shim.c
 * i386_cfstr_to_real). Without that gate every void* (memcpy, free, GL data)
 * was probed as a candidate record, and a HEAP buffer whose bytes happen to
 * read as an i386 CFConstantString {isa, 0x7c8, cstr, length} was swapped for
 * a native NSString: memcpy copied the NSString's bytes, not the buffer's.
 * Exit 42 = the buffer is copied exactly. Kill switch
 * ABICONV_NO_CFSTR_IMAGE_GATE=1 (run time) restores the probe. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char text[] = "gate";

int main(void) {
   uint32_t *rec = malloc(16);
   rec[0] = 0;                              /* isa: never inspected */
   rec[1] = 0x7c8;                          /* ASCII8 constant flags */
   rec[2] = (uint32_t)(uintptr_t)text;      /* cstr */
   rec[3] = 4;                              /* exact strlen */
   uint32_t out[4] = {0};
   void *(*volatile cpy)(void *, const void *, size_t) = memcpy;
   cpy(out, rec, sizeof out);
   const int same = memcmp(out, rec, sizeof out) == 0;
   printf("copied %s: %08x %08x %08x %08x\n", same ? "exactly" : "SOMETHING ELSE",
          out[0], out[1], out[2], out[3]);
   exit(same ? 42 : 1);
}
