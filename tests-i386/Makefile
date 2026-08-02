# tests-i386: regression suite for the 86x64 translator.
#
# Builds small i386 Mach-O programs against an SL-era libSystem extracted from
# the Snow Leopard install DVD, translates each via macho-tool, runs the
# translated x86_64 binary, and diffs stdout against an expected/<name>.txt.
#
# Layout:
#   src/<name>.{asm,c,cc,m}   — test source
#   expected/<name>.txt       — expected stdout
#   build/<name>.i386         — i386 executable (built once, cached)
#   build/<name>.x86_64       — translated x86_64 executable
#   build/<name>.actual       — actual stdout from translated run
#
# Usage:
#   make            — build + translate + run all tests
#   make <name>     — single test
#   make clean      — wipe build/
#   make sysroot    — rebuild /tmp/i386-sysroot from the SL DVD mount
#
# The i386 SDK Apple shipped before macOS 10.15 is gone from Tahoe's CLT, so
# we synthesize one from /Volumes/Mac OS X Install DVD. Mount the SL DVD
# (or the iLife/SL install image) before first build.

I386_SYSROOT := /tmp/i386-sysroot
SL_DVD       := /Volumes/Mac OS X Install DVD
PROJ_ROOT     := $(realpath ..)
MACHO_TOOL    := $(PROJ_ROOT)/build/src/macho-tool/macho-tool
MACHO_TOOL_DIR:= $(PROJ_ROOT)/build/src/macho-tool
LIBABICONV    := $(PROJ_ROOT)/build/src/abiconv/libabiconv.dylib
LIBWRAPPER    := $(PROJ_ROOT)/build/src/86x64/libwrapper.a
LIBINTERPOSE  := $(PROJ_ROOT)/build/src/86x64/libinterpose.dylib
PIPELINE      := $(PROJ_ROOT)/src/86x64/86x64.sh
export PATH   := $(MACHO_TOOL_DIR):$(PATH)

NASM   := nasm
LD     := ld
CC_I386:= clang -arch i386 -isysroot $(I386_SYSROOT) -mmacosx-version-min=10.6
LDFLAGS_I386 := -arch i386 -macos_version_min 10.6 -no_pie -syslibroot $(I386_SYSROOT) -lSystem

# ObjC tests compile against host SDK headers (the i386 sysroot ships no
# objc/Foundation headers) but link against the SL-era i386 libobjc/Foundation
# staged by `make sysroot-objc`. -fobjc-runtime=macosx-fragile selects the
# ObjC1 ABI the translator expects (legacy __OBJC,__cls_refs/__message_refs).
HOST_SDK := $(shell xcrun --show-sdk-path)
CC_I386_OBJC := clang -arch i386 -isysroot $(HOST_SDK) -mmacosx-version-min=10.6 -fobjc-runtime=macosx-fragile
LDFLAGS_I386_OBJC := $(LDFLAGS_I386) -lobjc -framework Foundation -framework CoreFoundation

# C++ tests compile against the host SDK headers (the i386 sysroot ships no
# libstdc++ headers) and link against the SL-era i386 libstdc++ staged by
# `make sysroot-cpp`. -dead_strip_dylibs drops the libstdc++ load command for
# tests that don't actually use the C++ runtime (e.g. the self-contained
# weak_bind guard), so they run with NO libstdc++ at load — modern macOS ships
# none. Tests that genuinely use libstdc++ (exceptions/RTTI/std::) keep the dep.
CXX_I386 := clang++ -arch i386 -isysroot $(HOST_SDK) -mmacosx-version-min=10.6
LDFLAGS_I386_CPP := $(LDFLAGS_I386) -lstdc++ -dead_strip_dylibs

SRC_DIR      := src
EXPECTED_DIR := expected
BUILD_DIR    := build

ASM_SRCS := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.asm)
C_SRCS   := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.c)
S_SRCS   := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.s)
OBJC_SRCS := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.m)
CPP_SRCS  := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.cc)
TESTS    := $(notdir $(basename $(ASM_SRCS) $(C_SRCS) $(S_SRCS)))
OBJC_TESTS := $(notdir $(basename $(OBJC_SRCS)))
CPP_TESTS  := $(notdir $(basename $(CPP_SRCS)))

# No crt0 — the 86x64.sh pipeline's wrapper exec calls into the translated
# _main directly, replacing the libc start crt0 would have provided. Tests
# end their _main with `call _exit` rather than `ret` since the wrapper
# uses `jmp` (no return address) to enter _main. crt0.s is kept in src/
# for transform-only experiments but not linked into the pipeline path.

.PHONY: all clean sysroot sysroot-objc sysroot-cpp sysroot-gl list run-only build-only objc cpp change-deps cg-color getctable cfprefs-scalar-forward cfprefs-global-locale-forward cfxml-infoptr-layout qd-pict qt-movie synth-dyld-info nopie-exec-dylib-rebase jt-weak-import dyld-multicopy dyld-multicopy-reprocess arena-publish-toctou zerofill-reslide carbon-nib classic-alert classic-dialog carbon-dangling stale-check stripped-stackarg-procptr code-interior-alias code-entry-alias code-thunk-entry memdisp-code-alias handle-marshal byval-marshal fpreg-return handle-arg fmgr-vinfo structret-regclass pack-record cb-extended sub-framework va-list-skip ae-shadow record-shadow byval-int32 selfref-imm-stage-shift pcmap-stage-shift inplace-symlink-guard rsrc-datafork vendor-dead-system closure-considerset closure-data-shadow snapshot-compat draw-clip lockfocus-1x cachedisplay-1x offscreen-redraw lcd-color displayrect-comark nscopybits focus-ring layerback-invariance window-chrome carbon-window-compositing agl-window-drawable
all: run-only

# selfref-imm-stage-shift: guard for the STAGE-SHIFT-STALE pointer-immediate fix
# (core instruction.cc MOV_MEMv_IMMz abs32-dest arm; Civ IV Steam boost.python
# registry _Rb_tree header left/right stored &_M_header-0x1000 -> rb_decrement
# crash at 0x4). The old transform cloned `movl $abs32, abs32` keeping the
# pointer as a raw imm32 in __text; later pipeline stages re-parse the M64
# archive and re-resolve only the rip-relative DESTINATION, so any __DATA
# layout shift left the imm one page stale. The fix emits lea r11,[rip+target];
# mov [rip+dest],r11d (no pointer immediate survives). The test forces a
# __DATA page shift with repeated `modify --insert load-dylib` re-parses and
# asserts the self-referential stores track the final layout. The runtime half
# of the fixture runs in the main suite as 86_selfref_imm_store. Needs the
# i386 sysroot; SKIPs without.
selfref-imm-stage-shift: $(MACHO_TOOL)
	@bash selfref_imm_stage_shift_test.sh $(MACHO_TOOL)

# pcmap-stage-shift: guard for the __86x64_pcmap/__86x64_ehlsda STAGE-SHIFT
# staleness (core section.cc reparse routing + PcmapBlob/EhlsdaBlob::Parse).
# The C++ EH unwinder's PC map used to round-trip M64 re-parses through the
# generic DataParser, which pointer-detects rows whose frozen i386 `orig`
# vmaddrs alias the image's own range (any input based inside the M64 layout
# window) and re-emits them "rebased" by any later layout shift -> every throw
# in the deployed binary resolved the wrong original PC/LSDA. The fix lifts
# both sections back into live blobs that re-resolve the TRANSLATED side and
# keep the original-i386 side verbatim (the __86x64_xrel/abs32 pattern).
# Needs the i386 sysroot + staged libstdc++; SKIPs without.
pcmap-stage-shift: $(MACHO_TOOL)
	@bash pcmap_stage_shift_test.sh $(MACHO_TOOL)

# snapshot-compat: guard for the legacy NSView-snapshot bridge (objc_shim.c
# legacy_snapshot_compat_install). The pre-10.6 offscreen-window capture idiom
# (display + lockFocus + -[NSBitmapImageRep initWithFocusedViewRect:]) is dead
# on modern AppKit (nil rep; downstream "CGImageDestinationFinalize failed for
# output type 'public.tiff'", empty GL textures — Quinn bitmapFromView/
# imageFromView -> GLImage board-flip + AnimatedContainerView transitions).
# The compat swizzles initWithFocusedViewRect: to a cacheDisplayInRect:
# capture of [NSView focusView] on a hand-built 1x rep (10.6 contract).
# Self-contained native x86_64 (dlopen of the REAL libabiconv + exported
# test hook; no i386 sysroot).
snapshot-compat: $(LIBABICONV)
	@bash snapshot_compat_test.sh $(LIBABICONV)

# byval-int32: guard for the legacy fixed-32 by-value-struct SIZING fix (abigen
# byval_field_is_int32; Civ HIViewFindByID wall). The 10.6 parse canonicalizes
# SInt32/UInt32/OSType to `long`, so abigen sized an all-int 8B record (HIViewID/
# ControlID {OSType;SInt32}) as 16B -> two eightbytes -> two GP regs, shifting the
# next arg. Fix: a `long`-canonical field whose typedef chain ends in "32" is a
# fixed-32 type = 4 bytes on x86_64. Asserts the record rides ONE GP reg + the
# following pointer isn't shifted, and a genuine {long,long} still spans two.
# Self-contained (abigen + nasm; no i386 sysroot).
byval-int32:
	@bash byval_int32_test.sh

# va-list-skip: guard for the abigen va_list-arg SKIP (C6). abigen can't marshal a
# va_list (i386 char* vs x86_64 __va_list_tag) and used to emit a broken
# __va_list_tag deep-copy (native va_arg -> SIGSEGV); it now detects the va_list
# structurally and skips the function (correct v* fns get a hand-shim). Asserts
# the va_list fn is skipped/not-emitted while an ordinary fn still is. Self-
# contained (abigen + nasm). The CF va_list hand-shim runs in 53_cf_format_valist.
va-list-skip:
	@bash va_list_skip_test.sh

# cb-extended: guard for the extended callback-bridge marshalling (C4): a
# long-long callback RETURN (CBR_I64; _86x64_call_i386 folds i386 edx:eax -> rax:
# funopen fpos_t seekfn, CGDataProvider off_t skipForward) and a small by-VALUE
# struct callback ARG (an all-int <=8B aggregate rides one GP reg == a 32/64-bit
# int -> CBA_I32/CBA_I64: CGScreenUpdateMoveDelta, classic Carbon Point). Asserts
# the emitted cb-signature descriptors + that FP-bearing structs stay unsupported.
# Self-contained (abigen + nasm; no i386 sysroot).
cb-extended:
	@bash cb_extended_test.sh

# pack-record: guard for the #pragma pack record-LAYOUT fix (abigen convert_record
# + align_field pack_cap; Civ 'oapp' AEGetParamDesc EXC_BAD_ACCESS). Classic Carbon
# AppleEvent structs are pack(2) (AEDesc: Handle @+4, not the natural +8); abigen
# built the native record with natural alignment so the native AE callee's +4 read
# straddled -> garbage handle. The fix caps field alignment at clang_Type_getAlignOf
# (the effective pack). Self-contained (abigen + nasm; no i386 sysroot).
pack-record:
	@bash pack_record_test.sh

