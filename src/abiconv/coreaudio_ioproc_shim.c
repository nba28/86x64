/*
 * coreaudio_ioproc_shim.c — ONE job: an i386 CoreAudio HAL IOProc
 * (AudioDeviceAddIOProc / Start / Stop / RemoveIOProc).
 *
 * The IOProc receives AudioBufferLists whose layout differs by arch and whose
 * mData are CoreAudio's own high-memory buffers (coreaudio_abl.h). The generic
 * callback bridge bounced the lists as raw bytes, so BASS (PvZ) mixed every
 * cycle into garbage and the device played silence.
 *
 * Here the native side gets OUR IOProc; per cycle it lowers both lists and the
 * timestamps, calls the i386 proc through x64_cb_wrap with an all-word
 * signature, then raises the output samples back into CoreAudio's buffers. Kill switch M64_NO_HAL_IOPROC_SHIM=1 (generic bridge).
 * ABI: MTSHIM (rdi -> &i386 args[0]); symbols in custom.syms.
 */
#include <CoreAudio/CoreAudio.h>
#include <os/lock.h>
#include <stdint.h>
#include <stdlib.h>
#include "cb_bridge.h"
#include "coreaudio_abl.h"

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/* seven plain words in, OSStatus out */
static const x64_cb_sig k_sig7 = { 7, CBR_I32, { CBA_I32 }, { 0 } };
typedef uint32_t (*ioproc32_fn)(uint64_t, uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t);

/* ponytail: fixed table; BASS registers one proc per device. */
#define MAX_PROCS 8
typedef struct {
   AudioDeviceID   dev;
   uint32_t        fn32, client32;
   ioproc32_fn     call;
   abl_low         in, out;             /* low lists + sample buffers */
   AudioTimeStamp *ts;                  /* 3 low timestamps */
} ioproc_ent;
static ioproc_ent     g_ent[MAX_PROCS];
static os_unfair_lock g_lock = OS_UNFAIR_LOCK_INIT;

static int off(void)
{
   static int v = -1;
   if (v < 0) { v = getenv("M64_NO_HAL_IOPROC_SHIM") != NULL; }
   return v;
}

#define P32(p) ((uint64_t)(uint32_t)(uintptr_t)(p))

static OSStatus hal_ioproc(AudioDeviceID dev, const AudioTimeStamp *now,
                           const AudioBufferList *in, const AudioTimeStamp *inTime,
                           AudioBufferList *out, const AudioTimeStamp *outTime, void *ctx)
{
   ioproc_ent *e = ctx;
   if (now)     { e->ts[0] = *now; }
   if (inTime)  { e->ts[1] = *inTime; }
   if (outTime) { e->ts[2] = *outTime; }
   abl32 *i32 = abl_lower(&e->in, in, 1);
   abl32 *o32 = abl_lower(&e->out, out, 0);
   const OSStatus r = (OSStatus)e->call(dev, now ? P32(&e->ts[0]) : 0, P32(i32),
                                        inTime ? P32(&e->ts[1]) : 0, P32(o32),
                                        outTime ? P32(&e->ts[2]) : 0, e->client32);
   if (o32) { abl_raise(&e->out, out); }
   return r;
}

static ioproc_ent *find(AudioDeviceID dev, uint32_t fn32)
{
   for (int i = 0; i < MAX_PROCS; ++i) {
      if (g_ent[i].fn32 && g_ent[i].dev == dev && g_ent[i].fn32 == fn32) { return &g_ent[i]; }
   }
   return NULL;
}

/* OSStatus AudioDeviceAddIOProc(AudioDeviceID, AudioDeviceIOProc, void *) */
uint32_t shim_AudioDeviceAddIOProc(uint32_t *a)
{
   if (off()) {
      return (uint32_t)AudioDeviceAddIOProc(a[0], (AudioDeviceIOProc)(uintptr_t)
                                            x64_cb_wrap(a[1], &k_sig7), (void *)(uintptr_t)a[2]);
   }
   os_unfair_lock_lock(&g_lock);
   ioproc_ent *e = find(a[0], a[1]);
   if (!e) {
      for (int i = 0; i < MAX_PROCS && !e; ++i) { if (!g_ent[i].fn32) { e = &g_ent[i]; } }
   }
   if (!e) { os_unfair_lock_unlock(&g_lock); return (uint32_t)kAudioHardwareUnspecifiedError; }
   if (!e->ts) { e->ts = calloc(3, sizeof(AudioTimeStamp)); }   /* low */
   e->dev = a[0]; e->fn32 = a[1]; e->client32 = a[2];
   e->call = (ioproc32_fn)(uintptr_t)x64_cb_wrap(a[1], &k_sig7);
   os_unfair_lock_unlock(&g_lock);
   const OSStatus r = AudioDeviceAddIOProc(a[0], hal_ioproc, e);
   if (r != noErr) { e->fn32 = 0; }
   return (uint32_t)r;
}

/* OSStatus AudioDeviceRemoveIOProc(AudioDeviceID, AudioDeviceIOProc) */
uint32_t shim_AudioDeviceRemoveIOProc(uint32_t *a)
{
   ioproc_ent *e = off() ? NULL : find(a[0], a[1]);
   if (!e) {
      return (uint32_t)AudioDeviceRemoveIOProc(a[0], (AudioDeviceIOProc)(uintptr_t)
                                               x64_cb_wrap(a[1], &k_sig7));
   }
   const OSStatus r = AudioDeviceRemoveIOProc(a[0], hal_ioproc);
   e->fn32 = 0;   /* buffers kept for reuse */
   return (uint32_t)r;
}

/* OSStatus AudioDeviceStart/Stop(AudioDeviceID, AudioDeviceIOProc) — NULL = the device. */
static AudioDeviceIOProc native_proc(uint32_t dev, uint32_t fn32)
{
   if (!fn32) { return NULL; }
   if (!off() && find(dev, fn32)) { return hal_ioproc; }
   return (AudioDeviceIOProc)(uintptr_t)x64_cb_wrap(fn32, &k_sig7);
}
uint32_t shim_AudioDeviceStart(uint32_t *a) { return (uint32_t)AudioDeviceStart(a[0], native_proc(a[0], a[1])); }
uint32_t shim_AudioDeviceStop(uint32_t *a)  { return (uint32_t)AudioDeviceStop(a[0], native_proc(a[0], a[1])); }
