/* 99_cpp_cow_string_tail.cc — the rest of the i386 GCC-4 COW std::string /
 * std::wstring out-of-line surface (companion to 66_cpp_cow_string.cc).
 *
 * Every basic_string member a translated image imports but cow_string_shim.c
 * does NOT reimplement binds RAW to the native x86_64 libstdc++, which reads
 * `this` from rdi (garbage: the i386 caller passed it on the stack) and an
 * 8-byte-field _Rep. MEASURED, Portal 2 libcef.dylib: CEF init calls
 * wstring::reserve -> native `mov rax,[rdi]; cmp rsi,[rax-0x10]` -> SIGSEGV
 * at 0xfffffffffffffff0. The tree-wide Portal 2 audit found 41 such members
 * (libcef, libsteam, vguimatsurface, vscript); this drives each shape once.
 *
 * Same technique as 66: mangled libstdc++ exports declared via asm() labels,
 * driven on hand-built i386 COW objects { uint32_t _M_p }.
 */
#include <cstdio>
#include <cstdint>
#include <cstdlib>

struct GccStr  { uint32_t p; GccStr()  {} GccStr(const GccStr&  o) : p(o.p) {} ~GccStr()  {} };
struct GccWStr { uint32_t p; GccWStr() {} GccWStr(const GccWStr& o) : p(o.p) {} ~GccWStr() {} };
struct Rep { uint32_t length, capacity; int32_t refcount; };

#define SS "__ZNSs"
#define KSS "__ZNKSs"
#define SW "__ZNSbIwSt11char_traitsIwESaIwEE"
#define KSW "__ZNKSbIwSt11char_traitsIwESaIwEE"

/* ---- already-shimmed helpers used to build / print ---- */
void        Ss_ctor_cstr(void* self, const char* s, const void* a)  asm(SS "C1EPKcRKSaIcE");
void        Ss_dtor(void* self)                                     asm(SS "D1Ev");
const char* Ss_c_str(const void* self)                              asm(KSS "5c_strEv");
unsigned    Ss_size(const void* self)                               asm(KSS "4sizeEv");
void        Sw_ctor_cstr(void* self, const wchar_t* s, const void* a) asm(SW "C1EPKwRKS1_");
void        Sw_dtor(void* self)                                     asm(SW "D1Ev");
const wchar_t* Sw_c_str(const void* self)                           asm(KSW "5c_strEv");

/* ---- narrow: the new surface ---- */
unsigned Ss_flo_ch(const void*, int, unsigned)                      asm(KSS "12find_last_ofEcm");
unsigned Ss_flo_cstr(const void*, const char*, unsigned)            asm(KSS "12find_last_ofEPKcm");
unsigned Ss_flo_buf(const void*, const char*, unsigned, unsigned)   asm(KSS "12find_last_ofEPKcmm");
unsigned Ss_ffo_ch(const void*, int, unsigned)                      asm(KSS "13find_first_ofEcm");
unsigned Ss_flno_buf(const void*, const char*, unsigned, unsigned)  asm(KSS "16find_last_not_ofEPKcmm");
unsigned Ss_ffno_ch(const void*, int, unsigned)                     asm(KSS "17find_first_not_ofEcm");
const char* Ss_cat(const void*, unsigned)                           asm(KSS "2atEm");
char*    Ss_at(void*, unsigned)                                     asm(SS "2atEm");
unsigned Ss_copy(const void*, char*, unsigned, unsigned)            asm(KSS "4copyEPcmm");
bool     Ss_empty(const void*)                                      asm(KSS "5emptyEv");
unsigned Ss_rfind_buf(const void*, const char*, unsigned, unsigned) asm(KSS "5rfindEPKcmm");
unsigned Ss_length(const void*)                                     asm(KSS "6lengthEv");
int      Ss_cmp_sub_cstr(const void*, unsigned, unsigned, const char*) asm(KSS "7compareEmmPKc");
int      Ss_cmp_sub_str(const void*, unsigned, unsigned, const void*)  asm(KSS "7compareEmmRKSs");
char*    Ss_S_construct(unsigned, int, const void*)                 asm(SS "12_S_constructEmcRKSaIcE");
void     Ss_M_destroy(void* rep, const void*)                       asm(SS "4_Rep10_M_destroyERKSaIcE");
Rep*     Ss_S_create(unsigned, unsigned, const void*)               asm(SS "4_Rep9_S_createEmmRKSaIcE");
void*    Ss_append_fill(void*, unsigned, int)                       asm(SS "6appendEmc");
void*    Ss_append_substr(void*, const void*, unsigned, unsigned)   asm(SS "6appendERKSsmm");
void*    Ss_insert_buf(void*, unsigned, const char*, unsigned)      asm(SS "6insertEmPKcm");
void*    Ss_insert_substr(void*, unsigned, const void*, unsigned, unsigned) asm(SS "6insertEmRKSsmm");
void     Ss_resize1(void*, unsigned)                                asm(SS "6resizeEm");
void*    Ss_replace_fill(void*, unsigned, unsigned, unsigned, int)  asm(SS "7replaceEmmmc");
void*    Ss_replace_iter_fill(void*, char*, char*, unsigned, int)   asm(SS "7replaceEN9__gnu_cxx17__normal_iteratorIPcSsEES2_mc");
void*    Ss_assign_ch(void*, int)                                   asm(SS "aSEc");
void*    Ss_assign_cstr_op(void*, const char*)                      asm(SS "aSEPKc");
void     Ss_ctor_substr(void*, const void*, unsigned, unsigned)     asm(SS "C1ERKSsmm");
void*    Ss_pluseq_ch(void*, int)                                   asm(SS "pLEc");
void*    Ss_pluseq_cstr(void*, const char*)                         asm(SS "pLEPKc");
void*    Ss_pluseq_str(void*, const void*)                          asm(SS "pLERKSs");
char*    Ss_begin(void*)                                            asm(SS "5beginEv");