# structret-regclass: guard for the register-class struct RETURN + byval struct
# ARG completion (abigen relaxed the "byval arg + unhandled struct return" skip to
# admit fp_reg [CGPoint/CGSize -> eax:edx] and int_reg [NSRange/CFRange rax:rdx,
# UnsignedWide/AbsoluteTime rax hi-split] returns). Self-contained (abigen + nasm).
# The fp_reg runtime path is value-checked by test 51_structret_regclass.
structret-regclass:
	@bash structret_regclass_test.sh

# handle-arg: regression for the bare opaque-Handle ARGUMENT marshalling fix
# (abigen is_opaque_handle_type at the top-level arg dispatch; companion to
# handle-marshal's struct-FIELD case). A Handle (char**, spelled Handle/*Handle)
# passed directly as an arg is an opaque token; abigen's generic Pointer path
# would dereference it (deep-copy) -> EXC_BAD_ACCESS. The fix marshals the pointer
# VALUE. Serves GetDialogItemText/SetDialogItemText (Halo), HLock/DisposeHandle,
# AppleEvent AEDesc handles. Self-contained (abigen + nasm; no i386 sysroot).
handle-arg:
	@bash handle_arg_test.sh

# byval-marshal: regression for the GENERALIZED SysV struct-by-value ARG
# marshalling (abigen.cc byval_classify / byval_flat_convert). abigen used to
# skip every by-value struct outside {1-2 long} and homogeneous-FP; the skipped
# set bound NATIVE and crashed (classic Carbon Point-by-value family, in_addr,
# CFUUIDBytes, CFGregorianDate, NSCreateMapTableWithZone). Self-contained (abigen
# parses a synthetic header + nasm assembles the output; no i386 sysroot).
byval-marshal:
	@bash byval_marshal_test.sh

# fpreg-return: regression for the SSE-register FP-pair struct RETURN fix
# (abigen.cc fp_reg_return). CGPoint/CGSize/NSPoint/NSSize are returned by x86_64
# in xmm0:xmm1 but by i386 Darwin in the integer pair eax:edx; abigen emitted no
# return conversion, so the i386 caller read garbage. Self-contained (abigen +
# nasm; no i386 sysroot). i386 conventions verified via clang -arch i386 disasm.
fpreg-return:
	@bash fpreg_return_test.sh

list:
	@echo "Tests: $(TESTS)"
	@echo "ObjC tests (opt-in via 'make objc'): $(OBJC_TESTS)"
	@echo "C++ tests (opt-in via 'make cpp'): $(CPP_TESTS)"

# change-deps: regression test for macho-tool's classic-dylib-safe dependency-
# path rewriter (the deploy fix for translated classic dylibs). Self-contained
# (native x86_64 cc, no i386 sysroot), so it runs anywhere macho-tool is built.
change-deps: $(MACHO_TOOL)
	@bash change_deps_test.sh $(MACHO_TOOL)

# cg-color: regression guard for the forward CGFloat-array colour marshalling
# fix (cg_color_shim.c). Self-contained (native x86_64 cc + CoreGraphics, no
# i386 sysroot): converted double[] renders white, raw i386 float[] does not.
cg-color:
	@bash cg_color_test.sh

# getctable: regression guard for the GetCTable color-table shim (qd_gworld.c).
# Native QuickDraw GetCTable is gone -> abigen ___GetCTable returned NULL -> Civ
# IV's HBITMAP_Mac paletted-bitmap path deref'd the NULL CTabHandle (NULL-deref
# at 0x0). The fix returns a real correctly-sized ColorTable handle. Self-
# contained (native x86_64 cc): the shim's table construction delivers a non-NULL
# table with ctSize=n-1 + room for n ColorSpec entries + a default gray ramp; the
# old blanket-NULL (negative control) reproduces the deref bug.
getctable:
	@bash getctable_test.sh

# cfprefs-scalar-forward: regression guard for the CFPreferences scalar-read
# forwarding fix (prefs_shim.c public GetApp{Boolean,Integer}Value forward to the
# real CF impl instead of a blanket false/0 stub). Self-contained (native x86_64
# cc + CoreFoundation, no i386 sysroot): the fixed body delivers a stored
# bool(true)/int(7) and reports an absent key as absent; the old blanket stub
# (negative control) returns false. Root cause of Civ IV's first-run/EULA exit.
cfprefs-scalar-forward:
	@bash cfprefs_scalar_forward_test.sh

# cfprefs-global-locale-forward: regression guard for the CFPreferences *Copy*
# SYSTEM-GLOBAL-locale-key forward (prefs_shim.c pref_is_system_global_key). The
# *Copy* family blanket-NULLs to bypass the cfprefsd livelock, but that starved
# AppleLanguages -> -[NSLocale preferredLanguages] empty -> Civ IV's Aspyr
# localization did preferredLanguages[0] -> uncatchable Swift bounds-trap SIGILL
# (rc=132 graphics-setup crash). The fix forwards exactly the global locale/lang
# keys to real CF; app-domain reads keep the bypass. Self-contained (native x86_64
# cc + CoreFoundation): global key forwards non-empty, app-domain key stays absent,
# old blanket-NULL (negative control) starved it.
cfprefs-global-locale-forward:
	@bash cfprefs_global_locale_forward_test.sh

# cfxml-infoptr-layout: regression guard for the CFXMLNodeCreate additionalInfoPtr
# struct-relayout shim (cfxml_shim.c). CFXMLNodeCreate's `const void*
# additionalInfoPtr` is a DISCRIMINATED UNION selected by xmlType (CFXMLElementInfo*
# etc.) whose pointer/CFIndex-bearing layout differs i386(12B, 4-byte fields) vs
# x86_64(24B, 8-byte fields); abigen forwards the i386 record verbatim (opaque void*
# pointee) so native CF reads fused-handle garbage 0x08000100_00000008 ->
# EXC_BAD_ACCESS (Civ IV Steam "Launch in Window" startup XML parse). Self-contained
# (native x86_64 cc + CoreFoundation): the relayout body (copied from cfxml_shim.c)
# rebuilds the native record and real CFXMLNodeCreate round-trips the attributes
# dict/order array/isEmpty/tag out via CFXMLNodeGetInfoPtr; the negative control
# asserts the i386/x86_64 layout divergence (12!=24, off 4!=8, the fused garbage
# pointer) the fix repairs.
cfxml-infoptr-layout:
	@bash cfxml_infoptr_layout_test.sh

# draw-clip: regression guard for the LEGACY DRAW-CLIP CONTRACT restoration in
# objc_shim.c (_86x64_reverse_prep/_ret). Self-contained (native x86_64 cc +
# AppKit, no i386 sysroot): reproduces the sibling-stomp structurally — a legacy
# view whose drawRect: fills BEYOND its bounds overwrites an adjacent sibling
# under modern AppKit's no-per-view-clip cacheDisplay path (unclipped -> sibling
# white), while the fix's save-gstate + intersect-with-bounds + clip primitive
# preserves the sibling (clipped -> sibling intact).
draw-clip:
	@bash draw_clip_test.sh

# lockfocus-1x: regression guard for the legacy -[NSImage lockFocus] 1x-backing
# compat (objc_shim.c legacy_lockfocus_1x_install). Modern AppKit backs an NSImage
# lockFocus context at the display backingScaleFactor (2x on Retina), but legacy
# 10.6 lockFocus was always 1x (pixels == points) and legacy consumers index
# bitmapData / upload glTexImage2D / lay out assuming points == pixels. The
# compat swizzles lockFocus/unlockFocus so a LEGACY-originated lockFocus (flagged
# by the forward bridge) draws into an explicit 1x rep (pre-painting existing
# content, committing the 1x rep on unlock); native lockFocus stays device-scaled.
# Self-contained native x86_64 guard (dlopen of the REAL libabiconv + exported test
# hooks; no i386 sysroot). Proves the structural gate BOTH ways: legacy send pinned
# to 1x + content preserved, native send left at 2x.
lockfocus-1x: $(LIBABICONV)
	@bash lockfocus_1x_test.sh $(LIBABICONV)

# cachedisplay-1x: regression guard for the legacy -[NSView cacheDisplay] 1x-rep
# compat (objc_shim.c legacy_cachedisplay_1x_install) — the VIEW-side sibling of
# lockfocus-1x. bitmapImageRepForCachingDisplayInRect: + cacheDisplayInRect: vends
# a device-scaled (2x Retina) rep, but legacy 10.6 vended 1x (points == pixels)
# and legacy consumers index bitmapData / upload glTexImage2D in points. The compat
# swizzles bitmapImageRepForCachingDisplayInRect: so a LEGACY-originated capture
# (flagged by the forward bridge) returns a 1x rep; native captures stay 2x.
# Self-contained native x86_64 guard (dlopen of the REAL libabiconv + test hooks;
# no i386 sysroot). Proves the gate BOTH ways.
cachedisplay-1x: $(LIBABICONV)
	@bash cachedisplay_1x_test.sh $(LIBABICONV)
# offscreen-redraw: FULL-PATH canary for the legacy -[NSView
# cacheDisplayInRect:toBitmapImageRep:] RE-RENDER under translation (the
# offscreen snapshot path feeding the legacy immediate-display overlay,
# print/PDF export and drag images). A real classic-ObjC1 NSView subclass
# (registered by reverse_register_image, drawRect: reverse-dispatched) draws
# FIVE draw families — solid fill, sprite CGContextDrawImage, gloss
# CGFunction/CGShadingCreateAxial, CGImageMask ClipToMask fill, NSRectFillList
# (cg_geoarray_shim) — gated on getRectsBeingDrawn:count: (bp_getrects)
# exactly like Quinn, into snapshots taken windowless / windowed /
# layer-backed / 2x-rep / NESTED-inside-drawRect. Asserts the ACTUAL PIXELS of
# every family in every scenario (exit 255). Born from the 2026-07-18
# "offscreen re-render loses image+gradient draws" suspicion — which this
# guard REFUTED headlessly (all families land); kept as the regression canary
# pinning that whole contract. Needs the i386 sysroot + staged libobjc; SKIPs
# without.
offscreen-redraw: $(MACHO_TOOL)
	@bash offscreen_redraw_test.sh $(MACHO_TOOL)
# lcd-color: repro+guard for the NSColor CGFloat-arg COLOR-FACTORY marshalling
# fix (objc_shim.c g_cgfloat_sels: the white/HSB/alpha NSColor creators). A
# translated i386 view draws the exact -[LCDCell drawDigitElements:] colour
# idiom — on-colour via +[NSColor colorWithCalibratedWhite:alpha:] +
# colorWithAlphaComponent: (LIT), off-colour via colorWithCalibratedWhite:alpha:
# (GHOST), and a redColor control — into an offscreen bitmap and reads back the
# pixels. Unmasked, the two CGFloat args each read as an 8-byte double (2 i386
# slots) -> fused garbage -> black/transparent colour: Quinn's lit digits went
# DARK, the faint ghost segments collapsed to alpha~0, the light-gray labels
# vanished. A/B: exit 4 pre-fix (only red control) -> 7 post-fix (lit light @230,
# ghost faint @26, red intact — matches the native reference). Needs the i386
# sysroot + staged libobjc; SKIPs without.
lcd-color: $(MACHO_TOOL)
	@bash lcd_color_test.sh $(MACHO_TOOL)
