/*
 * carbon_timemgr_shim.c — ONE job: the classic Time Manager for i386 callers.
 *
 *   InsTime / InsXTime / InstallTimeTask / InstallXTimeTask   install a TMTask
 *   PrimeTime / PrimeTimeTask                                 arm it
 *   RmvTime / RemoveTimeTask                                  remove it
 *
 * WHY NOT abigen's bridge: a TMTask is a QUEUE ELEMENT the Time Manager keeps
 * after the call returns, and its tmAddr is a procedure the Time Manager calls
 * later. The generated bridge deep-copies the i386 record into a scratch slot of
 * its own frame (gone on return) and hands native CarbonCore an i386 function
 * pointer to call natively. Portal 2's Bink (RAD's background callback,
 * RADCB_resume_handler -> PrimeTime) came back with %esi clobbered and faulted.
 *
 * WHAT THIS DOES: the i386 TMTask stays the one record (read/written in place,
 * packed-2 layout: qLink@0 qType@4 tmAddr@6 tmCount@10 tmWakeUp@14 tmReserved@18).
 * PrimeTime arms a one-shot dispatch timer; on expiry the task's kTMTaskActive
 * bit is cleared and tmAddr(tmTaskPtr) runs through the i386 callback bridge.
 * Tasks run on ONE serial queue: classic Time Manager tasks never ran
 * concurrently with each other. A task may re-prime itself from its procedure.
 * RmvTime reports the unexpired time in tmCount, in the units it was primed with.
 *
 * ABI: MTSHIM trampolines (maptable_tramp.asm, rdi -> &i386 args[0]); the names
 * are in custom.syms so abigen emits no competing bridge.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <os/lock.h>
#include <dispatch/dispatch.h>
#include "cb_bridge.h"

#define TM_ACTIVE 0x8000     /* kTMTaskActive: high bit of qType */

static inline uint16_t *tm_qtype(uint32_t t)  { return (uint16_t *)(uintptr_t)(t + 4); }
static inline uint32_t  tm_addr(uint32_t t)   { uint32_t v; memcpy(&v, (void *)(uintptr_t)(t + 6), 4); return v; }
static inline void      tm_set_count(uint32_t t, int32_t v) { memcpy((void *)(uintptr_t)(t + 10), &v, 4); }

/* ponytail: linear table under one lock; a process has a handful of tasks. */
#define TM_MAX 64
static struct tm_ent {
   uint32_t task;
   dispatch_source_t timer;
   uint64_t deadline;         /* dispatch_time of expiry while armed */
   int      units_us;         /* primed with a negative (microsecond) count */
   uint32_t gen;              /* bumps on every prime/remove: stale fires no-op */
} g_tm[TM_MAX];
static os_unfair_lock g_tm_lk = OS_UNFAIR_LOCK_INIT;

static dispatch_queue_t tm_queue(void)
{
   static dispatch_queue_t q;
   static dispatch_once_t once;
   dispatch_once(&once, ^{ q = dispatch_queue_create("86x64.timemanager", DISPATCH_QUEUE_SERIAL); });
   return q;
}

/* Caller holds g_tm_lk. */
static struct tm_ent *tm_find(uint32_t task, int create)
{
   struct tm_ent *free_e = NULL;
   for (int i = 0; i < TM_MAX; i++) {
      if (g_tm[i].task == task) { return &g_tm[i]; }
      if (!g_tm[i].task && !free_e) { free_e = &g_tm[i]; }
   }
   if (!create) { return NULL; }
   if (!free_e) { fprintf(stderr, "timemgr: more than %d tasks installed\n", TM_MAX); return NULL; }
   memset(free_e, 0, sizeof *free_e);
   free_e->task = task;
   return free_e;
}

/* Caller holds g_tm_lk. */
static void tm_disarm(struct tm_ent *e)
{
   if (e->timer) { dispatch_source_cancel(e->timer); dispatch_release(e->timer); e->timer = NULL; }
   e->gen++;
}

