/*
 * cfnumber_width_shim.c — ONE job: CFNumber types whose WIDTH is the C ABI's.
 *
 * Four CFNumberType values name a C type rather than a width:
 *     kCFNumberLongType (10), kCFNumberCFIndexType (14), kCFNumberNSIntegerType (15)
 *         -> 4 bytes on i386, 8 on x86_64
 *     kCFNumberCGFloatType (16)
 *         -> float (4) on i386, double (8) on x86_64
 * The generic abigen bridge forwards the type verbatim, so NATIVE CF reads or
 * writes 8 bytes through a pointer to the i386 caller's 4-byte slot.
 * CFNumberGetValue(n, kCFNumberLongType, &int32_local) writes the value's high
 * half over the NEXT stack slot.
 *
 * MEASURED, Portal 2 launcher.dylib GLMDisplayDB::GetModeInfo (i386 0x14b20):
 * it reads Width into [ebp-0x14], Height into [ebp-0x18] and RefreshRate into
 * [ebp-0x1c], in that order, all with kCFNumberLongType. Each 8-byte write zeroes
 * the slot above it, so Width and Height both come back 0 and the current mode
 * never matches the mode list. GetAdapterDisplayMode then fails and shaderapidx9
 * turns the uninitialised format into -1 -> D16 depth -> togl CreateDevice
 * refuses -> Debugger() -> SIGTRAP. PopulateModes loses Height the same way, so
 * every mode is rejected (h < 384).
 *
 * The cure maps each ABI-width type to the fixed-width type of the i386 ABI
 * (SInt32 / Float32) for CFNumberGetValue and CFNumberCreate. Every other type
 * is fixed-width already and passes through unchanged.
 *
 * KILL SWITCH: M64_NO_CFNUMBER_I386_WIDTH=1 forwards the type verbatim (the
 * pre-fix behaviour). Guard: tests-i386 99_cfnumber_i386_width.
 * ABI: reached through the ___CFNumber* trampolines in cfnumber_width_tramp.asm
 * (rdi = &i386 args[0], result in eax); listed in custom.syms.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <stdlib.h>

extern uint64_t _86x64_unwrap_obj_arg(uint32_t h); /* i386 handle -> real CF ref */
extern uint32_t x64_objc_wrap(uint64_t real);

static CFNumberType i386_type(CFNumberType t)
{
   static int off = -1;
   if (off < 0) {
      const char *e = getenv("M64_NO_CFNUMBER_I386_WIDTH");
      off = (e && *e && *e != '0');
   }
   if (off) return t;
   switch (t) {
   case kCFNumberLongType:
   case kCFNumberCFIndexType:
   case kCFNumberNSIntegerType: return kCFNumberSInt32Type;
   case kCFNumberCGFloatType:   return kCFNumberFloat32Type;
   default:                     return t;
   }
}

/* Boolean CFNumberGetValue(CFNumberRef, CFNumberType, void *) */
uint32_t shim_CFNumberGetValue(uint32_t *a)
{
   CFNumberRef n = (CFNumberRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   return CFNumberGetValue(n, i386_type((CFNumberType)(int32_t)a[1]),
                           (void *)(uintptr_t)a[2]);
}

/* CFNumberRef CFNumberCreate(CFAllocatorRef, CFNumberType, const void *) */
uint32_t shim_CFNumberCreate(uint32_t *a)
{
   CFAllocatorRef al = (CFAllocatorRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   CFNumberRef n = CFNumberCreate(al, i386_type((CFNumberType)(int32_t)a[1]),
                                  (const void *)(uintptr_t)a[2]);
   uint64_t v = (uint64_t)(uintptr_t)n;
   return (v >> 32) ? x64_objc_wrap(v) : (uint32_t)v;   /* abigen's CF-return rule */
}
