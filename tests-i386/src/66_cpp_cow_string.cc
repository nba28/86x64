/* 66_cpp_cow_string.cc — the SL i386 GCC-4 libstdc++ copy-on-write
 * std::string / std::wstring surface, bridged by libabiconv's
 * cow_string_shim.c (the ____ZNSs... / ____ZNSbIw... / ____ZStpl... exports
 * reached through cow_string_tramp.asm after static-interpose).
 *
 * The host SDK's <string> is libc++'s header-inlined std::__1::string, which
 * would never emit the out-of-line __ZNSs... COW entry points a real GCC-4
 * game (Civ IV) calls.  So — exactly like 63_cpp_static_map_rbtree.cc did for
 * the _Rb_tree_* surface — we declare the mangled libstdc++ exports DIRECTLY
 * via asm() labels and drive them on hand-built i386 COW string objects
 * (a bare { uint32_t _M_p }).  The string-returning members (substr,
 * operator+) return a class with a non-trivial copy ctor/dtor so the compiler
 * emits the Darwin-i386 memory-return (hidden sret pointer, callee-pops)
 * calling convention — the shape the ____...substr / ____...pl shims expect.
 *
 * This binds precisely like Civ IV's own std::string calls, so after
 * translation every one of these is redirected to cow_string_shim.c: it is the
 * end-to-end proof that the reimplemented COW surface is ABI-faithful.
 */
#include <cstdio>
#include <cstdint>
#include <cstdlib>

/* i386 COW string object = { uint32_t _M_p }.  A non-trivial copy ctor + dtor
 * force by-value returns through memory (sret), matching the real std::string
 * (a non-POD-for-calls class). */
struct GccStr  { uint32_t p; GccStr()  {} GccStr(const GccStr&  o) : p(o.p) {} ~GccStr()  {} };
struct GccWStr { uint32_t p; GccWStr() {} GccWStr(const GccWStr& o) : p(o.p) {} ~GccWStr() {} };

/* ---- narrow std::string entry points ---- */
void        Ss_ctor_default(void* self)                             asm("__ZNSsC1Ev");
void        Ss_ctor_cstr(void* self, const char* s, const void* a)  asm("__ZNSsC1EPKcRKSaIcE");
void        Ss_ctor_copy(void* self, const void* o)                 asm("__ZNSsC1ERKSs");
void        Ss_dtor(void* self)                                     asm("__ZNSsD1Ev");
const char* Ss_c_str(const void* self)                              asm("__ZNKSs5c_strEv");
unsigned    Ss_size(const void* self)                               asm("__ZNKSs4sizeEv");
void*       Ss_append_cstr(void* self, const char* s)               asm("__ZNSs6appendEPKc");
void*       Ss_append_str(void* self, const void* o)                asm("__ZNSs6appendERKSs");
void*       Ss_assign_op(void* self, const void* o)                 asm("__ZNSsaSERKSs");
void*       Ss_assign_cstr(void* self, const char* s)               asm("__ZNSs6assignEPKc");
unsigned    Ss_find_cstr(const void* self, const char* s, unsigned p)asm("__ZNKSs4findEPKcm");
unsigned    Ss_find_ch(const void* self, int c, unsigned p)         asm("__ZNKSs4findEcm");
unsigned    Ss_rfind_ch(const void* self, int c, unsigned p)        asm("__ZNKSs5rfindEcm");
int         Ss_compare_str(const void* self, const void* o)         asm("__ZNKSs7compareERKSs");
char*       Ss_begin(void* self)                                    asm("__ZNSs5beginEv");
char*       Ss_index(void* self, unsigned pos)                      asm("__ZNSsixEm");
void        Ss_push_back(void* self, int c)                         asm("__ZNSs9push_backEc");
void        Ss_reserve(void* self, unsigned n)                      asm("__ZNSs7reserveEm");
void*       Ss_resize(void* self, unsigned n, int c)                asm("__ZNSs6resizeEmc");
void*       Ss_erase(void* self, unsigned pos, unsigned n)          asm("__ZNSs5eraseEmm");
void*       Ss_replace(void* self, unsigned pos, unsigned n1, const char* s) asm("__ZNSs7replaceEmmPKc");
void        Ss_clear(void* self)                                    asm("__ZNSs5clearEv");
GccStr      Ss_substr(void* self, unsigned pos, unsigned n)         asm("__ZNKSs6substrEmm");
GccStr      Ss_plus_ss(const void* l, const void* r) asm("__ZStplIcSt11char_traitsIcESaIcEESbIT_T0_T1_ERKS6_S8_");
GccStr      Ss_plus_cs(const char* l, const void* r) asm("__ZStplIcSt11char_traitsIcESaIcEESbIT_T0_T1_EPKS3_RKS6_");

