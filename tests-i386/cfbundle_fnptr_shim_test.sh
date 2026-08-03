#!/bin/bash
#
# cfbundle-fnptr-shim — regression guard for
#   src/abiconv/posix_shim.c: shim_CFBundleGetFunctionPointerForName
#
# THE BLOCKER, reproduced (Halo CE 2.0.4, SIGSEGV 2026-08-03 11:12:10):
#   Halo has NO `_fopen` import at all — verified with `nm -u` on BOTH the
#   translated Halo.dylib and the pristine i386 original. It resolves fopen
#   at RUN TIME, CFBundle-as-dynamic-loader style (i386 0x29e72e):
#       g_fopen = CFBundleGetFunctionPointerForName(bundle, CFSTR("fopen"));
#       ...
#       calll *g_fopen                      /* i386 cdecl, 32-bit result */
#   and later hands the result to its IMPORTED `_ftell`.
#
#   fnptr_lookup_result minted a generic marshalling thunk for the RAW NATIVE
#   fopen. The thunk lifts the i386 cdecl frame to the x86_64 ABI correctly, so
#   the call works — but it returns what the native callee returned IN %eax,
#   i.e. TRUNCATED TO 32 BITS. Native fopen returns a >4GB FILE*, so Halo stored
#   `(uint32_t)(uintptr_t)fp` and the next
#       ___ftell.l1 -> ftell -> resolve_file -> is_shim
#   dereferenced fp+8 (the SHIM_FILE_MAGIC probe) on an address in NO VM REGION:
#       EXC_BAD_ACCESS KERN_INVALID_ADDRESS at 0x59d56d88, rip = _is_shim+0x4e.
#
#   libabiconv's own interpose (`_fopen` in file_shim.c) exists precisely to
#   prevent that: it wraps the real FILE* in a low-4GB `struct shim_FILE` so the
#   4-byte i386 slot holds it losslessly. shim_dlsym and ns_symbol_resolve
#   already prefer that interpose shim over a raw thunk; this CF twin was the
#   one member of the by-name-lookup family that never got the step.
#
# THE FIX (universal, structural — triggers on "libabiconv exports an interpose
# shim for this name", never on an app): shim_CFBundleGetFunctionPointerForName
# consults dlsym_shim_for(name) first and returns the interpose shim (nlist
# `___<name>`). Kill switch M64_NO_CFBUNDLE_FNPTR_SHIM=1.
#
# This guard runs BOTH ARMS and fails if they do not differ, so it cannot pass
# inertly. Native x86_64; no i386 sysroot needed.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ_ROOT="$(cd "$HERE/.." && pwd)"
LIBABICONV="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
SRC="$PROJ_ROOT/src/abiconv/posix_shim.c"

