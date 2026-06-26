/*
 * jscstackpatch.m — let a legacy WebKit1 WebView spin up under translation by
 * widening JSC's per-thread cached native-stack bounds to also cover our
 * low-4GB translation stack.  ONE job (Unix philosophy); a separate shim from
 * webnoop (which SUPPRESSES the WebView) and from geoshim.
 *
 * Problem (the WebKit-B fix; see project memory init_high_stack_bug /
 * iphoto_fairplay_pic_jumptable s59):
 *   Translated i386 code runs on a <4GB stack.  A nib-embedded or programmatic
 *   WebView spins up a JSC VM; JSC::sanitizeStackForVM RELEASE_ASSERTs that the
 *   current rsp lies within WTF::Thread::current()'s cached stack bounds.  WTF
 *   caches those bounds ONCE (the first WTF::Thread::current() on the thread),
 *   capturing the REAL HIGH native stack — so when the SAME thread later runs
 *   the WebView init on the low-4GB stack, rsp is "out of range" and JSC traps
 *   (EXC_BREAKPOINT).  The s59b pthread_get_stackaddr_np interposers cannot help
 *   because WTF never re-queries after the one-time cache.
 *
 * Fix (B, targeted): at the WebView-init chokepoint
 * (-[WebView _commonInitializationWithFrameName:groupName:], reached by BOTH the
 * NIB initWithCoder: path AND the programmatic _initWithFrame:frameName:
 * groupName: path), LOWER the calling thread's cached WTF::Thread stack BOUND
 * (the low limit) to the base of the low-4GB translation stack it is actually
 * running on.  The cached ORIGIN (high) is left intact, so the resulting
 * [low-4GB-base, high-native-origin] range covers BOTH stacks the thread uses
 * and sanitizeStackForVM passes.  Then call the original init: the real WebView
 * runs (functionality retained) instead of webnoop's blank stand-in.
 *
 * Offsets (empirical, version-specific — VALIDATED before any write):
 *   WTF::Thread::current() ptr  : %gs:0x2e0  (per-thread TLS slot)
 *   WTF::Thread m_stack.m_origin: +0x18      (high address, stack base)
 *   WTF::Thread m_stack.m_bound : +0x20      (low address, stack limit)
 * The patch is a NO-OP unless the slot/offsets look sane (non-null ptr, origin
 * is a plausible high native address, bound<origin), so a stale offset on a
 * future OS degrades to "WebView still traps" rather than memory corruption.
 *
 * ⚠️ Mutually exclusive with webnoop for the SAME WebView: webnoop replaces the
 * WebView with a blank view (no WebKit), this lets the real WebView run.  Deploy
 * EITHER, not both, for a given WebView path.  RISK: passing sanitizeStackForVM
 * may merely expose the next WebKit-under-translation wall (JSC JIT codegen on a
 * low-4GB stack) — pair with JSC_useJIT=0 / JSC_useGigacage=0 and TEST
 * File->Import end to end.  geoshim's SIGSEGV/BUS handler also does not chain to
 * the previous handler; once WebKit runs it will take SIGSEGV/SIGTRAP for GC/JIT
 * barriers, so geoshim's handler must be made to chain first (see memory).
 *
 * Enable with JSCSTACKPATCH=1 (opt-in: leaves the working build untouched until
 * deliberately turned on).
 */

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <mach-o/dyld.h>

static int g_done;
static IMP  g_orig_commoninit;     /* original -[WebView _commonInit...] */

/* Discover the VM region of the current (low-4GB) stack from an on-stack
 * address — mirrors interpose.c low4gb_stack_region. Returns the region base
 * (the low end the stack can grow down to) or 0 on a high/native stack. */
static uintptr_t low4gb_stack_base(void)
{
	volatile int probe;
	uintptr_t onstack = (uintptr_t)&probe;
	if (onstack >= 0x100000000UL) { return 0; }     /* real high native stack */
	mach_vm_address_t a = onstack;
	mach_vm_size_t sz = 0;
	vm_region_basic_info_data_64_t info;
	mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
	mach_port_t obj = MACH_PORT_NULL;
	if (mach_vm_region(mach_task_self(), &a, &sz, VM_REGION_BASIC_INFO_64,
	                   (vm_region_info_t)&info, &cnt, &obj) != KERN_SUCCESS) {
		return 0;
	}
	if (onstack < a || onstack >= a + sz) { return 0; }
	return (uintptr_t)a;
}

