// List every window in the WindowServer, optionally filtered by owner-name substring.
#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

int main(int argc, char **argv) {
    @autoreleasepool {
        NSString *want = argc > 1 ? [NSString stringWithUTF8String:argv[1]] : nil;
        CFArrayRef wl = CGWindowListCopyWindowInfo(kCGWindowListOptionAll, kCGNullWindowID);
        long n = 0;
        for (NSDictionary *w in (NSArray *)wl) {
            NSString *owner = w[(id)kCGWindowOwnerName] ?: @"";
            if (want && [owner rangeOfString:want options:NSCaseInsensitiveSearch].location == NSNotFound)
                continue;
            NSDictionary *b = w[(id)kCGWindowBounds];
            printf("pid=%s owner='%s' name='%s' layer=%s onscreen=%s alpha=%s bounds=(%s,%s %sx%s)\n",
                   [[w[(id)kCGWindowOwnerPID] description] UTF8String],
                   [owner UTF8String],
                   [[(w[(id)kCGWindowName] ?: @"(nil)") description] UTF8String],
                   [[w[(id)kCGWindowLayer] description] UTF8String],
                   [[(w[(id)kCGWindowIsOnscreen] ?: @"?") description] UTF8String],
                   [[w[(id)kCGWindowAlpha] description] UTF8String],
                   [[b[@"X"] description] UTF8String], [[b[@"Y"] description] UTF8String],
                   [[b[@"Width"] description] UTF8String], [[b[@"Height"] description] UTF8String]);
            n++;
        }
        printf("total matching windows: %ld\n", n);
    }
    return 0;
}
