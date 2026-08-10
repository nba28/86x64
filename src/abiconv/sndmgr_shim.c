// sndmgr_shim.c — a faithful reimplementation of the classic Sound Manager
// (removed from 64-bit macOS) on top of CoreAudio's AudioQueue.
//
// WHY: SndNewChannel / SndDoCommand / SndDoImmediate / SndPlay / SndChannelStatus
// / SndDisposeChannel and the SndCommand queue (bufferCmd / soundCmd / quietCmd /
// flushCmd / ampCmd / volumeCmd / rateCmd / rateMultiplierCmd / callBackCmd /
// pauseCmd / resumeCmd) are the classic SFX/music path for pre-Carbon-era games
// (Halo CE, Civ IV, ...). The whole Sound Manager was deleted from modern macOS,
// so the i386 imports bind here. Rather than no-op them (which silences all
// classic audio), we regain REAL functionality: a SndChannel becomes an
// AudioQueue, bufferCmd/soundCmd enqueue the converted PCM, the gain/rate
// commands map to queue parameters, and callBackCmd marshals the i386
// SndCallBackUPP back to the translated program when its buffer completes.
//
// The format decoding (SoundHeader / ExtSoundHeader / 'snd ' resource) lives in
// the pure, framework-free snd_convert.c so it can be unit-tested headless
// (snd_convert_test.c). This file adds the i386 pointer following (the app heap
// is low-4GB dereferenceable) + the AudioQueue lifecycle + the callback bridge.
//
// Universal: triggers on the removed-Sound-Manager symbol shape, benefits any
// legacy i386 app that plays audio through the Sound Manager. No app-specific
// logic. Degrades gracefully (returns noErr, plays nothing) if no output device
// is available (e.g. a headless host), so it never fails an app that gates on
// SndNewChannel succeeding (Halo's CantAllocSndChannel self-check).
//
// MTSHIM convention: rdi -> &i386 args[0]; OSErr result in eax. The trampolines
// (___SndNewChannel/... -> _shim_Snd*) already exist in maptable_tramp.asm.

#include <stdint.h>
#include <string.h>
#include <stdlib.h>     /* malloc/free/getenv == libabiconv low-4GB heap */
#include <stdio.h>
#include <pthread.h>
#include <AudioToolbox/AudioToolbox.h>

#include "snd_convert.h"
#include "carbon_shim.h"   /* cm_handle_block / cm_handle_size / i386_ptr / to_i386 */

/* objc_reverse.asm: lay nwords i386 cdecl args + a return frame on a low-4GB
 * stack, enter the translated fn, return its eax. Used to invoke SndCallBackUPP. */
extern uint32_t _86x64_call_i386(uint64_t fn, uint64_t nwords,
                                 const uint32_t *words, uint64_t lowstack_top);

#define SND_NO_ERR (0)
/* Classic MemError/Sound Manager out-of-memory (Memory Manager `memFullErr`,
 * which is what the classic Sound Manager returns when it cannot allocate a
 * channel). Used only on the allocation-failure path, which must still leave
 * the caller's out-param defined — see shim_SndNewChannel. */
#define SND_NOT_ENOUGH_MEMORY ((uint32_t)(int32_t)-108)
#define PTR(n) ((void *)(uintptr_t)args[(n)])

/* ---- classic SndChannel field offsets (pack(2), i386) --------------------- */
#define SC_NEXTCHAN   0    /* SndChannelPtr */
#define SC_FIRSTMOD   4    /* Ptr */
#define SC_CALLBACK   8    /* SndCallBackUPP (i386 fn ptr) */
#define SC_USERINFO   12   /* long */
#define SC_WAIT       16
#define SC_CMDINPROG  20   /* SndCommand (8 bytes): reused to pass &cmd to cb */
#define SC_FLAGS      28
#define SC_QLENGTH    30
#define SC_QHEAD      32
#define SC_QTAIL      34
#define SC_QUEUE      36   /* SndCommand[128] */
#define SC_SIZE       (SC_QUEUE + 128 * 8)   /* 1060 bytes */

