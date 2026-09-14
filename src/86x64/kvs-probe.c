/*
 * kvs-probe.c — watch the runtime vtable of translated libvstdlib's
 * g_KeyValuesSystem singleton.
 *
 * WHY. Portal 2's KeyValues::MakeCopy (soundemittersystem) does
 *   eax = KeyValuesSystem(); ecx = [eax]; ecx = [ecx+0x24]; call ecx
 * and the call target came out as a MID-INSTRUCTION address inside
 * soundemittersystem's own __text, which turned into a 4-million-iteration
 * self-loop that ate the whole 16 MB i386 stack. The static vtable in the
 * translated libvstdlib is correct (slot +0x2c = GetSymbolForStringCaseSensitive),
 * so the question is what the RUNTIME object holds — is the vptr right, is the
 * slot right, and does either change after it is first written?
 *
 * Addresses are the file's unslid vmaddrs plus the image's real slide.
 *
 * Build: clang -arch x86_64 -dynamiclib -o /tmp/kvs-probe.dylib kvs-probe.c
 * Use:   run-portal2.sh --insert /tmp/kvs-probe.dylib
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <mach-o/dyld.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdlib.h>

/* From the translated bin/osx64/libvstdlib.dylib (unslid vmaddrs):
 *   _KeyValuesSystem returns &g_KeyValuesSystem                       */
#define G_KVS          0x1006a254u   /* __DATA,__bss                   */
#define VTABLE         0x100396a0u   /* __ZTV16CKeyValuesSystem        */
#define SLOT_GETSTRING 0x1001e50au   /* vptr+0x10 GetStringForSymbol   */
#define SLOT_GETSYMCS  0x1001e068u   /* vptr+0x24 GetSymbolForString…  */

#define P(...) do { fprintf(stderr, "[kvs] " __VA_ARGS__); fflush(stderr); } while (0)

/* soundemittersystem.dylib, translated: `mov ecx, dword ptr [rax]` (8b 08) —
 * the instruction that loads the vptr out of whatever KeyValuesSystem()
 * returned. Swap it for ud2 and the SIGILL handler can read rax: that is the
 * one value the crash destroys and no amount of static reading can supply. */
#define PATCH_OFF  0x18436u
#define PATCH_ORIG "\x8b\x08"
#define PATCH_UD2  "\x0f\x0b"

/* Two traps, both EMULATED rather than restored, so they stay armed for every
 * execution instead of sampling one call in a 25 ms window:
 *   A  0x18436  mov ecx,[rax]        (2 bytes)  — load the vptr
 *   B  0x18438  mov ecx,[rcx+0x24]   (3 bytes)  — load the virtual slot
 * Between them they show the exact target the following `call ecx` will use. */
#define TRAP_A_OFF 0x18436u
#define TRAP_B_OFF 0x18438u
/*   C  0x18460  jmp rcx              (2 bytes)  — the call itself. This is the
 *      one that matters: it is where a bad target actually leaves the rails,
 *      and freezing there keeps the stack (and the return address the call
 *      idiom just pushed) intact for reading. */
#define TRAP_C_OFF 0x18460u
/*   D  0x1843b  mov [rsp+8],edi      (4 bytes)  — EMULATED purely so that its
 *      interior byte 0x1843d is never executed as part of a real instruction,
 *      which frees that address to carry a trap of its own.
 *   E  0x1843d  the MID-INSTRUCTION address the runaway loops on. Nothing
 *      legitimate ever starts here, so any hit is the moment control first
 *      leaves the rails — with the stack still intact. */
#define TRAP_D_OFF 0x1843bu
#define TRAP_E_OFF 0x1843du

static volatile uintptr_t g_trap_a, g_trap_b, g_trap_c, g_trap_d, g_trap_e;
static volatile int       g_hits;
static uint32_t           g_expect_obj, g_expect_slot;