# displayrect-comark: guard for the LEGACY immediate-displayRect compositor
# co-mark (objc_shim.c legacy_displayrect_comark_install /
# ABICONV_QUINN_DISPLAYRECT_COMARK). A legacy animation framework's IMMEDIATE
# -[NSView displayRect:] on a layer-backed view (Quinn's ATViewAnimation
# rotate/move redraw) lands in the layer backing but the WindowServer may not
# recomposite the sub-rect -> the rotate TRAIL + lost squares. The fix swizzles
# displayRect:/displayRectIgnoringOpacity: to run the original immediate draw
# then setNeedsDisplayInRect: (one coalesced compositor pass) for a legacy-origin
# call on a layer-backed view. On-screen result is a live manual A/B (compositor
# can't run headlessly); this guard proves the MECHANISM by counting the extra
# mark: COMARK ON issues EXACTLY one more setNeedsDisplayInRect: than OFF.
# Default OFF/inert. Needs the i386 sysroot + staged libobjc; SKIPs without.
displayrect-comark: $(MACHO_TOOL)
	@bash displayrect_comark_test.sh $(MACHO_TOOL)
# nscopybits: guard for the legacy -[NSView gState] + NSCopyBits view pixel-blit
# restore (objc_shim.c legacy_gstate_capture_install + nscopybits_shim.c). Modern
# -[NSView gState] returns 0 so the pre-10.10 view-capture idiom NSCopyBits([view
# gState],srcRect,dest) copies NOTHING -> Quinn's board REFLECTION loses the
# settled pieces (the gradient reflects via a direct draw). The fix hands a legacy
# gState caller a TOKEN mapped to the view + a NSCopyBits shim that renders the
# source view's rect into the current focus context. The guard replays the idiom
# under translation (source view draws a red piece; NSCopyBits into a bitmap dest;
# read back) — piece + bg copied == 3. (Pre-fix: gState==0 -> nothing copied ==
# red absent, verified natively.) Needs the i386 sysroot + staged libobjc; SKIPs.
nscopybits: $(MACHO_TOOL)
	@bash nscopybits_test.sh $(MACHO_TOOL)

# window-chrome: regression guard for the legacy private-frame-view window CHROME
# compat (objc_shim.c legacy_window_chrome_install / chrome_install_overlay). A
# pre-10.x app that swapped NSWindow's private frame-view class (NSFrameView) to
# get a custom titlebar look has its -drawWindowBorderInRect: / -drawWindowTitle
# overrides ORPHANED on modern macOS (NSFrameView is gone; the titlebar is a
# separate layer-composited NSTitlebarView), so the window shows the stock white
# system titlebar. The compat detects such a window STRUCTURALLY (responds to the
# legacy -drawWindowBorderInRect: with a translated-i386 reverse-bridge IMP) and
# installs a native overlay into its NSTitlebarView that re-invokes the window's
# own chrome overrides, re-materializing the authentic look; native windows are
# untouched. Self-contained native x86_64 guard (dlopen of the REAL libabiconv +
# test hooks; no i386 sysroot). Proves the gate BOTH ways + non-white band pixels.
window-chrome: $(LIBABICONV)
	@bash window_chrome_test.sh $(LIBABICONV)

# carbon-window-compositing: A/B guard for the 64-bit Window Manager COMPOSITING
# contract (carbon_window_shim.c). HIToolbox's NewWindowCommon opens with
# `btl $0x13,%r14d / jae` on the attributes and returns -5601
# (errUnsupportedWindowAttributesForClass) for EVERY class when
# kWindowCompositingAttribute (bit 19) is absent — the non-compositing window
# model was deleted in 64-bit. Practically all 32-bit-era Carbon code omits that
# bit, so its windows come back NULL (Halo CE's fullscreen kPlainWindowClass +
# kWindowNoConstrainAttribute and windowed kDocumentWindowClass + 0x82000009
# render windows -> the app's error path -> an alert instead of a window). The
# ON side is the main-suite fixture 99_carbon_window_compositing (real window +
# content-bounds round-trip); this target adds the OFF side by re-running that
# translated binary under M64_NO_CARBON_COMPOSITING=1 and asserting -5601 + NULL.
# Reuses the main suite's binary; SKIPs when it has not been built.
carbon-window-compositing: $(BUILD_DIR)/99_carbon_window_compositing.x86_64
	@bash carbon_window_compositing_test.sh
# agl-window-drawable: A/B guard for the Carbon+AGL DRAWABLE contract
# (qd_gworld.c window-backed ports + agl_drawable_shim.c). 64-bit macOS deleted
# GetWindowPort, so the universal 32-bit-era idiom
#   aglSetDrawable(ctx, GetWindowPort(win))
# attached a NULL drawable -> aglSetDrawable returns 0 and NO GL context comes
# up, dropping the app into an error path 2006 hardware never ran (Halo CE: 21
# aglSetDrawable call sites, zero aglSetWindowRef). The fix gives the window a
# real port that records its WindowRef and routes such a drawable to the
# surviving native aglSetWindowRef. The ON side is the main-suite fixture
# 99_agl_window_drawable, which asserts a LIVE GL context (glGetString(RENDERER)
# non-empty) plus the port round-trip and the save/detach/restore cycle apps run
# around QuickDraw UI; this target adds the OFF side by re-running that binary
# under M64_NO_AGL_WINDOWREF=1 and asserting the NULL-drawable failure returns.
# Reuses the main suite's binary; SKIPs when it has not been built.
agl-window-drawable: $(BUILD_DIR)/99_agl_window_drawable.x86_64
	@bash agl_window_drawable_test.sh
# focus-ring: canary guard for the legacy focus-ring contract (audit verdict:
# no compat needed — pins WHY). (A) OS side: NSSetFocusRingStyle(NSFocusRingOnly)
# + fill still draws headlessly, leaves the shape interior unpainted, and the
# CG focus-ring style is EXEMPT from the current clip (verified against a live
# clip a normal fill cannot escape) — the exact 10.6 mechanism, and the reason
# the draw-clip compat's clip-to-bounds cannot amputate legacy exterior rings.
# (B) bridge side: libabiconv exports the real __NSSetFocusRingStyle marshalling
# thunk (imported by 7/9 Apps32 targets; a stale shimdb auto-stub would turn
# the NSFocusRingOnly+fill idiom into a solid fill painted over the control).
focus-ring: $(LIBABICONV)
	@bash focusring_test.sh $(LIBABICONV)

# layerback-invariance: canary guard pinning that forced layer-backing
# (wantsLayer=YES on a legacy-style tree — AppKit's force-migration mechanism)
# does NOT alter the headless draw contract: [display] on a never-shown window
# fires identically (today: zero drawRect:, the snapshot-compat premise), and
# cacheDisplayInRect hands subviews the identical in-draw state (arg/clip/CTM/
# antialias/interpolation) with identical pixels. Asserts plain==layered
# INVARIANCE, not absolute values, so only a genuine layer-vs-not divergence
# (what would silently change translated legacy rendering) trips it.
layerback-invariance:
	@bash layerback_invariance_test.sh

# ae-shadow: regression guard for the Apple Event Manager native-shadow bridge
# (ae_shim.c). Self-contained (native x86_64 cc + CoreServices, no i386 sysroot):
# links the real ae_shim.c and proves (A) the shadow registry preserves a FULL
# >4GB native AEDesc pointer across the 32-bit i386 shadow key (the old path
# truncated it -> Civ 'oapp' AEGetParamDesc crash); (B) shim_AEGetParamPtr
# resolves a registered shadow to the ORIGINAL native descriptor; (C)
# ae_native_dispatch enters a bare i386 handler (low-4GB, out-of-image; UPP ==
# ProcPtr on Mach-O so apps install the proc bare) via _86x64_call_i386 — the
# old classifier compared Darwin basename(3)'s single static buffer with
# itself (always equal), called EVERY handler natively, and the i386 4-byte
# ret under-popped the native frame -> fused return PC (Civ 'oapp'
# rip=0x173efb8b`00000000); (D) a cb_bridge trampoline handler keeps the
# native fast path (x64_cb_fn_is_tramp).
ae-shadow:
	@bash ae_shadow_test.sh

# record-shadow: regression guard for the record-VALUE data-shadow fix
# (abigen.cc handle_var_decl CXType_Record case + objc_slide.c load-time
# redirect). Self-contained (abigen parses a synthetic header, no i386 sysroot):
# an 8-byte record VALUE data constant (HIViewID-shaped) is shadowed with a
# low-4GB copy (info=8); a >8-byte record is not (8-byte slot cap). Fixes Civ
# EULADialog+356 -> kHIViewWindowContentID truncation.
record-shadow:
	@bash record_shadow_test.sh

# qd-pict: regression guard for the classic-PICT decode path (qd_gworld.c
# shim_DrawPicture/QDPict*). Self-contained native x86_64 (no i386 sysroot):
# strips a real system .pict's 512-byte file header to simulate an in-memory
# PicHandle body, prepends the header, and decodes via ImageIO's com.apple.pict
# reader — exactly what the shims do. SKIPs if no .pict is present.
qd-pict:
	@bash qd_pict_test.sh

# qt-movie: regression guard for the QuickTime Movie Toolbox -> AVFoundation bridge
# (quicktime_movie_bridge.m) + the GetMoviePict wrapped-picture decode (qd_gworld.c
# pict_decode 'MVPX'). Self-contained native x86_64 (no i386 sysroot, no display):
# synthesizes an H.264 movie, pulls a decoded ARGB frame via AVPlayerItemVideoOutput
# and validates the pixels, then round-trips a GetMoviePict wrapped PicHandle body.
qt-movie:
	@bash qt_movie_test.sh

# synth-dyld-info: regression test for `convert --synthesize-dyld-info` (route C:
# classic LC_DYSYMTAB image -> canonical modern LC_DYLD_INFO_ONLY dylib that
# stock install_name_tool/llvm accept). Needs a real classic i386 dylib as input
# (modern ld can't emit classic format); SKIPs if none is present.
synth-dyld-info: $(MACHO_TOOL)
	@bash synth_dyld_info_test.sh $(MACHO_TOOL)

# nopie-exec-dylib-rebase: regression for the NON-PIE-executable -> dylib REBASE
# synthesis fix (core archive.cc synthesize_dyld_info rebase_only path). A non-PIE
# i386 executable (Civ IV Steam) carries an LC_DYLD_INFO with an EMPTY rebase
# table (fixed base => absolute internal pointers, zero rebases). Converting it to
# an always-slid dylib left those pointers stale -> EXC_BAD_ACCESS. The fix merges
# synthesized REBASE opcodes into the existing empty-rebase stream. Builds a
# non-PIE i386 exec with an internal __data func-ptr array, converts to a dylib,
# asserts input rebase_size==0 and output rebase_size>0. Needs i386 sysroot+nasm.
nopie-exec-dylib-rebase: $(MACHO_TOOL)
	@bash nopie_exec_dylib_rebase_test.sh $(MACHO_TOOL)