/* SndCommand fields (pack(2), i386) */
#define CMD_CMD    0   /* u16 */
#define CMD_PARAM1 2   /* s16 */
#define CMD_PARAM2 4   /* u32 (long) */

static int snd_trace(void) {
   static int t = -1;
   if (t < 0) { const char *e = getenv("ABICONV_SND_TRACE"); t = (e && *e && *e != '0') ? 1 : 0; }
   return t;
}
#define TR(...) do { if (snd_trace()) { fprintf(stderr, "[snd] " __VA_ARGS__); } } while (0)

/* sample-endianness override: default follows the decoded header endianness.
 * ABICONV_SND_LE / ABICONV_SND_BE force a byte order for 16-bit samples (a tuning
 * knob for targets whose PCM byte order differs from their header fields). */
static int snd_force_endian(void) {   /* -1 auto, 0 LE, 1 BE */
   static int e = -2;
   if (e == -2) {
      if (getenv("ABICONV_SND_BE"))      e = 1;
      else if (getenv("ABICONV_SND_LE")) e = 0;
      else                               e = -1;
   }
   return e;
}

/* ---- per-channel state ---------------------------------------------------- */
struct snd_ch {
   uint32_t                    chan;      /* app-visible SndChannelPtr (low-4GB) — key */
   AudioQueueRef               aq;        /* NULL until first buffer / no device */
   AudioStreamBasicDescription fmt;       /* current queue format */
   int                         fmt_valid;
   int                         running;
   int                         paused;
   float                       gain;      /* 0..1 (ampCmd/volumeCmd) */
   double                      rate_mult; /* rateCmd/rateMultiplierCmd (1.0 = normal) */
   int                         inflight;  /* buffers enqueued & not yet completed */
   /* a callBackCmd queued behind unfinished buffers: fired when the channel
    * next drains (inflight -> 0), classic-faithfully (the callback signals the
    * app that the preceding sound finished). */
   int                         cb_pending;
   uint16_t                    cb_cmd;
   int16_t                     cb_p1;
   uint32_t                    cb_p2;
   pthread_mutex_t             lk;
   struct snd_ch              *next;
};

static struct snd_ch  *g_chans;
static pthread_mutex_t g_reg = PTHREAD_MUTEX_INITIALIZER;

static struct snd_ch *chan_lookup(uint32_t chan) {
   struct snd_ch *c;
   pthread_mutex_lock(&g_reg);
   for (c = g_chans; c; c = c->next) if (c->chan == chan) break;
   pthread_mutex_unlock(&g_reg);
   return c;
}

/* Invoke the i386 SndCallBackUPP(chan, &cmd) on a fresh low-4GB stack. Writes
 * the command into the channel's cmdInProgress slot so the callback's cmd->param1
 * / cmd->param2 reads land in the app's low-4GB block, classic-faithfully. */
static void fire_callback(uint32_t chan, uint16_t cmd, int16_t p1, uint32_t p2) {
   if (!chan) return;
   uint8_t *cb_ptr = (uint8_t *)i386_ptr(chan + SC_CALLBACK);
   uint32_t upp = *(uint32_t *)cb_ptr;
   if (!upp) return;
   /* stage the SndCommand into cmdInProgress (@+20) */
   uint8_t *cip = (uint8_t *)i386_ptr(chan + SC_CMDINPROG);
   *(uint16_t *)(cip + CMD_CMD)    = cmd;
   *(int16_t  *)(cip + CMD_PARAM1) = p1;
   *(uint32_t *)(cip + CMD_PARAM2) = p2;

   void *stk = malloc(256 * 1024);   /* low-4GB */
   if (!stk) return;
   uint64_t top = ((uint64_t)(uintptr_t)stk + 256 * 1024) & ~0xfULL;
   uint32_t words[2] = { chan, chan + SC_CMDINPROG };
   TR("fire callBack upp=0x%x chan=0x%x p1=%d p2=0x%x\n", upp, chan, p1, p2);
   _86x64_call_i386((uint64_t)upp, 2, words, top);
   free(stk);
}