/* ---- wide std::wstring entry points (wchar_t = 4 bytes on i386) ---- */
void           Sw_ctor_default(void* self)                          asm("__ZNSbIwSt11char_traitsIwESaIwEEC1Ev");
void           Sw_ctor_cstr(void* self, const wchar_t* s, const void* a) asm("__ZNSbIwSt11char_traitsIwESaIwEEC1EPKwRKS1_");
void           Sw_dtor(void* self)                                  asm("__ZNSbIwSt11char_traitsIwESaIwEED1Ev");
const wchar_t* Sw_c_str(const void* self)                           asm("__ZNKSbIwSt11char_traitsIwESaIwEE5c_strEv");
void*          Sw_append_cstr(void* self, const wchar_t* s)         asm("__ZNSbIwSt11char_traitsIwESaIwEE6appendEPKw");
int            Sw_compare_str(const void* self, const void* o)      asm("__ZNKSbIwSt11char_traitsIwESaIwEE7compareERKS2_");
GccWStr        Sw_substr(void* self, unsigned pos, unsigned n)      asm("__ZNKSbIwSt11char_traitsIwESaIwEE6substrEmm");

/* ---- std::__detail::_List_node_base ops ---- */
void L_hook(void* self, void* pos)                asm("__ZNSt15_List_node_base4hookEPS_");
void L_unhook(void* self)                         asm("__ZNSt15_List_node_base6unhookEv");
void L_transfer(void* self, void* first, void* last) asm("__ZNSt15_List_node_base8transferEPS_S0_");

struct LNode { LNode* next; LNode* prev; int id; };   /* node_base {next,prev} + payload */

static void pr(const char* tag, const void* s) { printf("%s '%s' (%u)\n", tag, Ss_c_str(s), Ss_size(s)); }
static void prw(const char* tag, const void* s) {
    const wchar_t* w = Sw_c_str(s);
    printf("%s w[", tag);
    for (unsigned i = 0; w[i]; i++) printf("%s%d", i ? "," : "", (int)w[i]);
    printf("]\n");
}

