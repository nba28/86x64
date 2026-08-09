/*
 * 99_mp_task_spawn.c — does a Carbon Multiprocessing Services TASK actually run
 * when an i386 program creates one THROUGH OUR BRIDGES?
 *
 * WHY (Halo #42, 2026-08-09): with the #39 allocator fix in, Halo reaches its
 * complete main menu and then FREEZES — ~100% CPU, 2316/2490 main-thread samples
 * in `___MPYield.l1 -> cthread_yield`, and NO MP task thread anywhere in the
 * process. It is spin-waiting for a worker that never runs.
 *
 * Already RULED OUT by measurement (see the journal):
 *   - the i386 task entry IS reverse-wrapped (`___MPCreateTask.l1` calls
 *     `_x64_cb_wrap` with `_x64_cbsig_1`);
 *   - NATIVE MP Services works fine in an x86_64 Rosetta process
 *     (src/86x64/probes/mptask_probe.c: MPCreateTask status=0, task ran);
 *   - the opaque-ID out-params are proxy-wrapped, not truncated;
 *   - every ___MP* import binds to libabiconv and none is a null-jump bridge.
 *
 * So the remaining question is what happens on the path Halo actually takes:
 * i386 caller -> our bridge -> native MP. This fixture walks the whole classic
 * sequence and reports the status of EVERY step, so an UPSTREAM failure (a queue
 * / semaphore / event create returning an error, which a classic app checks
 * before it ever calls MPCreateTask) is distinguishable from a tasking defect.
 *
 * ★It is deliberately a PROBE first and a guard second: it prints what each call
 * returned rather than asserting a single expected value, because at the time of
 * writing the cause is not yet known and a fixture that asserts the wrong thing
 * would be worse than none.
 *
 * No libc headers: the i386 sysroot's <stdio.h> includes <_types.h>, which is
 * absent. exit() rather than `return` — the 86x64.sh wrapper enters _main via
 * jmp, so there is no return frame.
 */

extern int  printf(const char *, ...);
extern int  fflush(void *);
extern void exit(int status);
extern unsigned int sleep(unsigned int);
#define FLUSH() fflush(0)

typedef int            OSStatus;
typedef unsigned int   ItemCount;
typedef void          *MPQueueID;
typedef void          *MPTaskID;
typedef void          *MPSemaphoreID;
typedef void          *MPEventID;
typedef unsigned int   MPTaskOptions;
typedef unsigned long  ByteCount;
typedef OSStatus (*TaskProc)(void *parameter);

/* Reached through libabiconv's ___MP* bridges; static-interpose binds them at
 * translate time. Declared here because the i386 sysroot has no CarbonCore. */
extern ItemCount MPProcessors(void);
extern OSStatus  MPCreateQueue(MPQueueID *queue);
extern OSStatus  MPCreateSemaphore(unsigned int maximumValue,
                                   unsigned int initialValue,
                                   MPSemaphoreID *semaphore);
extern OSStatus  MPCreateEvent(MPEventID *event);
extern OSStatus  MPCreateTask(TaskProc entryPoint, void *parameter,
                              ByteCount stackSize, MPQueueID notifyQueue,
                              void *termParam1, void *termParam2,
                              MPTaskOptions options, MPTaskID *task);
extern OSStatus  MPTerminateTask(MPTaskID task, OSStatus terminationStatus);
extern void      MPYield(void);

typedef unsigned int MPEventFlags;
typedef int          Duration;               /* + = ms, - = us, 0x7FFFFFFF = forever */
#define kDurationImmediate 0
extern OSStatus  MPSetEvent(MPEventID event, MPEventFlags flags);
extern OSStatus  MPWaitForEvent(MPEventID event, MPEventFlags *flags,
                                Duration timeout);

/* Written by the spawned task, read by the main thread. `volatile` only — the
 * point is to observe whether the worker ever executes at all. */
static volatile int g_task_ran;
static volatile int g_task_param_ok;

static OSStatus task_entry(void *parameter)
{
   g_task_param_ok = (parameter == (void *)0x1234);
   g_task_ran = 1;
   return 0;
}

/* Arm B's worker: signal the event the main thread is parked on. */
static MPEventID       g_event;
static volatile int    g_setter_ran;

static OSStatus setter_entry(void *parameter)
{
   (void)parameter;
   g_setter_ran = 1;
   MPSetEvent(g_event, 0xA);
   return 0;
}