/* AudioQueue completion callback (runs on the queue's internal thread). */
/* Kill switch for the AudioQueue lock-ordering fix: 1 restores the original
 * order (blocking AudioQueue call made WHILE holding c->lk), which deadlocks
 * the caller against aq_output_cb. That is the OFF arm of the guard — it must
 * genuinely reproduce the hang, so it is a real behaviour switch, not a log. */
static int snd_lock_fix_disabled(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_SND_LOCK_FIX") ? 1 : 0; }
   return t;
}

static void aq_output_cb(void *ud, AudioQueueRef aq, AudioQueueBufferRef buf) {
   struct snd_ch *c = (struct snd_ch *)ud;
   uint16_t cmd = 0; int16_t p1 = 0; uint32_t p2 = 0; int fire = 0;
   pthread_mutex_lock(&c->lk);
   if (c->inflight > 0) c->inflight--;
   if (c->inflight == 0 && c->cb_pending) {   /* channel drained: fire the pending callBack */
      cmd = c->cb_cmd; p1 = c->cb_p1; p2 = c->cb_p2; c->cb_pending = 0; fire = 1;
   }
   pthread_mutex_unlock(&c->lk);
   AudioQueueFreeBuffer(aq, buf);
   if (fire) fire_callback(c->chan, cmd, p1, p2);
}

/* (Re)create the channel's AudioQueue for `fmt`. Returns 0 on success, -1 if no
 * output device (headless). Caller holds c->lk. */
static int ensure_queue(struct snd_ch *c, const AudioStreamBasicDescription *fmt) {
   if (c->aq && c->fmt_valid && memcmp(&c->fmt, fmt, sizeof *fmt) == 0)
      return 0;                       /* already the right format */
   /* ASYNCHRONOUS dispose on purpose: this runs with c->lk HELD (see the
    * function comment), and a synchronous dispose blocks on pending callbacks
    * while aq_output_cb is waiting for that very lock — the deadlock documented
    * in do_command. Async dispose does not wait, so it cannot deadlock. A late
    * callback from the old queue is harmless: it still targets this same live
    * channel and its inflight decrement is guarded by `> 0`. */
   if (c->aq) { AudioQueueDispose(c->aq, false); c->aq = NULL; c->fmt_valid = 0; c->running = 0; c->inflight = 0; }
   AudioQueueRef q = NULL;
   OSStatus st = AudioQueueNewOutput(fmt, aq_output_cb, c, NULL, NULL, 0, &q);
   if (st != noErr || !q) { TR("AudioQueueNewOutput failed st=%d (no device?)\n", (int)st); return -1; }
   c->aq = q; c->fmt = *fmt; c->fmt_valid = 1;
   AudioQueueSetParameter(q, kAudioQueueParam_Volume, c->gain);
   return 0;
}

/* Build an ASBD from a decoded PCM description, applying the channel rate mult. */
static void desc_to_asbd(const struct snd_ch *c, const snd_pcm_desc *d,
                         AudioStreamBasicDescription *a) {
   memset(a, 0, sizeof *a);
   int be = d->is_bigendian;
   if (snd_force_endian() == 0) be = 0;
   else if (snd_force_endian() == 1) be = 1;
   a->mSampleRate       = d->sample_rate * (c->rate_mult > 0 ? c->rate_mult : 1.0);
   a->mFormatID         = kAudioFormatLinearPCM;
   a->mChannelsPerFrame = d->channels;
   a->mBitsPerChannel   = d->bits;
   a->mFramesPerPacket  = 1;
   a->mBytesPerFrame    = d->channels * (d->bits / 8);
   a->mBytesPerPacket   = a->mBytesPerFrame;
   UInt32 fl = kAudioFormatFlagIsPacked;
   if (d->is_signed)              fl |= kAudioFormatFlagIsSignedInteger;
   if (be && d->bits > 8)         fl |= kAudioFormatFlagIsBigEndian;
   a->mFormatFlags = fl;
}

/* Follow a SoundHeader pointer, decode it, and enqueue its PCM on the channel.
 * hdr32 = i386 address of the SoundHeader. Returns 0 (played or gracefully
 * skipped) — never an error, so a missing device / unsupported codec can't abort
 * the app. Optionally attaches a pending callBackCmd to the enqueued buffer. */
