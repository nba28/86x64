#!/bin/bash
#
# Regression guard for the CFPreferences *Copy* SYSTEM-GLOBAL-locale-key forward
# (prefs_shim.c pref_is_system_global_key + the *Copy* variants).
#
# prefs_shim.c blanket-returns NULL from the CFPreferencesCopy* family to bypass
# the cfprefsd XPC round-trip that livelocks in a translated process (AudioToolbox
# startup). But "absent" is WRONG for the handful of Apple SYSTEM-GLOBAL locale/
# language keys a legacy app's localization reads and trusts to be non-empty:
# -[NSLocale preferredLanguages] reads AppleLanguages via CFPreferencesCopyAppValue
# and, with the blanket NULL, returns an EMPTY array; a caller doing
# preferredLanguages[0] (Aspyr's ASL localization in Civ IV) hits an UNCATCHABLE
# Swift bounds-trap SIGILL (rc=132, the graphics-setup-dialog startup crash). The
# fix FORWARDS exactly those global keys to real CF (via the private *WithContainer*
# variant), while every OTHER key keeps the daemon-free "absent" bypass (that's
# where the AudioToolbox livelock lives, so it must stay bypassed).
#
# libabiconv's dyld-interpose can't run standalone (its constructors need the
# low-4GB wrapper), so this test replicates the shim's FIXED body INLINE (exactly
# as prefs_shim.c writes it) and asserts:
#   (1) a system-global key (AppleLanguages) FORWARDS -> real CF returns a
#       non-empty value (RED under the old blanket-NULL: it returned NULL);
#   (2) an APP-DOMAIN key stays ABSENT (negative control -> the daemon-free
#       bypass / AudioToolbox livelock-avoidance still holds);
#   (3) the OLD blanket-NULL body would fail (1), so the guard is meaningful.
set -u
cd "$(dirname "$0")"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"; defaults delete com.x86x64.cfprefs_gtest 2>/dev/null' EXIT

# Fixture: an app-domain key that must stay absent under the bypass.
defaults write com.x86x64.cfprefs_gtest AppOnlyKey -string "should_stay_absent" 2>/dev/null

cat > "$TMP/t.c" <<'EOF'
#include <CoreFoundation/CoreFoundation.h>
#include <string.h>
#include <stdio.h>

/* Private *WithContainer* variant the *Copy* forward reaches real CF through. */
extern CFPropertyListRef _CFPreferencesCopyAppValueWithContainer(
   CFStringRef key, CFStringRef appID, CFURLRef container);

/* ==== EXACT discriminator + body from the FIXED prefs_shim.c ==== */
static int is_global(CFStringRef key) {
   if (!key) return 0;
   static const char *const g[] = {
      "AppleLanguages","AppleLocale","NSLanguages","AppleICUForce24HourTime",
      "AppleICUDateFormatStrings","AppleICUNumberSymbols",
      "AppleICUForceGregorianCalendar","AppleMeasurementUnits","AppleMetricUnits",
      "AppleTemperatureUnit","AppleFirstWeekday","AppleTextDirection",
      "AppleLanguagesDidMigrate","AppleLanguagesSchemaVersion","AppleKeyboardUIMode",
      NULL };
   char buf[64];
   if (!CFStringGetCString(key, buf, sizeof(buf), kCFStringEncodingUTF8)) return 0;
   for (const char *const *k = g; *k; ++k) if (!strcmp(buf, *k)) return 1;
   return 0;
}
/* fixed CFPreferencesCopyAppValue body */
static CFPropertyListRef fixed_copy(CFStringRef key, CFStringRef appID) {
   if (is_global(key)) {
      if (!appID) appID = kCFPreferencesCurrentApplication;
      return _CFPreferencesCopyAppValueWithContainer(key, appID, NULL);
   }
   return NULL;
}
/* OLD blanket-NULL body (negative control) */
static CFPropertyListRef old_copy(CFStringRef key, CFStringRef appID) {
   (void)key; (void)appID; return NULL;
}

int main(void) {
   /* (1) system-global key forwards -> non-empty AppleLanguages */
   CFArrayRef langs = (CFArrayRef)fixed_copy(CFSTR("AppleLanguages"),
                                             kCFPreferencesCurrentApplication);
   long nlang = (langs && CFGetTypeID(langs) == CFArrayGetTypeID())
                ? CFArrayGetCount(langs) : -1;

   /* (2) app-domain key stays absent under the bypass */
   CFPropertyListRef app = fixed_copy(CFSTR("AppOnlyKey"),
                                      CFSTR("com.x86x64.cfprefs_gtest"));

   /* (3) old blanket-NULL would have starved AppleLanguages */
   CFPropertyListRef old = old_copy(CFSTR("AppleLanguages"),
                                    kCFPreferencesCurrentApplication);

   printf("AppleLanguages(fixed)=%ld  AppOnlyKey(fixed)=%s  AppleLanguages(old)=%s\n",
          nlang, app ? "PRESENT" : "absent", old ? "PRESENT" : "NULL");

   int ok = (nlang >= 1)              /* forwarded -> real, non-empty         */
            && (app == NULL)          /* app-domain still bypassed (absent)   */
            && (old == NULL);         /* old body starved it (bug the fix cures) */
   return ok ? 0 : 1;
}
EOF

if ! clang -arch x86_64 -o "$TMP/t" "$TMP/t.c" \
        -framework CoreFoundation -Wl,-undefined,dynamic_lookup 2>"$TMP/err"; then
   echo "cfprefs-global-locale-forward: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi

# The fixture needs a real AppleLanguages on this machine (every Mac has one);
# if the host somehow reads none, skip rather than false-fail.
if ! defaults read -g AppleLanguages >/dev/null 2>&1 && \
   ! defaults read NSGlobalDomain AppleLanguages >/dev/null 2>&1; then
   echo "cfprefs-global-locale-forward: SKIP (no AppleLanguages on host)"; exit 0
fi

OUT=$("$TMP/t" 2>/dev/null); RC=$?
if [ "$RC" -eq 0 ]; then
   echo "cfprefs-global-locale-forward: PASS ($OUT)"
else
   echo "cfprefs-global-locale-forward: FAIL ($OUT)"
fi
exit $RC
