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

   printf("done=1\n"); FLUSH();
   exit(0);
}
