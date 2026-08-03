/* cfstrrange.m — name the CFString and the RANGE behind an uncaught NSRangeException.
 *
 * WHY THIS EXISTS. After Civ IV was retranslated against the fixed core (15c9dd5)
 * its crash changed from SIGBUS to SIGABRT, and the crash report's backtrace is:
 *
 *     abort <- __abort_message <- demangling_terminate_handler <- _objc_terminate
 *          <- __cxa_throw <- objc_exception_throw <- _CFThrowFormattedException
 *          <- -[NSTaggedPointerString getCharacters:range:]
 *          <- libabiconv __CFStringGetCharacters.l1 <- (i386 frame, unresolvable)
 *
 * So this is an ObjC exception, NOT the C++ RTTI machinery that 15c9dd5 repaired
 * (that hypothesis is refuted by this trace). CoreFoundation is rejecting the
 * RANGE we hand it for a short/tagged string.
 *
 * The crash report cannot tell us more: the unwinder stops at our abigen `.l1`
 * shim because it cannot walk the 4-byte i386 frame below it, and the report
 * carries no exception reason (only "abort() called"). Two things are missing and
 * both are cheap to measure:
 *
 *   1. the exception REASON string, which spells out the offending index/length
 *   2. the string's REAL length vs the range the translated caller asked for
 *
 * (2) is the discriminator that matters. If the requested range is exactly what
 * the app computed and the string is genuinely shorter, the defect is UPSTREAM —
 * some earlier bridged call handed the app a truncated string or a wrong length.
 * If the range itself is malformed (huge, negative, or sign-extended garbage),
 * the defect is in the by-value CFRange marshalling in the generated bridge.
 *
 * WHY AN INTERPOSE AND NOT A SHIM: the generated ___CFStringGetCharacters bridge
 * is already verified correct by inspection (both 4-byte i386 CFIndex fields are
 * sign-extended to 8 bytes and staged at the right offsets). Instrumenting it
 * would mean editing shipped, working code purely to observe it.
 * DYLD_INSERT_LIBRARIES observes the identical call with zero effect on the
 * deployed artifact.
 *
 * ⚠HEISENBUG DISCIPLINE: an observer can hide what it observes (this project has
 * been bitten twice). This failure is a deterministic argument-validation throw
 * inside CoreFoundation, not stack garbage, so printing should be safe — but
 * CONFIRM the abort still reproduces with this loaded before trusting anything it
 * says. If the abort disappears, the observer is the finding, not the evidence.
 *
 * Build (must be x86_64 — the translated app is x86_64 under Rosetta):
 *   clang -arch x86_64 -dynamiclib -framework Foundation -framework CoreFoundation \
 *         -o cfstrrange.dylib cfstrrange.m
 * Use:
 *   DYLD_INSERT_LIBRARIES=/path/cfstrrange.dylib "<App>/Contents/MacOS/<bin>"
 *   CFSPY_ALL=1   also log in-bounds calls (firehose; use to see the last good call)
 */

#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>

#define SPY(...) do { fprintf(stderr, "[cfspy] " __VA_ARGS__); } while (0)

static int spy_all(void) {
   static int v = -1;
   if (v < 0) { const char *e = getenv("CFSPY_ALL"); v = (e && *e && *e != '0'); }
   return v;
}

/* Render a short preview of the string without going through the very call we
 * are debugging. CFStringGetCString validates nothing about our range. */
static void preview(CFStringRef s, char *buf, size_t n)
{
   buf[0] = '\0';
   if (!s) { snprintf(buf, n, "(null)"); return; }
   if (!CFStringGetCString(s, buf, (CFIndex)n, kCFStringEncodingUTF8))
      snprintf(buf, n, "(unrenderable)");
}

static void spy_CFStringGetCharacters(CFStringRef theString, CFRange range, UniChar *buffer)
{
   CFIndex len = theString ? CFStringGetLength(theString) : -1;
   /* Out of bounds is exactly what CF is about to throw on. Negative fields catch
    * a sign-extension defect; the sum catches an over-long read. */
   const int bad = (!theString)
                 || range.location < 0 || range.length < 0
                 || range.location + range.length > len;

   if (bad || spy_all()) {
      char buf[64];
      preview(theString, buf, sizeof buf);
      SPY("%sCFStringGetCharacters(str=%p cls=%s len=%ld, range={loc=%ld,len=%ld}, buf=%p) '%s'\n",
          bad ? "*** OUT OF BOUNDS *** " : "",
          (void *)theString,
          theString ? object_getClassName((id)theString) : "-",
          (long)len, (long)range.location, (long)range.length, (void *)buffer, buf);
      if (bad) {
         SPY("    ^ CoreFoundation will throw here. loc+len=%ld exceeds len=%ld by %ld\n",
             (long)(range.location + range.length), (long)len,
             (long)(range.location + range.length - len));
      }
   }
   CFStringGetCharacters(theString, range, buffer);
}

__attribute__((used)) static struct {
   const void *replacement;
   const void *replacee;
} interposers[] __attribute__((section("__DATA,__interpose"))) = {
   { (const void *)&spy_CFStringGetCharacters, (const void *)&CFStringGetCharacters },
};

/* _objc_terminate() invokes the uncaught-exception handler before aborting, so
 * this is what recovers the reason string the crash report throws away. */
static void cfspy_uncaught(NSException *e)
{
   SPY("=== UNCAUGHT EXCEPTION ===\n");
   SPY("  name:   %s\n", [[e name] UTF8String] ?: "?");
   SPY("  reason: %s\n", [[e reason] UTF8String] ?: "?");
   SPY("  info:   %s\n", [[[e userInfo] description] UTF8String] ?: "-");
   SPY("  --- native frames (the i386 caller is NOT walkable below our shim) ---\n");
   for (NSString *f in [e callStackSymbols]) SPY("    %s\n", [f UTF8String] ?: "?");
}

__attribute__((constructor)) static void cfspy_init(void)
{
   setvbuf(stderr, NULL, _IONBF, 0);
   NSSetUncaughtExceptionHandler(&cfspy_uncaught);
   SPY("loaded — watching CFStringGetCharacters%s + uncaught exceptions\n",
       spy_all() ? " (ALL calls)" : " (out-of-bounds only)");
}
