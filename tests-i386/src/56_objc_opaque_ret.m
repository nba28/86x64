/* 56_objc_opaque_ret.m — opaque `void*` (^v) RETURN wrap across the i386->x86_64
 * forward bridge. The RETURN-side counterpart of 46_objc_opaque_ptr (the `^v`
 * ARG unwrap).
 *
 * A native ObjC method that RETURNS a `void *` (encoding `^v`) hands back a real
 * 64-bit pointer; the i386 caller stores it in a 32-bit slot, truncating a >4GB
 * native pointer to its low 32 bits. The `@`/`#`/`^{CF=}` return kinds were
 * already wrapped into low-4GB arena handles, but a bare `^v` return was NOT —
 * it fell through to ret kind 0 (scalar passthrough) and truncated.
 *
 * Quinn 3.5.7's `_QuinnGeneralFastDrawCells` fetches its CoreGraphics draw
 * context with `[[NSGraphicsContext currentContext] graphicsPort]` — a `^v`
 * return whose value is a >4GB CGContextRef. Pre-fix the truncated context fed
 * every `CGContextDrawImage` / `CGContextFillRect` (abigen CGContext* shims that
 * unwrap their context arg via convert_cf_ptr), and CoreGraphics SILENTLY no-ops
 * on an invalid context — so the falling/placed Tetris BLOCKS rendered invisibly
 * (no crash). graphicsPort returns `^v`; the modern `-[NSGraphicsContext
 * CGContext]` returns `^{CGContext=}` (already wrapped), but Quinn uses the
 * deprecated graphicsPort.
 *
 * Fix: a `^v` return is wrapped CONDITIONALLY — only a >4GB value becomes a
 * low-4GB arena handle (a genuine <=4GB `void*` buffer passes through verbatim,
 * exactly like abigen's `.cfretlow`). The consuming C shim's
 * __86x64_unwrap_obj_arg recovers the real pointer.
 *
 * Reproduced WITHOUT AppKit on the pure marshalling fact, Foundation only:
 *
 *   A (the fix): a real >4GB object pointer flows out through a `^v` return
 *     (`-[NSValue pointerValue]`) and, re-boxed through the (already-fixed) `^v`
 *     ARG path, must yield a value EQUAL to the reference. Pre-fix the `^v`
 *     return truncates, so the re-boxed pointer differs -> NOT equal.
 *   C (negative): a genuine low-4GB `void*` returned through `^v` must pass
 *     through UNCHANGED (the conditional wrap must never disturb real buffers).
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   int failures = 0;

   /* A: stash a real >4GB object pointer in an NSValue (the `^v` ARG path unwraps
    * the handle to the real pointer), read it back through the `^v` RETURN
    * (pointerValue), and re-box it. A non-truncating `^v` return round-trips to
    * the SAME real pointer -> equal values. A truncating one stores the low 32
    * bits -> a different pointer -> NOT equal (isEqualToValue compares the stored
    * pointer bytes; no dereference, so the failure is deterministic, not a crash). */
   id token = [[NSObject alloc] init];                  /* real obj >4GB (held as an arena handle) */
   NSValue *v        = [NSValue valueWithPointer:(void *)token];  /* ^v arg unwrap -> stores real obj */
   void    *p        = [v pointerValue];                /* ^v RETURN (the fix under test) */
   NSValue *roundtrip = [NSValue valueWithPointer:p];   /* ^v arg unwrap -> real obj iff p intact */
   if ([roundtrip isEqualToValue:v]) { puts("ok opaque_voidptr_return_wrapped"); }
   else { puts("FAIL opaque_voidptr_return_wrapped"); failures++; }

   /* C (negative): a low-4GB `void*` returned through `^v` passes through verbatim
    * (the conditional wrap skips <=4GB), so real i386 buffers are never disturbed. */
   void *raw  = (void *)0x12345678;
   void *back = [[NSValue valueWithPointer:raw] pointerValue];    /* ^v return, low -> passthrough */
   if (back == raw) { puts("ok plain_voidptr_return_passthrough"); }
   else { puts("FAIL plain_voidptr_return_passthrough"); failures++; }

   [pool release];
   exit(failures);
}
