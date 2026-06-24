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

SRC_DIR      := src
EXPECTED_DIR := expected
BUILD_DIR    := build

ASM_SRCS := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.asm)
C_SRCS   := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.c)
S_SRCS   := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.s)
OBJC_SRCS := $(wildcard $(SRC_DIR)/[0-9][0-9]_*.m)
TESTS    := $(notdir $(basename $(ASM_SRCS) $(C_SRCS) $(S_SRCS)))
OBJC_TESTS := $(notdir $(basename $(OBJC_SRCS)))

# No crt0 — the 86x64.sh pipeline's wrapper exec calls into the translated
# _main directly, replacing the libc start crt0 would have provided. Tests
# end their _main with `call _exit` rather than `ret` since the wrapper
# uses `jmp` (no return address) to enter _main. crt0.s is kept in src/
# for transform-only experiments but not linked into the pipeline path.

.PHONY: all clean sysroot sysroot-objc list run-only build-only objc
all: run-only

list:
	@echo "Tests: $(TESTS)"
	@echo "ObjC tests (opt-in via 'make objc'): $(OBJC_TESTS)"

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

# Tests use _main as the entry directly (-e _main); the 86x64.sh wrapper
# enters _main via jmp, so tests must end with `call _exit` not `ret`.
$(BUILD_DIR)/%.i386: $(BUILD_DIR)/%.o
	$(LD) $(LDFLAGS_I386) -e _main -o $@ $<

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
