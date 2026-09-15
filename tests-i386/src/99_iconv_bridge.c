/*
 * 99_iconv_bridge — iconv_open / iconv / iconv_close must reach libiconv
 * through a bridge.
 *
 * All three were in abigen's consider set but had no prototype (no <iconv.h>
 * in src/abiconv/includes.h), so abigen emitted no bridge and static-interpose
 * left the binds on the REAL libiconv: a raw cross-ABI call whose "arguments"
 * were whatever sat in rdi/rsi. MEASURED, Portal 2: vguimatsurface's
 * _V_UTF8ToUCS2 (from CMatSystemSurface::Init) died in iconv_open ->
 * _citrus_iconv_open -> strlcpy -> strlen(0x10).
 *
 * What the bridges must get right, each checked below:
 *   - a native iconv_t lives ABOVE 4GB, so iconv_open must hand back a handle
 *     that iconv / iconv_close accept;
 *   - (iconv_t)-1 must stay -1, or the app's own failure test never fires;
 *   - iconv's four in/out args point at 4-byte i386 slots (char **, size_t *):
 *     the native call advances 8-byte copies that must be written back. A
 *     deliberately short output buffer makes the advanced counts the only
 *     evidence of how far the conversion got.
 *
 * WARNING: ends with exit(), never `return`. These tests link `-e _main` with
 * no crt0 and the 86x64.sh wrapper enters _main via `jmp`.
 */
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char kIn[] = "h\xc3\xa9llo";                 /* "héllo", 6 bytes */
static const unsigned char kUcs2[] = { 'h',0, 0xe9,0, 'l',0, 'l',0, 'o',0 };

/* Non-leaf with locals live across every call, so a bridge that damages the
 * frame shows up as a wrong result here instead of somewhere unrelated. */
static int __attribute__((noinline)) round_trip(void)
{
   unsigned char out[32];
   iconv_t cd = iconv_open("UCS-2LE", "UTF-8");
   if (cd == (iconv_t)-1) { return 1; }

   /* room for two UCS-2 units only: converts "h" and "é", then E2BIG */
   char *in = (char *)kIn;  size_t inleft = sizeof kIn - 1;
   char *op = (char *)out;  size_t outleft = 4;
   size_t r = iconv(cd, &in, &inleft, &op, &outleft);
   if (r != (size_t)-1)                        { return 2; }
   if (in != kIn + 3 || inleft != 3)           { return 3; }
   if (op != (char *)out + 4 || outleft != 0)  { return 4; }

   /* the rest, into the remaining space */
   outleft = sizeof out - 4;
   r = iconv(cd, &in, &inleft, &op, &outleft);
   if (r == (size_t)-1 || inleft != 0)              { return 5; }
   if ((size_t)(op - (char *)out) != sizeof kUcs2)  { return 6; }
   if (memcmp(out, kUcs2, sizeof kUcs2) != 0)       { return 7; }

   if (iconv_close(cd) != 0) { return 8; }
   return 0;
}

int main(void)
{
   if (iconv_open("NO-SUCH-CHARSET-86x64", "UTF-8") != (iconv_t)-1) {
      printf("bogus charset did not return (iconv_t)-1\n");
      exit(20);
   }
   for (int i = 0; i < 4; ++i) {
      int rc = round_trip();
      if (rc != 0) { printf("round %d failed rc=%d\n", i, rc); exit(rc); }
   }
   printf("iconv bridged\n");
   exit(42);
}
