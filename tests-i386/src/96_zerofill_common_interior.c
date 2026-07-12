/*
 * 96_zerofill_common_interior — guard for the ZEROFILL section rework (the
 * Halo renderer SIGFPE root cause). Zerofill sections (S_ZEROFILL __bss /
 * __common) occupy NO file bytes; the translator used to parse them BYTE-BY-
 * BYTE from nonexistent file offsets (one 1-byte ZeroBlob per section byte)
 * and let Build advance the FILE cursor across the whole span. For a 1.5MB
 * __common that (a) bloated every image by the span, (b) flooded the
 * resolvers with per-byte keys so that ORDINARY OFFSETS aliasing the huge
 * vmaddr range resolved "exactly" and were mis-relocated (`addl $0x124f80,
 * %edx` computing &freqstruct[150000] against a slot-loaded base gained a
 * translated section base -> garbage pointer -> SIGBUS here, zeroed timer
 * freq -> divide error in Halo), and (c) let file-backed sections land after
 * the zerofill span, overlaying their bytes onto the zerofill's mapped page.
 *
 * Now a zerofill section is ONE spanning ZeroBlob extent (split at interior
 * placeholder anchors) that reserves vmaddr space only. This test asserts:
 *   - a huge __common survives translation (size + zero-init preserved);
 *   - runtime base+offset arithmetic into its interior is NOT mis-relocated
 *     (the PIC-anchored heuristic-immediate cancel);
 *   - a compile-time interior pointer baked into __data IS relocated to the
 *     same place (containing-extent fallback — the Halo non-lazy-slot shape);
 *   - a second zerofill symbol keeps a correct, zero-initialized identity
 *     (extent split at symbol placeholders + zerofill kept as segment tail).
 *
 * Expected exit code: 42.
 */
extern int printf(const char *, ...);
extern void exit(int);

long long freqstruct[200000];  /* __DATA,__common: 0x186a00 = 1.5MB zerofill */
int after_marker;              /* second zerofill symbol -> extent split */

/* Compile-time INTERIOR pointer into the zerofill span, baked into __data as
 * an absolute i386 address (the Halo `[0x5b5578] -> 0x5b32a0` slot shape). */
long long *baked = &freqstruct[150000];

int main(void) {
   long long *p = &freqstruct[150000]; /* runtime slot-base + offset arith */
   freqstruct[150000] = 0x1122334455667788LL;
   after_marker = 7;
   printf("via_runtime_ptr=%08x%08x\n",
          (unsigned)(*p >> 32), (unsigned)(*p & 0xffffffffu));
   printf("via_baked_ptr=%08x%08x\n",
          (unsigned)(*baked >> 32), (unsigned)(*baked & 0xffffffffu));
   printf("baked_eq_runtime=%d\n", baked == p);
   printf("zero_neighbor=%d\n", (int)freqstruct[150001]); /* never written */
   printf("after_marker=%d\n", after_marker);
   exit((*p == 0x1122334455667788LL && baked == p &&
         freqstruct[150001] == 0 && after_marker == 7) ? 42 : 1);
}
