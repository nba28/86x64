/*
 * 99_aq_mirror — the AudioQueue output API for i386 callers (aq_shim.c): a low
 * queue handle, buffers in the i386 AudioQueueBuffer layout with low audio
 * memory, and an output callback that gets those same buffers back. Portal 2
 * CAudioDeviceAudioQueue::OpenWaveOut: AudioQueueAllocateBuffer failed through
 * the raw bridge and the game wrote the NULL buffer's mAudioDataByteSize.
 *
 * Plays 3 x 4 KB of SILENCE. Exit 42 = every buffer came back through the
 * callback as the caller's own pointer. Exit 2 = no output device (SKIP).
 * Kill switch M64_NO_AQ_MIRROR=1 (run time): a fault or a wrong exit.
 */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { uint32_t cap; void *data; uint32_t size; void *user; uint32_t pdcap; void *pd; uint32_t pdcount; } AQBuf;
typedef struct { double rate; uint32_t fmt, flags, bpp, fpp, bpf, ch, bits, res; } ASBD;
typedef void (*AQCb)(void *, void *, AQBuf *);
extern int32_t AudioQueueNewOutput(const ASBD *, AQCb, void *, void *, void *, uint32_t, void **);
extern int32_t AudioQueueAllocateBuffer(void *, uint32_t, AQBuf **);
extern int32_t AudioQueueEnqueueBuffer(void *, AQBuf *, uint32_t, const void *);
extern int32_t AudioQueueStart(void *, const void *);
extern int32_t AudioQueueStop(void *, unsigned char);
extern int32_t AudioQueueDispose(void *, unsigned char);

static AQBuf *bufs[3];
static void *gq;
static volatile int back, wrong;

static void cb(void *user, void *q, AQBuf *b) {
   if (user != (void *)0x1234 || q != gq) { wrong++; return; }
   for (int i = 0; i < 3; i++) if (b == bufs[i]) { back |= 1 << i; return; }
   wrong++;
}

int main(void) {
   ASBD f = { 44100.0, 0x6c70636d /* 'lpcm' */, 0xc /* signed int, packed */, 4, 1, 4, 2, 16, 0 };
   if (AudioQueueNewOutput(&f, cb, (void *)0x1234, 0, 0, 0, &gq) != 0 || !gq) exit(2);
   for (int i = 0; i < 3; i++) {
      if (AudioQueueAllocateBuffer(gq, 4096, &bufs[i]) != 0 || !bufs[i]) exit(3);
      memset(bufs[i]->data, 0, 4096);
      bufs[i]->size = 4096;
      if (AudioQueueEnqueueBuffer(gq, bufs[i], 0, 0) != 0) exit(4);
   }
   if (AudioQueueStart(gq, 0) != 0) exit(5);
   for (int t = 0; t < 200 && back != 7; t++) usleep(10000);
   AudioQueueStop(gq, 1);
   AudioQueueDispose(gq, 1);
   exit(back == 7 && !wrong ? 42 : 6);
}
