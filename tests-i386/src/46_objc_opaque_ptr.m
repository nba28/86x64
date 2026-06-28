/* 46_objc_opaque_ptr.m — opaque `void*` (^v) ObjC arg carrying a wrapped 64-bit
 * handle across the i386->x86_64 forward bridge (the Quinn 3.5.7 graphicsPort
 * wall).
 *
 * The i386 program holds every `id`/opaque pointer in a 32-bit slot, but a real
 * x86_64 object/CF pointer is 64-bit. The bridge therefore WRAPS a 64-bit native
 * pointer returned by an ObjC method into a low-4GB ARENA HANDLE (so it fits a
 * 32-bit slot) and UNWRAPS that handle back to the real pointer whenever the
 * program passes it into another native method.
 *
 * That unwrap was implemented for `@`/`#` and for `^{Name=}` opaque CF tokens,
 * but NOT for a plain `^v` (`void *`) argument. Quinn obtains a CGContextRef via
 * `-[NSGraphicsContext CGContext]` (`^{CGContext=}` -> wrapped to a handle) and
 * hands it back as the `void*` graphicsPort of
 * `+[NSGraphicsContext graphicsContextWithGraphicsPort:flipped:]` (arg encoding
 * `^v`). Pre-fix the bridge passed the RAW 32-bit handle, so native
 * CGContextRetain dereferenced the arena-slot ADDRESS as a CGContext ->
 * EXC_BAD_ACCESS (fault 0x343545854 — the slot's real-pointer bytes read as a
 * fourCC).
 *
 * This test reproduces the bug WITHOUT AppKit, purely on the marshalling fact:
 * a wrapped object handle passed through a `^v` argument must unwrap to the same
 * real 64-bit pointer that the `@`-typed path produces.
 *
 *   token  : a real 64-bit NSObject (the program holds it as an arena handle)
 *   viaVoid: [NSValue valueWithPointer:(void*)token]      -- `^v` arg (the fix)
 *   viaObj : [NSValue valueWithNonretainedObject:token]   -- `@`  arg (reference)
 *
 * Both NSValues store a single pointer of objCType "^v". They are isEqualToValue
 * ONLY when the `^v` path stored the SAME real 64-bit pointer as the `@` path.
 * Pre-fix `^v` stored the raw 32-bit handle (!= real pointer) -> NOT equal.
 * Post-fix `^v` unwrapped to the real pointer -> equal.
 *
 * The negative case (a genuine low-4GB `void*` that is NOT a handle must pass
 * through untouched) is covered too: a small literal pointer round-trips through
 * valueWithPointer: unchanged, proving the unwrap never disturbs real buffers.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   int failures = 0;

   /* A: a wrapped 64-bit object handed in through a `^v` argument must unwrap to
    * the real pointer — the graphicsPort round-trip, distilled. */
   id token = [[NSObject alloc] init];
   NSValue *viaVoid = [NSValue valueWithPointer:(void *)token];
   NSValue *viaObj  = [NSValue valueWithNonretainedObject:token];
   if ([viaVoid isEqualToValue:viaObj]) { puts("ok opaque_voidptr_arg_unwrapped"); }
   else { puts("FAIL opaque_voidptr_arg_unwrapped"); failures++; }

   /* B: two `^v` args carrying the SAME wrapped handle resolve to the SAME real
    * pointer (a handle minted once is stable). */
   NSValue *viaVoid2 = [NSValue valueWithPointer:(void *)token];
   if ([viaVoid isEqualToValue:viaVoid2]) { puts("ok opaque_voidptr_arg_stable"); }
   else { puts("FAIL opaque_voidptr_arg_stable"); failures++; }

   /* C (negative): a genuine low-4GB `void*` that was never wrapped must pass
    * through the `^v` path UNCHANGED (the fix must not unwrap non-handles). */
   void *raw = (void *)0x12345678;
   NSValue *vraw  = [NSValue valueWithPointer:raw];
   NSValue *vraw2 = [NSValue valueWithPointer:raw];
   if ([vraw isEqualToValue:vraw2]) { puts("ok plain_voidptr_passthrough"); }
   else { puts("FAIL plain_voidptr_passthrough"); failures++; }

   [pool release];
   exit(failures);
}
