/* 99_invocation_cgfloat_widen.m — NSInvocation into a LEGACY method with an
 * NSRect argument and a CGFloat return.
 *
 * The signature comes from the legacy class, so it keeps the i386 encoding
 * ({_NSRect={_NSPoint=ff}{_NSSize=ff}}, 'f'). Native -invoke laid the rect out
 * as 4 packed floats while the reverse bridge reads CGFloats as doubles, so
 * the method saw garbage. Quinn's reflection queues exactly this invocation
 * (sourceViewDidChange:inRect:) and repainted {0,0,0,0} forever.
 *
 * Exit 42 = the method received the rect and the return came back.
 * ABICONV_NO_INVOKE_WIDEN=1 turns the fix off (expected: exit 1).
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

@interface Target : NSObject
- (float)area:(id)tag inRect:(NSRect)r;
@end
@implementation Target
- (float)area:(id)tag inRect:(NSRect)r
{
   printf("got {%g,%g,%g,%g}\n", r.origin.x, r.origin.y, r.size.width, r.size.height);
   return r.size.width * r.size.height;
}
@end

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   /* NSClassFromString, not [Target ...]: modern clang emits no fragile-ABI
    * .objc_class_name_Target for a same-file class ref, so it would not link. */
   Class tc = NSClassFromString(@"Target");
   Target *t = [[tc alloc] init];
   SEL s = @selector(area:inRect:);
   NSInvocation *inv = [NSInvocation invocationWithMethodSignature:
                          [tc instanceMethodSignatureForSelector:s]];
   NSRect r = NSMakeRect(1, 2, 30, 40);
   id tag = @"tag";
   [inv setSelector:s];
   [inv setTarget:t];
   [inv setArgument:&tag atIndex:2];
   [inv setArgument:&r atIndex:3];
   [inv invoke];
   float a = 0;
   [inv getReturnValue:&a];
   printf("area %g\n", a);
   [pool release];
   exit(a == 1200.0f ? 42 : 1);
}
