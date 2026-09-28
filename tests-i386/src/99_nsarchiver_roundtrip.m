/* 99_nsarchiver_roundtrip.m — the CLASSIC NSArchiver round trip.
 *
 * Pre-NSKeyedArchiver apps persist their state with +[NSArchiver
 * archivedDataWithRootObject:] and read it back with +[NSUnarchiver
 * unarchiveObjectWithData:]. That format is TYPE-ENCODING driven: the stream
 * carries @encode() strings, and several of those differ between i386 and
 * x86_64 (CGFloat 'f' vs 'd', long/NSInteger 4 vs 8 bytes), so a translated
 * app's archive is written and read through NATIVE Foundation with LEGACY type
 * strings.
 *
 * MEASURED, Quinn (2026-09-23): a live probe caught
 * `NSUnarchiver unarchiveObjectWithData: len=67 -> NIL` — a round trip that
 * silently yields nil, which is exactly how an app loses persisted state.
 *
 * Exit 42 = every round trip returned an equal object.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

/* A class DEFINED BY THE TRANSLATED APP, the shape a real save uses: the
 * archive names the class and NSUnarchiver must instantiate it and run its
 * -initWithCoder: — for a legacy i386 class that is the reverse bridge. */
@interface QScore : NSObject <NSCoding>
{
   int      score;
   NSString *player;
}
- (id)initWithScore:(int)s player:(NSString *)p;
@end

@implementation QScore
- (id)initWithScore:(int)s player:(NSString *)p
{
   self = [super init];
   if (self) { score = s; player = [p retain]; }
   return self;
}
- (void)dealloc { [player release]; [super dealloc]; }
- (void)encodeWithCoder:(NSCoder *)c
{
   [c encodeValueOfObjCType:@encode(int) at:&score];
   [c encodeObject:player];
}
- (id)initWithCoder:(NSCoder *)c
{
   self = [super init];
   if (self) {
      [c decodeValueOfObjCType:@encode(int) at:&score];
      player = [[c decodeObject] retain];
   }
   return self;
}
- (BOOL)isEqual:(id)o
{
   if (![o isKindOfClass:[QScore class]]) { return NO; }
   QScore *q = o;
   return q->score == score && [q->player isEqual:player];
}
- (NSString *)description
{
   return [NSString stringWithFormat:@"QScore(%d,%@)", score, player];
}
@end

static int bad;

static void check(int ok, const char *what)
{
   printf("%s %s\n", ok ? "ok" : "FAIL", what);
   if (!ok) { bad++; }
}

static id roundtrip(id root, const char *what)
{
   NSData *d = [NSArchiver archivedDataWithRootObject:root];
   printf("   %s: archived %ld bytes\n", what, d ? (long)[d length] : -1L);
   if (!d || [d length] == 0) { check(0, what); return nil; }
   id back = [NSUnarchiver unarchiveObjectWithData:d];
   if (!back) { printf("   %s: unarchive -> nil\n", what); check(0, what); return nil; }
   check([back isEqual:root], what);
   return back;
}

int main(void)
{
   setbuf(stdout, NULL);
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

   roundtrip(@"hello", "string");
   roundtrip([NSNumber numberWithInt:12410], "number-int");
   roundtrip([NSNumber numberWithDouble:3.5], "number-double");
   roundtrip([NSArray arrayWithObjects:@"a", @"b", [NSNumber numberWithInt:7], nil], "array");
   roundtrip([NSDictionary dictionaryWithObjectsAndKeys:
                @"nba28", @"player", [NSNumber numberWithInt:12410], @"score", nil], "dictionary");
   roundtrip([NSDate dateWithTimeIntervalSince1970:1000000], "date");

   /* ★ the real shape: the app's OWN class, and an array of them. */
   QScore *q = [[[QScore alloc] initWithScore:12410 player:@"nba28"] autorelease];
   roundtrip(q, "custom-class");
   roundtrip([NSArray arrayWithObjects:q,
                [[[QScore alloc] initWithScore:3921 player:@"nba28"] autorelease], nil],
             "custom-array");

   printf("bad=%d\n", bad);
   [pool release];
   exit(bad ? 1 : 42);
}