int main(void)
{
   printf("processors=%u\n", MPProcessors()); FLUSH();

   MPQueueID q = 0;
   OSStatus qs = MPCreateQueue(&q);
   printf("createqueue_status=%d\n", (int)qs); FLUSH();
   printf("createqueue_nonnull=%d\n", q != 0); FLUSH();

   MPSemaphoreID sem = 0;
   OSStatus ss = MPCreateSemaphore(1, 0, &sem);
   printf("createsem_status=%d\n", (int)ss); FLUSH();
   printf("createsem_nonnull=%d\n", sem != 0); FLUSH();

   MPEventID ev = 0;
   OSStatus es = MPCreateEvent(&ev);
   printf("createevent_status=%d\n", (int)es); FLUSH();
   printf("createevent_nonnull=%d\n", ev != 0); FLUSH();

   MPTaskID task = 0;
   OSStatus ts = MPCreateTask(task_entry, (void *)0x1234, 0, q, 0, 0, 0, &task);
   printf("createtask_status=%d\n", (int)ts); FLUSH();
   printf("createtask_nonnull=%d\n", task != 0); FLUSH();

   /* Halo's own idiom: yield in a loop waiting for the worker. Bounded so a
    * failure is a REPORT, not a hang -- an unbounded spin here would reproduce
    * the bug by wedging the suite, which is not useful. */
   for (int i = 0; i < 300 && !g_task_ran; i++) {
      MPYield();
      if ((i % 100) == 99) { sleep(1); }
   }
   printf("task_ran=%d\n", g_task_ran); FLUSH();
   printf("task_param_ok=%d\n", g_task_param_ok); FLUSH();

   if (task) { MPTerminateTask(task, 0); }

   /* ---- stackSize variants -------------------------------------------------
    * The call above passes stackSize 0 (= default), which is NOT what a real
    * app does. `ByteCount` is `unsigned long`: 4 bytes on i386, 8 on x86_64, so
    * it is exactly the width that a bridge can widen wrongly — and a garbage
    * huge stackSize is a very plausible way for MPCreateTask to fail in a real
    * app while succeeding in a minimal probe. Test realistic values. */
   static const unsigned long sizes[] = { 65536UL, 524288UL, 1048576UL };
   for (unsigned s = 0; s < 3; s++) {
      g_task_ran = 0; g_task_param_ok = 0;
      MPTaskID t2 = 0;
      OSStatus s2 = MPCreateTask(task_entry, (void *)0x1234, sizes[s],
                                 q, 0, 0, 0, &t2);
      int ran = 0;
      for (int i = 0; i < 300 && !g_task_ran; i++) {
         MPYield();
         if ((i % 100) == 99) { sleep(1); }
      }
      ran = g_task_ran;
      printf("stack_%lu_status=%d\n", sizes[s], (int)s2); FLUSH();
      printf("stack_%lu_ran=%d\n", sizes[s], ran); FLUSH();
      if (t2) { MPTerminateTask(t2, 0); }
   }

   /* ---- MPEvent round trip, BOTH orderings (Halo #42) ----------------------
    * Classic MPEvent semantics are LATCHED: MPSetEvent ORs flag bits into the
    * event and MPWaitForEvent returns IMMEDIATELY, consuming them, if bits are
    * already set. That latch is what makes set-before-wait safe and is exactly
    * what a lost-wakeup bug would destroy. Halo's two MP workers sit forever in
    * MPWaitForEvent while the main thread paces in MPDelayUntil, and the freeze
    * is INTERMITTENT — the signature of a set/wait race, so test the orderings
    * separately. Every wait is BOUNDED so a failure reports instead of hanging
    * the suite. */
   MPEventID ev2 = 0;
   OSStatus e2 = MPCreateEvent(&ev2);
   printf("ev2_status=%d\n", (int)e2); FLUSH();

   /* Arm A -- SET BEFORE WAIT. The latched bits must satisfy an IMMEDIATE wait.
    * If this returns a timeout error, the latch is lost and that IS Halo's bug. */
   MPEventFlags got = 0;
   OSStatus sa = MPSetEvent(ev2, 0x5);
   OSStatus wa = MPWaitForEvent(ev2, &got, kDurationImmediate);
   printf("setfirst_set_status=%d\n", (int)sa); FLUSH();
   printf("setfirst_wait_status=%d\n", (int)wa); FLUSH();
   printf("setfirst_flags=0x%x\n", (unsigned)got); FLUSH();
   printf("setfirst_latched=%d\n", (wa == 0 && got == 0x5)); FLUSH();

   /* Arm A2 -- the latch must also be CONSUMED: a second immediate wait with no
    * intervening set must NOT report flags again. */
   MPEventFlags got2 = 0xdead;
   OSStatus wa2 = MPWaitForEvent(ev2, &got2, kDurationImmediate);
   printf("consumed_wait_status=%d\n", (int)wa2); FLUSH();
   printf("consumed=%d\n", (wa2 != 0)); FLUSH();

   /* Arm B -- WAIT BEFORE SET, across a real MP task boundary: the worker sets
    * the event, main blocks on it with a bounded timeout. */
   g_event = ev2;
   g_setter_ran = 0;
   MPTaskID t3 = 0;
   OSStatus s3 = MPCreateTask(setter_entry, 0, 0, q, 0, 0, 0, &t3);
   MPEventFlags got3 = 0;
   OSStatus wb = MPWaitForEvent(ev2, &got3, 5000);   /* 5s bound, not forever */
   printf("waitfirst_task_status=%d\n", (int)s3); FLUSH();
   printf("waitfirst_wait_status=%d\n", (int)wb); FLUSH();
   printf("waitfirst_flags=0x%x\n", (unsigned)got3); FLUSH();
   printf("waitfirst_woke=%d\n", (wb == 0 && got3 == 0xA)); FLUSH();
   if (t3) { MPTerminateTask(t3, 0); }

   printf("done=1\n"); FLUSH();
   exit(0);
}
