/* 99_translated_provider_defer — when a TRANSLATED image already provides the
 * symbol, does a by-name lookup return THAT, or does it hand back libabiconv's
 * legacy bridge and double-convert the ABI?
 *
 * WHY THIS EXISTS (measured on Civilization IV, 2026-08-08).
 *   SIGSEGV KERN_INVALID_ADDRESS at 0x11
 *     #0  ?+0x100a338c8                          (native-convention call debris)
 *     #1  libabiconv+0x22db86  ___Py_Initialize+0x33     <- OUR abigen bridge
 *
 * An abigen bridge `___X` exists to reach a NATIVE `_X`: it is an i386-callable
 * entry that lifts the i386 cdecl frame to the x86_64 SysV ABI and `call`s out.
 *   ___PyInt_AsLong.l1: mov edi,[rbp+0xc]   ; i386 4-byte stack slot -> SysV reg
 *                       call _PyInt_AsLong  ; pushes an 8-BYTE return address
 *                       mov r11d,[rsp]; add rsp,4; jmp r11    ; i386 4-byte ret
 * Apple removed Python 2 in macOS 12.3, so no native `_Py_Initialize` is left;
 * the bundle vendors our OWN TRANSLATED Python 2.6 instead. Translated code
 * keeps the i386 convention, so the bridge's native-convention call hands it
 * registers it never reads and a return address twice the width it pops. The
 * bridge is not redundant there — it is WRONG. A DOUBLE CONVERSION.
 *
 * ⚠The bind tables look perfect. `Civilization IV.dylib` lazy-binds
 * `Python/_Py_Initialize` correctly and static-interpose never redirected it
 * (`_Py_Initialize` is in libabiconv.nulljump, so the exclusion already held).
 * NOTHING in the bundle binds `___Py_Initialize`. The detour is minted at RUN
 * TIME by the by-name lookup family — Civ imports the classic trio
 * `___NSIsSymbolNameDefined` / `___NSLookupAndBindSymbol` / `___NSAddressOfSymbol`
 * — whose resolver PREFERS libabiconv's interpose shim. That preference is the
 * documented fix for the raw-thunk defects (shim_dlsym c62c908,
 * ns_symbol_resolve 632b1e5, the CFBundle twin b30ccd9); this is its MIRROR
 * IMAGE: when the provider is itself translated, the resolver must DEFER.
 *
 * THIS FIXTURE reproduces the shape with the smallest possible parts. It IS the
 * translated provider: it defines and exports `_PyInt_AsLong` (a name libabiconv
 * carries a bridge for, and which no native library on this OS defines), then
 * asks for it BY NAME through the same classic trio Civ uses and calls whatever
 * comes back.
 *
 *   ON  (fixed)    the resolver sees a TRANSLATED image defining the symbol,
 *                  suppresses the bridge, and returns this file's own function.
 *                  A translated-to-translated call: 21 -> 42.
 *   OFF (kill switch M64_NO_TRANSLATED_PROVIDER_DEFER=1) the resolver returns
 *                  ___PyInt_AsLong, we call the bridge with the i386 frame, the
 *                  bridge re-enters this very function through its
 *                  <flat-namespace>/_PyInt_AsLong lazy bind with the NATIVE
 *                  convention -> the callee reads a stack slot that holds no
 *                  argument. The double conversion, executed.
 *
 * The OFF arm is driven by translated_provider_defer_test.sh, which re-runs this
 * same binary with the kill switch and REQUIRES it to disagree. A green ON arm
 * on its own would prove nothing.
 */
extern int  printf(const char *, ...);
extern void exit(int);

/* The classic pre-dlopen dynamic-loader trio, declared by hand: modern
 * <mach-o/dyld.h> marks them unavailable. Linked -undefined dynamic_lookup, so
 * static-interpose binds each to libabiconv's shim at translate time — exactly
 * how Civilization IV.dylib reaches them. */
typedef void *NSSymbol;
extern unsigned char NSIsSymbolNameDefined(const char *symbolName);
extern NSSymbol      NSLookupAndBindSymbol(const char *symbolName);
extern void         *NSAddressOfSymbol(NSSymbol symbol);

/* The vendored-framework stand-in. Ordinary translated i386 code: its argument
 * arrives in a 4-byte stack slot and it returns with the i386 4-byte `ret`.
 * `volatile` + the odd multiplier keep it from being folded away. */
int PyInt_AsLong(int v)
{
   volatile int x = v;
   return x * 2;
}

int main(void)
{
   int direct = PyInt_AsLong(21);      /* the honest translated-to-translated call */
   printf("direct=%d\n", direct);

   int defined = NSIsSymbolNameDefined("_PyInt_AsLong") ? 1 : 0;
   printf("defined=%d\n", defined);

   NSSymbol s = NSLookupAndBindSymbol("_PyInt_AsLong");
   void *p = s ? NSAddressOfSymbol(s) : 0;
   printf("bound=%d\n", p != 0 ? 1 : 0);

   /* THE ASSERTION. Deferral means the by-name lookup hands back the
    * translated provider itself, not a bridge in front of it. */
   printf("is_provider=%d\n", p == (void *)PyInt_AsLong ? 1 : 0);

   int r = -1;
   if (p) {
      int (*fn)(int) = (int (*)(int))p;
      r = fn(21);                      /* OFF arm: this goes through the bridge */
   }
   printf("byname=%d\n", r);

   /* ★ THE NORMAL CASE MUST SURVIVE, and it is asserted in the SAME PROCESS in
    * which deferral just fired. `tolower` has a libabiconv bridge (___tolower)
    * and NO translated provider, so the by-name lookup must still hand back the
    * bridge and that bridge must still work. If the deferral had been written
    * as a blanket "stop bridging", this line would break — and every target
    * that depends on the legacy-shim mechanism with it. Both arms must print 1:
    * the fix is invisible here BY CONSTRUCTION, which is the point. */
   NSSymbol ts = NSLookupAndBindSymbol("_tolower");
   void *tp = ts ? NSAddressOfSymbol(ts) : 0;
   int kept = 0;
   if (tp) {
      int (*tf)(int) = (int (*)(int))tp;
      kept = (tf('A') == 'a' && tf('z') == 'z');
   }
   printf("kept_bridge=%d\n", kept);

   exit(r == 42 && p == (void *)PyInt_AsLong && kept ? 0 : 1);
}