/* ---- wide: the new surface ---- */
unsigned Sw_ffo_buf(const void*, const wchar_t*, unsigned, unsigned)  asm(KSW "13find_first_ofEPKwmm");
unsigned Sw_flno_buf(const void*, const wchar_t*, unsigned, unsigned) asm(KSW "16find_last_not_ofEPKwmm");
unsigned Sw_ffno_buf(const void*, const wchar_t*, unsigned, unsigned) asm(KSW "17find_first_not_ofEPKwmm");
int      Sw_cmp_sub_str(const void*, unsigned, unsigned, const void*) asm(KSW "7compareEmmRKS2_");
wchar_t* Sw_S_construct(unsigned, wchar_t, const void*)             asm(SW "12_S_constructEmwRKS1_");
void     Sw_M_destroy(void* rep, const void*)                       asm(SW "4_Rep10_M_destroyERKS1_");
Rep*     Sw_S_create(unsigned, unsigned, const void*)               asm(SW "4_Rep9_S_createEmmRKS1_");
void*    Sw_append_buf(void*, const wchar_t*, unsigned)             asm(SW "6appendEPKwm");
void     Sw_resize(void*, unsigned, wchar_t)                        asm(SW "6resizeEmw");
void     Sw_reserve(void*, unsigned)                                asm(SW "7reserveEm");
void     Sw_ctor_buf(void*, const wchar_t*, unsigned, const void*)  asm(SW "C1EPKwmRKS1_");

/* ---- refcount atomics called out of line by inlined COW code ---- */
int  X_exchange_and_add(volatile int*, int) asm("__ZN9__gnu_cxx18__exchange_and_addEPVii");
void X_atomic_add(volatile int*, int)       asm("__ZN9__gnu_cxx12__atomic_addEPVii");

static void pr(const char* tag, const void* s) { printf("%s '%s' (%u)\n", tag, Ss_c_str(s), Ss_size(s)); }
static void prw(const char* tag, const void* s) {
    const wchar_t* w = Sw_c_str(s);
    printf("%s w'", tag);
    for (unsigned i = 0; w[i]; i++) printf("%c", (char)w[i]);
    printf("'\n");
}