# dyld-multicopy: regression test for the multi-libabiconv-copy __dyld+8
# mod-init artifact fix (wrap_mod_init_funcs skip; Halo CE + bundled translated
# QuickTime). Builds an i386 fixture that recreates patch_dyld_section's
# func_lookup artifact in a typed __mod_init_func slot, dlopens a SECOND
# libabiconv copy, and asserts the re-scan skips (unfixed: SIGSEGV in dlopen).
# Needs the i386 sysroot + ABICONV_RUN_INITS (set by the script); SKIPs without.
dyld-multicopy: $(MACHO_TOOL)
	@bash dyld_multicopy_test.sh $(MACHO_TOOL)

# dyld-multicopy-reprocess: regression for the CROSS-COPY slide_objc per-image
# processing claim (objc_shim.c objc_shared_ctrl.proc_img +
# _86x64_objc_shared_claim_image; objc_slide.c slide_objc). A second co-located
# libabiconv copy's _dyld_register_func_for_add_image re-fires slide_objc over
# every already-mapped image with a PRIVATE processed-set, re-walking not-yet-
# ready metadata -> NULL-base +0x88 deref (~40% of Civ IV launches). The claim
# makes each image processed exactly ONCE across all copies. A/B: default arm
# asserts the re-scan skips (no re-process); ABICONV_NO_XCOPY_CLAIM=1 arm asserts
# the re-scan re-processes (the pre-fix hazard). Needs the i386 sysroot; SKIPs
# without.
dyld-multicopy-reprocess: $(MACHO_TOOL)
	@bash dyld_multicopy_reprocess_test.sh $(MACHO_TOOL)

# arena-publish-toctou: regression for the arena_init shared-ctrl PUBLISH TOCTOU
# (objc_shim.c arena_init setenv/getenv -> the residual rc=139 Civ IV ~1/5 startup
# crash after the proc_img CLAIM fix). Two libabiconv copies whose arena_init
# publish/adopt races build TWO ctrls -> two proc_img tables -> the same image's
# static inits run twice -> Civ's same-image FConsole intrusive-list registry
# NULL-splices at +0x88/+0x8c. Fix = fixed low-4GB rendezvous word + atomic CAS
# publish (one ctrl always). A/B via ABICONV_ARENA_TRACE `[arena] create` counting;
# SKIPs until the trace hook is in libabiconv. Needs the i386 sysroot.
arena-publish-toctou: $(MACHO_TOOL)
	@bash arena_publish_toctou_test.sh $(MACHO_TOOL)

# zerofill-reslide: regression for the zerofill-section double-slide fix
# (objc_slide.c slide_data_fnptrs skips S_ZEROFILL/S_GB_ZEROFILL/
# S_THREAD_LOCAL_ZEROFILL). A __bss/__common word is always a RUNTIME value
# (no file bytes), so a second co-located libabiconv copy's add-image re-scan
# must never re-slide it (Civ IV: GTokenizer::FreeAllBuffers()'s __common
# buffer array double-slid -> write-into-read-only-__TEXT at exit). Native
# unit test over objc_slide.c.o's test-only export with a forced nonzero
# slide (the harness can't slide translated no_pie images). SKIPs without the
# built objc_slide.c.o.
zerofill-reslide: $(MACHO_TOOL)
	@bash zerofill_reslide_test.sh $(MACHO_TOOL)

# stripped-stackarg-procptr: regression for the stripped-binary stack-arg
# ProcPtr relocation fix (core instruction.cc; Halo CE renderer-check event
# handlers registered via `movl $handler,(%esp)` in a symbol-stripped binary).
# Asserts the raw i386 ProcPtr immediate is relocated (not left as a bare
# c7 04 24 <i386 addr>). Needs the i386 sysroot; SKIPs without.
stripped-stackarg-procptr: $(MACHO_TOOL)
	@bash stripped_stackarg_procptr_test.sh $(MACHO_TOOL)

# code-interior-alias: regression for the CODE-INTERIOR ALIAS gate (core
# section.cc DataParser + ParseEnv::code_interior_alias; Civ IV STEAM static-init
# SIGSEGV). A locals-stripped, reloc-less, fixed-address i386 exec disarms every
# other pointer-detection discriminator, so a 4-byte __DATA constant aliasing a
# NON-instruction byte of __text is falsely rebased — Civ's strtok delimiter
# " ._" became "mT\xa9\x10", splitting the registry path "Game" on 'm' and
# NULL-dereferencing the failed lookup at +0x88. Asserts BOTH directions via the
# M64_NO_CODE_INTERIOR_GATE kill-switch. Needs the i386 sysroot; SKIPs without.
code-interior-alias: $(MACHO_TOOL)
	@bash code_interior_alias_test.sh $(MACHO_TOOL)

# code-entry-alias: regression for the CODE-ENTRY gate (core section.cc
# DataParser + ParseEnv::code_alias_lacks_entry_evidence; Civ IV STEAM
# static-init SIGSEGV). Companion to code-interior-alias: that gate rejects
# values that are not instruction starts, this one rejects values that ARE valid
# boundaries but sit MID-FUNCTION with no entry evidence (no nlist, no
# `55 89 e5` prologue) — Civ's " ._" = 0x005F2E20 is the latter. Asserts BOTH
# directions via the M64_NO_CODE_ENTRY_GATE kill-switch. Needs the i386 sysroot.
code-entry-alias: $(MACHO_TOOL)
	@bash code_entry_alias_test.sh $(MACHO_TOOL)

# code-thunk-entry: the OTHER direction of the same ENTRY gate — a __DATA word
# pointing at an i386 C++ ABI ADJUSTOR THUNK (`add|sub $imm,disp8(%esp)` +
# tail-`jmp`) is a genuine vtable slot and MUST still be relocated, even though
# the thunk has no nlist after `strip -x` and no `55 89 e5` frame setup.
# Measured on the real i386 Civ IV: without this evidence shape the gate demoted
# 7883 real vtable slots. Asserts BOTH directions via the
# M64_NO_THUNK_ENTRY_EVIDENCE kill-switch. Needs the i386 sysroot.
code-thunk-entry: $(MACHO_TOOL)
	@bash code_thunk_entry_test.sh $(MACHO_TOOL)

# memdisp-code-alias: regression for the CODE-ALIAS gate on BASE-REGISTER memory
# displacements (core instruction.cc memdisp_code_alias_is_constant; Civ IV
# STEAM "launch in window" SIGBUS at 0x10382008, 2026-07-28). The absolute-table
# heuristic relocated `lea 0x27ec(%ecx),%edx` — the GMemory free-list loop
# terminator (0x2800 chunk minus one 0x14 node) — because 0x27ec aliases __text,
# so the loop never terminated and walked off the chunk into a read-only
# neighbouring mapping. Applies the IMMEDIATE family's entry-evidence classifier
# to displacements that carry a base register. Asserts BOTH directions via the
# M64_NO_MEMDISP_CODE_ALIAS_GATE kill-switch PLUS a negative control (a genuine
# __DATA table base must still be relocated). Needs the i386 sysroot.
memdisp-code-alias: $(MACHO_TOOL)
	@bash memdisp_code_alias_test.sh $(MACHO_TOOL)

# sub-framework: guard for LC_SUB_FRAMEWORK (0x12) support in the core load-command
# parser (lc.cc). Umbrella sub-frameworks (iMovie's Helium/HeliumRender) carry it;
# the parser's default case used to throw "load command 0x12 not supported" and
# abort the translate. Builds an i386 dylib with -umbrella, runs rebasify+transform,
# asserts no throw + LC_SUB_FRAMEWORK preserved. Needs the i386 sysroot; SKIPs without.
sub-framework: $(MACHO_TOOL)
	@bash sub_framework_test.sh $(MACHO_TOOL)

# handle-marshal: regression for the opaque-Handle-in-struct marshalling fix
# (abigen typeconv.cc; Civ IV 'oapp' AppleEvent crash — AEDesc::dataHandle is a
# Memory Manager Handle that was deep-copied/dereferenced instead of marshalled
# as an opaque pointer value). Self-contained (abigen parses a synthetic header;
# no i386 sysroot). Asserts the Handle field is value-marshalled AND a genuine
# int** field is still deep-copied (fix is name+structure scoped).
handle-marshal:
	@bash handle_marshal_test.sh

# fmgr-vinfo: regression for the REAL PBHGetVInfoSync free-space report
# (fmgr_shim.c; Halo CE false "Free up some space on your local disk" abort —
# the old no-op returned nsvErr so every classic disk-space check failed).
# Self-contained native cc of fmgr_shim.c with a shrunken __PAGEZERO (the shim
# reads its param block through 32-bit i386 slots): asserts noErr + real
# statfs-backed free space with the classic 2GB pinning (15-bit counts, no
# 32-bit product overflow) + single-volume index-enumeration termination.
fmgr-vinfo:
	@bash fmgr_vinfo_test.sh

# jt-weak-import: regression test for the WEAK-IMPORT jump-table fix. Exercises
# the classic i386 __IMPORT,__jump_table self-modifying stub path: undefined
# imports are marked N_WEAK_REF (=> WEAK_IMPORT bind, so REMOVED symbols bind
# NULL and the image LOADS) and get NULL-checking `ud2` diagnostic trampolines
# that survive the convert reparse. Needs a real classic i386 __jump_table
# binary (modern ld rewrites the stubs); SKIPs if none is present.
jt-weak-import: $(MACHO_TOOL)
	@bash jt_weak_import_test.sh $(MACHO_TOOL)

# carbon-nib: regression guard for the IBCarbon nib reader (carbon_nib_parse.h)
# that drives carbon_nib_shim.c's real window/control materialization. Pure
# native x86_64 (no i386 sysroot, no HIToolbox, no window server): parses a
# synthetic objects.xib + Halo's real Halo.nib and asserts nameTable lookup,
# window/rect, nested control classes, controlID/command/bounds, XML unescape.
carbon-nib:
	@bash carbon_nib_parse_test.sh

# classic-alert: regression guard for the AUTHENTIC classic Carbon alert
# (carbon_classic_alert.c). The native HIToolbox standard alert is measurably
# INERT on modern macOS (no click / Return / AXPress dismisses it) and draws
# modern chrome, so the alert is re-materialized on the real classic substrate:
# a real Carbon movable-modal window with self-drawn classic HIViews. Proves,
# with a REAL synthetic click through the session event tap, that clicking OK
# returns classic item 1 and clicking Cancel returns item 2, that the window is
# the classic 420pt-wide alert WITH a title bar (movable), and that the
# M64_NO_CLASSIC_ALERT kill switch makes the path decline so the caller falls
# back. Native x86_64; SKIPs without a window server / Accessibility rights.
classic-alert:
	@bash carbon_classic_alert_test.sh

