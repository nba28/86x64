/* 99_dladdr_shim — does a translated i386 `call dladdr` get a correctly shaped
 * Dl_info back, or the native 64-bit record smeared over a 16-byte struct?
 *
 * WHY THIS EXISTS (measured on Civilization IV, 2026-08-09/10).
 * libabiconv hand-shims dlopen/dlsym/dlclose/dlerror and, until now, NOT dladdr.
 * With no `___dladdr` for static-interpose to redirect to, a translated image's
 * `_dladdr` bind stayed on NATIVE libSystem, and the i386 caller's 4-byte stack
 * arguments were never where the native callee looks. Under lldb, at the fault:
 *     frame #0  dyld`dyld4::APIs::dladdr + 635
 *     ->  movq 0x8(%rbx), %rcx     rbx = 0x4e   (0x4e + 8 = 0x56 = fault addr)
 *         rdi = 0x8aee8378 (a stack address, read as `addr`)
 *         rsi = 0x8        (read as `Dl_info *`)
 * CPython 2.6 calls dladdr while starting up, so this landed inside
 * Py_Initialize and looked for a long time like a Python-bridge defect.
 * `bt 40` gave only frame #0 — not even lldb can unwind out to a translated
 * 4-byte frame — so the ARGUMENTS, not the backtrace, identified it.
 *
 * ⚠THE RECORD, not just the call, has to be converted. Dl_info is four pointer
 * fields: 16 bytes on i386, 32 on x86_64.
 *     i386   +0 dli_fname  +4 dli_fbase  +8 dli_sname  +12 dli_saddr
 *     x86_64 +0 dli_fname  +8 dli_fbase +16 dli_sname  +24 dli_saddr
 * So a shim that merely forwards would overrun the caller's buffer by 16 bytes
 * AND put every field at the wrong offset — `dli_fbase` would be read out of the
 * HIGH HALF of `dli_fname`. That is what the OFF arm reproduces.
 *
 * ARMS (same translated binary, run twice — dladdr_shim_test.sh drives the OFF
 * side):
 *   ON  : ret=1, fbase_ok=1, fname_ok=1 — a real low-4GB image base at or below
 *         this function, and a readable bounced path string.
 *   OFF : M64_NO_DLADDR_MARSHAL=1 copies the native record verbatim, so
 *         dli_fbase comes from the high half of a >4GB char* -> fbase_ok=0.
 *
 * The deref of dli_fname is deliberately gated on fbase_ok: in the OFF arm that
 * pointer is the truncated low half of a native address and dereferencing it
 * would just crash, which proves less and reads worse than a clean 0.
 */
extern int  printf(const char *, ...);
extern void exit(int);
extern unsigned long strlen(const char *);

/* The i386 Dl_info: four 4-byte fields. `pad` absorbs the 16-byte overrun the
 * OFF arm deliberately performs, so the kill-switch arm cannot corrupt the
 * frame and turn a measurement into a crash. */
struct dl_info32 {
   unsigned int dli_fname;
   unsigned int dli_fbase;
   unsigned int dli_sname;
   unsigned int dli_saddr;
   unsigned int pad[8];
};

extern int dladdr(const void *addr, struct dl_info32 *info);

/* The address we ask about. Must be a real function in this image. */
int marker(void) { return 7; }

int main(void)
{
   struct dl_info32 di;
   unsigned int i;
   for (i = 0; i < 12; i++) { ((unsigned int *)&di)[i] = 0; }

   int r = dladdr((const void *)&marker, &di);
   printf("ret=%d\n", r != 0 ? 1 : 0);

   /* ★THE DISCRIMINATOR: dli_saddr. dladdr sets it to the address of the symbol
    * it resolved, so correct marshalling makes it EXACTLY &marker. In the
    * unmarshalled record that slot (+12) holds the HIGH half of the native
    * dli_fbase, and this image is mapped in the low 4GB, so it is 0.
    * Deterministic, deref-free, and independent of where we happen to load.
    *
    * ⚠Two earlier versions of this check were WRONG, both in the direction that
    * makes a guard lie:
    *   - "fbase != 0 && fbase <= &marker" was satisfied by the broken arm too
    *     (that slot holds a small integer like 1 or 0x7ff8), so BOTH arms passed;
    *   - adding "fbase >= 0x1000000" then broke the CORRECT arm, because this
    *     fixture actually loads near 0x5d0000, well under 16MB. Both arms failed.
    * An absolute-address bound was the wrong idea twice. Compare against a value
    * the test already knows instead. */
   int saddr_ok = (di.dli_saddr == (unsigned int)(unsigned long)&marker);
   printf("saddr_ok=%d\n", saddr_ok);

   /* A real image base is non-zero and at or below the symbol it contains. */
   int fbase_ok = (di.dli_fbase != 0 && di.dli_fbase <= di.dli_saddr && saddr_ok);
   printf("fbase_ok=%d\n", fbase_ok);

   /* Gated on saddr_ok so the broken arm never dereferences the truncated low
    * half of a native char* — that would just crash, which proves less. */
   int fname_ok = 0;
   if (saddr_ok && di.dli_fname != 0) {
      const char *p = (const char *)(unsigned long)di.dli_fname;
      fname_ok = strlen(p) > 0 ? 1 : 0;
   }
   printf("fname_ok=%d\n", fname_ok);

   exit((r != 0 && saddr_ok && fbase_ok && fname_ok) ? 0 : 1);
}
