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

/* ---- RUN B: the app's per-VOICE record, for the 44100-vs-22050 split -------
 *
 * ⚠⚠DIAGNOSTIC ONLY, AND APP-SPECIFIC. Unlike everything else in this file
 * this reads HARDCODED addresses decoded from ONE binary (Halo CE). It must
 * never ship enabled, never influence behaviour, and never be treated as a
 * normal probe. It exists to answer one question that no general instrument
 * can reach, and it is triple-gated: ABICONV_SND_TRACE + ABICONV_SND_DSPROBE +
 * ABICONV_SND_VOICEPROBE, all three required.
 *
 * THE QUESTION. After the __TEXT,__const cohesion fix, Halo's 22050 Hz voices
 * play (370 audible buffers, ring 264600 = 2*22050*6) while every 44100 Hz
 * buffer is still silent (4914/4914, ring 529200 = 2*44100*6). rateTable[1]
 * was never corrupted, so that ring size is CORRECT and the mixer's Lock size
 * arithmetic on that path should be sound. The mixer has a second silent path:
 *
 *     0x248db2:  eax = [R+0x94];  if (!eax) skip this voice
 *
 * i.e. a voice whose SOURCE-DATA pointer is null contributes nothing and the
 * ring stays zero. This reports that pointer for the voice that owns the ring
 * we were just handed, so "the music voice has no source" and "the music voice
 * has a source but nothing writes" separate in a single run.
 *
 * LAYOUT (i386 original): per-voice records at 0x592920, stride 1612;
 *   R+0x650 the IDirectSoundBuffer*, R+0x84 write offset, R+0x88 write end
 *   (-1 = idle), R+0x94 the source-data pointer.
 * The translated image rebases per section: __common 0x457c00 -> 0x10aee4c0,
 * delta 0x106968C0, so the array lands at 0x10C291E0 + the runtime image slide.
 *
 * ★STRUCTURAL GATE, non-negotiable: a record is only believed when
 * *(u32*)(R+0x650) == dsbuf. That identity is to this probe what
 * base + play == samplePtr is to the whole-buffer probe — without it a
 * hardcoded address is a guess, and a guess that printed numbers would be worse
 * than no probe. If no record matches, say so and report nothing else. */
#include <mach-o/dyld.h>

#define VOICE_ARRAY_TRANS 0x10C291E0u   /* 0x592920 + (0x10aee4c0 - 0x457c00) */
#define VOICE_STRIDE      1612u
/* ★HARD BOUND — the section end, not a guess.
 * The array lives in __DATA,__common: i386 0x457c00 + 0x15cf10, translated
 * 0x10aee4c0 + 0x15cf10 = 0x10c4b3d0. From the array base that leaves
 * (0x10c4b3d0 - 0x10C291E0) / 1612 = 86 whole records.
 * ⚠The first version of this probe scanned a flat 256 records and therefore
 * walked 272912 bytes PAST the end of __common into unmapped memory — an
 * out-of-bounds read in a DIAGNOSTIC, i.e. exactly the thing that must never
 * destabilise the target. Halo took an EXC_BAD_ACCESS on the run that carried
 * it. I cannot prove that read was the fault (the .ips frame #0 resolves to no
 * loaded image, and the address did not match this region), but an unbounded
 * scan is indefensible whatever the crash turns out to be, so it is bounded by
 * construction now: the loop cannot step outside the section under any input.
 * Observed voices are 27 and 30, so 86 is ample headroom. */
#define VOICE_COMMON_END  0x10c4b3d0u   /* 0x10aee4c0 + 0x15cf10 */
#define VOICE_SCAN_MAX    86u
#define VOICE_OFF_DSBUF   0x650
#define VOICE_OFF_WROFF   0x84
#define VOICE_OFF_WREND   0x88
#define VOICE_OFF_SOURCE  0x94
#define VOICE_OFF_STATE   0x0c   /* u16 — see snd_voice_state_sweep */
#define VOICE_OFF_F14     0x14   /* u8, adjacent flag: 0x24b8de sets 1, 0x24a605 sets 0 */
#define VOICE_OFF_MODE    0x15   /* u8 — the FALSIFIED "one-shot vs stream" byte */

