/*
 * prefs_shim.c — bypass the CFPreferences -> cfprefsd synchronous XPC path.
 *
 * WHY: on modern macOS every CFPreferences read does a synchronous XPC
 * round-trip to the cfprefsd daemon via dispatch_mach. Inside a translated
 * i386 app (Rosetta + our low-4GB wrapper) that round-trip livelocks in
 * libdispatch's __DISPATCH_WAIT_FOR_ENQUEUER__ spin and never returns, so the
 * app hangs at the first preference read during startup. (See project memory:
 * the hang is real-libdispatch-internal, not bad translated data, and is NOT
 * fixable via our allocation policy.)
 *
 * HOW: the read is not always made by the translated app — it is frequently
 * made framework->framework (the first observed one is
 * libAudioToolboxUtility's CASmartPreferences::Pref::Load() calling the
 * PRIVATE _CFPreferencesCopyAppValueWithContainerAndConfiguration). Neither
 * static-interpose (which only rewrites the i386 app's binds) nor a
 * libabiconv-local symbol definition can catch a CoreFoundation call made by
 * AudioToolbox. So we use DYLD INTERPOSING (__DATA,__interpose, the same
 * mechanism libinterpose uses for mmap): it replaces the target functions for
 * EVERY caller in the process, including system frameworks.
 *
 * We interpose at the C entry points all preference reads funnel through —
 * both the public CFPreferences* API and the private *WithContainer* variants
 * that the public ones and frameworks call internally — and report "no
 * preference set" (reads) / succeed silently (sync). That is correct for a
 * fresh launch and lets every caller fall back to built-in defaults, with no
 * daemon round-trip. GENERIC: this helps EVERY revived legacy app.
 *
 * NOTE: NSUserDefaults reaches -[_CFXPreferences copyAppValueForKey:...]
 * (Objective-C) directly, BELOW these C functions, so this does not cover that
 * path; if a target hangs there too, the objc bridge must special-case
 * NSUserDefaults. The first blocker (AudioToolbox) is a pure C-function path.
 *
 * libabiconv is linked -undefined dynamic_lookup, so the private CF symbols
 * referenced below resolve at load time even though the SDK stub omits them.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

/* Private CoreFoundation entry points (mach-o: __CFPreferences...). */
extern CFPropertyListRef
_CFPreferencesCopyAppValueWithContainerAndConfiguration(CFStringRef key, CFStringRef appID,
                                                        CFURLRef container, CFURLRef configuration);
extern CFPropertyListRef
_CFPreferencesCopyAppValueWithContainer(CFStringRef key, CFStringRef appID, CFURLRef container);
extern CFPropertyListRef
_CFPreferencesCopyValueWithContainer(CFStringRef key, CFStringRef appID, CFStringRef user,
                                     CFStringRef host, CFURLRef container);
/* The Get{Boolean,Integer}Value public API funnels through these PRIVATE
 * *WithContainer* variants. The public entry points below are interposed, but a
 * caller that reaches the private variant directly (e.g. our abigen marshalling
 * shim ___CFPreferencesGetAppBooleanValue.l1 forwards to the real public symbol,
 * whose body tail-calls the WithContainer variant BELOW the interposed frame)
 * still hits the real implementation. On modern macOS that implementation
 * dereferences appID as _Nonnull inside -[_CFXPreferences withSearchListFor
 * Identifier:...] (CFStringGetCharacterAtIndex -> __CF_IS_OBJC), so a legacy
 * app that passed a NULL / current-application appID (tolerated on 10.6) now
 * SIGSEGVs. Interpose the private variants too so the whole family reports
 * "absent" without ever dereferencing appID. (Root cause: Civ IV Steam
 * ASLShowFPS -> CFPreferencesGetAppBooleanValue with a NULL appID.) */
extern Boolean
_CFPreferencesGetAppBooleanValueWithContainer(CFStringRef key, CFStringRef appID,
                                              CFURLRef container, Boolean *valid);
