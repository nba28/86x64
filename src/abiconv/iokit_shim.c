/*
 * Hand-written i386->x86_64 shims for IOKit System-Power notifications and the
 * CFRunLoop calls that wire them up.
 *
 * WHY abigen can't do this:
 *   - IOKit is not in ABICONV_SYM_SOURCES at all, so its functions get no
 *     shim and the translated i386-cdecl call lands on the real x86_64 entry
 *     point reading empty registers -> NULL args -> crash. (iPhoto's
 *     -[AppController _registerForSleepNotifications] crashed at
 *     IORegisterForSystemPower+35 with rdi=rsi=rcx=0.)
 *   - The registration produces OPAQUE 64-BIT POINTERS that don't fit i386's
 *     4-byte slots: IONotificationPortRef (allocated by IOKit, lives high in
 *     the dyld region -> truncates), and the CFRunLoopSourceRef /
 *     CFRunLoopRef that flow into CFRunLoopAddSource. Note io_connect_t /
 *     io_object_t are mach_port_t (32-bit on BOTH archs) so the return value
 *     and the notifier do NOT truncate -- only the C pointers do.
 *
 * Approach (contained, no generic CF-pointer-wrapping needed):
 *   We register natively in x86_64, keep the real 64-bit pointers in a small
 *   slot table, and hand the i386 caller back 32-bit TOKENS for the opaque
 *   out-params. The two CFRunLoop functions that consume those tokens
 *   (CFRunLoopAddSource / CFRunLoopRemoveSource) are OVERRIDDEN here
 *   (excluded from abigen via custom.syms) with token-aware versions that:
 *     - if the source arg is one of our tokens: do the add/remove against the
 *       REAL source on the REAL current run loop with real kCFRunLoopCommonModes;
 *     - otherwise: behave EXACTLY like the abigen shim did (zero-extend the
 *       i386 args and call through), so the ~25 non-IOKit CFRunLoop call sites
 *       in iPhoto are unaffected.
 *   CFRunLoopGetCurrent is left to abigen (its truncated return is ignored on
 *   the IOKit path -- we call CFRunLoopGetCurrent() natively instead).
 *
 * The power callback is handled NATIVELY (auto-acknowledge sleep) rather than
 * bridged back to the legacy i386 callback: the i386 callback only fires on
 * real sleep/wake (never during launch) and a native acknowledge is the
 * robust default (failing to ack kIOMessageCanSystemSleep/SystemWillSleep
 * stalls system sleep ~30s). Bridging to the i386 callback via a reverse
 * trampoline is a future enhancement; the i386 fn ptr + refcon are stored.
 *
 * Reached via the ___IO* / ___CFRunLoop* trampolines in maptable_tramp.asm
 * (static-interpose redirects the binary's _IO* / _CFRunLoop* binds).
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/IOMessage.h>
#include <CoreFoundation/CoreFoundation.h>
#include <os/lock.h>
#include <stdint.h>
#include <string.h>

/* objc_shim.c: 32-bit proxy-arena handle -> real 64-bit ref (non-handles pass
 * through zero-extended). */
extern uint64_t x64_objc_unwrap(uint32_t h);

/* Tokens live in the i386 kernel-reserved top (> LOW_REGION_END 0xF0000000,
 * < 0xFFFFFFFF) so they can never collide with a real low-4GB pointer from
 * the interposer's window NOR with the low 32 bits of a truncated high heap
 * pointer in practice. Index is the low byte. */
#define TOK_PORT_BASE 0xF1A00000u
#define TOK_SRC_BASE  0xF1B00000u
#define TOK_MASK      0xFFF00000u
#define TOK_IDX(t)    ((int)((t) & 0xFFu))

#define IOK_MAX_SLOTS 16

typedef struct {
   int                    in_use;
   io_connect_t           connect;   /* 32-bit mach port (root power domain) */
   IONotificationPortRef  port;      /* 64-bit opaque */
   io_object_t            notifier;  /* 32-bit mach port */
   CFRunLoopSourceRef     source;    /* 64-bit opaque, lazily fetched */
   uint32_t               i386_callback; /* legacy IOServiceInterestCallback */
   uint32_t               i386_refcon;
} iok_slot_t;