/* Lower this thread's cached WTF::Thread stack bound to cover the low-4GB
 * stack. Defensive: validates the TLS slot + offsets before any write.
 *
 * VALIDATED 2026-06-26 against the live macOS WebKit via lldb (disasm of
 * JSC::sanitizeStackForVM(VM&)): the function does, with no intervening write
 * in the thread-already-exists path,
 *     movq %gs:0x2e0, %r14            ; r14 = WTF::Thread::current()
 *     movq 0x18(%r14), %rax           ; origin (high addr / stack base)
 *     movq 0x1f078(%rbx), %rcx        ; rcx   = VM's recorded stack pointer
 *     cmp  %rcx, %rax ; jb  -> CRASH   ; require origin >= rcx
 *     cmp  0x20(%r14), %rcx ; jbe -> CRASH ; require rcx > bound
 * i.e. it RELEASE_ASSERTs  bound < rcx <= origin.  Under translation the thread
 * runs on a low-4GB stack and rcx is that low rsp, but WTF cached a stack whose
 * low limit (`bound`, +0x20) sits ABOVE the actual rsp (the s59b pthread
 * interposers make WTF cache one low-4GB stack region while the translated main
 * frame executes lower in / in a different low-4GB region) — so rcx <= bound and
 * it traps.  Lowering `bound` to the floor of the stack region we are ACTUALLY
 * executing on restores bound < rcx <= origin and the assert passes; the path
 * only READS +0x20 so the patch survives.  origin (+0x18) is left intact. */
static void patch_wtf_stack_bound(void)
{
	volatile int probe;
	uintptr_t onstack = (uintptr_t)&probe;
	uintptr_t base = low4gb_stack_base();
	if (!base) { return; }            /* not on a low stack: nothing to do */

	void *wtf = NULL;
	__asm__ volatile ("movq %%gs:0x2e0, %0" : "=r"(wtf));
	if (!wtf) { return; }             /* WTF::Thread not yet created on thread */

	uintptr_t *origin = (uintptr_t *)((char *)wtf + 0x18);
	uintptr_t *bound  = (uintptr_t *)((char *)wtf + 0x20);

	/* Validate the object looks like a WTF::Thread carrying a [bound,origin)
	 * stack range, so a stale %gs/field offset on a future OS degrades to a
	 * no-op rather than corrupting an unrelated heap object.  origin may be a
	 * HIGH native address OR a low-4GB one (the interposed case) — accept both,
	 * but require a sane span (a real thread stack, not a wild pointer pair). */
	if (*origin == 0 || *bound == 0 || *bound >= *origin) { return; }
	if (*origin - *bound > 0x40000000UL) { return; }   /* span >1GB: not a stack */
	if (*origin <= onstack) { return; }                /* origin not above our rsp */
	if (base >= *bound) { return; }                    /* already covers our stack */
	if (*origin - base > 0x40000000UL) { return; }     /* widening to cover us would
	                                                    * make an insane >1GB range
	                                                    * (the disjoint high-native-
	                                                    * stack impedance): leave it */

	uintptr_t newbound = base;        /* floor of the low-4GB stack region we are
	                                   * actually running on; keeps overflow
	                                   * detection meaningful for that stack */
	if (getenv("JSCSTACKPATCH"))      /* opt-in trace */
		fprintf(stderr,
		        "[jscstackpatch] tid=%x wtf=%p rsp~0x%lx origin=0x%lx bound 0x%lx -> 0x%lx\n",
		        pthread_mach_thread_np(pthread_self()), wtf, (unsigned long)onstack,
		        (unsigned long)*origin, (unsigned long)*bound,
		        (unsigned long)newbound);
	*bound = newbound;
}

/* Replacement for -[WebView _commonInitializationWithFrameName:groupName:].
 * Widen the JSC stack bound, then run the real init (the WebView is retained,
 * unlike webnoop's blank stand-in). */
static void wv_commoninit(id self, SEL _cmd, id frameName, id groupName)
{
	patch_wtf_stack_bound();
	if (g_orig_commoninit) {
		((void (*)(id, SEL, id, id))g_orig_commoninit)(self, _cmd, frameName,
		                                               groupName);
	}
}

static void try_swizzle(void)
{
	if (g_done) { return; }
	if (!getenv("JSCSTACKPATCH")) { return; }   /* opt-in: off by default */
	Class wv = objc_getClass("WebView");
	if (!wv) { return; }                         /* WebKit not loaded yet */
	SEL sel = sel_registerName("_commonInitializationWithFrameName:groupName:");
	Method m = class_getInstanceMethod(wv, sel);
	if (!m) { return; }
	g_orig_commoninit = method_setImplementation(m, (IMP)wv_commoninit);
	g_done = 1;
	if (getenv("JSCSTACKPATCH")) {
		fprintf(stderr, "[jscstackpatch] hooked -[WebView "
		        "_commonInitializationWithFrameName:groupName:]\n");
		fflush(stderr);
	}
}

static void on_image(const struct mach_header *mh, intptr_t slide)
{
	(void)mh; (void)slide;
	try_swizzle();
}

__attribute__((constructor))
static void jscstackpatch_init(void)
{
	try_swizzle();                                /* WebKit may already be loaded */
	_dyld_register_func_for_add_image(on_image);  /* else catch it when it loads */
}
