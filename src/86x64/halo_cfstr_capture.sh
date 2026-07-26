#!/bin/bash
# halo_cfstr_capture.sh — capture the i386 CALL SITE that hands a bad CFStringRef
# to native CoreFoundation.
#
# Why this exists: the crashlog unwinder loses the i386 frame below our abigen
# `.l1` shim (only 3 frames survive), and ABICONV_ARGSTR_TRACE's `caller=` field
# is the unwrap wrapper's own return address, identical on every line. Neither
# identifies the translated call site.
#
# How it works: an abigen `.l1` shim does `push rbp; mov rsp,rbp` and then only
# ever scratches rsp, so when it calls the native function %rbp STILL points at
# the shim's frame. In that frame the i386 caller's 4-byte return address sits at
# [rbp+8] and the i386 cdecl args start at [rbp+0xc]. So we break on the NATIVE
# CF entry point (before it pushes its own frame) and read them straight out.
#
# The captured return address is a TRANSLATED address; subtract the Halo.dylib
# load base and add its __TEXT vmaddr (0x10000000) to get the address to
# disassemble, which mirrors the original i386 layout.
#
# Usage:  bash halo_cfstr_capture.sh [logfile]
# Then:   drive Halo to the failing UI (Graphics Settings -> OK) and let it crash.
set -u
HALO="${HALO_APP:-$HOME/projects/translations/Apps64/Halo.app}/Contents/MacOS/Halo"
LOG="${1:-/tmp/halo_cfstr_capture.log}"
CMDS="$(mktemp -t halocap).lldb"

cat > "$CMDS" <<'EOF'
# nil CFStringRef reaching CFStringGetCString(theString, buf, size, encoding)
breakpoint set -n CFStringGetCString -c '$rdi == 0'
breakpoint command add 1
script print("\n=== BAD CFStringGetCString ===")
p/x *(unsigned int *)($rbp+8)
p/x *(unsigned int *)($rbp+0xc)
p/x *(unsigned int *)($rbp+0x10)
p/x *(unsigned int *)($rbp+0x14)
continue
DONE

# sub-page / nil allocator reaching CFStringCreateWithCString(alloc, cStr, enc)
breakpoint set -n CFStringCreateWithCString -c '$rdi < 0x1000'
breakpoint command add 2
script print("\n=== BAD CFStringCreateWithCString ===")
p/x *(unsigned int *)($rbp+8)
p/x *(unsigned int *)($rbp+0xc)
p/x *(unsigned int *)($rbp+0x10)
p/x *(unsigned int *)($rbp+0x14)
continue
DONE

run
image list Halo.dylib
EOF

echo "Launching Halo under lldb. Drive it to Graphics Settings -> OK."
echo "Log: $LOG"
lldb -b -s "$CMDS" "$HALO" 2>&1 | tee "$LOG"
rm -f "$CMDS"

echo
echo "--- captured frames (first word of each pair = i386 return address) ---"
grep -A5 "=== BAD" "$LOG" | grep -E "===|0x" || echo "(none captured)"
