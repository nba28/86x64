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
# Modern ld (ld-27037+) no longer links i386; prefer Snow Leopard's ld64-95 wrapper
# when it is installed (see memory sl_sysroot_remount). Override with LD=...
SL_LD  := $(HOME)/projects/Library/Toolchains/sl-ld64/ld-i386
LD     := $(if $(wildcard $(SL_LD)),$(SL_LD),ld)
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

# Every fixture run gets a deadline: a hang (e.g. a broken OFF arm spinning)
# fails the fixture with exit_code 137 instead of blocking the suite. The timer
# lives in a watchdog PARENT: an alarm() armed before exec would survive into
# the fixture as a live ITIMER_REAL (65_const_struct_ro_copyback reads it).
FIXTURE_DEADLINE ?= 30
RUN_FIXTURE := perl -e 'my $$t = shift; my $$p = fork; if (!$$p) { exec @ARGV or exit 127 } \
    $$SIG{ALRM} = sub { kill "KILL", $$p }; alarm $$t; waitpid($$p, 0); \
    exit($$? & 127 ? 128 + ($$? & 127) : $$? >> 8)' $(FIXTURE_DEADLINE)

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
CPP_TESTS  := $(filter-out 99_libstdcxx_redirect,$(notdir $(basename $(CPP_SRCS))))
# 99_libstdcxx_redirect.cc is excluded from the generic `cpp:` loop: it only
# passes with (a) the GNU libstdc++ 4.2.1 headers (this SDK's <sstream> etc.
# are libc++'s std::__1::, wrong ABI -- see libstdcxx_redirect_test.sh) and
# (b) the libstdc++-redirect applied (abi-hazard-vendor.py) before running,
# neither of which the generic loop does. It has its own dedicated guard,
# `libstdcxx-redirect`, which builds/redirects/runs it correctly, twice.

# No crt0 — the 86x64.sh pipeline's wrapper exec calls into the translated
# _main directly, replacing the libc start crt0 would have provided. Tests
# end their _main with `call _exit` rather than `ret` since the wrapper
# uses `jmp` (no return address) to enter _main. crt0.s is kept in src/
# for transform-only experiments but not linked into the pipeline path.

.PHONY: check dyld-section-width dptr-table init-slot-rebase native-free fault-thread-altstack pgmemo-saturation pic-anchor-entry-save seltypes-kind pic-push-mem color-getter-steal loop-ecx-count prologue-sched-entry cstring-tailmerge-ptr objc-imp-entry zerofill-stripped-ptr zerofill-code-literal heap-below-2g classic-reloc-accept disp-reloc-accept imm-reloc-accept call-abs-pagezero pic-anchor-call-adopt entry-pad-fallthrough export-trie-deep jt-esp-spill-push call-mem-esp memdisp-entry-store qsort-ptr-elems atomic-bit-volatile sib-rbp-base-wrap sib-rbp-index-wrap abs-base-neg-wrap ebp-gpr-narrow lazy-self-def standard-alert translated-provider-defer jt-spill-slot nulljump-sidecar carbon-alias carbon-fileops fscatinfo-bulk nulljump-probe-arch all clean sysroot sysroot-objc sysroot-cpp sysroot-gl list run-only build-only objc cpp change-deps cg-color getctable cfprefs-scalar-forward cfprefs-global-locale-forward cfxml-infoptr-layout qd-pict qt-movie synth-dyld-info nopie-exec-dylib-rebase jt-weak-import dyld-multicopy dyld-multicopy-reprocess arena-publish-toctou zerofill-reslide carbon-nib classic-alert classic-dialog carbon-dangling stale-check stripped-stackarg-procptr code-interior-alias code-entry-alias code-thunk-entry memdisp-code-alias handle-marshal byval-marshal fpreg-return handle-arg fmgr-vinfo fsspec-resolve structret-regclass pack-record cb-extended sub-framework va-list-skip ae-shadow record-shadow byval-int32 selfref-imm-stage-shift pcmap-stage-shift inplace-symlink-guard rsrc-datafork vendor-dead-system closure-considerset closure-data-shadow snapshot-compat draw-clip lockfocus-1x cachedisplay-1x offscreen-redraw lcd-color displayrect-comark nscopybits focus-ring layerback-invariance window-chrome carbon-window-compositing agl-window-drawable agl-fullscreen-present display-fullscreen-bridge agl-renderer-info cgl-macro-dispatch cfbundle-fnptr-shim zerofill-target-pair const-alias-output-image record-field-pair mp-allocate-buffer fault-report-once snd-lock-order snd-cb-reentrancy snd-cb-fifo snd-status-layout jt-cohesion qword-high-skip literal-abs-disp classic-input-coords mp-task-spawn libstdcxx-redirect uidl-resolves agl-oversize-window
all: run-only

# check: every suite + every guard target, each under a deadline; exit 1 on any failure.
check:
	@bash check.sh

# check-gui: everything, INCLUDING the guards that open windows / go fullscreen.
check-gui:
	@CHECK_GUI=1 bash check.sh

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

# libstdcxx-redirect: guard for the libstdc++-redirect fix (Portal 2
# 2026-09-29): a translated i386 caller into libstdc++'s iostream/ios_base
# surface (basic_ios/ostream/istream/filebuf/sentry, operator<<, ...) was a
# raw cross-ABI call against the NATIVE x86_64 system libstdc++
# (unbridged_native_call_bug). src/86x64/abi-hazard-vendor.py redirects a
# translated consumer's bind to our own TRANSLATED i386 libstdc++ instead (a
# sibling translated module -> same 4-byte-return convention, correct object
# layout). Needs the i386 sysroot + staged libstdc++ AND a built golden
# translated libstdc++; SKIPs without either. See src/99_libstdcxx_redirect.cc.
libstdcxx-redirect: $(MACHO_TOOL)
	@bash libstdcxx_redirect_test.sh $(MACHO_TOOL)

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

# color-getter-steal: A/B guard that FOREIGN code cannot replace NSColor's
# system component getters with pre-2010 ones that raise on newer colorspaces
# (libinterpose; measured on ProKit). Native, self-contained. OFF arm = the same
# program without libinterpose inserted.
color-getter-steal:
	@bash color_getter_steal_test.sh

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
# agl-oversize-window: a windowed GL window larger than the screen goes on the
# scaled surface as an ordinary window (Halo "Play in a window" at 2560x1440).
agl-oversize-window: $(BUILD_DIR)/99_agl_window_drawable.x86_64 $(BUILD_DIR)/libabiconv.dylib
	@bash agl_oversize_window_test.sh

# uidl-resolves: every native carbon_ui_shim.c resolves by name must exist on
# this OS (a dropped name silently no-ops: Get/SetControl32BitValue made every
# native control read 0). OFF arm: the check flags a known-dropped name.
uidl-resolves:
	@bash uidl_resolves_test.sh

# Reuses the main suite's binary; SKIPs when it has not been built.
agl-window-drawable: $(BUILD_DIR)/99_agl_window_drawable.x86_64
	@bash agl_window_drawable_test.sh
# agl-fullscreen-present: A/B guard for the WINDOWED-context fullscreen idiom
# (capture + CGDisplaySwitchToMode while AGL draws into a Carbon window): the
# context is presented on cgl_fullscreen_shim.m's surface instead of the Carbon
# window being moved to the screen corner. Reuses 99_agl_window_drawable
# (AGL_PRESENT mode); OFF arm = M64_NO_CGL_FULLSCREEN_BRIDGE=1.
agl-fullscreen-present: $(BUILD_DIR)/99_agl_window_drawable.x86_64
	@bash agl_fullscreen_present_test.sh
# display-fullscreen-bridge: A/B guard for the classic FULLSCREEN DISPLAY-MODE
# idiom (cg_display_fullscreen_shim.c + cg_display_tramp.asm). Performing
#   CGCaptureAllDisplays(); CGDisplaySwitchToMode(d, CGDisplayBestModeForParameters(...))
# on modern macOS is fatal: the reconfiguration makes AppKit setFrame: every
# window in the process and -[NSCGSWindow _createContext] cannot be created for
# an NSCarbonWindow -> SIGILL in NSCGSPanic. The bridge never switches: it records
# a VIRTUAL mode, answers every mode query from it, tracks the capture family
# without performing it, and presents the app's GL context on the fullscreen
# surface (its window is never moved or resized). The ON side is the main-suite fixture
# 99_display_fullscreen_bridge, which runs the whole idiom and asserts the
# physical display is untouched at the end; this target adds the OFF side by
# re-running that binary under M64_NO_DISPLAY_FULLSCREEN_BRIDGE=1 and asserting
# the native (non-exact, wrong-resolution) answer comes back.
# Reuses the main suite's binary; SKIPs when it has not been built.
display-fullscreen-bridge: $(BUILD_DIR)/99_display_fullscreen_bridge.x86_64
	@bash display_fullscreen_bridge_test.sh
# agl-renderer-info: A/B guard for the classic GPU-ENUMERATION contract
# (agl_renderer_shim.c + the DM* half in carbon_ui_shim.c, bridged by
# qd_gworld.c's GDevice registry). On 64-bit macOS the 32-bit-era sequence
#   gd = DMGetFirstScreenDevice(true);
#   DMGetDisplayIDByGDevice(gd, &displayID, true);
#   ri = aglQueryRendererInfo(&gd, 1);
# is dead twice over: aglQueryRendererInfo takes a QuickDraw GDHandle and
# returns NULL for EVERY input (measured -- it does not even set an AGL error),
# and DMGetDisplayIDByGDevice was shimmed to paramErr. The app concludes the
# machine has no accelerated GPU (Halo CE's IDirect3D_Mac::IDirect3D_Mac then
# raises its own "unrecoverable error" alert at i386 0x2b477f / 0x2b47a5). The
# fix routes the query to the surviving aglQueryRendererInfoForCGDirectDisplayIDs
# and answers the DM pair from the registry that minted the GDevice. The ON side
# is the main-suite fixture 99_agl_renderer_info, which asserts a REAL
# accelerated renderer with a real renderer id and VRAM figure; this target adds
# the OFF side by re-running that binary under M64_NO_AGL_RENDERERINFO=1
# M64_NO_DM_DISPLAYID=1 and asserting the zero-GPU failure returns.
# Reuses the main suite's binary; SKIPs when it has not been built.
# cgl-macro-dispatch: A/B guard for the <OpenGL/CGLMacro.h> direct-dispatch
# contract. A 32-bit game never calls a GL symbol -- it jumps through its
# CGLContextObj's embedded dispatch table -- so the context cannot be a proxy
# arena handle. The ON side is the main-suite fixture 99_cgl_macro_dispatch; this
# adds the M64_NO_CGL_MACRO=1 side and fails if the arms do not differ.
cgl-macro-dispatch: $(BUILD_DIR)/99_cgl_macro_dispatch.x86_64
	@bash cgl_macro_dispatch_test.sh

agl-renderer-info: $(BUILD_DIR)/99_agl_renderer_info.x86_64
	@bash agl_renderer_info_test.sh
# cfstring-range: A/B guard for the classic CFRange TOLERANCE contract. Classic
# CoreFoundation never validated the range handed to CFStringGetCharacters, so
# asking for length+1 to pick up a NUL terminator was a normal 32-bit-era idiom;
# modern CF validates in NSTaggedPointerString but NOT __NSCFString, so the same
# call site is fatal for a SHORT string and harmless for a longer one, and the
# throw is an ObjC exception a 2006 C frame cannot catch -> abort. The ON side
# is the main-suite fixture 99_cfstring_range_overrun; this adds the
# M64_NO_CFSTRING_RANGE_CLAMP=1 side and fails if the arms do not differ.
cfstring-range: $(BUILD_DIR)/99_cfstring_range_overrun.x86_64
	@bash cfstring_range_test.sh
# cfdata-bytebuf-bounce: A/B guard for the byte-buffer CF-return LENGTH
# contract. CFDataGetBytePtr/CFDataGetMutableBytePtr/CFStringGetPascalStringPtr
# return a pointer into a buffer whose length is carried separately (never a
# NUL terminator), so abigen's generic cstring-return bounce (strlen) silently
# truncates any buffer whose first byte is 0x00 — exactly the shape of Halo's
# real "Graphics Options" preference. The ON side is the main-suite fixture
# 99_cfdata_bytebuf_bounce; this adds the M64_NO_CFDATA_BYTEBUF_BOUNCE=1 side
# and fails if the arms do not differ.
cfdata-bytebuf-bounce: $(BUILD_DIR)/99_cfdata_bytebuf_bounce.x86_64
	@bash cfdata_bytebuf_bounce_test.sh
