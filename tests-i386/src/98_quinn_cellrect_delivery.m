/* 98_quinn_cellrect_delivery — Quinn's rotate-redraw DELIVERY chain, i386->
 * x86_64, checking VALUE integrity of every marshalling hop (the "rotating a
 * T/L piece leaves exactly ONE stale cube + ONE missing cube" defect).
 *
 * Disassembly of Quinn 3.5.7 (-[QuinnPlayerController keyDown:] 0x43f61,
 * -[QuinnPlayerController quinnGameDidChangeBoard:] 0x44ada,
 * -[QuinnBoardView setNeedsDisplayInCellRect:] 0x78ee9) shows the board
 * repaint rect for a piece rotate/move is delivered as an 8-byte NESTED
 * ANONYMOUS short-struct  IGRect = {?={?=ss}{?=ss}}  through these hops:
 *
 *   1. rect = [[game dirtyRegion] unionRect]     {?={?=ss}{?=ss}} RETURN in
 *      EAX:EDX via plain legacy msgSend (guarded by 59, re-pinned here);
 *   2a. DIRECT [boardView setNeedsDisplayInCellRect:rect] — the {ssss}
 *      8-byte struct BY-VALUE ARG through legacy msgSend (keyDown's rotate
 *      shadow update; NOT previously guarded — 59 covers only a 4-byte {ss}
 *      arg);
 *   2b. QUEUED delivery (quinnGameDidChangeBoard:, keyDown drop): an
 *      NSInvocation built from instanceMethodSignatureForSelector: with
 *      setArgument:&rect atIndex:2, queued ([taskQueue queueTask:inv]) and
 *      INVOKED LATER after the builder's i386 frame is DEAD — 71 records a
 *      KNOWN value gap for float-structs through -invoke into a legacy IMP;
 *      this pins the INTEGER {ssss} case Quinn's board repaint rides on;
 *   3. inside setNeedsDisplayInCellRect: the {ssss} arg is forwarded to
 *      [self viewRectFromMatrixRect:r] — {?={?=ff}{?=ff}} FLOAT-RECT return
 *      (i386 STRET, native TWO-SSE-EIGHTBYTE register return) + the same
 *      {ssss} by-value arg;
 *   4. the ANIMATED rotate reads center = [game pieceRotationCenter]
 *      ({?=ff} return: T/L/J centers are (2.5,2.5) from kRotationCenters)
 *      and passes it BY VALUE to animateRotatePiece:inDirection:
 *      rotationCenter: ({?=ff} arg after an object + an int);
 *   5. setNeedsDisplayInCellRegion: enumerates [region rectCount] x
 *      [region rectAtIndex:i] ({ssss} returns) — replayed with a 3-rect
 *      T-shaped (non-rectangular) region.
 *
 * Values are printed on every hop; any corrupted half (e.g. the w/h pair
 * arriving <=0 -> AppKit invalidates NOTHING -> the un-repainted rotation
 * delta of a T piece is exactly 1 stale + 1 missing cube) diffs RED against
 * expected/. T-rotate uses union bbox {1,1,3,3} (odd w/h), I-rod {0,0,4,4}
 * (even) to catch value-dependent corruption.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>

typedef struct { short x, y; } IGPair;            /* {?=ss}            */
typedef struct { IGPair origin, size; } IGRect4s; /* {?={?=ss}{?=ss}}  */
typedef struct { float x, y; } FPointf;           /* {?=ff}            */
typedef struct { FPointf origin, size; } FRect4f; /* {?={?=ff}{?=ff}}  */

static int failures = 0;

static void check_rect(const char *what, IGRect4s got, short x, short y,
                       short w, short h) {
   if (got.origin.x == x && got.origin.y == y &&
       got.size.x == w && got.size.y == h) {
      printf("ok %s {%d,%d,%d,%d}\n", what, got.origin.x, got.origin.y,
             got.size.x, got.size.y);
   } else {
      printf("FAIL %s got {%d,%d,%d,%d} want {%d,%d,%d,%d}\n", what,
             got.origin.x, got.origin.y, got.size.x, got.size.y, x, y, w, h);
      failures++;
   }
}

/* ---- the board/region analogs (legacy ObjC1 classes) -------------------- */

