#!/bin/bash
# nib_edit_field_test.sh — A/B guard for the native-nib edit-field swap
# (carbon_nib_shim.c swap_native_edit_fields + the key-filter bridge). Builds a
# throwaway bundle whose Key.nib mirrors Call of Duty 4's key-code window (a
# movable-modal IBCarbonWindow with one IBCarbonEditText {'Item',1}) and runs the
# translated fixture against it. The window is never shown, but it is a real
# HIToolbox window, so this is a GUI guard (check-gui).
# ON exits 42; OFF = M64_NO_NIB_EDIT_SWAP=1 (the native UserPane stays).
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/build/99_nib_edit_field.x86_64"
[ -x "$BIN" ] || { echo "nib-edit-field: FAIL (fixture not built: $BIN)"; exit 1; }
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
NIB="$TMP/Key.bundle/Contents/Resources/Key.nib"
mkdir -p "$NIB"
cat > "$NIB/objects.xib" <<'XIB'
<?xml version="1.0" standalone="yes"?>
<object class="NSIBObjectData">
  <string name="targetFramework">IBCarbonFramework</string>
  <object name="rootObject" class="NSCustomObject" id="1">
    <string name="customClass">NSApplication</string>
  </object>
  <array count="3" name="allObjects">
    <object class="IBCarbonWindow" id="166">
      <string name="windowRect">330 535 506 948 </string>
      <string name="title">Key Code Check</string>
      <object name="rootControl" class="IBCarbonRootControl" id="167">
        <string name="bounds">0 0 176 413 </string>
        <array count="1" name="subviews">
          <object class="IBCarbonEditText" id="206">
            <string name="bounds">91 84 107 134 </string>
            <ostype name="controlSignature">Item</ostype>
            <int name="controlID">1</int>
            <boolean name="isUnicode">TRUE</boolean>
            <boolean name="isSingleLine">TRUE</boolean>
          </object>
        </array>
      </object>
      <boolean name="isResizable">FALSE</boolean>
      <int name="carbonWindowClass">4</int>
      <int name="windowPosition">7</int>
    </object>
    <reference idRef="167"/>
    <reference idRef="206"/>
  </array>
  <array count="3" name="allParents">
    <reference idRef="1"/>
    <reference idRef="166"/>
    <reference idRef="167"/>
  </array>
  <dictionary count="2" name="nameTable">
    <string>File&apos;s Owner</string>
    <reference idRef="1"/>
    <string>KeyCode</string>
    <reference idRef="166"/>
  </dictionary>
  <unsigned_int name="nextObjectID">207</unsigned_int>
</object>
XIB
run() { perl -e 'alarm 60; exec @ARGV' "$@" >"$TMP/out.$#" 2>&1; echo $?; }
on=$(run "$BIN" "$TMP/Key.bundle")
off=$(M64_NO_NIB_EDIT_SWAP=1 run "$BIN" "$TMP/Key.bundle")
if [ "$on" = 42 ] && [ "$off" != 42 ]; then
    echo "nib-edit-field: PASS (on=$on off=$off)"
else
    echo "nib-edit-field: FAIL (on=$on off=$off)"; cat "$TMP"/out.*; exit 1
fi
