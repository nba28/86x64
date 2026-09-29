/* 99_cfdata_bytebuf_bounce — CFDataGetBytePtr/CFDataGetMutableBytePtr must be
 * bounced by their REAL length (CFDataGetLength), never strlen().
 *
 * WHY THIS EXISTS. abigen's generic return-value rule (a `char*`/`unsigned
 * char*` return is treated as a C string, bounced through a strlen(p)+1 copy
 * into the low-4GB heap) is correct for a genuine C string (glGetString) but
 * WRONG for a pointer into a byte BUFFER whose size is carried separately —
 * a 0x00 byte anywhere inside it is legitimate CONTENT, not a terminator.
 *
 * MEASURED (Halo CE, i386 0x2ab7a4): CFPreferencesCopyAppValue returns the
 * "Graphics Options" CFData — 88 bytes, byte 0 == 0x00 (a 3D-mode enum that
 * happens to be zero for the default mode). The cstring bounce saw
 * strlen()==0 and copied ONE byte; Halo's own
 * `memcpy(dst, CFDataGetBytePtr(v), CFDataGetLength(v))` then read 87 bytes
 * of low-heap garbage past that single byte into its global prefs struct,
 * so a flag the app checks came out nonzero and it silently skips the
 * Graphics Settings dialog on every launch. This fixture reproduces the
 * SHAPE (a buffer starting 0x00) directly, without needing CFPreferences
 * (whose Copy side prefs_shim.c blanket-nulls outside a small system-locale
 * allowlist, to dodge the cfprefsd XPC livelock — round-tripping through it
 * here would exercise that allowlist gate, not this bug).
 *
 * src/abiconv/cfdata_bytebuf_shim.c fixes it: CFDataGetBytePtr/
 * CFStringGetPascalStringPtr are bounced by their real length, and
 * CFDataGetMutableBytePtr keeps a per-CFData low mirror flushed
 * write-through by the CF-ref arg unwrap (objc_shim.c unwrap_obj_arg) before
 * the ref's next native use.
 *
 * ARMS. This fixture is the ON side and must exit 0 having survived both
 * checks below. cfdata_bytebuf_bounce_test.sh adds the OFF side by
 * re-running this same binary under M64_NO_CFDATA_BYTEBUF_BOUNCE=1, which
 * reproduces the OLD generic-cstring-bounce behaviour exactly (see the kill
 * switch in cfdata_bytebuf_shim.c) — both checks must FAIL there, or this
 * guard is not exercising the fix.
 *
 * ⚠ The OFF arm's mutable check deliberately triggers a HEAP OOB WRITE (this
 * IS the pre-fix defect: the app writes through a 1-byte cstring-bounce copy
 * as if it were the full buffer) and may crash instead of printing its
 * result line — stdout is buffered, so a lost line is itself a normal way
 * for this guard to observe the OFF arm diverging (see the immutable-only
 * exit code below, and cfstring_range_test.sh for the same idiom).
 *
 * Prints one key=value per line; distinct exit codes (2/3) let the OFF arm's
 * possible crash still be distinguished from a clean run even if buffered
 * stdout is lost.
 */
extern int   printf(const char *, ...);
extern void  exit(int status);
extern void *memcpy(void *, const void *, unsigned long);

typedef const void      *CFAllocatorRef;
typedef const void      *CFDataRef;
typedef void             *CFMutableDataRef;
typedef long              CFIndex;
typedef unsigned char     UInt8;

extern CFDataRef        CFDataCreate(CFAllocatorRef, const UInt8 *, CFIndex);
extern CFIndex           CFDataGetLength(CFDataRef);
extern const UInt8      *CFDataGetBytePtr(CFDataRef);
extern CFMutableDataRef  CFDataCreateMutable(CFAllocatorRef, CFIndex);
extern void              CFDataAppendBytes(CFMutableDataRef, const UInt8 *, CFIndex);
extern UInt8            *CFDataGetMutableBytePtr(CFMutableDataRef);
extern void              CFRelease(CFDataRef);

/* Same size as Halo's real "Graphics Options" blob. */
#define N 88

int main(void)
{
   /* ---- [1] IMMUTABLE: CFDataGetBytePtr + memcpy must deliver every real
    * byte, not just up to the first 0x00. This is the exact Halo shape. */
   UInt8 src[N];
   src[0] = 0;                              /* the leading zero strlen() choked on */
   for (int i = 1; i < N; i++) { src[i] = 0x37; }

   CFDataRef d = CFDataCreate(0, src, N);
   if (!d) { printf("create=0\n"); exit(2); }
   printf("create=1\n");

   CFIndex len = CFDataGetLength(d);
   printf("len=%ld\n", (long)len);

   const UInt8 *p = CFDataGetBytePtr(d);
   if (!p) { printf("getbyteptr=0\n"); exit(2); }
   printf("getbyteptr=1\n");

   UInt8 dst[N];
   for (int i = 0; i < N; i++) { dst[i] = 0xAA; }   /* sentinel */
   memcpy(dst, p, (unsigned long)len);

   int ok = (len == N);
   for (int i = 0; i < N && ok; i++) {
      if (dst[i] != src[i]) { ok = 0; }
   }
   printf("immutable_roundtrip=%d\n", ok);
   CFRelease(d);
   if (!ok) { exit(2); }

   /* ---- [2] MUTABLE write-through: a write through the pointer
    * CFDataGetMutableBytePtr returns must be visible to a SEPARATE later
    * bridge call on the SAME CFDataRef (CFDataGetBytePtr here) — proving the
    * low-mirror sync fires on the ref's next native use, not just that one
    * call in isolation returns something writable. */
   CFMutableDataRef m = CFDataCreateMutable(0, 0);
   if (!m) { printf("mcreate=0\n"); exit(3); }
   printf("mcreate=1\n");

   UInt8 zeros[N];
   for (int i = 0; i < N; i++) { zeros[i] = 0; }
   CFDataAppendBytes(m, zeros, N);

   UInt8 *mp = CFDataGetMutableBytePtr(m);
   if (!mp) { printf("getmutableptr=0\n"); exit(3); }
   printf("getmutableptr=1\n");
   mp[0] = 0;
   for (int i = 1; i < N; i++) { mp[i] = 0x5A; }

   const UInt8 *rp = CFDataGetBytePtr((CFDataRef)m);
   UInt8 rdst[N];
   for (int i = 0; i < N; i++) { rdst[i] = 0xAA; }
   memcpy(rdst, rp, (unsigned long)CFDataGetLength((CFDataRef)m));

   int mok = 1;
   for (int i = 0; i < N; i++) {
      UInt8 want = (i == 0) ? 0 : 0x5A;
      if (rdst[i] != want) { mok = 0; break; }
   }
   printf("mutable_writethrough=%d\n", mok);
   CFRelease((CFDataRef)m);

   printf("done=1\n");
   exit(mok ? 0 : 3);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
