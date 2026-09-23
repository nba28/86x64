/* tablecolwidth.m — what width does an NSTableColumn ACTUALLY receive?
 *
 * DYLD_INSERT_LIBRARIES probe (native x86_64): swizzles the NSTableColumn
 * width setters and logs the CGFloat each one is handed, with the column's
 * identifier. A translated i386 caller pushes a CGFloat as ONE 4-byte float;
 * if the bridge reads the native 'd' encoding instead, the method sees that
 * float fused with the next 4 bytes — a denormal ~0 or a huge value.
 *
 * Build:
 *   clang -arch x86_64 -dynamiclib -framework AppKit -framework Foundation \
 *       -o /tmp/tablecolwidth.dylib tablecolwidth.m
 * Use:
 *   DYLD_INSERT_LIBRARIES=/tmp/tablecolwidth.dylib App.app/Contents/MacOS/App
 */
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdio.h>

static void (*o_setMin)(id, SEL, CGFloat);
static void (*o_setMax)(id, SEL, CGFloat);
static void (*o_setW)(id, SEL, CGFloat);

static void log_w(id self, const char *what, CGFloat v)
{
   fprintf(stderr, "[colw] %-10s %-14s %.17g%s\n", what,
           [[self identifier] isKindOfClass:[NSString class]]
              ? [[self identifier] UTF8String] : "?",
           (double)v, (v != 0.0 && (v < 1e-6 || v > 1e6)) ? "   <== SUSPECT" : "");
   fflush(stderr);
}

static void n_setMin(id s, SEL c, CGFloat v) { log_w(s, "setMinWidth", v); o_setMin(s, c, v); }
static void n_setMax(id s, SEL c, CGFloat v) { log_w(s, "setMaxWidth", v); o_setMax(s, c, v); }
static void n_setW  (id s, SEL c, CGFloat v) { log_w(s, "setWidth",    v); o_setW(s, c, v); }

static void hook(Class k, SEL sel, IMP repl, void *save)
{
   Method m = class_getInstanceMethod(k, sel);
   if (!m) { fprintf(stderr, "[colw] no method %s\n", sel_getName(sel)); return; }
   *(IMP *)save = method_getImplementation(m);
   method_setImplementation(m, repl);
}

__attribute__((constructor))
static void arm(void)
{
   Class k = objc_getClass("NSTableColumn");
   if (!k) { fprintf(stderr, "[colw] no NSTableColumn\n"); return; }
   hook(k, @selector(setMinWidth:), (IMP)n_setMin, &o_setMin);
   hook(k, @selector(setMaxWidth:), (IMP)n_setMax, &o_setMax);
   hook(k, @selector(setWidth:),    (IMP)n_setW,   &o_setW);
   fprintf(stderr, "[colw] armed\n");
   fflush(stderr);
}