extern CFIndex
_CFPreferencesGetAppIntegerValueWithContainer(CFStringRef key, CFStringRef appID,
                                              CFURLRef container, Boolean *valid);

/* ---- SYSTEM-GLOBAL LOCALE/LANGUAGE key allowlist -----------------------------
 *
 * The *Copy* variants below blanket-return NULL to bypass the cfprefsd XPC
 * round-trip that livelocks in a translated process (the AudioToolbox startup
 * path). But "absent" is WRONG for the handful of Apple SYSTEM-GLOBAL locale /
 * language keys that a legacy app's localization code reads and then trusts to be
 * non-empty: e.g. -[NSLocale preferredLanguages] reads AppleLanguages via
 * CFPreferencesCopyAppValue; with the blanket NULL it returns an EMPTY array, and
 * a caller that does preferredLanguages[0] (Aspyr's ASL localization in Civ IV)
 * hits an UNCATCHABLE Swift bounds-trap SIGILL. These keys live in the global
 * (.GlobalPreferences / kCFPreferencesAnyApplication) domain, are read-mostly,
 * and do NOT trigger the startup livelock (verified: reading AppleLanguages via
 * the real _CFPreferencesCopyAppValueWithContainer in-process returns the genuine
 * value with no hang). So for exactly these keys we FORWARD to the real CF
 * implementation (through the private *WithContainer* variant — dyld skips self-
 * interposition, so a direct call from inside THIS dylib reaches the REAL CF, no
 * recursion); every other key keeps the daemon-free "absent" bypass. GENERIC:
 * this helps ANY translated app whose localization reads the system language/
 * locale globals, and is keyed on the KEY (a structural property), never on an
 * app name. NB the appID is irrelevant to the discriminator — these are read
 * against kCFPreferencesCurrentApplication, which falls back to the global domain
 * for keys not set app-locally. (Longer term: forward ALL *Copy* with a BLOCKLIST
 * of just the livelocking path — see the known-gaps list.) */
static int pref_is_system_global_key(CFStringRef key)
{
	if (!key) { return 0; }
	static const char *const kGlobals[] = {
		"AppleLanguages",              /* -> NSLocale preferredLanguages */
		"AppleLocale",
		"NSLanguages",
		"AppleICUForce24HourTime",
		"AppleICUDateFormatStrings",
		"AppleICUNumberSymbols",
		"AppleICUForceGregorianCalendar",
		"AppleMeasurementUnits",
		"AppleMetricUnits",
		"AppleTemperatureUnit",
		"AppleFirstWeekday",
		"AppleTextDirection",
		"AppleLanguagesDidMigrate",
		"AppleLanguagesSchemaVersion",
		"AppleKeyboardUIMode",
		NULL,
	};
	char buf[64];
	if (!CFStringGetCString(key, buf, sizeof(buf), kCFStringEncodingUTF8)) {
		return 0;
	}
	for (const char *const *k = kGlobals; *k; ++k) {
		if (!strcmp(buf, *k)) { return 1; }
	}
	return 0;
}

/* The keyboard input-source domain is SYSTEM state the app never owns: HIToolbox
 * reads AppleEnabledInputSources / AppleDefaultAsciiInputSource from it at
 * startup, and with the blanket "absent" the process has no enabled layouts and
 * falls back to U.S. — every translated app then types QWERTY on a Hungarian
 * QWERTZ keyboard (Quinn's 'y' key answered on the physical Z). Forward the
 * whole domain, keyed on the DOMAIN (structural), like the global keys above. */
static int pref_should_forward(CFStringRef key, CFStringRef appID)
{
	return pref_is_system_global_key(key)
	    || (appID && CFEqual(appID, CFSTR("com.apple.HIToolbox")));
}

/* PREFS_TRACE=1: log every *Copy* read with its verdict (which keys a
 * framework reads, and which of them the bypass answers "absent"). */