static iok_slot_t     g_slots[IOK_MAX_SLOTS];
static os_unfair_lock g_lock = OS_UNFAIR_LOCK_INIT;

/* Native power callback: refcon = &g_slots[i]. Acknowledge sleep so the
 * system does not stall waiting for a (legacy, un-bridged) responder. */
static void iok_power_cb(void *refcon, io_service_t service,
                         uint32_t messageType, void *messageArgument)
{
   iok_slot_t *s = (iok_slot_t *)refcon;
   (void)service;
   switch (messageType) {
      case kIOMessageCanSystemSleep:   /* idle-sleep query: allow */
      case kIOMessageSystemWillSleep:  /* forced sleep: must ack or ~30s stall */
         if (s)
            IOAllowPowerChange(s->connect, (long)(intptr_t)messageArgument);
         break;
      default:
         break;
   }
}

/* io_connect_t IORegisterForSystemPower(void *refcon, IONotificationPortRef *thePortRef,
 *                                       IOServiceInterestCallback callback, io_object_t *notifier);
 * i386 frame: refcon[0] thePortRef[1] callback[2] notifier[3]. */
int shim_IORegisterForSystemPower(uint32_t *a)
{
   uint32_t i386_refcon   = a[0];
   uint32_t portref_out   = a[1];   /* i386 ptr to 4-byte IONotificationPortRef slot */
   uint32_t i386_callback = a[2];
   uint32_t notifier_out  = a[3];   /* i386 ptr to 4-byte io_object_t slot */

   os_unfair_lock_lock(&g_lock);
   int i = -1;
   for (int k = 0; k < IOK_MAX_SLOTS; k++)
      if (!g_slots[k].in_use) { i = k; break; }
   if (i >= 0) { memset(&g_slots[i], 0, sizeof(g_slots[i])); g_slots[i].in_use = 1; }
   os_unfair_lock_unlock(&g_lock);
   if (i < 0)
      return MACH_PORT_NULL;

   IONotificationPortRef port = NULL;
   io_object_t notifier = MACH_PORT_NULL;
   io_connect_t connect = IORegisterForSystemPower(&g_slots[i], &port,
                                                   iok_power_cb, &notifier);
   if (connect == MACH_PORT_NULL) {
      g_slots[i].in_use = 0;
      return MACH_PORT_NULL;
   }

   g_slots[i].connect       = connect;
   g_slots[i].port          = port;
   g_slots[i].notifier      = notifier;
   g_slots[i].source        = NULL;
   g_slots[i].i386_callback = i386_callback;
   g_slots[i].i386_refcon   = i386_refcon;

   if (portref_out)
      *(uint32_t *)(uintptr_t)portref_out = TOK_PORT_BASE | (uint32_t)i;
   if (notifier_out)
      *(uint32_t *)(uintptr_t)notifier_out = (uint32_t)notifier;

   return (int)connect;
}

/* IOReturn IODeregisterForSystemPower(io_object_t *notifier);
 * i386 frame: notifier[0] = i386 ptr to the io_object_t. */
int shim_IODeregisterForSystemPower(uint32_t *a)
{
   uint32_t notifier_ptr = a[0];
   if (!notifier_ptr)
      return kIOReturnBadArgument;
   io_object_t n = *(uint32_t *)(uintptr_t)notifier_ptr;
   int r = IODeregisterForSystemPower(&n);
   *(uint32_t *)(uintptr_t)notifier_ptr = (uint32_t)n;
   return r;
}

