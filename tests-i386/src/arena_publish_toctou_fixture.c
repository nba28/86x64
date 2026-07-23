/* arena_publish_toctou_fixture.c — fixture for arena_publish_toctou_test.sh.
 *
 * COMPANION to dyld_multicopy_reprocess_fixture.c. That fixture guards the
 * cross-copy per-image CLAIM (proc_img) GIVEN one shared ctrl. THIS fixture
 * guards the step BEFORE it: that arena_init produces EXACTLY ONE shared ctrl
 * across all libabiconv copies, even when a second copy's publish-adopt races
 * the first copy's publish.
 *
 * THE RESIDUAL BUG (rc=139, Civ IV ~1/5 startup crash after the proc_img fix):
 * arena_init (objc_shim.c) publishes the shared objc_shared_ctrl via
 * setenv(OBJC_CTRL_ENV,&ctrl) and later copies adopt it via getenv — a
 * setenv/getenv TOCTOU. arena_init is NOT ctor-only: it is reached at message
 * time from objc_bridge_prep{,_stret,_super,_super_stret} (any thread) and has
 * NO lock around its check-then-act create path. If a second copy's arena_init
 * runs its getenv before the first copy's setenv is visible, BOTH copies decide
 * "I am first" and build SEPARATE ctrls with SEPARATE proc_img tables. The
 * cross-copy image claim then dedups against the WRONG table -> both copies
 * "win" the same image -> both RUN its static initializers -> a same-image
 * intrusive-list registry (Civ's FConsole command tree) is spliced twice -> a
 * link field reads back NULL and is dereferenced at +0x88/+0x8c (the crash).
 *
 * THE FIX (Option A): replace the setenv/getenv publish with a fixed low-4GB
 * rendezvous word + atomic compare_exchange. The first copy to CAS wins and
 * installs; every other copy atomic_loads the winner. No env, no visibility
 * gap -> always exactly one shared ctrl.
 *
 * This fixture is trivial (a translated i386 binary can make no direct
 * libabiconv call): it just dlopen()s a SECOND libabiconv copy so that copy's
 * ctor runs arena_init. The harness A/Bs via the (coordinator-added)
 * ABICONV_ARENA_TRACE `[arena] create|attach <addr>` trace:
 *   - POST-FIX (default): exactly ONE `[arena] create` across all copies.
 *   - PRE-FIX arm (ABICONV_ARENA_PUBLISH_GAP=1 makes the first copy's create
 *     path STAGE-but-not-publish, modeling the unseen setenv): the second copy
 *     also creates -> TWO `[arena] create` lines (the duplicate-ctrl root).
 * If the ABICONV_ARENA_TRACE hook is absent (fix/hook not yet in the tree), the
 * harness SKIPs.
 */

extern int printf(const char *, ...);
extern void exit(int);
extern void *dlopen(const char *, int);
extern char *dlerror(void);
extern char *getenv(const char *);

int main(void) {
   const char *path = getenv("DYLD_MULTICOPY_LIB");
   if (path == 0) { path = "build/libabiconv_copy2.dylib"; }
   void *h = dlopen(path, 1 /* RTLD_LAZY */);
   if (h == 0) { printf("dlerror: %s\n", dlerror()); }
   printf("copy2: %s\n", h != 0 ? "loaded" : "failed");
   exit(0);
   return 0;
}
