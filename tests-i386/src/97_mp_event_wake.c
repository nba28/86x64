/* 97_mp_event_wake — does an MPEvent round-trip through our bridges REPORT
 * "not signalled" when it is not signalled?
 *
 * WHY THIS EXACT SEQUENCE. Halo Mac is a Win32->Mac shim port: its
 * WaitForSingleObject is a virtual on a HANDLE class whose event override is
 * (i386 original, __TEXT,__textcoal_nt @0x3381be):
 *
 *     err = MPWaitForEvent(this->mpEvent, NULL, ms == INFINITE ? 0x7FFFFFFF : ms);
 *     if (err == 0)            { if (manualReset) MPSetEvent(evt, 1); return 0; }
 *     if (err == kMPTimeoutErr) return WAIT_TIMEOUT;   // 0x102
 *     return 0;                                        // ★ANY OTHER ERROR = "SIGNALLED"
 *
 * and its callers (@0xc2748 / @0xc2782) dispatch work like this:
 *
 *     if (WaitForSingleObject(doneEvent, 0) != 0)    // i.e. NOT signalled
 *         SetEvent(workEvent);                       // ...only then wake the worker
 *
 * So a poll that WRONGLY reports "signalled" — whether because the wait really
 * succeeded or because it failed with anything that is not kMPTimeoutErr and
 * got flattened to 0 — makes the wake never fire. The worker then parks in
 * MPWaitForEvent forever, no hang and no crash, just work that is silently
 * never done.
 *
 * ★THE ASSERTION THAT MATTERS is therefore NOT "does MPSetEvent wake a waiter"
 * (that is the easy direction and it is already known good). It is the
 * NEGATIVE direction: an unsignalled event polled with kDurationImmediate must
 * come back as kMPTimeoutErr (-29296) and NOT as noErr, and NOT as some third
 * error, because the app cannot tell a third error apart from success.
 *
 * MEASURED, native x86_64, for reference: MPCreateEvent -> 0 with an id of
 * 0x100001 (small — no >4GB handle, so no proxy wrapping is involved);
 * MPWaitForEvent(e, NULL, kDurationImmediate) -> -29296 before the set and 0
 * after it. A NULL MPEventFlags* out-param is tolerated. This fixture checks
 * that the same holds after the i386->x86_64 bridge.
 *
 * MP Services is not in the i386 sysroot: these are undefined dynamic_lookup
 * imports that static-interpose binds at translate time to libabiconv's
 * ___MP<name> trampolines, exactly like 99_mp_allocate_buffer.
 */

extern long write(int fd, const void *buf, unsigned long n);
extern void exit(int status);
extern int  snprintf(char *s, unsigned long n, const char *f, ...);

typedef int   OSStatus;
typedef void *MPEventID;
typedef unsigned int MPEventFlags;
typedef int   Duration;

extern OSStatus MPCreateEvent(MPEventID *event);
extern OSStatus MPSetEvent(MPEventID event, MPEventFlags flags);
extern OSStatus MPWaitForEvent(MPEventID event, MPEventFlags *flags, Duration timeout);
extern OSStatus MPDeleteEvent(MPEventID event);

#define kDurationImmediate 0
#define kMPTimeoutErr      (-29296)

static void say(const char *s) {
    unsigned long n = 0;
    while (s[n]) n++;
    write(1, s, n);
}

static void sayf(const char *k, int v) {
    char b[96];
    snprintf(b, sizeof b, "%s=%d\n", k, v);
    say(b);
}

int main(void) {
    MPEventID e = 0;
    OSStatus  s;

    s = MPCreateEvent(&e);
    sayf("create_rc", (int)s);
    sayf("id_nonzero", e ? 1 : 0);
    if (s != 0 || !e) { say("done=1\n"); exit(1); }

    /* ---- THE assertion: an unsignalled event must NOT report signalled. ---- */
    s = MPWaitForEvent(e, 0, kDurationImmediate);
    sayf("poll_unsignalled_rc", (int)s);
    say(s == kMPTimeoutErr ? "poll_unsignalled=TIMEOUT_OK\n"
        : (s == 0 ? "poll_unsignalled=WRONGLY_SIGNALLED\n"
                  : "poll_unsignalled=OTHER_ERROR_READS_AS_SIGNALLED\n"));

    /* ---- and the positive direction, so the arms are comparable ---- */
    s = MPSetEvent(e, 1);
    sayf("set_rc", (int)s);

    s = MPWaitForEvent(e, 0, kDurationImmediate);
    sayf("poll_after_set_rc", (int)s);
    say(s == 0 ? "poll_after_set=SIGNALLED_OK\n" : "poll_after_set=NOT_SIGNALLED\n");

    /* ---- auto-reset: the wait consumed it, so the next poll must time out --- */
    s = MPWaitForEvent(e, 0, kDurationImmediate);
    sayf("poll_reconsumed_rc", (int)s);
    say(s == kMPTimeoutErr ? "poll_reconsumed=TIMEOUT_OK\n" : "poll_reconsumed=STILL_SIGNALLED\n");

    /* ---- non-NULL flags out-param, the other calling convention ---- */
    MPEventFlags f = 0;
    MPSetEvent(e, 0x2a);
    s = MPWaitForEvent(e, &f, kDurationImmediate);
    sayf("poll_flags_rc", (int)s);
    sayf("poll_flags_val", (int)f);

    MPDeleteEvent(e);
    say("done=1\n");
    exit(0);
    return 0;
}
