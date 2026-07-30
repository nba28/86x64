// Post a synthetic left click at CG global coords: clicker <x> <y>
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: clicker x y\n"); return 2; }
    CGFloat x = atof(argv[1]), y = atof(argv[2]);
    printf("trusted=%d click at (%g,%g)\n", AXIsProcessTrusted(), x, y);
    CGPoint p = CGPointMake(x, y);
    CGEventRef mv = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, p, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, mv); CFRelease(mv);
    usleep(150000);
    CGEventRef dn = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, p, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, dn); CFRelease(dn);
    usleep(80000);
    CGEventRef up = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, p, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, up); CFRelease(up);
    return 0;
}
