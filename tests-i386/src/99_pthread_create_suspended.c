/*
 * 99_pthread_create_suspended — pthread_create_suspended_np shares
 * pthread_create's hand shim (pthread_sync_shim.c): the start routine through a
 * callback trampoline, the caller's pthread_t written as the thread's Mach-port
 * token. The generated bridge deep-converted *pthread_t as a struct and wrote
 * 8 KB past its frame (Portal 2 libmilesx86 sound init).
 *
 * Exit 42 = created suspended with an attr, resumed through its Mach port,
 * joined with the routine's value. M64_NO_PTHREAD_TOKEN=1 (run time) drops the
 * handle write-back: resume/join fail, not 42.
 */
#include <pthread.h>
#include <stdlib.h>

extern int pthread_create_suspended_np(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);
extern int thread_resume(unsigned int);
extern unsigned int pthread_mach_thread_np(pthread_t);

static void *body(void *arg) { return (void *)((long)arg + 2); }

int main(void) {
   pthread_attr_t at;
   pthread_attr_init(&at);
   pthread_attr_setstacksize(&at, 256 * 1024);
   pthread_t t = 0;
   if (pthread_create_suspended_np(&t, &at, body, (void *)40) != 0) exit(3);
   if (!t) exit(4);
   if (thread_resume(pthread_mach_thread_np(t)) != 0) exit(5);
   void *r = 0;
   if (pthread_join(t, &r) != 0) exit(6);
   exit((long)r == 42 ? 42 : 7);
}
