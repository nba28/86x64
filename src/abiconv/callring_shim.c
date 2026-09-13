/*
 * callring_shim.c — env-gated (ABICONV_CALLRING) per-thread reverse-bridge
 * ObjC call-ring + Swift-trap dumper. ONE job: name the exact system API that
 * hands a translated (i386) caller an object it then mis-uses (the immediate
 * motivator: Civ IV's graphics-setup worker thread receives an EMPTY
 * Swift.__EmptyArrayStorage from some modern Swift-implemented Foundation/AppKit
 * API and does objectAtIndex:0 -> uncatchable Swift bounds-trap SIGILL; the
 * built-in lldb unwind can't cross the i386-frame reverse-bridge boundary, so we
 * record the calls in-process and dump them at the trap).
 *
 * DIAGNOSTIC ONLY — zero behavior change. Inert unless ABICONV_CALLRING is set:
 * the recorder early-returns on a cached flag and the SIGILL handler isn't
 * installed. Reusable for ANY future "translated caller mis-uses a value a
 * modern API returned" crash (records per-thread {selector, receiver class,
 * RETURN class + array count}).
 *
 * WHY per-thread + return-capture (vs the existing global g_fwd_ring in
 * objc_shim.c): (1) the crash is on a WORKER thread whose sends are evicted from
 * the 128-deep global ring by main-thread traffic; a __thread ring keeps each
 * thread's own recent history. (2) g_fwd_ring records only the SEND; naming the
 * culprit API needs the RETURN value's class/count — the call that RETURNED the
 * empty array is X. We record at the return site (objc_bridge_ret_finish), so
 * each entry carries what the send returned.
 *
 * The recorder is called from objc_shim.c's objc_bridge_ret_finish (the reverse
 * bridge's return path, after the native objc_msgSend completes) via the tiny
 * ABI below. It never allocates, never sends ObjC messages that could recurse
 * (only class_getName / object_getClass / CFGetTypeID-free count via a cached
 * NSArray IMP-free path), and is signal-safe on the dump side (only async-safe
 * writes of already-captured strings).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <objc/runtime.h>
#include <objc/message.h>

/* ---- forward decls (definitions below) ----------------------------------- */
int  _86x64_callring_enabled(void);
void _86x64_callring_arm(void);
void _86x64_callring_dump_all(uint32_t crashing_tid);
void _86x64_callring_send(uint64_t self, uint64_t sel, uint32_t caller_ra);
void _86x64_callring_ret(uint64_t ret);

/* ---- enablement (cached) ------------------------------------------------- */
static int g_callring_on = -1;
int _86x64_callring_enabled(void) {
	int v = __atomic_load_n(&g_callring_on, __ATOMIC_RELAXED);
	if (v < 0) {
		v = getenv("ABICONV_CALLRING") ? 1 : 0;
		__atomic_store_n(&g_callring_on, v, __ATOMIC_RELAXED);
	}
	return v;
}

/* ---- per-thread ring ----------------------------------------------------- */
#define CR_RING 96          /* per-thread depth */
#define CR_NAMELEN 48

struct cr_ent {
	const char *sel;        /* interned selector name (stable libobjc string) */
	const char *recv_cls;   /* interned class name of the receiver            */
	const char *ret_cls;    /* interned class name of the return value        */
	long        ret_count;  /* NSArray/NSDictionary count if applicable, else -1 */
	uint64_t    ret_raw;    /* raw native return (vrax)                       */
	uint32_t    caller_ra;  /* i386 caller return address (Civ .text-relative-ish) */
};

struct cr_ring {
	struct cr_ent ents[CR_RING];
	uint32_t pos;
	uint32_t tid;
	struct cr_ring *next;   /* global list for the signal dumper             */
};

static __thread struct cr_ring *tls_ring;         /* this thread's ring       */
static struct cr_ring *g_ring_list;               /* all threads' rings       */
static pthread_mutex_t g_ring_list_lock = PTHREAD_MUTEX_INITIALIZER;

static struct cr_ring *cr_get_ring(void) {
	struct cr_ring *r = tls_ring;
	if (!r) {
		r = (struct cr_ring *)calloc(1, sizeof(*r));
		if (!r) return NULL;
		r->tid = pthread_mach_thread_np(pthread_self());
		pthread_mutex_lock(&g_ring_list_lock);
		r->next = g_ring_list;
		g_ring_list = r;
		pthread_mutex_unlock(&g_ring_list_lock);
		tls_ring = r;
	}
	return r;
}

