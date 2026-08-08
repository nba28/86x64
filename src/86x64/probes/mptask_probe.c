/*
 * mptask_probe.c — does NATIVE Carbon Multiprocessing Services still spawn a
 * task on this OS, in an x86_64 (Rosetta) process?
 *
 * WHY: Halo now reaches its complete main menu and then FREEZES, spinning at
 * ~100% CPU with 2316/2490 samples in ___MPYield -> cthread_yield, and NO MP
 * task thread anywhere in the process. The main thread is waiting for a worker
 * that never runs. Before blaming our bridge we must know whether the native
 * implementation the bridge forwards to can create a task AT ALL.
 *
 * dlsym(RTLD_DEFAULT) finding the symbol only proves an EXPORT EXISTS; it says
 * nothing about whether the implementation still works. This calls it.
 *
 *   clang -arch x86_64 -o mptask_probe mptask_probe.c \
 *         -framework CoreServices && ./mptask_probe
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <stdint.h>

typedef int32_t  OSStatus;
typedef uint32_t ItemCount;
typedef uint64_t ByteCount64;
typedef void    *MPQueueID;
typedef void    *MPTaskID;
typedef uint32_t MPTaskOptions;
typedef OSStatus (*TaskProc)(void *parameter);

static volatile int g_ran = 0;

static OSStatus task_entry(void *parameter)
{
   (void)parameter;
   g_ran = 1;
   return 0;
}

int main(void)
{
   void *h = dlopen("/System/Library/Frameworks/CoreServices.framework/CoreServices",
                    RTLD_NOW);
   printf("dlopen CoreServices = %p\n", h);

   ItemCount (*pMPProcessors)(void) = (ItemCount (*)(void))dlsym(RTLD_DEFAULT, "MPProcessors");
   OSStatus (*pMPCreateTask)(TaskProc, void *, unsigned long, MPQueueID,
                             void *, void *, MPTaskOptions, MPTaskID *)
      = (OSStatus (*)(TaskProc, void *, unsigned long, MPQueueID, void *, void *,
                      MPTaskOptions, MPTaskID *))dlsym(RTLD_DEFAULT, "MPCreateTask");
   OSStatus (*pMPCreateQueue)(MPQueueID *) =
      (OSStatus (*)(MPQueueID *))dlsym(RTLD_DEFAULT, "MPCreateQueue");
   void *pMPLibIsCompat = dlsym(RTLD_DEFAULT, "MPLibraryIsCompatible");

   printf("MPProcessors=%p MPCreateTask=%p MPCreateQueue=%p MPLibraryIsCompatible=%p\n",
          (void *)pMPProcessors, (void *)pMPCreateTask,
          (void *)pMPCreateQueue, pMPLibIsCompat);

   if (pMPProcessors) { printf("MPProcessors() = %u\n", pMPProcessors()); }

   MPQueueID q = 0;
   if (pMPCreateQueue) {
      OSStatus qs = pMPCreateQueue(&q);
      printf("MPCreateQueue -> status=%d queue=%p\n", (int)qs, q);
   }

   if (!pMPCreateTask) { printf("RESULT: MPCreateTask ABSENT\n"); return 1; }

   MPTaskID t = 0;
   OSStatus s = pMPCreateTask(task_entry, (void *)0x1234, 0 /*default stack*/,
                              q, 0, 0, 0, &t);
   printf("MPCreateTask -> status=%d task=%p\n", (int)s, t);

   for (int i = 0; i < 200 && !g_ran; i++) { usleep(10000); }   /* up to 2s */
   printf("task_entry_ran=%d\n", g_ran);
   printf("RESULT: %s\n", g_ran ? "NATIVE MP TASKS WORK"
                                : "NATIVE MP TASK NEVER RAN");
   return 0;
}
