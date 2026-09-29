/*
 * iokit_com_shim.c — IOKit CFPlugIn (COM) interfaces for i386 callers.
 *
 * IOCreatePlugInInterfaceForService hands back `IOCFPlugInInterface **`: a
 * pointer to a pointer to a table of FUNCTION POINTERS, which the app calls as
 * `(*iface)->Method(iface, ...)`. Native, every level is 64-bit and every entry
 * is an x86_64 function, so i386 code can neither dereference the returned
 * pointer (it truncates) nor call an entry (cross-ABI call). Halo uses this for
 * gamepads/joysticks: plug-in -> QueryInterface(IOHIDDeviceInterface) -> open,
 * element values and an event queue.
 *
 * The bridge gives the app a low-4GB PROXY per native interface. Its first word
 * is the i386 vtable pointer, so `*proxy` is a table of 4-byte entries, each an
 * i386-callable MTSHIM trampoline (maptable_tramp.asm ___iokcom_*) landing in a
 * shim_iokcom_* below. The shim maps `this` (the proxy) back to the native
 * interface and calls the native entry with the arguments marshalled:
 * CF refs unwrap/wrap through the proxy arena, IOHIDEventStruct is converted
 * between layouts (only longValue differs: 4 vs 8 bytes at +24), callbacks bind
 * through x64_cb_wrap, and interfaces returned by QueryInterface / allocQueue
 * become proxies too. Implemented surface: IUnknown, IOCFPlugInInterface,
 * IOHIDDeviceInterface up to version 1.2.2 and IOHIDQueueInterface. Anything
 * else (e.g. FireWire AV/C, IOHIDOutputTransaction) is refused with
 * E_NOINTERFACE / NULL, the documented "not supported" answers the app
 * already handles.
 *
 * Residual: a callback's `sender` argument is the NATIVE interface pointer,
 * which the generic callback bridge passes as a truncated word.
 */

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/hid/IOHIDLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFPlugInCOM.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cb_bridge.h"
#include "gap.h"

/* The i386-callable trampolines (maptable_tramp.asm). */
#define IOKCOM_METHODS(X)                                                   \
   X(QueryInterface) X(AddRef) X(Release)                                   \
   X(Probe) X(Start) X(Stop)                                                \
   X(createAsyncEventSource) X(getAsyncEventSource) X(createAsyncPort)      \
   X(getAsyncPort) X(open) X(close) X(setRemovalCallback)                   \
   X(getElementValue) X(setElementValue) X(queryElementValue)               \
   X(startAllQueues) X(stopAllQueues) X(allocQueue) X(allocOutputTransaction) \
   X(setReport) X(getReport) X(copyMatchingElements)                        \
   X(setInterruptReportHandlerCallback)                                     \
   X(q_create) X(q_dispose) X(q_addElement) X(q_removeElement)              \
   X(q_hasElement) X(q_start) X(q_stop) X(q_getNextEvent)                   \
   X(q_setEventCallout) X(q_getEventCallout)
#define IOKCOM_DECL(n) extern void __iokcom_##n(void);
IOKCOM_METHODS(IOKCOM_DECL)
#define T(n) ((uint32_t)(uintptr_t)__iokcom_##n)

enum { K_PLUGIN = 1, K_HID = 2, K_QUEUE = 3 };
#define PROXY_MAGIC 0x434f4d58u /* "XMOC" */

struct com_proxy {
   uint32_t vtbl32;          /* i386 view: `*this` = the i386 vtable */
   uint32_t magic;
   uint32_t kind;
   uint32_t cb32[3];         /* queue: i386 callout, target, refcon */
   void **native;            /* the native interface (pointer to its vtable ptr) */
};

/* i386 vtables (4-byte slots). Filled once; entries not bridged stay 0 so a
 * call through one faults at a recognisable NULL rather than a wild address. */
static uint32_t g_vt_plugin[9], g_vt_hid[24], g_vt_queue[18];