static void pref_trace(const char *fn, CFStringRef key, CFStringRef appID, int fwd)
{
	static int on = -1;
	if (on < 0) { on = getenv("PREFS_TRACE") != NULL; }
	if (!on) { return; }
	char k[128] = "(null)", a[128] = "(null)";
	if (key) { CFStringGetCString(key, k, sizeof k, kCFStringEncodingUTF8); }
	if (appID) { CFStringGetCString(appID, a, sizeof a, kCFStringEncodingUTF8); }
	fprintf(stderr, "[prefs] %s key=%s app=%s -> %s\n", fn, k, a, fwd ? "FORWARD" : "absent");
}

/* ---- replacements: report "absent" / succeed without the daemon ---- */

static CFPropertyListRef
x64_pref_copy_app_cc(CFStringRef key, CFStringRef appID, CFURLRef container, CFURLRef configuration)
{
	pref_trace("x64_pref_copy_app_cc", key, appID, pref_should_forward(key, appID));
	if (pref_should_forward(key, appID)) {
		if (!appID) { appID = kCFPreferencesCurrentApplication; }
		return _CFPreferencesCopyAppValueWithContainer(key, appID, container);
	}
	(void) container; (void) configuration;
	return NULL;
}

static CFPropertyListRef
x64_pref_copy_app_c(CFStringRef key, CFStringRef appID, CFURLRef container)
{
	pref_trace("x64_pref_copy_app_c", key, appID, pref_should_forward(key, appID));
	if (pref_should_forward(key, appID)) {
		if (!appID) { appID = kCFPreferencesCurrentApplication; }
		return _CFPreferencesCopyAppValueWithContainer(key, appID, container);
	}
	return NULL;
}

static CFPropertyListRef
x64_pref_copy_value_c(CFStringRef key, CFStringRef appID, CFStringRef user, CFStringRef host,
                      CFURLRef container)
{
	pref_trace("x64_pref_copy_value_c", key, appID, pref_should_forward(key, appID));
	if (pref_should_forward(key, appID)) {
		if (!appID) { appID = kCFPreferencesCurrentApplication; }
		return _CFPreferencesCopyValueWithContainer(key, appID, user, host, container);
	}
	return NULL;
}

static CFPropertyListRef
x64_CFPreferencesCopyAppValue(CFStringRef key, CFStringRef applicationID)
{
	pref_trace("x64_CFPreferencesCopyAppValue", key, applicationID, pref_should_forward(key, applicationID));
	if (pref_should_forward(key, applicationID)) {
		if (!applicationID) { applicationID = kCFPreferencesCurrentApplication; }
		return _CFPreferencesCopyAppValueWithContainer(key, applicationID, NULL);
	}
	return NULL;
}

static CFPropertyListRef
x64_CFPreferencesCopyValue(CFStringRef key, CFStringRef applicationID, CFStringRef userName,
                           CFStringRef hostName)
{
	pref_trace("x64_CFPreferencesCopyValue", key, applicationID, pref_should_forward(key, applicationID));
	if (pref_should_forward(key, applicationID)) {
		if (!applicationID) { applicationID = kCFPreferencesCurrentApplication; }
		return _CFPreferencesCopyValueWithContainer(key, applicationID, userName,
		                                            hostName, NULL);
	}
	return NULL;
}

