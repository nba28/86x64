/* keylayout.m — which characters does a translated app ACTUALLY get per key?
 *
 * DYLD_INSERT_LIBRARIES probe (native x86_64): swizzles -[NSEvent characters]
 * and -[NSEvent charactersIgnoringModifiers] and logs keyCode -> result plus
 * the process's current keyboard input source, to tell "the app reads a US
 * layout" apart from "the process is ON a US layout".
 *
 * Build:
 *   clang -arch x86_64 -dynamiclib -framework AppKit -framework Carbon \
 *       -o /tmp/keylayout.dylib keylayout.m
 * Use:
 *   DYLD_INSERT_LIBRARIES=/tmp/keylayout.dylib App.app/Contents/MacOS/App
 */
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>
#import <objc/runtime.h>
#include <stdio.h>

static id (*o_chars)(id, SEL);
static id (*o_charsIM)(id, SEL);

static void log_k(NSEvent *e, const char *what, NSString *s)
{
   TISInputSourceRef src = TISCopyCurrentKeyboardInputSource();
   NSString *sid = src ? (__bridge NSString *)TISGetInputSourceProperty(src, kTISPropertyInputSourceID) : @"?";
   fprintf(stderr, "[keylayout] %-28s keyCode=%3u -> \"%s\" src=%s\n", what,
           (unsigned)[e keyCode], [s UTF8String] ?: "(nil)", [sid UTF8String]);
   fflush(stderr);
   if (src) CFRelease(src);
}

static id n_chars(id s, SEL c)   { id r = o_chars(s, c);   log_k(s, "characters", r); return r; }
static id n_charsIM(id s, SEL c) { id r = o_charsIM(s, c); log_k(s, "charactersIgnoringModifiers", r); return r; }

__attribute__((constructor)) static void init(void)
{
   Method m = class_getInstanceMethod([NSEvent class], @selector(characters));
   o_chars = (void *)method_setImplementation(m, (IMP)n_chars);
   m = class_getInstanceMethod([NSEvent class], @selector(charactersIgnoringModifiers));
   o_charsIM = (void *)method_setImplementation(m, (IMP)n_charsIM);
   TISInputSourceRef src = TISCopyCurrentKeyboardInputSource();
   fprintf(stderr, "[keylayout] armed, src at load=%s\n",
           src ? [(__bridge NSString *)TISGetInputSourceProperty(src, kTISPropertyInputSourceID) UTF8String] : "?");
   if (src) CFRelease(src);
}
