/* cfstring_range_shim.c — ONE job: restore classic CoreFoundation's TOLERANT
 * range handling for CFStringGetCharacters.
 *
 * ── THE CONTRACT CHANGE ──────────────────────────────────────────────────────
 * Classic CoreFoundation (10.4/10.6, the world our input binaries were built
 * for) did not validate the CFRange handed to CFStringGetCharacters. A caller
 * could ask for one character past the end and CF would simply copy whatever
 * was there. Modern CoreFoundation validates — but only in SOME of its concrete
 * string classes. Measured on macOS 26.6:
 *
 *     -[NSTaggedPointerString getCharacters:range:]  -> _CFThrowFormattedException
 *     __NSCFString                                   -> tolerated silently
 *
 * The thrown NSException is an ObjC exception crossing a C frame in a 2006
 * binary that has no handler for it, so it reaches _objc_terminate and aborts.
 * The whole process dies on a one-character overread.
 *
 * ── WHY THIS IS A TRANSLATOR CONCERN, NOT AN APP BUG ────────────────────────
 * Asking for length+1 to pick up a NUL terminator is a normal classic idiom: it
 * WORKED for the entire supported life of these binaries, and the app cannot be
 * recompiled. Worse, whether it kills the process now depends on which private
 * class CF happens to back the string with, which in turn depends on its length
 * and encoding — so the same code path is fatal for a 6-character string and
 * harmless for a 12-character one. That is exactly the kind of
 * modern-substrate-drift this project exists to absorb.
 *
 * Measured on Civilization IV (2026-08-03), which reaches this on EVERY launch
 * of the game proper — four calls, all asking for exactly length+1:
 *
 *     'kernel32.dll'  len=12  range={0,13}   __NSCFString          tolerated
 *     'Civ4 21031'    len=10  range={0,11}   __NSCFString          tolerated
 *     (124 chars)     len=124 range={0,125}  __NSCFString          tolerated
 *     'USER32'        len=6   range={0,7}    NSTaggedPointerString  ★ABORT
 *
 * ── THE FIX ─────────────────────────────────────────────────────────────────
 * Trigger on STRUCTURE, never on an app: any range extending past the end of
 * any string, in any binary. Clamp the copy to the characters that actually
 * exist and ZERO-FILL the remainder of the caller's buffer.
 *
 * Zero-fill rather than leave-untouched is deliberate and is strictly better
 * than what classic CF did. Classic copied whatever bytes followed the string,
 * i.e. garbage the caller could not rely on; a caller that asks for length+1 is
 * asking for a terminator. Writing U+0000 there gives it a DETERMINISTIC one.
 * The caller's buffer is sized for the range it asked for — it passed that size
 * itself — so filling the tail cannot overrun anything the caller did not
 * already commit to.
 *
 * Deliberately NOT extended to the rest of the CFRange-taking family
 * (CFStringGetBytes, CFStringGetCharacterAtIndex, ...): they share the contract
 * but nothing has been MEASURED hitting them, and shipping unverified surface
 * is how a "universal" fix turns into a guess. The clamp helper below is
 * factored so adding a sibling is a few lines once one is observed. Tracked in
 * todo_gaps.
 *
 *   kill switch : M64_NO_CFSTRING_RANGE_CLAMP=1  (exact pre-fix behaviour:
 *                 hand the raw range straight to CF and let it throw)
 *   trace       : ABICONV_CFSTRING_RANGE_TRACE=1 (one line per ACTUAL clamp)
 *
 * Guard: tests-i386/src/98_cfstring_range_overrun.c runs both arms.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uint64_t _86x64_unwrap_obj_arg(uint32_t h); /* i386 handle -> real CF ref */

static int clamp_enabled(void)
{
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("M64_NO_CFSTRING_RANGE_CLAMP");
      v = !(e && *e && *e != '0');
   }
   return v;
}

static int range_trace(void)
{
   static int v = -1;
   if (v < 0) {
      const char *e = getenv("ABICONV_CFSTRING_RANGE_TRACE");
      v = (e && *e && *e != '0');
   }
   return v;
}

/* Clamp `want` (starting at `loc`) to what `s` actually holds. Returns the
 * number of characters that may legally be copied; *out_loc is the (also
 * clamped) start. A wholly out-of-range request yields 0, which is the classic
 * outcome the caller can survive. */
static CFIndex cfstr_clamp(CFStringRef s, CFIndex loc, CFIndex want, CFIndex *out_loc)
{
   CFIndex slen = s ? CFStringGetLength(s) : 0;
   if (loc < 0) loc = 0;
   if (loc > slen) loc = slen;
   *out_loc = loc;
   if (want <= 0) return 0;
   CFIndex avail = slen - loc;
   return (want > avail) ? avail : want;
}

/* i386 cdecl slots: [0]=CFStringRef handle  [1]=range.location  [2]=range.length
 * [3]=UniChar *buffer (a 32-bit pointer into the low 4GB).
 *
 * NOTE the i386 CFRange occupies TWO 4-byte slots (CFIndex is `long` = 4 bytes
 * there); the generated bridge this replaces sign-extended both into the
 * x86_64 16-byte CFRange, and so do we via the int32_t casts. */
uint32_t shim_CFStringGetCharacters(uint32_t *a)
{
   CFStringRef s   = (CFStringRef)(uintptr_t)_86x64_unwrap_obj_arg(a[0]);
   CFIndex    loc  = (CFIndex)(int32_t)a[1];
   CFIndex    want = (CFIndex)(int32_t)a[2];
   UniChar   *buf  = (UniChar *)(uintptr_t)a[3];

   if (!clamp_enabled()) {          /* pre-fix arm: let CF validate and throw */
      CFStringGetCharacters(s, CFRangeMake(loc, want), buf);
      return 0;
   }
   if (!buf || want <= 0) return 0;

   CFIndex cloc = 0;
   CFIndex n    = cfstr_clamp(s, loc, want, &cloc);

   if (n > 0) CFStringGetCharacters(s, CFRangeMake(cloc, n), buf);

   if (n < want) {                  /* the tail the app asked for but CF lacks */
      memset(buf + n, 0, (size_t)(want - n) * sizeof(UniChar));
      if (range_trace())
         fprintf(stderr,
                 "[cfrange] CFStringGetCharacters clamped: len=%ld request={%ld,%ld}"
                 " -> copied %ld, zero-filled %ld\n",
                 (long)(s ? CFStringGetLength(s) : -1), (long)loc, (long)want,
                 (long)n, (long)(want - n));
   }
   return 0;
}