/* The PUBLIC Get{Boolean,Integer}Value entry points must FORWARD to the real CF
 * implementation, NOT report a blanket "absent" — for the same reason the private
 * *WithContainer* variants below do (an always-false stub silently zeroes every
 * bool/int preference read in the process). The prior blanket stub here was a bug:
 * the comment above claimed these "funnel through" the forwarding private variant
 * and so "still hit the real implementation", but that only holds for a caller
 * that reaches the PRIVATE variant directly — a translated app (or framework) that
 * calls the PUBLIC CFPreferencesGetAppBooleanValue hits THIS interposed entry and
 * never reaches the private forwarder, so it always got false. Civ IV's first-run
 * gate reads CFPreferencesGetAppBooleanValue(@"AspyrEulaAccepted", current-app):
 * the stub forced it false EVERY launch -> the EULA is never considered accepted
 * (and the acceptance can never persist) -> Civ tries to show the EULA window and,
 * when that fails, exit()s at first-run. Forward to the real value (with the same
 * NULL-appID guard the *WithContainer* forwarders use); dyld skips self-
 * interposition, so calling the public real symbol from inside this dylib would
 * recurse — go through the private *WithContainer* variant instead, whose real CF
 * body is what the public API tail-calls anyway. The cfprefsd XPC-livelock bypass
 * stays on the *Copy* variants above (the AudioToolbox startup path); these
 * scalar reads are ordinary post-startup lookups that must return genuine data. */
static Boolean
x64_CFPreferencesGetAppBooleanValue(CFStringRef key, CFStringRef applicationID,
                                    Boolean *keyExistsAndHasValidFormat)
{
	if (!applicationID) { applicationID = kCFPreferencesCurrentApplication; }
	if (!applicationID) {
		if (keyExistsAndHasValidFormat) { *keyExistsAndHasValidFormat = false; }
		return false;
	}
	return _CFPreferencesGetAppBooleanValueWithContainer(
	           key, applicationID, NULL, keyExistsAndHasValidFormat);
}

static CFIndex
x64_CFPreferencesGetAppIntegerValue(CFStringRef key, CFStringRef applicationID,
                                    Boolean *keyExistsAndHasValidFormat)
{
	if (!applicationID) { applicationID = kCFPreferencesCurrentApplication; }
	if (!applicationID) {
		if (keyExistsAndHasValidFormat) { *keyExistsAndHasValidFormat = false; }
		return 0;
	}
	return _CFPreferencesGetAppIntegerValueWithContainer(
	           key, applicationID, NULL, keyExistsAndHasValidFormat);
}

/* Private *WithContainer* variants the Get{Boolean,Integer}Value family funnels
 * through.
 *
 * CAUTION: -[NSUserDefaults integerForKey:] / boolForKey: reach the runtime
 * through EXACTLY these two private variants (verified live: with an "always
 * absent" stub here, integerForKey: returned 0 for a key whose objectForKey:
 * and CFPreferencesGetAppIntegerValue both returned the real value 1). Returning
 * a blanket "absent" therefore silently zeroes every NSUserDefaults integer/bool
 * read in the process — which broke Quinn: its new-game code reads
 * integerForKey:@"QuinnStartingLevel", got 0, and tripped
 * NSParameterAssert(level >= 1 && level <= 10) at QuinnGame.m:300 -> uncaught
 * NSException -> abort on pressing Play.
 *
 * So these two must NOT lie: they FORWARD to the real CF implementation and
 * return the genuine stored value. We keep only the NULL-appID guard (Civ IV
 * Steam's ASLShowFPS read passes a NULL / current-application appID, which
 * modern CF dereferences as _Nonnull and SIGSEGVs): substitute
 * kCFPreferencesCurrentApplication for a NULL appID, exactly like libinterpose's
 * public-API guard, then forward. dyld skips self-interposition, so a direct
 * call to _CFPreferencesGet...WithContainer from inside THIS same dylib reaches
 * the REAL CF (not this stub) — no recursion. The cfprefsd XPC-livelock bypass
 * stays on the *Copy* variants above (the AudioToolbox startup path); these
 * integer/bool reads are ordinary post-startup NSUserDefaults lookups.
 * (kCFPreferencesCurrentApplication comes from the included <CoreFoundation.h>.) */
static Boolean
x64_pref_get_bool_c(CFStringRef key, CFStringRef appID, CFURLRef container, Boolean *valid)
{
	if (!appID) { appID = kCFPreferencesCurrentApplication; }
	if (!appID) { if (valid) { *valid = false; } return false; }
	return _CFPreferencesGetAppBooleanValueWithContainer(key, appID, container, valid);
}

