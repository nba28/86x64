#!/bin/bash
# carbon_fileops_test.sh — A/B guard for the classic File Manager WRITE path.
#
# FSpCreate/FSpDirCreate/DirCreate/HDelete/FSpDelete/FSpRename/HRename/CatMove/
# FSpCatMove/FSpExchangeFiles/FSpSetFInfo/FlushVol are what a classic app uses to
# SAVE. They were NULL-jump orphans: the legacy shim pass emits a bridge that
# calls a native which has never existed on x86_64 (verified against the real
# Mojave 10.14 x86_64 CarbonCore, not an SDK stub), and nothing else in the
# process supplies them.
#
# ON : create/rename/move/delete really happen, each confirmed by an INDEPENDENT
#      classic call (FSMakeFSSpec returns noErr iff the target exists).
# OFF: ABICONV_FSSPEC_LEGACY=1 — every call declines with a File Manager error and
#      nothing is created. That is the pre-fix behaviour minus the jump to zero,
#      so the arms must differ or the guard is proving nothing.
set -u
cd "$(dirname "$0")"
BIN=build/99_carbon_fileops.x86_64
if [ ! -x "$BIN" ]; then
  echo "carbon-fileops: SKIP (build/99_carbon_fileops.x86_64 missing)"; exit 0
fi
fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

on=$("$BIN" 2>/dev/null); rc=$?
want='survived=1 findfolder=1 dircreate=1 dircreate_dup=1 fspcreate=1
fspcreate_seen=1 fspcreate_dup=1 setfinfo=1 fsprename=1 fsprename_new=1
fsprename_old=1 subdir=1 fspcatmove=1 fspcatmove_new=1 fspcatmove_old=1
flushvol=1 fspdelete=1 fspdelete_gone=1 hdelete_sub=1 hdelete_dir=1
hdelete_gone=1 done=1'
miss=""; for k in $want; do has "$on" "$k" || miss="$miss $k"; done
if [ "$rc" = 0 ] && [ -z "$miss" ]; then
  echo "  ON  (shims armed):  create/rename/move/delete all happen for real, each"
  echo "                      confirmed by an independent FSMakeFSSpec            OK"
else
  echo "  ON  (shims armed):  rc=$rc missing:$miss"
  printf '%s\n' "$on" | sed 's/^/      /'; fail=1
fi

off=$(ABICONV_FSSPEC_LEGACY=1 "$BIN" 2>/dev/null); orc=$?
# Must still SURVIVE (declining is not crashing) but must not create anything.
if [ "$orc" = 0 ] && has "$off" 'survived=1' \
   && ! has "$off" 'dircreate=1' && ! has "$off" 'fspcreate=1' \
   && ! has "$off" 'fsprename=1' && ! has "$off" 'fspcatmove=1'; then
  echo "  OFF (kill switch):  every call declines with a File Manager error and"
  echo "                      nothing is created — the arms differ                OK"
else
  echo "  OFF (kill switch):  expected every write to decline (rc=$orc):"
  printf '%s\n' "$off" | sed 's/^/      /'; fail=1
fi

[ "$fail" = 0 ] && echo "carbon-fileops: PASS" || echo "carbon-fileops: FAIL"
exit $fail
