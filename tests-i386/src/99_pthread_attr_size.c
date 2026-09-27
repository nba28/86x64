/* pthread-attr-size: Darwin's _opaque_pthread_*_t.__opaque is arch-sized
 * (attr: 36 bytes i386, 56 x86_64). abigen marshalled the i386 side with the
 * x86_64 length, so pthread_attr_init's copy-back wrote 60 bytes into a
 * 40-byte i386 object -- PvZ's stack attr zeroed its caller's saved regs and
 * return address. The canary right after the attr must survive.
 * i386 layout declared by hand (the SL sysroot mount ships no pthread.h). */
typedef struct { long sig; char opaque[36]; } attr32;
int pthread_attr_init(attr32 *);
int pthread_attr_setdetachstate(attr32 *, int);
int pthread_attr_getdetachstate(const attr32 *, int *);
int pthread_attr_destroy(attr32 *);
void exit(int) __attribute__((noreturn));

int main(void) {
    struct { attr32 a; volatile unsigned canary[6]; } s;
    for (int i = 0; i < 6; i++) s.canary[i] = 0xC0FFEE00u + i;
    if (pthread_attr_init(&s.a)) exit(2);
    if (pthread_attr_setdetachstate(&s.a, 2 /* PTHREAD_CREATE_DETACHED */)) exit(3);
    int st = 0;
    pthread_attr_getdetachstate(&s.a, &st);
    pthread_attr_destroy(&s.a);
    for (int i = 0; i < 6; i++) if (s.canary[i] != 0xC0FFEE00u + i) exit(1);
    exit(42);
}
