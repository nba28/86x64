#import <Cocoa/Cocoa.h>
#include <stdio.h>

static void shim_note(const char *s) { fprintf(stderr, "[shimauto:%s] %s\n", "WebKit", s); }

long objc_exception_extract(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_extract called (auto-stub)"); return 0; }

long objc_exception_match(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_match called (auto-stub)"); return 0; }

long objc_exception_try_enter(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_try_enter called (auto-stub)"); return 0; }

long objc_exception_try_exit(long a, long b, long c_, long d, long e, long f) { shim_note("objc_exception_try_exit called (auto-stub)"); return 0; }

long objc_msgSend_fpret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_fpret called (auto-stub)"); return 0; }

long objc_msgSend_stret(long a, long b, long c_, long d, long e, long f) { shim_note("objc_msgSend_stret called (auto-stub)"); return 0; }