static void vt_init(void) {
   static int done;
   if (__atomic_load_n(&done, __ATOMIC_ACQUIRE)) { return; }
   const uint32_t iu[4] = { 0, T(QueryInterface), T(AddRef), T(Release) };
   memcpy(g_vt_plugin, iu, sizeof iu);
   memcpy(g_vt_hid, iu, sizeof iu);
   memcpy(g_vt_queue, iu, sizeof iu);
   /* IOCFPlugInInterface: ..., UInt16 version, UInt16 revision @16, Probe @20 */
   g_vt_plugin[4] = 1u;                       /* version 1, revision 0 */
   g_vt_plugin[5] = T(Probe); g_vt_plugin[6] = T(Start); g_vt_plugin[7] = T(Stop);
   const uint32_t hid[] = {
      T(createAsyncEventSource), T(getAsyncEventSource), T(createAsyncPort),
      T(getAsyncPort), T(open), T(close), T(setRemovalCallback),
      T(getElementValue), T(setElementValue), T(queryElementValue),
      T(startAllQueues), T(stopAllQueues), T(allocQueue),
      T(allocOutputTransaction),                          /* 1.0.0 */
      T(setReport), T(getReport),                         /* 1.2.1 */
      T(copyMatchingElements), T(setInterruptReportHandlerCallback), /* 1.2.2 */
   };
   memcpy(g_vt_hid + 4, hid, sizeof hid);
   const uint32_t q[] = {
      T(createAsyncEventSource), T(getAsyncEventSource), T(createAsyncPort),
      T(getAsyncPort), T(q_create), T(q_dispose), T(q_addElement),
      T(q_removeElement), T(q_hasElement), T(q_start), T(q_stop),
      T(q_getNextEvent), T(q_setEventCallout), T(q_getEventCallout),
   };
   memcpy(g_vt_queue + 4, q, sizeof q);
   __atomic_store_n(&done, 1, __ATOMIC_RELEASE);
}

static uint32_t proxy_new(void **native, uint32_t kind) {
   if (!native) { return 0; }
   vt_init();
   struct com_proxy *p = calloc(1, sizeof *p);      /* shim heap: low 4GB */
   if (!p) { return 0; }
   p->vtbl32 = (uint32_t)(uintptr_t)(kind == K_PLUGIN ? g_vt_plugin
                                   : kind == K_HID ? g_vt_hid : g_vt_queue);
   p->magic = PROXY_MAGIC;
   p->kind = kind;
   p->native = native;
   return (uint32_t)(uintptr_t)p;
}

static struct com_proxy *proxy_of(uint32_t this32) {
   struct com_proxy *p = (struct com_proxy *)(uintptr_t)this32;
   return (p && p->magic == PROXY_MAGIC) ? p : NULL;
}

/* The native vtables. Each native interface pointer is `Vtbl **`. */
#define IU(p)  (*(IUnknownVTbl **)(p)->native)
#define PLG(p) (*(IOCFPlugInInterface **)(p)->native)
#define HID(p) (*(IOHIDDeviceInterface122 **)(p)->native)
#define QUE(p) (*(IOHIDQueueInterface **)(p)->native)
#define SELF(p) ((void *)(p)->native)

static uint32_t out_ref(uint32_t slot32, const void *ref) {
   if (slot32) { *(uint32_t *)(uintptr_t)slot32 = ref ? x64_objc_wrap((uint64_t)(uintptr_t)ref) : 0; }
   return 0;
}

/* ---- IUnknown ------------------------------------------------------------- */

static int uuid_is(CFUUIDBytes b, CFUUIDRef u) {
   CFUUIDBytes w = CFUUIDGetUUIDBytes(u);
   return memcmp(&b, &w, sizeof b) == 0;
}

/* HRESULT QueryInterface(void *self, REFIID iid, LPVOID *ppv)
 * i386 frame: self[0] iid[1..4] (16 bytes by value) ppv[5]. */
