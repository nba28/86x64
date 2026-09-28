/*
 * 99_cf_key_concat_replace — Civ IV localizer's KEY-CONCAT + TOKEN-SUBSTITUTION
 * chain (the "blank alert" root-cause probe).
 *
 * The blank CreateStandardAlert (icon + OK button, NO title/message text) traces
 * to Civ's localizer at translated 0x1000d1ac. An arg-string trace on the
 * deployed Civ showed it looking up the alert key "kErrInsufficientSystemVersion"
 * plus the suffixes "_err"/"_explain", and getting EMPTY CFStrings back — while a
 * native CFBundleCopyLocalizedString on Civ's own bundle returns the real text.
 * The trace showed the arena handle holding the 29-char base key, then going to
 * len=0 right after the suffix append. Civ builds the full lookup key like:
 *     CFMutableStringRef k = CFStringCreateMutableCopy(NULL, 0, baseKey);
 *     CFStringAppend(k, suffix);                       // "kErr..." + "_err"
 * and substitutes its game-name token like:
 *     range = CFStringFind(s, "<<<kGameName>>>", 0);
 *     CFStringReplace(s, range, "Civilization IV");
 *
 * If the mutable-copy+append concatenation OR the find+replace substitution
 * yields an EMPTY string through the arena wrap/unwrap round-trip, the alert
 * renders blank. Guard 98 already pins CFStringCreateMutable+AppendFormat; this
 * one pins the SIBLING mutable ops Civ's localizer actually uses:
 * CFStringCreateMutableCopy / CFStringAppend / CFStringFind / CFStringReplace,
 * each of which mutates a proxy-wrapped mutable CF object in place across
 * consecutive C-function shims.
 *
 * CoreFoundation isn't in the i386 sysroot, so every CF symbol is an undefined
 * dynamic_lookup import resolved at translate time by static-interpose ->
 * libabiconv's ___CF* shims (Makefile rule mirrors 98). Validated by stdout
 * content AND exit code.
 */

extern int  printf(const char *, ...);
extern void exit(int status);

typedef const void       *CFStringRef;
typedef void             *CFMutableStringRef;
typedef const void       *CFAllocatorRef;
typedef unsigned char     Boolean;
typedef long              CFIndex;
typedef unsigned int      CFStringEncoding;
typedef unsigned long     CFOptionFlags;
struct CFRange { CFIndex location, length; };
typedef struct CFRange    CFRange;

#define kCFStringEncodingASCII 0x0600u

extern CFStringRef        CFStringCreateWithCString(CFAllocatorRef, const char *,
                                                    CFStringEncoding);
extern CFMutableStringRef CFStringCreateMutableCopy(CFAllocatorRef, CFIndex,
                                                    CFStringRef);
extern void               CFStringAppend(CFMutableStringRef, CFStringRef);
extern void               CFStringReplace(CFMutableStringRef, CFRange, CFStringRef);
extern CFRange            CFStringFind(CFStringRef, CFStringRef, CFOptionFlags);
extern CFIndex            CFStringGetLength(CFStringRef);
extern Boolean            CFStringGetCString(CFStringRef, char *, CFIndex,
                                             CFStringEncoding);

int main(void)
{
   /* (1) KEY CONCAT: baseKey + "_err" -> the exact string Civ hands to
    * CFBundleCopyLocalizedString. Empty here == blank alert. */
   CFStringRef base = CFStringCreateWithCString((CFAllocatorRef)0,
                        "kErrInsufficientSystemVersion", kCFStringEncodingASCII);
   CFStringRef sfx  = CFStringCreateWithCString((CFAllocatorRef)0,
                        "_err", kCFStringEncodingASCII);
   if (!base || !sfx) { exit(90); }
   CFMutableStringRef key = CFStringCreateMutableCopy((CFAllocatorRef)0, 0, base);
   if (!key) { exit(91); }
   CFStringAppend(key, sfx);
   char kbuf[128] = {0};
   Boolean kok = CFStringGetCString(key, kbuf, sizeof kbuf, kCFStringEncodingASCII);
   printf("key.len=%ld ok=%d \"%s\"\n", (long)CFStringGetLength(key),
          kok ? 1 : 0, kbuf);

   /* (2) TOKEN SUBSTITUTION: replace "<<<kGameName>>>" in a message template.
    * Civ's post-processor (0x100793e0) does this; the trace showed the token
    * surviving UNsubstituted, so pin find+replace round-trips content. */
   CFStringRef tmpl = CFStringCreateWithCString((CFAllocatorRef)0,
                        "<<<kGameName>>> requires MacOS version 10.4 or later.",
                        kCFStringEncodingASCII);
   CFStringRef tok  = CFStringCreateWithCString((CFAllocatorRef)0,
                        "<<<kGameName>>>", kCFStringEncodingASCII);
   CFStringRef name = CFStringCreateWithCString((CFAllocatorRef)0,
                        "Civilization IV", kCFStringEncodingASCII);
   if (!tmpl || !tok || !name) { exit(92); }
   CFMutableStringRef msg = CFStringCreateMutableCopy((CFAllocatorRef)0, 0, tmpl);
   if (!msg) { exit(93); }
   CFRange r = CFStringFind(msg, tok, 0);
   printf("find.loc=%ld find.len=%ld\n", (long)r.location, (long)r.length);
   if (r.length > 0) { CFStringReplace(msg, r, name); }
   char mbuf[256] = {0};
   Boolean mok = CFStringGetCString(msg, mbuf, sizeof mbuf, kCFStringEncodingASCII);
   printf("msg.len=%ld ok=%d \"%s\"\n", (long)CFStringGetLength(msg),
          mok ? 1 : 0, mbuf);
   exit(0);
}
