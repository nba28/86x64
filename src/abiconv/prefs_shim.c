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

/* Private CoreFoundation entry points (mach-o: __CFPreferences...). */
extern CFPropertyListRef
_CFPreferencesCopyAppValueWithContainerAndConfiguration(CFStringRef key, CFStringRef appID,
                                                        CFURLRef container, CFURLRef configuration);
extern CFPropertyListRef
_CFPreferencesCopyAppValueWithContainer(CFStringRef key, CFStringRef appID, CFURLRef container);
extern CFPropertyListRef
_CFPreferencesCopyValueWithContainer(CFStringRef key, CFStringRef appID, CFStringRef user,
                                     CFStringRef host, CFURLRef container);

/* ---- replacements: report "absent" / succeed without the daemon ---- */

static CFPropertyListRef
x64_pref_copy_app_cc(CFStringRef key, CFStringRef appID, CFURLRef container, CFURLRef configuration)
{
	(void) key; (void) appID; (void) container; (void) configuration;
	return NULL;
}

static CFPropertyListRef
x64_pref_copy_app_c(CFStringRef key, CFStringRef appID, CFURLRef container)
{
	(void) key; (void) appID; (void) container;
	return NULL;
}

static CFPropertyListRef
x64_pref_copy_value_c(CFStringRef key, CFStringRef appID, CFStringRef user, CFStringRef host,
                      CFURLRef container)
{
	(void) key; (void) appID; (void) user; (void) host; (void) container;
	return NULL;
}

static CFPropertyListRef
x64_CFPreferencesCopyAppValue(CFStringRef key, CFStringRef applicationID)
{
	(void) key; (void) applicationID;
	return NULL;
}

static CFPropertyListRef
x64_CFPreferencesCopyValue(CFStringRef key, CFStringRef applicationID, CFStringRef userName,
                           CFStringRef hostName)
{
	(void) key; (void) applicationID; (void) userName; (void) hostName;
	return NULL;
}

static Boolean
x64_CFPreferencesGetAppBooleanValue(CFStringRef key, CFStringRef applicationID,
                                    Boolean *keyExistsAndHasValidFormat)
{
	(void) key; (void) applicationID;
	if (keyExistsAndHasValidFormat) { *keyExistsAndHasValidFormat = false; }
	return false;
}

static CFIndex
x64_CFPreferencesGetAppIntegerValue(CFStringRef key, CFStringRef applicationID,
                                    Boolean *keyExistsAndHasValidFormat)
{
	(void) key; (void) applicationID;
	if (keyExistsAndHasValidFormat) { *keyExistsAndHasValidFormat = false; }
	return 0;
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
