#!/bin/bash
#
# Regression test for the REAL FSSpec resolution + the output-buffer invariant
# (src/abiconv/carbon_fsspec_shim.c: shim_FSpMakeFSRef / shim_FSMakeFSSpec /
# shim_FSRefMakePath).
#
# THE BLOCKER, reproduced (Halo Graphics-Settings-OK SIGSEGV, 2026-08-02):
#   Halo i386 0x2da50c does, with every OSErr IGNORED —
#       FSpMakeFSRef(&spec, &fsref)        fsref lives on ITS OWN STACK
#       FSRefMakePath(&fsref, buf, 0x400)  buf   lives on ITS OWN STACK
#       CFStringCreateWithCString(NULL, buf, kCFStringEncodingUTF8)
#       CFStringGetCString(<that>, buf, 0x800, kCFStringEncodingWindowsLatin1)
#   The old one-line shim `return fnfErr;` never wrote *newRef, so fsref stayed
#   uninitialised stack garbage; FSRefMakePath then failed and (Apple documents
#   `path` as UNDEFINED on error) left buf as stack garbage too;
#   CFStringCreateWithCString legitimately returns NULL for bytes that are not
#   valid UTF-8; and the unchecked NULL reached CFStringGetCString ->
#   EXC_BAD_ACCESS at 0x0. Because it depends on the stack bytes below rsp it is
#   a heisenbug: any observer (lldb, or our own [callsite] trace) perturbs the
#   garbage into valid UTF-8 and the crash vanishes.
#
# The fix is two independent, structural things and this test guards both:
#   (1) An FSSpec is RESOLVABLE again, on both branches: a classic directory ID
#       IS the catalog node id = inode, addressable as /.vol/<st_dev>/<CNID>; and
#       for the ids modern CarbonCore's FindFolder invents (small internal tokens
#       no surviving API can consume) we own the producer too — shim_FindFolder
#       delegates to the native FSRef-based FSFindFolder and mints a dirID our
#       consumer half can resolve. Asserted as HALO'S EXACT CHAIN,
#       FindFolder -> FSpMakeFSRef -> FSRefMakePath == the native folder path.
#   (2) A FAILING shim still leaves every caller-supplied output buffer DEFINED,
#       so an unchecked-OSErr caller can never read stack garbage.
#
# Kill switch ABICONV_FSSPEC_LEGACY=1 restores the historical stub behaviour; the
# test runs both arms and requires them to DIFFER, so the guard cannot pass inertly.
#
# Self-contained: compiles carbon_fsspec_shim.c as x86_64 (the arch it really runs
# as under Rosetta) with -pagezero_size 0x1000 so the FSSpec/FSRef/path buffers can
# be mapped in the low 4GB — the shims read their addresses from 32-bit i386 slots.
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$PROJ_ROOT/src/abiconv/carbon_fsspec_shim.c"
ASM="$PROJ_ROOT/src/abiconv/maptable_tramp.asm"
[ -f "$SRC" ] || { echo "SKIP fsspec-resolve (no $SRC)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.c" <<'EOF'
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <CoreServices/CoreServices.h>

uint32_t shim_FSpMakeFSRef(uint32_t *args);
uint32_t shim_FSMakeFSSpec(uint32_t *args);
uint32_t shim_FSRefMakePath(uint32_t *args);
uint32_t shim_FindFolder(uint32_t *args);

#define FSREF_SZ  80
#define FSSPEC_SZ 70
#define POISON    0xFF          /* not valid UTF-8 anywhere in a sequence */

static uint8_t *lowmem;         /* low-4GB scratch: shims take 32-bit addresses */
static int fails;

static void fail(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fputs("FAIL fsspec-resolve: ", stdout); vprintf(fmt, ap); putchar('\n');
    va_end(ap); fails++;
}

/* build an FSSpec at `spec` from (vRefNum, dirID, optional C leaf name) */
static void mkspec(uint8_t *spec, int16_t vref, int32_t dirid, const char *leaf) {
    memset(spec, 0, FSSPEC_SZ);
    memcpy(spec, &vref, 2);
    memcpy(spec + 2, &dirid, 4);
    if (leaf && *leaf) {
        size_t n = strlen(leaf); if (n > 63) n = 63;
        spec[6] = (uint8_t)n; memcpy(spec + 7, leaf, n);
    }
}

int main(int argc, char **argv) {
    int legacy = (argc > 1 && strcmp(argv[1], "legacy") == 0);

    lowmem = mmap((void *)0x20000000, 0x10000, PROT_READ | PROT_WRITE,
                  MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (lowmem == MAP_FAILED || (uintptr_t)lowmem >= 0x100000000ULL) {
        printf("SKIP fsspec-resolve (no low-4GB mapping)\n");
        return 0;
    }
    uint8_t *spec = lowmem;              /* 0x00.. */
    uint8_t *ref  = lowmem + 0x100;      /* 0x100.. */
    uint8_t *path = lowmem + 0x400;      /* 0x400.. */
    const uint32_t PATHMAX = 0x800;

    /* ---- ground truth via the PRODUCER half: FindFolder, exactly as Halo does ---- */
    int16_t *pv = (int16_t *)(lowmem + 0x3000);
    int32_t *pd = (int32_t *)(lowmem + 0x3010);
    *pv = 0x5A5A; *pd = 0x5A5A5A5A;
    uint32_t ff[5] = { (uint32_t)(int32_t)-32763 /*kUserDomain*/,
                       0x61737570u /*'asup'*/, 0 /*kDontCreateFolder*/,
                       (uint32_t)(uintptr_t)pv, (uint32_t)(uintptr_t)pd };
    int32_t ffe = (int32_t)shim_FindFolder(ff);

    FSRef  natref;
    UInt8  natpath[1024];
    natpath[0] = 0;
    if (FSFindFolder(kUserDomain, kApplicationSupportFolderType,
                     kDontCreateFolder, &natref) == noErr) {
        FSRefMakePath(&natref, natpath, sizeof natpath);
    }
    if (natpath[0] == 0) {
        printf("SKIP fsspec-resolve (no native Application Support folder)\n"); return 0;
    }

    if (!legacy) {
        if (ffe != 0) { fail("FindFolder returned %d, want noErr", ffe); }
        if (*pd == 0x5A5A5A5A) { fail("FindFolder left foundDirID undefined"); }
    }

    /* =========================================================================
     * (1) ★HALO'S EXACT CHAIN — FindFolder -> FSpMakeFSRef -> FSRefMakePath
     *     must land back on the same directory the native FSFindFolder names.
     * ====================================================================== */
    mkspec(spec, *pv, *pd, NULL);
    memset(ref, POISON, FSREF_SZ);
    uint32_t a[4] = { (uint32_t)(uintptr_t)spec, (uint32_t)(uintptr_t)ref, 0, 0 };
    int32_t e = (int32_t)shim_FSpMakeFSRef(a);

    if (!legacy) {
        if (e != 0) { fail("FSpMakeFSRef(FindFolder spec) returned %d, want noErr", e); }
        else {
            memset(path, POISON, PATHMAX);
            uint32_t b[3] = { (uint32_t)(uintptr_t)ref, (uint32_t)(uintptr_t)path, PATHMAX };
            int32_t s = (int32_t)shim_FSRefMakePath(b);
            if (s != 0) { fail("FSRefMakePath returned %d", s); }
            else if (strcmp((char *)path, (char *)natpath) != 0) {
                fail("round trip: got '%s', want '%s'", (char *)path, (char *)natpath);
            }
        }
    } else {
        /* KILL SWITCH: the historical stub — fnfErr and the FSRef left untouched. */
        if (e != -43) { fail("legacy: FSpMakeFSRef returned %d, want fnfErr(-43)", e); }
        if (ref[0] != POISON) {
            fail("legacy: FSRef was written (0x%02x) - kill switch is not in effect",
                 ref[0]);
        }
    }

    /* =========================================================================
     * (2) THE OTHER RESOLUTION BRANCH — a REAL catalog node id (= inode), the
     *     form a dirID takes when it comes out of an app's own settings file,
     *     plus a named leaf and the FSMakeFSSpec always-fill contract.
     * ====================================================================== */
    if (!legacy) {
        struct stat ts;
        if (stat("/private/tmp", &ts) == 0 && (uint64_t)ts.st_ino <= 0x7FFFFFFFULL) {
            char tmpl[] = "/private/tmp/fsspec_resolve_XXXXXX";
            int fd = mkstemp(tmpl);
            if (fd >= 0) {
                close(fd);
                const char *leaf = strrchr(tmpl, '/') + 1;
                mkspec(spec, (int16_t)-100, (int32_t)ts.st_ino, leaf);
                memset(ref, POISON, FSREF_SZ);
                int32_t e2 = (int32_t)shim_FSpMakeFSRef(a);
                if (e2 != 0) { fail("CNID branch: FSpMakeFSRef returned %d, want noErr", e2); }
                else {
                    memset(path, POISON, PATHMAX);
                    uint32_t b[3] = { (uint32_t)(uintptr_t)ref,
                                      (uint32_t)(uintptr_t)path, PATHMAX };
                    if ((int32_t)shim_FSRefMakePath(b) != 0 ||
                        strcmp((char *)path, tmpl) != 0) {
                        fail("CNID branch: got '%s', want '%s'", (char *)path, tmpl);
                    }
                }
                /* FSMakeFSSpec must ALWAYS fill the spec (classic contract: a valid
                 * spec even for a file that does not exist yet). */
                uint8_t pn[64]; size_t ln = strlen(leaf); if (ln > 63) ln = 63;
                pn[0] = (uint8_t)ln; memcpy(pn + 1, leaf, ln);
                memcpy(lowmem + 0x2000, pn, ln + 1);
                memset(spec, POISON, FSSPEC_SZ);
                uint32_t c[4] = { (uint32_t)(uint16_t)(int16_t)-100, (uint32_t)ts.st_ino,
                                  (uint32_t)(uintptr_t)(lowmem + 0x2000),
                                  (uint32_t)(uintptr_t)spec };
                int32_t e3 = (int32_t)shim_FSMakeFSSpec(c);
                int16_t gv; int32_t gd; memcpy(&gv, spec, 2); memcpy(&gd, spec + 2, 4);
                if (e3 != 0) { fail("FSMakeFSSpec returned %d for an existing file", e3); }
                if (gv != (int16_t)-100 || gd != (int32_t)ts.st_ino ||
                    spec[6] != (uint8_t)ln) {
                    fail("FSMakeFSSpec left the spec undefined (vref=%d dir=%d len=%u)",
                         gv, gd, spec[6]);
                }
                unlink(tmpl);
            }
        }
    }

    /* =========================================================================
     * (3) ★THE BLOCKER ITSELF — an unchecked-OSErr caller must never end up
     *     with an undefined path buffer. Exactly Halo's 0x2da50c sequence.
     * ====================================================================== */
    mkspec(spec, (int16_t)0x7FFE, (int32_t)0x7FFFFFF0, NULL);   /* cannot resolve */
    memset(ref, POISON, FSREF_SZ);
    memset(path, POISON, PATHMAX);
    (void)shim_FSpMakeFSRef(a);                       /* OSErr IGNORED, like Halo */
    uint32_t b2[3] = { (uint32_t)(uintptr_t)ref, (uint32_t)(uintptr_t)path, PATHMAX };
    (void)shim_FSRefMakePath(b2);                     /* OSStatus IGNORED, like Halo */
    CFStringRef cf = CFStringCreateWithCString(NULL, (const char *)path,
                                               kCFStringEncodingUTF8);
    if (!legacy) {
        if (path[0] != '\0') {
            fail("unchecked path buffer left undefined (path[0]=0x%02x)", path[0]);
        }
        if (cf == NULL) {
            fail("CFStringCreateWithCString(NULL, path, UTF8) returned NULL — "
                 "this is the nil CFStringRef Halo dereferences");
        }
    } else {
        if (path[0] == '\0' || cf != NULL) {
            fail("legacy: the defect did NOT reproduce (path[0]=0x%02x cf=%p) — "
                 "the kill switch is not disabling the guard", path[0], (void *)cf);
        }
    }
    if (cf) { CFRelease(cf); }

    if (fails) { return 1; }
    printf("PASS fsspec-resolve (%s arm: %s)\n", legacy ? "legacy" : "fixed",
           legacy ? "historical stub behaviour confirmed (defect reproduces)"
                  : "FSSpec resolves + no undefined output buffer");
    return 0;
}
EOF

# the fail() helper needs stdarg
sed -i '' 's/#include <stdint.h>/#include <stdarg.h>\n#include <stdint.h>/' "$TMP/t.c"

cc -arch x86_64 -o "$TMP/t" "$TMP/t.c" "$SRC" -Wl,-pagezero_size,0x1000 \
   -framework CoreServices -Wno-deprecated-declarations 2> "$TMP/cc.log" \
   || { echo "FAIL fsspec-resolve (cc error)"; sed 's/^/    /' "$TMP/cc.log" | head -20; exit 1; }

rc=0
out_fixed="$("$TMP/t" fixed 2>&1)";  [ $? -eq 0 ] || rc=1
echo "$out_fixed" | sed 's/^/  /'
case "$out_fixed" in SKIP*) echo "$out_fixed"; exit 0;; esac

out_legacy="$(ABICONV_FSSPEC_LEGACY=1 "$TMP/t" legacy 2>&1)"; [ $? -eq 0 ] || rc=1
echo "$out_legacy" | sed 's/^/  /'

# the two arms MUST differ — otherwise the guard is inert
if [ "$out_fixed" = "$out_legacy" ]; then
    echo "FAIL fsspec-resolve (both arms identical — kill switch has no effect)"
    rc=1
fi

# the shim must stay wired: MTSHIM entries + a built libabiconv that exports them
if [ -f "$ASM" ]; then
    for s in ___FSpMakeFSRef ___FSMakeFSSpec ___FSRefMakePath; do
        grep -q "MTSHIM[[:space:]]*$s," "$ASM" || {
            echo "FAIL fsspec-resolve ($s not MTSHIM-wired in maptable_tramp.asm)"; rc=1; }
    done
fi
LIB="$PROJ_ROOT/build/src/abiconv/libabiconv.dylib"
if [ -f "$LIB" ]; then
    for s in ___FSpMakeFSRef ___FSMakeFSSpec ___FSRefMakePath; do
        nm -g "$LIB" 2>/dev/null | grep -q " T $s\$" || {
            echo "FAIL fsspec-resolve ($s not exported by the built libabiconv)"; rc=1; }
    done
    # abigen must NOT also emit its own FSRefMakePath shim (duplicate-symbol guard)
    n=$(nm "$LIB" 2>/dev/null | grep -c "___FSRefMakePath$")
    [ "$n" = "1" ] || { echo "FAIL fsspec-resolve (___FSRefMakePath defined $n times)"; rc=1; }
fi

[ $rc -eq 0 ] && echo "PASS fsspec-resolve"
exit $rc
