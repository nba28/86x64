/* 99_cfstring_range_overrun — does an over-long CFRange still kill the process?
 *
 * WHY THIS EXISTS. Classic CoreFoundation never validated the CFRange handed to
 * CFStringGetCharacters, so asking for length+1 to pick up a NUL terminator was
 * a normal idiom in 32-bit-era code and worked for the entire supported life of
 * those binaries. Modern CoreFoundation validates — but only in SOME of its
 * concrete string classes:
 *
 *     -[NSTaggedPointerString getCharacters:range:]  -> _CFThrowFormattedException
 *     __NSCFString                                   -> tolerated silently
 *
 * The throw is an ObjC exception crossing a C frame in a binary compiled in
 * 2006, which has no handler for it, so it reaches _objc_terminate and ABORTS
 * THE PROCESS. Measured on Civilization IV: four such calls on every launch of
 * the game proper, all asking for exactly length+1, and the ONLY fatal one is
 * the 6-character 'USER32' — short enough for CF to back it with a tagged
 * pointer. The 12-character 'kernel32.dll' on the same code path is harmless.
 *
 * ★That length-dependence is why this guard uses a SHORT string. A longer one
 * would be backed by __NSCFString, the OFF arm would not throw, the two arms
 * would not differ, and the guard would silently stop testing anything.
 *
 * src/abiconv/cfstring_range_shim.c clamps the copy to the characters that
 * actually exist and zero-fills the remainder of the caller's buffer — a
 * DETERMINISTIC terminator, which is better than the garbage classic CF copied.
 *
 * ARMS. This fixture is the ON side and must exit 0 having survived all three
 * shapes. cfstring_range_test.sh adds the OFF side by re-running this same
 * binary under M64_NO_CFSTRING_RANGE_CLAMP=1, where case 1 must ABORT — if it
 * survives there too, the clamp is not what is keeping the app alive and the
 * guard says so.
 *
 * Prints one key=value per line so the arms can be diffed mechanically.
 */
/* The i386 sysroot stages the CoreFoundation LIBRARY but not its headers (see
 * `make sysroot-objc`), so CF is hand-declared here exactly as the other CF
 * fixtures do. Note CFIndex is a 4-byte `long` on i386, which is why the
 * by-value CFRange occupies TWO cdecl stack slots on the way into the shim. */
extern int  printf(const char *, ...);
extern void exit(int status);

typedef const void      *CFStringRef;
typedef const void      *CFAllocatorRef;
typedef unsigned char    Boolean;
typedef long             CFIndex;
typedef unsigned int     CFStringEncoding;
typedef unsigned short   UniChar;
struct CFRange { CFIndex location, length; };
typedef struct CFRange   CFRange;

#define kCFStringEncodingUTF8 0x08000100u

extern CFStringRef CFStringCreateWithCString(CFAllocatorRef, const char *,
                                             CFStringEncoding);
extern CFIndex     CFStringGetLength(CFStringRef);
extern void        CFStringGetCharacters(CFStringRef, CFRange, UniChar *);
extern void        CFRelease(CFStringRef);

static CFRange mkrange(CFIndex loc, CFIndex len)
{
   CFRange r; r.location = loc; r.length = len; return r;
}

#define CAP 128
#define SENTINEL 0xAAAA

/* 6 chars: short enough that modern CF backs it with a tagged pointer, which is
 * the only class that actually throws. Same length as Civ's fatal 'USER32'. */
static const char *kShort = "USER32";
#define SHORT_LEN 6

static void fill(UniChar *b) { for (int i = 0; i < CAP; i++) b[i] = SENTINEL; }

/* Every character the string really has must be copied verbatim... */
static int chars_ok(const UniChar *b, const char *expect, int n)
{
   for (int i = 0; i < n; i++)
      if (b[i] != (UniChar)expect[i]) return 0;
   return 1;
}

/* ...and every character past the end must be a deterministic 0, not the
 * 0xAAAA sentinel (which would mean the shim left the tail untouched) and not
 * whatever happened to follow the string in memory. */
static int zeros_ok(const UniChar *b, int from, int to)
{
   for (int i = from; i < to; i++)
      if (b[i] != 0) return 0;
   return 1;
}

int main(void)
{
   UniChar buf[CAP];
   CFStringRef s = CFStringCreateWithCString(0, kShort, kCFStringEncodingUTF8);
   if (!s) { printf("create=0\n"); exit(1); }
   printf("create=1\n");
   printf("len=%ld\n", (long)CFStringGetLength(s));

   /* [1] THE CIV SHAPE: ask for length+1 to pick up a terminator. Under
    * M64_NO_CFSTRING_RANGE_CLAMP=1 the process dies right here. */
   fill(buf);
   CFStringGetCharacters(s, mkrange(0, SHORT_LEN + 1), buf);
   printf("c1_survived=1\n");
   printf("c1_chars=%d\n", chars_ok(buf, kShort, SHORT_LEN));
   printf("c1_term=%d\n", zeros_ok(buf, SHORT_LEN, SHORT_LEN + 1));

   /* [2] a PARTIAL overrun starting mid-string: 4 real characters remain, the
    * other 96 requested must come back zeroed. Exercises the clamp's
    * nonzero-location branch. */
   fill(buf);
   CFStringGetCharacters(s, mkrange(2, 100), buf);
   printf("c2_chars=%d\n", chars_ok(buf, kShort + 2, SHORT_LEN - 2));
   printf("c2_zeros=%d\n", zeros_ok(buf, SHORT_LEN - 2, 100));

   /* [3] a range starting WHOLLY past the end: nothing to copy, all zeros.
    * Exercises the clamp's location > length branch. */
   fill(buf);
   CFStringGetCharacters(s, mkrange(50, 4), buf);
   printf("c3_zeros=%d\n", zeros_ok(buf, 0, 4));

   CFRelease(s);
   printf("done=1\n");
   exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
