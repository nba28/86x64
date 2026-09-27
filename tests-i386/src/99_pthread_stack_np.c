/* pthread-stack-np: pthread_get_stackaddr_np/stacksize_np on pthread_self()
 * must describe the stack this code runs on. abigen dereferenced the i386
 * pthread_t token as a struct pointer (Numbers SFCompatibility SIGSEGV).
 * ON exits 42; OFF (the previous libabiconv) faults. */
typedef void *pthread_t;
pthread_t pthread_self(void);
void *pthread_get_stackaddr_np(pthread_t);
unsigned long pthread_get_stacksize_np(pthread_t);
unsigned pthread_mach_thread_np(pthread_t);
void exit(int) __attribute__((noreturn));
int main(void) {
    volatile int local = 0;
    pthread_t t = pthread_self();
    unsigned long top = (unsigned long)pthread_get_stackaddr_np(t);
    unsigned long size = pthread_get_stacksize_np(t);
    unsigned long p = (unsigned long)&local;
    if (!(p < top && p >= top - size)) exit(1);
    if (pthread_mach_thread_np(t) == 0) exit(2);
    exit(42);
}