# nulljump-no-shadow: A/B guard proving static-interpose refuses to redirect a
# bind into a bridge that can only jump to NULL. abigen emits ___X while a native
# _X exists; when Apple removes the API the bridge survives, the pipeline weakens
# the dangling bind to NULL, and redirecting into it REPLACES whatever the app
# would have bound to with a guaranteed crash. Measured: 507 such bridges, 358
# shadowing implementations the app itself ships (Civ IV's
# QTNewDataReferenceFromFSRef). OFF arm empties the list via NULLJUMP=/dev/null.
nulljump-no-shadow: $(BUILD_DIR)/libabiconv.dylib $(MACHO_TOOL)
	@bash nulljump_no_shadow_test.sh
# nulljump-probe-arch: the exclusion list above is only as good as the dlsym that
# builds it, and that question is ARCHITECTURE-DEPENDENT. Asking it in-process
# was wrong twice: python3 here is arm64 (33 x86_64-only symbols called dead,
# incl. objc_msgSend_stret and the $INODE64 stat/readdir family), and the process
# was python (63 Py* bridges called healthy because RTLD_DEFAULT found the C-API
# in the interpreter). Invariant: nothing on the list may resolve on x86_64.
nulljump-probe-arch: $(BUILD_DIR)/libabiconv.dylib
	@bash nulljump_probe_arch_test.sh
# nulljump-sidecar: the exclusion list must TRAVEL with libabiconv (static-interpose
# resolves it next to the dylib) and its absence must be LOUD. Halo 2026-08-05: the
# deployed bundle had 4 libabiconv.dylib and 0 libabiconv.nulljump, so its
# translated QuickTime got _ResolveAliasFile redirected into a NULL-jump bridge and
# died live with rip=0. The old code set NULLJUMP_N=0 and said nothing.
nulljump-sidecar: $(BUILD_DIR)/libabiconv.dylib
	@bash nulljump_sidecar_test.sh
# translated-provider-defer: the MIRROR IMAGE of the "prefer the interpose shim"
# family. A bridge exists to reach a NATIVE implementation; when the provider is
# itself TRANSLATED, putting the bridge in front of it double-converts the ABI
# (Civ IV: ___Py_Initialize called our vendored translated Python with the native
# convention -> SIGSEGV). Runs the SAME translated binary twice and requires the
# M64_NO_TRANSLATED_PROVIDER_DEFER arm to actually execute the double conversion.
translated-provider-defer: $(BUILD_DIR)/99_translated_provider_defer.x86_64
	@bash translated_provider_defer_test.sh
# fault-report-once: the M64_FAULT_REPORT reporter must arm ONCE PER PROCESS, not
# once per co-located libabiconv copy (Civ's bundle loads THREE, so one fault was
# reported three times), and must symbolize from a PRE-FAULT snapshot rather than
# calling the non-async-signal-safe _dyld_* APIs from inside a signal handler.
# Self-contained native x86_64; no i386 sysroot.
fault-report-once: $(LIBABICONV)
	@bash fault_report_once_test.sh
# dladdr-shim: the dl* family was shimmed for dlopen/dlsym/dlclose/dlerror and NOT
# dladdr, so a translated _dladdr bind stayed on native libSystem and the i386
# caller's stack args were read from rdi/rsi (Civ IV: rsi=8 as the Dl_info*,
# fault at 0x56 inside dyld). Dl_info is 16 bytes on i386 vs 32 on x86_64, so the
# RECORD needs converting too.
dladdr-shim: $(BUILD_DIR)/99_dladdr_shim.x86_64
	@bash dladdr_shim_test.sh
# carbon-fileops: A/B guard for the classic File Manager WRITE path — the calls a
# classic app uses to SAVE. NULL-jump orphans: the legacy pass emits a bridge to a
# native that has never existed on x86_64 (checked against the real Mojave 10.14
# x86_64 CarbonCore, not an SDK stub), and nothing else in the process supplies
# them. OFF arm is ABICONV_FSSPEC_LEGACY=1.
carbon-fileops: $(BUILD_DIR)/99_carbon_fileops.x86_64
	@bash carbon_fileops_test.sh
# sib-rbp-base-wrap: A/B guard that a scaled-index operand with %ebp as a DATA
# base keeps the i386 mod-2^32 EA wrap (PvZ libbass). ON exits 42; OFF
# (M64_SIB_RBP_BASE_WIDE=1 at translate time) faults.
sib-rbp-base-wrap: $(BUILD_DIR)/99_sib_ebp_base_neg_wrap.i386
	@for arm in on off; do \
	  e=""; [ $$arm = off ] && e="M64_SIB_RBP_BASE_WIDE=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_sib_ebp_base_neg_wrap.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_sib_ebp_base_neg_wrap.$$arm; echo "$$arm=$$?"; \
	done | tr '\n' ' ' | grep -q "on=42 off=139" && echo "sib-rbp-base-wrap: PASS" || { echo "sib-rbp-base-wrap: FAIL"; exit 1; }
# sib-rbp-index-wrap: A/B guard that a scaled-index operand with %ebp as the
# INDEX keeps the i386 mod-2^32 EA wrap (Portal 2 libbink). ON exits 42; OFF
# (M64_SIB_RBP_INDEX_WIDE=1 at translate time) faults.
sib-rbp-index-wrap: $(BUILD_DIR)/99_sib_ebp_index_neg_wrap.i386
	@for arm in on off; do \
	  e=""; [ $$arm = off ] && e="M64_SIB_RBP_INDEX_WIDE=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_sib_ebp_index_neg_wrap.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_sib_ebp_index_neg_wrap.$$arm; echo "$$arm=$$?"; \
	done | tr '\n' ' ' | grep -q "on=42 off=139" && echo "sib-rbp-index-wrap: PASS" || { echo "sib-rbp-index-wrap: FAIL"; exit 1; }
# abs-base-neg-wrap: A/B guard that an absolute `table+disp(%base)` with a
# negative base wraps mod 2^32 (Portal 2 libbink ICC table walk). OFF arm is
# M64_NO_ABS_BASE_ADDR32=1 at TRANSLATE time.
abs-base-neg-wrap: $(BUILD_DIR)/99_abs_base_neg_wrap.i386
	@for arm in on off; do \
	  e=""; [ $$arm = off ] && e="M64_NO_ABS_BASE_ADDR32=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_abs_base_neg_wrap.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_abs_base_neg_wrap.$$arm >/dev/null 2>&1; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "abs-base-neg-wrap: PASS (on=$$r_on off=$$r_off)" || { echo "abs-base-neg-wrap: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# ebp-gpr-narrow: A/B guard that a register-only op on %ebp as a DATA register
# keeps i386 32-bit semantics (Portal 2 libbink Huffman reader). OFF arm is
# M64_RBP_REG_WIDE=1 at TRANSLATE time.
ebp-gpr-narrow: $(BUILD_DIR)/99_ebp_gpr_narrow.i386
	@for arm in on off; do \
	  e=""; [ $$arm = off ] && e="M64_RBP_REG_WIDE=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_ebp_gpr_narrow.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_ebp_gpr_narrow.$$arm >/dev/null 2>&1; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off = 7 ] && echo "ebp-gpr-narrow: PASS (on=$$r_on off=$$r_off)" || { echo "ebp-gpr-narrow: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# lazy-self-def: A/B guard that a classic image's LAZY pointer to a symbol it
# defines itself is translated to that definition, not its stub_helper (Portal 2
# client.dylib static init jumped to 0). Checks a real classic dylib by content;
# OFF arm is M64_NO_LAZY_SELF_DEF=1 at TRANSLATE time.
lazy-self-def:
	@bash lazy_self_def_test.sh
