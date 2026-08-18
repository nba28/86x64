/* 99_snd_cb_reentrancy — guard for the classic Sound Manager's DELIVERY
 * CONTRACT: a QUEUED callback must never run on the caller's stack.
 *
 * THE CONTRACT. SndDoCommand *queues* a command; the Sound Manager executes it
 * later, on its own context (classically at interrupt time). SndDoCommand
 * returning therefore never means "the callback has already run", and no
 * classic app can observe its own callBackCmd re-entering it from inside its
 * own SndDoCommand call. Delivering the callback inline invents a re-entrancy
 * the real API cannot produce.
 *
 * WHY THAT BREAKS REAL APPS. An app may hold a lock across SndDoCommand, and a
 * double-buffering app naturally does: take the voice lock, refill the ring,
 * queue the next bufferCmd, re-arm the callBackCmd, release. Deliver the
 * callback inline and it re-enters that critical section ON THE SAME THREAD,
 * on a lock that does not recurse.
 *
 * MEASURED on Halo CE (2026-08-19). Its SndCallBackProc (i386 0x2d80fa) is
 * exactly that shape:
 *     MPWaitOnSemaphore(voiceSem, 500ms) -> refill -> SndDoCommand(bufferCmd)
 *     -> SndDoCommand(callBackCmd) -> MPSignalSemaphore(voiceSem)
 * 0x2d8127 is the ONLY 500 ms wait in the whole binary. With the inline fire it
 * re-entered on the caller's thread while voiceSem was still held; MP
 * semaphores do not recurse, so the nested wait burned its full deadline,
 * returned kMPTimeoutErr, and took the early-out that SKIPS THE REFILL.
 *
 * THE FIXTURE reproduces the shape without needing MP: the callback re-arms
 * itself with a callBackCmd, exactly as Halo does, and simply reports whether
 * it was ever entered while already inside itself. That is the contract stated
 * directly, so the guard cannot pass for an unrelated reason.
 *
 * ON  (default)             : depth never exceeds 1  -> reentered = 0
 * OFF (M64_NO_SND_CB_DEFER=1): the nested SndDoCommand fires inline -> depth 2
 *
 * NON-VACUITY. "reentered = 0" is also what a callback that never ran at all
 * would report, so the ON arm additionally REQUIRES at least two deliveries.
 * A guard that passes because nothing happened is worse than no guard.
 *
 * The Sound Manager is gone from 64-bit macOS, so these are undefined
 * dynamic_lookup imports resolved at translate time by static-interpose to
 * libabiconv's ___Snd* trampolines (maptable_tramp.asm) -> shim_Snd*.
 */

extern int  usleep(unsigned int usec);
extern long write(int fd, const void *buf, unsigned long n);
extern void exit(int status);

/* SndCommand is pack(2): cmd @0 (u16), param1 @2 (s16), param2 @4 (long) = 8B. */
typedef struct { unsigned short cmd; short param1; unsigned int param2; } SndCommand;

extern short SndNewChannel(void **chan, short synth, int init, void *cb);
extern short SndDoCommand(void *chan, const SndCommand *cmd, unsigned char noWait);
extern short SndDisposeChannel(void *chan, unsigned char quietNow);

#define callBackCmd    13
#define sampledSynth    5

static void put(const char *s) {
   unsigned long n = 0; while (s[n]) n++;
   write(1, s, n);
}
static void put_int(const char *label, int v) {
   char b[32]; int i = 0, neg = v < 0; unsigned int u = neg ? (unsigned)(-v) : (unsigned)v;
   if (!u) { b[i++] = '0'; }
   while (u) { b[i++] = (char)('0' + (u % 10)); u /= 10; }
   if (neg) { b[i++] = '-'; }
   put(label);
   while (i) { char c = b[--i]; write(1, &c, 1); }
   put("\n");
}

static void         *g_chan;
static volatile int  g_depth, g_calls, g_reentered, g_maxdepth;

/* The app's SndCallBackProc. Signature is (SndChannelPtr, SndCommand *) — the
 * shim stages the command into the channel's cmdInProgress slot and passes a
 * pointer to it, classic-faithfully. */
static void my_cb(void *chan, SndCommand *cmd) {
   (void)cmd;
   g_depth++;
   g_calls++;
   if (g_depth > g_maxdepth) { g_maxdepth = g_depth; }
   if (g_depth > 1) { g_reentered = 1; }

   /* Re-arm from INSIDE the callback, exactly as a double-buffering app does.
    * Only from the outermost frame: the OFF arm must demonstrate the defect and
    * then terminate, not recurse until the stack runs out. */
   if (g_depth == 1 && g_calls < 4) {
      SndCommand c;
      c.cmd = callBackCmd; c.param1 = 0; c.param2 = 0;
      SndDoCommand(chan, &c, 0);
   }
   g_depth--;
}

int main(void) {
   if (SndNewChannel(&g_chan, sampledSynth, 0, (void *)my_cb) != 0 || !g_chan) {
      /* No output device (headless CI): the contract cannot be exercised, and
       * claiming a pass here would be exactly the silent-inert guard failure. */
      put("SKIP no channel\n");
      exit(2);
   }

   /* An IDLE channel: inflight == 0 is precisely the state in which the shim
    * used to take its "fire immediately" shortcut. */
   SndCommand c;
   c.cmd = callBackCmd; c.param1 = 0; c.param2 = 0;
   SndDoCommand(g_chan, &c, 0);

   /* Deferred delivery is asynchronous by design, so give it a bounded window
    * rather than assuming it has already happened. */
   for (int i = 0; i < 200 && g_calls < 2; i++) { usleep(5000); }

   put_int("calls=",     g_calls);
   put_int("maxdepth=",  g_maxdepth);
   put_int("reentered=", g_reentered);

   SndDisposeChannel(g_chan, 1);

   if (g_reentered) { put("FAIL callback re-entered on the caller's stack\n"); exit(1); }
   if (g_calls < 2) { put("FAIL callback was not delivered\n");                exit(1); }
   put("PASS\n");
   exit(0);
}