# classic-dialog: regression guard for the AUTHENTIC classic Carbon Dialog
# Manager (carbon_classic_dialog.c) and, above all, for THE CLASSIC EDIT FIELD
# (carbon_classic_widgets.c). 64-bit macOS deleted TextEdit AND the Control
# Manager's edit-text control, so the sunken frame, blinking caret,
# click-to-place, drag/shift selection and the classic editing keys are all
# drawn and driven by us; Halo's product-key gate and every IBCarbonEditText in
# a nib depend on them. The editing-model half runs HEADLESS (no window server,
# no Accessibility rights) so it is a real always-on guard; the window +
# synthetic-typing + click half SKIPs cleanly where input cannot be synthesised.
classic-dialog:
	@bash carbon_classic_dialog_test.sh

# carbon-dangling: regression guard that the reclassified "dangling abigen" classic
# Carbon symbols (carbon_control_shim/carbon_dialog_shim/carbon_classic_ui_shim/
# upp_shim) are DEFINED hand shims in libabiconv, carry no undefined-native dangle,
# and are excluded from the abigen legacy pass. Inspects the built libabiconv.
carbon-dangling:
	@bash carbon_dangling_shims_test.sh

# stale-check: regression guard for `m64 stale-check`, the retranslate-staleness
# detector (src/86x64/m64). Asserts the _shim_for matcher mirrors static-interpose
# (true positive for a shimmed symbol, no false positive for an unshimmed native
# one, `$`-variant folding) and that the retranslated HeliumRender reports CLEAN
# end-to-end (ios_base fix guarded). Inspects the built libabiconv + m64 module;
# the end-to-end leg skips if iMovie isn't deployed. No i386 sysroot needed.
stale-check: $(MACHO_TOOL)
	@bash stale_check_test.sh $(MACHO_TOOL)

# inplace-symlink-guard: regression guard for the m64 in-place-symlink DATA-LOSS
# guard (src/86x64/m64: _inplace_symlink_escape / cmd_translate). The Apps32
# Steam entries are symlinks into the live Steam install; `m64 translate` WITHOUT
# `-o` used to write THROUGH the link and destroy the i386 source. Asserts the
# detector flags a cross-tree escape but not an intra-bundle Versions/Current
# link or a /tmp firmlink, and end-to-end that in-place is refused while `-o`
# passes. Inspects the m64 module (+ built artifacts if present); no sysroot.
inplace-symlink-guard:
	@bash inplace_symlink_guard_test.sh

# rsrc-datafork: regression guard for the CLASSIC RESOURCE FORK -> DATA-FORK
# RESOURCE FILE migration (src/86x64/m64 flatten_forks + src/abiconv/
# rsrc_datafork_shim.c). codesign refuses any bundle carrying an HFS resource
# fork, and on Apple Silicon an unsigned bundle cannot launch — so the pipeline
# had to strip the forks, silently emptying every classic *.rsrc (Halo's
# first-launch EULA: CreateScrollingTextBoxControl -> -192 resNotFound, dialog
# built then DisposeWindow + exit). Asserts, on a hand-built REAL resource map:
# codesign refuses the fork / accepts the flattened bundle, flatten_forks moves
# the payload byte-for-byte (in place when the data fork is empty, else to a
# .86x64rsrc sidecar, never clobbering a real data fork) and is idempotent, the
# modern Resource Manager still reads it via FSOpenResourceFile(ref,0,NULL,...),
# and the shim stays wired (MTSHIM + custom.syms + CMake + built libabiconv).
# No sysroot needed.
rsrc-datafork:
	@bash rsrc_datafork_test.sh

# vendor-dead-system: regression guard for the DEAD-ABSOLUTE-SYSTEM-PATH auto-
# vendor in src/86x64/_vendor_deps.py. An i386 app links a system framework by
# absolute path Apple later REMOVED (Civ IV / iPhoto embed Python 2.6 via
# /System/.../Python.framework/Versions/2.6/Python; modern macOS ships no Python
# 2.x) -> dyld hard-fails at load. The fix auto-vendors a dead /System//usr dep
# (path neither on disk nor in the dyld shared cache, no native home) so
# rpath-fix can repoint the consumer at @rpath/... Asserts the dyld-cache oracle
# (libSystem cache-served=loadable vs removed Python 2.6=dead) and end-to-end
# that vendor() pulls the dead framework while leaving cache-served libs and
# already-in-bundle leaves alone. Pure Python-module unit test; no sysroot.
vendor-dead-system:
	@bash vendor_dead_system_test.sh

# closure-considerset: guard for the CO-TRANSLATED-CLOSURE import discovery
# (_i386_closure.py + abigen_modern_manifest.py). The consider set used to see
# only each target MAIN EXE's imports — apps whose code lives in companion
# binaries (iWork thin exe + SF*.framework pool dylibs, Source-engine
# @loader_path siblings) contributed nothing, so e.g. SFTabular's AddressBook
# kAB* data constants got no shadow -> truncated native address -> crash.
# Also pins _vendor_deps.is_macho's THIN 32-bit Mach-O magics and the
# AddressBook umbrella mapping. Hermetic x86_64 fixtures; no sysroot.
closure-considerset:
	@bash closure_considerset_test.sh

# closure-data-shadow: END-TO-END i386 guard for the same fix — builds an i386
# program reading AddressBook's kABTitleProperty exactly like SFTabular's
# static init, translates it, runs it, expects the real value ("Title") via
# libabiconv's ___kABTitleProperty low-4GB shadow. Needs the objc sysroot +
# the 10.6 SDK i386 AddressBook link stub (auto-staged).
closure-data-shadow:
	@bash closure_data_shadow_test.sh

# ObjC test group. Kept separate from the default `all` so the core
# (asm/C) suite stays green while ObjC support is still landing. Builds each
# src/NN_*.m i386 ObjC1 binary, translates it via the full pipeline, runs it,
# and diffs stdout against expected/. Requires `make sysroot-objc` once.
objc: $(BUILD_DIR) $(BUILD_DIR)/libabiconv.dylib $(MACHO_TOOL)
	@pass=0; fail=0; \
	for t in $(OBJC_TESTS); do \
	  $(CC_I386_OBJC) -c $(SRC_DIR)/$$t.m -o $(BUILD_DIR)/$$t.o 2>/dev/null || { echo "FAIL $$t (compile)"; fail=$$((fail+1)); continue; }; \
	  $(LD) $(LDFLAGS_I386_OBJC) -e _main -o $(BUILD_DIR)/$$t.i386 $(BUILD_DIR)/$$t.o 2>/dev/null || { echo "FAIL $$t (link — run 'make sysroot-objc'?)"; fail=$$((fail+1)); continue; }; \
	  bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) -o $(BUILD_DIR)/$$t.x86_64 $(BUILD_DIR)/$$t.i386 >/dev/null 2>&1 || { echo "FAIL $$t (translate)"; fail=$$((fail+1)); continue; }; \
	  chmod +x $(BUILD_DIR)/$$t.x86_64; \
	  actual=$(BUILD_DIR)/$$t.actual; expected=$(EXPECTED_DIR)/$$t.txt; \
	  $(BUILD_DIR)/$$t.x86_64 > $$actual 2>/dev/null; \
	  echo "exit_code: $$?" >> $$actual; \
	  if [ -f $$expected ] && diff -q $$expected $$actual >/dev/null 2>&1; then \
	    echo "PASS $$t"; pass=$$((pass+1)); \
	  else \
	    echo "FAIL $$t"; [ -f $$expected ] && diff -u $$expected $$actual | sed 's/^/    /' | head -12; \
	    fail=$$((fail+1)); \
	  fi; \
	done; \
	echo ""; echo "===== objc: $$pass passed, $$fail failed ====="

# C++ test group (opt-in via 'make cpp'). Mirrors `objc`. Builds each
# src/NN_*.cc i386 C++ binary with clang++, translates via the full pipeline,
# runs, and diffs stdout. Self-contained tests (no C++ runtime use) link to
# libSystem only thanks to -dead_strip_dylibs and need no sysroot-cpp; tests
# that use the C++ runtime (exceptions/RTTI/std::) need `make sysroot-cpp` once.
# Per-test compile flags: the weak_bind guard builds -fno-rtti -fno-exceptions
# so its only deps are its own weak vtables (no libstdc++ / typeinfo / cxa).
cpp: $(BUILD_DIR) $(BUILD_DIR)/libabiconv.dylib $(MACHO_TOOL)
	@pass=0; fail=0; \
	for t in $(CPP_TESTS); do \
	  xtra=""; case $$t in *weak_bind|*member_func_ptr) xtra="-fno-rtti -fno-exceptions";; esac; \
	  $(CXX_I386) $$xtra -c $(SRC_DIR)/$$t.cc -o $(BUILD_DIR)/$$t.o 2>/dev/null || { echo "FAIL $$t (compile)"; fail=$$((fail+1)); continue; }; \
	  $(LD) $(LDFLAGS_I386_CPP) -e _main -o $(BUILD_DIR)/$$t.i386 $(BUILD_DIR)/$$t.o 2>/dev/null || { echo "FAIL $$t (link — run 'make sysroot-cpp'?)"; fail=$$((fail+1)); continue; }; \
	  bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) -o $(BUILD_DIR)/$$t.x86_64 $(BUILD_DIR)/$$t.i386 >/dev/null 2>&1 || { echo "FAIL $$t (translate)"; fail=$$((fail+1)); continue; }; \
	  chmod +x $(BUILD_DIR)/$$t.x86_64; \
	  actual=$(BUILD_DIR)/$$t.actual; expected=$(EXPECTED_DIR)/$$t.txt; \
	  $(BUILD_DIR)/$$t.x86_64 > $$actual 2>/dev/null; \
	  echo "exit_code: $$?" >> $$actual; \
	  if [ -f $$expected ] && diff -q $$expected $$actual >/dev/null 2>&1; then \
	    echo "PASS $$t"; pass=$$((pass+1)); \
	  else \
	    echo "FAIL $$t"; [ -f $$expected ] && diff -u $$expected $$actual | sed 's/^/    /' | head -12; \
	    fail=$$((fail+1)); \
	  fi; \
	done; \
	echo ""; echo "===== cpp: $$pass passed, $$fail failed ====="

build-only: $(addprefix $(BUILD_DIR)/, $(addsuffix .x86_64, $(TESTS)))

run-only: build-only
	@pass=0; fail=0; \
	for t in $(TESTS); do \
	  actual=$(BUILD_DIR)/$$t.actual; \
	  expected=$(EXPECTED_DIR)/$$t.txt; \
	  if [ ! -f $$expected ]; then \
	    echo "SKIP $$t (no expected/$$t.txt)"; continue; \
	  fi; \
	  $(BUILD_DIR)/$$t.x86_64 > $$actual 2>&1; \
	  ec=$$?; \
	  echo "exit_code: $$ec" >> $$actual; \
	  if diff -q $$expected $$actual >/dev/null 2>&1; then \
	    echo "PASS $$t"; pass=$$((pass+1)); \
	  else \
	    echo "FAIL $$t"; \
	    diff -u $$expected $$actual | sed 's/^/    /' | head -20; \
	    fail=$$((fail+1)); \
	  fi; \
	done; \
	echo ""; echo "===== $$pass passed, $$fail failed ====="; \
	[ $$fail = 0 ]