@interface TRegion : NSObject {
   IGRect4s _rects[4];
   unsigned _count;
}
- (id)initWithRects:(const IGRect4s *)r count:(unsigned)n;
- (unsigned)rectCount;
- (IGRect4s)rectAtIndex:(unsigned)i;
- (IGRect4s)unionRect;
@end
@implementation TRegion
- (id)initWithRects:(const IGRect4s *)r count:(unsigned)n {
   self = [super init];
   if (self) { _count = n; memcpy(_rects, r, n * sizeof *r); }
   return self;
}
- (unsigned)rectCount { return _count; }
- (IGRect4s)rectAtIndex:(unsigned)i { return _rects[i]; }
- (IGRect4s)unionRect {
   IGRect4s u = _rects[0];
   unsigned i;
   for (i = 1; i < _count; i++) {
      short x0 = u.origin.x < _rects[i].origin.x ? u.origin.x : _rects[i].origin.x;
      short y0 = u.origin.y < _rects[i].origin.y ? u.origin.y : _rects[i].origin.y;
      short x1a = (short)(u.origin.x + u.size.x), x1b = (short)(_rects[i].origin.x + _rects[i].size.x);
      short y1a = (short)(u.origin.y + u.size.y), y1b = (short)(_rects[i].origin.y + _rects[i].size.y);
      short x1 = x1a > x1b ? x1a : x1b, y1 = y1a > y1b ? y1a : y1b;
      u.origin.x = x0; u.origin.y = y0;
      u.size.x = (short)(x1 - x0); u.size.y = (short)(y1 - y0);
   }
   return u;
}
@end

@interface TBoard : NSObject {
   IGRect4s _got;         /* last rect received by setNeedsDisplayInCellRect: */
   unsigned _calls;
   FPointf  _gotCenter;
   int      _gotDir;
   id       _gotPiece;
}
- (void)setNeedsDisplayInCellRect:(IGRect4s)r;
- (FRect4f)viewRectFromMatrixRect:(IGRect4s)r;    /* cellSize=20, origin 0 */
- (FPointf)pieceRotationCenter;
- (void)animateRotatePiece:(id)p inDirection:(int)d rotationCenter:(FPointf)c;
- (IGRect4s)gotRect;
- (unsigned)gotCalls;
- (FPointf)gotCenter;
- (int)gotDir;
- (id)gotPiece;
@end
@implementation TBoard
- (void)setNeedsDisplayInCellRect:(IGRect4s)r { _got = r; _calls++; }
- (FRect4f)viewRectFromMatrixRect:(IGRect4s)r {
   FRect4f v;
   v.origin.x = (float)r.origin.x * 20.0f;
   v.origin.y = (float)r.origin.y * 20.0f;
   v.size.x   = (float)r.size.x   * 20.0f;
   v.size.y   = (float)r.size.y   * 20.0f;
   return v;
}
- (FPointf)pieceRotationCenter { FPointf p; p.x = 2.5f; p.y = 2.5f; return p; }
- (void)animateRotatePiece:(id)p inDirection:(int)d rotationCenter:(FPointf)c {
   _gotPiece = p; _gotDir = d; _gotCenter = c;
}
- (IGRect4s)gotRect { return _got; }
- (unsigned)gotCalls { return _calls; }
- (FPointf)gotCenter { return _gotCenter; }
- (int)gotDir { return _gotDir; }
- (id)gotPiece { return _gotPiece; }
@end

/* Build the invocation EXACTLY like -[QuinnPlayerController
 * quinnGameDidChangeBoard:] does, in a frame that DIES before the invoke
 * (Quinn queues the invocation; the rect lives in a stack local). */
static NSInvocation *build_inv(TBoard *b, IGRect4s r) {
   NSMethodSignature *sig = [[b class] instanceMethodSignatureForSelector:
                                @selector(setNeedsDisplayInCellRect:)];
   NSInvocation *inv = [NSInvocation invocationWithMethodSignature:sig];
   [inv setSelector:@selector(setNeedsDisplayInCellRect:)];
   [inv setTarget:b];
   [inv setArgument:&r atIndex:2];
   return inv;
}

/* Clobber the dead frame region so a lazily-referenced i386 buffer reads
 * garbage rather than the accidentally-still-intact value. */
static void scribble(void) {
   volatile char junk[512];
   unsigned i;
   for (i = 0; i < sizeof junk; i++) junk[i] = (char)0x5A;
}

