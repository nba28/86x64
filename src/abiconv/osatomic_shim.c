/*
 * Hand-written i386->x86_64 shim for OSAtomicAdd32 / OSAtomicAdd32Barrier.
 *
 * abigen DOES generate a correct ABI shim for these now (commit 5fc6c8a added
 * libsystem_platform.dylib to ABICONV_SYM_SOURCES), which fixed the dominant
 * Thumbnailer-teardown crash where the translated i386 `call OSAtomicAdd32`
 * reached the native register-ABI function directly.
 *
 * A RESIDUAL intermittent crash remains: a reverse-bridged legacy class
 * (Thumbnailer) tears down and a generic refcount-Release helper
 * (translated fn 0x1006778a) does `OSAtomicAdd32(-1, &obj->refcount@8)`
 * BEFORE its `obj != nil` guard, with obj == a near-null garbage value (0x2c)
 * read out of a member-object ivar. The decrement then faults at obj+8 (0x34)
 * INSIDE the atomic, before the caller's nil check can save it.
 *
 * This override (replacing the abigen shim via custom.syms + the MTSHIM
 * trampoline) makes the primitive defensive AND self-diagnosing:
 *   - a legitimate call (address points into real i386 memory) is forwarded to
 *     the native OSAtomicAdd32 unchanged;
 *   - a call whose target ADDRESS is in the first 64 KiB (page-zero / garbage
 *     ivar) is treated as operating on a not-yet-zero refcount: we log the
 *     i386 caller + value so the exact translated call site / ivar can be
 *     identified WITHOUT a debugger (the bug is a Heisenbug — any lldb attach
 *     perturbs the garbage register/value), and return a non-zero result so a
 *     refcount-Release helper skips the dealloc deref and the object simply
 *     leaks rather than crashing during teardown.
 *
 * The near-null guard is generic: no real i386 object lives in page zero, so a
 * sub-64K atomic target can only be a nil-based member address — never a valid
 * refcount slot.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <libkern/OSAtomic.h>

/* Real native primitives (libabiconv's own calls are NOT static-interposed). */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#define OSATOMIC_BOGUS_LIMIT 0x10000u

static int g_osatomic_trace = -1;

static int osatomic_trace_on(void) {
   if (g_osatomic_trace < 0)
      g_osatomic_trace = getenv("ABICONV_OSATOMIC_TRACE") ? 1 : 0;
   return g_osatomic_trace;
}

/* i386 frame (MTSHIM): a = &args[0]; a[-1] = i386 return address.
 * OSAtomicAdd32(int32_t amount, volatile int32_t *address):
 *   a[0] = amount, a[1] = address (i386 pointer, low-4GB). */
static int32_t osatomic_add32_common(uint32_t *a, int barrier) {
   int32_t  amount = (int32_t)a[0];
   uint32_t addr32 = a[1];

   if (addr32 < OSATOMIC_BOGUS_LIMIT) {
      if (osatomic_trace_on()) {
         fprintf(stderr,
                 "[osatomic] BOGUS %s addr=0x%x amount=%d\n",
                 barrier ? "add32barrier" : "add32", addr32, amount);
         /* Scan the i386 stack window above this frame for return addresses
          * that land in a loaded image; print each as <image>+<file_offset>
          * (ASLR-independent) so the caller chain maps straight to the
          * static disasm. a[-1] is the return into the Release helper. */
         for (int i = -1; i < 48; i++) {
            uint32_t v = a[i];
            if (v < 0x1000) continue;
            Dl_info di;
            if (dladdr((void *)(uintptr_t)v, &di) && di.dli_fname) {
               const char *base = di.dli_fname;
               for (const char *p = di.dli_fname; *p; p++)
                  if (*p == '/') base = p + 1;
               unsigned long off = (uintptr_t)v - (uintptr_t)di.dli_fbase;
               fprintf(stderr, "[osatomic]   stk[%+d]=0x%x -> %s+0x%lx\n",
                       i, v, base, off);
            }
         }
      }
      /* Pretend the refcount did not reach zero -> caller skips dealloc. */
      return amount ? amount : 1;
   }

   volatile int32_t *addr = (volatile int32_t *)(uintptr_t)addr32;
   return barrier ? OSAtomicAdd32Barrier(amount, addr)
                  : OSAtomicAdd32(amount, addr);
}

int32_t shim_OSAtomicAdd32(uint32_t *a)        { return osatomic_add32_common(a, 0); }
int32_t shim_OSAtomicAdd32Barrier(uint32_t *a) { return osatomic_add32_common(a, 1); }

/* Runtime breadcrumb for the libgcc_shim.asm 64-bit helpers, gated by
 * ABICONV_LIBGCC_TRACE. Used to diagnose Portal 2's libtier0 CalculateCPUFreq=0
 * (the FP/loop-context bug that no freestanding repro reproduced): logs the
 * ___udivdi3 quotient (the eventual divsd DIVISOR, via fildll) and the
 * ___fixunsdfdi INPUT double (the final freq before truncation = the divsd
 * RESULT). If the quotient is 0 -> the dividend / loop accumulation is wrong; if
 * the fixunsdfdi input is ~0.0 while the quotient is sane -> the SSE2 numerator
 * / divsd is wrong. Inert (one getenv) when the env var is unset. which: 0 =
 * udivdi3 (a=dividend, b=divisor, result=quotient); 1 = fixunsdfdi (a=input
 * double bits, result=int64 out). */
/* Zero-divisor breadcrumb for the 64-bit integer div/mod helpers. Called from
 * libgcc_shim.asm just before the faulting `div`/`idiv` WHEN the divisor is 0,
 * so a would-be #DE becomes a diagnosable log line identifying the i386 caller
 * (ret = the 4-byte return address at [rsp] on entry, i.e. the site in the
 * translated image + a Halo.dylib load-base). Gated by ABICONV_LIBGCC_TRACE.
 * which: 0=udivdi3 1=umoddi3 2=divdi3 3=moddi3. */
void abiconv_libgcc_divzero(uint64_t a, uint64_t ret, uint64_t which) {
   static int on = -1;
   if (on < 0) { on = getenv("ABICONV_LIBGCC_TRACE") ? 1 : 0; }
   if (!on) { return; }
   static const char *nm[4] = { "udivdi3", "umoddi3", "divdi3", "moddi3" };
   fprintf(stderr, "[libgcc] DIV-BY-ZERO in %s: dividend=0x%llx i386_ret=0x%llx\n",
           nm[which & 3], (unsigned long long)a, (unsigned long long)ret);
}

void abiconv_libgcc_log(uint64_t a, uint64_t b, uint64_t result, uint64_t which) {
   static int on = -1;
   if (on < 0) { on = getenv("ABICONV_LIBGCC_TRACE") ? 1 : 0; }
   if (!on) { return; }
   if (which == 0) {
      fprintf(stderr, "[libgcc] udivdi3 a=%llu b=%llu -> q=%llu\n",
              (unsigned long long)a, (unsigned long long)b,
              (unsigned long long)result);
   } else {
      double d;
      memcpy(&d, &a, sizeof d);
      fprintf(stderr, "[libgcc] fixunsdfdi in=%.6f (bits=0x%llx) -> %llu\n",
              d, (unsigned long long)a, (unsigned long long)result);
   }
}

#pragma clang diagnostic pop
