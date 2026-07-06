#!/bin/bash
# carbon_dangling_shims_test.sh — regression guard for the reclassified "dangling
# abigen" classic-Carbon symbols (carbon_control_shim.c / carbon_dialog_shim.c /
# carbon_classic_ui_shim.c / upp_shim.c). Before the fix, abigen's legacy pass
# emitted these as forwards to now-removed 64-bit natives (`call _DisposeControl`,
# etc.), so libabiconv carried an UNDEFINED `_X (dynamically looked up)` that aborts
# the moment a translated app calls ___X. After the fix each ___X is a DEFINED text
# symbol (our hand shim) and the dangling native reference is gone.
#
# This checks the built libabiconv directly (no i386 sysroot needed): for a sample
# of the reclassified symbols, assert ___X is exported (T) AND the bare native _X is
# NOT an undefined dynamically-looked-up import. Exit 0 = reclassified cleanly.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
LIBAB="$HERE/../build/src/abiconv/libabiconv.dylib"
[ -f "$LIBAB" ] || { echo "libabiconv not built — SKIP"; exit 0; }

# (___export , bare-native-that-must-NOT-dangle)
SYMS="DisposeControl EmbedControl GetControlID GetControlCommandID NewCWindow DisposeMenu
      GetNewDialog ModalDialog StopAlert DisposeDialog GetDialogItemText SetPortDialogPort
      TESetSelect TEGetText LActivate GetFlavorData CountDragItems GetDropLocation
      TrackMouseLocation DisposeThemeDrawingState NewControlActionUPP DisposeEventLoopTimerUPP"

fail=0
exports="$(nm -gU "$LIBAB" | awk '{print $3}')"
undefs="$(nm -mu "$LIBAB" | awk '{print $NF}')"
for s in $SYMS; do
  if ! grep -qx "___$s" <<<"$exports"; then
    echo "MISSING export ___$s"; fail=1
  fi
  # the bare native _X must not remain as an undefined import (that was the dangle)
  if grep -qx "_$s" <<<"$undefs"; then
    echo "DANGLING native _$s still undefined-imported"; fail=1
  fi
done

# The abigen legacy pass must NOT also emit these (would be a duplicate symbol).
LEG="$HERE/../build/src/abiconv/abiconv_legacy.asm"
if [ -f "$LEG" ]; then
  for s in DisposeControl ModalDialog GetNewDialog TESetSelect NewCWindow; do
    if grep -q "^[[:space:]]*global[[:space:]]*___$s\$" "$LEG"; then
      echo "abigen STILL emits ___$s (should be excluded via MTSHIM)"; fail=1
    fi
  done
fi

if [ "$fail" = 0 ]; then echo "carbon_dangling_shims_test: PASS"; exit 0; fi
echo "carbon_dangling_shims_test: FAIL"; exit 1
