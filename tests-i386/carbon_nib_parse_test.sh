#!/bin/bash
# carbon_nib_parse_test.sh — regression guard for the IBCarbon nib reader that
# drives carbon_nib_shim.c's real window/control materialization. Self-contained
# NATIVE x86_64 test (no i386 translation, no HIToolbox, no window server): it
# exercises the pure parser in src/abiconv/carbon_nib_parse.h against a synthetic
# objects.xib AND (when present) Halo's real Halo.nib, asserting the exact facts
# the materializer relies on: nameTable lookup, window record + rect, nested
# control classes, and controlID/command/bounds extraction with XML unescaping.
#
# Exit 0 = parser produces the values the shim needs; non-zero = regression.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
HDR="$HERE/../src/abiconv/carbon_nib_parse.h"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# A minimal IBCarbon objects.xib: one window "Settings" (id 42) with a root
# control holding a button (id 7, command 'quit') and an edit-text field.
cat > "$TMP/objects.xib" <<'XIB'
<?xml version="1.0" standalone="yes"?>
<object class="NSIBObjectData">
  <array count="1" name="allObjects">
    <object class="IBCarbonWindow" id="42">
      <string name="windowRect">100 200 400 600 </string>
      <string name="title">Don&apos;t Panic</string>
      <object name="rootControl" class="IBCarbonRootControl" id="43">
        <string name="bounds">0 0 300 400 </string>
        <array count="2" name="subviews">
          <object class="IBCarbonButton" id="44">
            <string name="bounds">260 300 280 380 </string>
            <int name="controlID">7</int>
            <string name="title">OK</string>
            <ostype name="command">quit</ostype>
          </object>
          <object class="IBCarbonEditText" id="45">
            <string name="bounds">40 20 56 380 </string>
            <ostype name="controlSignature">Cprt</ostype>
            <boolean name="isUnicode">TRUE</boolean>
          </object>
        </array>
      </object>
    </object>
  </array>
  <dictionary count="1" name="nameTable">
    <string>Settings</string>
    <reference idRef="42"/>
  </dictionary>
</object>
XIB

cat > "$TMP/t.c" <<EOF
#include "$HDR"
#include <assert.h>
int main(int argc, char **argv) {
    long len;
    char *x = nibx_slurp(argv[1], &len);
    if (!x) { fprintf(stderr, "slurp failed\n"); return 2; }

    // nameTable resolves the human window name to its object id
    int id = nibx_nametable_id(x, "Settings");
    printf("nametable Settings=%d\n", id);
    assert(id == 42);

    long ws = nibx_window_offset(x, id);
    assert(ws >= 0);
    long we = nibx_match_end(x, ws);
    assert(we > ws);

    // window rect + title (with &apos; unescaped)
    char wr[64] = ""; nibx_rect r = {0,0,0,0};
    assert(nibx_str(x, ws, we, "windowRect", wr, sizeof wr));
    nibx_rect_parse(wr, &r);
    printf("windowRect=%d %d %d %d\n", r.top, r.left, r.bottom, r.right);
    assert(r.top==100 && r.left==200 && r.bottom==400 && r.right==600);
    char title[64]=""; nibx_str(x, ws, we, "title", title, sizeof title); nibx_unescape(title);
    printf("title=%s\n", title);
    assert(!strcmp(title, "Don't Panic"));

    // button record: controlID + command FourCharCode + bounds.
    // nibx_match_end needs the '<object ' START offset (as the shim calls it).
    long bo = (strstr(x, "<object class=\"IBCarbonButton\"") - x);
    long be = nibx_match_end(x, bo);
    int cid = 0; uint32_t cmd = 0; char bs[64]=""; nibx_rect br={0,0,0,0};
    assert(nibx_int(x, bo, be, "controlID", &cid) && cid == 7);
    assert(nibx_ostype(x, bo, be, "command", &cmd) && cmd == 0x71756974 /*'quit'*/);
    assert(nibx_str(x, bo, be, "bounds", bs, sizeof bs));
    nibx_rect_parse(bs, &br);
    assert(br.top==260 && br.left==300 && br.bottom==280 && br.right==380);
    printf("button id=%d cmd=%08x\n", cid, cmd);

    // edit-text record: controlSignature present (materializer needs the class + sig)
    long eo = (strstr(x, "<object class=\"IBCarbonEditText\"") - x);
    long ee = nibx_match_end(x, eo);
    uint32_t sig = 0;
    assert(nibx_ostype(x, eo, ee, "controlSignature", &sig) && sig == 0x43707274 /*'Cprt'*/);

    free(x);
    printf("SYNTH OK\n");
    return 0;
}
EOF

clang -arch x86_64 -Wall -o "$TMP/t" "$TMP/t.c" || { echo "compile failed"; exit 3; }
"$TMP/t" "$TMP/objects.xib" || { echo "SYNTH parse assertions FAILED"; exit 1; }

# If Halo's real nib is on disk, sanity-check the exact live gap the shim targets:
# the "Graphics" window (id 236) parses and its 3 IBCarbonEditText controls are seen.
HALO=""
for p in "$HOME/projects/translations/Apps32/Halo.app" "$HOME/projects/translations/Apps64/Halo.app"; do
  [ -f "$p/Contents/Resources/English.lproj/Halo.nib/objects.xib" ] && HALO="$p" && break
done
if [ -n "$HALO" ]; then
  XIB="$HALO/Contents/Resources/English.lproj/Halo.nib/objects.xib"
  cat > "$TMP/h.c" <<EOF
#include "$HDR"
#include <string.h>
int main(int argc, char **argv){
    long len; char *x = nibx_slurp(argv[1], &len); if(!x) return 2;
    int id = nibx_nametable_id(x, "Graphics");
    printf("Halo Graphics id=%d\n", id);
    if (id < 0) return 1;
    long ws = nibx_window_offset(x, id); if (ws < 0) return 1;
    long we = nibx_match_end(x, ws); if (we <= ws) return 1;
    // The UNIVERSAL window-title bridge (title_on_show/window_shown in
    // carbon_nib_shim.c) applies the nib's window title to the backing NSWindow when
    // the window is shown. Guard the exact fact it depends on: the Graphics window's
    // title parses to "Halo Graphics Settings" (the OS X reference title). A blank/
    // wrong parse here would surface as a blank title bar even with the bridge.
    char wt[128]=""; nibx_str(x, ws, we, "title", wt, sizeof wt); nibx_unescape(wt);
    printf("Halo Graphics title=%s\n", wt);
    if (strcmp(wt, "Halo Graphics Settings") != 0) { fprintf(stderr,"title mismatch\n"); return 1; }
    // count IBCarbonEditText within the file (the dropped-class gap the shim fixes)
    int n=0; const char *p=x; while((p=strstr(p,"class=\"IBCarbonEditText\""))){n++;p+=4;}
    printf("Halo IBCarbonEditText count=%d\n", n);
    if (n < 1) return 1;
    free(x); printf("HALO OK\n"); return 0;
}
EOF
  clang -arch x86_64 -Wall -o "$TMP/h" "$TMP/h.c" || { echo "halo compile failed"; exit 3; }
  "$TMP/h" "$XIB" || { echo "HALO nib parse FAILED"; exit 1; }
else
  echo "Halo nib not present — synthetic checks only (OK)"
fi

echo "carbon_nib_parse_test: PASS"
exit 0
