#import <AppKit/AppKit.h>
int main(int argc, char **argv) {
    @autoreleasepool {
        if (argc < 2) { printf("usage: activate <pid>\n"); return 2; }
        pid_t p = atoi(argv[1]);
        NSRunningApplication *a = [NSRunningApplication runningApplicationWithProcessIdentifier:p];
        if (!a) { printf("no NSRunningApplication for pid %d\n", p); return 1; }
        printf("before: active=%d policy=%ld\n", (int)[a isActive], (long)[a activationPolicy]);
        BOOL ok = [a activateWithOptions:NSApplicationActivateAllWindows |
                                         NSApplicationActivateIgnoringOtherApps];
        usleep(900000);
        printf("activate=%d active=%d policy=%ld\n", (int)ok, (int)[a isActive], (long)[a activationPolicy]);
        printf("frontmost=%s\n",
               [[[NSWorkspace sharedWorkspace] frontmostApplication].localizedName UTF8String]);
    }
    return 0;
}