/* Is `cls` (or a superclass) named `want`? Cheap, no message sends. */
static int cr_is_kind(Class cls, const char *want) {
	for (Class c = cls; c; c = class_getSuperclass(c)) {
		const char *n = class_getName(c);
		if (n && !strcmp(n, want)) return 1;
	}
	return 0;
}

/* Pending send captured at PREP (self+sel+caller_ra), completed at the RETURN
 * site with the native return value. prep and the wrap/finish return run
 * back-to-back on the SAME thread with no intervening reverse-bridge send that
 * would clobber this (a re-entrant send from inside the callee completes its own
 * prep+return pair before this one's return runs), so a single __thread slot is
 * safe and correct. */
struct cr_pending { uint64_t self; uint64_t sel; uint32_t caller_ra; int live; };
static __thread struct cr_pending tls_pending;

/* Called from objc_bridge_prep (reverse bridge, i386->native, before the send).
 * self/sel are the native resolved receiver + selector. */
void _86x64_callring_send(uint64_t self, uint64_t sel, uint32_t caller_ra);
void _86x64_callring_send(uint64_t self, uint64_t sel, uint32_t caller_ra) {
	if (!_86x64_callring_enabled()) return;
	_86x64_callring_arm();
	tls_pending.self = self;
	tls_pending.sel = sel;
	tls_pending.caller_ra = caller_ra;
	tls_pending.live = 1;
}

/* Called from x64_objc_wrap_ret / objc_bridge_ret_finish (return site, after the
 * native send) with the raw native return value. Completes the pending entry. */
void _86x64_callring_ret(uint64_t ret);
void _86x64_callring_ret(uint64_t ret) {
	if (!_86x64_callring_enabled()) return;
	if (!tls_pending.live) return;         /* no matching send captured */
	uint64_t self = tls_pending.self, sel = tls_pending.sel;
	uint32_t caller_ra = tls_pending.caller_ra;
	tls_pending.live = 0;

	struct cr_ring *r = cr_get_ring();
	if (!r) return;

	struct cr_ent *e = &r->ents[r->pos & (CR_RING - 1)];
	r->pos++;

	e->sel = sel ? sel_getName((SEL)(uintptr_t)sel) : "?";
	e->recv_cls = "?";
	if (self) {
		Class c = object_getClass((id)(uintptr_t)self);
		if (c) e->recv_cls = class_getName(c);
	}
	e->ret_raw = ret;
	e->ret_cls = "-";
	e->ret_count = -1;
	e->caller_ra = caller_ra;

	/* Only inspect the return if it's a plausible heap object pointer:
	 * non-null, > low tagged range, 8-aligned-ish, not an arena/handle. A
	 * tagged pointer has bit 63 or bit 0 set on x86_64 — skip those. */
	if (ret && (ret & 1) == 0 && ret >= 0x1000 && (ret >> 63) == 0) {
		Class c = object_getClass((id)(uintptr_t)ret);
		if (c) {
			e->ret_cls = class_getName(c);
			/* count for array/dictionary/set — via the cached selector, still
			 * a message send but to a well-known collection; safe here (not in
			 * the signal handler). Gate on the class family to avoid sending
			 * count to something that doesn't respond. */
			if (cr_is_kind(c, "NSArray") || cr_is_kind(c, "__NSArray0") ||
			    cr_is_kind(c, "__SwiftNativeNSArrayWithContiguousStorage") ||
			    cr_is_kind(c, "Swift.__EmptyArrayStorage") ||
			    cr_is_kind(c, "NSDictionary") || cr_is_kind(c, "NSSet") ||
			    cr_is_kind(c, "__NSArrayM") || cr_is_kind(c, "__NSArrayI")) {
				SEL cnt = sel_registerName("count");
				if (class_respondsToSelector(c, cnt) ||
				    class_getInstanceMethod(c, cnt)) {
					typedef unsigned long (*cnt_fn)(id, SEL);
					cnt_fn f = (cnt_fn)objc_msgSend;
					e->ret_count = (long)f((id)(uintptr_t)ret, cnt);
				}
			}
		}
	}
}

