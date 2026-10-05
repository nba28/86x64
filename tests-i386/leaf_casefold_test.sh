#!/bin/bash
# leaf_casefold_test.sh — A/B guard for the case-insensitive dependency lookup
# (src/abiconv/dyld_image_list.c x64_img_find_leaf). A load command names a
# dependency by install name; on a case-insensitive volume dyld loads the file
# whatever its case (Portal 2: libMilesX86.dylib -> libmilesx86.dylib), so the
# lookup process_deps uses to fix dependencies up BEFORE a dependent's
# initializers run must match case-insensitively too, or the Miles MP3 provider
# registers into un-relocated Miles and voice lines stay mute.
# Self-contained native x86_64 harness (no i386 translation).
# ON: "LIBSYSTEM.B.DYLIB" resolves to libSystem's image; OFF (M64_NO_LEAF_CASEFOLD=1) it does not.
set -u
PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cat > "$TMP/t.c" <<'C'
#include <stdint.h>
#include <stdio.h>
struct mach_header;
const struct mach_header *x64_img_find_leaf(const char *leaf, intptr_t *slide_out);
int main(void) {
    const struct mach_header *exact = x64_img_find_leaf("libSystem.B.dylib", 0);
    const struct mach_header *folded = x64_img_find_leaf("LIBSYSTEM.B.DYLIB", 0);
    printf("exact=%p folded=%p\n", (const void *)exact, (const void *)folded);
    return (exact && folded == exact) ? 42 : 1;
}
C
cc -arch x86_64 -o "$TMP/t" "$TMP/t.c" "$PROJ_ROOT/src/abiconv/gap.c" "$PROJ_ROOT/src/abiconv/dyld_image_list.c" \
   "$PROJ_ROOT/build/src/abiconv/CMakeFiles/abiconv.dir/gap_tramp.asm.o" -Wl,-pagezero_size,0x1000 2> "$TMP/cc.log" \
   || { echo "leaf-casefold: FAIL (cc error)"; sed 's/^/    /' "$TMP/cc.log" | head -10; exit 1; }
"$TMP/t" >/dev/null; on=$?
M64_NO_LEAF_CASEFOLD=1 "$TMP/t" >/dev/null; off=$?
if [ "$on" = 42 ] && [ "$off" != 42 ]; then echo "leaf-casefold: PASS (on=$on off=$off)"
else echo "leaf-casefold: FAIL (on=$on off=$off)"; exit 1; fi
