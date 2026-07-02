/* 59_objc_short_pair_struct.m — tiny {short,short} by-value struct through
 * LEGACY class dispatch (the QuinnMatrix piece-size corruption).
 *
 * Quinn's IGSize is an anonymous pair of shorts, encoding {?=ss} (4 bytes).
 * +[QuinnMatrix matrixWithContentNoCopy:size:freeWhenDone:] (@20@0:4*8{?=ss}12c16)
 * receives it by value, forwards it through -initWithContentNoCopy:size:
 * freeWhenDone: to -initWithMatrixSize: (@12@0:4{?=ss}8) which stores it. Every
 * hop is a legacy-class msgSend round trip (forward bridge marshals the i386
 * frame to SysV, the reverse-IMP trampoline rebuilds the i386 frame). If the
 * 4-byte aggregate is mis-marshalled on any hop, the stored size is garbage:
 * Quinn's -[QuinnMatrix usedRect] then scans w*h = millions of cells off the
 * end of a 16-byte piece shape -> blank board + non-deterministic SIGBUS
 * (crashlog Quinn-2026-06-30-024447: w=8156 instead of 4).
 *
 * This mirrors the exact shapes: the 3-arg class-method entry, the init chain,
 * a {?=ss} return (-size), and the 8-byte {?={?=ss}{?=ss}} return (-usedRect).
 */
#include <Foundation/Foundation.h>
#include <stdio.h>

typedef struct { short w, h; } TSize;              /* {?=ss}   4 bytes */
typedef struct { TSize origin, size; } TRect;      /* {?={?=ss}{?=ss}} 8 bytes */

@interface TMatrix : NSObject {
   TSize      _size;
   const char *_content;
   char       _own;
}
+ (id)matrixWithContentNoCopy:(const char *)c size:(TSize)s freeWhenDone:(BOOL)f;
- (id)initWithContentNoCopy:(const char *)c size:(TSize)s freeWhenDone:(BOOL)f;
- (id)initWithMatrixSize:(TSize)s;
- (TSize)size;
- (TRect)usedRect;
@end

@implementation TMatrix
+ (id)matrixWithContentNoCopy:(const char *)c size:(TSize)s freeWhenDone:(BOOL)f {
   return [[[self alloc] initWithContentNoCopy:c size:s freeWhenDone:f] autorelease];
}
- (id)initWithContentNoCopy:(const char *)c size:(TSize)s freeWhenDone:(BOOL)f {
   self = [self initWithMatrixSize:s];
   if (self) { _content = c; _own = f; }
   return self;
}
- (id)initWithMatrixSize:(TSize)s {
   self = [super init];
   if (self) { _size = s; }
   return self;
}
- (TSize)size { return _size; }
- (TRect)usedRect {
   /* derived from the stored size, like Quinn computing the used cell rect */
   TRect r; r.origin.w = 1; r.origin.h = 2; r.size = _size;
   return r;
}
@end

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   int failures = 0;
   static const char shape[16] = { 0,1,1,0, 0,1,1,0, 0,0,0,0, 0,0,0,0 };

   TSize four = { 4, 4 };
   TMatrix *m = [TMatrix matrixWithContentNoCopy:shape size:four freeWhenDone:NO];

   TSize got = [m size];
   if (got.w == 4) puts("ok size_w"); else { printf("FAIL size_w %d\n", got.w); failures++; }
   if (got.h == 4) puts("ok size_h"); else { printf("FAIL size_h %d\n", got.h); failures++; }

   TRect r = [m usedRect];
   if (r.origin.w == 1 && r.origin.h == 2 && r.size.w == 4 && r.size.h == 4)
      puts("ok rect");
   else { printf("FAIL rect {{%d,%d},{%d,%d}}\n", r.origin.w, r.origin.h,
                 r.size.w, r.size.h); failures++; }

   /* a second, asymmetric size through the direct init (skips the class hop) */
   TSize wide = { 10, 20 };
   TMatrix *b = [[TMatrix alloc] initWithMatrixSize:wide];
   TSize gb = [b size];
   if (gb.w == 10 && gb.h == 20) puts("ok direct");
   else { printf("FAIL direct {%d,%d}\n", gb.w, gb.h); failures++; }
   [b release];

   [pool drain];
   exit(failures);
}
