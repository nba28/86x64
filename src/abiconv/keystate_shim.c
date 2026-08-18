/*
 * keystate_shim.c — emulate a physically-held key at launch (Carbon GetKeys).
 *
 * Several legacy Mac games gate a capability self-check behind a documented
 * "hold KEY at launch to bypass" escape hatch, polled via the classic Carbon
 * GetKeys() KeyMap. (Halo's InsufficientCPUSpeed check: "To bypass this check,
 * hold down the 'p' key when you start Halo." — is_key_held(0x23) reads GetKeys
 * and, if 'p' is down, skips the check.)
 *
 * Under our translation the user frequently cannot supply that keypress: the app
 * is launched from Finder/LaunchServices (double-click), and the shell consumes
 * the keystroke (type-select) before the app polls it. This shim lets the
 * vendor's OWN escape hatch be triggered without a physical key.
 *
 * Behaviour: override GetKeys so that, for keycodes named in ABICONV_HOLD_KEYS
 * (comma/space-separated virtual keycodes; 'p' = 35 = 0x23), the corresponding
 * KeyMap bits read as PRESSED for the first ABICONV_HOLD_KEYS_MS milliseconds of
 * process life (default 20000). Real keyboard state is preserved — native
 * GetKeys still fills the map; we only OR-in the forced bits, and only during the
 * startup window, so gameplay input is unaffected once the window elapses.
 *
 * Default-off: with ABICONV_HOLD_KEYS unset this is a transparent passthrough to
 * native GetKeys. Structural, not app-specific — any i386 binary with a
 * hold-key-at-launch bypass benefits. Wired via maptable_tramp.asm MTSHIM
 * ___GetKeys (which also excludes GetKeys from the abigen consider set).
 *
 * KeyMap layout: GetKeys fills 16 bytes; a virtual keycode K is at byte (K>>3),
 * bit (K&7) — the exact byte-indexed access Halo's is_key_held performs
 * (movzbl keymap[K>>3]; (>> (K&7)) & 1), so OR-ing that bit is consistent.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <mach/mach_time.h>

typedef void (*getkeys_fn)(void *);

static getkeys_fn native_getkeys(void) {
   static getkeys_fn fn = (getkeys_fn)-1;
   if (fn == (getkeys_fn)-1) {
      fn = (getkeys_fn)dlsym(RTLD_DEFAULT, "GetKeys");
   }
   return fn;
}

/* Monotonic process uptime in milliseconds. */
static uint64_t uptime_ms(void) {
   static mach_timebase_info_data_t tb;
   static uint64_t t0;
   if (tb.denom == 0) {
      mach_timebase_info(&tb);
      t0 = mach_absolute_time();
   }
   uint64_t dns = (mach_absolute_time() - t0) * tb.numer / tb.denom;
   return dns / 1000000ULL;
}

/* ---- diagnostic counters (ABICONV_KEYS_TRACE) -------------------------------
 * Halo's main menu moves its HIGHLIGHT and plays its sound, but neither a click
 * nor RETURN activates anything. Carbon delivers the RETURN key events (traced:
 * keycode 36, char 0x0d, both down and up) yet Halo installs NO raw-key handler
 * for them - so if it reads the keyboard at all, it reads it HERE, by polling.
 * Whether this call is polled, and whether it ever reports a key as down, is
 * therefore a load-bearing fact and not a detail. Counting it costs nothing and
 * distinguishes "polled thousands of times, always empty" (our bug) from "never
 * called" (Halo reads input somewhere else entirely). */
static unsigned long gk_calls, gk_nonempty, gk_return_down;

static void gk_summary(void) {
   if (!getenv("ABICONV_KEYS_TRACE")) { return; }
   fprintf(stderr, "[keys] ===== summary =====\n");
   fprintf(stderr, "[keys] GetKeys calls=%lu  map-nonempty=%lu  RETURN(36)-down=%lu\n",
           gk_calls, gk_nonempty, gk_return_down);
   if (gk_calls && !gk_nonempty) {
      fprintf(stderr, "[keys] ⚠polled %lu times and the keymap was ALWAYS EMPTY - "
              "native GetKeys is not reporting keyboard state\n", gk_calls);
   } else if (!gk_calls) {
      fprintf(stderr, "[keys] GetKeys was never called - the app does not poll "
              "the keyboard this way\n");
   }
}

/* void GetKeys(KeyMap keys);  i386 frame: keys[0] = KeyMap ptr (16 bytes). */
uint32_t shim_GetKeys(uint32_t *a) {
   uint8_t *km = (uint8_t *)(uintptr_t)a[0];   /* i386 KeyMap, low-4GB, usable */
   if (!km) return 0;

   getkeys_fn gk = native_getkeys();
   if (gk) gk(km);                              /* real keyboard state ... */
   else    memset(km, 0, 16);                   /* ... or a clean empty map */

   if (getenv("ABICONV_KEYS_TRACE")) {
      static int reg;
      if (!reg) { reg = 1; atexit(gk_summary); }
      gk_calls++;
      int any = 0;
      for (int i = 0; i < 16; i++) { if (km[i]) { any = 1; break; } }
      if (any) { gk_nonempty++; }
      /* keycode 36 = Return, in the same bit order this file already uses */
      if (km[36 >> 3] & (uint8_t)(1u << (36 & 7))) {
         if (gk_return_down++ < 5) {
            fprintf(stderr, "[keys] GetKeys reports RETURN(36) DOWN "
                    "(call #%lu)\n", gk_calls);
         }
      }
      if (gk_calls <= 3 || (any && gk_nonempty <= 5)) {
         fprintf(stderr, "[keys] GetKeys #%lu map=%02x%02x%02x%02x%02x%02x%02x%02x"
                 "%02x%02x%02x%02x%02x%02x%02x%02x%s\n", gk_calls,
                 km[0],km[1],km[2],km[3],km[4],km[5],km[6],km[7],
                 km[8],km[9],km[10],km[11],km[12],km[13],km[14],km[15],
                 any ? "  (a key IS down)" : "");
      }
   }

   const char *hold = getenv("ABICONV_HOLD_KEYS");
   if (!hold || !*hold) return 0;               /* default: transparent */

   const char *ms_s = getenv("ABICONV_HOLD_KEYS_MS");
   uint64_t    win  = ms_s ? strtoull(ms_s, NULL, 0) : 20000ULL;
   if (uptime_ms() > win) return 0;             /* window elapsed -> normal */

   for (const char *p = hold; *p; ) {
      char *end;
      long  k = strtol(p, &end, 0);
      if (end == p) break;
      if (k >= 0 && k < 128) {
         km[(unsigned long)k >> 3] |= (uint8_t)(1u << (k & 7));
      }
      p = end;
      while (*p == ',' || *p == ' ' || *p == '\t') p++;
   }

   if (getenv("ABICONV_KEYS_TRACE")) {
      fprintf(stderr, "[keys] GetKeys forced ABICONV_HOLD_KEYS=%s (win=%llums)\n",
              hold, (unsigned long long)win);
   }
   return 0;
}
