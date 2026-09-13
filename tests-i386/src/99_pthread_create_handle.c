/*
 * 99_pthread_create_handle — does pthread_create WRITE BACK the pthread_t?
 *
 * WHY (Portal 2 2026-09-13, s20m root cause candidate). 99_pthread_create_arg
 * checks the arg the worker receives; NOTHING checked the OUT-PARAM. That slot is
 * load-bearing in a way that hides itself: Valve's CThread::Start does
 *
 *     pthread_create( &this->m_hThread /* +0x98 * /, &attr, ThreadProc, this );
 *
 * and CWorkerThread::Call then opens with
 *
 *     if ( m_hThread == 0 ) return -1;     // silent, no dispatch
 *
 * so a missing write-back does NOT break thread creation -- the thread runs, its
 * ThreadProc signals the start handshake, everything looks healthy -- but every
 * later CallWorker() silently no-ops. CThreadPool::SuspendExecution dispatches and
 * then waits for the replies in a SECOND loop, so it waits forever for a reply to
 * a call that was never sent. Measured symptom: m_EventSend signals=0 while the
 * worker polls it, main spinning at 100% CPU in WaitForMultiple.
 *
 * ___pthread_create is an abigen bridge that marshals `struct _opaque_pthread_t **`
 * by deep copy -- it converts into native scratch buffers and must copy the
 * resulting pthread_t BACK into the caller's 4-byte i386 slot. That round trip is
 * what this test pins.
 *
 * The handle is checked two ways, because non-zero alone is weak: it must also be
 * USABLE (pthread_equal against the value the thread itself reports, then join).
 *
 * 42 = handle written and usable.  9 = handle left 0 (the Portal 2 shape).
 * 8 = handle non-zero but not the running thread.  7 = worker never ran.
 * 6 = join failed.
 */
#include <pthread.h>
#include <sys/time.h>

extern void exit(int status);

static pthread_mutex_t mu;
static pthread_cond_t  cv;
static volatile int    ran = 0;
static pthread_t       self_reported;

static void *worker(void *arg) {
   (void)arg;
   pthread_mutex_lock(&mu);
   self_reported = pthread_self();
   ran = 1;
   pthread_cond_signal(&cv);
   pthread_mutex_unlock(&mu);
   return 0;
}

int main(void) {
   pthread_mutex_init(&mu, 0);
   pthread_cond_init(&cv, 0);

   pthread_t th;
   /* Pre-poison: a bridge that never writes the slot leaves exactly this, which
    * is what CThread::Start's zero-initialised m_hThread would keep. */
   th = 0;

   if (pthread_create(&th, 0, worker, 0) != 0) { exit(7); }

   struct timeval now;
   gettimeofday(&now, 0);
   struct timespec deadline;
   deadline.tv_sec  = now.tv_sec + 3;
   deadline.tv_nsec = now.tv_usec * 1000;

   pthread_mutex_lock(&mu);
   while (ran == 0) {
      if (pthread_cond_timedwait(&cv, &mu, &deadline) != 0) { break; }
   }
   int r = ran;
   pthread_t reported = self_reported;
   pthread_mutex_unlock(&mu);

   if (!r)                                    { exit(7); }
   if (th == 0)                               { exit(9); }  /* never written back */
   if (!pthread_equal(th, reported))          { exit(8); }  /* written, but wrong */
   if (pthread_join(th, 0) != 0)              { exit(6); }  /* not a usable handle */
   exit(42);
}
