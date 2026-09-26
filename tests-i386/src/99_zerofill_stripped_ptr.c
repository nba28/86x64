/* 99_zerofill_stripped_ptr — an initialised __data pointer into ZERO-FILL
 * memory of a locals-stripped image must still be relocated.
 *
 * iPhoto '11's main image is non-PIE and locals-stripped: its __common carries
 * no symbol at all. A __data slot holds &__common+0xac (a struct timeval) and
 * the startup code passes it to gettimeofday(). The zero-fill target gate
 * (99_zerofill_target_pair, Halo) treated the missing anchor symbol as proof
 * of a constant, left the raw i386 address in the slot, and iPhoto SIGSEGVed
 * in the gettimeofday bridge before main's first window.
 *
 * A section with no symbols can attest nothing either way, so the gate only
 * judges sections that have some. Linked with -x so g_zf has none here.
 * ON: prints ok=1. OFF (M64_ZF_GATE_STRIPPED=1 at TRANSLATE time): the slot
 * keeps its i386 value and the store faults.
 */
extern int  printf(const char *, ...);
extern void exit(int);

static unsigned char g_zf[64 * 1024];
unsigned char *g_slot = &g_zf[0x1234];   /* the __data pointer slot */

int main(void)
{
   *g_slot = 0x5a;
   printf("ok=%d\n", g_zf[0x1234] == 0x5a);
   exit(0);
}