uint32_t shim_iokcom_QueryInterface(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   uint32_t *ppv = (uint32_t *)(uintptr_t)a[5];
   if (ppv) { *ppv = 0; }
   if (!p) { return (uint32_t)E_POINTER; }
   CFUUIDBytes iid;
   memcpy(&iid, &a[1], sizeof iid);
   uint32_t kind = 0;
   if (uuid_is(iid, kIOHIDDeviceInterfaceID) || uuid_is(iid, kIOHIDDeviceInterfaceID121) ||
       uuid_is(iid, kIOHIDDeviceInterfaceID122)) {
      kind = K_HID;
   } else if (uuid_is(iid, kIOHIDQueueInterfaceID)) {
      kind = K_QUEUE;
   } else if (uuid_is(iid, kIOCFPlugInInterfaceID) || uuid_is(iid, IUnknownUUID)) {
      kind = p->kind;              /* same object, same table */
   } else {
      return (uint32_t)E_NOINTERFACE;
   }
   void *out = NULL;
   HRESULT hr = IU(p)->QueryInterface(SELF(p), iid, &out);
   if (hr != S_OK || !out) { return (uint32_t)hr; }
   uint32_t px = proxy_new((void **)out, kind);
   if (!px) { IU(p)->Release(out); return (uint32_t)E_OUTOFMEMORY; }
   if (ppv) { *ppv = px; }
   return S_OK;
}

uint32_t shim_iokcom_AddRef(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? IU(p)->AddRef(SELF(p)) : 0;
}

uint32_t shim_iokcom_Release(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return 0; }
   ULONG n = IU(p)->Release(SELF(p));
   if (n == 0) { p->magic = 0; free(p); }
   return n;
}

/* ---- IOCFPlugInInterface -------------------------------------------------- */

/* IOReturn Probe(self, CFDictionaryRef propertyTable, io_service_t, SInt32 *order) */
uint32_t shim_iokcom_Probe(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   return PLG(p)->Probe(SELF(p), (CFDictionaryRef)(uintptr_t)x64_objc_unwrap(a[1]),
                        a[2], (SInt32 *)(uintptr_t)a[3]);
}

uint32_t shim_iokcom_Start(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   return PLG(p)->Start(SELF(p), (CFDictionaryRef)(uintptr_t)x64_objc_unwrap(a[1]), a[2]);
}

uint32_t shim_iokcom_Stop(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? PLG(p)->Stop(SELF(p)) : kIOReturnBadArgument;
}

/* ---- shared by IOHIDDeviceInterface and IOHIDQueueInterface -------------- *
 * The first four entries after IUnknown have identical signatures in both. */

uint32_t shim_iokcom_createAsyncEventSource(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   CFRunLoopSourceRef src = NULL;
   IOReturn r = p->kind == K_QUEUE ? QUE(p)->createAsyncEventSource(SELF(p), &src)
                                   : HID(p)->createAsyncEventSource(SELF(p), &src);
   out_ref(a[1], src);
   return r;
}

uint32_t shim_iokcom_getAsyncEventSource(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return 0; }
   CFRunLoopSourceRef src = p->kind == K_QUEUE ? QUE(p)->getAsyncEventSource(SELF(p))
                                               : HID(p)->getAsyncEventSource(SELF(p));
   return src ? x64_objc_wrap((uint64_t)(uintptr_t)src) : 0;
}

uint32_t shim_iokcom_createAsyncPort(uint32_t *a) {   /* mach_port_t is 32-bit */
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   mach_port_t *port = (mach_port_t *)(uintptr_t)a[1];
   return p->kind == K_QUEUE ? QUE(p)->createAsyncPort(SELF(p), port)
                             : HID(p)->createAsyncPort(SELF(p), port);
}

uint32_t shim_iokcom_getAsyncPort(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return MACH_PORT_NULL; }
   return p->kind == K_QUEUE ? QUE(p)->getAsyncPort(SELF(p))
                             : HID(p)->getAsyncPort(SELF(p));
}

