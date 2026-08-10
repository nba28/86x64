/* 99_snd_lock_order — guard for the classic Sound Manager's AudioQueue
 * LOCK-ORDERING contract (src/abiconv/sndmgr_shim.c).
 *
 * THE DEFECT. AudioQueueReset, AudioQueueStop(...,true) and
 * AudioQueueDispose(...,true) all block inside
 * AQ::API::Queue::AwaitAllPendingCallbacks until every pending buffer callback
 * has run. Our completion callback, aq_output_cb, takes the channel lock c->lk
 * on entry. So calling any of them WHILE HOLDING c->lk parks the caller
 * forever: the caller holds the lock and waits for the callbacks to drain, and
 * the callbacks wait for the lock. Nothing times out.
 *
 * MEASURED on Halo CE (2026-08-10): pressing Return on a main-menu item plays
 * the confirm sound, which issues snd_flushCmd. 3364 of 3364 `sample` frames
 * had the MAIN THREAD in shim_SndDoImmediate -> do_command -> AudioQueueReset
 * -> AwaitAllPendingCallbacks -> pthread_cond_wait. The menu never advanced,
 * and the game read as "frozen" while still rendering its last frame.
 *
 * THE FIX (ON arm): mutate our own channel state under the lock, snapshot the
 * AudioQueueRef, RELEASE the lock, and only then make the blocking call.
 *
 * OFF arm: M64_NO_SND_LOCK_FIX=1 restores the original order, so this fixture
 * must HANG at the first flushCmd. The test script runs it under a watchdog and
 * fails the guard if the OFF arm completes — a clean OFF arm would mean the
 * fixture never got a live queue with callbacks pending, i.e. the guard is not
 * exercising the fix.
 *
 * WHY IT NEEDS A REAL QUEUE. The deadlock only exists once buffers are actually
 * enqueued on a started AudioQueue, so this fixture builds a real classic
 * stdSH SoundHeader and plays it. It is INAUDIBLE by construction: the samples
 * are constant 0x80 (silence in classic 8-bit unsigned PCM) and the channel
 * amplitude is set to 0 with ampCmd before anything is queued.
 *
 * The Sound Manager is gone from 64-bit macOS, so these are undefined
 * dynamic_lookup imports resolved at translate time by static-interpose to
 * libabiconv's ___Snd* trampolines (maptable_tramp.asm) -> shim_Snd*.
 */

extern int  usleep(unsigned int usec);
extern long write(int fd, const void *buf, unsigned long n);
extern void exit(int status);

/* SndCommand is pack(2): cmd @0 (u16), param1 @2 (s16), param2 @4 (long) = 8B.
 * Natural i386 alignment already lays it out exactly like that. */
typedef struct { unsigned short cmd; short param1; unsigned int param2; } SndCommand;

extern short SndNewChannel(void **chan, short synth, int init, void *cb);
extern short SndDoImmediate(void *chan, const SndCommand *cmd);
extern short SndChannelStatus(void *chan, short theLength, void *theStatus);
extern short SndDisposeChannel(void *chan, unsigned char quietNow);

/* Sound.h opcodes */
#define quietCmd    3
#define flushCmd    4
#define ampCmd     43
#define bufferCmd  81
#define sampledSynth 5

/* SCStatus is 28 bytes; scChannelBusy is the Boolean at @24. */
#define SCSTATUS_SIZE   28
#define SCSTATUS_BUSY   24

#define RATE   22050u
#define NSAMP  22050u          /* one second per buffer */
#define NBUF   6               /* six seconds queued: callbacks are still pending */

static unsigned char snd[22 + NSAMP];

static void say(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    write(1, s, n);
}

static void put32le(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v);       p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

/* A classic stdSH SoundHeader: samplePtr 0 (samples inline at +22), length in
 * bytes, UnsignedFixed 16.16 rate, encode 0x00, baseFrequency kMiddleC. */
static void build_header(void) {
    unsigned int i;
    for (i = 0; i < 22; i++) snd[i] = 0;
    put32le(snd + 0, 0);                  /* samplePtr = inline */
    put32le(snd + 4, NSAMP);              /* length (bytes == frames at 8-bit mono) */
    put32le(snd + 8, RATE << 16);         /* sampleRate, 16.16 */
    snd[20] = 0x00;                       /* encode = stdSH */
    snd[21] = 60;                         /* baseFrequency = kMiddleC */
    for (i = 0; i < NSAMP; i++) snd[22 + i] = 0x80;   /* silence */
}

static short immediate(void *chan, unsigned short cmd, short p1, unsigned int p2) {
    SndCommand c;
    c.cmd = cmd; c.param1 = p1; c.param2 = p2;
    return SndDoImmediate(chan, &c);
}

int main(void) {
    void *chan = 0;
    unsigned char status[SCSTATUS_SIZE];
    int i;

    build_header();

    if (SndNewChannel(&chan, sampledSynth, 0, 0) != 0 || !chan) {
        say("chan_ok=0\n");
        exit(1);
    }
    say("chan_ok=1\n");

    /* Amplitude 0 BEFORE anything is queued: the queue runs for real, the
     * speakers stay silent. */
    immediate(chan, ampCmd, 0, 0);

    for (i = 0; i < NBUF; i++)
        immediate(chan, bufferCmd, 0, (unsigned int)(unsigned long)&snd[0]);
    say("enqueued=1\n");

    /* Let the queue start and begin consuming. Six seconds are queued, so at
     * 250ms the callbacks are unmistakably still pending. */
    usleep(250000);

    for (i = 0; i < SCSTATUS_SIZE; i++) status[i] = 0;
    SndChannelStatus(chan, SCSTATUS_SIZE, status);
    say(status[SCSTATUS_BUSY] ? "busy=1\n" : "busy=0\n");

    /* ---- the deadlock sites, in the order Halo hits them ---------------- */
    immediate(chan, flushCmd, 0, 0);      /* -> AudioQueueReset */
    say("flush_ok=1\n");

    immediate(chan, quietCmd, 0, 0);      /* -> AudioQueueStop(q, true) */
    say("quiet_ok=1\n");

    SndDisposeChannel(chan, 1);           /* -> Stop(true) + Dispose(true) */
    say("dispose_ok=1\n");

    say("done=1\n");
    exit(0);
    return 0;
}
