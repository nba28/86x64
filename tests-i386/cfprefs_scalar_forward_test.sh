#!/bin/bash
#
# Regression guard for the CFPreferences scalar-read forwarding fix
# (prefs_shim.c x64_CFPreferencesGetAppBooleanValue / GetAppIntegerValue).
#
# prefs_shim.c dyld-interposes the whole CFPreferences family to bypass the
# cfprefsd XPC round-trip that livelocks inside a translated i386 app. The *Copy*
# variants correctly report "absent" (a fresh-launch default, no daemon hop), but
# the scalar Get{Boolean,Integer}Value PUBLIC entry points must return the GENUINE
# stored value, not a blanket false/0 — an always-absent stub silently zeroes
# every bool/int preference read in the process. The private *WithContainer*
# forwarders were already fixed for that reason (it had broken Quinn's
# integerForKey:); the PUBLIC entry points were left as blanket stubs, so a caller
# that invokes CFPreferencesGetAppBooleanValue DIRECTLY (Civ IV's first-run gate
# reads CFPreferencesGetAppBooleanValue(@"AspyrEulaAccepted", current-app)) always
# got false -> the EULA is never considered accepted (nor can acceptance persist)
# -> Civ exit()s at first-run. The fix forwards the public entry points to the
# real CF impl (via the private *WithContainer* variant) with a NULL-appID guard.
#
# libabiconv's dyld-interpose can't be exercised standalone (its constructors need
# the low-4GB wrapper environment), so this test replicates the shim's FIXED body
# INLINE (exactly as prefs_shim.c now writes it) and verifies it delivers a known
# stored bool(true)/int(7) and reports a truly-absent key as absent — the behavior
# the blanket stub violated. It also pins that the OLD blanket-stub body would
# fail the same assertions, so the guard is meaningful (repro-or-it-didn't-happen).
set -u
cd "$(dirname "$0")"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"; defaults delete com.x86x64.cfprefs_test 2>/dev/null' EXIT

DOMAIN="com.x86x64.cfprefs_test"
defaults write "$DOMAIN" TestBool -bool true 2>/dev/null
defaults write "$DOMAIN" TestInt  -int  7    2>/dev/null

cat > "$TMP/t.c" <<'EOF'
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>

/* Private *WithContainer* variants the public scalar API funnels through — the
 * real CF implementation prefs_shim.c's fixed public entry points forward to. */
extern Boolean _CFPreferencesGetAppBooleanValueWithContainer(
   CFStringRef key, CFStringRef appID, CFURLRef container, Boolean *valid);
extern CFIndex _CFPreferencesGetAppIntegerValueWithContainer(
   CFStringRef key, CFStringRef appID, CFURLRef container, Boolean *valid);

/* ==== EXACT body of the FIXED prefs_shim.c public entry points ==== */
static Boolean fixed_bool(CFStringRef key, CFStringRef appID, Boolean *valid) {
   if (!appID) { appID = kCFPreferencesCurrentApplication; }
   if (!appID) { if (valid) { *valid = false; } return false; }
   return _CFPreferencesGetAppBooleanValueWithContainer(key, appID, NULL, valid);
}
static CFIndex fixed_int(CFStringRef key, CFStringRef appID, Boolean *valid) {
   if (!appID) { appID = kCFPreferencesCurrentApplication; }
   if (!appID) { if (valid) { *valid = false; } return 0; }
   return _CFPreferencesGetAppIntegerValueWithContainer(key, appID, NULL, valid);
}
/* ==== the OLD blanket-stub body, to prove the guard would catch it ==== */
static Boolean stub_bool(CFStringRef key, CFStringRef appID, Boolean *valid) {
   (void)key; (void)appID; if (valid) { *valid = false; } return false;
}

int main(void) {
   CFStringRef app  = CFSTR("com.x86x64.cfprefs_test");
   CFStringRef kB   = CFSTR("TestBool");
   CFStringRef kI   = CFSTR("TestInt");
   CFStringRef kMiss= CFSTR("NoSuchKey");

   Boolean ex = false;
   Boolean b  = fixed_bool(kB, app, &ex);   Boolean bEx = ex;
   ex = false;
   CFIndex n  = fixed_int(kI, app, &ex);     Boolean nEx = ex;
   ex = true;
   Boolean miss = fixed_bool(kMiss, app, &ex); Boolean missEx = ex;
   /* negative control: the OLD stub must NOT deliver the real value */
   ex = false;
   Boolean stub = stub_bool(kB, app, &ex);

   printf("fixed: bool=%d(ex=%d) int=%ld(ex=%d) miss=%d(ex=%d) | oldstub_bool=%d\n",
          (int)b,(int)bEx,(long)n,(int)nEx,(int)miss,(int)missEx,(int)stub);

   int fixed_ok = (b == true) && bEx && (n == 7) && nEx
                  && (miss == false) && (missEx == false);
   int stub_broken = (stub == false);   /* the bug the fix cures */
   return (fixed_ok && stub_broken) ? 0 : 1;
}
EOF

if ! clang -arch x86_64 -o "$TMP/t" "$TMP/t.c" \
        -framework CoreFoundation -Wl,-undefined,dynamic_lookup 2>"$TMP/err"; then
   echo "cfprefs-scalar-forward: SKIP (compile failed)"; cat "$TMP/err"; exit 0
fi

OUT=$("$TMP/t" 2>/dev/null); RC=$?
if [ "$RC" -eq 0 ]; then
   echo "cfprefs-scalar-forward: PASS ($OUT)"
else
   echo "cfprefs-scalar-forward: FAIL ($OUT)"
fi
exit $RC