int main(void) {
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

   TBoard *board = [[TBoard alloc] init];

   /* T-rotate union bbox (piece 5, rot0 (1,1,3,2) U rot1 (2,1,2,3)) and the
    * I-rod union bbox (piece 4, (0,2,4,1) U (2,0,1,4)) from kPieceRects. */
   IGRect4s rectT = {{1, 1}, {3, 3}};
   IGRect4s rectI = {{0, 0}, {4, 4}};

   /* -- hop 1: {?={?=ss}{?=ss}} RETURN through legacy msgSend ------------- */
   IGRect4s two[2] = {{{1, 1}, {3, 2}}, {{2, 1}, {2, 3}}};
   TRegion *regionT = [[TRegion alloc] initWithRects:two count:2];
   IGRect4s u = [regionT unionRect];
   check_rect("union_return", u, 1, 1, 3, 3);

   /* -- hop 2a: {ssss} BY-VALUE ARG through DIRECT legacy msgSend --------- */
   [board setNeedsDisplayInCellRect:u];
   check_rect("direct_arg_T", [board gotRect], 1, 1, 3, 3);
   [board setNeedsDisplayInCellRect:rectI];
   check_rect("direct_arg_I", [board gotRect], 0, 0, 4, 4);

   /* -- hop 2b: {ssss} arg through DEFERRED NSInvocation invoke ----------- */
   NSInvocation *invT = build_inv(board, rectT);
   NSInvocation *invI = build_inv(board, rectI);
   scribble();
   [invT invoke];
   check_rect("invocation_arg_T", [board gotRect], 1, 1, 3, 3);
   scribble();
   [invI invoke];
   check_rect("invocation_arg_I", [board gotRect], 0, 0, 4, 4);
   if ([board gotCalls] == 4) {
      puts("ok call_count 4");
   } else {
      printf("FAIL call_count got %u want 4\n", [board gotCalls]);
      failures++;
   }

   /* -- hop 3: {?={?=ff}{?=ff}} stret return + {ssss} arg (view convert) -- */
   FRect4f v = [board viewRectFromMatrixRect:rectT];
   if (v.origin.x == 20.0f && v.origin.y == 20.0f &&
       v.size.x == 60.0f && v.size.y == 60.0f) {
      printf("ok view_rect {%.0f,%.0f,%.0f,%.0f}\n", v.origin.x, v.origin.y,
             v.size.x, v.size.y);
   } else {
      printf("FAIL view_rect got {%.1f,%.1f,%.1f,%.1f} want {20,20,60,60}\n",
             v.origin.x, v.origin.y, v.size.x, v.size.y);
      failures++;
   }

   /* -- hop 4: {?=ff} return + {?=ff} by-value arg (rotation center) ------ */
   FPointf c = [board pieceRotationCenter];
   if (c.x == 2.5f && c.y == 2.5f) {
      printf("ok center_return {%.1f,%.1f}\n", c.x, c.y);
   } else {
      printf("FAIL center_return got {%f,%f} want {2.5,2.5}\n", c.x, c.y);
      failures++;
   }
   [board animateRotatePiece:board inDirection:8 rotationCenter:c];
   {
      FPointf g = [board gotCenter];
      if ([board gotDir] == 8 && [board gotPiece] == board &&
          g.x == 2.5f && g.y == 2.5f) {
         printf("ok center_arg dir=8 {%.1f,%.1f}\n", g.x, g.y);
      } else {
         printf("FAIL center_arg dir=%d piece=%s {%f,%f} want dir=8 self {2.5,2.5}\n",
                [board gotDir], [board gotPiece] == board ? "self" : "OTHER",
                g.x, g.y);
         failures++;
      }
   }

   /* -- hop 5: region ENUMERATION for a non-rectangular T cell set -------- */
   /* T-up at (1,1): row {1..3,y=1} + stem {2,2}; plus the incoming-state
    * stem {2,0} — a 3-rect non-rectangular region like old-cells U new-cells. */
   IGRect4s tcells[3] = {{{1, 1}, {3, 1}}, {{2, 2}, {1, 1}}, {{2, 0}, {1, 1}}};
   TRegion *tregion = [[TRegion alloc] initWithRects:tcells count:3];
   {
      unsigned n = [tregion rectCount];
      unsigned i;
      printf("region rectCount %u\n", n);
      for (i = 0; i < n; i++) {
         IGRect4s r = [tregion rectAtIndex:i];
         FRect4f  vr = [board viewRectFromMatrixRect:r];
         printf("rect[%u] {%d,%d,%d,%d} view {%.0f,%.0f,%.0f,%.0f}\n", i,
                r.origin.x, r.origin.y, r.size.x, r.size.y,
                vr.origin.x, vr.origin.y, vr.size.x, vr.size.y);
      }
   }

   [regionT release];
   [tregion release];
   [board release];
   [pool drain];
   exit(failures);
}