/* Slide of the image whose name ends in "Halo.dylib", or 0 if absent. Resolved
 * once. Structural (a name suffix), not a path, so a moved bundle still works. */
static intptr_t halo_image_slide(int *found) {
   static int done = 0, ok = 0;
   static intptr_t slide = 0;
   if (!done) {
      uint32_t n = _dyld_image_count();
      for (uint32_t i = 0; i < n; i++) {
         const char *nm = _dyld_get_image_name(i);
         if (!nm) { continue; }
         size_t l = strlen(nm);
         if (l >= 10 && strcmp(nm + l - 10, "Halo.dylib") == 0) {
            slide = _dyld_get_image_vmaddr_slide(i); ok = 1; break;
         }
      }
      done = 1;
   }
   if (found) { *found = ok; }
   return slide;
}

/* ★STATE-TRANSITION SWEEP (task #46, 2026-08-17).
 *
 * WHY A SWEEP AND NOT ANOTHER DUMPED FIELD. `word[R+0x0c]` was already being
 * dumped (as state_hyp) — but ONCE, at the first enqueue for a given ring size,
 * for the single record that owns that ring. Both restrictions defeat the
 * question:
 *   - it is a STATE, not a constant. The "is it finished" query at 0x2482ac
 *     early-outs the moment it reads 0 (`0x2482e7 cmpw $0x0,0xc(%eax)`), and
 *     the mix path at 0x24c333 proceeds ONLY while it is non-zero. What matters
 *     is its value over TIME, not at one arbitrary moment.
 *   - the SILENT music voice is precisely the one that never owns a ring we are
 *     handed, so an owner-only probe structurally cannot see it. Sweeping every
 *     record is the only way it appears at all.
 *
 * WHY `word[R+0x0c]` IS THE GATE. At 0x24c333 the mixer does
 *     cmpw $0x0, 0xc(%ecx)   ; jne PROCEED
 *     cmpb $0x1, 0x9(%esi)   ; jne BAIL          (esi = R+0xc, so byte R+0x15)
 * and that second test can never succeed: `R+0x15` has exactly four writers in
 * the whole sound subsystem — 0x249af7, 0x24a623, 0x24b8ec, 0x24c208 — and all
 * four store $0x0. (Verified on the i386 ORIGINAL, so it is not a translation
 * artifact; no 0x15(%reg) store, and no wider store covers the byte.) The
 * effective mix condition is therefore exactly `word[R+0x0c] != 0`, and the
 * older "armed one-shot but configured as a stream, so make R+0x15 non-zero"
 * reading is dead.
 *
 * Logs only on CHANGE, so a long run stays readable, and reuses the same
 * section-end bound as the owner scan — it cannot step outside __common.
 * R+0x88 rides along because 0x2482ee (`cmpl $-0x1,0x7c(%esi)`, esi = R+0xc,
 * so R+0x88) makes the SAME query report not-playing when it is -1, and the
 * silent voice was measured at -1. */