static void enqueue_sound(struct snd_ch *c, uint32_t hdr32) {
   if (!hdr32) return;
   const uint8_t *hdr = (const uint8_t *)i386_ptr(hdr32);
   snd_pcm_desc d;
   int r = snd_header_parse(hdr, 4096, hdr32, &d);
   if (r != 0) { TR("enqueue: header rc=%d (encode=0x%x) declined\n", r, hdr ? hdr[20] : 0); return; }

   const uint8_t *samples = d.samplePtr ? (const uint8_t *)i386_ptr(d.samplePtr)
                                        : hdr + d.inline_off;
   uint32_t nbytes = d.nbytes;
   if (nbytes == 0 || nbytes > (64u << 20)) { TR("enqueue: nbytes=%u out of range\n", nbytes); return; }

   AudioStreamBasicDescription fmt;
   desc_to_asbd(c, &d, &fmt);
   /* Trace-only: peak amplitude of the PCM we are about to hand to the queue.
    * Distinguishes "the pipeline works but we are enqueueing SILENCE" (bad
    * sample pointer / wrong decode) from "real audio goes out and is inaudible
    * for some other reason". Costs nothing unless tracing is on. */
   if (snd_trace()) {
      uint32_t peak = 0;
      if (d.bits == 16) {
         uint32_t n = nbytes / 2;
         const int16_t *s16 = (const int16_t *)samples;
         for (uint32_t i = 0; i < n; i++) {
            int32_t v = s16[i]; if (v < 0) v = -v;
            if ((uint32_t)v > peak) peak = (uint32_t)v;
         }
      } else {
         for (uint32_t i = 0; i < nbytes; i++) {
            int32_t v = (int32_t)samples[i] - (d.is_signed ? 0 : 128);
            if (v < 0) v = -v;
            if ((uint32_t)v > peak) peak = (uint32_t)v;
         }
      }
      TR("enqueue rate=%.1f ch=%u bits=%u %s nbytes=%u ptr=0x%x peak=%u%s\n",
         fmt.mSampleRate, d.channels, d.bits, d.is_signed ? "S" : "U", nbytes,
         d.samplePtr, peak, peak == 0 ? "  <-- SILENT BUFFER" : "");
   }

   pthread_mutex_lock(&c->lk);
   if (ensure_queue(c, &fmt) != 0) { pthread_mutex_unlock(&c->lk); return; }
   AudioQueueBufferRef buf = NULL;
   if (AudioQueueAllocateBuffer(c->aq, nbytes, &buf) != noErr || !buf) { pthread_mutex_unlock(&c->lk); return; }
   memcpy(buf->mAudioData, samples, nbytes);
   buf->mAudioDataByteSize = nbytes;
   buf->mUserData = NULL;
   if (AudioQueueEnqueueBuffer(c->aq, buf, 0, NULL) != noErr) { AudioQueueFreeBuffer(c->aq, buf); pthread_mutex_unlock(&c->lk); return; }
   c->inflight++;
   if (!c->running) { AudioQueueStart(c->aq, NULL); c->running = 1; c->paused = 0; }
   pthread_mutex_unlock(&c->lk);
}

/* Apply one SndCommand to the channel. `cmd32` = i386 address of the SndCommand
 * (8 bytes). Returns OSErr (noErr). */
