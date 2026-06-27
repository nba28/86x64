/*
 * 35_pthread_cond_handshake — cross-thread pthread mutex+cond rendezvous.
 *
 * Reproduces the Portal 2 thread-pool startup DEADLOCK (s19). POSIX sync
 * objects (pthread_mutex_t / pthread_cond_t) require a STABLE, SHARED address
 * across every thread that touches them. abigen marshalled them BY VALUE: each
 * call copied the i386 struct into a fresh native stack buffer, ran the native
 * pthread call on the COPY, then copied the state back. Single-threaded
 * sequential lock/unlock survives via that round-trip, but any genuine
 * cross-thread contention breaks:
 *   - pthread_cond_timedwait atomically unlocks a COPY, so the real shared
 *     mutex stays locked -> a second thread deadlocks acquiring it;
 *   - a signal on one thread's cond copy never reaches the waiter on another
 *     thread's cond copy -> lost wakeup.
 *
 * This is exactly the handshake below: main locks `mu`, starts a worker, then
 * cond_timedwait()s (which must release `mu` so the worker can take it). The
 * worker takes `mu`, sets `ready`, signals `cv`, releases `mu`; main wakes,
 * re-takes `mu`, sees `ready`, and exits 42. Under the by-value bug the worker
 * deadlocks on `mu` and main's wait TIMES OUT -> exit 7. A 3s absolute timeout
 * keeps the test bounded (it can never hang the suite): 42 = fixed, 7 = bug.
 *
 * Also exercises the recursive-mutex attribute path (pthread_mutexattr_init/
 * settype/init) that Valve's CThreadMutex relies on.
 */
#include <pthread.h>
#include <sys/time.h>

extern void exit(int status);

static pthread_mutex_t mu;
static pthread_cond_t  cv;
static volatile int    ready = 0;

static void *worker(void *arg) {
   (void)arg;
   pthread_mutex_lock(&mu);     /* blocks until main's timedwait releases mu */
   ready = 1;
   pthread_cond_signal(&cv);
   pthread_mutex_unlock(&mu);
   return 0;
}

int main(void) {
   pthread_mutexattr_t at;
   pthread_mutexattr_init(&at);
   pthread_mutexattr_settype(&at, PTHREAD_MUTEX_RECURSIVE);
   if (pthread_mutex_init(&mu, &at) != 0) exit(2);
   pthread_mutexattr_destroy(&at);
   if (pthread_cond_init(&cv, 0) != 0) exit(3);

   /* Recursive-attr check (bounded; trylock never blocks): a recursive mutex
    * lets the owning thread re-lock it. If the attr was lost, trylock on the
    * self-held mutex returns EBUSY/EDEADLK -> exit 5. */
   pthread_mutex_lock(&mu);
   if (pthread_mutex_trylock(&mu) != 0) exit(5);
   pthread_mutex_unlock(&mu);
   pthread_mutex_unlock(&mu);

   pthread_mutex_lock(&mu);

   pthread_t th;
   if (pthread_create(&th, 0, worker, 0) != 0) exit(4);

   struct timeval now;
   gettimeofday(&now, 0);
   struct timespec ts;
   ts.tv_sec  = now.tv_sec + 3;          /* absolute deadline, +3s */
   ts.tv_nsec = (long)now.tv_usec * 1000;

   while (!ready) {
      int r = pthread_cond_timedwait(&cv, &mu, &ts);
      if (r != 0) {                      /* ETIMEDOUT => lost wakeup = the bug */
         exit(7);
      }
   }
   pthread_mutex_unlock(&mu);
   pthread_join(th, 0);
   exit(42);
}
