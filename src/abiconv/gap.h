// gap.h — a SILENT gap speaks once: a shim that returns a constant without doing
// the job (STUB), or a by-name native lookup that found nothing (dlsym). Each
// site reports its first hit per process through x64_gap_hit() (gap.c); later
// hits cost one plain load. coverage-audit.py --reach reads the ledger it writes.
#ifndef ABICONV_GAP_H
#define ABICONV_GAP_H
#include <stdint.h>

void x64_gap_hit(const char *kind, const char *sym, const char *who, uint32_t caller);
// raw native calls (see gap.c): is this address i386-callable (libabiconv or a
// translated image)? / arm an image's bound call slots with one-shot stubs.
int  x64_gap_i386_callable(const void *addr);
struct mach_header_64;
void x64_gap_arm_raw_slots(const struct mach_header_64 *mh, intptr_t slide);

#define GAP_ONCE(kind, sym, who, caller) do { static int gap_hit_; \
   if (!gap_hit_ && !__atomic_exchange_n(&gap_hit_, 1, __ATOMIC_RELAXED)) \
      x64_gap_hit(kind, sym, who, caller); } while (0)

// In a shim_X(uint32_t *args) called through MTSHIM: args[-1] is the i386
// caller's return address; __func__ + 5 drops "shim_".
#define GAP_STUB(args) GAP_ONCE("stub", __func__ + 5, 0, ((const uint32_t *)(args))[-1])

#endif
