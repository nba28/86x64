// carbon_event_param_shim.c — ONE job: Carbon event PARAMETERS across the ABI.
//
// GetEventParameter / SetEventParameter move a typed value through a caller
// buffer, sized by the caller. Most parameter types have the same layout on
// i386 and x86_64 (QDPoint, UInt32, Boolean, UniChar text, ...) and pass
// straight through. These do NOT:
//   * references (WindowRef, ControlRef, MenuRef, EventRef, CF types, ...):
//     4-byte handles on the i386 side, 8-byte pointers natively;
//   * CGFloat geometry (HIPoint, HISize, HIRect, CGFloat): float vs double;
//   * HICommand: it embeds a MenuRef (16 bytes i386, 24 native);
//   * CFIndex: long.
// The generic bridge passed the i386 buffer and size straight through, so a
// WindowRef came back TRUNCATED to its low half. MEASURED (PvZ windowed,
// probes/appdownspy.m, 2026-09-29): its mouse-down handler reads 'wind', got
// 0x..030d6300 instead of its window, decided the click was not for it, and
// ignored every click; hover never asks for the window, so it worked.
// Here the native call gets a native-sized buffer and the value is converted.
#include <Carbon/Carbon.h>
#include <stdint.h>
#include <string.h>

extern uint32_t x64_objc_wrap(uint64_t real);           // objc_shim.c
extern uint64_t _86x64_unwrap_obj_arg(uint32_t h);      // objc_shim.c: handle/CF const -> real

enum { K_SAME = 0, K_REF, K_POINT, K_SIZE, K_RECT, K_CGFLOAT, K_CMD, K_INDEX };

static int kind_of(EventParamType t)
{
   switch (t) {
   case 'wind': case 'ctrl': case 'menu': case 'evrf': case 'etrg': case 'hiob':
   case 'cntx': case 'cgim': case 'drag': case 'void': case 'graf': case 'shap':
   case 'scrp': case 'pbrd': case 'cltn': case 'ctra': case 'tbar': case 'tbit':
   case 'ctfr': case 'cfst': case 'cfty': case 'cfar': case 'cfdc': case 'cfma':
   case 'cfmd': case 'cfms': case 'cfas': case 'cfaa': case 'cftf': case 'cfnb':
      return K_REF;
   case 'hipt': return K_POINT;
   case 'hisz': return K_SIZE;
   case 'hirc': return K_RECT;
   case 'cgfl': return K_CGFLOAT;
   case 'hcmd': return K_CMD;
   case 'cfix': return K_INDEX;
   default:     return K_SAME;
   }
}
static const uint32_t i386_size[] = { 0, 4, 8, 8, 16, 4, 16, 4 };
static const uint32_t native_size[] = { 0, 8, 16, 16, 32, 8, 24, 8 };

static uint32_t ref_out(uint64_t v) { return (v >> 32) ? x64_objc_wrap(v) : (uint32_t)v; }

/* native bytes -> i386 bytes */
static void to_i386(int k, const uint8_t *n, uint8_t *o)
{
   switch (k) {
   case K_REF: case K_INDEX: {
      uint64_t v; memcpy(&v, n, 8);
      uint32_t w = k == K_REF ? ref_out(v) : (uint32_t)v;
      memcpy(o, &w, 4); break; }
   case K_POINT: case K_SIZE: case K_RECT: case K_CGFLOAT:
      for (uint32_t i = 0; i < i386_size[k] / 4; i++) {
         double d; memcpy(&d, n + 8 * i, 8);
         float f = (float)d; memcpy(o + 4 * i, &f, 4);
      }
      break;
   case K_CMD: {                       /* attributes, commandID, menuRef, index */
      uint64_t m; memcpy(o, n, 8); memcpy(&m, n + 8, 8);
      uint32_t mh = ref_out(m); memcpy(o + 8, &mh, 4);
      memcpy(o + 12, n + 16, 2); memset(o + 14, 0, 2); break; }
   }
}

/* i386 bytes -> native bytes */
static void to_native(int k, const uint8_t *o, uint8_t *n)
{
   switch (k) {
   case K_REF: {
      uint32_t h; memcpy(&h, o, 4);
      uint64_t v = h ? _86x64_unwrap_obj_arg(h) : 0; memcpy(n, &v, 8); break; }
   case K_INDEX: {
      int32_t h; memcpy(&h, o, 4); int64_t v = h; memcpy(n, &v, 8); break; }
   case K_POINT: case K_SIZE: case K_RECT: case K_CGFLOAT:
      for (uint32_t i = 0; i < i386_size[k] / 4; i++) {
         float f; memcpy(&f, o + 4 * i, 4);
         double d = f; memcpy(n + 8 * i, &d, 8);
      }
      break;
   case K_CMD: {
      uint32_t mh; memcpy(n, o, 8); memcpy(&mh, o + 8, 4);
      uint64_t m = mh ? _86x64_unwrap_obj_arg(mh) : 0; memcpy(n + 8, &m, 8);
      memcpy(n + 16, o + 12, 2); memset(n + 18, 0, 6); break; }
   }
}

#define I386PTR(v) ((void *)(uintptr_t)(uint32_t)(v))

/* OSStatus GetEventParameter(EventRef, EventParamName, EventParamType desired,
 *   EventParamType *outActualType, ByteCount bufferSize, ByteCount *outActualSize,
 *   void *outData)                                  (ByteCount = 4 bytes on i386) */
uint32_t shim_GetEventParameter(uint32_t *a)
{
   EventRef e = (EventRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   const EventParamName name = a[1];
   const EventParamType want = a[2];
   uint32_t *oat = I386PTR(a[3]), *osz = I386PTR(a[5]);
   void *out = I386PTR(a[6]);
   EventParamType at = 0;
   ByteCount n = 0;
   const int k = kind_of(want);
   if (k == K_SAME) {
      OSStatus r = GetEventParameter(e, name, want, oat ? &at : NULL, a[4], osz ? &n : NULL, out);
      if (oat) *oat = at;
      if (osz) *osz = (uint32_t)n;
      return (uint32_t)r;
   }
   uint8_t buf[32] = { 0 };
   OSStatus r = GetEventParameter(e, name, want, &at, native_size[k], &n, out ? buf : NULL);
   if (oat) *oat = at;
   if (osz) *osz = r == noErr ? i386_size[k] : (uint32_t)n;
   if (r == noErr && out) {
      if (a[4] < i386_size[k]) return (uint32_t)errDataSizeMismatch;
      to_i386(k, buf, out);
   }
   return (uint32_t)r;
}

/* OSStatus SetEventParameter(EventRef, EventParamName, EventParamType,
 *   ByteCount size, const void *data) */
uint32_t shim_SetEventParameter(uint32_t *a)
{
   EventRef e = (EventRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   const EventParamType t = a[2];
   const void *data = I386PTR(a[4]);
   const int k = kind_of(t);
   if (k == K_SAME || !data) return (uint32_t)SetEventParameter(e, a[1], t, a[3], data);
   if (a[3] < i386_size[k]) return (uint32_t)errDataSizeMismatch;
   uint8_t buf[32] = { 0 };
   to_native(k, data, buf);
   return (uint32_t)SetEventParameter(e, a[1], t, native_size[k], buf);
}
