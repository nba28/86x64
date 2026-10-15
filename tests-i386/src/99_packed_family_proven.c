/* 99_packed_family_proven — a column of packed u16 pairs sharing one low half
 * stays integers even when every member aliases some section.
 *
 * Halo's table at 0x39cd88 holds {2, 0x36}, {2, 0x38} ... {2, 0x5b} in 0x24-byte
 * records. In its small image each pair spells an address: those in symbol-free
 * __bss were proven constants, those in __data/__const were relocated. The
 * family rule wanted a member that addresses nothing; a member a gate proved
 * constant now counts too.
 *
 * Linked with -x so __bss carries no symbol. ON: ok=1. OFF
 * (M64_NO_PACKED_FAMILY_PROVEN=1 at translate time): {2, 1} is relocated.
 */
extern int  printf(const char *, ...);
extern void exit(int);

char filler[0x20000] = { 1 };           /* __data spans 0x00010002 */
static char big[0x100000];               /* __bss spans 0x00040002.. */

struct rec { int tag; unsigned pair; int none; float f[6]; };   /* 0x24 bytes */
struct rec recs[5] = {
   { 0x42524, 0x00010002, -1, { 0.6f, 0.7f } },
   { 0x42524, 0x00040002, -1, { 0.8f, 0.7f } },
   { 0x42524, 0x00050002, -1, { 1.0f, 0.7f } },
   { 0x42524, 0x00060002, -1, { 0.8f, 0.6f } },
   { 0x42524, 0x00070002, -1, { 0.8f, 0.6f } },
};

int main(void)
{
   big[0] = filler[0];
   const unsigned v = recs[0].pair;
   /* compare halves: an imm32 0x00010002 in this code aliases __data too */
   printf("ok=%d pair=0x%08x\n", (v & 0xffff) == 2 && (v >> 16) == 1, v);
   exit(0);
}
