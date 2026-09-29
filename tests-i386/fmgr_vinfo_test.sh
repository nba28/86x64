#!/bin/bash
#
# Regression test for the REAL PBHGetVInfoSync free-space report
# (src/abiconv/fmgr_shim.c shim_PBHGetVInfoSync; Halo CE false "Free up some
# space on your local disk" abort, 2026-07-06).
#
# Halo's startup disk check fills a pack(2) HVolumeParam (memset 0x7A),
# calls PBHGetVInfoSync, and computes
#     free_bytes = ioVFrBlk(u16@+62) * ioVAlBlkSiz(u32@+48)     (32-bit imull)
# requiring > 0xF9FFFFF (250MB). The old shim was a no-op returning nsvErr,
# so EVERY classic free-space check failed. The real shim reports the boot
# volume from statfs("/") with the documented classic 2GB pinning (block size
# scaled so the 16-bit counts stay <= 0x7FFF and count*blocksize products
# never overflow a caller's 32-bit multiply).
#
# Self-contained: compiles fmgr_shim.c as x86_64 (the arch the shim really
# runs as, under Rosetta — arm64 SIGKILLs a shrunken-__PAGEZERO binary) with
# -pagezero_size 0x1000 so a low-4GB param block can be mapped — the shim
# reads the pb address from a 32-bit i386 arg slot.
set -u

PROJ_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$PROJ_ROOT/src/abiconv/fmgr_shim.c"
[ -f "$SRC" ] || { echo "SKIP fmgr-vinfo (no $SRC)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/t.c" <<'EOF'
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/mount.h>

uint32_t shim_PBHGetVInfoSync(uint32_t *args);

int main(void) {
    /* low-4GB block: the shim reads pb (and ioNamePtr) from 32-bit slots */
    uint8_t *pb = mmap((void *)0x20000000, 0x1000, PROT_READ | PROT_WRITE,
                       MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (pb == MAP_FAILED || (uintptr_t)pb >= 0x100000000ULL) {
        printf("SKIP fmgr-vinfo (no low-4GB mapping)\n");
        return 0;
    }
    memset(pb, 0, 0x7A);                       /* Halo's exact memset */
    uint8_t *name = pb + 0x100;
    *(uint32_t *)(pb + 18) = (uint32_t)(uintptr_t)name;  /* ioNamePtr */
    *(int16_t *)(pb + 22) = 0;                 /* ioVRefNum: default volume */
    *(int16_t *)(pb + 28) = 0;                 /* ioVolIndex: use ioVRefNum */
    uint32_t args[1] = { (uint32_t)(uintptr_t)pb };

    uint32_t r = shim_PBHGetVInfoSync(args);
    int16_t  iores  = *(int16_t  *)(pb + 16);
    uint16_t nmblks = *(uint16_t *)(pb + 46);
    uint32_t blksz  = *(uint32_t *)(pb + 48);
    uint16_t frblk  = *(uint16_t *)(pb + 62);
    uint32_t freeprod = (uint32_t)frblk * blksz;   /* Halo's 32-bit multiply */
    uint32_t totprod  = (uint32_t)nmblks * blksz;

    if ((int32_t)r != 0 || iores != 0) {
        printf("FAIL fmgr-vinfo (r=%d ioResult=%d)\n", (int32_t)r, iores);
        return 1;
    }
    if (blksz == 0 || nmblks == 0) {
        printf("FAIL fmgr-vinfo (zero total: nmblks=%u blksz=%u)\n", nmblks, blksz);
        return 1;
    }
    if (nmblks > 0x7FFF || frblk > 0x7FFF) {
        printf("FAIL fmgr-vinfo (15-bit pin violated: nmblks=%u frblk=%u)\n",
               nmblks, frblk);
        return 1;
    }
    if (frblk > nmblks) {
        printf("FAIL fmgr-vinfo (free > total)\n");
        return 1;
    }
    if (name[0] == 0 || name[0] > 27) {
        printf("FAIL fmgr-vinfo (volume name len=%u)\n", name[0]);
        return 1;
    }
    /* the reported free space must track reality: > 250MB (Halo's threshold)
     * whenever the disk really has that much (pinning only LOWERS it to 2GB) */
    struct statfs sf;
    if (statfs("/", &sf) == 0) {
        uint64_t realfree = (uint64_t)sf.f_bavail * (uint64_t)sf.f_bsize;
        if (realfree > 0x12C00000ULL /*300MB*/ && freeprod <= 0xF9FFFFFu) {
            printf("FAIL fmgr-vinfo (disk has %llu free but shim reports %u)\n",
                   (unsigned long long)realfree, freeprod);
            return 1;
        }
        if ((uint64_t)freeprod > realfree) {
            printf("FAIL fmgr-vinfo (reports MORE than real free: %u > %llu)\n",
                   freeprod, (unsigned long long)realfree);
            return 1;
        }
    }
    /* single-volume model: index enumeration must terminate at 1 */
    *(int16_t *)(pb + 28) = 2;
    r = shim_PBHGetVInfoSync(args);
    if ((int32_t)r != -35) {
        printf("FAIL fmgr-vinfo (volIndex=2 returned %d, want nsvErr -35)\n",
               (int32_t)r);
        return 1;
    }
    printf("PASS fmgr-vinfo (free=%u bytes = %u x %u, total=%u, name_len=%u)\n",
           freeprod, frblk, blksz, totprod, name[0]);
    return 0;
}
EOF

cc -arch x86_64 -o "$TMP/t" "$TMP/t.c" "$SRC" "$PROJ_ROOT/src/abiconv/gap.c" "$PROJ_ROOT/src/abiconv/dyld_image_list.c" "$PROJ_ROOT/build/src/abiconv/CMakeFiles/abiconv.dir/gap_tramp.asm.o" -Wl,-pagezero_size,0x1000 \
   2> "$TMP/cc.log" \
   || { echo "FAIL fmgr-vinfo (cc error)"; sed 's/^/    /' "$TMP/cc.log" | head -10; exit 1; }

"$TMP/t"
exit $?