int main() {
    char alloc;                       /* dummy const allocator& target */

    printf("== ctors ==\n");
    GccStr s0; Ss_ctor_default(&s0);              pr("default", &s0);
    GccStr s1; Ss_ctor_cstr(&s1, "hello", &alloc); pr("cstr", &s1);
    GccStr s2; Ss_ctor_copy(&s2, &s1);            pr("copy", &s2);

    printf("== COW append (s1 shared with s2) ==\n");
    Ss_append_cstr(&s1, " world");
    pr("s1", &s1); pr("s2", &s2);                 /* s2 must stay 'hello' */

    printf("== assign ==\n");
    GccStr a1; Ss_ctor_default(&a1);
    Ss_assign_op(&a1, &s1);   pr("op=", &a1);
    Ss_assign_cstr(&a1, "xyz"); pr("assign cstr", &a1);

    printf("== operator+ ==\n");
    GccStr foo; Ss_ctor_cstr(&foo, "foo", &alloc);
    GccStr bar; Ss_ctor_cstr(&bar, "bar", &alloc);
    GccStr fb = Ss_plus_ss(&foo, &bar);  pr("ss +", &fb);
    GccStr pf = Ss_plus_cs("pre-", &fb); pr("cs +", &pf);

    printf("== substr ==\n");
    GccStr w = Ss_substr(&s1, 6, 5);  pr("substr(6,5)", &w);
    GccStr h = Ss_substr(&s1, 0, 5);  pr("substr(0,5)", &h);
    GccStr t = Ss_substr(&s1, 6, 99); pr("substr(6,99)", &t);   /* n clamps */

    printf("== find/compare ==\n");
    printf("find 'o' @0    = %u\n", Ss_find_ch(&s1, 'o', 0));
    printf("find \"world\"   = %u\n", Ss_find_cstr(&s1, "world", 0));
    printf("rfind 'o'      = %u\n", Ss_rfind_ch(&s1, 'o', (unsigned)-1));
    printf("find 'z'       = %d\n", (int)Ss_find_ch(&s1, 'z', 0)); /* npos = -1 */
    printf("cmp abc<abd    = %d\n", Ss_compare_str(&foo, &bar) ? 1 : 0); /* foo>bar */
    { GccStr c1; Ss_ctor_cstr(&c1, "abc", &alloc);
      GccStr c2; Ss_ctor_cstr(&c2, "abc", &alloc);
      GccStr c3; Ss_ctor_cstr(&c3, "abd", &alloc);
      printf("cmp abc==abc   = %d\n", Ss_compare_str(&c1, &c2));
      printf("cmp abc<abd    = %d\n", Ss_compare_str(&c1, &c3) < 0 ? -1 : 1);
      Ss_dtor(&c1); Ss_dtor(&c2); Ss_dtor(&c3); }

    printf("== COW leak via begin ==\n");
    GccStr b1; Ss_ctor_cstr(&b1, "hello", &alloc);
    GccStr b2; Ss_ctor_copy(&b2, &b1);        /* shares b1's rep */
    char* it = Ss_begin(&b2);                 /* non-const begin -> _M_leak (deep copy) */
    *it = 'J';                                /* mutate b2's now-private buffer */
    pr("b1", &b1); pr("b2", &b2);             /* b1 stays 'hello', b2 'Jello' */

    printf("== operator[] ==\n");
    GccStr ix; Ss_ctor_cstr(&ix, "hello", &alloc);
    char* pc = Ss_index(&ix, 1);              /* leaks, returns &data[1] */
    *pc = 'a';
    pr("ix[1]='a'", &ix);                     /* 'hallo' */

    printf("== push_back/reserve/resize ==\n");
    GccStr pb; Ss_ctor_default(&pb);
    Ss_push_back(&pb, 'a'); Ss_push_back(&pb, 'b'); Ss_push_back(&pb, 'c');
    pr("push_back", &pb);
    Ss_reserve(&pb, 100);   pr("reserve(100)", &pb);
    Ss_resize(&pb, 5, 'Z'); pr("resize(5,Z)", &pb);
    Ss_resize(&pb, 2, '?'); pr("resize(2,?)", &pb);

    printf("== erase/replace/clear ==\n");
    GccStr er; Ss_ctor_cstr(&er, "hello world", &alloc);
    Ss_erase(&er, 5, 6);         pr("erase(5,6)", &er);   /* 'hello' */
    Ss_replace(&er, 0, 1, "J");  pr("replace(0,1,J)", &er); /* 'Jello' */
    Ss_replace(&er, 1, 0, "XY"); pr("replace(1,0,XY)", &er); /* 'JXYello' (insert) */
    Ss_clear(&er);               pr("clear", &er);

    printf("== wstring ==\n");
    GccWStr ws; Sw_ctor_cstr(&ws, L"hello", &alloc); prw("wcstr", &ws);
    Sw_append_cstr(&ws, L"!!"); prw("wappend", &ws);
    GccWStr wsub = Sw_substr(&ws, 1, 3); prw("wsubstr(1,3)", &wsub);
    GccWStr we; Sw_ctor_cstr(&we, L"hello!!", &alloc);
    printf("wcompare eq    = %d\n", Sw_compare_str(&ws, &we));

    printf("== list ==\n");
    LNode H = { &H, &H, -1 };                  /* empty ring header */
    LNode A = { 0, 0, 1 }, B = { 0, 0, 2 }, C = { 0, 0, 3 };
    L_hook(&A, &H); L_hook(&B, &H); L_hook(&C, &H);  /* append A,B,C before header */
    printf("hooked:");
    for (LNode* n = H.next; n != &H; n = n->next) printf(" %d", n->id);
    printf("\n");
    L_unhook(&B);                              /* remove B */
    printf("unhook B:");
    for (LNode* n = H.next; n != &H; n = n->next) printf(" %d", n->id);
    printf("\n");
    LNode H2 = { &H2, &H2, -2 };               /* second ring */
    LNode D = { 0, 0, 4 }, E = { 0, 0, 5 };
    L_hook(&D, &H2); L_hook(&E, &H2);
    L_transfer(&H, &D, &H2);                    /* splice [D,H2) before H (append to list1) */
    printf("transfer:");
    for (LNode* n = H.next; n != &H; n = n->next) printf(" %d", n->id);
    printf("\n");

    /* teardown (dispose the reps we still own) */
    Ss_dtor(&s0); Ss_dtor(&s1); Ss_dtor(&s2); Ss_dtor(&a1);
    Ss_dtor(&foo); Ss_dtor(&bar); Ss_dtor(&fb); Ss_dtor(&pf);
    Ss_dtor(&w); Ss_dtor(&h); Ss_dtor(&t);
    Ss_dtor(&b1); Ss_dtor(&b2); Ss_dtor(&ix); Ss_dtor(&pb); Ss_dtor(&er);
    Sw_dtor(&ws); Sw_dtor(&wsub); Sw_dtor(&we);

    printf("done\n");
    /* No fflush(stdout): the i386 `stdout` (__stdoutp) is a native >4GB FILE*
     * the translated caller would truncate to 4 bytes.  Native exit(0) flushes
     * stdio for us (same as every other cpp test). */
    exit(0);   /* the 86x64.sh wrapper enters _main via jmp: no return frame */
}
