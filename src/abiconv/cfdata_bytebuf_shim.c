/*
 * cfdata_bytebuf_shim.c — ONE job: byte-buffer CF return pointers must be
 * bounced by their REAL length, never treated as a C string.
 *
 * abigen's generic return-value rule (abigen.cc ~line 1569) treats any
 * `char*`/`unsigned char*` return as a C string and bounces a high (>=4GB)
 * native pointer through x64_cstr_ret_low (strlen(p)+1 bytes copied into the
 * low-4GB heap the i386 caller can address). That rule is correct for a
 * genuine C string (glGetString's GLubyte*) but wrong for a pointer into a
 * BYTE BUFFER whose length is carried separately:
 *
 *   CFDataGetBytePtr(CFDataRef)        length = CFDataGetLength(theData)
 *   CFDataGetMutableBytePtr(CFMutableDataRef)  same, PLUS write-through
 *   CFStringGetPascalStringPtr(...)    length = 1 + the leading Pascal
 *                                       length byte (no NUL terminator)
 *
 * MEASURED (Halo CE, i386 0x2ab7a4): CFPreferencesCopyAppValue returns the
 * "Graphics Options" CFData (88 bytes, byte 0 == 0x00 — a 3D-mode enum which
 * happens to be zero for the default mode). CFDataGetBytePtr's cstring
 * bounce saw strlen()==0 and copied ONE byte; the app's own
 * `memcpy(dst, CFDataGetBytePtr(v), CFDataGetLength(v))` then read 87 bytes
 * of low-heap garbage past that one byte into its global prefs struct. The
 * garbage flag Halo checks at +0x50 came out nonzero, so Halo skips the
 * Graphics Settings dialog on every launch (unless Command is held) and
 * later WRITES the garbage-derived struct back, persisting it.
 *
 * These three symbols are listed in custom.syms (abigen ignore list) so
 * neither abigen pass emits a competing `global ___CFDataGetBytePtr` etc.;
 * this is the hand-bridge sibling of cfnumber_width_shim.c /
 * cfstring_range_shim.c (own MTSHIM trampoline copy in
 * cfdata_bytebuf_tramp.asm, per the one-shim-one-job rule — no edit to
 * the shared maptable_tramp.asm).
 *
 * ── WRITE-THROUGH for the MUTABLE accessor ──────────────────────────────
 * CFDataGetMutableBytePtr hands the app a pointer it writes through
 * directly — a copy would silently discard those writes (e.g. Halo's own
 * "write Graphics Options back" path at i386 0x29ce97 would then persist
 * whatever the copy started as, never the app's edits). When the real
 * pointer is already low (<4GB — the common case for a freshly malloc'd
 * small buffer under ASLR is NOT guaranteed, so this is not assumed, only
 * checked) it passes straight through with no mirror at all. Only a genuine
 * high pointer gets a per-CFData low MIRROR buffer that the app writes into;
 * x64_cfdata_mirror_flush (called from objc_shim.c's unwrap_obj_arg, the one
 * choke point every subsequent CF call on that same ref passes through
 * before reaching native code) pushes the mirror's bytes back into the real
 * buffer first, so the next native use of that CFDataRef (CFDataGetBytePtr,
 * CFPreferencesSetAppValue, CFDataGetLength, ...) sees the app's writes.
 * This is the "keep a per-CFData low mirror and sync" design named in the
 * task over "ensure the CFData's storage is low": forcing CFMutableData's
 * OWN allocator low would need hooking every CFData creation entry point
 * (CFDataCreateMutable, NSMutableData, CFDataCreateMutableCopy, ...) to
 * inject a custom low-backed CFAllocator, a much wider change for the same
 * correctness the mirror already gives at the one place it is observable.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t _86x64_unwrap_obj_arg(uint32_t h);        /* i386 handle -> real CF ref */
extern void *x64_databuf_ret_low(const void *p, size_t n); /* objc_shim.c */
extern const char *x64_cstr_ret_low(const char *p);        /* objc_shim.c */

/* KILL SWITCH, OFF-ARM CONTROL ONLY: M64_NO_CFDATA_BYTEBUF_BOUNCE=1 makes this
 * shim's return bounce IDENTICAL to abigen's old generic cstring-return rule
 * (the one that used to fire here before these symbols were added to
 * custom.syms) — strlen-truncated, and CFDataGetMutableBytePtr un-mirrored —
 * reproducing the exact pre-fix defect for tests-i386's two-arm guard
 * (cfdata_bytebuf_bounce_test.sh). Not a real-world escape hatch: no
 * legitimate reason to run a shipped app this way. */
static int bounce_broken(void)
{
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("M64_NO_CFDATA_BYTEBUF_BOUNCE");
      v = (e && *e && *e != '0');
   }
   return v;
}

