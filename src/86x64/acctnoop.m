/*
 * acctnoop.m — neutralize iPhoto's startup discovery of "unknown" MobileMe /
 * system accounts in the translated process.  ONE job (Unix philosophy): stop
 * the DEAD MobileMe account-discovery path from running.  NOT folded into
 * geoshim (geolocation + FairPlay-DRM no-op), webnoop (WebView suppression) or
 * the interpose layer (low-4GB allocation reconciliation) — a new concern gets
 * its own shim.
 *
 * Why: at startup iPhoto's -[IPAccountConfigurationManager findUnknownAccounts]
 * (gated by -shouldFindUnknownAccountsWithDefaultsAccountConfigurations:) probes
 * for a SYSTEM MobileMe account (-findUnknownSystemMobileMeAccount) so it can
 * auto-import it and auto-open an account-configuration pane.  MobileMe was shut
 * down by Apple in 2012, so this whole path is DEAD.  Worse, under translation
 * it crashes ~50% of launches, two ways seen in the geoshim fault ring:
 *   (1) -findUnknownSystemMobileMeAccount spawns a worker via pthread_create and
 *       faults in libabiconv __pthread_create.4 (addr=0x8, near-null), and
 *   (2) the auto-opened pane builds a popup menu via KVC on a garbage receiver
 *       (self32=0xffffffff) -> nil menu-item title -> AppKit aString!=nil abort
 *       (the fused garbage-PC 0x....01af2cff variant).
 * Both are the SAME dead subsystem.  There is no live service to talk to, so the
 * correct, proportionate fix is to no-op the discovery (last-resort suppression,
 * justified: genuinely dead surface — cf. project doctrine).
 *
 * Fix: when the (translated, reverse-registered) class IPAccountConfiguration
 * Manager appears at runtime, replace three instance methods with no-ops:
 *   -shouldFindUnknownAccountsWithDefaultsAccountConfigurations:  -> NO  (the
 *        designed off-switch; using iPhoto's own gate semantics)
 *   -findUnknownAccounts                  -> nil/void (defense in depth)
 *   -findUnknownSystemMobileMeAccount     -> nil      (the dead MobileMe probe)
 * Account configuration data (-accountConfigurations) is left untouched, so the
 * non-dead account UI (manually adding Facebook/Flickr/Email) is unaffected.
 *
 * IPAccountConfigurationManager is reverse-registered at RUNTIME (not via a dyld
 * image load), so — exactly like geoshim's ImageDB maintenance no-ops — a poll
 * thread waits for objc_getClass() then installs the no-ops.  The discovery runs
 * well after applicationDidFinishLaunching:, long after the class registers, so
 * the poll wins the race comfortably (the same approach geoshim uses reliably).
 *
 * Universal: triggers on the structural property (a class named IPAccount
 * ConfigurationManager performing dead unknown-account discovery under the
 * translation runtime), not on any window/app-name conditional.  Load via
 * DYLD_INSERT_LIBRARIES (debug) or a wrapper LC_LOAD_DYLIB / bundle insert.
 */
#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <objc/message.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

static int g_done = 0;

static int install_noops(void)
{
	Class c = objc_getClass("IPAccountConfigurationManager");
	if (!c) { return 0; }

	/* nil/void for the two probe methods; NO for the BOOL gate. */
	IMP noop_nil  = imp_implementationWithBlock(^id(id self){ (void)self; return nil; });
	IMP noop_gate = imp_implementationWithBlock(^BOOL(id self, id defs){
		(void)self; (void)defs; return NO; });

	const char *nil_sels[] = { "findUnknownAccounts",
	                           "findUnknownSystemMobileMeAccount", NULL };
	for (int i = 0; nil_sels[i]; ++i) {
		Method m = class_getInstanceMethod(c, sel_registerName(nil_sels[i]));
		if (m) {
			method_setImplementation(m, noop_nil);
		}
	}
	Method g = class_getInstanceMethod(c,
	             sel_registerName("shouldFindUnknownAccountsWithDefaultsAccountConfigurations:"));
	if (g) {
		method_setImplementation(g, noop_gate);
	}
	return 1;
}

static void *acct_poll(void *arg)
{
	(void)arg;
	for (int i = 0; i < 2000; ++i) {        /* up to ~100s */
		if (install_noops()) { g_done = 1; return NULL; }
		usleep(50000);                       /* 50ms */
	}
	return NULL;
}

__attribute__((constructor))
static void acctnoop_init(void)
{
	if (install_noops()) { g_done = 1; return; }   /* already registered? */
	pthread_t t;
	if (pthread_create(&t, NULL, acct_poll, NULL) == 0)
		pthread_detach(t);
}