[ -f "$SRC" ] || { echo "SKIP cfbundle-fnptr-shim (no $SRC)"; exit 0; }
[ -f "$LIBABICONV" ] || { echo "SKIP cfbundle-fnptr-shim (libabiconv not built)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.c" <<'EOF'
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <CoreFoundation/CoreFoundation.h>

/* file_shim.c's low-4GB FILE wrapper: { void *real_fp; uint32_t magic; int fd; } */
#define SHIM_FILE_MAGIC 0x68690a55u

/* libabiconv takes its low-4GB allocator from the translated WRAPPER EXECUTABLE
 * (src/86x64/wrapper_setup.c) as a WEAK UNDEFINED symbol. Stand in for the
 * wrapper here: without it wrap_file takes its out-of-low-memory fallback and
 * the guard would measure the fallback instead of the fix. */
void *_86x64_alloc_low_4gb(size_t n) {
    static char *cur, *end;
    if (!cur) {
        for (uintptr_t a = 0x30000000; a < 0x40000000; a += 0x20000) {
            void *p = mmap((void *)a, 0x10000, PROT_READ | PROT_WRITE,
                           MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
            if (p != MAP_FAILED && (uintptr_t)p < 0x100000000ULL) {
                cur = p; end = cur + 0x10000; break;
            }
        }
        if (!cur) return NULL;
    }
    n = (n + 15) & ~(size_t)15;
    if (cur + n > end) return NULL;
    void *r = cur; cur += n; return r;
}

static int fails;
static void fail(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fputs("FAIL cfbundle-fnptr-shim: ", stdout); vprintf(fmt, ap); putchar('\n');
    va_end(ap); fails++;
}

int main(int argc, char **argv) {
    int legacy = (argc > 1 && strcmp(argv[1], "legacy") == 0);
    const char *libpath = getenv("LIBABICONV");
    if (!libpath) { printf("SKIP cfbundle-fnptr-shim (LIBABICONV unset)\n"); return 0; }

    /* RTLD_LAZY, not RTLD_NOW: libabiconv legitimately carries flat-namespace
     * references to APIs that only exist in the translated environment (e.g.
     * _ATSFontActivateFromFileSpecification); eager binding would refuse the
     * load. Nothing on this test's path calls them. */
    void *h = dlopen(libpath, RTLD_LAZY | RTLD_LOCAL);
    if (!h) { printf("SKIP cfbundle-fnptr-shim (dlopen: %s)\n", dlerror()); return 0; }

    int32_t  (*cf_shim)(uint32_t *)  = dlsym(h, "shim_CFBundleGetFunctionPointerForName");
    uint32_t (*objc_wrap)(uint64_t)  = dlsym(h, "x64_objc_wrap");
    void    *abi_fopen_bridge        = dlsym(h, "__fopen");   /* nlist ___fopen */
    FILE *(*abi_fopen)(const char *, const char *) = dlsym(h, "fopen");
    long  (*abi_ftell)(FILE *)       = dlsym(h, "ftell");
    int   (*abi_fclose)(FILE *)      = dlsym(h, "fclose");

    if (!cf_shim || !objc_wrap) {
        printf("SKIP cfbundle-fnptr-shim (libabiconv lacks the shim entry points)\n");
        return 0;
    }
    if (!abi_fopen_bridge) {
        fail("libabiconv exports no ___fopen legacy bridge — the interpose shim "
             "this fix routes to does not exist");
        return 1;
    }
    if ((uintptr_t)abi_fopen_bridge >= 0x100000000ULL) {
        printf("SKIP cfbundle-fnptr-shim (libabiconv not loaded in the low 4GB)\n");
        return 0;
    }

    /* ---------------------------------------------------------------------
     * Halo's exact call: CFBundleGetFunctionPointerForName(bundle, "fopen").
     * Both object args go in as i386 32-bit slots, wrapped the way the
     * translated side wraps a >4GB object (x64_objc_wrap -> arena handle).
     * ------------------------------------------------------------------ */
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFStringRef name   = CFSTR("fopen");
    uint32_t a[2] = { objc_wrap((uint64_t)(uintptr_t)bundle),
                      objc_wrap((uint64_t)(uintptr_t)name) };
    uint32_t ret = (uint32_t)cf_shim(a);

    if (!legacy) {
        if (ret != (uint32_t)(uintptr_t)abi_fopen_bridge) {
            fail("fixed arm: returned 0x%x, want libabiconv's ___fopen interpose "
                 "bridge 0x%x (a raw thunk truncates fopen's >4GB FILE*)",
                 ret, (uint32_t)(uintptr_t)abi_fopen_bridge);
        }
        /* ★ END TO END: the handle ___fopen produces must survive a 32-bit slot
         * AND be recognised by the resolve_file/is_shim path Halo's ___ftell
         * goes through. That is the invariant whose absence killed Halo. */
        if (abi_fopen && abi_ftell) {
            FILE *f = abi_fopen("/etc/hosts", "r");
            if (!f) { fail("libabiconv fopen(/etc/hosts) returned NULL"); }
            else {
                if ((uintptr_t)f >= 0x100000000ULL) {
                    fail("libabiconv fopen returned %p — >4GB, would not survive "
                         "the i386 4-byte slot", (void *)f);
                }
                if (*(uint32_t *)((char *)f + 8) != SHIM_FILE_MAGIC) {
                    fail("libabiconv fopen result is not a shim_FILE "
                         "(magic 0x%x) — is_shim would reject it",
                         *(uint32_t *)((char *)f + 8));
                }
                /* the literal call Halo made and crashed on */
                if (abi_ftell(f) < 0) { fail("libabiconv ftell(shim FILE*) failed"); }
                if (abi_fclose) { abi_fclose(f); }
            }
        }
    } else {
        if (ret == (uint32_t)(uintptr_t)abi_fopen_bridge) {
            fail("legacy arm: still returned the interpose bridge 0x%x — the "
                 "M64_NO_CFBUNDLE_FNPTR_SHIM kill switch has no effect", ret);
        }
        /* Demonstrate the defect the fix removes: the raw path forwards to the
         * NATIVE fopen, whose FILE* does not fit in the i386 4-byte slot. */
        FILE *nf = fopen("/etc/hosts", "r");
        if (!nf) { fail("legacy arm: native fopen(/etc/hosts) failed"); }
        else {
            if ((uintptr_t)nf < 0x100000000ULL) {
                printf("  note: native FILE* %p happens to be low-4GB on this run; "
                       "the truncation defect is latent, not absent\n", (void *)nf);
            }
            fclose(nf);
        }
    }

    if (fails) { return 1; }
    printf("PASS cfbundle-fnptr-shim (%s arm: %s)\n",
           legacy ? "legacy" : "fixed",
           legacy ? "interpose shim NOT returned — the raw lookup path is back "
                    "(defect reproduces)"
                  : "CFBundleGetFunctionPointerForName(\"fopen\") -> ___fopen, "
                    "result is a low-4GB shim FILE* that ftell accepts");
    return 0;
}
EOF

sed -i '' 's/#include <stdio.h>/#include <stdarg.h>\n#include <stdio.h>/' "$TMP/t.c"

# -pagezero_size 0x1000: libabiconv's constructors mint their low-4GB arenas at
# load time; with the default 4GB __PAGEZERO there is no low memory to map and
# the dlopen faults in the constructor.
cc -arch x86_64 -o "$TMP/t" "$TMP/t.c" -framework CoreFoundation \
   -Wl,-pagezero_size,0x1000 -Wl,-export_dynamic 2> "$TMP/cc.log" \
   || { echo "FAIL cfbundle-fnptr-shim (cc error)"; sed 's/^/    /' "$TMP/cc.log" | head -20; exit 1; }

rc=0
export LIBABICONV
out_fixed="$("$TMP/t" fixed 2>&1)";  [ $? -eq 0 ] || rc=1
echo "$out_fixed" | sed 's/^/  /'
case "$out_fixed" in SKIP*) echo "$out_fixed"; exit 0;; esac

out_legacy="$(M64_NO_CFBUNDLE_FNPTR_SHIM=1 "$TMP/t" legacy 2>&1)"; [ $? -eq 0 ] || rc=1
echo "$out_legacy" | sed 's/^/  /'

# the two arms MUST differ — otherwise the guard is inert
if [ "$out_fixed" = "$out_legacy" ]; then
    echo "FAIL cfbundle-fnptr-shim (both arms identical — kill switch has no effect)"
    rc=1
fi

# static wiring: the shim, the bridge it routes to, and the kill switch must all
# be present in the BUILT libabiconv (not just in the source tree).
nm -g "$LIBABICONV" 2>/dev/null | grep -q " T _shim_CFBundleGetFunctionPointerForName\$" || {
    echo "FAIL cfbundle-fnptr-shim (built libabiconv exports no shim_CFBundleGetFunctionPointerForName)"; rc=1; }
nm -g "$LIBABICONV" 2>/dev/null | grep -q " T ___fopen\$" || {
    echo "FAIL cfbundle-fnptr-shim (built libabiconv exports no ___fopen interpose bridge)"; rc=1; }
strings -a "$LIBABICONV" 2>/dev/null | grep -q "M64_NO_CFBUNDLE_FNPTR_SHIM" || {
    echo "FAIL cfbundle-fnptr-shim (kill-switch string absent from the built libabiconv)"; rc=1; }

[ $rc -eq 0 ] && echo "PASS cfbundle-fnptr-shim"
exit $rc
