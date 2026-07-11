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

.PHONY: all clean sysroot sysroot-objc sysroot-cpp list run-only build-only objc cpp change-deps cg-color qd-pict qt-movie synth-dyld-info nopie-exec-dylib-rebase jt-weak-import dyld-multicopy carbon-nib carbon-dangling stale-check stripped-stackarg-procptr handle-marshal byval-marshal fpreg-return handle-arg fmgr-vinfo structret-regclass pack-record cb-extended sub-framework va-list-skip ae-shadow record-shadow byval-int32 selfref-imm-stage-shift inplace-symlink-guard vendor-dead-system snapshot-compat
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

# ae-shadow: regression guard for the Apple Event Manager native-shadow bridge
# (ae_shim.c). Self-contained (native x86_64 cc + CoreServices, no i386 sysroot):
# links the real ae_shim.c and proves (A) the shadow registry preserves a FULL
# >4GB native AEDesc pointer across the 32-bit i386 shadow key (the old path
# truncated it -> Civ 'oapp' AEGetParamDesc crash) and (B) shim_AEGetParamPtr
# resolves a registered shadow to the ORIGINAL native descriptor.
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

# stripped-stackarg-procptr: regression for the stripped-binary stack-arg
# ProcPtr relocation fix (core instruction.cc; Halo CE renderer-check event
# handlers registered via `movl $handler,(%esp)` in a symbol-stripped binary).
# Asserts the raw i386 ProcPtr immediate is relocated (not left as a bare
# c7 04 24 <i386 addr>). Needs the i386 sysroot; SKIPs without.
stripped-stackarg-procptr: $(MACHO_TOOL)
	@bash stripped_stackarg_procptr_test.sh $(MACHO_TOOL)

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
