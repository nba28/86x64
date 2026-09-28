/*
 * Hand-written i386->x86_64 shim for OSAtomicAdd32 / OSAtomicAdd32Barrier.
 *
 * abigen DOES generate a correct ABI shim for these now (commit 5fc6c8a added
 * libsystem_platform.dylib to ABICONV_SYM_SOURCES), which fixed the dominant
 * Thumbnailer-teardown crash where the translated i386 `call OSAtomicAdd32`
 * reached the native register-ABI function directly.
 *
 * A RESIDUAL intermittent crash remains: a reverse-bridged legacy class
 * (Thumbnailer) tears down and a generic refcount-Release helper
 * (translated fn 0x1006778a) does `OSAtomicAdd32(-1, &obj->refcount@8)`
 * BEFORE its `obj != nil` guard, with obj == a near-null garbage value (0x2c)
 * read out of a member-object ivar. The decrement then faults at obj+8 (0x34)
 * INSIDE the atomic, before the caller's nil check can save it.
 *
 * This override (replacing the abigen shim via custom.syms + the MTSHIM
 * trampoline) makes the primitive defensive:
 *   - a legitimate call (address points into real i386 memory) is forwarded to
 *     the native OSAtomicAdd32 unchanged;
 *   - a call whose target ADDRESS is in the first 64 KiB (page-zero / garbage
 *     ivar) is treated as operating on a not-yet-zero refcount and returns a
 *     non-zero result, so a refcount-Release helper skips the dealloc deref and
 *     the object simply leaks rather than crashing during teardown.
 *
 * The near-null guard is generic: no real i386 object lives in page zero, so a
 * sub-64K atomic target can only be a nil-based member address — never a valid
 * refcount slot.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libkern/OSAtomic.h>

/* Real native primitives (libabiconv's own calls are NOT static-interposed). */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#define OSATOMIC_BOGUS_LIMIT 0x10000u

/* i386 frame (MTSHIM): a = &args[0]; a[-1] = i386 return address.
 * OSAtomicAdd32(int32_t amount, volatile int32_t *address):
 *   a[0] = amount, a[1] = address (i386 pointer, low-4GB). */
static int32_t osatomic_add32_common(uint32_t *a, int barrier) {
   int32_t  amount = (int32_t)a[0];
   uint32_t addr32 = a[1];

   if (addr32 < OSATOMIC_BOGUS_LIMIT) {
      /* Pretend the refcount did not reach zero -> caller skips dealloc. */
      return amount ? amount : 1;
   }

   volatile int32_t *addr = (volatile int32_t *)(uintptr_t)addr32;
   return barrier ? OSAtomicAdd32Barrier(amount, addr)
                  : OSAtomicAdd32(amount, addr);
}

int32_t shim_OSAtomicAdd32(uint32_t *a)        { return osatomic_add32_common(a, 0); }
int32_t shim_OSAtomicAdd32Barrier(uint32_t *a) { return osatomic_add32_common(a, 1); }

#pragma clang diagnostic pop
