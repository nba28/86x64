/*
 * uckey_shim.c — UCKeyTranslate for i386 callers. abigen skips it (the
 * UniChar[] parameter is an incomplete array) and UniCharCount is 4 bytes on
 * i386 but 8 on x86_64, so the actual-length out-param must be narrowed.
 * PvZ passes *GetResource('uchr'); with no classic resource that is NULL,
 * which gets a clean paramErr instead of a native deref.
 */
#include <stdint.h>
#include <Carbon/Carbon.h>

uint32_t shim_UCKeyTranslate(uint32_t *a) {
   const UCKeyboardLayout *layout = (const UCKeyboardLayout *)(uintptr_t)a[0];
   uint32_t *actual32 = (uint32_t *)(uintptr_t)a[8];
   if (actual32) { *actual32 = 0; }
   if (!layout) { return (uint32_t)(int32_t)paramErr; }
   UniCharCount actual = 0;
   OSStatus st = UCKeyTranslate(layout, (UInt16)a[1], (UInt16)a[2], a[3], a[4],
                                (OptionBits)a[5], (UInt32 *)(uintptr_t)a[6],
                                (UniCharCount)a[7], &actual,
                                (UniChar *)(uintptr_t)a[9]);
   if (actual32) { *actual32 = (uint32_t)actual; }
   return (uint32_t)st;
}
