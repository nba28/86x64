#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "InstantMessage", s); }

long IMAVManagerStateChangedNotification(long a, long b, long c_, long d, long e, long f) { shim_note("IMAVManagerStateChangedNotification called (auto-stub)"); return 0; }
