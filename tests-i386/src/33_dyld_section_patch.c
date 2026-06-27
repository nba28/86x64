/* 33_dyld_section_patch.c — regression guard for the classic __DATA,__dyld
 * crt-bootstrap neutralization (libabiconv: objc_slide.c patch_dyld_section +
 * dyld_func_lookup.asm).
 *
 * Pre-10.5 i386 binaries (Halo CE, Civ IV) carry a __DATA,__dyld section whose
 * two 4-byte slots ([+0]=lazy-symbol-binder, [+4]=dyld_func_lookup) ship as the
 * i386 dyld placeholders 0x8fe01000 / 0x8fe01008. The host dyld points the
 * func_lookup slot at native legacyDyldLookup4OldBinaries, which reads its args
 * from registers (x86_64 ABI) while the i386 crt passed them on the stack ->
 * NULL write / SIGSEGV. libabiconv's add-image callback must instead overwrite
 * those slots with the <4GB i386-cdecl shims before the image's code runs.
 *
 * The host dyld won't treat this modern-built test as legacy, so we assert the
 * STRUCTURAL fix rather than the live crt jump: a translated image carrying a
 * __DATA,__dyld section has its placeholder slots rewritten, in-process, to
 * non-placeholder <4GB addresses. Without the fix the slots keep the i386
 * placeholders and the check reports "no". The section detection is the whole
 * universal trigger, so this guards the load-bearing behavior. */

extern int  printf(const char *, ...);
extern void exit(int);

/* A classic-style __DATA,__dyld section: two 4-byte slots holding the i386 dyld
 * placeholder addresses, exactly as old crt1.o emits them. `volatile` so the
 * reads in main aren't constant-folded against the static initializer (the
 * add-image callback patches them at load time). */
__attribute__((used, section("__DATA,__dyld")))
static volatile unsigned int g_dyld[2] = { 0x8fe01000u, 0x8fe01008u };

int main(void) {
   unsigned int lazy   = g_dyld[0];   /* [+0] lazy-symbol-binder slot   */
   unsigned int lookup = g_dyld[1];   /* [+4] dyld_func_lookup slot      */

   /* Patched iff both slots were moved off the i386 placeholders to real,
    * 32-bit-representable (<4GB) libabiconv shim addresses. */
   int patched = lookup != 0x8fe01008u && lookup != 0u
              && lazy   != 0x8fe01000u && lazy   != 0u;

   printf("dyld __dyld patched: %s\n", patched ? "yes" : "no");
   exit(0);
   return 0;
}
