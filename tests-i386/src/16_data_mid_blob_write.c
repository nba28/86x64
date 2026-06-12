/*
 * 16_data_mid_blob_write — absolute byte write to a flag that lives MID a
 * larger initialized __DATA object (not at a section/blob boundary).
 *
 * Tests #11's hypothesis for the iPhoto 8th blocker: `movb $0,[abs32]` where
 * abs32 is in __DATA,__data (S_REGULAR, parsed as multi-byte DataBlobs rather
 * than per-byte ZeroBlobs like __bss). If the resolver only registers a blob
 * at each DataBlob START, then resolve(mid-blob-addr) misses the exact key and
 * the store mis-relocates (the iPhoto crash: resolved memdisp landed in __TEXT
 * → write fault). Test 12 already proves the __bss (per-byte ZeroBlob) case
 * works; this is the __data multi-byte-blob analogue.
 *
 * `state` is initialized non-zero so it lands in __DATA,__data (not __bss).
 * flags[] sits MID the initialized object; we clear flags[3] (a non-boundary,
 * non-aligned offset) via absolute addressing (-fno-pic, see Makefile rule).
 *
 * Expected exit code: 0  (flags[3] cleared to 0; checksum of the rest == 0x2a).
 */
extern void exit(int status);

/* one initialized __data object; flags[3] is deliberately mid-object */
static struct {
    unsigned int  guard;
    unsigned char flags[8];
    unsigned int  tail;
} state = { 0xAABBCCDD, {10, 11, 12, 13, 14, 15, 16, 17}, 0x11223344 };

__attribute__((noinline)) void clear_flag(void) {
    state.flags[3] = 0;     /* movb $0, <abs32 = &state.flags[3]> (mid-blob) */
}

int main(void) {
    clear_flag();
    /* flags now {10,11,12,0,14,15,16,17}; sum = 10+11+12+0+14+15+16+17 = 95.
       Validate flags[3]==0 AND the neighbours are intact (guard/tail unchanged). */
    int ok = (state.flags[3] == 0)
          && (state.flags[2] == 12) && (state.flags[4] == 14)
          && (state.guard == 0xAABBCCDD) && (state.tail == 0x11223344);
    exit(ok ? 0 : 1);
    return 0;
}