/* IONotificationPortRef IONotificationPortCreate(mach_port_t mainPort);
 *
 * The GENERAL device-notification port creator (device/HID hot-plug, IOKit
 * matching notifications) — the sibling of IORegisterForSystemPower's power
 * port. Without this hand shim, abigen auto-forwards to the native creator and
 * TRUNCATES the returned 64-bit IONotificationPortRef into i386's 4-byte eax;
 * the caller then hands that truncated pointer to
 * IONotificationPortGetRunLoopSource, whose token check fails -> it returns a
 * NULL CFRunLoopSourceRef -> the app's CFRunLoopAddSource(rl, NULL, ...) faults
 * (Halo: EXC_BAD_ACCESS in CFRunLoopAddSource+114 after
 * IONotificationPortCreate at i386 site 0x2f0109). Fix: create the REAL native
 * port and tokenize it into the SAME slot table the power path uses, so
 * IONotificationPortGetRunLoopSource / IONotificationPortDestroy resolve it.
 * mainPort (mach_port_t) is 32-bit on both archs (typically kIOMainPortDefault
 * == 0), so a[0] passes through un-truncated. Universal: any i386 app wiring
 * IOKit device notifications into its run loop. */
uint32_t shim_IONotificationPortCreate(uint32_t *a)
{
   os_unfair_lock_lock(&g_lock);
   int i = -1;
   for (int k = 0; k < IOK_MAX_SLOTS; k++)
      if (!g_slots[k].in_use) { i = k; break; }
   if (i >= 0) { memset(&g_slots[i], 0, sizeof(g_slots[i])); g_slots[i].in_use = 1; }
   os_unfair_lock_unlock(&g_lock);
   if (i < 0)
      return 0;   /* slot table full -> NULL port (caller null-checks or we've capped) */

   IONotificationPortRef port = IONotificationPortCreate((mach_port_t)a[0]);
   if (!port) {
      g_slots[i].in_use = 0;
      return 0;
   }
   g_slots[i].port = port;   /* connect/notifier/source stay 0/NULL (no power path) */
   return TOK_PORT_BASE | (uint32_t)i;
}

/* CFRunLoopSourceRef IONotificationPortGetRunLoopSource(IONotificationPortRef port);
 * port arg is our port token -> return a source token. */
uint32_t shim_IONotificationPortGetRunLoopSource(uint32_t *a)
{
   uint32_t tok = a[0];
   if ((tok & TOK_MASK) == TOK_PORT_BASE) {
      int i = TOK_IDX(tok);
      if (i < IOK_MAX_SLOTS && g_slots[i].in_use) {
         if (!g_slots[i].source)
            g_slots[i].source = IONotificationPortGetRunLoopSource(g_slots[i].port);
         return TOK_SRC_BASE | (uint32_t)i;
      }
   }
   return 0;
}

/* void IONotificationPortDestroy(IONotificationPortRef port); */
void shim_IONotificationPortDestroy(uint32_t *a)
{
   uint32_t tok = a[0];
   if ((tok & TOK_MASK) == TOK_PORT_BASE) {
      int i = TOK_IDX(tok);
      if (i < IOK_MAX_SLOTS && g_slots[i].in_use) {
         IONotificationPortDestroy(g_slots[i].port);
         os_unfair_lock_lock(&g_lock);
         memset(&g_slots[i], 0, sizeof(g_slots[i]));
         os_unfair_lock_unlock(&g_lock);
      }
   }
}

/* IOReturn IOServiceClose(io_connect_t connect); 32-bit passthrough. */
int shim_IOServiceClose(uint32_t *a)
{
   return IOServiceClose((io_connect_t)a[0]);
}

/* IOReturn IOAllowPowerChange(io_connect_t kernelPort, long notificationID); */
int shim_IOAllowPowerChange(uint32_t *a)
{
   return IOAllowPowerChange((io_connect_t)a[0], (long)(uintptr_t)a[1]);
}

/* IOReturn IOCancelPowerChange(io_connect_t kernelPort, long notificationID); */
int shim_IOCancelPowerChange(uint32_t *a)
{
   return IOCancelPowerChange((io_connect_t)a[0], (long)(uintptr_t)a[1]);
}

