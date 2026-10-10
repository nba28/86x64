/* 99_recfield_zf_column — a table of pointers to separate symbol-free zero-fill
 * objects must be relocated: its entries are one column of a pointer array,
 * not integers that happen to alias zero-fill.
 *
 * Call of Duty 4 (non-PIE, locals-stripped) runs an init table of __bss object
 * pointers {0x163d9e0, 0x163cec0, 0x15e4ea0, ...}. The record-field gate saw
 * each entry's siblings land in symbol-free zero-fill, counted them as proven
 * integers and left every entry raw; the first init wrote through raw i386
 * 0x1587300 (SIGBUS).
 *
 * Linked with -x so __bss carries no symbol; the objects are reached only
 * through the table. ON: ok=1. OFF (M64_NO_RECFIELD_ZF_COLUMN=1 and
 * M64_NO_ZF_ABS_NEIGHBOUR=1 at TRANSLATE time): the entries stay raw and the
 * first store faults.
 */
extern int  printf(const char *, ...);
extern void exit(int);

static char o0[32], o1[32], o2[32], o3[32], o4[32], o5[32], o6[32], o7[32],
            o8[32], o9[32], o10[32], o11[32], o12[32], o13[32], o14[32], o15[32];
char *g_objs[16] = { o0, o1, o2, o3, o4, o5, o6, o7,
                     o8, o9, o10, o11, o12, o13, o14, o15 };

int main(void)
{
   int ok = 1;
   for (int i = 0; i < 16; i++) g_objs[i][0] = (char)(i + 1);
   for (int i = 0; i < 16; i++) ok &= g_objs[i][0] == (char)(i + 1);
   printf("ok=%d\n", ok);
   exit(0);
}