static int poke_ud2(uintptr_t at)
{
   void *page = (void *)(at & ~(uintptr_t)0xfff);
   if (mprotect(page, 0x2000, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) { return 0; }
   ((volatile unsigned char *)at)[0] = 0x0f;
   ((volatile unsigned char *)at)[1] = 0x0b;
   mprotect(page, 0x2000, PROT_READ | PROT_EXEC);
   return 1;
}

static void on_sigill(int sig, siginfo_t *si, void *uctx)
{
   (void)sig; (void)si;
   ucontext_t *uc = (ucontext_t *)uctx;
   uintptr_t rip = (uintptr_t)uc->uc_mcontext->__ss.__rip;
   int n = ++g_hits;

   if (rip == g_trap_a) {                       /* mov ecx,[rax] */
      uint64_t rax = uc->uc_mcontext->__ss.__rax;
      uint32_t v = (rax && rax < 0x100000000ull)
                 ? *(const uint32_t *)(uintptr_t)rax : 0;
      if (n <= 400 || (uint32_t)rax != g_expect_obj)
         P("#%-4d A rax=%#010llx %s -> ecx=%#010x\n", n,
           (unsigned long long)rax,
           (uint32_t)rax == g_expect_obj ? "OK " : "BAD", v);
      uc->uc_mcontext->__ss.__rcx = v;
      uc->uc_mcontext->__ss.__rip = rip + 2;
      return;
   }
   if (rip == g_trap_b) {                       /* mov ecx,[rcx+0x24] */
      uint64_t rcx = uc->uc_mcontext->__ss.__rcx;
      uint32_t v = (rcx && rcx < 0x100000000ull)
                 ? *(const uint32_t *)(uintptr_t)(rcx + 0x24) : 0;
      if (n <= 400 || v != g_expect_slot)
         P("#%-4d B ecx=%#010llx -> target=%#010x %s\n", n,
           (unsigned long long)rcx, v,
           v == g_expect_slot ? "OK " : "BAD");
      uc->uc_mcontext->__ss.__rcx = v;
      uc->uc_mcontext->__ss.__rip = rip + 3;
      return;
   }
   if (rip == g_trap_c) {                       /* jmp rcx (the call) */
      uint64_t rcx = uc->uc_mcontext->__ss.__rcx;
      uint64_t rsp = uc->uc_mcontext->__ss.__rsp;
      if ((uint32_t)rcx == g_expect_slot) {
         uc->uc_mcontext->__ss.__rip = rcx;     /* emulate the jump */
         return;
      }
      P("=== BAD CALL TARGET ===\n");
      P("  ecx=%#010llx (expected %#010x)  rsp=%#010llx  rbp=%#010llx\n",
        (unsigned long long)rcx, g_expect_slot,
        (unsigned long long)rsp, (unsigned long long)uc->uc_mcontext->__ss.__rbp);
      P("  rax=%#010llx rbx=%#010llx rsi=%#010llx rdi=%#010llx\n",
        (unsigned long long)uc->uc_mcontext->__ss.__rax,
        (unsigned long long)uc->uc_mcontext->__ss.__rbx,
        (unsigned long long)uc->uc_mcontext->__ss.__rsi,
        (unsigned long long)uc->uc_mcontext->__ss.__rdi);
      const uint32_t *sp = (const uint32_t *)(uintptr_t)rsp;
      for (int i = 0; i < 16; i++)
         P("  [rsp+%2d] = %#010x\n", i * 4, sp[i]);
      uint32_t fp = (uint32_t)uc->uc_mcontext->__ss.__rbp;
      for (int i = 0; i < 12 && fp; i++) {
         const uint32_t *f = (const uint32_t *)(uintptr_t)fp;
         P("  frame #%-2d ebp=%#010x ret=%#010x\n", i, fp, f[1]);
         if (f[0] <= fp) { break; }
         fp = f[0];
      }
      _exit(93);
   }
   if (rip == g_trap_d) {                       /* mov [rsp+8], edi */
      uint64_t rsp = uc->uc_mcontext->__ss.__rsp;
      *(uint32_t *)(uintptr_t)(rsp + 8) = (uint32_t)uc->uc_mcontext->__ss.__rdi;
      uc->uc_mcontext->__ss.__rip = rip + 4;
      return;
   }
   if (rip == g_trap_e) {                       /* the mid-instruction landing */
      uint64_t rsp = uc->uc_mcontext->__ss.__rsp;
      P("=== CONTROL REACHED THE MID-INSTRUCTION ADDRESS %#lx ===\n",
        (unsigned long)rip);
      P("  ecx=%#010llx rax=%#010llx rbx=%#010llx rdx=%#010llx\n",
        (unsigned long long)uc->uc_mcontext->__ss.__rcx,
        (unsigned long long)uc->uc_mcontext->__ss.__rax,
        (unsigned long long)uc->uc_mcontext->__ss.__rbx,
        (unsigned long long)uc->uc_mcontext->__ss.__rdx);
      P("  rsp=%#010llx rbp=%#010llx r11=%#010llx\n",
        (unsigned long long)rsp,
        (unsigned long long)uc->uc_mcontext->__ss.__rbp,
        (unsigned long long)uc->uc_mcontext->__ss.__r11);
      const uint32_t *sp = (const uint32_t *)(uintptr_t)rsp;
      for (int i = 0; i < 12; i++)
         P("  [rsp+%2d] = %#010x\n", i * 4, sp[i]);
      uint32_t fp = (uint32_t)uc->uc_mcontext->__ss.__rbp;
      for (int i = 0; i < 10 && fp; i++) {
         const uint32_t *f = (const uint32_t *)(uintptr_t)fp;
         P("  frame #%-2d ebp=%#010x ret=%#010x\n", i, fp, f[1]);
         if (f[0] <= fp) { break; }
         fp = f[0];
      }
      _exit(94);
   }
   P("SIGILL at %#lx — not one of our traps\n", (unsigned long)rip);
   _exit(97);
}

static intptr_t find_image(const char *needle)
{
   for (uint32_t i = 0; i < _dyld_image_count(); ++i) {
      const char *n = _dyld_get_image_name(i);
      if (n && strstr(n, needle)) { return _dyld_get_image_vmaddr_slide(i); }
   }
   return 0;
}

static void *watch(void *arg)
{
   (void)arg;
   intptr_t slide = 0;
   for (int i = 0; i < 400 && !slide; ++i) {
      slide = find_image("libvstdlib.dylib");
      if (!slide) { usleep(250 * 1000); }
   }
   if (!slide) { P("libvstdlib.dylib never appeared\n"); return NULL; }
   P("libvstdlib slide=%#lx  g_KeyValuesSystem=%#lx\n",
     (unsigned long)slide, (unsigned long)(G_KVS + slide));
   P("expect vptr=%#lx  [+0x10]=%#lx  [+0x24]=%#lx\n",
     (unsigned long)(VTABLE + 8 + slide),
     (unsigned long)(SLOT_GETSTRING + slide),
     (unsigned long)(SLOT_GETSYMCS + slide));

   /* The caller reaches KeyValuesSystem() through soundemittersystem's
    * __la_symbol_ptr slot 0x10027110 (dyld_info: bind libvstdlib/_KeyValuesSystem).
    * If eax is wrong, the first thing to rule out is that slot. */
   intptr_t sslide = 0;
   for (int i = 0; i < 400 && !sslide; ++i) {
      sslide = find_image("soundemittersystem.dylib");
      if (!sslide) { usleep(250 * 1000); }
   }
   if (sslide) {
      const uint64_t *slot = (const uint64_t *)(uintptr_t)(0x10027110u + sslide);
      P("soundemitter slide=%#lx  la_symbol_ptr[_KeyValuesSystem]=%#llx  "
        "expect %#lx\n", (unsigned long)sslide, (unsigned long long)*slot,
        (unsigned long)(0x1001d836u + slide));
      const unsigned char *fn = (const unsigned char *)(uintptr_t)*slot;
      P("target bytes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
        fn[0], fn[1], fn[2], fn[3], fn[4], fn[5], fn[6], fn[7]);
   }

   /* Arm both traps. */
   if (sslide && getenv("KVS_TRAP")) {
      g_expect_obj  = (uint32_t)(G_KVS + slide);
      g_expect_slot = (uint32_t)(SLOT_GETSYMCS + slide);
      g_trap_a = (uintptr_t)(0x10000000u + TRAP_A_OFF + sslide);
      g_trap_b = (uintptr_t)(0x10000000u + TRAP_B_OFF + sslide);
      g_trap_c = (uintptr_t)(0x10000000u + TRAP_C_OFF + sslide);
      g_trap_d = (uintptr_t)(0x10000000u + TRAP_D_OFF + sslide);
      g_trap_e = (uintptr_t)(0x10000000u + TRAP_E_OFF + sslide);
      struct sigaction sa;
      memset(&sa, 0, sizeof sa);
      sa.sa_sigaction = on_sigill;
      sa.sa_flags = SA_SIGINFO | SA_NODEFER;
      sigemptyset(&sa.sa_mask);
      sigaction(SIGILL, &sa, NULL);
      P("traps armed A=%#lx B=%#lx (expect rax=%#x target=%#x)\n",
        (unsigned long)g_trap_a, (unsigned long)g_trap_b,
        g_expect_obj, g_expect_slot);
      if (!poke_ud2(g_trap_a) || !poke_ud2(g_trap_b) || !poke_ud2(g_trap_c) ||
          !poke_ud2(g_trap_d) || !poke_ud2(g_trap_e))
         { P("mprotect FAILED\n"); }
   }

   const uint32_t *obj = (const uint32_t *)(uintptr_t)(G_KVS + slide);
   const uint64_t *slot = sslide
      ? (const uint64_t *)(uintptr_t)(0x10027110u + sslide) : NULL;
   uint32_t last_vptr = 0, last_a = 0, last_b = 0;
   uint64_t last_slot = 0, last_fn = 0;
   for (int i = 0; i < 4000; ++i) {
      uint32_t vptr = obj[0], a = 0, b = 0;
      if (vptr) {
         const uint32_t *v = (const uint32_t *)(uintptr_t)vptr;
         a = v[4];        /* +0x10 GetStringForSymbol          */
         b = v[9];        /* +0x24 GetSymbolForStringCaseSens. */
      }
      uint64_t sv = slot ? *slot : 0, fn = 0;
      if (sv) { fn = *(const uint64_t *)(uintptr_t)sv; }
      if (vptr != last_vptr || a != last_a || b != last_b ||
          sv != last_slot || fn != last_fn) {
         P("t=%6dms vptr=%#010x [+0x10]=%#010x %s [+0x24]=%#010x %s "
           "slot=%#llx fn8=%#llx\n", i * 25, vptr,
           a, a == (uint32_t)(SLOT_GETSTRING + slide) ? "OK " : "BAD",
           b, b == (uint32_t)(SLOT_GETSYMCS + slide) ? "OK " : "BAD",
           (unsigned long long)sv, (unsigned long long)fn);
         last_vptr = vptr; last_a = a; last_b = b; last_slot = sv; last_fn = fn;
      }
      usleep(25 * 1000);
   }
   return NULL;
}

__attribute__((constructor)) static void kvs_probe_init(void)
{
   pthread_t t;
   if (pthread_create(&t, NULL, watch, NULL) == 0) { pthread_detach(t); }
   P("armed\n");
}