/* ---- IOHIDDeviceInterface ------------------------------------------------- */

/* IOHIDEventStruct: i386 {type, cookie, value, timestamp(8), longValueSize,
 * longValue(4)} = 28 bytes; native longValue is 8 bytes at the same +24. */
static void ev_to_native(const uint32_t *e32, IOHIDEventStruct *n) {
   memset(n, 0, sizeof *n);
   if (!e32) { return; }
   n->type = (IOHIDElementType)e32[0];
   n->elementCookie = (IOHIDElementCookie)e32[1];
   n->value = (int32_t)e32[2];
   memcpy(&n->timestamp, &e32[3], 8);
   n->longValueSize = e32[5];
   n->longValue = (void *)(uintptr_t)e32[6];
}

/* The library allocates a native longValue the CALLER frees: hand the i386
 * caller a copy on the shim heap its own free() releases. */
static void ev_to_i386(const IOHIDEventStruct *n, uint32_t *e32) {
   if (!e32) { return; }
   e32[0] = (uint32_t)n->type;
   e32[1] = (uint32_t)n->elementCookie;
   e32[2] = (uint32_t)n->value;
   memcpy(&e32[3], &n->timestamp, 8);
   e32[5] = n->longValueSize;
   e32[6] = 0;
   if (n->longValue && n->longValueSize) {
      void *low = malloc(n->longValueSize);
      if (low) { memcpy(low, n->longValue, n->longValueSize); }
      e32[6] = (uint32_t)(uintptr_t)low;
      free(n->longValue);
   }
}

/* IOHIDCallbackFunction(target, result, refcon, sender) */
static const x64_cb_sig k_sig_cb = { .nargs = 4, .ret_kind = CBR_VOID, .arg_kinds = { CBA_PTR, CBA_I32, CBA_PTR, CBA_PTR } };
/* IOHIDElementCallbackFunction(target, result, refcon, sender, cookie) */
static const x64_cb_sig k_sig_elem_cb =
   { .nargs = 5, .ret_kind = CBR_VOID, .arg_kinds = { CBA_PTR, CBA_I32, CBA_PTR, CBA_PTR, CBA_I32 } };
/* IOHIDReportCallbackFunction(target, result, refcon, sender, bufferSize) */
static const x64_cb_sig k_sig_report_cb =
   { .nargs = 5, .ret_kind = CBR_VOID, .arg_kinds = { CBA_PTR, CBA_I32, CBA_PTR, CBA_PTR, CBA_I32 } };

static void *cb(uint32_t fn32, const x64_cb_sig *sig) {
   return fn32 ? (void *)(uintptr_t)x64_cb_wrap(fn32, sig) : NULL;
}
#define PTR(v) ((void *)(uintptr_t)(v))

