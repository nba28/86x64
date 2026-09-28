/* 99_memmgr_blockmove — the classic Memory Manager block primitives, removed
 * from modern macOS and implemented in libabiconv (carbon_memory.c).
 * Overlapping moves in both directions, the Uncached variants, BlockZero, and
 * a NEGATIVE count: classic Size is signed, so it must be a no-op
 * (an unsigned read would memmove 4 GB). Exit 0 = ok. */
#include <stdlib.h>
#include <string.h>

typedef long Size;
void BlockMove(const void *, void *, Size);
void BlockMoveData(const void *, void *, Size);
void BlockMoveUncached(const void *, void *, Size);
void BlockMoveDataUncached(const void *, void *, Size);
void BlockZero(void *, Size);
void BlockZeroData(void *, Size);

int main(void) {
    char b[16];
    memcpy(b, "abcdefghijklmnop", 16);
    BlockMoveData(b, b + 2, 6);                   /* overlap, forward */
    if (memcmp(b, "ababcdefijklmnop", 16)) exit(1);
    BlockMove(b + 2, b, 6);                       /* overlap, backward */
    if (memcmp(b, "abcdefefijklmnop", 16)) exit(2);
    BlockMoveUncached("XY", b, 2);
    BlockMoveDataUncached("Z", b + 2, 1);
    if (memcmp(b, "XYZdefefijklmnop", 16)) exit(3);
    BlockZero(b + 3, 2);
    BlockZeroData(b + 5, 1);
    if (b[3] || b[4] || b[5] || b[6] != 'e') exit(4);
    BlockMoveData(b, b + 8, -1);                  /* signed Size: no-op */
    BlockZero(b, -5);
    if (b[0] != 'X' || b[8] != 'i') exit(5);
    exit(0);
}
