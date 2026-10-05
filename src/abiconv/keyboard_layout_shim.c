/*
 * keyboard_layout_shim.c — Keyboard Layout Services (KLGet*) on Text Input Sources.
 *
 * KLGetCurrentKeyboardLayout / KLGetKeyboardLayoutProperty were removed from
 * 64-bit HIToolbox (a NULL jump); TIS is their successor. Call of Duty 4 asks for
 * the current layout's script group (kKLGroupIdentifier) once at startup to key
 * its GetScriptVariable lookups. A KeyboardLayoutRef is the TIS input source,
 * wrapped as an i386 handle.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <Carbon/Carbon.h>
#include "gap.h"

/* Script.h's sm* codes are deprecated; these are their classic values. */
enum { scRoman = 0, scJapanese = 1, scTradChinese = 2, scKorean = 3, scArabic = 4,
       scHebrew = 5, scGreek = 6, scCyrillic = 7, scThai = 21, scSimpChinese = 25,
       scCentralEuroRoman = 29 };
#define KL_PARAM_ERR ((uint32_t)-50)
#define KL_MEMFULL_ERR ((uint32_t)-108)

extern uint32_t x64_objc_wrap(uint64_t real);
extern uint64_t x64_objc_unwrap(uint32_t h);

/* OSStatus KLGetCurrentKeyboardLayout(KeyboardLayoutRef *oKeyboardLayout) */
uint32_t shim_KLGetCurrentKeyboardLayout(uint32_t *a) {
    uint32_t *out = (uint32_t *)(uintptr_t)a[0];
    if (out) *out = 0;
    TISInputSourceRef s = TISCopyCurrentKeyboardLayoutInputSource();
    if (!s) return KL_PARAM_ERR;
    /* The classic ref was not owned by the caller: keep one live per layout
     * (the arena dedupes by pointer, so a repeat hands back the same handle). */
    static TISInputSourceRef held[8];
    static int nheld;
    int known = 0;
    for (int i = 0; i < nheld; i++) if (held[i] == s) known = 1;
    if (known) CFRelease(s);
    else if (nheld < 8) held[nheld++] = s;
    if (out) *out = x64_objc_wrap((uint64_t)(uintptr_t)s);
    return noErr;
}

/* Classic script group of a layout, from its primary language. */
static int32_t layout_script(TISInputSourceRef s) {
    CFArrayRef langs = (CFArrayRef)TISGetInputSourceProperty(s, kTISPropertyInputSourceLanguages);
    char l[16] = "";
    if (langs && CFArrayGetCount(langs) > 0)
        CFStringGetCString((CFStringRef)CFArrayGetValueAtIndex(langs, 0), l, sizeof l, kCFStringEncodingUTF8);
    static const struct { const char *lang; int32_t script; } map[] = {
        { "ja", scJapanese }, { "zh-Hant", scTradChinese }, { "ko", scKorean },
        { "ar", scArabic }, { "he", scHebrew }, { "el", scGreek }, { "ru", scCyrillic },
        { "uk", scCyrillic }, { "bg", scCyrillic }, { "sr", scCyrillic }, { "th", scThai },
        { "zh", scSimpChinese }, { "hu", scCentralEuroRoman }, { "cs", scCentralEuroRoman },
        { "pl", scCentralEuroRoman }, { "sk", scCentralEuroRoman }, { "et", scCentralEuroRoman },
        { "lv", scCentralEuroRoman }, { "lt", scCentralEuroRoman },
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++)
        if (!strncmp(l, map[i].lang, strlen(map[i].lang))) return map[i].script;
    return scRoman;
}

/* OSStatus KLGetKeyboardLayoutProperty(KeyboardLayoutRef, KeyboardLayoutPropertyTag,
 *                                      const void **oValue)
 * Scalar properties travel IN the pointer slot, as the classic API did. */
uint32_t shim_KLGetKeyboardLayoutProperty(uint32_t *a) {
    uint32_t *out = (uint32_t *)(uintptr_t)a[2];
    if (out) *out = 0;
    TISInputSourceRef s = (TISInputSourceRef)(uintptr_t)x64_objc_unwrap(a[0]);
    if (!s || !out) return KL_PARAM_ERR;
    switch (a[1]) {
    case kKLGroupIdentifier:
        *out = (uint32_t)layout_script(s);
        return noErr;
    case kKLuchrData: {             /* the 'uchr' bytes, copied where i386 can read them */
        CFDataRef d = (CFDataRef)TISGetInputSourceProperty(s, kTISPropertyUnicodeKeyLayoutData);
        if (!d) return KL_PARAM_ERR;
        static struct { TISInputSourceRef s; void *lo; } cache[8];
        for (int i = 0; i < 8; i++) if (cache[i].s == s) { *out = (uint32_t)(uintptr_t)cache[i].lo; return noErr; }
        void *lo = malloc((size_t)CFDataGetLength(d));     /* libabiconv malloc: low 4GB */
        if (!lo) return KL_MEMFULL_ERR;
        memcpy(lo, CFDataGetBytePtr(d), (size_t)CFDataGetLength(d));
        for (int i = 0; i < 8; i++) if (!cache[i].s) { cache[i].s = s; cache[i].lo = lo; break; }
        *out = (uint32_t)(uintptr_t)lo;
        return noErr;
    }
    case kKLName:
    case kKLLocalizedName: {
        CFStringRef n = (CFStringRef)TISGetInputSourceProperty(s, kTISPropertyLocalizedName);
        if (!n) return KL_PARAM_ERR;
        *out = x64_objc_wrap((uint64_t)(uintptr_t)n);
        return noErr;
    }
    case kKLKind:                   /* uchr-only on modern macOS */
        *out = kKLuchrKind;
        return noErr;
    default:                        /* KCHR data, numeric id, icon, language: not reached yet */
        GAP_STUB(a);
        return KL_PARAM_ERR;
    }
}
