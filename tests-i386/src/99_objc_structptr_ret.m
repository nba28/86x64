/* 99_objc_structptr_ret.m — a native ObjC method that RETURNS A POINTER TO A
 * STRUCT hands back a >4GB native pointer, which the i386 caller stores in a
 * 4-byte slot. That pointer is a HANDLE, not a buffer, and truncating it is
 * always wrong.
 *
 * Sibling of 56_objc_opaque_ret (the bare `^v` return) and 46_objc_opaque_ptr
 * (the `^v` arg). Those covered `^v`, `^{Name=}` (empty body) and `^{Name=#}`;
 * a struct pointer WITH A BODY was deliberately left on raw truncation, on the
 * assumption that "i386 dereferences it".
 *
 * ★MEASURED COUNTEREXAMPLE (Portal 2, 2026-09-22). libtogl's GLMContext ctor
 * does `this->ctx = (CGLContextObj)[nsglCtx CGLContextObj]` and then
 * `CGLSetParameter(this->ctx, kCGLCPClientStorage, ...)`. The ObjC runtime
 * types that return `^{_CGLContextObject=^{__GLIContextRec}{
 * __GLIFunctionDispatchRec=...}^{_CGLPrivateObject}^v}` — a struct pointer WITH
 * a body — because AppKit was built against OpenGL's INTERNAL header, while
 * every client header only forward-declares the struct. So the two bridges
 * disagreed about one pointer: the C side already treats CGLContextObj as an
 * opaque handle (typeconv.cc cb_is_cf_record_ptr: an incomplete record ->
 * convert_cf_ptr unwraps it), but the ObjC side truncated it. Measured: native
 * context 0x7f9bcb825e00 reached CGLSetParameter as 0xcb825e00 and OpenGL
 * faulted reading ctx+0x1f38.
 *
 * Fix (objc_shim.c enc_is_handle_structptr): a `^{Name=body}` RETURN joins the
 * existing kind-10 CONDITIONAL wrap (>4GB only, exactly like abigen's
 * `.cfretlow`), and the same encoding as an ARG joins the `^v` unwrap, so the
 * value round-trips. A genuine low-4GB struct pointer is untouched by either.
 *
 * Reproduced on the pure marshalling fact, Foundation only — no OpenGL:
 * `-[NSAppleEventDescriptor aeDesc]` returns `r^{AEDesc=I^^{
 * OpaqueAEDataStorageType}}`, a pointer INTERIOR to the native descriptor
 * object (so always >4GB), and `-[NSAppleEventDescriptor initWithAEDescNoCopy:]`
 * takes the same type back. A non-truncating return round-trips to a descriptor
 * whose type is still 'null'; a truncated one makes Foundation read a wild low
 * address -> a different type, or a fault. Same shape as CGLContextObj, with no
 * window server and no GL context.
 *
 * Sent through objc_msgSend by hand so the bridge classifies THE METHOD UNDER
 * TEST: routing via -performSelector: would classify performSelector's own `@`
 * return instead and the guard would be inert.
 *
 * KILL SWITCH  M64_NO_OBJC_STRUCTPTR_RET=1 -> raw truncation on both sides,
 *   i.e. reproduce the bug (the A/B guard's OFF arm).
 */
#define OBJC_OLD_DISPATCH_PROTOTYPES 1
#include <Foundation/Foundation.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   int failures = 0;

   id AED = (id)objc_getClass("NSAppleEventDescriptor");
   id d   = ((id (*)(id, SEL))objc_msgSend)(AED, sel_registerName("nullDescriptor"));

   /* `r^{AEDesc=I^^{OpaqueAEDataStorageType}}` RETURN: interior to the native
    * descriptor, hence >4GB, hence truncated pre-fix. */
   void *p = (void *)((id (*)(id, SEL))objc_msgSend)(d, sel_registerName("aeDesc"));
   printf("aeDesc nonnull=%d\n", p != NULL);

   /* Hand the same encoding straight back as an ARG. Post-fix the handle
    * unwraps to the real AEDesc and the re-wrapped descriptor is still 'null'
    * (0x6e756c6c); pre-fix Foundation reads the truncated low address. */
   id raw = ((id (*)(id, SEL))objc_msgSend)(AED, sel_registerName("alloc"));
   id d2  = ((id (*)(id, SEL, void *))objc_msgSend)(
               raw, sel_registerName("initWithAEDescNoCopy:"), p);
   unsigned t = ((unsigned (*)(id, SEL))objc_msgSend)(
               d2, sel_registerName("descriptorType"));
   if (t == 0x6e756c6cU) {
      puts("ok structptr_return_roundtrip");
   } else {
      printf("FAIL structptr_return_roundtrip type=0x%08x\n", t);
      failures++;
   }

   (void)pool;                  /* no drain: d2 took ownership of d's AEDesc */
   exit(failures ? 1 : 42);
}
