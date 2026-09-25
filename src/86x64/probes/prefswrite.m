/* prefswrite.m — does the app's SAVE actually reach NSUserDefaults?
 *
 * DYLD_INSERT_LIBRARIES probe (native x86_64). Hooks the classic persistence
 * path an old Cocoa app uses:
 *     +[NSArchiver archivedDataWithRootObject:]   (the legacy, non-keyed archiver)
 *     +[NSUnarchiver unarchiveObjectWithData:]
 *     -[NSUserDefaults setObject:forKey:] / -removeObjectForKey: / -synchronize
 * and prints what is passed and what comes back. A save that never calls
 * setObject:forKey:, or that archives to nil/0 bytes, is the bug — and the two
 * are distinguishable only by watching both ends.
 *
 * Also hooks -[NSButtonCell drawWithFrame:inView:] and -drawBezelWithFrame:inView:
 * (same run answers "is the invisible button ever drawn?").
 *
 * Build:
 *   clang -arch x86_64 -dynamiclib -framework AppKit -o /tmp/prefswrite.dylib prefswrite.m
 */
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#include <stdio.h>

static id   (*o_archive)(id, SEL, id);
static id   (*o_unarchive)(id, SEL, id);
static void (*o_setobj)(id, SEL, id, id);
static void (*o_rmobj)(id, SEL, id);
static BOOL (*o_sync)(id, SEL);
static void (*o_draw)(id, SEL, NSRect, id);
static void (*o_bezel)(id, SEL, NSRect, id);

static const char *cstr(id o)
{
   @try { return o ? [[o description] UTF8String] : "(nil)"; }
   @catch (id e) { return "(undescribable)"; }
}

static void hexhead(const char *tag, NSData *d)
{
   if (!d) { fprintf(stderr, "   %s: (nil)\n", tag); return; }
   const unsigned char *p = [d bytes];
   NSUInteger n = [d length] < 32 ? [d length] : 32;
   fprintf(stderr, "   %s: len=%lu head=", tag, (unsigned long)[d length]);
   for (NSUInteger i = 0; i < n; i++) { fprintf(stderr, "%02x", p[i]); }
   fprintf(stderr, "\n");
}

static id n_archive(id s, SEL c, id root)
{
   id r = o_archive(s, c, root);
   hexhead("archive-output", r);
   fprintf(stderr, "[save] NSArchiver archivedDataWithRootObject:%s -> %s len=%ld\n",
           root ? class_getName(object_getClass(root)) : "nil",
           r ? "data" : "NIL", r ? (long)[(NSData *)r length] : -1L);
   fflush(stderr);
   return r;
}
static id n_unarchive(id s, SEL c, id data)
{
   id r = o_unarchive(s, c, data);
   fprintf(stderr, "[save] NSUnarchiver unarchiveObjectWithData: len=%ld -> %s\n",
           data ? (long)[(NSData *)data length] : -1L,
           r ? class_getName(object_getClass(r)) : "NIL");
   hexhead("unarchive-input", data);
   fflush(stderr);
   return r;
}
static void n_setobj(id s, SEL c, id v, id k)
{
   fprintf(stderr, "[save] setObject:%s(%s) forKey:%s\n",
           v ? class_getName(object_getClass(v)) : "nil",
           (v && [v isKindOfClass:[NSData class]])
              ? [[NSString stringWithFormat:@"%lu bytes", (unsigned long)[v length]] UTF8String]
              : cstr(v),
           cstr(k));
   if (v && [v isKindOfClass:[NSData class]]) { hexhead("write-data", v); }
   fflush(stderr);
   o_setobj(s, c, v, k);
}
static void n_rmobj(id s, SEL c, id k)
{
   fprintf(stderr, "[save] removeObjectForKey:%s\n", cstr(k)); fflush(stderr);
   o_rmobj(s, c, k);
}
static BOOL n_sync(id s, SEL c)
{
   BOOL r = o_sync(s, c);
   fprintf(stderr, "[save] synchronize -> %d\n", (int)r); fflush(stderr);
   return r;
}
static void n_draw(id s, SEL c, NSRect f, id v)
{
   fprintf(stderr, "[btn] draw  cell=%s frame=(%.0f,%.0f,%.0f,%.0f)\n",
           class_getName(object_getClass(s)), f.origin.x, f.origin.y,
           f.size.width, f.size.height);
   fflush(stderr);
   o_draw(s, c, f, v);
}
static void n_bezel(id s, SEL c, NSRect f, id v)
{
   fprintf(stderr, "[btn] bezel cell=%s frame=(%.0f,%.0f,%.0f,%.0f)\n",
           class_getName(object_getClass(s)), f.origin.x, f.origin.y,
           f.size.width, f.size.height);
   fflush(stderr);
   o_bezel(s, c, f, v);
}

static void hook(Class k, SEL sel, IMP repl, void *save, int cls)
{
   Method m = cls ? class_getClassMethod(k, sel) : class_getInstanceMethod(k, sel);
   if (!m) { fprintf(stderr, "[probe] no %s\n", sel_getName(sel)); return; }
   *(IMP *)save = method_getImplementation(m);
   method_setImplementation(m, repl);
}

__attribute__((constructor))
static void arm(void)
{
   hook(objc_getClass("NSArchiver"),   @selector(archivedDataWithRootObject:), (IMP)n_archive,   &o_archive,   1);
   hook(objc_getClass("NSUnarchiver"), @selector(unarchiveObjectWithData:),    (IMP)n_unarchive, &o_unarchive, 1);
   hook(objc_getClass("NSUserDefaults"), @selector(setObject:forKey:),    (IMP)n_setobj, &o_setobj, 0);
   hook(objc_getClass("NSUserDefaults"), @selector(removeObjectForKey:),  (IMP)n_rmobj,  &o_rmobj,  0);
   hook(objc_getClass("NSUserDefaults"), @selector(synchronize),          (IMP)n_sync,   &o_sync,   0);
   hook(objc_getClass("NSButtonCell"), @selector(drawWithFrame:inView:),      (IMP)n_draw,  &o_draw,  0);
   hook(objc_getClass("NSButtonCell"), @selector(drawBezelWithFrame:inView:), (IMP)n_bezel, &o_bezel, 0);
   fprintf(stderr, "[probe] armed\n");
   fflush(stderr);
}