static CFIndex
x64_pref_get_int_c(CFStringRef key, CFStringRef appID, CFURLRef container, Boolean *valid)
{
	if (!appID) { appID = kCFPreferencesCurrentApplication; }
	if (!appID) { if (valid) { *valid = false; } return 0; }
	return _CFPreferencesGetAppIntegerValueWithContainer(key, appID, container, valid);
}

static CFArrayRef
x64_CFPreferencesCopyKeyList(CFStringRef applicationID, CFStringRef userName, CFStringRef hostName)
{
	(void) applicationID; (void) userName; (void) hostName;
	return NULL;
}

static CFDictionaryRef
x64_CFPreferencesCopyMultiple(CFArrayRef keysToFetch, CFStringRef applicationID,
                              CFStringRef userName, CFStringRef hostName)
{
	(void) keysToFetch; (void) applicationID; (void) userName; (void) hostName;
	/* empty (non-NULL) dict: callers often index without a NULL check */
	return CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
	                          &kCFTypeDictionaryKeyCallBacks,
	                          &kCFTypeDictionaryValueCallBacks);
}

static CFArrayRef
x64_CFPreferencesCopyApplicationList(CFStringRef userName, CFStringRef hostName)
{
	(void) userName; (void) hostName;
	return NULL;
}

static Boolean
x64_CFPreferencesAppSynchronize(CFStringRef applicationID)
{
	(void) applicationID;
	return true;
}

static Boolean
x64_CFPreferencesSynchronize(CFStringRef applicationID, CFStringRef userName, CFStringRef hostName)
{
	(void) applicationID; (void) userName; (void) hostName;
	return true;
}

/* ---- dyld interpose table: replace real CF prefs with the above ---- */

typedef struct { const void *replacement; const void *replacee; } interpose_t;

__attribute__((used)) static const interpose_t __prefs_interposers[]
__attribute__((section("__DATA,__interpose"))) = {
	{ (const void *) x64_pref_copy_app_cc,
	  (const void *) _CFPreferencesCopyAppValueWithContainerAndConfiguration },
	{ (const void *) x64_pref_copy_app_c,
	  (const void *) _CFPreferencesCopyAppValueWithContainer },
	{ (const void *) x64_pref_copy_value_c,
	  (const void *) _CFPreferencesCopyValueWithContainer },
	{ (const void *) x64_CFPreferencesCopyAppValue,
	  (const void *) CFPreferencesCopyAppValue },
	{ (const void *) x64_CFPreferencesCopyValue,
	  (const void *) CFPreferencesCopyValue },
	{ (const void *) x64_CFPreferencesGetAppBooleanValue,
	  (const void *) CFPreferencesGetAppBooleanValue },
	{ (const void *) x64_CFPreferencesGetAppIntegerValue,
	  (const void *) CFPreferencesGetAppIntegerValue },
	{ (const void *) x64_pref_get_bool_c,
	  (const void *) _CFPreferencesGetAppBooleanValueWithContainer },
	{ (const void *) x64_pref_get_int_c,
	  (const void *) _CFPreferencesGetAppIntegerValueWithContainer },
	{ (const void *) x64_CFPreferencesCopyKeyList,
	  (const void *) CFPreferencesCopyKeyList },
	{ (const void *) x64_CFPreferencesCopyMultiple,
	  (const void *) CFPreferencesCopyMultiple },
	{ (const void *) x64_CFPreferencesCopyApplicationList,
	  (const void *) CFPreferencesCopyApplicationList },
	{ (const void *) x64_CFPreferencesAppSynchronize,
	  (const void *) CFPreferencesAppSynchronize },
	{ (const void *) x64_CFPreferencesSynchronize,
	  (const void *) CFPreferencesSynchronize },
};
