#!/bin/bash
# rex_seg_null_trap_test.sh — A/B guard for two Instruction::Transform forms,
# checked in the translated bytes (bare transform, nothing runs).
#   on   shipped config                         -> `2e 48 89 ec`, 3 ud2 traps
#   off  M64_NO_REX_AFTER_SEG=1
#        M64_NO_ABS_NULL_TRAP=1                 -> `48 2e 89 ec`, 1 ud2 (the ret's)
set -u
cd "$(dirname "$0")"
MT=../build/src/macho-tool/macho-tool
I386=build/99_rex_seg_null_trap.i386
[ -f "$I386" ] || { echo "rex-seg-null-trap: FAIL ($I386 missing)"; exit 1; }

arm() { local a=$1; shift
  env MACHO_NULL_TRAP=1 "$@" "$MT" -- transform "$I386" "build/99_rsnt.$a" >/dev/null 2>&1 || return
  otool -tvj "build/99_rsnt.$a" | awk '{print $2}' | tr '\n' ' '
}
on=$(arm on);   off=$(arm off M64_NO_REX_AFTER_SEG=1 M64_NO_ABS_NULL_TRAP=1)
trap_n() { tr ' ' '\n' <<<"$1" | grep -cx 0f0b; }
if [[ $on == *" 2e4889ec "* && $(trap_n "$on") == 3 &&
      $off == *" 482e89ec "* && $(trap_n "$off") == 1 ]]; then
  echo "rex-seg-null-trap: PASS"
else
  echo "rex-seg-null-trap: FAIL (on traps=$(trap_n "$on") off traps=$(trap_n "$off"))"; exit 1
fi
