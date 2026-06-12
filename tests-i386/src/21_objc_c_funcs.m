/* 21_objc_c_funcs.m — abigen ObjC-object C-function params (23rd blocker).
 * NSClassFromString previously had NO shim (objc arg unsupported) so the
 * i386 caller bound directly to native Foundation, whose 64-bit `ret`
 * popped 8 bytes off a 4-byte-slot i386 frame -> wild rip. */
#include <Foundation/Foundation.h>
#include <stdio.h>

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

   Class c = NSClassFromString(@"NSString");
   puts(c != Nil ? "NSClassFromString ok" : "NSClassFromString FAILED");

   NSString *name = NSStringFromClass(c);
   printf("NSStringFromClass: %s\n", [name UTF8String]);

   SEL sel = NSSelectorFromString(@"length");
   NSString *selname = NSStringFromSelector(sel);
   printf("NSStringFromSelector: %s\n", [selname UTF8String]);

   Class missing = NSClassFromString(@"TotallyNotARealClass");
   puts(missing == Nil ? "missing class is Nil" : "missing class NOT nil");

   [pool drain];
   exit(0);
}