static void snd_voice_state_sweep(const uint8_t *arr, const uint8_t *arr_end,
                                  uint32_t ringsize) {
   static uint16_t last_state[VOICE_SCAN_MAX];
   static uint8_t  last_f14[VOICE_SCAN_MAX], last_f15[VOICE_SCAN_MAX];
   static uint8_t  seen[VOICE_SCAN_MAX];
   for (uint32_t i = 0; i < VOICE_SCAN_MAX; i++) {
      const uint8_t *R = arr + (size_t)i * VOICE_STRIDE;
      if (R + VOICE_STRIDE > arr_end) { break; }
      uint16_t st  = *(const uint16_t *)(R + VOICE_OFF_STATE);
      uint8_t  f14 = *(const uint8_t  *)(R + VOICE_OFF_F14);
      uint8_t  f15 = *(const uint8_t  *)(R + VOICE_OFF_MODE);
      if (seen[i] && st == last_state[i] &&
          f14 == last_f14[i] && f15 == last_f15[i]) { continue; }
      if (!seen[i]) {
         TR("voicestate: voice=%u FIRST state(R+0x0c)=%u f14=%u f15=%u "
            "wr_end(R+0x88)=%d dsbuf=0x%x ring=%u\n", i, st, f14, f15,
            (int)*(const int32_t *)(R + VOICE_OFF_WREND),
            *(const uint32_t *)(R + VOICE_OFF_DSBUF), ringsize);
      } else {
         TR("voicestate: voice=%u state(R+0x0c) %u->%u f14 %u->%u f15 %u->%u "
            "wr_end(R+0x88)=%d dsbuf=0x%x ring=%u\n", i,
            last_state[i], st, last_f14[i], f14, last_f15[i], f15,
            (int)*(const int32_t *)(R + VOICE_OFF_WREND),
            *(const uint32_t *)(R + VOICE_OFF_DSBUF), ringsize);
      }
      last_state[i] = st; last_f14[i] = f14; last_f15[i] = f15; seen[i] = 1;
   }
}

