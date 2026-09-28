/*
 * coreaudio_ioproc_shim.c — ONE job: an i386 CoreAudio HAL IOProc
 * (AudioDeviceAddIOProc / Start / Stop / RemoveIOProc).
 *
 * The IOProc receives AudioBufferLists whose layout differs by arch:
 *     i386   { u32 n; { u32 nch; u32 size; u32 mData } [n] }        12-byte buffers
 *     x86_64 { u32 n; pad; { u32 nch; u32 size; u64 mData } [n] }  16-byte buffers
 * and whose mData are CoreAudio's own high-memory sample buffers. The generic
 * callback bridge bounced the lists as raw bytes, so BASS (PvZ) mixed every
 * cycle into garbage and the device played silence.
 *
 * Here the native side gets OUR IOProc; per cycle it builds i386-layout lists
 * over low-4GB sample buffers (libabiconv's malloc is the low arena), copies
 * the input in, copies the timestamps low, calls the i386 proc through
 * x64_cb_wrap with an all-word signature, then copies the output samples back
 * into CoreAudio's buffers. Kill switch M64_NO_HAL_IOPROC_SHIM=1 (generic bridge).
 * ABI: MTSHIM (rdi -> &i386 args[0]); symbols in custom.syms.
 */
#include <CoreAudio/CoreAudio.h>
#include <os/lock.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/* cb_bridge.c's descriptor (must match its layout) */
#define X64_CB_MAX_ARGS 16
typedef struct {
   uint32_t nargs;
   uint32_t ret_kind;
   uint8_t  arg_kinds[X64_CB_MAX_ARGS];
   uint32_t arg_sizes[X64_CB_MAX_ARGS];
} x64_cb_sig;
extern uint64_t x64_cb_wrap(uint32_t fn32, const x64_cb_sig *sig);
extern uint64_t x64_objc_unwrap(uint32_t h);

/* seven plain words in, OSStatus out (CBA_I32 = 0, CBR_I32 = 1) */
static const x64_cb_sig k_sig7 = { 7, 1, { 0 }, { 0 } };
typedef uint32_t (*ioproc32_fn)(uint64_t, uint64_t, uint64_t, uint64_t,
                                uint64_t, uint64_t, uint64_t);

typedef struct { uint32_t nch, size, data; } buf32;
typedef struct { uint32_t n; buf32 b[]; } abl32;

/* ponytail: fixed table; BASS registers one proc per device. */
#define MAX_PROCS 8
typedef struct {
   AudioDeviceID dev;
   uint32_t      fn32, client32;
   ioproc32_fn   call;
   abl32        *in32, *out32;          /* low headers (n <= MAX_BUFS) */
   void         *data[2][16];           /* low sample buffers [in/out][i] */
   uint32_t      cap[2][16];
   AudioTimeStamp *ts;                  /* 3 low timestamps */
} ioproc_ent;
#define MAX_BUFS 16
static ioproc_ent        g_ent[MAX_PROCS];
static os_unfair_lock    g_lock = OS_UNFAIR_LOCK_INIT;

static int off(void)
{
   static int v = -1;
   if (v < 0) { v = getenv("M64_NO_HAL_IOPROC_SHIM") != NULL; }
   return v;
}

#define P32(p) ((uint64_t)(uint32_t)(uintptr_t)(p))

/* Native list -> low i386 list. `copy` moves the samples (input side). */
static abl32 *lower(ioproc_ent *e, int side, abl32 *dst, const AudioBufferList *src, int copy)
{
   if (!src) { return NULL; }
   const uint32_t n = src->mNumberBuffers < MAX_BUFS ? src->mNumberBuffers : MAX_BUFS;
   dst->n = n;
   for (uint32_t i = 0; i < n; ++i) {
      const AudioBuffer *s = &src->mBuffers[i];
      if (e->cap[side][i] < s->mDataByteSize) {       /* grows once, off the steady state */
         free(e->data[side][i]);
         e->data[side][i] = malloc(s->mDataByteSize);
         e->cap[side][i]  = e->data[side][i] ? s->mDataByteSize : 0;
      }
      dst->b[i].nch  = s->mNumberChannels;
      dst->b[i].size = e->cap[side][i] ? s->mDataByteSize : 0;
      dst->b[i].data = (uint32_t)(uintptr_t)e->data[side][i];
      if (copy && s->mData && dst->b[i].size) { memcpy(e->data[side][i], s->mData, dst->b[i].size); }
      else if (dst->b[i].size) { memset(e->data[side][i], 0, dst->b[i].size); }
   }
   return dst;
}

static OSStatus hal_ioproc(AudioDeviceID dev, const AudioTimeStamp *now,
                           const AudioBufferList *in, const AudioTimeStamp *inTime,
                           AudioBufferList *out, const AudioTimeStamp *outTime, void *ctx)
{
   ioproc_ent *e = ctx;
   if (now)     { e->ts[0] = *now; }
   if (inTime)  { e->ts[1] = *inTime; }
   if (outTime) { e->ts[2] = *outTime; }
   abl32 *i32 = lower(e, 0, e->in32, in, 1);
   abl32 *o32 = lower(e, 1, e->out32, out, 0);
   static int traced;
   if (!traced && getenv("ABICONV_HAL_TRACE")) {
      traced = 1;
      fprintf(stderr, "[hal] ioproc in=%p(n=%u) out=%p(n=%u)", (void *)i32, i32 ? i32->n : 0,
              (void *)o32, o32 ? o32->n : 0);
      for (uint32_t i = 0; o32 && i < o32->n; ++i)
         fprintf(stderr, " out[%u]={nch %u size %u data %#x}", i, o32->b[i].nch, o32->b[i].size, o32->b[i].data);
      fprintf(stderr, " ts=%p client=%#x\n", (void *)e->ts, e->client32);
   }
   const OSStatus r = (OSStatus)e->call(dev, now ? P32(&e->ts[0]) : 0, P32(i32),
                                        inTime ? P32(&e->ts[1]) : 0, P32(o32),
                                        outTime ? P32(&e->ts[2]) : 0, e->client32);
   if (out && o32) {
      for (uint32_t i = 0; i < o32->n; ++i) {
         AudioBuffer *d = &out->mBuffers[i];
         uint32_t sz = o32->b[i].size < d->mDataByteSize ? o32->b[i].size : d->mDataByteSize;
         if (d->mData && sz) { memcpy(d->mData, e->data[1][i], sz); }
      }
   }
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
   if (!e->ts) {   /* low allocations: headers + timestamps */
      e->in32  = calloc(1, sizeof(abl32) + MAX_BUFS * sizeof(buf32));
      e->out32 = calloc(1, sizeof(abl32) + MAX_BUFS * sizeof(buf32));
      e->ts    = calloc(3, sizeof(AudioTimeStamp));
   }
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
