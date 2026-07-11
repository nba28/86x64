/* 84_cpp_cow_empty_rep.cc — the GCC-4 libstdc++ COW-string EMPTY-REP DATA
 * surface: _S_empty_rep_storage + the out-of-line COW internals that reach it
 * (_Rep::_M_dispose, _M_mutate, _M_replace_aux, swap, append(PKc,n),
 * find(PKc,pos,n), const operator[]).
 *
 * THE BUG THIS GUARDS (Civ IV Steam, first static initializer): GCC-4 INLINES
 * the COW fast paths (default ctor, empty-rep identity checks) into the
 * target's own code, referencing the DATA symbol _S_empty_rep_storage through
 * a non-lazy pointer slot. Un-shadowed, dyld binds that 8-byte slot to the
 * NATIVE x86_64 libstdc++ in the shared cache (>4GB) and the translated i386
 * 32-bit `movl slot(%rip),%reg` reads a TRUNCATED address -> the following
 * rep-header deref (length at _M_p-12) faults. Civ IV: slot 0x11e5e100 held
 * 0x00007ff955019680, code deref'd 0x55019680 -> SIGSEGV in the very first
 * std::string-using static initializer, killing startup inside slide_objc's
 * run-now init loop.
 *
 * cow_string_shim.c now EXPORTS the storage (____ZNSs4_Rep20_S_empty_rep_
 * storageE + the wide variant) as real low-4GB zeroed i386-layout reps, and
 * empty_data() returns THAT storage so the target's inlined identity checks
 * (`this != &_S_empty_rep()`) and the shim's own `p == empty` checks agree.
 * This test drives both worlds and checks they share ONE rep.
 *
 * Mirrors 66_cpp_cow_string.cc: mangled entry points declared via asm()
 * labels, hand-built i386 COW objects ({ uint32_t _M_p }).
 *
 * NOTE on exit path (fflush(NULL)+_exit, unbuffered stdout): avoids two
 * PRE-EXISTING harness gaps unrelated to this fix — (a) a translated
 * minimal-cpp crt TEARDOWN crash after main returns (rip=0x1; toggles with
 * the output FILENAME length — the identical source passes or SIGSEGVs purely
 * by name), and (b) fflush(<i386 stdout FILE*>) faulting in flockfile (no
 * FILE* is marshalled across the boundary; fflush(NULL) is safe). See
 * the known-gaps list. Neither touches the COW empty-rep surface under test.
 */
#include <cstdio>
#include <cstdint>
#include <unistd.h>

/* ---- the DATA symbols (the Civ IV crasher) ---- */
extern uint32_t NarrowERS[] asm("__ZNSs4_Rep20_S_empty_rep_storageE");
extern uint32_t WideERS[]   asm("__ZNSbIwSt11char_traitsIwESaIwEE4_Rep20_S_empty_rep_storageE");

/* ---- out-of-line entry points ---- */
void        Ss_ctor_default(void* self)                            asm("__ZNSsC1Ev");
void        Ss_dtor(void* self)                                    asm("__ZNSsD1Ev");
const char* Ss_c_str(const void* self)                             asm("__ZNKSs5c_strEv");
unsigned    Ss_size(const void* self)                              asm("__ZNKSs4sizeEv");
void        Rep_dispose(void* rep, const void* alloc)              asm("__ZNSs4_Rep10_M_disposeERKSaIcE");
void*       Ss_append_buf(void* self, const char* s, unsigned n)   asm("__ZNSs6appendEPKcm");
unsigned    Ss_find_buf(const void* self, const char* s, unsigned pos, unsigned n) asm("__ZNKSs4findEPKcmm");
const char* Ss_cindex(const void* self, unsigned pos)              asm("__ZNKSsixEm");
void        Ss_swap(void* self, void* other)                       asm("__ZNSs4swapERSs");
void*       Ss_replace_aux(void* self, unsigned pos, unsigned n1, unsigned n2, char c) asm("__ZNSs14_M_replace_auxEmmmc");

/* i386 COW string object = { uint32_t _M_p } */
struct GccStr { uint32_t p; };

static uint32_t rd32(uint32_t a) { return *(const uint32_t*)(uintptr_t)a; }

int main() {
   setvbuf(stdout, 0, _IONBF, 0);
   /* 1) the Civ IV crash shape: take the ADDRESS of _S_empty_rep_storage
    * through its data bind and deref the rep header fields directly. */
   uint32_t ep  = (uint32_t)(uintptr_t)NarrowERS + 12;   /* _S_empty_rep()._M_refdata() */
   uint32_t epw = (uint32_t)(uintptr_t)WideERS + 12;
   printf("narrow empty rep: len=%u cap=%u ref=%d\n",
          rd32(ep - 12), rd32(ep - 8), (int)rd32(ep - 4));
   printf("wide empty rep: len=%u\n", rd32(epw - 12));

   /* 2) identity coherence: a default-constructed string's _M_p must BE the
    * shared exported storage (+12) — the target's inlined fast paths and the
    * shim's empty_data() have to agree on ONE rep. */
   GccStr s; Ss_ctor_default(&s);
   printf("identity: %s\n", s.p == ep ? "shared" : "DIVERGED");

   /* 3) _Rep::_M_dispose on the empty rep must be a no-op (never freed). */
   Rep_dispose((void*)(uintptr_t)(s.p - 12), 0);
   printf("dispose empty ok len=%u\n", rd32(ep - 12));

   /* 4) the newly-covered out-of-line members. */
   Ss_append_buf(&s, "abcdefgh", 5);                     /* append(PKc, n) */
   printf("append5: '%s' size=%u\n", Ss_c_str(&s), Ss_size(&s));
   printf("find(cde,0,3)=%u\n", Ss_find_buf(&s, "cdezz", 0, 3));
   printf("cindex[2]=%c\n", *Ss_cindex(&s, 2));
   Ss_replace_aux(&s, 1, 2, 4, 'x');                     /* abcde -> axxxxde */
   printf("replace_aux: '%s'\n", Ss_c_str(&s));
   GccStr t; Ss_ctor_default(&t);
   Ss_swap(&s, &t);
   printf("swap: s='%s' t='%s'\n", Ss_c_str(&s), Ss_c_str(&t));
   Ss_dtor(&t); Ss_dtor(&s);
   printf("done\n");
   fflush(0);
   _exit(0);
}
