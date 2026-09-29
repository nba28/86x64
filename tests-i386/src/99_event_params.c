/* 99_event_params — do Carbon event parameters survive the i386<->x86_64
 * boundary? References are 4-byte handles on i386 and 8-byte pointers natively,
 * CGFloat geometry is float vs double, HICommand embeds a MenuRef. The generic
 * bridge passed the i386 buffer/size straight through: PvZ windowed read its
 * WindowRef back truncated and ignored every click (2026-09-29).
 * Headless: an event is built with CreateEvent, no window involved. */
extern int  printf(const char *, ...);
extern void exit(int);
typedef const void *EventRef; typedef unsigned int u32;
typedef struct { short v, h; } QDPoint;
typedef struct { float x, y; } HIPoint32;
typedef struct { u32 attributes, commandID; u32 menuRef; unsigned short index; } HICommand32;
extern int CreateEvent(const void *alloc, u32 cls, u32 kind, double when, u32 attrs, EventRef *out);
extern int SetEventParameter(EventRef, u32 name, u32 type, u32 size, const void *data);
extern int GetEventParameter(EventRef, u32 name, u32 type, u32 *outType, u32 size, u32 *outSize, void *out);
extern const void *__CFStringMakeConstantString(const char *);
extern long CFStringGetLength(const void *);
int main(void)
{
   EventRef e = 0;
   int ok = CreateEvent(0, 'mous', 1, 0.0, 0, &e) == 0 && e;
   printf("event=%d\n", ok);
   QDPoint q = { 20, 10 }, q2 = { 0, 0 };           /* v=20 h=10 */
   SetEventParameter(e, 'mloc', 'QDpt', sizeof q, &q);
   GetEventParameter(e, 'mloc', 'QDpt', 0, sizeof q2, 0, &q2);
   printf("qdpoint_same=%d\n", q2.h == 10 && q2.v == 20);
   HIPoint32 hp = { -1, -1 }; u32 sz = 0;
   int r = GetEventParameter(e, 'mloc', 'hipt', 0, sizeof hp, &sz, &hp);   /* native coercion */
   printf("hipoint_coerced=%d\n", r == 0 && hp.x == 10.0f && hp.y == 20.0f && sz == 8);
   const void *s = __CFStringMakeConstantString("hi"), *s2 = 0;
   SetEventParameter(e, 'dobj', 'cfst', 4, &s);
   GetEventParameter(e, 'dobj', 'cfst', 0, 4, 0, &s2);
   printf("cfref_roundtrip=%d\n", s2 && CFStringGetLength(s2) == 2);
   HICommand32 c = { 0, 'quit', 0, 0 }, c2 = { 0, 0, 0, 0 };
   SetEventParameter(e, '----', 'hcmd', sizeof c, &c);
   r = GetEventParameter(e, '----', 'hcmd', 0, sizeof c2, 0, &c2);
   printf("hicommand_roundtrip=%d\n", r == 0 && c2.commandID == 'quit');
   exit(0);
}