static void snd_voice_probe(uint32_t dsbuf, uint32_t ringsize) {
   if (!getenv("ABICONV_SND_VOICEPROBE")) { return; }
   int have = 0;
   intptr_t slide = halo_image_slide(&have);
   if (!have) { TR("voiceprobe: no Halo.dylib image — declining\n"); return; }
   const uint8_t *arr = (const uint8_t *)(VOICE_ARRAY_TRANS + slide);

   const uint8_t *arr_end = (const uint8_t *)(VOICE_COMMON_END + slide);

   /* Before the owner scan, and unconditionally: the voice we most need to see
    * is the one that never owns the ring we were handed. */
   snd_voice_state_sweep(arr, arr_end, ringsize);
   for (uint32_t i = 0; i < VOICE_SCAN_MAX; i++) {
      const uint8_t *R = arr + (size_t)i * VOICE_STRIDE;
      /* Belt and braces: never read a record that is not wholly inside the
       * section, even if the constants above are ever edited inconsistently. */
      if (R + VOICE_STRIDE > arr_end) { break; }
      uint32_t owner = *(const uint32_t *)(R + VOICE_OFF_DSBUF);
      if (owner != dsbuf) { continue; }          /* ★the structural gate */

      /* Per-enqueue one-liner: only the two fields whose MEANING is established
       * by the mixer caller's own arithmetic (0x24bb92: dwBytes = play -
       * [esi+0x78]; 0x24bbb8: [esi+0x78] = play), i.e. R+0x84 is the voice's
       * rolling write offset. Everything else is dumped raw below rather than
       * named. */
      TR("voiceprobe: ring=%u voice=%u wr_off=%u\n",
         ringsize, i, *(const uint32_t *)(R + VOICE_OFF_WROFF));

      /* ★RAW WINDOW, once per distinct ring size. Why raw: my first pass
       * NAMED R+0x94 "source" and the control falsified it — the AUDIBLE voice
       * reads 0 there, so that field cannot be what gates playback. The lesson
       * is that a hand-decoded offset is a hypothesis, and printing it under a
       * confident name invites the same mistake twice. So dump the window and
       * let the DIFF between the audible class (ring 264600) and the silent one
       * (ring 529200) name the field. Any candidate must DIFFER between the two
       * classes — that is now a permanent acceptance test, not a nicety.
       * One dump per ring size keeps a 3000-line run readable. */
      static uint32_t dumped[8];
      static int ndumped;
      int seen = 0;
      for (int k = 0; k < ndumped; k++) { if (dumped[k] == ringsize) { seen = 1; } }
      if (!seen && ndumped < (int)(sizeof dumped / sizeof *dumped)) {
         dumped[ndumped++] = ringsize;
         TR("voicedump: ===== ring=%u voice=%u dsbuf=0x%x =====\n",
            ringsize, i, dsbuf);
         for (uint32_t off = 0; off < 0xB0; off += 0x10) {
            const uint32_t *w = (const uint32_t *)(R + off);
            TR("voicedump: ring=%u R+0x%02x: %08x %08x %08x %08x\n",
               ringsize, off, w[0], w[1], w[2], w[3]);
         }
         /* The two words the mixer caller keys its branch selection on, kept
          * adjacent for the eye: byte R+0x15 selects the branch family and the
          * word at R+0x0c is the voice STATE the "is it finished" routine at
          * 0x2482ac steps 2 -> 1 -> 0. Named as HYPOTHESES only. */
         TR("voicedump: ring=%u state_hyp(R+0x0c,u16)=%u mode_hyp(R+0x15,u8)=%u "
            "latch_hyp(R+0x90)=%d\n", ringsize,
            *(const uint16_t *)(R + 0x0c), *(const uint8_t *)(R + 0x15),
            (int)*(const int32_t *)(R + 0x90));
      }
      return;
   }
   TR("voiceprobe: ring=%u no voice record owns dsbuf=0x%x (scanned %u) — "
      "declining to report\n", ringsize, dsbuf, VOICE_SCAN_MAX);
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

   /* WHOLE-BUFFER probe (ABICONV_SND_DSPROBE=1, trace only).
    *
    * The per-enqueue peak above measures only the window we are about to hand
    * to the queue. If every window is silent, that is consistent with two very
    * different worlds: the producer never wrote ANYWHERE (defect upstream of
    * us), or it wrote somewhere else and our cursor arithmetic is reading a
    * silent region (defect OURS). Peaking the ENTIRE backing buffer separates
    * them, and the answer is decisive either way.
    *
    * Layout, decoded from the i386 original's DirectSound-on-Sound-Manager
    * shim: the SoundHeader we were handed is embedded in the app's buffer
    * object at +0x44, so dsbuf = hdr32 - 0x44, with base @+0x38, size @+0x40
    * and the play cursor @+0x98.
    *
    * The identity base + play == samplePtr IS the layout proof — it can only
    * hold if all three fields are what we think they are — so the probe
    * refuses to report unless it holds and the declared size covers the
    * window. A guessed layout that silently reported a peak would be worse
    * than no probe at all. */
   if (snd_trace() && getenv("ABICONV_SND_DSPROBE")) {
      uint32_t dsbuf = hdr32 - 0x44;
      const uint8_t *db = (const uint8_t *)i386_ptr(dsbuf);
      uint32_t base = 0, bsize = 0, play = 0;
      if (db) {
         base  = *(const uint32_t *)(db + 0x38);
         bsize = *(const uint32_t *)(db + 0x40);
         play  = *(const uint32_t *)(db + 0x98);
      }
      if (!db || !base || base + play != d.samplePtr || bsize < nbytes) {
         TR("dsprobe: layout NOT confirmed (dsbuf=0x%x base=0x%x size=%u play=%u "
            "samplePtr=0x%x) — declining to report\n",
            dsbuf, base, bsize, play, d.samplePtr);
      } else {
         const uint8_t *whole = (const uint8_t *)i386_ptr(base);
         uint32_t wpeak = 0, first_nz = 0xffffffffu;
         if (d.bits == 16) {
            uint32_t n = bsize / 2;
            const int16_t *s16 = (const int16_t *)whole;
            for (uint32_t i = 0; i < n; i++) {
               int32_t v = s16[i]; if (v < 0) v = -v;
               if ((uint32_t)v > wpeak) { wpeak = (uint32_t)v;
                  if (first_nz == 0xffffffffu) first_nz = i * 2; }
            }
         } else {
            for (uint32_t i = 0; i < bsize; i++) {
               int32_t v = (int32_t)whole[i] - (d.is_signed ? 0 : 128);
               if (v < 0) v = -v;
               if ((uint32_t)v > wpeak) { wpeak = (uint32_t)v;
                  if (first_nz == 0xffffffffu) first_nz = i; }
            }
         }
         TR("dsprobe: base=0x%x size=%u play=%u whole_peak=%u first_nz=%d  %s\n",
            base, bsize, play, wpeak, (int)first_nz,
            wpeak == 0 ? "<-- ENTIRE BUFFER IS ZERO: the producer never wrote"
                       : "<-- buffer HAS audio: our cursor is reading a silent region");
         snd_voice_probe(dsbuf, bsize);
      }
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

/* ---- SCStatus, the authoritative layout ------------------------------------
 * CarbonSound.framework/Headers/Sound.h (10.6 SDK):
 *
 *   UnsignedFixed scStartTime;         @0
 *   UnsignedFixed scEndTime;           @4
 *   UnsignedFixed scCurrentTime;       @8
 *   Boolean       scChannelBusy;       @12
 *   Boolean       scChannelDisposed;   @13
 *   Boolean       scChannelPaused;     @14
 *   Boolean       scUnused;            @15
 *   unsigned long scChannelAttributes; @16
 *   long          scCPULoad;           @20
 *                                      == 24 bytes
 *
 * ★TWO RULES THIS ENCODES, both universal:
 *  1. The busy/paused flags live at @12/@14, NOT at the end of the record. A
 *     shim that writes them anywhere else leaves scChannelBusy permanently 0,
 *     and every classic "spin until the channel drains" / "find a free voice"
 *     loop then believes every channel is idle forever.
 *  2. `theLength` is the caller's declared buffer size and is a HARD BOUND. A
 *     shim must never write past it — the caller's SCStatus is usually a stack
 *     local, so an overrun corrupts its frame. Clamping to theLength makes the
 *     overrun impossible for any record size, present or future, instead of
 *     merely fixing today's constant. */
#define SC_STATUS_SIZE  24
#define SC_ST_BUSY      12
#define SC_ST_DISPOSED  13
#define SC_ST_PAUSED    14

/* Kill switch for the SCStatus layout fix: 1 restores the original behaviour
 * (a fixed 28-byte clear with the flags written at @24/@26), which both
 * overruns a 24-byte caller record by 4 bytes AND leaves scChannelBusy@12
 * permanently 0. That is the OFF arm of the guard — a real behaviour switch,
 * not a log. */
static int snd_status_fix_disabled(void) {
   static int t = -1;
   if (t < 0) { t = getenv("M64_NO_SND_STATUS_FIX") ? 1 : 0; }
   return t;
}

// SndChannelStatus(chan, short theLength, SCStatusPtr theStatus): report a real
// busy/idle status from the AudioQueue instead of stack garbage.
uint32_t shim_SndChannelStatus(uint32_t *args) {
   uint32_t chan = args[0];
   int16_t  want = (int16_t)(uint16_t)(args[1] & 0xFFFFu);   /* theLength */
   uint8_t *st   = (uint8_t *)PTR(2);
   if (!st) return SND_NO_ERR;

   if (snd_status_fix_disabled()) {            /* OFF arm: the original defect */
      memset(st, 0, 28);
      struct snd_ch *b = chan_lookup(chan);
      if (b) {
         pthread_mutex_lock(&b->lk);
         st[24] = (b->inflight > 0 && !b->paused) ? 1 : 0;
         st[26] = b->paused ? 1 : 0;
         pthread_mutex_unlock(&b->lk);
      }
      return SND_NO_ERR;
   }

   /* Clamp to what the caller declared: never touch a byte it did not offer.
    * A caller that asks for less than the full record (legal — the classic API
    * lets you request a prefix) simply gets the prefix. */
   uint32_t n = (want <= 0) ? 0 : (uint32_t)want;
   if (n > SC_STATUS_SIZE) { n = SC_STATUS_SIZE; }
   memset(st, 0, n);

   struct snd_ch *c = chan_lookup(chan);
   if (c) {
      pthread_mutex_lock(&c->lk);
      int busy   = (c->inflight > 0 && !c->paused);
      int paused = c->paused;
      pthread_mutex_unlock(&c->lk);
      if (n > SC_ST_BUSY)   { st[SC_ST_BUSY]   = busy   ? 1 : 0; }
      if (n > SC_ST_PAUSED) { st[SC_ST_PAUSED] = paused ? 1 : 0; }
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