/* ---- CFRunLoop overrides (token-aware; abigen excludes these via custom.syms) ----
 * void CFRunLoopAddSource(CFRunLoopRef rl, CFRunLoopSourceRef src, CFStringRef mode);
 * i386 frame: rl[0] src[1] mode[2]. */
void shim_CFRunLoopAddSource(uint32_t *a)
{
   uint32_t rl = a[0], src = a[1], mode = a[2];
   if ((src & TOK_MASK) == TOK_SRC_BASE) {
      int i = TOK_IDX(src);
      if (i < IOK_MAX_SLOTS && g_slots[i].in_use && g_slots[i].source)
         CFRunLoopAddSource(CFRunLoopGetCurrent(), g_slots[i].source,
                            kCFRunLoopCommonModes);
      return;
   }
   /* non-IOKit source: like the abigen shim, but every ref may be a
    * proxy-arena handle (rl from a wrapped CFRunLoopGetCurrent return, mode
    * from a __CF* data shadow) — unwrap; raw values pass through. */
   CFRunLoopAddSource((CFRunLoopRef)(uintptr_t)x64_objc_unwrap(rl),
                      (CFRunLoopSourceRef)(uintptr_t)x64_objc_unwrap(src),
                      (CFStringRef)(uintptr_t)x64_objc_unwrap(mode));
}

/* void CFRunLoopRemoveSource(CFRunLoopRef rl, CFRunLoopSourceRef src, CFStringRef mode); */
void shim_CFRunLoopRemoveSource(uint32_t *a)
{
   uint32_t rl = a[0], src = a[1], mode = a[2];
   if ((src & TOK_MASK) == TOK_SRC_BASE) {
      int i = TOK_IDX(src);
      if (i < IOK_MAX_SLOTS && g_slots[i].in_use && g_slots[i].source)
         CFRunLoopRemoveSource(CFRunLoopGetCurrent(), g_slots[i].source,
                               kCFRunLoopCommonModes);
      return;
   }
   CFRunLoopRemoveSource((CFRunLoopRef)(uintptr_t)x64_objc_unwrap(rl),
                         (CFRunLoopSourceRef)(uintptr_t)x64_objc_unwrap(src),
                         (CFStringRef)(uintptr_t)x64_objc_unwrap(mode));
}

/* CFRetain/CFRelease must treat our port/source TOKENS as inert: their
 * lifetime is owned by the slot table + the notification port, not by the
 * legacy caller's refcounts. iPhoto's _registerForSleepNotifications does a
 * CFRelease on the run-loop source it got from IONotificationPortGetRunLoopSource
 * (a token) -> real CFRelease would deref the token and fault. We OVERRIDE both
 * (abigen-excluded via custom.syms); everything that isn't our token passes
 * through identically to the former abigen shim. */
#define IS_IOK_TOKEN(v) (((v) & TOK_MASK) == TOK_SRC_BASE || \
                         ((v) & TOK_MASK) == TOK_PORT_BASE)

/* CFTypeRef CFRetain(CFTypeRef cf); */
uint32_t shim_CFRetain(uint32_t *a)
{
   uint32_t cf = a[0];
   if (IS_IOK_TOKEN(cf))
      return cf;                 /* inert: return the token unchanged */
   /* cf may be a proxy-arena HANDLE for a wrapped 64-bit object (handles are
    * low-4GB by construction, so a "looks low, pass raw" shortcut is wrong:
    * native CFRetain would msgSend the handle — the 23rd-blocker `release`
    * crash). Unwrap first; CFRetain returns its argument, so hand the caller
    * back its own 32-bit view. */
   CFRetain((CFTypeRef)(uintptr_t)x64_objc_unwrap(cf));
   return cf;
}

/* void CFRelease(CFTypeRef cf); */
void shim_CFRelease(uint32_t *a)
{
   uint32_t cf = a[0];
   if (IS_IOK_TOKEN(cf))
      return;                    /* inert */
   CFRelease((CFTypeRef)(uintptr_t)x64_objc_unwrap(cf));
}
