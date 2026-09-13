/*
 * 99_pthread_create_arg — does pthread_create's `arg` reach the start routine?
 *
 * WHY THIS EXISTS (Portal 2 2026-09-13, s20m). 35_pthread_cond_handshake already
 * covers the cross-thread mutex+cond rendezvous, but its worker does `(void)arg`
 * -- so the ARGUMENT was never verified by any test. That argument is what every
 * C++ thread wrapper rides on: Valve's CThread passes `this` through it, and
 * Source's CWorkerThread call/reply protocol then uses `this->m_EventSend`. If the
 * arg were dropped or truncated, each worker would operate on a DIFFERENT object
 * than the one main signals, which presents as "main waits forever on an event
 * nobody ever sets" -- with every thread alive and no crash anywhere.
 *
 * pthread_create is an abigen bridge (___pthread_create) whose start_routine is a
 * REVERSE call: native pthread invokes TRANSLATED i386 code, so the arg has to
 * move from the x86_64 register ABI (rdi) onto the i386 cdecl stack, and the
 * return address has to narrow from 8 bytes to 4. Two conversions, either of
 * which would silently corrupt a `this`.
 *
 * The worker checks the POINTER ITSELF (not just a global): it dereferences the
 * struct main handed it and verifies both a magic and a self-pointer, so a
 * truncated-but-still-readable address fails rather than passing by luck.
 *
 * 42 = arg arrived intact.  9 = arg wrong/garbage.  7 = worker never ran
 * (3s bounded wait, so this can never hang the suite).
 */
#include <pthread.h>
#include <sys/time.h>

extern void exit(int status);

struct payload {
   unsigned long    magic;
   struct payload  *self;      /* catches a truncated pointer that still reads */
   int              seen_ok;
};

#define MAGIC 0x0C0FFEE5UL

static pthread_mutex_t mu;
static pthread_cond_t  cv;
static volatile int    done = 0;

static void *worker(void *arg) {
   struct payload *p = (struct payload *)arg;
   int ok = 0;
   /* A NULL or wildly wrong arg must not fault the test into an ambiguous
    * signal -- report it as a value instead. */
   if (p != 0 && p->magic == MAGIC && p->self == p) { ok = 1; }
   pthread_mutex_lock(&mu);
   if (p != 0) { p->seen_ok = ok; }
   done = ok ? 1 : 2;
   pthread_cond_signal(&cv);
   pthread_mutex_unlock(&mu);
   return 0;
}

int main(void) {
   static struct payload pl;
   pl.magic = MAGIC;
   pl.self  = &pl;
   pl.seen_ok = -1;

   pthread_mutex_init(&mu, 0);
   pthread_cond_init(&cv, 0);

   pthread_t th;
   if (pthread_create(&th, 0, worker, &pl) != 0) { exit(7); }

   struct timeval now;
   gettimeofday(&now, 0);
   struct timespec deadline;
   deadline.tv_sec  = now.tv_sec + 3;
   deadline.tv_nsec = now.tv_usec * 1000;

   pthread_mutex_lock(&mu);
   while (done == 0) {
      if (pthread_cond_timedwait(&cv, &mu, &deadline) != 0) { break; }
   }
   int d = done;
   int seen = pl.seen_ok;
   pthread_mutex_unlock(&mu);

   if (d == 0)               { exit(7); }   /* worker never reported */
   if (d != 1 || seen != 1)  { exit(9); }   /* arg did not survive the bridge */
   exit(42);
}
