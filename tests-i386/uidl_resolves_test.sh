#!/bin/bash
# uidl_resolves_test.sh — every native carbon_ui_shim.c resolves by name
# (UIDL(fn,...) = dlsym(RTLD_DEFAULT, "fn")) must EXIST on this OS. A name that
# 64-bit HIToolbox dropped resolves to NULL and the shim silently no-ops:
# Get/SetControl32BitValue did, so every native control read 0 (Halo's
# "Play in a window" checkbox). Their surviving twins are HIViewGet/SetValue.
#   ON : every UIDL name in the shim resolves
#   OFF: the check itself flags a known-dropped name (not inert)
set -u
cd "$(dirname "$0")"
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
check() {   # names on stdin -> prints the missing ones
  { echo '#include <dlfcn.h>'; echo '#include <stdio.h>'
    echo 'int main(void){dlopen("/System/Library/Frameworks/Carbon.framework/Carbon",RTLD_LAZY);'
    while read -r n; do echo "if(!dlsym(RTLD_DEFAULT,\"$n\"))puts(\"$n\");"; done
    echo 'return 0;}'; } > "$TMP/c.c"
  clang -arch x86_64 -w "$TMP/c.c" -o "$TMP/c" && "$TMP/c"
}
names=$(grep -o 'UIDL([A-Za-z0-9_]*' ../src/abiconv/carbon_ui_shim.c | sed 's/UIDL(//' | grep -v '^fn$' | sort -u)
fail=0
miss=$(echo "$names" | check)
if [ -z "$miss" ]; then echo "  ON : all $(echo "$names" | wc -l | tr -d ' ') UIDL names resolve  OK"
else echo "  ON : unresolved: $miss"; fail=1; fi
miss=$(echo GetControl32BitValue | check)
if [ "$miss" = GetControl32BitValue ]; then echo "  OFF: a dropped name is flagged  OK"
else echo "  OFF: GetControl32BitValue not flagged (check inert)"; fail=1; fi
[ $fail = 0 ] && echo "uidl-resolves: PASS" || echo "uidl-resolves: FAIL"
exit $fail