static uint32_t do_command(uint32_t chan, uint32_t cmd32) {
   if (!cmd32) return SND_NO_ERR;
   struct snd_ch *c = chan_lookup(chan);
   if (!c) { TR("do_command: unknown chan 0x%x\n", chan); return SND_NO_ERR; }
   const uint8_t *cp = (const uint8_t *)i386_ptr(cmd32);
   uint16_t cmd = *(const uint16_t *)(cp + CMD_CMD);
   int16_t  p1  = *(const int16_t  *)(cp + CMD_PARAM1);
   uint32_t p2  = *(const uint32_t *)(cp + CMD_PARAM2);
   uint16_t base = cmd & ~(uint16_t)SND_DATA_OFFSET_FLAG;

   switch (base) {
   case snd_bufferCmd:
   case snd_soundCmd:
      /* param2 = pointer to a SoundHeader (dataOffsetFlag rarely set on live
       * channel commands; treat param2 as an absolute i386 pointer). */
      enqueue_sound(c, p2);
      break;
   case snd_ampCmd:                         /* param1 = 0..255 amplitude */
      pthread_mutex_lock(&c->lk);
      c->gain = (p1 < 0 ? 0 : (p1 > 255 ? 255 : p1)) / 255.0f;
      if (c->aq) AudioQueueSetParameter(c->aq, kAudioQueueParam_Volume, c->gain);
      pthread_mutex_unlock(&c->lk);
      break;
   case snd_volumeCmd: {                    /* param2 = left|right<<16, 0x100=unity */
      uint32_t left = p2 & 0xFFFF, right = (p2 >> 16) & 0xFFFF;
      float g = ((left + right) / 2.0f) / 256.0f;
      if (g < 0) g = 0; if (g > 2.0f) g = 2.0f;
      pthread_mutex_lock(&c->lk);
      c->gain = g;
      if (c->aq) AudioQueueSetParameter(c->aq, kAudioQueueParam_Volume, c->gain);
      pthread_mutex_unlock(&c->lk);
      break;
   }
   case snd_getVolumeCmd: {                 /* write current volume to *param2 */
      if (p2) { uint32_t v = (uint32_t)(c->gain * 256.0f); *(uint32_t *)i386_ptr(p2) = (v << 16) | v; }
      break;
   }
   case snd_rateCmd:
   case snd_rateMultiplierCmd:              /* param2 = Fixed 16.16, 0x10000=normal */
      pthread_mutex_lock(&c->lk);
      c->rate_mult = p2 ? (double)p2 / 65536.0 : 1.0;
      pthread_mutex_unlock(&c->lk);
      break;
   case snd_getRateMultiplierCmd:
      if (p2) *(uint32_t *)i386_ptr(p2) = (uint32_t)(c->rate_mult * 65536.0);
      break;
   /* ★DEADLOCK RULE: AudioQueueReset, AudioQueueStop(...,true) and
    * AudioQueueDispose(...,true) all BLOCK until every pending buffer callback
    * has completed (AQ::API::Queue::AwaitAllPendingCallbacks). Our callback,
    * aq_output_cb, takes c->lk on entry. Calling any of them WHILE HOLDING
    * c->lk therefore deadlocks the caller against its own callback: the caller
    * holds the lock and waits for the callback to drain; the callback waits for
    * the lock. Nothing times out — the thread is parked forever.
    *
    * MEASURED, Halo (2026-08-10): pressing Return on a main-menu item plays the
    * confirm sound, which issues snd_flushCmd. 3364 of 3364 samples had the
    * MAIN THREAD in shim_SndDoImmediate -> do_command -> AudioQueueReset ->
    * AwaitAllPendingCallbacks -> pthread_cond_wait. The menu never advanced
    * because the app was wedged inside the click sound, which is also why the
    * game read as "frozen" while still rendering.
    *
    * The fix everywhere below: mutate our own state UNDER the lock, snapshot
    * the queue handle, RELEASE the lock, then make the blocking call. The
    * callback can then run to completion and the blocking call returns.
    * Kill switch M64_NO_SND_LOCK_FIX=1 restores the deadlocking order. */
   case snd_quietCmd: {                     /* stop immediately, drop queue */
      pthread_mutex_lock(&c->lk);
      AudioQueueRef q = c->aq;
      c->running = 0; c->paused = 0; c->inflight = 0;
      if (snd_lock_fix_disabled()) {
         if (q) AudioQueueStop(q, true);
         pthread_mutex_unlock(&c->lk);
      } else {
         pthread_mutex_unlock(&c->lk);
         if (q) AudioQueueStop(q, true);
      }
      break;
   }
   case snd_flushCmd: {                     /* drop queued (not-yet-played) buffers */
      pthread_mutex_lock(&c->lk);
      AudioQueueRef q = c->aq;
      c->inflight = 0;
      if (snd_lock_fix_disabled()) {
         if (q) AudioQueueReset(q);
         pthread_mutex_unlock(&c->lk);
      } else {
         pthread_mutex_unlock(&c->lk);
         if (q) AudioQueueReset(q);
      }
      break;
   }
   case snd_pauseCmd:
      pthread_mutex_lock(&c->lk);
      if (c->aq && c->running && !c->paused) { AudioQueuePause(c->aq); c->paused = 1; }
      pthread_mutex_unlock(&c->lk);
      break;
   case snd_resumeCmd:
      pthread_mutex_lock(&c->lk);
      if (c->aq && c->paused) { AudioQueueStart(c->aq, NULL); c->paused = 0; }
      pthread_mutex_unlock(&c->lk);
      break;
   case snd_callBackCmd: {                  /* SndCallBackUPP(chan,&cmd) when sound done */
      pthread_mutex_lock(&c->lk);
      int fire_now = (c->inflight == 0);     /* channel idle: fire immediately */
      if (!fire_now) { c->cb_pending = 1; c->cb_cmd = cmd; c->cb_p1 = p1; c->cb_p2 = p2; }
      pthread_mutex_unlock(&c->lk);
      if (fire_now) fire_callback(chan, cmd, p1, p2);
      break;
   }
   case snd_reInitCmd:   /* reinit channel — format handled per-buffer */
   case snd_syncCmd:     /* multi-channel sync — single-queue no-op */
   case snd_waitCmd:     /* wait N ticks — audio timing owns pacing */
   case snd_availableCmd:
   case snd_versionCmd:
   case snd_nullCmd:
   default:
      TR("do_command: cmd=%u (noErr passthrough)\n", base);
      break;
   }
   return SND_NO_ERR;
}

