/*
 * aq_shim.c — the AudioQueue output API for i386 callers. ONE job: give the
 * 32-bit caller a queue handle, buffers in the i386 AudioQueueBuffer layout,
 * and callbacks it can receive.
 *
 * Unbridged (abigen) the queue and its buffers are native objects above 4GB:
 * AudioQueueAllocateBuffer could not hand back a usable AudioQueueBufferRef,
 * failed, and Portal 2's CAudioDeviceAudioQueue::OpenWaveOut then wrote the
 * NULL buffer's mAudioDataByteSize (SIGSEGV at 0x8). The i386 buffer is
 *   { u32 capacity; u32 mAudioData; u32 mAudioDataByteSize; u32 mUserData;
 *     u32 packetDescCapacity; u32 mPacketDescriptions; u32 packetDescCount }
 * so the caller gets a LOW mirror with low audio memory; Enqueue copies it into
 * the native buffer. The native output callback maps the native buffer back to
 * its mirror and calls the i386 callback through a cb_bridge trampoline;
 * property listeners likewise. ASBD, AudioTimeStamp and packet descriptions
 * have the same layout on both ABIs.
 *
 * Kill switch M64_NO_AQ_MIRROR=1: AllocateBuffer hands back the truncated
 * native buffer (what the raw bridge amounted to). Guard tests-i386 aq-mirror.
 */
#include <AudioToolbox/AudioQueue.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cb_bridge.h"
#include "gap.h"

extern uint64_t _86x64_unwrap_obj_arg(uint32_t handle);

#define P(v) ((void *)(uintptr_t)(uint32_t)(v))
#define AQ_MAGIC  0x41513332u   /* 'AQ32' */
#define BUF_MAGIC 0x41514231u   /* 'AQB1' */
#define MAX_LISTENERS 8

struct buf32 {
   uint32_t cap, data, size, user, pd_cap, pd, pd_count;   /* i386 AudioQueueBuffer */
   uint32_t magic;
   AudioQueueBufferRef nat;
};

struct listener { AudioQueuePropertyID id; uint32_t proc, user, q32; uint64_t tramp; };

struct aq32 {
   uint32_t magic;
   AudioQueueRef q;
   uint64_t cb_tramp;           /* i386 output callback, as a native trampoline */
   uint32_t user;
   pthread_mutex_t lk;
   struct listener ls[MAX_LISTENERS];
};

/* the i386 callbacks take three 32-bit words: (user, queue, buffer|property) */
static const x64_cb_sig sig3 = { 3, CBR_VOID, { CBA_I32, CBA_I32, CBA_I32 }, { 0 } };

static struct aq32 *aq_of(uint32_t h) {
   struct aq32 *a = P(h);
   return (a && a->magic == AQ_MAGIC) ? a : NULL;
}
static struct buf32 *buf_of(uint32_t h) {
   struct buf32 *b = P(h);
   return (b && b->magic == BUF_MAGIC) ? b : NULL;
}

static void out_cb(void *user, AudioQueueRef q, AudioQueueBufferRef nb) {
   (void)q;
   struct aq32 *a = user;
   struct buf32 *b = nb->mUserData;
   if (!b) return;
   b->size = 0;
   ((void (*)(uint32_t, uint32_t, uint32_t))(uintptr_t)a->cb_tramp)(
      a->user, (uint32_t)(uintptr_t)a, (uint32_t)(uintptr_t)b);
}

/* OSStatus AudioQueueNewOutput(const AudioStreamBasicDescription *,
 *   AudioQueueOutputCallback, void *user, CFRunLoopRef, CFStringRef mode,
 *   UInt32 flags, AudioQueueRef *out) */
