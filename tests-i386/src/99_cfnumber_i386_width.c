/* 99_cfnumber_i386_width — CFNumber types named after a C type keep the i386 width.
 *
 * kCFNumberLongType / CFIndexType / NSIntegerType are 4 bytes on i386 and 8 on
 * x86_64; kCFNumberCGFloatType is float vs double. Forwarded verbatim, native CF
 * writes 8 bytes through a pointer to a 4-byte i386 slot (clobbering the next
 * one) and CFNumberCreate reads 8.
 *
 * MEASURED, Portal 2 launcher.dylib GLMDisplayDB::GetModeInfo: Width, Height and
 * RefreshRate read with kCFNumberLongType into ADJACENT stack slots, in
 * descending address order, so each write zeroed the one read before it ->
 * Width = Height = 0 -> "no current mode" -> display format -1 -> D16 depth ->
 * togl CreateDevice Debugger() -> SIGTRAP. This fixture reproduces exactly that
 * slot order.
 *
 * ARMS: exit 42 = all correct. OFF arm: run the same binary with
 * M64_NO_CFNUMBER_I386_WIDTH=1 (runtime switch in cfnumber_width_shim.c); it must
 * NOT exit 42 (Width comes back 0, the canary dies, the Long create reads the
 * canary as the high half, CGFloat reads garbage).
 */
typedef const void *CFTypeRef;
typedef const struct __CFNumber *CFNumberRef;
typedef long CFIndex;
typedef unsigned char Boolean;
extern CFNumberRef CFNumberCreate(const void *alloc, CFIndex type, const void *valuePtr);
extern Boolean CFNumberGetValue(CFNumberRef n, CFIndex type, void *valuePtr);
extern void CFRelease(CFTypeRef);
extern int printf(const char *, ...);
extern void exit(int);

enum { kSInt32 = 3, kSInt64 = 4, kFloat64 = 6, kLong = 10, kCFIndex = 14, kCGFloat = 16 };

struct frame { long rr, h, w, canary; };   /* ascending addresses, like [ebp-0x1c..-0x10] */

int main(void)
{
   int bad = 0;
   int wv = 1512, hv = 982, rv = 120;
   CFNumberRef nw = CFNumberCreate(0, kSInt32, &wv);
   CFNumberRef nh = CFNumberCreate(0, kSInt32, &hv);
   CFNumberRef nr = CFNumberCreate(0, kSInt32, &rv);

   /* 1. GetValue, GetModeInfo's order: w, then h (below it), then rr (below h) */
   volatile struct frame f = { 0, 0, 0, 0x5a5a5a5a };
   CFNumberGetValue(nw, kLong, (void *)&f.w);
   CFNumberGetValue(nh, kLong, (void *)&f.h);
   CFNumberGetValue(nr, kCFIndex, (void *)&f.rr);
   if (f.w != 1512 || f.h != 982 || f.rr != 120 || f.canary != 0x5a5a5a5a) bad |= 1;

   /* 2. Create from a 4-byte long followed by a non-zero neighbour */
   volatile long src[2] = { 77, 0x11111111 };
   CFNumberRef nl = CFNumberCreate(0, kLong, (const void *)&src[0]);
   long long got = 0;
   CFNumberGetValue(nl, kSInt64, &got);
   if (got != 77) bad |= 2;

   /* 3. CGFloat is a float on i386 */
   volatile float fl[2] = { 2.5f, 0 };
   CFNumberRef nf = CFNumberCreate(0, kCGFloat, (const void *)&fl[0]);
   double d = 0;
   CFNumberGetValue(nf, kFloat64, &d);
   volatile float out[2] = { 0, 1.0f };
   CFNumberGetValue(nf, kCGFloat, (void *)&out[0]);
   if (d != 2.5 || out[0] != 2.5f || out[1] != 1.0f) bad |= 4;

   printf("w=%ld h=%ld rr=%ld canary=%lx long=%ld cgfloat*10=%d/%d bad=%d\n",
          f.w, f.h, f.rr, f.canary, (long)got, (int)(d * 10), (int)(out[0] * 10), bad);
   exit(bad ? 64 + bad : 42);
}