int main() {
    char alloc;

    printf("== narrow search ==\n");
    GccStr s; Ss_ctor_cstr(&s, "a/b/c.txt", &alloc);
    printf("flo '/'        = %u\n", Ss_flo_ch(&s, '/', (unsigned)-1));   /* 3 */
    printf("flo \"./\"       = %u\n", Ss_flo_cstr(&s, "./", (unsigned)-1)); /* 5 */
    printf("flo buf(\"/x\",4,1) = %u\n", Ss_flo_buf(&s, "/x", 4, 1));    /* 3 */
    printf("ffo '/' @2     = %u\n", Ss_ffo_ch(&s, '/', 2));              /* 3 */
    printf("flno \"txt\"     = %u\n", Ss_flno_buf(&s, "txt", (unsigned)-1, 3)); /* 5 */
    printf("ffno 'a'       = %u\n", Ss_ffno_ch(&s, 'a', 0));             /* 1 */
    printf("rfind buf \"/\"  = %u\n", Ss_rfind_buf(&s, "/c", (unsigned)-1, 1)); /* 3 */
    printf("at(2)          = %c\n", *Ss_cat(&s, 2));                     /* b */
    printf("length         = %u\n", Ss_length(&s));                      /* 9 */
    printf("empty          = %d\n", (int)Ss_empty(&s));                  /* 0 */
    { char buf[8] = {0}; unsigned n = Ss_copy(&s, buf, 5, 6);
      printf("copy(5,6)      = %u '%s'\n", n, buf); }                     /* 3 'txt' */
    printf("cmp(2,1,\"b\")   = %d\n", Ss_cmp_sub_cstr(&s, 2, 1, "b"));    /* 0 */
    { GccStr t; Ss_ctor_cstr(&t, "c.txt", &alloc);
      printf("cmp(4,5,str)   = %d\n", Ss_cmp_sub_str(&s, 4, 5, &t));      /* 0 */
      Ss_dtor(&t); }

    printf("== narrow mutate ==\n");
    GccStr m; Ss_ctor_cstr(&m, "abc", &alloc);
    GccStr share; Ss_ctor_substr(&share, &m, 0, 3);  pr("ctor_substr", &share);
    Ss_append_fill(&m, 3, 'x');          pr("append(3,x)", &m);       /* abcxxx */
    Ss_append_substr(&m, &s, 2, 1);      pr("append(s,2,1)", &m);     /* abcxxxb */
    Ss_insert_buf(&m, 1, "ZZ", 2);       pr("insert(1,ZZ)", &m);      /* aZZbcxxxb */
    Ss_insert_substr(&m, 0, &s, 6, 3);   pr("insert(0,s,6,3)", &m);   /* txtaZZbcxxxb */
    Ss_resize1(&m, 4);                   pr("resize(4)", &m);         /* txta */
    Ss_replace_fill(&m, 1, 2, 3, '-');   pr("replace(1,2,3,-)", &m);  /* t---a */
    { char* b = Ss_begin(&m);
      Ss_replace_iter_fill(&m, b + 1, b + 4, 1, '+'); pr("replace(it,it,1,+)", &m); } /* t+a */
    *Ss_at(&m, 0) = 'T';                 pr("at(0)=T", &m);           /* T+a */
    Ss_assign_ch(&m, 'q');               pr("op=(q)", &m);
    Ss_assign_cstr_op(&m, "hi");         pr("op=(hi)", &m);
    Ss_pluseq_ch(&m, '!');               pr("+=!", &m);
    Ss_pluseq_cstr(&m, " yo");           pr("+=yo", &m);
    Ss_pluseq_str(&m, &share);           pr("+=share", &m);           /* hi! yoabc */
    pr("share intact", &share);                                        /* abc */

    printf("== narrow COW internals ==\n");
    Rep* r = Ss_S_create(10, 0, &alloc);
    printf("S_create cap   = %u ref=%d\n", r->capacity, r->refcount);   /* 10 0 */
    Ss_M_destroy(r, &alloc);
    Rep* r2 = Ss_S_create(12, 10, &alloc);
    printf("S_create grow  = %u\n", r2->capacity);                      /* 20 */
    Ss_M_destroy(r2, &alloc);
    { GccStr c; c.p = (uint32_t)(uintptr_t)Ss_S_construct(4, 'k', &alloc); pr("S_construct", &c); Ss_dtor(&c); }

    printf("== wide ==\n");
    GccWStr w; Sw_ctor_buf(&w, L"hello world", 5, &alloc); prw("ctor_buf(5)", &w);
    Sw_reserve(&w, 64);                                                 /* the CEF crash */
    printf("reserve cap>=64 = %d\n", ((Rep*)(uintptr_t)(w.p - 12))->capacity >= 64);
    prw("after reserve", &w);
    Sw_append_buf(&w, L", there", 7);  prw("append_buf", &w);
    Sw_resize(&w, 14, L'.');           prw("resize(14,.)", &w);
    printf("wffo \"ol\"      = %u\n", Sw_ffo_buf(&w, L"ol", 0, 2));      /* 2 */
    printf("wflno \".\"      = %u\n", Sw_flno_buf(&w, L".", (unsigned)-1, 1)); /* 11 */
    printf("wffno \"hel\"    = %u\n", Sw_ffno_buf(&w, L"hel", 0, 3));    /* 4 */
    { GccWStr t; Sw_ctor_cstr(&t, L"there", &alloc);
      printf("wcmp(7,5,str)  = %d\n", Sw_cmp_sub_str(&w, 7, 5, &t));     /* 0 */
      Sw_dtor(&t); }
    { GccWStr c; c.p = (uint32_t)(uintptr_t)Sw_S_construct(3, L'z', &alloc); prw("wS_construct", &c); Sw_dtor(&c); }
    Rep* wr = Sw_S_create(5, 0, &alloc);
    printf("wS_create cap  = %u\n", wr->capacity);
    Sw_M_destroy(wr, &alloc);

    printf("== atomics ==\n");
    { volatile int rc = 5;
      int old = X_exchange_and_add(&rc, -1);
      printf("exchange_and_add old=%d new=%d\n", old, rc);   /* 5 4 */
      X_atomic_add(&rc, 3);
      printf("atomic_add new=%d\n", rc); }                   /* 7 */

    Ss_dtor(&s); Ss_dtor(&m); Ss_dtor(&share); Sw_dtor(&w);
    printf("done\n");
    exit(0);
}