uint32_t shim_AudioQueueNewOutput(uint32_t *a) {
   uint32_t *out = P(a[6]);
   if (out) *out = 0;
   if (!out || !a[1]) return (uint32_t)kAudio_ParamError;
   struct aq32 *q = calloc(1, sizeof *q);                    /* low heap */
   q->magic = AQ_MAGIC;
   q->user = a[2];
   q->cb_tramp = x64_cb_wrap(a[1], &sig3);
   pthread_mutex_init(&q->lk, NULL);
   CFRunLoopRef rl = a[3] ? (CFRunLoopRef)(uintptr_t)_86x64_unwrap_obj_arg(a[3]) : NULL;
   CFStringRef mode = a[4] ? (CFStringRef)(uintptr_t)_86x64_unwrap_obj_arg(a[4]) : NULL;
   OSStatus st = AudioQueueNewOutput(P(a[0]), out_cb, q, rl, mode, a[5], &q->q);
   if (st != 0) { free(q); return (uint32_t)st; }
   *out = (uint32_t)(uintptr_t)q;
   return 0;
}

/* OSStatus AudioQueueAllocateBuffer(AudioQueueRef, UInt32 capacity, AudioQueueBufferRef *) */
uint32_t shim_AudioQueueAllocateBuffer(uint32_t *a) {
   uint32_t *out = P(a[2]);
   if (out) *out = 0;
   struct aq32 *q = aq_of(a[0]);
   if (!q || !out) return (uint32_t)kAudio_ParamError;
   AudioQueueBufferRef nb = NULL;
   OSStatus st = AudioQueueAllocateBuffer(q->q, a[1], &nb);
   if (st != 0) return (uint32_t)st;
   static int raw = -1;
   if (raw < 0) raw = getenv("M64_NO_AQ_MIRROR") != NULL;
   if (raw) { *out = (uint32_t)(uintptr_t)nb; return 0; }
   struct buf32 *b = calloc(1, sizeof *b);
   void *data = malloc(a[1] ? a[1] : 1);
   b->cap = a[1]; b->data = (uint32_t)(uintptr_t)data;
   b->magic = BUF_MAGIC; b->nat = nb;
   nb->mUserData = b;
   *out = (uint32_t)(uintptr_t)b;
   return 0;
}

/* OSStatus AudioQueueEnqueueBuffer(AudioQueueRef, AudioQueueBufferRef, UInt32 nDescs,
 *   const AudioStreamPacketDescription *) */
uint32_t shim_AudioQueueEnqueueBuffer(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   struct buf32 *b = buf_of(a[1]);
   if (!q || !b) return (uint32_t)kAudio_ParamError;
   uint32_t n = b->size <= b->cap ? b->size : b->cap;
   memcpy(b->nat->mAudioData, P(b->data), n);
   b->nat->mAudioDataByteSize = n;
   return (uint32_t)AudioQueueEnqueueBuffer(q->q, b->nat, a[2], P(a[3]));
}

/* OSStatus AudioQueueFreeBuffer(AudioQueueRef, AudioQueueBufferRef) */
uint32_t shim_AudioQueueFreeBuffer(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   struct buf32 *b = buf_of(a[1]);
   if (!q || !b) return (uint32_t)kAudio_ParamError;
   OSStatus st = AudioQueueFreeBuffer(q->q, b->nat);
   b->magic = 0;
   free(P(b->data));
   free(b);
   return (uint32_t)st;
}

/* OSStatus AudioQueueDispose(AudioQueueRef, Boolean immediate) */
uint32_t shim_AudioQueueDispose(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   if (!q) return (uint32_t)kAudio_ParamError;
   OSStatus st = AudioQueueDispose(q->q, (Boolean)a[1]);
   /* ponytail: mirrors of buffers the caller never freed leak here (the native
    * buffers die with the queue); track them per queue if a target churns queues */
   q->magic = 0;
   pthread_mutex_destroy(&q->lk);
   free(q);
   return (uint32_t)st;
}