uint32_t shim_iokcom_open(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? HID(p)->open(SELF(p), a[1]) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_close(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? HID(p)->close(SELF(p)) : kIOReturnBadArgument;
}

/* (self, IOHIDCallbackFunction, target, refcon) */
uint32_t shim_iokcom_setRemovalCallback(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   return HID(p)->setRemovalCallback(SELF(p), (IOHIDCallbackFunction)cb(a[1], &k_sig_cb),
                                     PTR(a[2]), PTR(a[3]));
}

/* (self, cookie, IOHIDEventStruct *) */
uint32_t shim_iokcom_getElementValue(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   IOHIDEventStruct ev;
   memset(&ev, 0, sizeof ev);
   IOReturn r = HID(p)->getElementValue(SELF(p), (IOHIDElementCookie)a[1], &ev);
   ev_to_i386(&ev, (uint32_t *)PTR(a[2]));
   return r;
}

/* set/query: (self, cookie, IOHIDEventStruct *, timeoutMS, callback, target,
 * refcon). The event lives in the caller's i386 layout; with a callback the
 * library may complete later, so the native copy must outlive the call:
 * issue the request synchronously and run the i386 callback ourselves. */
static uint32_t hid_elem_io(uint32_t *a, int query) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   uint32_t *e32 = (uint32_t *)PTR(a[2]);
   IOHIDEventStruct ev;
   ev_to_native(e32, &ev);
   IOReturn r = query
      ? HID(p)->queryElementValue(SELF(p), (IOHIDElementCookie)a[1], &ev, a[3], NULL, NULL, NULL)
      : HID(p)->setElementValue(SELF(p), (IOHIDElementCookie)a[1], &ev, a[3], NULL, NULL, NULL);
   if (query) { ev_to_i386(&ev, e32); }
   if (a[4]) {
      IOHIDElementCallbackFunction f = (IOHIDElementCallbackFunction)cb(a[4], &k_sig_elem_cb);
      if (f) { f(PTR(a[5]), r, PTR(a[6]), PTR(a[0]), (IOHIDElementCookie)a[1]); }
   }
   return r;
}
uint32_t shim_iokcom_setElementValue(uint32_t *a) { return hid_elem_io(a, 0); }
uint32_t shim_iokcom_queryElementValue(uint32_t *a) { return hid_elem_io(a, 1); }