sysroot:
	@mkdir -p $(I386_SYSROOT)/usr/lib/system
	@# headers: the install DVD carries no usr/include; stage the extracted
	@# 10.6 SDK's headers (see memory asset_library_convention) so CC_I386's
	@# -isysroot finds <stdio.h> etc. after a /tmp wipe.
	@if [ ! -f "$(I386_SYSROOT)/usr/include/stdio.h" ] && \
	    [ -d "$(HOME)/projects/Library/SDKs/MacOSX10.6.sdk/usr/include" ]; then \
	  cp -R "$(HOME)/projects/Library/SDKs/MacOSX10.6.sdk/usr/include" "$(I386_SYSROOT)/usr/include"; \
	fi
	@for f in "$(SL_DVD)/usr/lib"/libSystem*.dylib; do \
	  n=$$(basename "$$f"); \
	  lipo -thin i386 "$$f" -output "$(I386_SYSROOT)/usr/lib/$$n" 2>/dev/null || true; \
	done
	@for f in "$(SL_DVD)/usr/lib/system"/*.dylib; do \
	  n=$$(basename "$$f"); \
	  lipo -thin i386 "$$f" -output "$(I386_SYSROOT)/usr/lib/system/$$n" 2>/dev/null || true; \
	done
	@ls $(I386_SYSROOT)/usr/lib/ | head -10
	@echo "sysroot ready at $(I386_SYSROOT)"

# Stage the i386 ObjC runtime + Foundation/CoreFoundation from the SL DVD into
# the sysroot so `make objc` can link. Run once after `make sysroot`.
sysroot-objc: sysroot
	@lipo -thin i386 "$(SL_DVD)/usr/lib/libobjc.A.dylib" -output "$(I386_SYSROOT)/usr/lib/libobjc.A.dylib" 2>/dev/null || true
	@( cd "$(I386_SYSROOT)/usr/lib" && ln -sf libobjc.A.dylib libobjc.dylib )
	@for fw in Foundation CoreFoundation; do \
	  mkdir -p "$(I386_SYSROOT)/System/Library/Frameworks/$$fw.framework"; \
	  lipo -thin i386 "$(SL_DVD)/System/Library/Frameworks/$$fw.framework/$$fw" \
	    -output "$(I386_SYSROOT)/System/Library/Frameworks/$$fw.framework/$$fw" 2>/dev/null || true; \
	done
	@echo "objc sysroot ready (libobjc + Foundation + CoreFoundation i386)"

# Stage the i386 libstdc++ from the SL DVD so `make cpp` can link C++ tests that
# use the C++ runtime (exceptions / RTTI / std::). Run once after `make sysroot`.
# (The weak_bind guard is self-contained and needs only the compiler; the
# exception-unwinding tests need the real libstdc++.) The dylib's install_name is
# /usr/lib/libstdc++.6.dylib, so stage the versioned file plus the .6 and bare
# symlinks ld resolves -lstdc++ through.
# Stage the i386 OpenGL + ApplicationServices frameworks from the SL DVD so
# 99_cgl_renderer_check can link. Both are UMBRELLAS: ld refuses to link a
# sub-framework directly and follows every LC_REEXPORT_DYLIB by its INSTALL NAME
# under -syslibroot, so the reexported children have to be staged at their real
# Versions/A paths too (and the framework binary itself duplicated there, since
# the install name points at Versions/A). Missing children are tolerated — ld
# only needs the ones on the reexport chain. Run once after `make sysroot`.
sysroot-gl: sysroot
	@set -e; SR="$(I386_SYSROOT)/System/Library/Frameworks"; \
	 stage() { \
	   mkdir -p "$$2"; \
	   lipo -thin i386 "$$1" -output "$$2/$$3" 2>/dev/null || return 0; \
	 }; \
	 for fw in OpenGL ApplicationServices CoreFoundation; do \
	   src="$(SL_DVD)/System/Library/Frameworks/$$fw.framework/Versions/A/$$fw"; \
	   stage "$$src" "$$SR/$$fw.framework" "$$fw"; \
	   stage "$$src" "$$SR/$$fw.framework/Versions/A" "$$fw"; \
	 done; \
	 for f in "$(SL_DVD)"/System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries/*.dylib; do \
	   [ -f "$$f" ] || continue; \
	   stage "$$f" "$$SR/OpenGL.framework/Versions/A/Libraries" "$$(basename "$$f")"; \
	 done; \
	 for f in ImageIO CoreText ATS ColorSync HIServices LangAnalysis PrintCore QD \
	          SpeechSynthesis CoreGraphics; do \
	   stage "$(SL_DVD)/System/Library/Frameworks/ApplicationServices.framework/Versions/A/Frameworks/$$f.framework/Versions/A/$$f" \
	         "$$SR/ApplicationServices.framework/Versions/A/Frameworks/$$f.framework/Versions/A" "$$f"; \
	 done; \
	 stage "$(SL_DVD)/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices" \
	       "$$SR/CoreServices.framework/Versions/A" "CoreServices"; \
	 for f in CarbonCore OSServices AE LaunchServices DictionaryServices SearchKit \
	          Metadata CFNetwork; do \
	   stage "$(SL_DVD)/System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/$$f.framework/Versions/A/$$f" \
	         "$$SR/CoreServices.framework/Versions/A/Frameworks/$$f.framework/Versions/A" "$$f"; \
	 done
	@echo "gl sysroot ready (OpenGL + ApplicationServices i386 umbrellas)"

sysroot-cpp: sysroot
	@lipo -thin i386 "$(SL_DVD)/usr/lib/libstdc++.6.0.9.dylib" \
	    -output "$(I386_SYSROOT)/usr/lib/libstdc++.6.0.9.dylib" 2>/dev/null || true
	@( cd "$(I386_SYSROOT)/usr/lib" && \
	   ln -sf libstdc++.6.0.9.dylib libstdc++.6.dylib && \
	   ln -sf libstdc++.6.dylib libstdc++.dylib )
	@echo "cpp sysroot ready (libstdc++ i386)"

$(BUILD_DIR):
	@mkdir -p $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)
	$(NASM) -f macho32 $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC_I386) -c $< -o $@

# GAS/AT&T hand-written asm tests — for i386 idioms nasm can't express (e.g. the
# cross-section SECTDIFF anchor+disp reloc in 25_pic_anchor_branch_store).
# clang's integrated assembler emits the correct scattered relocs.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.s | $(BUILD_DIR)
	$(CC_I386) -c $< -o $@

# 12_static_byte_flag needs -fno-pic so the static-data store uses absolute
# addressing (c6 05 <abs32> <imm8>) rather than a PIC call+pop pattern.
$(BUILD_DIR)/12_static_byte_flag.o: $(SRC_DIR)/12_static_byte_flag.c | $(BUILD_DIR)
	$(CC_I386) -fno-pic -c $< -o $@

# 16_data_mid_blob_write: -fno-pic for the absolute mid-__data byte store.
$(BUILD_DIR)/16_data_mid_blob_write.o: $(SRC_DIR)/16_data_mid_blob_write.c | $(BUILD_DIR)
	$(CC_I386) -fno-pic -c $< -o $@

# 60_data_const_text_alias: -fno-pic so the constants are read via absolute
# [disp32] loads (the shape real fixed-address i386 apps like Quinn use). The
# PIC-anchored variant of the same idioms (mid-blob SHORT read + anchor-relative
# fn-ptr call) is now covered by 61_pic_anchor_short_read (default PIC rule).
$(BUILD_DIR)/60_data_const_text_alias.o: $(SRC_DIR)/60_data_const_text_alias.c | $(BUILD_DIR)
	$(CC_I386) -fno-pic -c $< -o $@

# 64_imm_const_text_alias: -fno-pic so integer/pointer IMMEDIATES are emitted
# as bare absolute imm32 (`movl $0xffff,%edx` / `movl $imm,(%esp)` /
# `movl $imm,abs32`) — the fixed-load-address shape whose heuristic relocation
# the function-entry gate narrows (Civ IV static-init priority 0xffff).
$(BUILD_DIR)/64_imm_const_text_alias.o: $(SRC_DIR)/64_imm_const_text_alias.c | $(BUILD_DIR)
	$(CC_I386) -fno-pic -c $< -o $@

# 20_callback_qsort: -fno-stack-protector suppresses __stack_chk_guard setup,
# which the translator mishandles (PIC-relative load of the guard pointer gets
# mis-resolved). The test exercises callback bridging, not stack protection.
$(BUILD_DIR)/20_callback_qsort.o: $(SRC_DIR)/20_callback_qsort.c | $(BUILD_DIR)
	$(CC_I386) -fno-stack-protector -c $< -o $@

# 93_ns_symbol_lookup: same -fno-stack-protector dodge as test 20 (the "_"+name
# stack buffer would otherwise pull in the mishandled __stack_chk_guard setup).
# The test exercises the NSSymbol/__strcat_chk shims, not stack protection.
$(BUILD_DIR)/93_ns_symbol_lookup.o: $(SRC_DIR)/93_ns_symbol_lookup.c | $(BUILD_DIR)
	$(CC_I386) -fno-stack-protector -c $< -o $@

# 96_zerofill_common_interior: -fcommon so the 1.5MB tentative array lands in
# __DATA,__common (clang's default -fno-common would use __bss — also zerofill,
# but the huge-__common shape is the Halo SIGFPE repro: file offset 0, no file
# bytes, 0x186a00 of vmaddr span). Guards the ZeroBlob extent rework (vmaddr-
# only reservation, extent splits at symbol placeholders, zerofill-as-segment-
# tail section ordering) + the PIC-anchored zerofill-alias immediate cancel.
$(BUILD_DIR)/96_zerofill_common_interior.o: $(SRC_DIR)/96_zerofill_common_interior.c | $(BUILD_DIR)
	$(CC_I386) -fcommon -c $< -o $@

# 24_libm_float_return: -fno-builtin so floorf/ceilf/sqrt/floor are emitted as
# real library calls (clang would otherwise constant-fold/inline them as SSE
# intrinsics), exercising the libm shims + the abigen float/double-return
# xmm0->st0 conversion.
$(BUILD_DIR)/24_libm_float_return.o: $(SRC_DIR)/24_libm_float_return.c | $(BUILD_DIR)
	$(CC_I386) -fno-builtin -c $< -o $@

# 35_pthread_cond_handshake: the i386 SL sysroot ships no <pthread.h>, so compile
# against the host SDK headers under -arch i386 (correct i386 struct sizes +
# PTHREAD_MUTEX_RECURSIVE); the link still resolves pthread to the SL i386
# libSystem.
$(BUILD_DIR)/35_pthread_cond_handshake.o: $(SRC_DIR)/35_pthread_cond_handshake.c | $(BUILD_DIR)
	clang -arch i386 -isysroot $(HOST_SDK) -mmacosx-version-min=10.6 -c $< -o $@

# Tests use _main as the entry directly (-e _main); the 86x64.sh wrapper
# enters _main via jmp, so tests must end with `call _exit` not `ret`.
$(BUILD_DIR)/%.i386: $(BUILD_DIR)/%.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<

# 99_cstr_pagezero_arg / 99_cgl_renderer_check are the only tests in the MAIN
# suite that link a framework, so unlike the opt-in `objc`/`cpp` suites they
# cannot assume their sysroot prerequisite was staged first — a bare
# `make sysroot && make run-only` (the state after every reboot, since the
# sysroot lives in /tmp) must not go red on a link error. Each stages its own
# dependency on demand; the staging targets are idempotent and only run when the
# framework is genuinely missing, so a warm sysroot costs nothing.
$(BUILD_DIR)/99_cstr_pagezero_arg.i386: $(BUILD_DIR)/99_cstr_pagezero_arg.o
	@[ -f "$(I386_SYSROOT)/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation" ] \
	  || $(MAKE) --no-print-directory sysroot-objc
	$(LD) $(LDFLAGS_I386) -e _main -framework CoreFoundation -o $@ $<

$(BUILD_DIR)/99_cgl_renderer_check.i386: $(BUILD_DIR)/99_cgl_renderer_check.o
	@[ -f "$(I386_SYSROOT)/System/Library/Frameworks/OpenGL.framework/OpenGL" ] \
	  || $(MAKE) --no-print-directory sysroot-gl
	$(LD) $(LDFLAGS_I386) -e _main -framework OpenGL -framework ApplicationServices -o $@ $<

# 36_stripped_func_pad: STRIP local symbols (keep only the .globl _main) so the
# translator sees an empty func_syms, exercising the no-symbol padding-resync
# path. Without the strip, _h10/_h20 would be in func_syms and the existing
# boundary guard would catch them -> the test would pass for the wrong reason.
# An explicit rule for this target overrides the generic %.i386 pattern rule.
$(BUILD_DIR)/36_stripped_func_pad.i386: $(BUILD_DIR)/36_stripped_func_pad.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@

# 62_pic_xsection_thunk_stripped: STRIP local symbols so the cross-section
# get_pc_thunk is UNNAMED (the Halo shape: fully-stripped GCC binary, thunk in
# __textcoal_nt). The name-based ParseEnv::pic_thunks seed then finds nothing;
# only the structural executable-section byte-scan (archive.cc) establishes the
# anchor. Without the strip, the named-thunk seed would catch it and the test
# would pass for the wrong reason. Overrides the generic %.i386 pattern rule.
$(BUILD_DIR)/62_pic_xsection_thunk_stripped.i386: $(BUILD_DIR)/62_pic_xsection_thunk_stripped.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@

# 99_cmp_abs32_imm_notptr: STRIP local symbols (the Civ IV Steam shape:
# locals-stripped => have_local_text_syms false => the code_alias_is_constant
# mid-function gate is DISARMED), then ASSERT the compare constant 0x2400
# really falls inside __TEXT,__text's vmaddr range — the alias trap the test
# exists to spring (if section layout ever drifts, fail the BUILD loudly
# instead of letting the guard false-pass). Guards the CMP/TEST memory-dest
# COMPARISON-VALUE gate in instruction.cc's [abs32]+imm32 arm (Civ IV
# `cmpl $0x100308, _version; ja` OS-version check mis-relocated to a lea
# target -> "insufficient system version" exit). Overrides %.i386.
# 99_init_priority_code_alias: STRIP local symbols (the Civ IV Steam shape:
# locals-stripped => have_local_text_syms false => the code_alias_is_constant
# mid-function gate is DISARMED), then ASSERT the GCC default static-init
# priority constant 0xffff really falls inside __TEXT,__text's vmaddr range —
# the alias trap the test springs (if layout drifts, fail the BUILD loudly).
# Guards imm32_code_alias_is_constant (instruction.cc): the MOV_GPRv_IMMv/
# PUSH/ADD register-imm family + the CMP_GPRv_IMMz twin must classify a
# code-aliasing imm32 as a CONSTANT on the stripped path too (Civ IV Steam
# "Launch in Window": all 1084 `mov $0xffff,%edx` static-init stub priorities
# rebased to lea; the first-in-sweep dispatcher kept its literal
# `cmp $0xffff,%edx` -> that TU's ctors skipped -> NULL std::list registry ->
# EXC_BAD_ACCESS addr=0x8), while a prologue-target fn-pointer immediate
# (`movl $_helper,%ecx`) still relocates. Overrides %.i386.
$(BUILD_DIR)/99_init_priority_code_alias.i386: $(BUILD_DIR)/99_init_priority_code_alias.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@
	@a=$$(otool -l $@ | awk '/sectname __text$$/{s=1;next} s&&$$1=="addr"{print $$2;exit}'); \
	 z=$$(otool -l $@ | awk '/sectname __text$$/{s=1;next} s&&$$1=="size"{print $$2;exit}'); \
	 if [ $$((a)) -le $$((0xffff)) ] && [ $$((0xffff)) -lt $$((a + z)) ]; then \
	   echo "  99_init_priority_code_alias: 0xffff aliases __text [$$a, $$a+$$z) -- trap armed"; \
	 else \
	   echo "99_init_priority_code_alias: ERROR: 0xffff outside __text [$$a, $$a+$$z) -- adjust pad"; \
	   rm -f $@; exit 1; \
	 fi

# 99_code_alias_imm_falsereloc: STRIP local symbols (Civ IV Steam shape — locals-
# stripped => imm32_code_alias_is_constant's symboled discriminator disarmed, only
# the 55-89-e5 prologue-byte evidence remains). ASSERT the integer 0x1f90 falls
# inside __text AND that the prologue bytes 55 89 e5 sit exactly at it (the alias
# trap the test springs). Guards against 31de727's over-relocation: an integer
# CONSTANT aliasing a prologue-byte __text address must NOT be relocated when
# stored into a field (the Civ boost-registry rc=139 deterministic-crash root).
# Overrides %.i386.
$(BUILD_DIR)/99_code_alias_imm_falsereloc.i386: $(BUILD_DIR)/99_code_alias_imm_falsereloc.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@
	@a=$$(otool -l $@ | awk '/sectname __text$$/{s=1;next} s&&$$1=="addr"{print $$2;exit}'); \
	 z=$$(otool -l $@ | awk '/sectname __text$$/{s=1;next} s&&$$1=="size"{print $$2;exit}'); \
	 o=$$(otool -l $@ | awk '/sectname __text$$/{s=1;next} s&&$$1=="offset"{print $$2;exit}'); \
	 if [ $$((a)) -le $$((0x1f90)) ] && [ $$((0x1f90)) -lt $$((a + z)) ]; then \
	   fo=$$((o + 0x1f90 - a)); \
	   b=$$(xxd -s $$fo -l 3 -p $@); \
	   if [ "$$b" = "5589e5" ]; then \
	     echo "  99_code_alias_imm_falsereloc: 0x1f90 aliases __text w/ 55 89 e5 -- trap armed"; \
	   else \
	     echo "99_code_alias_imm_falsereloc: ERROR: bytes at 0x1f90 = $$b, not 55 89 e5 -- adjust pad"; \
	     rm -f $@; exit 1; \
	   fi; \
	 else \
	   echo "99_code_alias_imm_falsereloc: ERROR: 0x1f90 outside __text [$$a, $$a+$$z) -- adjust pad"; \
	   rm -f $@; exit 1; \
	 fi

# 99_cstring_interior_alias: STRIP local symbols (Civ IV Steam shape — locals-
# stripped => have_local_text_syms false => code_alias_is_constant and the
# classic-reloc gate are both disarmed, so the DataParser cstring-INTERIOR gate
# is the sole discriminator). ASSERT that _probe's value (_cstr+8) really lands
# in __TEXT,__cstring AND is a mid-string INTERIOR (the byte before it is NOT a
# NUL and it is not the section start) — the alias trap the test springs (fail
# the BUILD loudly if the string/section layout ever drifts). Overrides %.i386.
$(BUILD_DIR)/99_cstring_interior_alias.i386: $(BUILD_DIR)/99_cstring_interior_alias.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@
	@a=$$(otool -l $@ | awk '/sectname __cstring$$/{s=1;next} s&&$$1=="addr"{print $$2;exit}'); \
	 z=$$(otool -l $@ | awk '/sectname __cstring$$/{s=1;next} s&&$$1=="size"{print $$2;exit}'); \
	 o=$$(otool -l $@ | awk '/sectname __cstring$$/{s=1;next} s&&$$1=="offset"{print $$2;exit}'); \
	 v=$$(( a + 8 )); \
	 if [ $$((a)) -lt $$((v)) ] && [ $$((v)) -lt $$((a + z)) ]; then \
	   pb=$$(xxd -s $$(( o + v - 1 - a )) -l 1 -p $@); \
	   if [ "$$pb" != "00" ]; then \
	     echo "  99_cstring_interior_alias: _cstr+8 aliases __cstring INTERIOR (prev byte=$$pb) -- trap armed"; \
	   else \
	     echo "99_cstring_interior_alias: ERROR: _cstr+8 preceded by NUL (a string START, not interior) -- adjust string"; \
	     rm -f $@; exit 1; \
	   fi; \
	 else \
	   echo "99_cstring_interior_alias: ERROR: _cstr+8 outside __cstring [$$a, $$a+$$z) -- adjust string"; \
	   rm -f $@; exit 1; \
	 fi

$(BUILD_DIR)/99_cmp_abs32_imm_notptr.i386: $(BUILD_DIR)/99_cmp_abs32_imm_notptr.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@
	@a=$$(otool -l $@ | awk '/sectname __text$$/{s=1;next} s&&$$1=="addr"{print $$2;exit}'); \
	 z=$$(otool -l $@ | awk '/sectname __text$$/{s=1;next} s&&$$1=="size"{print $$2;exit}'); \
	 if [ $$((a)) -le $$((0x2400)) ] && [ $$((0x2400)) -lt $$((a + z)) ]; then \
	   echo "  99_cmp_abs32_imm_notptr: 0x2400 aliases __text [$$a, $$a+$$z) -- trap armed"; \
	 else \
	   echo "99_cmp_abs32_imm_notptr: ERROR: 0x2400 outside __text [$$a, $$a+$$z) -- adjust pad/constant"; \
	   rm -f $@; exit 1; \
	 fi

# 39_xml_memget_alloc: the i386 sysroot ships no libxml2, so `_xmlMemGet` is an
# undefined dynamic_lookup import (resolved at translate time by static-interpose
# -> libabiconv's ___xmlMemGet shim, exactly as a real binary's libxml2 bind is).
# -undefined dynamic_lookup lets the link leave it unresolved. Overrides %.i386.
$(BUILD_DIR)/39_xml_memget_alloc.i386: $(BUILD_DIR)/39_xml_memget_alloc.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 49_cgrect_byval: CoreGraphics isn't in the i386 sysroot, so the CG geometry
# predicates (CGRectIsEmpty / CGPointEqualToPoint / CGRectContainsPoint) are
# undefined dynamic_lookup imports resolved at translate time by static-interpose
# -> libabiconv's ___CG* shims (the abigen FP-by-value-struct marshalling).
# Overrides %.i386.
$(BUILD_DIR)/49_cgrect_byval.i386: $(BUILD_DIR)/49_cgrect_byval.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 51_structret_regclass: CoreGraphics isn't in the i386 sysroot, so
# CGPointApplyAffineTransform / CGSizeApplyAffineTransform (register-class struct
# RETURN + byval struct args) are undefined dynamic_lookup imports resolved at
# translate time by static-interpose -> libabiconv's ___CG* shims (the abigen
# byval-arg + fp_reg/int_reg struct-return completion). Overrides %.i386.
$(BUILD_DIR)/51_structret_regclass.i386: $(BUILD_DIR)/51_structret_regclass.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 79_qd_gworld_pixels: classic QuickDraw isn't in the i386 sysroot, so every QD
# entry (NewGWorld/EraseRect/PaintRect/GetPixBaseAddr/...) is an undefined
# dynamic_lookup import resolved at translate time by static-interpose ->
# libabiconv's ___<Name> shims (the CG-backed GWorld substrate, qd_gworld.c).
# Overrides %.i386.
$(BUILD_DIR)/79_qd_gworld_pixels.i386: $(BUILD_DIR)/79_qd_gworld_pixels.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 85_quinn_cell_draw: like 49, CoreGraphics isn't in the i386 sysroot, so every
# CG entry (CGBitmapContextCreate / CGColorSpaceCreateDeviceRGB / CGContextSetAlpha
# / SetFillColor[Space] / FillRect / DrawImage / CGImage*) is an undefined
# dynamic_lookup import resolved at translate time by static-interpose ->
# libabiconv's ___CG* shims (geo table + cg_color_shim). Offscreen pixel-readback
# repro of Quinn's _QuinnGeneralFastDrawCells. Overrides %.i386.
$(BUILD_DIR)/85_quinn_cell_draw.i386: $(BUILD_DIR)/85_quinn_cell_draw.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_cg_rect_arrays: like 85, CoreGraphics isn't in the i386 sysroot, so every
# CG entry is an undefined dynamic_lookup import resolved at translate time by
# static-interpose -> libabiconv's ___CG* shims. Guards the CGFloat-GEOMETRY-
# ARRAY argument family (CGContextClipToRects / AddRects / StrokeLineSegments;
# cg_geoarray_shim.c): i386 float-element arrays must be widened per-element
# for the full COUNT, not raw-pointer-passed / single-element converted (the
# abigen shape that garbled Quinn's LCD digit partial-redraw clip).
# Overrides %.i386.
$(BUILD_DIR)/99_cg_rect_arrays.i386: $(BUILD_DIR)/99_cg_rect_arrays.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 97_carbon_fixed32_paramrec: Carbon/HIToolbox isn't in the i386 sysroot, so
# GetStandardAlertDefaultParams is an undefined dynamic_lookup import resolved
# at translate time by static-interpose -> libabiconv's legacy-pass shim.
# Guards the legacy fixed-32 RECORD layout family (written_is_fixed32_long in
# record_decl::populate_fields): a UInt32 field canonicalized to `unsigned
# long` by the -arch i386 parse must stay 4 bytes in the NATIVE record image
# (keeping AlertStdCFStringAlertParamRec.defaultText at its true pack(2)
# offset 6 — the Civ IV CreateStandardAlert __CF_IS_OBJC crash), plus the
# (CFStringRef)-1 kAlertDefaultOKText sentinel round-trip through
# x64_objc_wrap / unwrap_obj_arg. Overrides %.i386.
$(BUILD_DIR)/97_carbon_fixed32_paramrec.i386: $(BUILD_DIR)/97_carbon_fixed32_paramrec.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_carbon_window_compositing: Carbon/HIToolbox isn't in the i386 sysroot, so
# CreateNewWindow / GetWindowBounds / DisposeWindow are undefined dynamic_lookup
# imports resolved at translate time by static-interpose -> libabiconv's
# ___CreateNewWindow (the hand shim, carbon_window_shim.c) and the abigen legacy
# bridges for the other two. Guards the 64-bit Window Manager COMPOSITING
# contract: NewWindowCommon rejects any window without kWindowCompositingAttribute
# (bit 19) with -5601 errUnsupportedWindowAttributesForClass, so every 32-bit-era
# CreateNewWindow (Halo CE's fullscreen + windowed render window) came back NULL
# and the app took its error path. Overrides %.i386.
$(BUILD_DIR)/99_carbon_window_compositing.i386: $(BUILD_DIR)/99_carbon_window_compositing.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_agl_window_drawable: neither Carbon/HIToolbox nor AGL is in the i386
# sysroot, and libabiconv exports a ___<sym> shim for every symbol this fixture
# needs (___CreateNewWindow/___GetWindowPort/___GetWindowFromPort/___DisposeWindow
# plus the ___agl*/___glGetString legacy bridges), so the whole surface is
# undefined dynamic_lookup and static-interpose binds it at translate time. That
# also keeps the fixture independent of `make sysroot-gl` / the SL DVD.
# Overrides %.i386.
$(BUILD_DIR)/99_agl_window_drawable.i386: $(BUILD_DIR)/99_agl_window_drawable.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 95_crypto_cdkey_verify: like 39/49/79/85, the i386 sysroot's openssl headers
# exist but there is no i386 libcrypto to link, so the OpenSSL 0.9.7 CD-key
# surface (CRYPTO_set_mem_functions / RIPEMD160* / BN_bin2bn / RSA_new/verify/
# free) is an undefined dynamic_lookup import resolved at translate time by
# static-interpose -> libabiconv's ___<sym> crypto shims (crypto_shim.c /
# crypto_tramp.asm). Mirrors Halo's product-key RSA verify that over-popped the
# i386 frame on the raw native bind. Overrides %.i386.
$(BUILD_DIR)/95_crypto_cdkey_verify.i386: $(BUILD_DIR)/95_crypto_cdkey_verify.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 50_cgaffine_sret: like 49, CoreGraphics isn't in the i386 sysroot, so the
# CGAffineTransform* builders (CGAffineTransformMakeScale / MakeTranslation /
# Translate / Concat / Invert) are undefined dynamic_lookup imports resolved at
# translate time by static-interpose -> libabiconv's ___CGAffineTransform* shims
# (the abigen FP-by-value-struct sret RETURN marshalling). Overrides %.i386.
$(BUILD_DIR)/50_cgaffine_sret.i386: $(BUILD_DIR)/50_cgaffine_sret.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 56_cf_collection_value_ret: CoreFoundation isn't in the base i386 sysroot, so
# the CF collection builders/accessors (CFArrayCreateMutable / CFArrayAppendValue
# / CFArrayGetValueAtIndex / CFDictionary* / CFGetTypeID) are undefined
# dynamic_lookup imports resolved at translate time by static-interpose ->
# libabiconv's ___CF* shims (the abigen RETURN-side const-void* CF wrap).
# Overrides %.i386.
$(BUILD_DIR)/56_cf_collection_value_ret.i386: $(BUILD_DIR)/56_cf_collection_value_ret.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 53_cf_format_valist: CoreFoundation isn't in the i386 sysroot, so
# CFStringCreateWithCString / CFStringCreateWithFormatAndArguments (the va_list CF
# formatting shim) / CFStringGetCString are undefined dynamic_lookup imports
# resolved at translate time by static-interpose -> libabiconv's ___CF* shims.
# Overrides %.i386.
$(BUILD_DIR)/53_cf_format_valist.i386: $(BUILD_DIR)/53_cf_format_valist.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 98_cf_append_format_valist: like 53, CF isn't in the i386 sysroot, so
# CFStringCreateWithCString / CFStringCreateMutable / CFStringAppendFormatAndArguments
# (the va_list APPEND shim that builds Civ IV's standard-alert title+message) /
# CFStringGetCString/Length are undefined dynamic_lookup imports resolved at
# translate time by static-interpose -> libabiconv's ___CF* shims. Guards that
# the append path DELIVERS CONTENT (the blank-alert follow-on). Overrides %.i386.
$(BUILD_DIR)/98_cf_append_format_valist.i386: $(BUILD_DIR)/98_cf_append_format_valist.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_cf_key_concat_replace: like 98, CF isn't in the i386 sysroot, so
# CFStringCreateWithCString / CFStringCreateMutableCopy / CFStringAppend /
# CFStringFind / CFStringReplace / CFStringGetCString/Length are undefined
# dynamic_lookup imports resolved at translate time by static-interpose ->
# libabiconv's ___CF* shims. Guards that Civ's localizer KEY-CONCAT
# (mutable-copy + append) and TOKEN-SUBSTITUTION (find + replace) deliver
# content through the proxy wrap/unwrap round-trip (the blank-alert probe).
# Overrides %.i386.
$(BUILD_DIR)/99_cf_key_concat_replace.i386: $(BUILD_DIR)/99_cf_key_concat_replace.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# Translate i386 → x86_64 via the full pipeline: rebasify + transform +
# abi-insert + static-interpose + wrap. Bare `macho-tool transform` output
# isn't standalone-runnable because the i386 entry semantics (high-address
# rsp, 4-byte argc on stack) don't match x86_64's; the wrapper exec
# (build/src/86x64/libwrapper.a) builds an i386-style low-4GB frame and
# jumps to _main. We use the full pipeline so tests exercise the same
# translation path real binaries take.
$(BUILD_DIR)/%.x86_64: $(BUILD_DIR)/%.i386 $(MACHO_TOOL) $(BUILD_DIR)/libabiconv.dylib
	bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) \
	    -i $(LIBINTERPOSE) -o $@ $<
	@chmod +x $@

$(BUILD_DIR)/libabiconv.dylib: $(LIBABICONV) | $(BUILD_DIR)
	cp $< $@

clean:
	rm -rf $(BUILD_DIR)

# 89_sret_regclass_gap: CoreFoundation isn't in the i386 sysroot, so
# CFAbsoluteTimeGetGregorianDate (a MEMORY-on-i386 / REGISTER-on-x86_64 struct
# return) is an undefined dynamic_lookup import resolved at translate time by
# static-interpose -> libabiconv's ___CFAbsoluteTimeGetGregorianDate shim.
# lldiv comes from the sysroot but is likewise shimmed by abigen. Overrides %.i386.
$(BUILD_DIR)/89_sret_regclass_gap.i386: $(BUILD_DIR)/89_sret_regclass_gap.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 90_fp_sret_stack_balance: CoreGraphics isn't in the i386 sysroot, so the
# CGAffineTransform* builders are undefined dynamic_lookup imports resolved at
# translate time by static-interpose -> libabiconv's ___CGAffineTransform*
# fp_sret shims. Overrides %.i386.
$(BUILD_DIR)/90_fp_sret_stack_balance.i386: $(BUILD_DIR)/90_fp_sret_stack_balance.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
