/* 48_objc_cfstr_cfunc_arg.m — CF-object-identity: an i386 CFConstantString
 * constant passed as a CFStringRef arg to a NATIVE CoreFoundation C function.
 *
 * In a real translated target the constant @"..."/CFSTR("...") record has its
 * isa xrel-bound to the low-4GB WRAPPED proxy handle of
 * ___CFConstantStringClassReference (data-shadow). Passing it RAW to a native
 * CF C function makes CF read that handle as the object's Class -> "Attempt to
 * use unknown class 0x800xxxxx" / __CF_IS_OBJC __builtin_trap -> SIGSEGV. This
 * is the Halo `CFURLCreateCopyAppendingPathComponent` wall (Crash #8) and the
 * Civ IV `__CFStringCreateCopy` -> __CF_IS_OBJC wall.
 *
 * The objc_msgSend forward bridge already resolves such constants (via the full
 * unwrap_obj_arg -> i386_cfstr_to_real), but the abigen-generated C-function
 * shims unwrapped their CFStringRef/objc-object args with the ARENA-ONLY weak
 * x64_objc_unwrap, which passes a non-arena constant straight through. The fix
 * routes the C-bridge object/CF-ref arg unwrap through the SAME full resolver.
 *
 * Repro of the EXACT structural condition without depending on the data-shadow
 * machinery: a hand-built i386 16-byte CFConstantString record
 *   { uint32_t isa; uint32_t flags(0x7c8); const char *cstr; uint32_t length; }
 * with a deliberately-INVALID isa (not a real Class), passed to CFStringGetLength.
 *  - WITHOUT the fix: the weak unwrap passes the record raw; native
 *    CFStringGetLength reads the bogus isa -> crash (or garbage length).
 *  - WITH the fix: i386_cfstr_to_real recognises the record by flags+exact
 *    strlen (NEVER the isa), converts it to a real immortal NSString, and
 *    CFStringGetLength returns the true length.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* i386 CFConstantString layout: four 4-byte fields. Built in __DATA (NOT
 * __cfstring), so the translator leaves it 16 bytes (no CFStringBlob expand).
 * flags == 0x7c8 marks an 8-bit ASCII constant (CFSTR_FLAGS_ASCII8). */
struct cfconst_i386 {
   uint32_t    isa;       /* deliberately invalid (no real Class lives here) */
   uint32_t    flags;     /* 0x7c8 */
   const char *cstr;      /* 4-byte ptr in i386 -> the character bytes        */
   uint32_t    length;    /* byte count, must equal strlen(cstr)              */
};

static const char k_payload[] = "hello, identity";   /* length 15 */

static struct cfconst_i386 g_fake = {
   0x00bad15a,            /* bogus isa: unmapped, never a valid objc Class */
   0x7c8u,
   k_payload,
   15u
};

int main(void) {
   CFStringRef s = (CFStringRef)(void *)&g_fake;

   /* C-function shim: CFStringRef arg -> convert_cf_ptr -> object-arg unwrap. */
   CFIndex len = CFStringGetLength(s);
   printf("CFStringGetLength=%ld\n", (long)len);

   /* A second CF C-function on the same constant: CFStringCreateCopy is the
    * exact Civ IV wall. The copy must be a real, messageable string. */
   CFStringRef copy = CFStringCreateCopy(kCFAllocatorDefault, s);
   printf("copy.len=%ld\n", copy ? (long)CFStringGetLength(copy) : -1);

   puts("done");
   /* exit(0), not `return`: the translated-binary return-from-main path is a
    * separate known wrapper gap (rc=0x1 / rip=1) unrelated to this CF test;
    * exit() also flushes stdout so the run is captured (mirrors 21_objc_c_funcs). */
   exit(0);
}