# pic-push-mem: A/B guard that a PIC-anchored `push disp(%ebx[,idx,s])` reads
# the translated slot (libbass pthread key). ON exits 42; OFF
# (M64_NO_PIC_OPERAND_LOWERING=1 at translate time) does not.
pic-push-mem: $(BUILD_DIR)/99_pic_push_mem.i386
	@r=$$(for arm in on off; do \
	  e=""; [ $$arm = off ] && e="M64_NO_PIC_OPERAND_LOWERING=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_pic_push_mem.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_pic_push_mem.$$arm; echo "$$arm=$$?"; \
	done 2>/dev/null | tr '\n' ' '); \
	case "$$r" in *"on=42 "*) case "$$r" in *"off=42"*) echo "pic-push-mem: FAIL ($$r)"; exit 1;; esac; echo "pic-push-mem: PASS";; *) echo "pic-push-mem: FAIL ($$r)"; exit 1;; esac
# cgl-fullscreen: A/B guard for the CGL fullscreen-context bridge
# (cgl_fullscreen_shim.m). ON exits 42; OFF (M64_NO_CGL_FULLSCREEN_BRIDGE=1 at
# run time) gets native 10012 and exits 7.
cgl-fullscreen: $(BUILD_DIR)/99_cgl_fullscreen.i386
	@bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	  -o $(BUILD_DIR)/99_cgl_fullscreen.ab $< >/dev/null 2>&1; \
	$(BUILD_DIR)/99_cgl_fullscreen.ab >/dev/null 2>&1; on=$$?; \
	M64_NO_CGL_FULLSCREEN_BRIDGE=1 $(BUILD_DIR)/99_cgl_fullscreen.ab >/dev/null 2>&1; off=$$?; \
	[ "$$on/$$off" = 42/7 ] && echo "cgl-fullscreen: PASS" || { echo "cgl-fullscreen: FAIL (on=$$on off=$$off)"; exit 1; }
# packed-pair-table: A/B guard that a table of packed (hi16,lo16) words whose
# members alias an attested zero-fill object stays constant (PvZ zlib lenfix ->
# "invalid distance too far back"). ON exits 42; OFF (M64_NO_PACKED_FAMILY=1 at
# translate time) relocates members and exits 1; u16off (M64_NO_U16_PAIR_RUN=1)
# leaves only the u16-array run (zlib dext) unprotected and exits 3.
packed-pair-table: $(BUILD_DIR)/99_packed_pair_table.i386
	@r=$$(for arm in on off u16off; do \
	  e=""; [ $$arm = off ] && e="M64_NO_PACKED_FAMILY=1"; [ $$arm = u16off ] && e="M64_NO_U16_PAIR_RUN=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_packed_pair_table.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_packed_pair_table.$$arm; echo "$$arm=$$?"; \
	done 2>/dev/null | tr '\n' ' '); \
	[ "$$r" = "on=42 off=1 u16off=3 " ] && echo "packed-pair-table: PASS" || { echo "packed-pair-table: FAIL ($$r)"; exit 1; }
# abigen-arg-extend: static guard that no generated bridge loads a char/short
# argument into a NARROW register (mov dil, byte [...]). The x86_64 caller must
# extend sub-32-bit args; GLEngine glColor4ub indexes with %esi (PvZ SIGBUS).
# Pre-fix abiconv.asm held such loads (OFF arm = the generator before 2026-09-27).
abigen-arg-extend:
	@n=$$(cat $(PROJ_ROOT)/build/src/abiconv/abiconv.asm $(PROJ_ROOT)/build/src/abiconv/abiconv_legacy.asm | \
	  grep -cE "^[[:space:]]+mov[[:space:]]+(dil|sil|dl|cl|r8b|r9b|di|si|dx|cx|r8w|r9w),[[:space:]]+(byte|word)"); \
	[ "$$n" = 0 ] && echo "abigen-arg-extend: PASS" || { echo "abigen-arg-extend: FAIL ($$n narrow arg loads)"; exit 1; }
# loop-ecx-count: static guard that every translated LOOP/LOOPE/LOOPNE/JECXZ
# carries 0x67 (x86_64 counts in RCX without it; i386 wraps ECX at 2^32). The
# runtime half is main-suite 99_loop_ecx_count. OFF arm = macho-tool before the
# fix (verified by hand: 5 bare e0-e3 forms, jrcxz).
loop-ecx-count: $(BUILD_DIR)/99_loop_ecx_count.x86_64
	@n=$$(objdump -d $<.dylib | grep -cE "\b(loop|loope|loopne|jrcxz|jecxz)\b"); \
	b=$$(objdump -d $<.dylib | grep -E "\b(loop|loope|loopne|jrcxz|jecxz)\b" | grep -cv "\b67 e[0-3]"); \
	[ "$$n" = 5 ] && [ "$$b" = 0 ] && echo "loop-ecx-count: PASS" || { echo "loop-ecx-count: FAIL ($$b of $$n without 0x67)"; exit 1; }
# pthread-attr-size: guard that abigen marshals Darwin's arch-sized pthread
# opaque bodies at the i386 size (attr 36, not 56). OFF arm = a libabiconv from
# before the fix (verified by hand: exits 1, canary overwritten).
pthread-attr-size: $(BUILD_DIR)/99_pthread_attr_size.i386
	@bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	  -o $(BUILD_DIR)/99_pthread_attr_size.on $< >/dev/null 2>&1; \
	$(BUILD_DIR)/99_pthread_attr_size.on; r=$$?; \
	[ $$r = 42 ] && echo "pthread-attr-size: PASS" || { echo "pthread-attr-size: FAIL ($$r)"; exit 1; }
# heap-below-2g: guard that malloc pointers are positive as signed int32 (real
# i386 Darwin's heap sat below 2 GB; PvZ range-checks Image* signed). OFF arm =
# M64_HEAP_HIGH_FIRST=1 at run time.
heap-below-2g: $(BUILD_DIR)/99_heap_below_2g.i386 $(BUILD_DIR)/libabiconv.dylib
	@bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	  -o $(BUILD_DIR)/99_heap_below_2g.on $< >/dev/null 2>&1; \
	$(BUILD_DIR)/99_heap_below_2g.on; on=$$?; \
	M64_HEAP_HIGH_FIRST=1 $(BUILD_DIR)/99_heap_below_2g.on; off=$$?; \
	[ $$on = 42 ] && [ $$off = 1 ] && echo "heap-below-2g: PASS" || { echo "heap-below-2g: FAIL (on=$$on off=$$off)"; exit 1; }
# classic-reloc-accept: a slot with a classic local reloc is a pointer even when
# its target has no entry evidence (PvZ libbass {fn,fn} table). OFF arm is
# M64_NO_CLASSIC_RELOC_ACCEPT=1 at TRANSLATE time (faults).
classic-reloc-accept: $(BUILD_DIR)/99_classic_reloc_accept.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_NO_CLASSIC_RELOC_ACCEPT=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_classic_reloc_accept.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_classic_reloc_accept.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "classic-reloc-accept: PASS" || { echo "classic-reloc-accept: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# export-trie-deep: a >255-char export must not make the translated image's
# export trie deeper than ld accepts (Angry Birds). OFF arm is
# M64_NO_TRIE_EDGE_COMPRESS=1 at TRANSLATE time (the link fails, no binary).
export-trie-deep: $(BUILD_DIR)/99_export_trie_deep.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_NO_TRIE_EDGE_COMPRESS=1"; \
	  rm -f $(BUILD_DIR)/99_export_trie_deep.$$arm; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_export_trie_deep.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_export_trie_deep.$$arm 2>/dev/null; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "export-trie-deep: PASS" || { echo "export-trie-deep: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# entry-pad-fallthrough: the even-address pad before a function entry must be
# executable (a NOP): code may fall through into the label (Portal 2 libbink
# RADCB). OFF arm is M64_ZERO_ENTRY_PAD=1 at TRANSLATE time (the old 0x00 pad).
entry-pad-fallthrough: $(BUILD_DIR)/99_entry_pad_fallthrough.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_ZERO_ENTRY_PAD=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_entry_pad_fallthrough.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_entry_pad_fallthrough.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "entry-pad-fallthrough: PASS" || { echo "entry-pad-fallthrough: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# disp-reloc-accept: an instruction disp32 with a classic local reloc is an
# address even in a relocatable image (Portal 2 libbink `leal 0x30e60(%edx)`
# into __bss). OFF arm is M64_NO_DISP_RELOC_ACCEPT=1 at TRANSLATE time.
disp-reloc-accept: $(BUILD_DIR)/99_disp_reloc_accept.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_NO_DISP_RELOC_ACCEPT=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_disp_reloc_accept.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_disp_reloc_accept.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "disp-reloc-accept: PASS" || { echo "disp-reloc-accept: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# imm-reloc-accept: an instruction imm32 with a classic local reloc is an
# address in any shape (Portal 2 libbink `pushl $0x30aa`, its IO callback).
# OFF arm is M64_NO_IMM_RELOC_ACCEPT=1 at TRANSLATE time.
imm-reloc-accept: $(BUILD_DIR)/99_imm_reloc_accept.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_NO_IMM_RELOC_ACCEPT=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_imm_reloc_accept.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_imm_reloc_accept.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "imm-reloc-accept: PASS" || { echo "imm-reloc-accept: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# call-abs-pagezero: `call *0x10` (a folded call through a NULL object) must
# translate (it aborted on the width guard) to an ABSOLUTE load that faults at
# 0x10 -- not the rip-relative reading of the same bytes (Portal 2 client/server).
call-abs-pagezero: $(BUILD_DIR)/99_call_abs_pagezero.x86_64
	@out=$$(M64_FAULT_REPORT=1 $(BUILD_DIR)/99_call_abs_pagezero.x86_64 2>&1); \
	echo "$$out" | grep -q 'signal=11.*fault addr=0x10$$' && echo "call-abs-pagezero: PASS" || \
	{ echo "$$out" | grep -E 'signal=|rip ' ; echo "call-abs-pagezero: FAIL"; exit 1; }
# pic-anchor-call-adopt: at a branch target right after a CALL, a caller-saved
# anchor comes from the branch (Portal 2 client `call _Unwind_Resume` then a
# je target). OFF arm is M64_NO_PIC_ANCHOR_CALL_ADOPT=1 at TRANSLATE time.
pic-anchor-call-adopt: $(BUILD_DIR)/99_pic_anchor_call_adopt.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_NO_PIC_ANCHOR_CALL_ADOPT=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_pic_anchor_call_adopt.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_pic_anchor_call_adopt.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "pic-anchor-call-adopt: PASS (on=$$r_on off=$$r_off)" || { echo "pic-anchor-call-adopt: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# jt-esp-spill-push: a PIC table base spilled to an %esp slot survives pushes,
# a call and `add $imm,%esp` (PvZ libbass MOD switch). OFF arm is
# M64_NO_JT_ESP_TRACK=1 at TRANSLATE time.
jt-esp-spill-push: $(BUILD_DIR)/99_jt_esp_spill_push.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_NO_JT_ESP_TRACK=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_jt_esp_spill_push.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_jt_esp_spill_push.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 0 ] && [ $$r_off != 0 ] && echo "jt-esp-spill-push: PASS" || { echo "jt-esp-spill-push: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# call-mem-esp: `call *disp(%esp)` loads its target before pushing the
# return address (PvZ libbass). OFF arm M64_CALL_MEM_LOAD_AFTER_PUSH=1.
call-mem-esp: $(BUILD_DIR)/99_call_mem_esp.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_CALL_MEM_LOAD_AFTER_PUSH=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_call_mem_esp.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_call_mem_esp.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 0 ] && [ $$r_off != 0 ] && echo "call-mem-esp: PASS" || { echo "call-mem-esp: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# memdisp-entry-store: a member store whose disp equals a function entry stays
# an offset (PvZ 0x558c). OFF arm M64_MEMDISP_ACCESS_ENTRY_RULE=1.
memdisp-entry-store: $(BUILD_DIR)/99_memdisp_entry_store.i386 $(BUILD_DIR)/libabiconv.dylib
	@for arm in on off; do E=""; [ $$arm = off ] && E="M64_MEMDISP_ACCESS_ENTRY_RULE=1"; \
	  env $$E bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_memdisp_entry_store.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_memdisp_entry_store.$$arm; eval r_$$arm=$$?; done; \
	[ $$r_on = 42 ] && [ $$r_off != 42 ] && echo "memdisp-entry-store: PASS" || { echo "memdisp-entry-store: FAIL (on=$$r_on off=$$r_off)"; exit 1; }
# qsort-ptr-elems: an i386 qsort comparator only ever sees pointers into the
# caller's array — libc's heapsort fallback compares against >4GB temporaries
# (PvZ sound sort). OFF arm M64_NO_QSORT_SHIM=1 at RUN time.
qsort-ptr-elems: $(BUILD_DIR)/99_qsort_ptr_elems.i386 $(BUILD_DIR)/libabiconv.dylib
	@bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	  -o $(BUILD_DIR)/99_qsort_ptr_elems.on $< >/dev/null 2>&1; \
	$(BUILD_DIR)/99_qsort_ptr_elems.on; on=$$?; \
	M64_NO_QSORT_SHIM=1 $(BUILD_DIR)/99_qsort_ptr_elems.on; off=$$?; \
	[ $$on = 42 ] && [ $$off != 42 ] && echo "qsort-ptr-elems: PASS" || { echo "qsort-ptr-elems: FAIL (on=$$on off=$$off)"; exit 1; }
# pthread-stack-np: guard that the pthread_t-taking _np introspection calls
# resolve the i386 thread token (stack answers = the i386 stack in use). OFF
# arm = the previous libabiconv (verified by hand: faults dereferencing the token).
pthread-stack-np: $(BUILD_DIR)/99_pthread_stack_np.i386
	@bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	  -o $(BUILD_DIR)/99_pthread_stack_np.on $< >/dev/null 2>&1; \
	$(BUILD_DIR)/99_pthread_stack_np.on; r=$$?; \
	[ $$r = 42 ] && echo "pthread-stack-np: PASS" || { echo "pthread-stack-np: FAIL ($$r)"; exit 1; }
# setjmp-frame: guard that setjmp/longjmp from translated code use the i386
# 72-byte jmp_buf (libabiconv capture) instead of native _setjmp's 148-byte
# write over the caller's frame (PvZ saved-esi clobber). Runtime fix: the OFF
# arm is a libabiconv built before the fix (verified once by hand: faults).
setjmp-frame: $(BUILD_DIR)/99_setjmp_frame.i386
	@bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	  -o $(BUILD_DIR)/99_setjmp_frame.on $< >/dev/null 2>&1; \
	$(BUILD_DIR)/99_setjmp_frame.on; r=$$?; \
	[ $$r = 42 ] && echo "setjmp-frame: PASS" || { echo "setjmp-frame: FAIL ($$r)"; exit 1; }
# prologue-sched-entry: A/B guard that a vtable slot aimed at a function whose
# frame setup has a scheduled instruction inside it is relocated (PvZ). ON
# exits 42; OFF (M64_STRICT_PROLOGUE=1 at translate time) faults.
prologue-sched-entry: $(BUILD_DIR)/99_prologue_sched_entry.i386
	@for arm in on off; do \
	  e=""; [ $$arm = off ] && e="M64_STRICT_PROLOGUE=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_prologue_sched_entry.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_prologue_sched_entry.$$arm; echo "$$arm=$$?"; \
	done 2>/dev/null | tr '\n' ' ' | grep -q "on=42 off=1[0-9][0-9]" && echo "prologue-sched-entry: PASS" || { echo "prologue-sched-entry: FAIL"; exit 1; }
# cstring-tailmerge-ptr: A/B guard that a string-pointer table entry aimed at
# a tail-merged cstring INTERIOR is relocated (PvZ build-info pointer). ON
# exits 0; OFF (M64_NO_CSTR_NEIGHBOUR=1 at translate time) exits 8.
cstring-tailmerge-ptr: $(BUILD_DIR)/99_cstring_tailmerge_ptr.i386
	@for arm in on off; do \
	  e=""; [ $$arm = off ] && e="M64_NO_CSTR_NEIGHBOUR=1"; \
	  env $$e bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) \
	    -o $(BUILD_DIR)/99_cstring_tailmerge_ptr.$$arm $< >/dev/null 2>&1; \
	  $(BUILD_DIR)/99_cstring_tailmerge_ptr.$$arm; echo "$$arm=$$?"; \
	done | tr '\n' ' ' | grep -q "on=0 off=8" && echo "cstring-tailmerge-ptr: PASS" || { echo "cstring-tailmerge-ptr: FAIL"; exit 1; }
# objc-imp-entry: A/B guard that an ObjC1 method IMP with no symbol and no
# frame prologue is relocated (iPhoto's stripped method lists). OFF arm is
# M64_NO_OBJC_IMP_ENTRY=1 at translate time.
objc-imp-entry: $(BUILD_DIR)
	@CC="$(CC_I386_OBJC)" LD="$(LD)" LDF="$(LDFLAGS_I386_OBJC)" bash objc_imp_entry_test.sh
# seltypes-kind: A/B guard that a legacy class's encoding for a selector never
# changes a NATIVE method's arg kind (objc_shim.c seltypes_refines). OFF arm is
# M64_NO_SELTYPES_KIND_GUARD=1 at run time.
seltypes-kind: $(BUILD_DIR)
	@CC="$(CC_I386_OBJC)" LD="$(LD)" LDF="$(LDFLAGS_I386_OBJC)" bash seltypes_kind_test.sh
# zerofill-stripped-ptr: A/B guard that a __data pointer into the zero-fill
# section of a locals-stripped image is relocated (iPhoto's gettimeofday slot).
# OFF arm is M64_ZF_GATE_STRIPPED=1 at TRANSLATE time.
zerofill-stripped-ptr: $(BUILD_DIR)/99_zerofill_stripped_ptr.i386
	@bash zerofill_stripped_ptr_test.sh
# zerofill-target-pair: A/B guard that the translator does not "rebase" two
# adjacent u16 fields whose bytes spell an address into ZERO-FILL memory. Halo CE
# died on exactly this every run: 'scen' (540,72) -> (27356,4272). The poisonous
# value must equal a link-time address, so the script patches it into the built
# i386 binary and translates the same bytes twice. OFF arm is
# M64_NO_ZEROFILL_TARGET_GATE=1 at TRANSLATE time.
zerofill-target-pair: $(BUILD_DIR)/99_zerofill_target_pair.i386
	@bash zerofill_target_pair_test.sh
# zerofill-code-literal: A/B guard that a __data pointer into symbol-free
# zero-fill space is relocated when the code addresses that object absolutely
# (PvZ's global-pointer table). OFF arm is M64_NO_ZF_CODE_LITERAL=1.
zerofill-code-literal: $(BUILD_DIR)/99_zerofill_code_literal.i386
	@bash zerofill_code_literal_test.sh
# jt-spill-slot: A/B guard that a PIC relative-offset switch jump table whose
# TABLE BASE IS SPILLED TO A FRAME SLOT is still recognised and relocated.
# DetectJumpTables tracked the base only through registers, lost it across the
# spill, and left the table with stale i386 deltas while the linear sweep
# disassembled its entries AS CODE. Civ IV "Init Python": Python 2.6's marshal
# r_object switch (126 entries) is this shape — 27 of Python's 99 PIC dispatches
# spill. Fixture carries a CONTROL dispatch (base kept in a register) that must
# stay correct in BOTH arms. OFF arm is M64_NO_JT_SPILL_SLOTS=1 at TRANSLATE time.
jt-spill-slot: $(BUILD_DIR)/99_jt_spill_slot.i386 $(BUILD_DIR)/libabiconv.dylib
	@bash jt_spill_slot_test.sh
# record-field-pair: A/B guard for the SAME u16-pair defect when the address the
# pair spells is MAPPED, so the zero-fill anchor argument above does not apply —
# a __DATA,__data target (no discriminator existed) and a __TEXT,__text target
# held in __TEXT,__const (where the code-entry gates are disarmed for switch jump
# tables). Halo CE task #35: (0,12,2,3) -> (0,12,44161,4099). Evidence is
# POSITIONAL, so the fixture also carries the two shapes that MUST still
# relocate: a jump-table run and Quinn's 32-byte pointer record. The poisonous
# value must equal a link-time address, so the script patches it into the built
# i386 binary and translates the same bytes twice. OFF arm is
# M64_NO_RECORD_FIELD_GATE=1 at TRANSLATE time.
record-field-pair: $(BUILD_DIR)/99_record_field_pair.i386
	@bash record_field_pair_test.sh
# const-alias-output-image: A/B guard that the M64 RE-PARSE does not reclassify
# an integer CONSTANT as a pointer just because its value aliases the
# 0x10000000-based TRANSLATED image. Mirror of zerofill-target-pair: there a
# (small,small) u16 pair looks like an address because the SOURCE image is
# small; here any 0x10-prefixed constant looks like one because the OUTPUT image
# is based at 0x10000000. Measured: Civ IV Steam 738, iPhoto 143, iMovie 10,
# Halo CE 1. Unlike that guard the poison needs no patching — the aliased image
# is the OUTPUT one, so it is a plain C initialiser. OFF arm is
# M64_NO_CONST_PIN=1 at RE-PARSE time.
const-alias-output-image: $(BUILD_DIR)/100_const_alias_output_image.i386
	@bash const_alias_output_image_test.sh
# carbon-alias: A/B guard for the classic ALIAS MANAGER. NULL-jump orphans: the
# legacy pass emits a bridge to a native Apple deleted from 64-bit Carbon in 2009.
# ★The load-bearing assertion is FOLLOWED_RENAME / FOLLOWED_MOVE — a stored path
# string cannot survive either, and neither can the surviving native
# FSResolveAlias (measured: the CNID search is gone from modern CarbonCore), so
# this is what proves a real alias. OFF arm is ABICONV_ALIAS_LEGACY=1.
carbon-alias: $(BUILD_DIR)/99_carbon_alias.x86_64
	@bash carbon_alias_test.sh
# gap-stub: a constant-return STUB shim reports its first hit (stderr + reach
# ledger, src/abiconv/gap.c); an .ok-listed shim stays quiet; M64_GAP=0 silences.
gap-stub: $(BUILD_DIR)/99_gap_stub.x86_64
	@bash gap_stub_test.sh
# gap-raw: a translated call to a NATIVE function with no bridge reports its
# first hit (gap.c's one-shot stub on the bound slot); M64_GAP=0 arms nothing.
gap-raw: $(BUILD_DIR)/99_gap_raw.x86_64
	@bash gap_raw_test.sh
# display-point-modes: every display-mode list describes the desktop in POINTS and
# only lists sizes that fit -- the pre-Mojave Retina contract (headless).
display-point-modes: $(BUILD_DIR)/99_display_point_modes.x86_64
	@bash display_point_modes_test.sh
# heap-guard: M64_HEAP_GUARD=1 (malloc_shim.c) faults AT a heap overrun or
# use-after-free write; without it the same writes land silently.
heap-guard: $(BUILD_DIR)/99_heap_guard.x86_64
	@bash heap_guard_test.sh
# memerr: MemError() reports the result of our Memory Manager shims (a refused
# SetPtrSize grow reads back as an error; Halo corrupted its heap without it).
memerr: $(BUILD_DIR)/99_memerr.x86_64
	@bash memerr_test.sh
# event-params: Carbon event parameters with an ABI-dependent layout (refs,
# CGFloat geometry, HICommand) cross intact (carbon_event_param_shim.c).
event-params: $(BUILD_DIR)/99_event_params.x86_64
	@bash event_params_test.sh
# carbon-datetime: A/B guard for the classic Date & Time Utilities, REMOVED from
# modern macOS. abigen still emits a bridge; the pipeline weakens the dangling
# bind to NULL, so the classic call jumps to 0 (Halo, rip=0, 2026-08-03).
carbon-datetime: $(BUILD_DIR)/99_carbon_datetime.x86_64
	@bash carbon_datetime_test.sh
# fscatinfo-bulk: A/B guard for the classic BULK directory enumeration.
# FSCatalogInfo is the one classic struct whose layouts genuinely differ (144
# i386 / 148 x86_64 -- FSPermissionInfo ends in an FSFileSecurityRef POINTER),
# so abigen deep-copies it through a scratch slot sized for ONE struct. That is
# right for FSGetCatalogInfo and fatal for FSGetCatalogInfoBulk, which fills
# `actualObjects` entries: the native callee overruns the shim frame straight
# into the i386 caller's locals (measured on Civ IV 2026-08-05 -- SIGSEGV with
# the caller's two just-null-checked `operator new[]` pointers reading back 0).
# The discriminator is the ARRAY, not the crash: every entry beyond index 0 must
# come back carrying its own file's size.
fscatinfo-bulk: $(BUILD_DIR)/99_fscatinfo_bulk.x86_64
	@bash fscatinfo_bulk_test.sh
# csmem-handle: A/B guard for CarbonCore's CSMem* Memory Manager twins. A classic
# Handle is Ptr* (4 bytes on i386); the CSMem spelling is what CarbonCore and the
# classic QuickTime.framework call internally, and it was never shimmed, so it
# bound to the native 64-bit Memory Manager -- >4GB Handles truncating into the
# i386 slot on the way out, and i386 Handles read as 8 bytes on the way in
# (measured: free() of 0x0000_0037_9000_ff9c aborted Civ IV). The ON side is the
# main-suite fixture 99_csmem_handle_family; this adds the M64_NO_CSMEM_SHIM=1
# side and fails if the arms do not differ.
csmem-handle: $(BUILD_DIR)/99_csmem_handle_family.x86_64
	@bash csmem_handle_test.sh
# mp-allocate-buffer: A/B guard for Carbon Multiprocessing Services' allocator.
# MPAllocateAligned returns LogicalAddress = `typedef void *`, so abigen's
# generic void*-return rule wrapped its >4GB native pointer into an 8-byte PROXY
# ARENA HANDLE -- the caller asked for N bytes and got an arena slot. MEASURED on
# Halo CE (2026-08-08): consecutive 64KB requests came back 8 BYTES APART, Halo
# wrote audio data over the live handle table, and the memset bridge unwrapped
# the "buffer" back through the arena to 0 -> SIGSEGV writing to 0x0. The ON side
# is the main-suite fixture 99_mp_allocate_buffer; this adds the
# M64_NO_MP_LOW_HEAP=1 side and fails if the arms do not differ.
mp-allocate-buffer: $(BUILD_DIR)/99_mp_allocate_buffer.x86_64
	@bash mp_allocate_buffer_test.sh
# snd-lock-order: A/B guard for the Sound Manager's AudioQueue LOCK-ORDERING
# rule. AudioQueueReset / Stop(...,true) / Dispose(...,true) all block in
# AQ::API::Queue::AwaitAllPendingCallbacks, and our completion callback
# aq_output_cb takes c->lk on entry -- so making any of them while HOLDING c->lk
# parks the caller against its own callback forever. MEASURED on Halo CE
# (2026-08-10): Return on a main-menu item issues flushCmd and 3364/3364 sample
# frames had the main thread in shim_SndDoImmediate -> do_command ->
# AudioQueueReset -> AwaitAllPendingCallbacks -> pthread_cond_wait; the game
# read as "frozen" while still rendering. The fixture queues six seconds of
# INAUDIBLE PCM (constant 0x80 + ampCmd 0) so the callbacks are genuinely
# pending, then flushes; the M64_NO_SND_LOCK_FIX=1 arm must hang and be killed
# by the script's watchdog.
snd-lock-order: $(BUILD_DIR)/99_snd_lock_order.x86_64
	@bash snd_lock_order_test.sh
# snd-cb-reentrancy: A/B guard for the Sound Manager's DELIVERY CONTRACT -- a
# QUEUED callback must never run on the caller's stack. SndDoCommand queues; the
# Sound Manager executes later on its own context, so a callBackCmd cannot
# re-enter the app from inside the app's own SndDoCommand call. Firing inline
# invents a re-entrancy the real API cannot produce, and any app holding a
# non-recursive lock across SndDoCommand (every double-buffering app) then
# deadlocks against itself. MEASURED on Halo CE (2026-08-19): its SndCallBackProc
# (i386 0x2d80fa) holds a voice semaphore across SndDoCommand(bufferCmd) +
# SndDoCommand(callBackCmd); 0x2d8127 is the ONLY 500 ms MPWaitOnSemaphore in the
# binary, and the inline fire made it wait on a semaphore its own outer frame
# held -- the nested wait burned the deadline, returned kMPTimeoutErr and took
# the early-out that SKIPS THE REFILL. M64_NO_SND_CB_DEFER=1 is the OFF arm and
# must reproduce the re-entry; a clean OFF arm fails the guard as inert.
snd-cb-reentrancy: $(BUILD_DIR)/99_snd_cb_reentrancy.x86_64
	@bash snd_cb_reentrancy_test.sh
# snd-cb-fifo: A/B guard that every callBackCmd fires, in order, once the
# buffers queued before it have played (Portal 2 Bink intro audio crackle).
# OFF arm is M64_NO_SND_CB_FIFO=1 at RUN time.
snd-cb-fifo: $(BUILD_DIR)/99_snd_cb_fifo.x86_64
	@bash snd_cb_fifo_test.sh
# snd-status-layout: A/B guard for the classic SCStatus ABI. Sound.h defines
# scStartTime@0 scEndTime@4 scCurrentTime@8 scChannelBusy@12 scChannelDisposed@13
# scChannelPaused@14 scUnused@15 scChannelAttributes@16 scCPULoad@20, sizeof 24.
# The old shim_SndChannelStatus did memset(theStatus,0,28) and wrote the flags at
# @24/@26, so it BOTH overran a 24-byte caller record by 4 bytes AND left
# scChannelBusy@12 permanently 0 -- every classic "wait until this channel
# drains" / "find a free voice" loop is told every channel is idle forever.
# MEASURED from Halo CE's i386 original: IDirectSoundBuffer_Mac::Play @0x2d93a7
# scans 47 channels for a non-busy one and IDirect Sound_Mac::GetCaps @0x2d8362
# counts free voices in a 48-slot loop -- both pass theLength=0x18 and read the
# flag at +0x0C. The fixture puts a canary immediately after a 24-byte SCStatus;
# the overrun half needs NO audio device (the shim clears the record before it
# looks the channel up), so this guard cannot go inert on a headless host.
snd-status-layout: $(BUILD_DIR)/98_snd_status_layout.x86_64
	@bash snd_status_layout_test.sh
# jt-cohesion: A/B guard for the JUMP-TABLE COHESION gate. The func-entry and
# code-entry gates are deliberately disarmed for __TEXT-resident data (switch
# jump tables live in __TEXT,__const and target unsymboled basic-block heads),
# leaving only code_interior_alias -- so a u32 INTEGER aliasing an exact
# instruction BOUNDARY passes every gate and is rebased. MEASURED on Halo CE:
# the sample-rate table {22050, 44100} became {0x1000573c, 44100}, overflowing
# the mixer's ring-size arithmetic so IDirectSoundBuffer::Lock returned
# E_INVALIDARG and the mixer silently skipped -- 7432 buffers of pure silence.
# ★The gate runs at TRANSLATE time, so the script translates BOTH arms itself
# (an .x86_64 already built cannot be A/B'd by an env var at run time). It only
# needs the .i386 from the normal rule.
jt-cohesion: $(BUILD_DIR)/96_jt_cohesion.i386
	@bash jt_cohesion_test.sh
# qword-high-skip: A/B guard for the QWORD-HIGH-HALF skip inside that same walk.
# An M32 image has no 8-byte pointers, so a const array of 64-bit INTEGERS is
# 8-aligned {lo, hi} pairs whose small entries put a ZERO in every odd word. The
# walk treated that zero as an OBJECT EDGE and stopped on it, so a low half that
# landed on an exact instruction BOUNDARY passed every gate and was rebased.
# MEASURED on Quinn: the u64 knapsack vector at __TEXT,__const 0xb2c60 that
# obfuscates the saved highscore file had entries 7 (0x29a5) and 9 (0xbd3c)
# rewritten to translated code addresses; 63 of the 102 stored records then
# decrypted to garbage and every highscore was lost on restart.
# ★The gate runs at TRANSLATE time, so the script translates all three arms
# itself. It needs the .i386 and the staged libabiconv from the normal rules.
# openssl-shim: guard for openssl_shim.c. A legacy app's libcrypto 0.9.7 calls
# (SHA1 / HMAC(EVP_sha1()) / RAND_pseudo_bytes) must reach the CommonCrypto
# shims; unshimmed, RAND_pseudo_bytes was a raw cross-ABI call (fused return PC
# -> crash when Quinn's server answered REQ_SERVER_INFO). Links against a
# built stub with libcrypto.0.9.7's install name. Checks the shims are bound
# (by content) and correct (SHA1/HMAC test vectors); the classic-jump-table
# crash itself cannot be rebuilt with ld64-95 (see the script).
openssl-shim: $(BUILD_DIR) $(BUILD_DIR)/libabiconv.dylib $(MACHO_TOOL) $(BUILD_DIR)/99_openssl_shim.i386
	@bash openssl_shim_test.sh

# Link-time stand-in for /usr/lib/libcrypto.0.9.7.dylib (the real one refuses
# direct linking). Same install name, so 99_openssl_shim imports exactly like
# Quinn. Overrides %.i386 for the fixture.
$(BUILD_DIR)/libcrypto.0.9.7.dylib: | $(BUILD_DIR)
	printf 'void SHA1(void){} void EVP_sha1(void){} void HMAC(void){} void RAND_pseudo_bytes(void){}\n' > $(BUILD_DIR)/crypto_stub.c
	$(CC_I386) -c $(BUILD_DIR)/crypto_stub.c -o $(BUILD_DIR)/crypto_stub.o
	$(LD) -arch i386 -dylib -macos_version_min 10.6 -syslibroot $(I386_SYSROOT) -lSystem \
	    -install_name /usr/lib/libcrypto.0.9.7.dylib -o $@ $(BUILD_DIR)/crypto_stub.o
$(BUILD_DIR)/99_openssl_shim.i386: $(BUILD_DIR)/99_openssl_shim.o $(BUILD_DIR)/libcrypto.0.9.7.dylib
	$(LD) $(LDFLAGS_I386) $(BUILD_DIR)/libcrypto.0.9.7.dylib -e _main -o $@ $<

qword-high-skip: $(BUILD_DIR)/99_qword_high_skip.i386 $(BUILD_DIR)/libabiconv.dylib
	@bash qword_high_skip_test.sh
# native-free: A/B guard for the malloc shim handing NATIVE allocations back to
# their zone (free / realloc / malloc_size; malloc_shim.c). Native x86_64.
native-free: $(LIBABICONV)
	@bash native_free_test.sh
# fault-thread-altstack: A/B guard for the fault reporter's per-thread alternate
# signal stack (fault_report_shim.c fr_thread_hook). Native x86_64.
fault-thread-altstack: $(LIBABICONV)
	@bash fault_thread_altstack_test.sh
# pgmemo-saturation: A/B guard for the bounded probe window of libabiconv's
# page-readability memo (objc_shim.c page_readable_len; Quinn crawled after a
# long session). Native x86_64, dlopens the real libabiconv.
pgmemo-saturation: $(LIBABICONV)
	@bash pgmemo_saturation_test.sh $(LIBABICONV)
# pic-anchor-entry-save: A/B guard for the PIC-anchor ENTRY-SAVE exemption
# (section.cc DetectPicAnchoredDisps step 2b); translates both arms itself.
pic-anchor-entry-save: $(BUILD_DIR)/99_pic_anchor_entry_save.i386 $(BUILD_DIR)/libabiconv.dylib
	@bash pic_anchor_entry_save_test.sh
# literal-abs-disp: A/B guard for the LITERAL-POOL absolute-displacement gate.
# i386 [abs32] and x86_64 [rip+disp32] share an encoding, so every absolute
# memory reference must be rewritten at translate time; the gate deciding that
# enumerated data section TYPES and so excluded __literal4/__literal8/__cstring.
# Found by a whole-image pcmap audit of Halo CE (4 float compares reading
# unrelated memory). ★The gate runs at TRANSLATE time, so the script translates
# both arms itself; it only needs the .i386 from the normal rule.
literal-abs-disp: $(BUILD_DIR)/99_literal_abs_disp.i386
	@bash literal_abs_disp_test.sh
# classic-input-coords: A/B guard for the classic GLOBAL-vs-LOCAL coordinate
# contract. GetMouse is defined against the CURRENT PORT and GetGlobalMouse
# against the SCREEN, with GlobalToLocal/LocalToGlobal converting between them;
# our old stubs answered GLOBAL to both and made both conversions identity
# no-ops -- correct only for an origin-(0,0) offscreen port. On a real window an
# app that hit-tests its own menu with GetMouse compares a SCREEN point against
# WINDOW-LOCAL item rects and finds the pointer inside nothing: the cursor moves
# and nothing is clickable. The fixture makes a real Carbon window at a non-zero
# position and asserts both arms positively. It briefly SHOWS that window, so do
# not run the suite while a GUI target is under live test.
classic-input-coords: $(BUILD_DIR)/99_classic_input_coords.x86_64
	@bash classic_input_coords_test.sh
# mp-task-spawn: PROBE (not a two-arm guard -- there is no fix behind it). It
# records a measured NEGATIVE result for Halo #42: an i386 caller going through
# our bridges CAN create an MP queue/semaphore/event and an MP TASK, the task
# entry really executes via the _x64_cb_wrap reverse trampoline with its
# parameter intact, and that holds for default and 64KB/512KB/1MB stack sizes.
# Keeping it runnable means the day MP tasking DOES break, we find out here
# instead of inside a hanging app. Halo's main-menu freeze is therefore NOT a
# generic MP Services bridge defect.
mp-task-spawn: $(BUILD_DIR)/99_mp_task_spawn.x86_64
	@./$(BUILD_DIR)/99_mp_task_spawn.x86_64
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

# dptr-table: the exact 4-byte DATA pointer-slot table (__DATA,__86x64_dptr,
# Archive::inject_dptr_section) replaces slide_data_fnptrs's value scan, which
# slid window-aliasing integer constants and missed unaligned-data pointers.
# Both arms: OFF (DPTR_OFF=1, no table) must still show the scan's errors.
dptr-table: $(MACHO_TOOL)
	@bash dptr_table_test.sh $(MACHO_TOOL)

# init-slot-rebase: synthesize_dyld_info rebases every in-image 8-byte slot per
# slot, not only when the input's rebase table was empty (a non-PIE C++ exec
# reaches convert with transform's lazy-pointer rebases, and its widened
# __mod_init_func slots used to get none). Shape arm + fix arm.
init-slot-rebase: $(MACHO_TOOL)
	@bash init_slot_rebase_test.sh $(MACHO_TOOL)

# dyld-section-width: a classic 8-byte __DATA,__dyld is padded to 16 bytes so
# the host dyld's 8-byte func_lookup write at +8 stays inside it.
dyld-section-width: $(MACHO_TOOL)
	@bash dyld_section_width_test.sh $(MACHO_TOOL)

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

# fsspec-resolve: regression guard for REAL FSSpec resolution + the OUTPUT-BUFFER
# invariant (src/abiconv/carbon_fsspec_shim.c). The FSSpec generation of the Carbon
# File Manager was deleted from 64-bit macOS, and the old one-line shims returned
# fnfErr WITHOUT writing their out-params -- so Halo's 0x2da50c
#   FSpMakeFSRef -> FSRefMakePath -> CFStringCreateWithCString -> CFStringGetCString
# (every OSErr ignored, as classic code does) read its own uninitialised stack as an
# FSRef, then as a path, and CFStringCreateWithCString legitimately returned NULL for
# non-UTF8 bytes -> nil CFStringRef -> EXC_BAD_ACCESS at 0x0. A heisenbug: any observer
# perturbs the stack garbage into valid UTF-8. Asserts (a) Halo's exact chain
# FindFolder -> FSpMakeFSRef -> FSRefMakePath lands on the native folder path, (b) the
# real-CNID (/.vol/<dev>/<inode>) branch and a named leaf, (c) FSMakeFSSpec always fills
# the spec, and (d) an unchecked-error caller NEVER sees an undefined output buffer.
# Runs both arms of the ABICONV_FSSPEC_LEGACY kill switch and requires them to differ.
# Native x86_64; no i386 sysroot needed.
fsspec-resolve:
	@bash fsspec_resolve_test.sh

# cfbundle-fnptr-shim: regression guard for shim_CFBundleGetFunctionPointerForName
# (src/abiconv/posix_shim.c). Halo CE has NO `_fopen` import at all; it resolves
# fopen at run time via CFBundleGetFunctionPointerForName(bundle, CFSTR("fopen"))
# and calls it through a pointer. The generic marshalling thunk got the CALL right
# but returned the native >4GB FILE* truncated into %eax, so Halo's next imported
# `_ftell` reached ___ftell.l1 -> ftell -> resolve_file -> is_shim and dereferenced
# fp+8 on an address in no VM region (SIGSEGV at 0x59d56d88, rip=_is_shim+0x4e).
# libabiconv's own `_fopen` interpose wraps the FILE* in a low-4GB shim_FILE; this
# CF twin was the only by-name-lookup shim that did not prefer that interpose (as
# shim_dlsym and ns_symbol_resolve already do). Asserts the shim returns ___fopen
# and that its result is a low-4GB shim FILE* that ftell accepts. Runs both arms of
# M64_NO_CFBUNDLE_FNPTR_SHIM and requires them to differ. Native x86_64.
cfbundle-fnptr-shim: $(LIBABICONV)
	@bash cfbundle_fnptr_shim_test.sh

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

# standard-alert: StandardAlert/CreateStandardAlert/RunStandardAlert must be a
# clickable, movable modal (routed through NSAlert, carbon_standardalert_shim.m).
standard-alert:
	@bash carbon_standardalert_test.sh

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
# `-undefined dynamic_lookup`: clang defines a class's `.objc_class_name_X` as an
# absolute symbol, but ld64-95 resolves a same-file `__cls_refs` literal only
# against a real atom, so any fixture that messages its OWN class failed to link
# ("undefined .objc_class_name_X"). The reference emits no bind either way.
objc: $(BUILD_DIR) $(BUILD_DIR)/libabiconv.dylib $(MACHO_TOOL)
	@pass=0; fail=0; \
	for t in $(OBJC_TESTS); do \
	  $(CC_I386_OBJC) -c $(SRC_DIR)/$$t.m -o $(BUILD_DIR)/$$t.o 2>/dev/null || { echo "FAIL $$t (compile)"; fail=$$((fail+1)); continue; }; \
	  $(LD) $(LDFLAGS_I386_OBJC) -undefined dynamic_lookup -e _main -o $(BUILD_DIR)/$$t.i386 $(BUILD_DIR)/$$t.o 2>/dev/null || { echo "FAIL $$t (link — run 'make sysroot-objc'?)"; fail=$$((fail+1)); continue; }; \
	  bash $(PIPELINE) -m $(MACHO_TOOL) -l $(LIBABICONV) -w $(LIBWRAPPER) -i $(LIBINTERPOSE) -o $(BUILD_DIR)/$$t.x86_64 $(BUILD_DIR)/$$t.i386 >/dev/null 2>&1 || { echo "FAIL $$t (translate)"; fail=$$((fail+1)); continue; }; \
	  chmod +x $(BUILD_DIR)/$$t.x86_64; \
	  actual=$(BUILD_DIR)/$$t.actual; expected=$(EXPECTED_DIR)/$$t.txt; \
	  $(RUN_FIXTURE) $(BUILD_DIR)/$$t.x86_64 > $$actual 2>/dev/null; \
	  echo "exit_code: $$?" >> $$actual; \
	  if [ -f $$expected ] && diff -q $$expected $$actual >/dev/null 2>&1; then \
	    echo "PASS $$t"; pass=$$((pass+1)); \
	  else \
	    echo "FAIL $$t"; [ -f $$expected ] && diff -u $$expected $$actual | sed 's/^/    /' | head -12; \
	    fail=$$((fail+1)); \
	  fi; \
	done; \
	echo ""; echo "===== objc: $$pass passed, $$fail failed ====="; \
	[ $$fail = 0 ]

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
	  $(RUN_FIXTURE) $(BUILD_DIR)/$$t.x86_64 > $$actual 2>/dev/null; \
	  echo "exit_code: $$?" >> $$actual; \
	  if [ -f $$expected ] && diff -q $$expected $$actual >/dev/null 2>&1; then \
	    echo "PASS $$t"; pass=$$((pass+1)); \
	  else \
	    echo "FAIL $$t"; [ -f $$expected ] && diff -u $$expected $$actual | sed 's/^/    /' | head -12; \
	    fail=$$((fail+1)); \
	  fi; \
	done; \
	echo ""; echo "===== cpp: $$pass passed, $$fail failed ====="; \
	[ $$fail = 0 ]

build-only: $(addprefix $(BUILD_DIR)/, $(addsuffix .x86_64, $(TESTS)))

run-only: build-only
	@pass=0; fail=0; \
	for t in $(TESTS); do \
	  actual=$(BUILD_DIR)/$$t.actual; \
	  expected=$(EXPECTED_DIR)/$$t.txt; \
	  if [ ! -f $$expected ]; then \
	    echo "SKIP $$t (no expected/$$t.txt)"; continue; \
	  fi; \
	  $(RUN_FIXTURE) $(BUILD_DIR)/$$t.x86_64 > $$actual 2>&1; \
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

# 25_libm_mixed_class_args: same -fno-builtin reason as 24. This one covers the
# shapes 24 structurally cannot -- arguments spanning BOTH x86_64 classes
# (double + int) and an INTEGER OUT-PARAM (frexp's int*), where a bridge that
# assigns registers positionally rather than by class silently corrupts every
# result.
$(BUILD_DIR)/25_libm_mixed_class_args.o: $(SRC_DIR)/25_libm_mixed_class_args.c | $(BUILD_DIR)
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

# 99_memmgr_blockmove binds the Memory Manager by CoreServices' install name.
# The framework binary is not staged (needs the SL DVD), so link a symbols-only
# i386 stub carrying that install name; translation re-binds to libabiconv.
$(BUILD_DIR)/coreservices_stub.dylib: | $(BUILD_DIR)
	@printf 'void BlockMove(){}\nvoid BlockMoveData(){}\nvoid BlockMoveUncached(){}\nvoid BlockMoveDataUncached(){}\nvoid BlockZero(){}\nvoid BlockZeroData(){}\n' > $(BUILD_DIR)/coreservices_stub.c
	@$(CC_I386) -c $(BUILD_DIR)/coreservices_stub.c -o $(BUILD_DIR)/coreservices_stub.o
	@$(LD) -arch i386 -macos_version_min 10.6 -syslibroot $(I386_SYSROOT) -lSystem -dylib \
	  -install_name /System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices \
	  -o $@ $(BUILD_DIR)/coreservices_stub.o
$(BUILD_DIR)/99_memmgr_blockmove.i386: $(BUILD_DIR)/99_memmgr_blockmove.o $(BUILD_DIR)/coreservices_stub.dylib
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $< $(BUILD_DIR)/coreservices_stub.dylib

# 99_cfnumber_i386_width links CoreFoundation the same way (see above).
$(BUILD_DIR)/99_cfnumber_i386_width.i386: $(BUILD_DIR)/99_cfnumber_i386_width.o
	@[ -f "$(I386_SYSROOT)/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation" ] \
	  || $(MAKE) --no-print-directory sysroot-objc
	$(LD) $(LDFLAGS_I386) -e _main -framework CoreFoundation -o $@ $<

# 99_subpage_text_fnptr: link with NO __PAGEZERO and __TEXT based at vmaddr 0, so
# __text starts a few hundred bytes in -- BELOW 0x1000 -- which is the layout every
# i386 DYLIB has and which the default -no_pie executable layout cannot produce
# (__PAGEZERO owns the first page, so __text starts at 0x1000 and the test would
# pass for the wrong reason, i.e. be inert). 16 of Portal 2's 42 i386 modules have
# __text below 0x1000; this is the only way to reproduce that here.
$(BUILD_DIR)/99_subpage_text_fnptr.i386: $(BUILD_DIR)/99_subpage_text_fnptr.o
	$(LD) $(LDFLAGS_I386) -pagezero_size 0 -seg1addr 0 -e _main -o $@ $<

# 99_lea_index_const links -pie (dropping the suite-wide -no_pie), because the
# rule it guards applies only to a POSITION-INDEPENDENT image — which is what
# every Portal 2 dylib is, and what the default -no_pie executable is not. Linked
# -no_pie the fixture would pass in BOTH arms, i.e. be silently inert.
$(BUILD_DIR)/99_lea_index_const.i386: $(BUILD_DIR)/99_lea_index_const.o
	$(LD) $(filter-out -no_pie,$(LDFLAGS_I386)) -pie -e _main -o $@ $<

$(BUILD_DIR)/99_cgl_renderer_check.i386: $(BUILD_DIR)/99_cgl_renderer_check.o
	@[ -f "$(I386_SYSROOT)/System/Library/Frameworks/OpenGL.framework/OpenGL" ] \
	  || $(MAKE) --no-print-directory sysroot-gl
	$(LD) $(LDFLAGS_I386) -e _main -framework OpenGL -framework ApplicationServices -o $@ $<

# 99_iconv_bridge: iconv lives in its own libiconv.2.dylib on SL, which `make
# sysroot` does not stage — take the i386 slice straight off the DVD on demand.
$(BUILD_DIR)/99_iconv_bridge.i386: $(BUILD_DIR)/99_iconv_bridge.o
	@[ -f "$(I386_SYSROOT)/usr/lib/libiconv.2.dylib" ] \
	  || { lipo -thin i386 "$(SL_DVD)/usr/lib/libiconv.2.dylib" -output "$(I386_SYSROOT)/usr/lib/libiconv.2.dylib" \
	       && ln -sf libiconv.2.dylib "$(I386_SYSROOT)/usr/lib/libiconv.dylib"; }
	$(LD) $(LDFLAGS_I386) -e _main -liconv -o $@ $<

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
# would pass for the wrong reason. The SL linker folds a __textcoal_nt input into
# __text and drops a custom section's attributes, so link with the custom name
# and then give it Halo's real header (__textcoal_nt, coalesced + instructions).
# Overrides the generic %.i386 pattern rule.
$(BUILD_DIR)/62_pic_xsection_thunk_stripped.i386: $(BUILD_DIR)/62_pic_xsection_thunk_stripped.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@
	python3 set_section.py $@ __picthunk __textcoal_nt 0x8000040b

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

# 99_stripped_fnptr_field_start: STRIP local symbols (Halo CE / Civ IV Steam
# shape — the game's own functions carry NO nlist, so func_syms evidence is
# unavailable and the field-store code-target admit must discriminate
# STRUCTURALLY). Both of the fixture's markers are LOCAL labels resolved by the
# assembler, so unlike 99_code_alias_imm_falsereloc no __text-geometry assertion
# is needed — the trap cannot drift. Asserts only that the strip really removed
# them (a surviving nlist would let arm A pass for the wrong reason).
# Overrides %.i386.
$(BUILD_DIR)/99_stripped_fnptr_field_start.i386: $(BUILD_DIR)/99_stripped_fnptr_field_start.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@
	@if nm -n $@ 2>/dev/null | grep -qE '_local_handler|_mbnop_handler|_mid_alias|_page_handler'; then \
	   echo "99_stripped_fnptr_field_start: ERROR: strip -x left a marker nlist -- trap disarmed"; \
	   rm -f $@; exit 1; \
	 fi
	@# ARM D only tests the page-multiple refusal if _page_handler really is one,
	@# and arms A/B/C only test the start rule if THEY are not page multiples
	@# (the refusal would mask the start test). Both directions asserted here so
	@# a layout drift fails the BUILD instead of silently disarming an arm.
	@otool -tV -j $@ > $(BUILD_DIR)/99_stripped_fnptr_field_start.dis 2>/dev/null; \
	 bad=0; \
	 for pat in 0x14 0x18; do \
	   v=$$(awk -F'[$$,]' '/movl.*\$$0x[0-9a-f]+, '$$pat'\(%ebx\)/{print $$2; exit}' \
	        $(BUILD_DIR)/99_stripped_fnptr_field_start.dis); \
	   if [ -n "$$v" ] && [ $$(( $$v & 0xfff )) -eq 0 ]; then \
	     echo "99_stripped_fnptr_field_start: ERROR: handler $$v is page-aligned -- arm masked by the refusal"; bad=1; \
	   fi; \
	 done; \
	 p=$$(awk -F'[$$,]' '/movl.*\$$0x[0-9a-f]+, 0x8c\(%ebx\)/{print $$2; exit}' \
	      $(BUILD_DIR)/99_stripped_fnptr_field_start.dis); \
	 if [ -z "$$p" ] || [ $$(( $$p & 0xfff )) -ne 0 ]; then \
	   echo "99_stripped_fnptr_field_start: ERROR: _page_handler=$$p is NOT page-aligned -- arm D disarmed"; bad=1; \
	 fi; \
	 if [ $$bad = 1 ]; then rm -f $@; exit 1; fi; \
	 echo "  99_stripped_fnptr_field_start: markers unsymboled, _page_handler=$$p page-aligned -- traps armed"

# 99_code_alias_imm_falsereloc: STRIP local symbols (Civ IV Steam shape — locals-
# stripped => imm32_code_alias_is_constant's symboled discriminator disarmed, only
# the 55-89-e5 prologue-byte evidence remains). The integer ALIAS must alias
# __text exactly at those bytes (the alias trap the test springs), and where
# __text lands depends on the linker, so: link once, measure _marker's vmaddr by
# content, reassemble with -defsym ALIAS=<it> (an imm32, so the layout is
# unchanged), relink, and ASSERT the bytes sit at ALIAS. Guards against
# 31de727's over-relocation: an integer CONSTANT aliasing a prologue-byte __text
# address must NOT be relocated when stored into a field (the Civ boost-registry
# rc=139 deterministic-crash root). Overrides %.i386.
CODE_ALIAS_MARKER = python3 text_addr_of.py
$(BUILD_DIR)/99_code_alias_imm_falsereloc.i386: $(SRC_DIR)/99_code_alias_imm_falsereloc.s | $(BUILD_DIR)
	$(CC_I386) -c $< -o $(BUILD_DIR)/99_code_alias_imm_falsereloc.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $(BUILD_DIR)/99_code_alias_imm_falsereloc.o
	@m=$$($(CODE_ALIAS_MARKER) $@ 5589e59090) || { rm -f $@; exit 1; }; \
	 $(CC_I386) -c -Wa,-defsym,ALIAS=$$m $< -o $(BUILD_DIR)/99_code_alias_imm_falsereloc.o && \
	 $(LD) $(LDFLAGS_I386) -e _main -o $@ $(BUILD_DIR)/99_code_alias_imm_falsereloc.o && \
	 strip -x $@ && \
	 if [ "$$($(CODE_ALIAS_MARKER) $@ 5589e59090)" = "$$m" ] && \
	    otool -tv $@ | grep -q "movl	\$$$$m, 0x88(%ebx)"; then \
	   echo "  99_code_alias_imm_falsereloc: ALIAS=$$m aliases __text w/ 55 89 e5 -- trap armed"; \
	 else \
	   echo "99_code_alias_imm_falsereloc: ERROR: marker moved or ALIAS not in code"; rm -f $@; exit 1; \
	 fi

# 99_prologue_sched_entry: stripped, fixed-address. Overrides %.i386.
$(BUILD_DIR)/99_prologue_sched_entry.i386: $(BUILD_DIR)/99_prologue_sched_entry.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@

# 99_cstring_tailmerge_ptr: same stripped, fixed-address shape as
# 99_cstring_interior_alias. Overrides %.i386.
$(BUILD_DIR)/99_cstring_tailmerge_ptr.i386: $(BUILD_DIR)/99_cstring_tailmerge_ptr.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<
	strip -x $@

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

# 99_cgl_shadow_objc_arg: CGL/QuartzCore aren't in the i386 sysroot, so CGL*
# are undefined dynamic_lookup imports bound at translate time to libabiconv's
# ___CGL* shims (the shadow-context path), and CIContext is reached through
# libobjc at run time. Overrides %.i386.
$(BUILD_DIR)/99_cgl_shadow_objc_arg.i386: $(BUILD_DIR)/99_cgl_shadow_objc_arg.o
	$(LD) $(LDFLAGS_I386) -lobjc -undefined dynamic_lookup -e _main -o $@ $<

# 39_xml_memget_alloc: the i386 sysroot ships no libxml2, so `_xmlMemGet` is an
# undefined dynamic_lookup import (resolved at translate time by static-interpose
# -> libabiconv's ___xmlMemGet shim, exactly as a real binary's libxml2 bind is).
# -undefined dynamic_lookup lets the link leave it unresolved. Overrides %.i386.
$(BUILD_DIR)/39_xml_memget_alloc.i386: $(BUILD_DIR)/39_xml_memget_alloc.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_iokit_hid_plugin: IOKit is not in the i386 sysroot — compile against the
# host SDK headers under -arch i386 and leave the IOKit/CF imports as undefined
# dynamic_lookup binds (static-interpose -> libabiconv's ___IO* shims).
$(BUILD_DIR)/99_iokit_hid_plugin.o: $(SRC_DIR)/99_iokit_hid_plugin.c | $(BUILD_DIR)
	clang -arch i386 -isysroot $(HOST_SDK) -mmacosx-version-min=10.6 -c $< -o $@
$(BUILD_DIR)/99_iokit_hid_plugin.i386: $(BUILD_DIR)/99_iokit_hid_plugin.o
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

# 99_classic_window_resource: GetNewCWindow/NewWindow (absent on 64-bit Carbon;
# carbon_control_shim.c). Carbon is undefined dynamic_lookup (bound by
# static-interpose), and the 'WIND' resource is compiled by Rez into the
# resource fork of build/99_classic_window_resource.rsrc (git cannot hold a
# fork). Overrides %.i386.
$(BUILD_DIR)/99_classic_window_resource.i386: $(BUILD_DIR)/99_classic_window_resource.o $(BUILD_DIR)/99_classic_window_resource.rsrc
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
$(BUILD_DIR)/99_classic_window_resource.rsrc: $(SRC_DIR)/99_classic_window_resource.r | $(BUILD_DIR)
	rm -f $@ && touch $@ && Rez $< -o $@

# 99_agl_window_drawable: neither Carbon/HIToolbox nor AGL is in the i386
# sysroot, and libabiconv exports a ___<sym> shim for every symbol this fixture
# needs (___CreateNewWindow/___GetWindowPort/___GetWindowFromPort/___DisposeWindow
# plus the ___agl*/___glGetString legacy bridges), so the whole surface is
# undefined dynamic_lookup and static-interpose binds it at translate time. That
# also keeps the fixture independent of `make sysroot-gl` / the SL DVD.
# Overrides %.i386.
$(BUILD_DIR)/99_agl_window_drawable.i386: $(BUILD_DIR)/99_agl_window_drawable.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_cgl_fullscreen: CGL is not in the i386 sysroot; dynamic_lookup, bound by
# static-interpose at translate time. Overrides %.i386.
$(BUILD_DIR)/99_cgl_fullscreen.i386: $(BUILD_DIR)/99_cgl_fullscreen.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_display_fullscreen_bridge: same shape as 99_agl_window_drawable -- neither
# Carbon/HIToolbox nor AGL nor the classic CoreGraphics display-mode family is in
# the i386 sysroot, and libabiconv exports a ___<sym> shim for every symbol the
# fixture needs (___CGDisplayBestModeForParameters / ___CGDisplaySwitchToMode /
# ___CGDisplayCurrentMode / ___CGDisplayBounds / ___CGCaptureAllDisplays / ... from
# cg_display_tramp.asm, plus the Carbon window + ___agl* bridges), so the whole
# surface is undefined dynamic_lookup and static-interpose binds it at translate
# time. Overrides %.i386.
$(BUILD_DIR)/99_display_fullscreen_bridge.i386: $(BUILD_DIR)/99_display_fullscreen_bridge.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_agl_renderer_info: same shape as 99_agl_window_drawable -- neither the
# Display Manager (___DMGetFirstScreenDevice / ___DMGetDisplayIDByGDevice) nor
# AGL (___aglQueryRendererInfo / ___aglNextRendererInfo / ___aglDescribeRenderer /
# ___aglDestroyRendererInfo) is in the i386 sysroot, and libabiconv exports a
# ___<sym> shim for every one, so the whole surface is undefined dynamic_lookup
# and static-interpose binds it at translate time. Overrides %.i386.
$(BUILD_DIR)/99_agl_renderer_info.i386: $(BUILD_DIR)/99_agl_renderer_info.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_cgl_macro_dispatch: same shape again -- AGL (___aglChoosePixelFormat /
# ___aglCreateContext / ___aglSetCurrentContext / ___aglDestroyContext /
# ___aglDestroyPixelFormat) is not in the i386 sysroot and libabiconv exports a
# ___<sym> shim for every one, so the surface is undefined dynamic_lookup and
# static-interpose binds it at translate time. Overrides %.i386.
$(BUILD_DIR)/99_cgl_macro_dispatch.i386: $(BUILD_DIR)/99_cgl_macro_dispatch.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_time_manager: the Time Manager is CoreServices (not in the i386 sysroot);
# libabiconv exports the ___<sym> shims, so link dynamic_lookup like above.
$(BUILD_DIR)/99_time_manager.i386: $(BUILD_DIR)/99_time_manager.o
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

# 99_cfstring_range_overrun: same shape — CFStringCreateWithCString /
# CFStringGetLength / CFStringGetCharacters / CFRelease are undefined
# dynamic_lookup imports resolved at translate time by static-interpose ->
# libabiconv's ___CF* shims, and ___CFStringGetCharacters is now the hand-written
# clamp trampoline (cfstring_range_tramp.asm) rather than an abigen bridge.
# Overrides %.i386.
$(BUILD_DIR)/99_cfstring_range_overrun.i386: $(BUILD_DIR)/99_cfstring_range_overrun.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_cfdata_bytebuf_bounce: CFDataCreate / CFDataGetLength / CFDataGetBytePtr /
# CFDataCreateMutable / CFDataAppendBytes / CFDataGetMutableBytePtr / CFRelease
# are undefined dynamic_lookup imports resolved at translate time by
# static-interpose -> libabiconv's ___CF* shims, and ___CFDataGetBytePtr /
# ___CFDataGetMutableBytePtr are now the hand-written length-aware bounce
# (cfdata_bytebuf_tramp.asm) rather than abigen's generic cstring-return
# bridge. Overrides %.i386.
$(BUILD_DIR)/99_cfdata_bytebuf_bounce.i386: $(BUILD_DIR)/99_cfdata_bytebuf_bounce.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_nulljump_no_shadow: CTabChanged is a removed classic QuickDraw call, so it
# is undefined dynamic_lookup and static-interpose decides at translate time
# whether to redirect it. Overrides %.i386.
$(BUILD_DIR)/99_nulljump_no_shadow.i386: $(BUILD_DIR)/99_nulljump_no_shadow.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_carbon_fileops: the classic File Manager write path was deleted from macOS,
# so there is no i386 library left to link against — the symbols are resolved at
# translate time by static-interpose redirecting them into libabiconv's shims,
# which is precisely what this guard measures. Overrides %.i386.
$(BUILD_DIR)/99_carbon_fileops.i386: $(BUILD_DIR)/99_carbon_fileops.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_zerofill_target_pair: linked with -x (strip local symbols) so the zero-fill
# section carries NO symbol. That is the point of the fixture — the gate accepts
# a zero-fill target ANCHORED by an object symbol and rejects one in symbol-free
# space. Overrides %.i386.
$(BUILD_DIR)/99_zerofill_target_pair.i386: $(BUILD_DIR)/99_zerofill_target_pair.o
	$(LD) $(LDFLAGS_I386) -x -e _main -o $@ $<

# 99_zerofill_stripped_ptr: linked with -x so its zero-fill section has NO
# symbol at all — iPhoto's stripped __common. Overrides %.i386.
$(BUILD_DIR)/99_zerofill_stripped_ptr.i386: $(BUILD_DIR)/99_zerofill_stripped_ptr.o
	$(LD) $(LDFLAGS_I386) -x -e _main -o $@ $<

# 99_zerofill_code_literal: -x (symbol-free below g_anchor) and non-PIC so the
# code stores to g_var through an absolute address. Overrides %.i386.
$(BUILD_DIR)/99_zerofill_code_literal.o: $(SRC_DIR)/99_zerofill_code_literal.c | $(BUILD_DIR)
	$(CC_I386) -mdynamic-no-pic -O0 -c -o $@ $<
$(BUILD_DIR)/99_zerofill_code_literal.i386: $(BUILD_DIR)/99_zerofill_code_literal.o
	$(LD) $(LDFLAGS_I386) -x -e _main -o $@ $<

# 99_record_field_pair: linked with -x for the same reason — the sibling values
# are patched to point into symbol-free zero-fill space, and the CANDIDATE must
# be a __text address with no symbol at it, which is what the code-entry gate
# treats as "no entry evidence". Overrides %.i386.
$(BUILD_DIR)/99_record_field_pair.i386: $(BUILD_DIR)/99_record_field_pair.o
	$(LD) $(LDFLAGS_I386) -x -e _main -o $@ $<
# 99_carbon_alias: the FSSpec generation of the Alias Manager was deleted from
# macOS, so there is no i386 library left to link against — the symbols are
# resolved at translate time by static-interpose redirecting them into
# libabiconv's shims, which is precisely what this guard measures. Overrides %.i386.
$(BUILD_DIR)/99_carbon_alias.i386: $(BUILD_DIR)/99_carbon_alias.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_gap_stub: TextWidth/EqualPt are QuickDraw, absent from the i386 sysroot;
# static-interpose redirects them into libabiconv's shims. Overrides %.i386.
$(BUILD_DIR)/99_gap_stub.i386: $(BUILD_DIR)/99_gap_stub.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_display_point_modes: CoreGraphics/Display Manager/CF are absent from the i386
# sysroot; static-interpose redirects them into libabiconv. Overrides %.i386.
$(BUILD_DIR)/99_display_point_modes.i386: $(BUILD_DIR)/99_display_point_modes.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_memerr: the Memory Manager is not in the i386 sysroot; static-interpose
# redirects NewPtr/SetPtrSize/MemError into libabiconv. Overrides %.i386.
$(BUILD_DIR)/99_memerr.i386: $(BUILD_DIR)/99_memerr.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_event_params: the Carbon Event Manager is not in the i386 sysroot;
# static-interpose redirects it into libabiconv. Overrides %.i386.
$(BUILD_DIR)/99_event_params.i386: $(BUILD_DIR)/99_event_params.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_carbon_datetime: the whole family is undefined dynamic_lookup — these
# symbols are gone from modern macOS and libabiconv exports a ___<name>
# trampoline for each (carbon_datetime_tramp.asm). Overrides %.i386.
$(BUILD_DIR)/99_carbon_datetime.i386: $(BUILD_DIR)/99_carbon_datetime.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_fscatinfo_bulk: the FSRef File Manager lives in CoreServices, which the i386
# sysroot does not stage; the symbols are resolved at translate time by
# static-interpose redirecting them into libabiconv's shims (carbon_fscatinfo_
# tramp.asm for the FSCatalogInfo family, abigen for FSPathMakeRef /
# FSOpenIterator / FSCloseIterator). Overrides %.i386.
$(BUILD_DIR)/99_fscatinfo_bulk.i386: $(BUILD_DIR)/99_fscatinfo_bulk.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_csmem_handle_family: the whole CSMem* family is undefined dynamic_lookup --
# CarbonCore's CSMem* names are not in the i386 sysroot and libabiconv exports a
# ___CSMem<name> trampoline for each (carbon_memory_tramp.asm), so
# static-interpose binds them at translate time. Overrides %.i386.
$(BUILD_DIR)/99_csmem_handle_family.i386: $(BUILD_DIR)/99_csmem_handle_family.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_translated_provider_defer: the classic NSIsSymbolNameDefined /
# NSLookupAndBindSymbol / NSAddressOfSymbol trio is absent from the i386 sysroot
# and libabiconv exports a ___NS<name> shim for each, so `undefined
# dynamic_lookup` lets static-interpose bind them at translate time — the exact
# route Civilization IV.dylib takes. The fixture is a PROVIDER: its own
# `_PyInt_AsLong` must be in the executable's export trie, visible to the
# RTLD_DEFAULT search the resolver uses to ask "does a translated image define
# this?". The SL linker exports an executable's globals by default.
# Overrides %.i386.
$(BUILD_DIR)/99_translated_provider_defer.i386: $(BUILD_DIR)/99_translated_provider_defer.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_low_native_thunk: the fixture dlopens a NATIVE x86_64 helper, which in a
# translated process gets mapped BELOW 4GB -- the case `fnptr_lookup_result` used
# to misread as "low, therefore translated, therefore i386-callable". It must
# carry NO libabiconv load command, since "links libabiconv" is the structural
# test the fix keys on.
# -image_base is a HINT only, and `-no_fixup_chains` is what lets the linker
# record it at all (ld disables preferred load addresses under chained fixups).
# MEASURED: dyld does not honour it -- the helper lands wherever the low arena
# has room (0xacb2f0, 0x2e182f0, ... across runs) -- which is fine, because low
# is all the guard needs. Lowness is therefore asserted at RUN TIME by the test
# script, via the "LOW NATIVE function" trace line that only the new branch
# emits, never inferred from this flag.
$(BUILD_DIR)/lownative_helper.dylib: $(SRC_DIR)/lownative_helper.c | $(BUILD_DIR)
	clang -arch x86_64 -dynamiclib -O0 \
	      -Wl,-image_base,0x30000000 -Wl,-no_fixup_chains \
	      -install_name @rpath/lownative_helper.dylib \
	      -o $@ $<

.PHONY: low-native-thunk
low-native-thunk: $(BUILD_DIR)/99_low_native_thunk.x86_64 \
                  $(BUILD_DIR)/lownative_helper.dylib
	@bash low_native_thunk_test.sh

# 99_dladdr_shim: dladdr is absent from the i386 sysroot's link surface here, so
# `undefined dynamic_lookup` lets static-interpose bind it to libabiconv's
# ___dladdr at translate time -- the route the translated Python takes.
$(BUILD_DIR)/99_dladdr_shim.i386: $(BUILD_DIR)/99_dladdr_shim.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
# 99_mp_allocate_buffer: the MP Services allocators are undefined dynamic_lookup
# for the same reason as the CSMem family -- MPAllocateAligned/MPAllocate/MPFree/
# MPGetAllocatedBlockSize are not in the i386 sysroot, libabiconv exports a
# ___MP<name> trampoline for each (mp_memory_tramp.asm) and static-interpose
# binds them at translate time. Overrides %.i386.
$(BUILD_DIR)/99_mp_allocate_buffer.i386: $(BUILD_DIR)/99_mp_allocate_buffer.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
# 99_snd_lock_order: the classic Sound Manager was deleted from 64-bit macOS, so
# SndNewChannel/SndDoImmediate/SndChannelStatus/SndDisposeChannel are undefined
# dynamic_lookup imports; libabiconv exports a ___Snd<name> trampoline for each
# (maptable_tramp.asm) and static-interpose binds them at translate time.
# Overrides %.i386.
$(BUILD_DIR)/99_snd_lock_order.i386: $(BUILD_DIR)/99_snd_lock_order.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
# 99_snd_cb_reentrancy: same reason as 99_snd_lock_order -- SndNewChannel /
# SndDoCommand / SndDisposeChannel are undefined dynamic_lookup imports bound at
# translate time by static-interpose to libabiconv's ___Snd<name> trampolines
# (maptable_tramp.asm). Overrides %.i386.
$(BUILD_DIR)/99_snd_cb_reentrancy.i386: $(BUILD_DIR)/99_snd_cb_reentrancy.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
# 99_snd_cb_fifo: same reason as 99_snd_lock_order (dynamic_lookup Snd* imports).
$(BUILD_DIR)/99_snd_cb_fifo.i386: $(BUILD_DIR)/99_snd_cb_fifo.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
# 98_snd_status_layout: same reason as 99_snd_lock_order -- SndNewChannel /
# SndDoImmediate / SndChannelStatus / SndDisposeChannel are undefined
# dynamic_lookup imports bound at translate time by static-interpose to
# libabiconv's ___Snd<name> trampolines (maptable_tramp.asm). Overrides %.i386.
$(BUILD_DIR)/98_snd_status_layout.i386: $(BUILD_DIR)/98_snd_status_layout.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
# 97_mp_event_wake: MP Services is not in the i386 sysroot -- MPCreateEvent /
# MPSetEvent / MPWaitForEvent / MPDeleteEvent are undefined dynamic_lookup
# imports bound at translate time by static-interpose to libabiconv's ___MP<name>
# bridges, exactly like 99_mp_allocate_buffer. Overrides %.i386.
$(BUILD_DIR)/97_mp_event_wake.i386: $(BUILD_DIR)/97_mp_event_wake.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<
# 96_jt_cohesion: plain fixture, but it must NOT be dead-stripped or reordered --
# the whole point is a __TEXT,__const word whose value aliases a specific
# __text address. Default %.i386 rule flags are what produce the measured
# layout (__text 0x2000..0xE033); the guard script re-asserts that by
# disassembly, so a layout change fails loudly instead of going inert.
# 99_classic_input_coords: the Window Manager and the classic QuickDraw coord
# calls are not in the i386 sysroot; libabiconv exports a ___<name> trampoline
# for each and static-interpose binds them at translate time. Overrides %.i386.
$(BUILD_DIR)/99_classic_input_coords.i386: $(BUILD_DIR)/99_classic_input_coords.o
	$(LD) $(LDFLAGS_I386) -undefined dynamic_lookup -e _main -o $@ $<

# 99_mp_task_spawn: same reason as 99_mp_allocate_buffer -- the MP Services task
# and synchronisation entry points are not in the i386 sysroot; libabiconv
# exports a ___MP<name> bridge for each and static-interpose binds them at
# translate time. Overrides %.i386.
$(BUILD_DIR)/99_mp_task_spawn.i386: $(BUILD_DIR)/99_mp_task_spawn.o
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

# The .nulljump list must travel WITH the dylib: static-interpose looks for it
# next to whatever -l it was given, and without it every guard would silently
# keep interposing the bridges that can only jump to NULL -- i.e. the suites
# would stop exercising the very behaviour they are meant to protect.
$(BUILD_DIR)/libabiconv.dylib: $(LIBABICONV) | $(BUILD_DIR)
	cp $< $@
	@cp $(dir $(LIBABICONV))libabiconv.nulljump $(BUILD_DIR)/ 2>/dev/null || echo "warning: libabiconv.nulljump missing next to $(LIBABICONV)"


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

# self-weak-rebase: guard for "a translated image must satisfy its OWN weak
# binds locally" (core archive.cc Archive<M64>::resolve_self_weak_binds).
# transform.cc clears MH_WEAK_DEFINES so translated i386 code can never satisfy
# a NATIVE image's C++ coalesced bind (the cross-ABI weak-def coalesce bug) --
# which ALSO stopped a self-contained C++ image satisfying its own weak binds to
# its linkonce_odr vtables / typeinfo / inline members. Those __nl/__la_symbol_ptr
# slots then kept their UNSLID preferred-base file value, and the first call
# through one jumped to an unmapped 0x100000xx. That single defect was behind ALL
# SIX long-standing `make cpp` failures. This runs the full A/B with
# M64_NO_SELF_WEAK_REBASE=1 and verifies the ON arm BY CONTENT (no weak bind to
# an in-image definition survives; no slot is rebased twice), plus two control
# fixtures that must pass in both arms. Depends on `cpp` for the .i386 fixtures.
.PHONY: self-weak-rebase
self-weak-rebase: cpp
	@bash self_weak_rebase_test.sh $(MACHO_TOOL) $(LIBABICONV) $(LIBWRAPPER) $(LIBINTERPOSE)

# shimgen-translated-skip: two-arm guard for src/86x64/shimgen.py — a TRANSLATED
# image (links libabiconv) must never have its missing binds collected into a
# native ShimAuto stub / observed.json (that pollution is what needs a
# libabiconv bridge / `m64 reinterpose`, never a native-ABI stub). Fully
# self-contained (builds its own fake-framework fixtures; no i386 sysroot).
.PHONY: shimgen-translated-skip
shimgen-translated-skip:
	@bash shimgen_translated_skip_test.sh

# 99_disp_reloc_accept: absolute text relocs in a -pie 10.5 image (classic
# local relocs on the instruction fields).
$(BUILD_DIR)/99_disp_reloc_accept.o: $(SRC_DIR)/99_disp_reloc_accept.s | $(BUILD_DIR)
	$(CC_I386) -c -o $@ $<
$(BUILD_DIR)/99_disp_reloc_accept.i386: $(BUILD_DIR)/99_disp_reloc_accept.o
	$(LD) $(filter-out -no_pie -macos_version_min 10.6,$(LDFLAGS_I386)) -macos_version_min 10.5 -pie -read_only_relocs suppress -x -no_compact_linkedit -e _main -o $@ $< $(HOME)/projects/Library/SDKs/MacOSX10.6.sdk/usr/lib/crt1.10.5.o

# 99_imm_reloc_accept: same link recipe as 99_disp_reloc_accept.
$(BUILD_DIR)/99_imm_reloc_accept.o: $(SRC_DIR)/99_imm_reloc_accept.s | $(BUILD_DIR)
	$(CC_I386) -c -o $@ $<
$(BUILD_DIR)/99_imm_reloc_accept.i386: $(BUILD_DIR)/99_imm_reloc_accept.o
	$(LD) $(filter-out -no_pie -macos_version_min 10.6,$(LDFLAGS_I386)) -macos_version_min 10.5 -pie -read_only_relocs suppress -x -no_compact_linkedit -e _main -o $@ $< $(HOME)/projects/Library/SDKs/MacOSX10.6.sdk/usr/lib/crt1.10.5.o

# 99_classic_reloc_accept: -O2 -fomit-frame-pointer (no prologue at the
# targets), linked -x -pie for 10.5 so the image carries CLASSIC local relocs.
$(BUILD_DIR)/99_classic_reloc_accept.o: $(SRC_DIR)/99_classic_reloc_accept.c | $(BUILD_DIR)
	$(CC_I386) -O2 -fomit-frame-pointer -c -o $@ $<
$(BUILD_DIR)/99_classic_reloc_accept.i386: $(BUILD_DIR)/99_classic_reloc_accept.o
	$(LD) $(filter-out -no_pie -macos_version_min 10.6,$(LDFLAGS_I386)) -macos_version_min 10.5 -pie -x -no_compact_linkedit -e _main -o $@ $< $(HOME)/projects/Library/SDKs/MacOSX10.6.sdk/usr/lib/crt1.10.5.o