static void tm_fire(uint32_t task, uint32_t gen)
{
   os_unfair_lock_lock(&g_tm_lk);
   struct tm_ent *e = tm_find(task, 0);
   if (!e || e->gen != gen) { os_unfair_lock_unlock(&g_tm_lk); return; }
   tm_disarm(e);
   *tm_qtype(task) &= (uint16_t)~TM_ACTIVE;
   tm_set_count(task, 0);
   const uint32_t proc = tm_addr(task);
   os_unfair_lock_unlock(&g_tm_lk);   /* the procedure may re-prime itself */

   if (proc) {
      static const x64_cb_sig sig = { .nargs = 1, .ret_kind = CBR_VOID,
                                      .arg_kinds = { CBA_PTR } };
      void (*fn)(uint64_t) = (void (*)(uint64_t))(uintptr_t)x64_cb_wrap(proc, &sig);
      if (fn) { fn(task); }
   }
}

static int32_t tm_install(uint32_t task)
{
   if (!task) { return -50; }                          /* paramErr */
   os_unfair_lock_lock(&g_tm_lk);
   struct tm_ent *e = tm_find(task, 1);
   if (e) { tm_disarm(e); }
   *tm_qtype(task) &= (uint16_t)~TM_ACTIVE;
   os_unfair_lock_unlock(&g_tm_lk);
   return e ? 0 : -108;                                /* memFullErr */
}

static int32_t tm_prime(uint32_t task, int32_t count)
{
   if (!task) { return -50; }
   os_unfair_lock_lock(&g_tm_lk);
   struct tm_ent *e = tm_find(task, 1);                /* priming an uninstalled task: install it */
   if (!e) { os_unfair_lock_unlock(&g_tm_lk); return -108; }
   tm_disarm(e);
   e->units_us = count < 0;
   const int64_t ns = count < 0 ? -(int64_t)count * 1000 : (int64_t)count * 1000000;
   e->deadline = dispatch_time(DISPATCH_TIME_NOW, ns);
   *tm_qtype(task) |= TM_ACTIVE;
   const uint32_t gen = e->gen;
   e->timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, DISPATCH_TIMER_STRICT, tm_queue());
   dispatch_source_set_timer(e->timer, e->deadline, DISPATCH_TIME_FOREVER, 0);
   dispatch_source_set_event_handler(e->timer, ^{ tm_fire(task, gen); });
   dispatch_resume(e->timer);
   os_unfair_lock_unlock(&g_tm_lk);
   return 0;
}

static int32_t tm_remove(uint32_t task)
{
   if (!task) { return -50; }
   os_unfair_lock_lock(&g_tm_lk);
   struct tm_ent *e = tm_find(task, 0);
   int32_t left = 0;
   if (e && (*tm_qtype(task) & TM_ACTIVE)) {
      const uint64_t now = dispatch_time(DISPATCH_TIME_NOW, 0);
      const int64_t ns = e->deadline > now ? (int64_t)(e->deadline - now) : 0;
      left = e->units_us ? -(int32_t)(ns / 1000) : (int32_t)(ns / 1000000);
   }
   if (e) { tm_disarm(e); e->task = 0; }
   *tm_qtype(task) &= (uint16_t)~TM_ACTIVE;
   tm_set_count(task, left);
   os_unfair_lock_unlock(&g_tm_lk);
   return 0;
}

/* void InsTime(QElemPtr) / void InsXTime(QElemPtr) */
uint32_t shim_InsTime(uint32_t *a)          { tm_install(a[0]); return 0; }
/* OSErr InstallTimeTask(QElemPtr) / InstallXTimeTask(QElemPtr) */
uint32_t shim_InstallTimeTask(uint32_t *a)  { return (uint32_t)(int32_t)(int16_t)tm_install(a[0]); }
/* void PrimeTime(QElemPtr, long) */
uint32_t shim_PrimeTime(uint32_t *a)        { tm_prime(a[0], (int32_t)a[1]); return 0; }
/* OSErr PrimeTimeTask(QElemPtr, long) */
uint32_t shim_PrimeTimeTask(uint32_t *a)    { return (uint32_t)(int32_t)(int16_t)tm_prime(a[0], (int32_t)a[1]); }
/* void RmvTime(QElemPtr) */
uint32_t shim_RmvTime(uint32_t *a)          { tm_remove(a[0]); return 0; }
/* OSErr RemoveTimeTask(QElemPtr) */
uint32_t shim_RemoveTimeTask(uint32_t *a)   { return (uint32_t)(int32_t)(int16_t)tm_remove(a[0]); }
