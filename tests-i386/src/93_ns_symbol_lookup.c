/* 93_ns_symbol_lookup.c — classic NeXT dyld NSSymbol API must be shimmed.
 *
 * NSIsSymbolNameDefined / NSLookupAndBindSymbol / NSAddressOfSymbol are the
 * pre-dlopen dynamic-loader API old Mac ports use as their dlsym: build
 * "_"+name into a stack buffer, probe, bind, take the address, call it.
 * abigen can't shim them (modern <mach-o/dyld.h> marks them unavailable), so
 * before the posix_shim.c hand shims the translated `call NSIsSymbolNameDefined`
 * reached NATIVE libdyld: it read its arg from %rdi (garbage — the i386
 * caller put it on the stack) and returned with a 64-bit `ret` that
 * over-popped the i386 4-byte return slot, fusing the adjacent slot (arg0,
 * the "_"+name stack buffer pointer) into the popped PC's high half.
 * Civ IV (Steam), deterministic, first probe right after embedded Python
 * loads: EXC_BAD_ACCESS at 0x877ffaf0`09babb13 = &stackbuf<<32 | retaddr.
 * Same latent defect: iPhoto/iWeb (NSLookupSymbolInImage), dbRepair (trio).
 *
 * Legs: the exact Civ probe/bind/address/call idiom on atoi (resolves
 * shim-first to libabiconv's ___atoi, so the returned pointer is directly
 * i386-callable), NSSymbol dedup across repeated binds, NSNameOfSymbol
 * round-trip, and an undefined-name probe that must answer 0 without
 * faulting.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* extern decls, not <mach-o/dyld.h>: the modern host SDK hides these behind
 * availability macros and the classic prototypes are ABI-stable. */
extern int NSIsSymbolNameDefined(const char *symbolName);
extern void *NSLookupAndBindSymbol(const char *symbolName);
extern void *NSAddressOfSymbol(void *symbol);
extern const char *NSNameOfSymbol(void *symbol);

typedef int (*atoi_fn)(const char *);

/* The classic idiom, byte-for-byte what Civ IV's wrapper does. */
static void *lookup(const char *name) {
    char buf[1024];
    buf[0] = '_';
    memset(buf + 1, 0, sizeof buf - 1);
    strcat(buf, name);
    if (!NSIsSymbolNameDefined(buf)) { return 0; }
    void *sym = NSLookupAndBindSymbol(buf);
    if (!sym) { return 0; }
    return NSAddressOfSymbol(sym);
}

int main(void) {
    atoi_fn f = (atoi_fn)lookup("atoi");
    if (!f) {
        printf("atoi: lookup failed\n");
        return 1;
    }
    printf("atoi(\"42\") = %d\n", f("42"));

    void *s1 = NSLookupAndBindSymbol("_atoi");
    void *s2 = NSLookupAndBindSymbol("_atoi");
    printf("dedup: %s\n", s1 == s2 ? "same" : "different");
    printf("name: %s\n", NSNameOfSymbol(s1));

    printf("bogus defined: %d\n",
           NSIsSymbolNameDefined("_no_such_symbol_86x64_xyzzy"));
    /* exit() rather than return: these crt-less test binaries enter at main
     * (LC_UNIXTHREAD eip=_main, no crt1.o in the SL sysroot), so [esp] holds
     * ARGC, not a return address — returning from main jumps to argc (rip=1)
     * on real i386 too. Suite-wide convention; real apps enter via crt1. */
    exit(0);
}
