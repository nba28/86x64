/* dyld_multicopy_fixture.c — fixture for dyld_multicopy_test.sh (NOT a numbered
 * suite test: it needs ABICONV_RUN_INITS=1 in the launch env, which the generic
 * suite runner does not set).
 *
 * Guards the multi-libabiconv-copy __dyld+8 artifact fix (objc_slide.c
 * wrap_mod_init_funcs): patch_dyld_section stamps the i386-cdecl
 * _86x64_dyld_func_lookup shim into the 8-BYTE view of the classic
 * __DATA,__dyld func_lookup slot (__dyld+8), which under the classic Csu
 * layout IS __mod_init_func[0]. A bundle can carry >1 libabiconv copy (Halo CE:
 * Contents/MacOS + inside the translated QuickTime.framework); each loaded copy
 * re-scans every image (the processed-set is per-copy), finds that slot
 * non-NULL again — holding the first copy's shim, a low-4GB value the >4GB
 * clobber-recovery can't flag — and collects+runs it as an initializer with an
 * argc=0/argv=NULL i386 frame: the shim writes its noop fn-ptr through *NULL ->
 * SIGSEGV inside dlopen. The fix skips a mod-init entry that dladdr-resolves
 * exactly to an exported _86x64_dyld_func_lookup entry outside the image.
 *
 * Modern ld cannot reproduce the classic Csu adjacency (__nl_symbol_ptr lands
 * between __dyld and __mod_init_func), so the fixture recreates the artifact
 * STATE through the same runtime paths instead: patch_dyld_section stamps the
 * real shim address into our classic-placeholder g_dyld[1] at load; main copies
 * that value into a properly-TYPED __mod_init_func slot (initially NULL so the
 * first pass has nothing to run — the suite's -no_pie inputs carry no rebase
 * info, so a real ctor entry would be collected unslid, an unrelated pre-
 * existing gap); then dlopen()s a second libabiconv copy, whose add-image
 * re-scan hits the artifact exactly as Halo's did. Unfixed: SIGSEGV inside
 * dlopen. Fixed: the entry is skipped and every line below prints. */

extern int printf(const char *, ...);
extern void exit(int);
extern void *dlopen(const char *, int);
extern char *dlerror(void);
extern char *getenv(const char *);

/* Classic crt __DATA,__dyld slots with the i386 placeholder values — the
 * structural trigger for patch_dyld_section (same fixture as test 33). */
__attribute__((used, section("__DATA,__dyld")))
static volatile unsigned int g_dyld[2] = { 0x8fe01000u, 0x8fe01008u };

/* One NULL entry in a properly-typed S_MOD_INIT_FUNC_POINTERS section (the
 * plain section attribute would produce S_REGULAR, which wrap_mod_init_funcs
 * ignores). NULL entries are skipped by the collector, so nothing runs from
 * here at load. */
/* SL ld64-95 rejects a symbol inside __mod_init_func, so the slot is an
 * assembler-local label reached through a pointer in __data. */
__asm__(".section __DATA,__mod_init_func,mod_init_funcs\n"
        ".p2align 2\n"
        "Lextra_slot:\n"
        ".long 0\n"
        ".data\n"
        ".globl _g_extra_slot_p\n"
        ".p2align 2\n"
        "_g_extra_slot_p:\n"
        ".long Lextra_slot\n");
extern void (*volatile *g_extra_slot_p)(void);
#define g_extra_slot (*g_extra_slot_p)

int main(void) {
   unsigned int shim = g_dyld[1];
   int patched = shim != 0x8fe01008u && shim != 0u;
   printf("dyld patched: %s\n", patched ? "yes" : "no");

   /* Recreate patch_dyld_section's __dyld+8 artifact: the func_lookup shim
    * address sitting in a __mod_init_func slot (a 4-byte i386 store into the
    * widened 8-byte slot — exactly the value shape the overlap leaves). */
   g_extra_slot = (void (*)(void))(unsigned long)shim;

   /* Load a SECOND libabiconv copy (distinct inode = distinct image). Its
    * add-image callback re-scans this image's __mod_init_func; unfixed it
    * collects the artifact and runs it -> *NULL write, never returns here. */
   const char *path = getenv("DYLD_MULTICOPY_LIB");
   if (path == 0) { path = "build/libabiconv_copy2.dylib"; }
   void *h = dlopen(path, 1 /* RTLD_LAZY */);
   if (h == 0) { printf("dlerror: %s\n", dlerror()); }
   printf("copy2: %s\n", h != 0 ? "loaded" : "failed");
   printf("slot: %s\n",
          (unsigned long)g_extra_slot == (unsigned long)shim
             ? "artifact-skipped" : "modified");
   exit(0);
   return 0;
}
