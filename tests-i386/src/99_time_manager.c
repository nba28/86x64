/* 99_time_manager — the classic Time Manager (carbon_timemgr_shim.c).
 *
 * A TMTask is a queue element the Time Manager keeps, and tmAddr is an i386
 * procedure it calls later with the task pointer. abigen's generic bridge
 * deep-copied the record into its own frame and handed native CarbonCore the
 * i386 procedure (Portal 2 Bink's RADCB timer faulted after PrimeTime).
 * Checks: the task fires with ITS OWN record, the active bit is set while armed
 * and cleared on expiry, a procedure can re-prime itself (Bink's pattern), and
 * RmvTime on an armed task reports the unexpired time in tmCount (negative =
 * microseconds when primed with microseconds).
 */
extern int printf(const char *, ...);
extern void exit(int);
extern int usleep(unsigned);

#pragma pack(push, 2)
typedef struct TMTask {
   void *qLink; short qType; void (*tmAddr)(struct TMTask *);
   long tmCount, tmWakeUp, tmReserved;
} TMTask;
#pragma pack(pop)

extern short InstallTimeTask(TMTask *);
extern void  InsTime(TMTask *);
extern void  PrimeTime(TMTask *, long);
extern void  RmvTime(TMTask *);
extern short RemoveTimeTask(TMTask *);

static volatile int fired, self_ok;
static TMTask t1, t2;

static void proc(TMTask *t) {
   if (t == &t1) self_ok = 1;
   if (++fired == 1) PrimeTime(t, 5);      /* re-prime from inside the task */
}

int main(void) {
   t1.tmAddr = proc;
   printf("install=%d\n", InstallTimeTask(&t1));
   PrimeTime(&t1, 10);
   printf("active=%d\n", (t1.qType & 0x8000) ? 1 : 0);
   for (int i = 0; i < 200 && fired < 2; i++) usleep(5000);
   printf("fired=%d self=%d\n", fired, self_ok);
   printf("cleared=%d\n", (t1.qType & 0x8000) ? 0 : 1);
   printf("remove=%d\n", RemoveTimeTask(&t1));

   InsTime(&t2);                            /* no tmAddr: a pure countdown */
   PrimeTime(&t2, -2000000);                /* 2 s in microseconds */
   usleep(10000);
   RmvTime(&t2);
   printf("left_us=%d\n", t2.tmCount < -1000000 && t2.tmCount > -2000000 ? 1 : 0);
   printf("stopped=%d\n", (t2.qType & 0x8000) ? 0 : 1);
   exit(0);
}