/* ---- Swift-trap (SIGILL) dumper ------------------------------------------ */
/* Async-signal context: only write already-captured strings (interned by the
 * runtime; stable) via one buffered fprintf per line. We do NOT walk the
 * ObjC runtime here. Dump the CRASHING thread's ring first, then all others. */
static struct sigaction g_prev_sigill;

static void cr_dump_ring(struct cr_ring *r, int is_crashing) {
	if (!r) return;
	fprintf(stderr, "[callring] thread tid=%x%s (newest last):\n",
	        r->tid, is_crashing ? " <<< CRASHING" : "");
	uint32_t pos = r->pos;
	int n = (pos < CR_RING) ? (int)pos : CR_RING;
	for (int i = n - 1; i >= 0; --i) {
		struct cr_ent *e = &r->ents[(pos - 1 - i) & (CR_RING - 1)];
		fprintf(stderr,
		        "   [-%2d] %-30s <- %-34s => %-40s count=%-4ld ra=0x%08x\n",
		        i, e->sel ? e->sel : "?",
		        e->recv_cls ? e->recv_cls : "?",
		        e->ret_cls ? e->ret_cls : "-",
		        e->ret_count, e->caller_ra);
	}
	fflush(stderr);
}

/* Dump every thread's ring, the named one first. Exposed because the ring's job
 * — naming the API that handed a translated caller a value it then mis-uses — is
 * not specific to the Swift trap it was written for. The fault reporter
 * (fault_report_shim.c) calls this on SIGSEGV/SIGBUS, which is the same question
 * asked about a different signal: Portal 2's renderer init faults inside Metal on
 * a stack the unwinder cannot cross, so the last few ObjC sends the translated
 * caller made ARE the evidence. Async-signal-safe on the same terms as the SIGILL
 * path: only already-interned strings, no runtime walks, no allocation. */
void _86x64_callring_dump_all(uint32_t crashing_tid);
void _86x64_callring_dump_all(uint32_t crashing_tid) {
	if (!_86x64_callring_enabled()) return;
	for (struct cr_ring *r = g_ring_list; r; r = r->next) {
		if (r->tid == crashing_tid) { cr_dump_ring(r, 1); break; }
	}
	for (struct cr_ring *r = g_ring_list; r; r = r->next) {
		if (r->tid != crashing_tid) cr_dump_ring(r, 0);
	}
	fprintf(stderr, "[callring] ====== end call-ring dump ======\n");
	fflush(stderr);
}

static void cr_sigill_handler(int sig, siginfo_t *info, void *uctx) {
	uint32_t me = pthread_mach_thread_np(pthread_self());
	fprintf(stderr,
	        "\n[callring] ====== SIGILL (Swift trap?) on tid=%x addr=%p ======\n",
	        me, info ? info->si_addr : NULL);
	_86x64_callring_dump_all(me);
	/* chain to the previous handler (or re-raise default) so the process still
	 * dies exactly as it would have — pure diagnostic, no behavior change. */
	if (g_prev_sigill.sa_flags & SA_SIGINFO) {
		if (g_prev_sigill.sa_sigaction) {
			g_prev_sigill.sa_sigaction(sig, info, uctx);
			return;
		}
	} else if (g_prev_sigill.sa_handler &&
	           g_prev_sigill.sa_handler != SIG_DFL &&
	           g_prev_sigill.sa_handler != SIG_IGN) {
		g_prev_sigill.sa_handler(sig);
		return;
	}
	/* default: restore SIG_DFL and re-raise so the SIGILL kills us normally. */
	signal(SIGILL, SIG_DFL);
	raise(SIGILL);
}

/* Installed lazily on the first recorded send (so it only arms when the ring is
 * actually in use), and idempotent. */
static pthread_once_t g_sigill_once = PTHREAD_ONCE_INIT;
static void cr_install_sigill(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_sigaction = cr_sigill_handler;
	sa.sa_flags = SA_SIGINFO | SA_NODEFER;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGILL, &sa, &g_prev_sigill);
	fprintf(stderr, "[callring] armed: SIGILL dumper installed (ABICONV_CALLRING)\n");
	fflush(stderr);
}
void _86x64_callring_arm(void);
void _86x64_callring_arm(void) {
	if (!_86x64_callring_enabled()) return;
	pthread_once(&g_sigill_once, cr_install_sigill);
}
