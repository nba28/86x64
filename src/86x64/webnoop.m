/*
 * webnoop.m — suppress legacy-WebKit (WebView) instantiation in the translated
 * process.  ONE job (Unix philosophy): keep iPhoto's non-critical WebViews from
 * spinning up WebKit.  NOT folded into geoshim (geolocation + FairPlay-DRM
 * no-op) or the interpose layer (low-4GB allocation reconciliation) — a new
 * concern gets its own shim.
 *
 * Why: iPhoto embeds WebKit1 `WebView`s in NIBs (Places / Google-Maps, the
 * "What's New" welcome panel, Event/Photo flip animations — all non-essential;
 * the photo grid / source-list / toolbar are AppKit).  On modern macOS,
 * unarchiving a WebView runs `-[WebView _commonInitializationWithFrameName:
 * groupName:]` -> `+[WebFrame _createMainFrameWithPage:]` -> `FrameLoader::init`
 * which fires a synthetic load-complete -> WebKit performance logging ->
 * `WebCore::commonVMSlow` -> `JSC::VM::create`.  JSC's `sanitizeStackForVM`
 * stack-bounds RELEASE_ASSERT then TRAPS (EXC_BREAKPOINT): the translated thread
 * runs on a low-4GB stack disjoint from its pthread-registered native stack, so
 * rsp is "out of bounds".  (That stack impedance can't be reconciled cleanly —
 * the thread legitimately uses two disjoint stacks; see the project memory.)
 *
 * Fix: `-[WebView initWithCoder:]` decodes the slot as a WebNoopView (an NSView
 * subclass) instead of a WebView, so WebKit never spins up.  The view still
 * slots into the nib hierarchy with its frame/subviews intact (returning nil
 * corrupts the archive; a plain NSView crashes when the owner sends it WebView
 * messages like setFrameLoadDelegate:).  WebNoopView GRACEFULLY SWALLOWS every
 * WebView message (forwards to a zero/nil return), so the WebKit-driven UI is
 * inert rather than fatal.  The only loss is WebView content (Places map /
 * welcome HTML), which is non-essential.  Universal: triggers on the structural
 * property (a WebView instantiated under the translation runtime), not on any
 * specific app/window.
 *
 * Load via DYLD_INSERT_LIBRARIES (debug) or a wrapper LC_LOAD_DYLIB / bundle
 * insert (deployed).  WebKit loads lazily, so we swizzle when the WebView class
 * first appears (constructor + dyld add-image callback).  AppKit is linked so
 * NSView is registered before WebNoopView.
 */
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#import <objc/message.h>
#import <mach-o/dyld.h>

/* A blank stand-in for a suppressed WebView: a real NSView (so it decodes into
 * the hierarchy and behaves as a view) that answers every *WebView* message
 * with a zero return instead of an unrecognized-selector crash. */
@interface WebNoopView : NSView
@end

@implementation WebNoopView

- (BOOL)respondsToSelector:(SEL)aSelector
{
	if ([super respondsToSelector:aSelector]) { return YES; }
	Class wv = objc_getClass("WebView");
	return (wv && class_getInstanceMethod(wv, aSelector)) ? YES : NO;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel
{
	NSMethodSignature *s = [super methodSignatureForSelector:sel];
	if (s) { return s; }
	/* Unknown selector: borrow the real WebView signature so forwardInvocation:
	 * can run (and so struct/float returns are sized correctly). */
	Class wv = objc_getClass("WebView");
	Method m = wv ? class_getInstanceMethod(wv, sel) : NULL;
	const char *t = m ? method_getTypeEncoding(m) : NULL;
	return [NSMethodSignature signatureWithObjCTypes:(t ? t : "v@:")];
}

- (void)forwardInvocation:(NSInvocation *)inv
{
	/* Swallow the message; zero-fill any return value (nil for objects, 0 for
	 * scalars, {0} for structs). */
	NSUInteger len = [[inv methodSignature] methodReturnLength];
	if (len) {
		void *zero = calloc(1, len);
		[inv setReturnValue:zero];
		free(zero);
	}
}

@end

static int g_done = 0;

/* Replacement for -[WebView initWithCoder:]: decode the slot as a WebNoopView.
 * The un-initialized WebView `self` is intentionally leaked (a handful at
 * startup): -release would invoke WebView's dealloc, which dereferences the
 * never-created WebKit private state. */
static id webnoop_initWithCoder(id self, SEL _cmd, id coder)
{
	(void)self;
	WebNoopView *v = [WebNoopView alloc];
	return ((id (*)(id, SEL, id))objc_msgSend)(v, _cmd, coder);
}

static void try_swizzle(void)
{
	if (g_done) { return; }
	/* Opt-out gate: lets the real WebView spin up (e.g. to A/B against the
	 * jscstackpatch stack-bounds shim). Default behaviour (gate unset) is
	 * unchanged — webnoop still suppresses the WebView. */
	if (getenv("WEBNOOP_DISABLE")) { return; }
	Class wv = objc_getClass("WebView");
	if (!wv) { return; }                      /* WebKit not loaded yet */
	SEL sel = sel_registerName("initWithCoder:");

	/* Only touch WebView's OWN override — never the inherited NSView IMP. */
	unsigned int n = 0;
	Method *list = class_copyMethodList(wv, &n);
	Method own = (Method)0;
	for (unsigned int i = 0; i < n; ++i) {
		if (method_getName(list[i]) == sel) { own = list[i]; break; }
	}
	if (list) { free(list); }

	if (own) {
		method_setImplementation(own, (IMP)webnoop_initWithCoder);
	} else {
		class_addMethod(wv, sel, (IMP)webnoop_initWithCoder, "@@:@");
	}
	g_done = 1;
}

static void on_image(const struct mach_header *mh, intptr_t slide)
{
	(void)mh; (void)slide;
	try_swizzle();
}

__attribute__((constructor))
static void webnoop_init(void)
{
	try_swizzle();                               /* WebKit may already be loaded */
	_dyld_register_func_for_add_image(on_image); /* else catch it when it loads  */
}
