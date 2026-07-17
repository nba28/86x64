/* dyld_multicopy_reprocess_fixture.c — fixture for
 * dyld_multicopy_reprocess_test.sh (NOT a numbered suite test: it needs
 * ABICONV_RUN_INITS=1 and a second-copy dlopen the generic runner doesn't set).
 *
 * Guards the CROSS-COPY slide_objc per-image processing claim
 * (objc_shim.c objc_shared_ctrl.proc_img + _86x64_objc_shared_claim_image,
 * consulted at the top of objc_slide.c slide_objc).
 *
 * THE BUG (the general case a501466's zerofill skip only partially cured):
 * a bundle co-locates one libabiconv per translated binary/framework
 * (the libabiconv multi-copy gotcha; Civ IV carries 9 copies). dyld runs EACH
 * copy's constructor, and each ctor calls
 * _dyld_register_func_for_add_image(&slide_objc), which fires slide_objc
 * SYNCHRONOUSLY for every already-mapped image. The first copy's processed-set
 * (g_processed) is PRIVATE to that copy, so the SECOND copy's registration
 * re-walks every image the first copy already slid / ObjC-registered / ran —
 * reading a pointer field the first copy left mid-set-up (or is writing on
 * another thread) back as NULL and dereferencing it at +offset: the intermittent
 * slide_objc `[rbx+0x88]` SIGSEGV (KERN at 0x88, rbx=0) that killed ~40% of Civ
 * launches before its version check even ran. The per-image work is
 * once-per-IMAGE, never once-per-copy.
 *
 * THE FIX: a process-global processed-set in the shared objc_shared_ctrl. The
 * first copy to reach an image WINS the claim and processes it; every later
 * copy's re-scan sees the claim and returns immediately. An image is processed
 * EXACTLY ONCE no matter how many copies register.
 *
 * This fixture is deliberately trivial (it makes NO libabiconv call — a
 * translated i386 binary cannot call a native libabiconv function without an
 * abigen ABI bridge): it just dlopen()s a SECOND libabiconv copy so that copy's
 * constructor re-fires slide_objc over this already-mapped image. The A/B is
 * observed by the harness from the env-gated `[xcopy] process|skip <image>`
 * trace slide_objc emits per (copy,image) decision:
 *   - POST-FIX (default): the second copy's re-scan is SKIPPED -> exactly ONE
 *     `[xcopy] process <this fixture>` line.
 *   - PRE-FIX arm (ABICONV_NO_XCOPY_CLAIM=1 disables the dedup): the second copy
 *     RE-PROCESSES -> TWO `process` lines for this fixture (the double-process
 *     that is the crash's root mechanism).
 * exit(0) like the sibling dyld_multicopy fixture (the harness reads via command
 * substitution, tolerant of the separate pre-existing multicopy teardown crash).
 */

extern int printf(const char *, ...);
extern void exit(int);
extern void *dlopen(const char *, int);
extern char *dlerror(void);
extern char *getenv(const char *);

int main(void) {
   /* Load a SECOND libabiconv copy (distinct inode = distinct image). Its
    * constructor registers its OWN add-image callback -> slide_objc re-fires
    * for every already-mapped image, including this one. Fixed: the cross-copy
    * claim makes it skip; unfixed: it re-processes. */
   const char *path = getenv("DYLD_MULTICOPY_LIB");
   if (path == 0) { path = "build/libabiconv_copy2.dylib"; }
   void *h = dlopen(path, 1 /* RTLD_LAZY */);
   if (h == 0) { printf("dlerror: %s\n", dlerror()); }
   printf("copy2: %s\n", h != 0 ? "loaded" : "failed");
   exit(0);
   return 0;
}