/* const UInt8 *CFDataGetBytePtr(CFDataRef theData) */
uint32_t shim_CFDataGetBytePtr(uint32_t *a)
{
   CFDataRef d = (CFDataRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   const UInt8 *p = d ? CFDataGetBytePtr(d) : NULL;
   if (!p) { return 0; }
   void *low = bounce_broken()
      ? (void *)x64_cstr_ret_low((const char *)p)
      : x64_databuf_ret_low(p, (size_t)CFDataGetLength(d));
   return (uint32_t)(uintptr_t)low;
}

/* ConstStringPtr CFStringGetPascalStringPtr(CFStringRef theString,
 *                                           CFStringEncoding encoding) */
uint32_t shim_CFStringGetPascalStringPtr(uint32_t *a)
{
   CFStringRef s = (CFStringRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   CFStringEncoding enc = (CFStringEncoding)a[1];
   const unsigned char *p =
      (const unsigned char *)CFStringGetPascalStringPtr(s, enc);
   if (!p) { return 0; }
   void *low;
   if (bounce_broken()) {
      low = (void *)x64_cstr_ret_low((const char *)p);
   } else {
      const size_t n = 1 + (size_t)p[0];   /* length byte + payload, no NUL */
      low = x64_databuf_ret_low(p, n);
   }
   return (uint32_t)(uintptr_t)low;
}

/* ---- CFDataGetMutableBytePtr: low mirror + write-through sync ---- */

#define CFDATA_MIRROR_CAP 32
static struct {
   CFDataRef ref;
   void *low;
   size_t cap;
} g_cfdata_mirror[CFDATA_MIRROR_CAP];
static unsigned g_cfdata_mirror_n = 0;

/* Sync the mirror's bytes back into the real CFMutableData buffer. Called
 * from the CF-ref arg unwrap (objc_shim.c) right before ANY resolved ref is
 * handed to native code, so it fires for every subsequent CF call the app
 * makes on this same CFDataRef — CFDataGetBytePtr, CFPreferencesSetAppValue,
 * CFDataGetLength, CFRelease, another CFDataGetMutableBytePtr, all of it.
 * O(1) no-op while no mutable CFData has ever been exposed (the common
 * case), then a tiny linear scan (never more than a handful of live
 * mutable-data objects in practice). */
void x64_cfdata_mirror_flush(uint64_t ref);
void x64_cfdata_mirror_flush(uint64_t ref)
{
   if (!g_cfdata_mirror_n) { return; }
   for (unsigned i = 0; i < g_cfdata_mirror_n; ++i) {
      if ((uint64_t)(uintptr_t)g_cfdata_mirror[i].ref != ref) { continue; }
      CFDataRef d = g_cfdata_mirror[i].ref;
      const CFIndex n = CFDataGetLength(d);
      UInt8 *native = CFDataGetMutableBytePtr((CFMutableDataRef)d);
      const size_t cpy =
         (size_t)n < g_cfdata_mirror[i].cap ? (size_t)n : g_cfdata_mirror[i].cap;
      if (native && cpy) { memcpy(native, g_cfdata_mirror[i].low, cpy); }
      return;
   }
}

/* Find (or start) this ref's mirror, grown to >= len, and sync IN from the
 * real buffer (native -> mirror) so this exposure reflects any bytes a
 * native call wrote directly since the app last saw it (CFDataAppendBytes
 * and friends write straight to the real buffer, never through the
 * mirror). A mirrored ref is RETAINED: were it freed, its address could be
 * reused by an unrelated object and the flush would scribble the stale
 * mirror into it. A full pool evicts round-robin (flush, then release).
 * ponytail: the last CFDATA_MIRROR_CAP mutable datas stay pinned; track
 * CFRelease if that ever matters. Returns NULL only on allocation failure. */
static unsigned g_cfdata_mirror_evict;
static void *cfdata_mirror_sync_in(CFDataRef ref, const void *native_p, size_t len)
{
   unsigned i;
   for (i = 0; i < g_cfdata_mirror_n; ++i) {
      if (g_cfdata_mirror[i].ref == ref) { break; }
   }
   if (i == g_cfdata_mirror_n) {
      if (g_cfdata_mirror_n >= CFDATA_MIRROR_CAP) {
         i = g_cfdata_mirror_evict++ % CFDATA_MIRROR_CAP;
         x64_cfdata_mirror_flush((uint64_t)(uintptr_t)g_cfdata_mirror[i].ref);
         CFRelease(g_cfdata_mirror[i].ref);
      } else {
         ++g_cfdata_mirror_n;
      }
      g_cfdata_mirror[i].ref = (CFDataRef)CFRetain(ref);
   }
   if (g_cfdata_mirror[i].cap < len) {
      void *grown = malloc(len);              /* low-4GB shim heap */
      if (!grown || (uintptr_t)grown >= 0x100000000UL) { return NULL; }
      free(g_cfdata_mirror[i].low);           /* NULL-safe; avoids leaking on regrowth */
      g_cfdata_mirror[i].low = grown;
      g_cfdata_mirror[i].cap = len;
   }
   if (len) { memcpy(g_cfdata_mirror[i].low, native_p, len); }
   return g_cfdata_mirror[i].low;
}

/* UInt8 *CFDataGetMutableBytePtr(CFMutableDataRef theData) */
uint32_t shim_CFDataGetMutableBytePtr(uint32_t *a)
{
   CFMutableDataRef d = (CFMutableDataRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   if (!d) { return 0; }
   UInt8 *native = CFDataGetMutableBytePtr(d);
   if (!native) { return 0; }
   if ((uintptr_t)native < 0x100000000ULL) {
      return (uint32_t)(uintptr_t)native;   /* already low: no mirror needed */
   }
   if (bounce_broken()) {
      /* pre-fix behaviour: an un-mirrored, un-length-aware cstring copy —
       * the app's writes through the returned pointer never reach `native` */
      return (uint32_t)(uintptr_t)x64_cstr_ret_low((const char *)native);
   }
   void *low = cfdata_mirror_sync_in(d, native, (size_t)CFDataGetLength(d));
   return low ? (uint32_t)(uintptr_t)low : 0;
}
