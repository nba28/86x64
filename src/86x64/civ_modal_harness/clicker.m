// Post a synthetic left click at CG global coords: clicker <x> <y>
// Session tap, not HID: the guard that reliably drives a classic dialog
// (tests-i386/carbon_classic_alert_test.sh) posts at kCGSessionEventTap, and
// HID-tap posts were silently dropped in the runs that reported zero key events.
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: clicker x y\n"); return 2; }
    CGFloat x = atof(argv[1]), y = atof(argv[2]);
    printf("trusted=%d click at (%g,%g)\n", AXIsProcessTrusted(), x, y);
    CGPoint p = CGPointMake(x, y);
    CGEventRef mv = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, p, kCGMouseButtonLeft);
    CGEventPost(kCGSessionEventTap, mv); CFRelease(mv);
    usleep(150000);
    CGEventRef dn = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, p, kCGMouseButtonLeft);
    CGEventPost(kCGSessionEventTap, dn); CFRelease(dn);
    usleep(80000);
    CGEventRef up = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseUp, p, kCGMouseButtonLeft);
    CGEventPost(kCGSessionEventTap, up); CFRelease(up);
    return 0;
}
