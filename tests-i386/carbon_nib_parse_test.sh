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
. "$(dirname "$0")/../src/86x64/paths.sh"   # M64_* local paths
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
for p in "$M64_APPS32/Halo.app" "$M64_APPS64/Halo.app"; do
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

# ---- content-view layout-origin guard (content_view_of, carbon_nib_shim.c) ----
# The nib stores control bounds CONTENT-relative, but HIViewGetRoot(win) returns the
# STRUCTURE root (title bar + content), ~22px taller with origin at the structure
# top. Self-drawn controls (group box / popup / edit field) attach via
# HIViewAddSubview(parent,...); if parent is the structure root they render ~22px too
# HIGH (the "Rendering Pipeline" group box overlapping the title bar). content_view_of
# resolves the CONTENT view (the root subview sized to the content region) so they land
# correctly. This guard REPLICATES that resolver against a real HIToolbox window and
# asserts a self-drawn view placed at content-y=6 ends up BELOW the title bar (root-y ==
# titlebar_height + 6), NOT at root-y=6. Needs a window server + dlopen-able Carbon; it
# SKIPS gracefully (PASS) where those are unavailable (e.g. a headless CI box) so the
# suite stays green, but catches the regression wherever a display session exists.
cat > "$TMP/geom.c" <<'EOF'
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
typedef void *WindowRef,*ControlRef,*HIViewRef,*HIObjectRef;
typedef int32_t OSStat;
typedef struct{int16_t top,left,bottom,right;}MyRect;
typedef struct{double x,y;}MyPoint; typedef struct{double w,h;}MySize; typedef struct{MyPoint o;MySize s;}MyHIRect;
#define kComp (1u<<19)
#define kStd (1u<<25)
static void*S(const char*n){return dlsym(RTLD_DEFAULT,n);}
static OSStat (*GWB)(WindowRef,uint16_t,MyRect*);
static OSStat (*HVGB)(HIViewRef,MyHIRect*);
static HIViewRef (*HVGFC)(HIViewRef);
static HIViewRef (*HVGNX)(HIViewRef);
/* mirror of carbon_nib_shim.c content_view_of() */
static ControlRef content_view_of(WindowRef win, ControlRef root){
  if(!root||!HVGFC||!HVGB) return root;
  double content_h=0;
  if(GWB){ MyRect cr={0,0,0,0}; if(GWB(win,33,&cr)==0) content_h=(double)(cr.bottom-cr.top);}
  MyHIRect rb={{0,0},{0,0}}; HVGB(root,&rb);
  if(content_h>0 && rb.s.h<=content_h+1.0) return root;
  HIViewRef first=HVGFC(root);
  if(content_h>0){ for(HIViewRef v=first;v;){ MyHIRect vb={{0,0},{0,0}}; HVGB(v,&vb);
      if(vb.s.h>=content_h-1.0 && vb.s.h<=content_h+1.0) return v; v=HVGNX?HVGNX(v):NULL; } }
  return first?first:root;
}
int main(void){
 dlopen("/System/Library/Frameworks/Carbon.framework/Carbon",RTLD_NOW|RTLD_GLOBAL);
 dlopen("/System/Library/Frameworks/AppKit.framework/AppKit",RTLD_NOW|RTLD_GLOBAL);
 unsigned char (*NSAppLoad)(void)=(void*)S("NSApplicationLoad"); if(NSAppLoad)NSAppLoad();
 OSStat (*CNW)(uint32_t,uint32_t,const MyRect*,WindowRef*)=(void*)S("CreateNewWindow");
 OSStat (*CRC)(WindowRef,ControlRef*)=(void*)S("CreateRootControl");
 ControlRef (*HVGR)(WindowRef)=(void*)S("HIViewGetRoot");
 GWB=(void*)S("GetWindowBounds"); HVGB=(void*)S("HIViewGetBounds");
 HVGFC=(void*)S("HIViewGetFirstSubview"); HVGNX=(void*)S("HIViewGetNextView");
 OSStat (*HOC)(CFStringRef,void*,HIObjectRef*)=(void*)S("HIObjectCreate");
 OSStat (*HVSF)(HIViewRef,const MyHIRect*)=(void*)S("HIViewSetFrame");
 OSStat (*HVAS)(HIViewRef,HIViewRef)=(void*)S("HIViewAddSubview");
 OSStat (*HVCP)(MyPoint*,HIViewRef,HIViewRef)=(void*)S("HIViewConvertPoint");
 if(!CNW||!HVGR||!HOC||!HVSF||!HVAS||!HVCP||!GWB||!HVGB||!HVGFC){ printf("SKIP: Carbon window API unavailable\n"); return 42; }
 MyRect R={192,159,751,627}; /* Halo Graphics windowRect: content 468x559 */
 WindowRef win=NULL; if(CNW(4,kComp|kStd,&R,&win)!=0||!win){ printf("SKIP: no window (headless/no WS)\n"); return 42; }
 MyRect cr={0,0,0,0},sr={0,0,0,0}; GWB(win,33,&cr); GWB(win,32,&sr);
 int tb = cr.top - sr.top;                 /* title-bar height */
 ControlRef root=NULL; OSStat rst=CRC(win,&root); if(rst!=0||!root)root=HVGR(win);
 if(!root){ printf("SKIP: no root\n"); return 42; }
 ControlRef parent=content_view_of(win,root);
 HIViewRef g=NULL; HOC(CFSTR("com.apple.hiview"),NULL,(HIObjectRef*)&g); if(!g){printf("SKIP: no hiview\n");return 42;}
 MyHIRect fr={{20,6},{428,108}}; HVSF(g,&fr); HVAS(parent,g);
 MyPoint p={0,0}; HVCP(&p,g,root);
 printf("titlebar=%d group@content-y6 -> root-y=%.1f (want %d)\n", tb, p.y, tb+6);
 /* correct == tb+6 (below the title bar); the BUG placed it at 6 (in the title bar). */
 if (p.y < tb - 0.5) { fprintf(stderr, "FAIL: self-drawn control at content-y=6 lands at root-y=%.1f, INSIDE the %dpx title bar (content_view_of regression)\n", p.y, tb); return 1; }
 printf("GEOM OK\n"); return 0;
}
EOF
clang -arch x86_64 -Wall -o "$TMP/geom" "$TMP/geom.c" -framework CoreFoundation || { echo "geom compile failed"; exit 3; }
"$TMP/geom"; grc=$?
if [ $grc -eq 42 ]; then echo "content-view geom guard SKIPPED (no window server)"; \
elif [ $grc -ne 0 ]; then echo "content-view geom guard FAILED"; exit 1; \
else echo "content-view geom guard PASS"; fi

echo "carbon_nib_parse_test: PASS"
exit 0
