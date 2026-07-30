// Post a synthetic mouse MOVE (no buttons) at CG global coords: mover <x> <y>
#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    CGPoint p = CGPointMake(atof(argv[1]), atof(argv[2]));
    CGEventRef mv = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, p, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, mv);
    CFRelease(mv);
    return 0;
}
