/* 98_snd_status_layout — guard for the classic SCStatus ABI in
 * src/abiconv/sndmgr_shim.c's shim_SndChannelStatus.
 *
 * THE DEFECT. CarbonSound/Sound.h defines
 *
 *     UnsignedFixed scStartTime@0, scEndTime@4, scCurrentTime@8;
 *     Boolean scChannelBusy@12, scChannelDisposed@13, scChannelPaused@14,
 *             scUnused@15;
 *     unsigned long scChannelAttributes@16;  long scCPULoad@20;   // 24 bytes
 *
 * The shim used to `memset(theStatus, 0, 28)` and write the busy/paused flags
 * at @24 / @26. That is wrong twice over:
 *
 *   (a) OVERRUN. `theLength` is the caller's declared buffer size and is a hard
 *       bound. Writing 28 bytes into a 24-byte record smashes 4 bytes of the
 *       caller's frame — SCStatus is almost always a stack local.
 *   (b) FLAGS NEVER SET. scChannelBusy@12 is inside the region we cleared and
 *       is then never written, so it reads 0 forever. Every classic
 *       "spin until this channel drains" / "find a free voice" loop is told
 *       every channel is idle, always.
 *
 * MEASURED on Halo CE (2026-08-10), from the i386 original, two independent
 * call sites in its DirectSound-on-Sound-Manager engine, both passing
 * theLength = 0x18 = 24 and both reading the flag at +0x0C:
 *   0x2d93a7 IDirectSoundBuffer_Mac::Play  — round-robins 47 Sound Manager
 *            channels looking for one that is not busy
 *   0x2d8362 IDirectSound_Mac::GetCaps     — counts free voices in a 48-slot
 *            loop, i.e. it commits the overrun 48 times per call
 *
 * WHAT EACH ARM PROVES
 *   ON  (fixed):  the canary bytes immediately after the 24-byte record are
 *                 untouched, and — when an output device exists — a channel
 *                 with buffers in flight reports scChannelBusy@12 == 1.
 *   OFF (M64_NO_SND_STATUS_FIX=1): the canary is clobbered (zeroed, and byte 0
 *                 of it may be set to 1 because that is where the old code put
 *                 "busy"), and scChannelBusy@12 stays 0.
 *
 * ★The canary half needs NO audio device: the shim's memset runs before any
 * channel lookup, so the overrun reproduces on any host, headless included.
 * The busy half needs a live queue and is reported separately.
 *
 * The Sound Manager is gone from 64-bit macOS, so these are undefined
 * dynamic_lookup imports resolved at translate time by static-interpose to
 * libabiconv's ___Snd* trampolines (maptable_tramp.asm) -> shim_Snd*.
 */

extern int  usleep(unsigned int usec);
extern long write(int fd, const void *buf, unsigned long n);
extern void exit(int status);

typedef struct { unsigned short cmd; short param1; unsigned int param2; } SndCommand;

extern short SndNewChannel(void **chan, short synth, int init, void *cb);
extern short SndDoImmediate(void *chan, const SndCommand *cmd);
extern short SndChannelStatus(void *chan, short theLength, void *theStatus);
extern short SndDisposeChannel(void *chan, unsigned char quietNow);

#define ampCmd     43
#define bufferCmd  81
#define sampledSynth 5

/* The real record and the real flag offset. */
#define SCSTATUS_SIZE  24
#define SC_BUSY        12
#define SC_PAUSED      14

#define CANARY_N   8
#define CANARY_VAL 0xA5

#define RATE   22050u
#define NSAMP  22050u          /* one second per buffer */
#define NBUF   6

static unsigned char snd[22 + NSAMP];

/* SCStatus immediately followed by a canary. Deliberately a file-scope struct
 * rather than a stack local so the layout is exact and nothing else can be
 * blamed for a change in the canary bytes. */
static struct {
    unsigned char st[SCSTATUS_SIZE];
    unsigned char canary[CANARY_N];
} probe;

static void say(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    write(1, s, n);
}

static void put32le(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v);       p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}

static void build_header(void) {
    unsigned int i;
    for (i = 0; i < 22; i++) snd[i] = 0;
    put32le(snd + 0, 0);                  /* samplePtr = inline */
    put32le(snd + 4, NSAMP);              /* length */
    put32le(snd + 8, RATE << 16);         /* sampleRate 16.16 */
    snd[20] = 0x00;                       /* encode = stdSH */
    snd[21] = 60;                         /* baseFrequency = kMiddleC */
    for (i = 0; i < NSAMP; i++) snd[22 + i] = 0x80;   /* silence */
}

static short immediate(void *chan, unsigned short cmd, short p1, unsigned int p2) {
    SndCommand c;
    c.cmd = cmd; c.param1 = p1; c.param2 = p2;
    return SndDoImmediate(chan, &c);
}

static void arm_canary(void) {
    int i;
    for (i = 0; i < SCSTATUS_SIZE; i++) probe.st[i] = 0;
    for (i = 0; i < CANARY_N; i++)      probe.canary[i] = CANARY_VAL;
}

/* Returns 1 if every canary byte still holds CANARY_VAL. */
static int canary_intact(void) {
    int i;
    for (i = 0; i < CANARY_N; i++)
        if (probe.canary[i] != CANARY_VAL) return 0;
    return 1;
}

int main(void) {
    void *chan = 0;
    int i;

    build_header();

    /* ---- HALF 1: the overrun. Device-independent: the shim clears the record
     * before it ever looks the channel up, so this reproduces even with no
     * channel at all. Do it FIRST, on a NULL-lookup channel, so a host with no
     * audio device still exercises it. ------------------------------------- */
    if (SndNewChannel(&chan, sampledSynth, 0, 0) != 0 || !chan) {
        say("chan_ok=0\n");
        exit(1);
    }
    say("chan_ok=1\n");

    arm_canary();
    SndChannelStatus(chan, SCSTATUS_SIZE, probe.st);
    say(canary_intact() ? "canary_idle=intact\n" : "canary_idle=CLOBBERED\n");

    /* ---- HALF 2: the flag offset. Needs a live queue with buffers in flight,
     * so it is reported separately and the script does not fail on a host with
     * no output device. ---------------------------------------------------- */
    immediate(chan, ampCmd, 0, 0);        /* silent: amplitude 0 before queueing */
    for (i = 0; i < NBUF; i++)
        immediate(chan, bufferCmd, 0, (unsigned int)(unsigned long)&snd[0]);
    usleep(250000);

    arm_canary();
    SndChannelStatus(chan, SCSTATUS_SIZE, probe.st);
    say(canary_intact() ? "canary_busy=intact\n" : "canary_busy=CLOBBERED\n");
    say(probe.st[SC_BUSY]    ? "busy12=1\n"   : "busy12=0\n");
    say(probe.st[SC_PAUSED]  ? "paused14=1\n" : "paused14=0\n");
    /* Where the OLD code wrote "busy": canary[0] is the byte at record offset
     * 24. Printing it names the defect instead of merely detecting it. */
    say(probe.canary[0] == 1 ? "byte24=1\n"
        : (probe.canary[0] == CANARY_VAL ? "byte24=untouched\n" : "byte24=zeroed\n"));

    SndDisposeChannel(chan, 1);
    say("done=1\n");
    exit(0);
    return 0;
}
