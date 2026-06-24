/*
 * 23_pthread_tls — exercises the abigen output-pointer bounce-buffer bridge
 * via the pthread thread-local-storage API (the Portal 2 CThreadLocalBase
 * blocker).
 *
 * `pthread_key_create(pthread_key_t *key, ...)` is an OUT-param call: abigen
 * allocates a native scratch buffer on the (high, >4GB) x86_64 stack, hands
 * its address to the native callee, then copies the written key back into the
 * i386 4-byte slot. The bug fixed here: the bounce-buffer pointer was passed
 * to the callee with the i386 pointer width (`mov edi,r11d`), truncating the
 * high stack address — so the native pthread_key_create wrote the new key to
 * a low garbage address and our buffer stayed uninitialised, leaving key == 0.
 * Get()/setspecific then used key 0 -> garbage TLS slot (Portal 2 crash).
 *
 * Round-trips a value through TLS: store &marker under a fresh key, read it
 * back, and exit(42) iff it survived. A truncated key fails (wrong/zero key,
 * or a fault inside the native key_create writing through the bad pointer).
 *
 * pthread_key_t is `unsigned long` (4 bytes on i386).
 */
extern void exit(int status);
extern int pthread_key_create(unsigned long *key, void (*destructor)(void *));
extern int pthread_setspecific(unsigned long key, const void *value);
extern void *pthread_getspecific(unsigned long key);

int main(void) {
   unsigned long key = 0;

   if (pthread_key_create(&key, 0) != 0) {
      exit(1);
   }

   int marker = 0;
   if (pthread_setspecific(key, &marker) != 0) {
      exit(2);
   }

   if (pthread_getspecific(key) != &marker) {
      exit(3);
   }

   exit(42);
}
