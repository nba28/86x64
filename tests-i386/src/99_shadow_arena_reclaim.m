/* 99_shadow_arena_reclaim.m — i386 shadows of released legacy instances must be reused.
 *
 * Every instance of an app-defined (legacy, reverse-registered) class gets a
 * low-4GB i386-layout shadow from a 64 MB bump arena. It was never freed, so
 * after ~58k instances over the process lifetime the arena ran out and the
 * reverse bridge called instance methods with self = nil: Quinn crashed after
 * 40 minutes of network play in -[AsyncWritePacket initWithData:timeout:tag:]
 * ([super init] on nil -> nil -> store to address 4).
 *
 * Creates and releases 150k instances; each must init non-nil and keep its ivar.
 * Off arm: ABICONV_NO_SHADOW_RECLAIM=1 must fail. Exit 42 = all good.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

@interface Packet : NSObject { int tag; int pad[6]; }
- (id)initWithTag:(int)t;
- (int)tag;
@end
@implementation Packet
- (id)initWithTag:(int)t { self = [super init]; if (self) { tag = t; } return self; }
- (int)tag { return tag; }
@end

int main(void)
{
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
   Class cls = NSClassFromString(@"Packet");
   int bad = 0, i;
   for (i = 0; i < 150000; i++) {
      id p = [[cls alloc] initWithTag:i];
      if (!p || [p tag] != i) { bad = i + 1; break; }
      [p release];
   }
   printf("created=%d first_bad=%d\n", i, bad);
   [pool release];
   exit(bad ? 1 : 42);
}
