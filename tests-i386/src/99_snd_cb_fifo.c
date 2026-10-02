/* 99_snd_cb_fifo — guard for the classic Sound Manager's callBackCmd ORDERING:
 * each callBackCmd fires once the buffers queued BEFORE it have played, every
 * one of them, in order.
 *
 * A streaming app double-buffers with `bufferCmd(chunk k) ; callBackCmd(k)`
 * pairs and refills chunk k from callback k (Portal 2's Bink: one pair per
 * ~39 ms chunk, param2 naming the chunk). The old shim kept ONE pending
 * callback, each new callBackCmd overwriting the last, and fired it only when
 * the whole queue drained: Bink lost every per-chunk completion, refilled in
 * bursts after the queue ran dry, and the intro audio crackled.
 *
 * ON  (default)             : callbacks 1,2,3 arrive, in order.
 * OFF (M64_NO_SND_CB_FIFO=1): only 3 arrives (1 and 2 were overwritten).
 *
 * The buffers are short silent 8-bit mono sounds (standard SoundHeader).
 * Imports are dynamic_lookup, bound at translate time by static-interpose to
 * libabiconv's ___Snd<name> trampolines (maptable_tramp.asm).
 */

extern int  usleep(unsigned int usec);
extern long write(int fd, const void *buf, unsigned long n);
extern void exit(int status);

typedef struct { unsigned short cmd; short param1; unsigned int param2; } SndCommand;
#pragma pack(push, 2)
typedef struct {                       /* classic SoundHeader, stdSH */
   unsigned char *samplePtr; unsigned int length; unsigned int sampleRate;
   unsigned int loopStart, loopEnd; unsigned char encode, baseFrequency;
} SoundHeader;
#pragma pack(pop)

extern short SndNewChannel(void **chan, short synth, int init, void *cb);
extern short SndDoCommand(void *chan, const SndCommand *cmd, unsigned char noWait);
extern short SndDisposeChannel(void *chan, unsigned char quietNow);

#define callBackCmd  13
#define bufferCmd    81
#define sampledSynth  5

static void put(const char *s) { unsigned long n = 0; while (s[n]) n++; write(1, s, n); }
static void put_digit(int d) { char c = (char)('0' + d); write(1, &c, 1); }

static unsigned char g_silence[2205];          /* 0.1 s at 22050 Hz, 8-bit unsigned */
static SoundHeader   g_hdr[3];
static volatile int  g_seen[8], g_n;

static void my_cb(void *chan, SndCommand *cmd) {
   (void)chan;
   if (g_n < 8) { g_seen[g_n] = (int)cmd->param2; }
   g_n++;
}

int main(void) {
   void *chan = 0;
   if (SndNewChannel(&chan, sampledSynth, 0, (void *)my_cb) != 0 || !chan) {
      put("SKIP no channel\n"); exit(2);
   }
   for (unsigned i = 0; i < sizeof g_silence; i++) { g_silence[i] = 0x80; }
   for (int k = 0; k < 3; k++) {
      g_hdr[k].samplePtr = g_silence; g_hdr[k].length = sizeof g_silence;
      g_hdr[k].sampleRate = 0x56220000u;     /* 22050 Hz, Fixed 16.16 */
      g_hdr[k].encode = 0; g_hdr[k].baseFrequency = 60;
      SndCommand b = { bufferCmd, 0, (unsigned int)&g_hdr[k] };
      SndCommand c = { callBackCmd, 0, (unsigned int)(k + 1) };
      SndDoCommand(chan, &b, 0);
      SndDoCommand(chan, &c, 0);
   }
   for (int i = 0; i < 200 && g_n < 3; i++) { usleep(10000); }   /* 0.3 s of audio */
   usleep(100000);                                               /* stragglers */
   SndDisposeChannel(chan, 1);

   put("callbacks=");
   for (int i = 0; i < g_n && i < 8; i++) { put_digit(g_seen[i]); }
   put("\n");
   if (g_n == 3 && g_seen[0] == 1 && g_seen[1] == 2 && g_seen[2] == 3) { put("PASS\n"); exit(0); }
   put("FAIL\n"); exit(1);
}
