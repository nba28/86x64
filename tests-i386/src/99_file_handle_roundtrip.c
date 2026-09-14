/*
 * 99_file_handle_roundtrip — a FILE* must survive the i386<->native boundary.
 *
 * WHY (Portal 2 2026-09-14). A native FILE lives in libSystem's own heap ABOVE
 * 4GB (measured: fopen -> 0x7ff8_5277abb8), but abigen treated `FILE *` as an
 * ordinary data pointer and simply truncated it into the i386 caller's eax,
 * because FILE escapes the "opaque record" test that marks CF-style handles:
 * macOS's <stdio.h> defines `struct __sFILE` COMPLETELY. So every stdio object
 * handed to translated code was a wild low-32 pointer, and the first use of it
 * faulted or corrupted. typeconv.cc now classifies it as a handle, so a >4GB
 * FILE is wrapped into a low-4GB arena handle and unwrapped again on the way in.
 *
 * ⚠ The printf family does NOT go through those bridges (variadic, hand-marshalled
 * in printf-conv.cc), so fprintf is exercised here DELIBERATELY -- it needs its
 * own unwrap and would otherwise hand native stdio an arena handle.
 *
 * Round-trips real data so a merely non-crashing but wrong pointer still fails:
 * write with fwrite AND fprintf, read back with fread, compare bytes.
 *
 * 42 = FILE* survived and the data matches.  9 = fopen failed (handle unusable).
 * 8 = write path failed.  7 = read-back mismatch.  6 = fprintf path failed.
 */
#include <stdio.h>
#include <string.h>

extern void exit(int status);

int main(void) {
   const char *path = "/tmp/86x64_file_handle_roundtrip.txt";
   static const char blob[] = "ABCDEFGHIJ";

   FILE *w = fopen(path, "wb");
   if (!w) { exit(9); }
   if (fwrite(blob, 1, 10, w) != 10) { exit(8); }
   /* variadic path: its own unwrap in printf-conv.cc */
   if (fprintf(w, "%s%d", "xy", 7) != 3) { exit(6); }
   if (fclose(w) != 0) { exit(8); }

   FILE *r = fopen(path, "rb");
   if (!r) { exit(9); }
   char got[32];
   memset(got, 0, sizeof got);
   size_t n = fread(got, 1, 13, r);
   fclose(r);
   if (n != 13) { exit(7); }
   if (memcmp(got, "ABCDEFGHIJxy7", 13) != 0) { exit(7); }

   remove(path);
   exit(42);
}