/* ---- exported MTSHIM entry points ----------------------------------------- */

// SndNewChannel(SndChannelPtr *chan, short synth, SInt32 init, SndCallBackUPP cb)
uint32_t shim_SndNewChannel(uint32_t *args) {
   uint32_t *chanpp   = (uint32_t *)PTR(0);   /* SndChannelPtr* */
   uint32_t  incoming = chanpp ? *chanpp : 0; /* app may preallocate the channel */
   uint32_t  cb       = args[3];              /* SndCallBackUPP (i386 fn ptr) */

   uint32_t block = incoming;
   if (!block) {
      void *p = malloc(SC_SIZE);              /* low-4GB, faithful SndChannel */
      if (!p) {
         /* RULE A: a failing shim must leave every out-param DEFINED. Returning
          * "success" while *chanpp keeps whatever the caller's stack happened to
          * hold is the FSpMakeFSRef defect verbatim — the app then stores an
          * undefined word into its channel table and dereferences it later, far
          * from here. Report the classic out-of-memory error AND define the
          * out-param, so a caller that checks either one behaves sanely. */
         if (chanpp) { *chanpp = 0; }
         return SND_NOT_ENOUGH_MEMORY;
      }
      memset(p, 0, SC_SIZE);
      block = (uint32_t)(uintptr_t)p;
   }
   /* store the callback into the classic callBack field (@+8) */
   *(uint32_t *)i386_ptr(block + SC_CALLBACK) = cb;

   struct snd_ch *c = (struct snd_ch *)calloc(1, sizeof *c);
   if (c) {
      c->chan = block; c->gain = 1.0f; c->rate_mult = 1.0;
      pthread_mutex_init(&c->lk, NULL);
      pthread_mutex_lock(&g_reg);
      c->next = g_chans; g_chans = c;
      pthread_mutex_unlock(&g_reg);
   }
   if (chanpp) *chanpp = block;
   TR("SndNewChannel -> chan=0x%x cb=0x%x\n", block, cb);
   return SND_NO_ERR;
}

// OSErr SndDoCommand(SndChannelPtr chan, const SndCommand *cmd, Boolean noWait)
uint32_t shim_SndDoCommand(uint32_t *args)   { return do_command(args[0], args[1]); }
// OSErr SndDoImmediate(SndChannelPtr chan, const SndCommand *cmd)
uint32_t shim_SndDoImmediate(uint32_t *args) { return do_command(args[0], args[1]); }

