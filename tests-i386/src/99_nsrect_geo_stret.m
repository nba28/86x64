/* 99_nsrect_geo_stret.m — NSRect-returning geometry C functions.
 *
 * NSIntersectionRect / NSUnionRect / NSInsetRect return a 16-byte NSRect
 * through an i386 hidden pointer; libabiconv bridges them in the GEOSHIM_S
 * table. Quinn's QuinnReflectionView computes its dirty rect with
 * NSIntersectionRect and got {0,0,0,0} in game, so the reflection never
 * repainted.
 *
 * Exit 42 = every result matches the arithmetic.
 */
#include <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>

static int same(NSRect a, float x, float y, float w, float h)
{
   return a.origin.x == x && a.origin.y == y && a.size.width == w && a.size.height == h;
}

int main(void)
{
   NSRect a = NSMakeRect(0, 0, 208, 84), b = NSMakeRect(40, 10, 100, 200);
   NSRect i = NSIntersectionRect(a, b);
   NSRect u = NSUnionRect(a, b);
   NSRect n = NSInsetRect(a, 4, 2);
   printf("inter {%g,%g,%g,%g}\n", i.origin.x, i.origin.y, i.size.width, i.size.height);
   printf("union {%g,%g,%g,%g}\n", u.origin.x, u.origin.y, u.size.width, u.size.height);
   printf("inset {%g,%g,%g,%g}\n", n.origin.x, n.origin.y, n.size.width, n.size.height);
   int ok = same(i, 40, 10, 100, 74) && same(u, 0, 0, 208, 210) && same(n, 4, 2, 200, 80);
   exit(ok ? 42 : 1);
}
