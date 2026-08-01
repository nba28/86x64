// axdump <pid> — enumerate a process's AX windows and their controls with
// CG-global centres, so a synthetic click can target a named button instead of
// a guessed coordinate.
#import <Foundation/Foundation.h>
#import <ApplicationServices/ApplicationServices.h>

static NSString *S(AXUIElementRef e, CFStringRef attr) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(e, attr, &v) != kAXErrorSuccess || !v) return nil;
    NSString *s = [(__bridge id)v description];
    CFRelease(v);
    return s;
}

static void walk(AXUIElementRef e, int depth, int maxdepth) {
    if (depth > maxdepth) return;
    NSString *role = S(e, kAXRoleAttribute);
    NSString *title = S(e, kAXTitleAttribute);
    NSString *desc  = S(e, kAXDescriptionAttribute);
    NSString *val   = S(e, kAXValueAttribute);

    CGPoint pos = {0, 0}; CGSize sz = {0, 0};
    CFTypeRef pv = NULL, sv = NULL;
    if (AXUIElementCopyAttributeValue(e, kAXPositionAttribute, &pv) == kAXErrorSuccess && pv) {
        AXValueGetValue((AXValueRef)pv, kAXValueCGPointType, &pos); CFRelease(pv);
    }
    if (AXUIElementCopyAttributeValue(e, kAXSizeAttribute, &sv) == kAXErrorSuccess && sv) {
        AXValueGetValue((AXValueRef)sv, kAXValueCGSizeType, &sz); CFRelease(sv);
    }
    printf("%*s%-22s title='%s' desc='%s' val='%s' centre=(%.0f,%.0f) size=%.0fx%.0f\n",
           depth * 2, "", role ? [role UTF8String] : "?",
           title ? [title UTF8String] : "", desc ? [desc UTF8String] : "",
           val ? [val UTF8String] : "",
           pos.x + sz.width / 2, pos.y + sz.height / 2, sz.width, sz.height);

    CFTypeRef kids = NULL;
    if (AXUIElementCopyAttributeValue(e, kAXChildrenAttribute, &kids) == kAXErrorSuccess && kids) {
        for (id k in (__bridge NSArray *)kids) walk((AXUIElementRef)k, depth + 1, maxdepth);
        CFRelease(kids);
    }
}

int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc < 2) { fprintf(stderr, "usage: axdump <pid> [maxdepth]\n"); return 2; }
        pid_t pid = atoi(argv[1]);
        int maxdepth = argc > 2 ? atoi(argv[2]) : 4;
        printf("trusted=%d pid=%d\n", AXIsProcessTrusted(), pid);
        AXUIElementRef app = AXUIElementCreateApplication(pid);
        CFTypeRef wins = NULL;
        if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, &wins) != kAXErrorSuccess || !wins) {
            printf("(no AX windows)\n"); return 1;
        }
        for (id w in (__bridge NSArray *)wins) { walk((AXUIElementRef)w, 0, maxdepth); printf("--\n"); }
        CFRelease(wins);
    }
    return 0;
}