/* OSStatus AudioQueueStart(AudioQueueRef, const AudioTimeStamp *) */
uint32_t shim_AudioQueueStart(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   return q ? (uint32_t)AudioQueueStart(q->q, P(a[1])) : (uint32_t)kAudio_ParamError;
}
/* OSStatus AudioQueueStop(AudioQueueRef, Boolean immediate) */
uint32_t shim_AudioQueueStop(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   return q ? (uint32_t)AudioQueueStop(q->q, (Boolean)a[1]) : (uint32_t)kAudio_ParamError;
}
/* OSStatus AudioQueuePrime(AudioQueueRef, UInt32 frames, UInt32 *outPrepared) */
uint32_t shim_AudioQueuePrime(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   return q ? (uint32_t)AudioQueuePrime(q->q, a[1], P(a[2])) : (uint32_t)kAudio_ParamError;
}
/* OSStatus AudioQueueSetParameter(AudioQueueRef, AudioQueueParameterID, Float32) */
uint32_t shim_AudioQueueSetParameter(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   float v; memcpy(&v, &a[2], 4);
   return q ? (uint32_t)AudioQueueSetParameter(q->q, a[1], v) : (uint32_t)kAudio_ParamError;
}
/* OSStatus AudioQueueGetProperty(AudioQueueRef, AudioQueuePropertyID, void *, UInt32 *) */
uint32_t shim_AudioQueueGetProperty(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   if (!q) return (uint32_t)kAudio_ParamError;
   if (a[1] == kAudioQueueProperty_CurrentDevice || a[1] == kAudioQueueProperty_ChannelLayout ||
       a[1] == kAudioQueueProperty_MagicCookie)
      x64_gap_hit("AudioQueue", "pointer-bearing property passed through", 0, a[1]);
   return (uint32_t)AudioQueueGetProperty(q->q, a[1], P(a[2]), P(a[3]));
}

static void prop_cb(void *user, AudioQueueRef q, AudioQueuePropertyID id) {
   (void)q;
   struct listener *l = user;
   ((void (*)(uint32_t, uint32_t, uint32_t))(uintptr_t)l->tramp)(l->user, l->q32, id);
}

/* OSStatus AudioQueueAddPropertyListener(AudioQueueRef, AudioQueuePropertyID,
 *   AudioQueuePropertyListenerProc, void *user) */
uint32_t shim_AudioQueueAddPropertyListener(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   if (!q || !a[2]) return (uint32_t)kAudio_ParamError;
   pthread_mutex_lock(&q->lk);
   struct listener *l = NULL;
   for (int i = 0; i < MAX_LISTENERS && !l; i++) if (!q->ls[i].proc) l = &q->ls[i];
   if (l) {
      l->id = a[1]; l->proc = a[2]; l->user = a[3]; l->q32 = a[0];
      l->tramp = x64_cb_wrap(a[2], &sig3);
   }
   pthread_mutex_unlock(&q->lk);
   if (!l) { x64_gap_hit("AudioQueue", "more than 8 property listeners", 0, a[1]); return (uint32_t)kAudio_ParamError; }
   return (uint32_t)AudioQueueAddPropertyListener(q->q, a[1], prop_cb, l);
}

/* OSStatus AudioQueueRemovePropertyListener(AudioQueueRef, AudioQueuePropertyID,
 *   AudioQueuePropertyListenerProc, void *user) */
uint32_t shim_AudioQueueRemovePropertyListener(uint32_t *a) {
   struct aq32 *q = aq_of(a[0]);
   if (!q) return (uint32_t)kAudio_ParamError;
   OSStatus st = kAudio_ParamError;
   pthread_mutex_lock(&q->lk);
   for (int i = 0; i < MAX_LISTENERS; i++) {
      struct listener *l = &q->ls[i];
      if (l->proc == a[2] && l->user == a[3] && l->id == a[1]) {
         st = AudioQueueRemovePropertyListener(q->q, a[1], prop_cb, l);
         l->proc = 0;
         break;
      }
   }
   pthread_mutex_unlock(&q->lk);
   return (uint32_t)st;
}
