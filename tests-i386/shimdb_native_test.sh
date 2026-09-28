#!/bin/bash
# shimdb-native: the shimdb/impl implementations called the way a NATIVE x86_64
# framework calls them through a shimgen shim (native ABI, native Handles). The
# i386 guards (carbon-datetime, fsspec-resolve, carbon-fileops, carbon-alias)
# cover libabiconv's glue into the same code; this covers the other consumer.
set -u
IMPL="$(cd "$(dirname "$0")/../src/86x64/shimdb/impl" && pwd)"
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
cat > "$T/t.c" <<'SRC'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <CoreServices/CoreServices.h>
typedef struct { int16_t y, mo, d, h, mi, s, dow; } DTR;
void SecondsToDate(uint32_t, DTR *);
void DateToSeconds(const DTR *, uint32_t *);
int16_t FSMakeFSSpec(int16_t, int32_t, const uint8_t *, uint8_t *);
int16_t FSpCreate(const uint8_t *, uint32_t, uint32_t, int16_t);
int16_t FSpDelete(const uint8_t *);
int16_t NewAlias(const uint8_t *, const uint8_t *, void ***);
int16_t ResolveAlias(const uint8_t *, void **, uint8_t *, uint8_t *);
int16_t FindFolderDirID(void);
int32_t cfs_find_folder(int16_t, uint32_t, uint8_t, int16_t *, int32_t *);
int cfs_path_to_fsspec(const char *, uint8_t *);
int cfs_fsspec_to_path(const uint8_t *, char *, size_t, int *);
#define CHECK(c, m) do { if (!(c)) { printf("FAIL %s\n", m); return 1; } } while (0)
int main(int argc, char **argv) {
    char dir[] = "/tmp/shimdb-native.XXXXXX", p[1024], q[1024], r[1024];
    uint8_t dspec[70], spec[70], moved[70];
    uint8_t name[] = "\x05" "a.txt", changed = 9;
    void **alias = NULL;
    DTR d; uint32_t s;
    CHECK(mkdtemp(dir), "mkdtemp");
    SecondsToDate(3786912000u, &d); DateToSeconds(&d, &s);
    CHECK(s == 3786912000u && d.y >= 2023, "SecondsToDate/DateToSeconds round trip");
    CHECK(cfs_path_to_fsspec(dir, dspec), "dir -> FSSpec");
    { int16_t v; int32_t id; memcpy(&v, dspec, 2);
      snprintf(p, sizeof p, "%s/a.txt", dir);
      struct stat st; stat(dir, &st); id = (int32_t)st.st_ino;
      CHECK(FSMakeFSSpec(v, id, name, spec) == -43, "FSMakeFSSpec fnfErr for a new file");
      CHECK(FSpCreate(spec, 'ttxt', 'TEXT', 0) == 0, "FSpCreate");
      CHECK(access(p, F_OK) == 0, "file exists after FSpCreate"); }
    CHECK(NewAlias(NULL, spec, &alias) == 0 && alias && *alias, "NewAlias -> native Handle");
    snprintf(q, sizeof q, "%s/b.txt", dir);
    CHECK(rename(p, q) == 0, "rename target");
    CHECK(ResolveAlias(NULL, alias, moved, &changed) == 0, "ResolveAlias after rename");
    { int ex = 0; CHECK(cfs_fsspec_to_path(moved, r, sizeof r, &ex) && ex, "resolved spec exists");
      CHECK(strstr(r, "b.txt") != NULL, "alias followed the rename");
      CHECK(changed == 1, "wasChanged reported"); }
    CHECK(FSpDelete(moved) == 0 && access(q, F_OK) != 0, "FSpDelete");
    rmdir(dir);
    puts("ok");
    return 0;
}
SRC
sed -i '' '/FindFolderDirID/d' "$T/t.c"
cc -arch x86_64 -Wno-deprecated-declarations -Wno-multichar -o "$T/t" "$T/t.c" \
   "$IMPL/datetime.c" "$IMPL/fsspec.c" "$IMPL/alias.c" -I "$IMPL" \
   -framework CoreServices -framework CoreFoundation 2> "$T/cc.log" \
   || { echo "shimdb-native: FAIL (build)"; head -20 "$T/cc.log"; exit 1; }
out=$("$T/t"); rc=$?
[ $rc = 0 ] && echo "shimdb-native: PASS" || { echo "shimdb-native: FAIL ($out)"; exit 1; }
