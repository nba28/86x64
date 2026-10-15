/* 99_record_array_column — a packed u16 pair in an array of large records stays
 * an integer even when its bytes spell an address in symbol-free zero-fill.
 *
 * Halo lists its object-type definitions (0xe0-byte records) in a pointer
 * table. Field +0xc of the `mach` record is {0x2a0, 0x40} = 0x004002a0, which
 * lands in its stripped __bss, so it was relocated as a pointer and the
 * campaign crashed reading tag data through the corrupted offset. The record
 * stride (0xe0) is wider than the record-field gate looks.
 *
 * Linked with -x so __bss carries no symbol. ON: ok=1. OFF
 * (M64_NO_RECORD_ARRAY_COLUMN=1 at translate time): the pair is relocated.
 */
extern int  printf(const char *, ...);
extern void exit(int);

static char big[0x100000];               /* __bss spans 0x00010100 */

static void f1(void) {}
static void f2(void) {}

struct rec {
   const char *name;
   int tag;
   short a, b;
   unsigned packed;                      /* {lo, hi} u16 pair */
   int none;
   void (*fn1)(void), (*fn2)(void);
   int pad[49];
};                                       /* 0xe0 bytes */

struct rec recs[6] = {
   { "obj",  'obj ', 0x228, 0x294, 0x00010100, -1, f1, f2 },
   { "unit", 'unit', 0x22c, 0x2c4, 0xffffffff, -1, f1, f2 },
   { "devi", 'devi', 0x214, -1,    0x00780234, -1, f1, f2 },
   { "item", 'item', 0x22c, -1,    0xffffffff, -1, f1, f2 },
   { "proj", 'proj', 0x2b0, -1,    0x005c027c, -1, f1, f2 },
   { "garb", 'garb', 0x244, -1,    0xffffffff, -1, f1, f2 },
};
struct rec *defs[6] = { &recs[0], &recs[1], &recs[2], &recs[3], &recs[4], &recs[5] };

int main(void)
{
   big[0] = 1;
   const unsigned v = defs[0]->packed;
   /* compare the halves: an imm32 0x00010100 in the code aliases __bss too */
   printf("ok=%d packed=0x%08x size=%d\n", (v & 0xffff) == 0x100 && (v >> 16) == 1,
          v, (int)sizeof(struct rec));
   exit(0);
}