uint32_t shim_iokcom_startAllQueues(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? HID(p)->startAllQueues(SELF(p)) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_stopAllQueues(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? HID(p)->stopAllQueues(SELF(p)) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_allocQueue(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? proxy_new((void **)HID(p)->allocQueue(SELF(p)), K_QUEUE) : 0;
}

/* Output transactions are not bridged: NULL is the documented failure. */
uint32_t shim_iokcom_allocOutputTransaction(uint32_t *a) { GAP_STUB(a); return 0; }

/* (self, reportType, reportID, void *buffer, uint32 size, uint32 timeoutMS,
 *  IOHIDReportCallbackFunction, target, refcon). The buffer is app-owned low
 * memory, valid natively, so an async completion is safe. */
uint32_t shim_iokcom_setReport(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   return HID(p)->setReport(SELF(p), (IOHIDReportType)a[1], a[2], PTR(a[3]), a[4], a[5],
                            (IOHIDReportCallbackFunction)cb(a[6], &k_sig_report_cb),
                            PTR(a[7]), PTR(a[8]));
}

/* (self, reportType, reportID, void *buffer, uint32 *size, uint32 timeoutMS,
 *  callback, target, refcon) */
uint32_t shim_iokcom_getReport(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   return HID(p)->getReport(SELF(p), (IOHIDReportType)a[1], a[2], PTR(a[3]),
                            (uint32_t *)PTR(a[4]), a[5],
                            (IOHIDReportCallbackFunction)cb(a[6], &k_sig_report_cb),
                            PTR(a[7]), PTR(a[8]));
}

/* (self, CFDictionaryRef matching, CFArrayRef *elements) */
uint32_t shim_iokcom_copyMatchingElements(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   CFArrayRef arr = NULL;
   IOReturn r = HID(p)->copyMatchingElements(
      SELF(p), (CFDictionaryRef)(uintptr_t)x64_objc_unwrap(a[1]), &arr);
   out_ref(a[2], arr);
   return r;
}

/* (self, void *reportBuffer, uint32 size, callback, target, refcon) */
uint32_t shim_iokcom_setInterruptReportHandlerCallback(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   return HID(p)->setInterruptReportHandlerCallback(
      SELF(p), PTR(a[1]), a[2], (IOHIDReportCallbackFunction)cb(a[3], &k_sig_report_cb),
      PTR(a[4]), PTR(a[5]));
}

/* ---- IOHIDQueueInterface -------------------------------------------------- */

uint32_t shim_iokcom_q_create(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? QUE(p)->create(SELF(p), a[1], a[2]) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_q_dispose(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? QUE(p)->dispose(SELF(p)) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_q_addElement(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? QUE(p)->addElement(SELF(p), (IOHIDElementCookie)a[1], a[2]) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_q_removeElement(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? QUE(p)->removeElement(SELF(p), (IOHIDElementCookie)a[1]) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_q_hasElement(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? QUE(p)->hasElement(SELF(p), (IOHIDElementCookie)a[1]) : 0;
}

uint32_t shim_iokcom_q_start(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? QUE(p)->start(SELF(p)) : kIOReturnBadArgument;
}

uint32_t shim_iokcom_q_stop(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   return p ? QUE(p)->stop(SELF(p)) : kIOReturnBadArgument;
}

/* (self, IOHIDEventStruct *, AbsoluteTime maxTime [2 words], uint32 timeoutMS) */
uint32_t shim_iokcom_q_getNextEvent(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   AbsoluteTime max;
   memcpy(&max, &a[2], sizeof max);
   IOHIDEventStruct ev;
   memset(&ev, 0, sizeof ev);
   IOReturn r = QUE(p)->getNextEvent(SELF(p), &ev, max, a[4]);
   if (r == kIOReturnSuccess) { ev_to_i386(&ev, (uint32_t *)PTR(a[1])); }
   return r;
}

/* (self, callback, target, refcon); the i386 values are kept for
 * getEventCallout, which must hand back what the app installed. */
uint32_t shim_iokcom_q_setEventCallout(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   IOReturn r = QUE(p)->setEventCallout(SELF(p), (IOHIDCallbackFunction)cb(a[1], &k_sig_cb),
                                        PTR(a[2]), PTR(a[3]));
   if (r == kIOReturnSuccess) { p->cb32[0] = a[1]; p->cb32[1] = a[2]; p->cb32[2] = a[3]; }
   return r;
}

/* (self, IOHIDCallbackFunction *out, void **target, void **refcon) */
uint32_t shim_iokcom_q_getEventCallout(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   for (int i = 0; i < 3; ++i) {
      if (a[1 + i]) { *(uint32_t *)PTR(a[1 + i]) = p->cb32[i]; }
   }
   return kIOReturnSuccess;
}

/* ---- entry points --------------------------------------------------------- */

/* kern_return_t IOCreatePlugInInterfaceForService(io_service_t service,
 *    CFUUIDRef pluginType, CFUUIDRef interfaceType,
 *    IOCFPlugInInterface ***theInterface, SInt32 *theScore);
 * i386 frame: service[0] pluginType[1] interfaceType[2] out[3] score[4]. */
uint32_t shim_IOCreatePlugInInterfaceForService(uint32_t *a) {
   uint32_t *out = (uint32_t *)PTR(a[3]);
   if (out) { *out = 0; }
   IOCFPlugInInterface **iface = NULL;
   SInt32 score = 0;
   kern_return_t kr = IOCreatePlugInInterfaceForService(
      (io_service_t)a[0], (CFUUIDRef)(uintptr_t)x64_objc_unwrap(a[1]),
      (CFUUIDRef)(uintptr_t)x64_objc_unwrap(a[2]), &iface, &score);
   if (a[4]) { *(SInt32 *)PTR(a[4]) = score; }
   if (kr != KERN_SUCCESS || !iface) { return (uint32_t)kr; }
   uint32_t px = proxy_new((void **)iface, K_PLUGIN);
   if (!px) { IODestroyPlugInInterface(iface); return kIOReturnNoMemory; }
   if (out) { *out = px; }
   return KERN_SUCCESS;
}

/* kern_return_t IODestroyPlugInInterface(IOCFPlugInInterface **interface) */
uint32_t shim_IODestroyPlugInInterface(uint32_t *a) {
   struct com_proxy *p = proxy_of(a[0]);
   if (!p) { return kIOReturnBadArgument; }
   kern_return_t kr = IODestroyPlugInInterface((IOCFPlugInInterface **)p->native);
   p->magic = 0;
   free(p);
   return (uint32_t)kr;
}