// OSErr SndPlay(SndChannelPtr chan, SndListHandle sndHandle, Boolean async)
uint32_t shim_SndPlay(uint32_t *args) {
   uint32_t chan = args[0];
   uint32_t hdl  = args[1];
   if (!chan || !hdl) return SND_NO_ERR;
   struct snd_ch *c = chan_lookup(chan);
   if (!c) return SND_NO_ERR;

   /* Handle -> resource bytes. rm_shim/carbon_memory model: cm_handle_block
    * gives the data block, cm_handle_size its length. Fall back to a manual
    * *(Handle) deref with a capped read if it isn't a carbon Handle. */
   const uint8_t *res = (const uint8_t *)cm_handle_block(hdl);
   uint32_t reslen = cm_handle_size(hdl);
   if (!res) {
      uint32_t master = *(uint32_t *)i386_ptr(hdl);
      if (!master) return SND_NO_ERR;
      res = (const uint8_t *)i386_ptr(master);
      reslen = 64 * 1024;   /* capped: find_header/parse bound their own reads */
   }
   uint32_t res_base = to_i386(res);
   uint32_t off = 0;
   if (snd_resource_find_header(res, reslen, res_base, &off) != 0) {
      TR("SndPlay: no sound header in resource (len=%u)\n", reslen);
      return SND_NO_ERR;
   }
   enqueue_sound(c, res_base + off);
   return SND_NO_ERR;
}

// SndChannelStatus(chan, short theLength, SCStatusPtr theStatus): report a real
// busy/idle status from the AudioQueue instead of stack garbage.
uint32_t shim_SndChannelStatus(uint32_t *args) {
   uint32_t chan = args[0];
   void *st = PTR(2);
   if (!st) return SND_NO_ERR;
   memset(st, 0, 28);                          /* SCStatus is 28 bytes */
   struct snd_ch *c = chan_lookup(chan);
   if (c) {
      pthread_mutex_lock(&c->lk);
      /* SCStatus.scChannelBusy @24 (Boolean); scChannelPaused @26 */
      ((uint8_t *)st)[24] = (c->inflight > 0 && !c->paused) ? 1 : 0;
      ((uint8_t *)st)[26] = c->paused ? 1 : 0;
      pthread_mutex_unlock(&c->lk);
   }
   return SND_NO_ERR;
}

// OSErr SndDisposeChannel(SndChannelPtr chan, Boolean quietNow)
uint32_t shim_SndDisposeChannel(uint32_t *args) {
   uint32_t chan = args[0];
   pthread_mutex_lock(&g_reg);
   struct snd_ch **pp = &g_chans, *c = NULL;
   while (*pp) { if ((*pp)->chan == chan) { c = *pp; *pp = c->next; break; } pp = &(*pp)->next; }
   pthread_mutex_unlock(&g_reg);
   if (c) {
      /* Same deadlock rule as do_command: both of these block on pending
       * callbacks, and aq_output_cb takes c->lk. Detach the queue under the
       * lock, then tear it down without it. The synchronous dispose still
       * completes before we destroy the mutex and free the channel, so the
       * callback can never outlive its userdata. */
      pthread_mutex_lock(&c->lk);
      AudioQueueRef q = c->aq;
      c->aq = NULL; c->running = 0; c->paused = 0; c->inflight = 0;
      pthread_mutex_unlock(&c->lk);
      if (q) { AudioQueueStop(q, true); AudioQueueDispose(q, true); }
      pthread_mutex_destroy(&c->lk);
      free(c);
   }
   TR("SndDisposeChannel chan=0x%x\n", chan);
   return SND_NO_ERR;
}

// SysBeep(short duration): the system alert sound (real functionality via
// AudioServices — no AppKit dependency). duration is ignored (modern alert).
void shim_SysBeep(uint32_t *args) {
   (void)args;
   AudioServicesPlayAlertSound(kSystemSoundID_UserPreferredAlert);
}
