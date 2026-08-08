#!/bin/bash
#
# fault-report-once — two-arm guard for src/abiconv/fault_report_shim.c:
#   (a) ONE reporter per process, not one per co-located libabiconv copy
#   (b) the handler symbolizes from a PRE-FAULT snapshot, never by asking dyld
#
# THE DEFECT, measured on Civilization IV 2026-08-08.
#   A deployed bundle carries many co-located libabiconv copies — 38 in Civ's,
#   of which THREE actually load (MacOS/, QuickTime.framework/,
#   Python.framework/). Each copy runs its own constructor with its own statics,
#   so each armed the reporter and each saved the PREVIOUS handler, which was
#   the previous copy's fr_handler. Chaining to that previous handler then
#   replayed the whole report per copy:
#       [fault] armed: ...                      x3
#       [fault] ==== FATAL FAULT ====           x3, byte-identical, ONE rip
#   The evidence a reader needs is buried in two redundant duplicates, and the
#   process's real disposition is reached only after N full passes.
#
#   ⚠Recorded because I first misread it: those three fr_handler frames in the
#   .ips look exactly like the reporter RE-FAULTING inside its own symbolisation
#   and masking the original fault. It is not — the reports are complete and
#   identical. Same family as the libabiconv multi-copy gotcha.
#
# THE SECOND FIX, on its own merits: fr_image_for() used to call
#   _dyld_image_count / _dyld_get_image_header / _dyld_get_image_name — none of
#   them async-signal-safe — for rip, rsp, rbp AND every one of the 48 stack
#   slots AND the image listing: ~51 re-entries into dyld per fault, from a
#   signal handler, in a project whose signature fault class is raised from
#   inside a lazy bind (i.e. from inside dyld). The table is now snapshotted
#   outside signal context (arm time + dyld's add-image callback) and the
#   handler reads nothing else. A probe must never crash — the same rule
#   objc_shim.c's mem_readable()/metaclass_probe_safe() already follow.
#
# ARMS:
#   ON  : two libabiconv copies loaded -> exactly ONE report.
#   OFF : M64_FAULT_REPORT_MULTI_INSTALL=1 -> TWO reports for one fault, the
#         duplication reproduces.
# Both arms must still resolve an image loaded AFTER arming (proving the
# snapshot is maintained by the add-image callback, not frozen at arm time).
#
# Self-contained native x86_64; no i386 sysroot.
set -u
cd "$(dirname "$0")"

LIBABICONV="$(cd .. && pwd)/build/src/abiconv/libabiconv.dylib"
[ -f "$LIBABICONV" ] || { echo "SKIP fault-report-once (libabiconv not built)"; exit 0; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# A second COPY of libabiconv, exactly as a bundle deploy produces. Its install
# name must differ or dyld will hand back the first image instead of loading it.
cp "$LIBABICONV" "$TMP/libabiconv_copy2.dylib"
install_name_tool -id "$TMP/libabiconv_copy2.dylib" "$TMP/libabiconv_copy2.dylib" 2>/dev/null
codesign -f -s - "$TMP/libabiconv_copy2.dylib" >/dev/null 2>&1

# A dylib loaded AFTER the reporter arms, whose initializer faults. Its rip must
# still resolve to an image name, which only the maintained snapshot can do.
cat > "$TMP/bad.c" <<'EOF'
#include <stdint.h>
__attribute__((constructor))
static void boom(void) { *(volatile uint32_t *)(uintptr_t)0x56 = 1; }
EOF
cat > "$TMP/host.c" <<'EOF'
#include <dlfcn.h>
#include <stdio.h>
int main(int argc, char **argv) {
    for (int i = 1; i < argc - 1; i++)
        if (!dlopen(argv[i], RTLD_LAZY | RTLD_LOCAL)) {
            printf("SKIP (dlopen %s: %s)\n", argv[i], dlerror()); return 0;
        }
    fflush(stdout);
    dlopen(argv[argc - 1], RTLD_NOW | RTLD_LOCAL);   /* faults in its ctor */
    return 0;
}
EOF
cc -arch x86_64 -dynamiclib -o "$TMP/bad.dylib" "$TMP/bad.c" 2>"$TMP/cc.log" \
  && cc -arch x86_64 -o "$TMP/host" "$TMP/host.c" -Wl,-pagezero_size,0x1000 2>>"$TMP/cc.log" \
  || { echo "FAIL fault-report-once (cc error)"; sed 's/^/    /' "$TMP/cc.log" | head; exit 1; }

run_arm() {   # $1 = extra env assignment ("" for none)
    env M64_FAULT_REPORT=1 M64_FAULT_REPORT_WORDS=2 ${1:+"$1"} \
        "$TMP/host" "$LIBABICONV" "$TMP/libabiconv_copy2.dylib" "$TMP/bad.dylib" 2>&1
}

fail=0
on_out="$(run_arm "")"
off_out="$(run_arm M64_FAULT_REPORT_MULTI_INSTALL=1)"

case "$on_out" in SKIP*) echo "$on_out"; exit 0;; esac

on_reports=$(printf '%s\n'  "$on_out"  | grep -c "FATAL FAULT")
off_reports=$(printf '%s\n' "$off_out" | grep -c "FATAL FAULT")
on_armed=$(printf '%s\n'    "$on_out"  | grep -c "\[fault\] armed:")
off_armed=$(printf '%s\n'   "$off_out" | grep -c "\[fault\] armed:")

echo "  ON  : armed=$on_armed  reports=$on_reports"
echo "  OFF : armed=$off_armed reports=$off_reports"

if [ "$on_reports" = 1 ]; then
    echo "  ON  (one reporter per process): 2 libabiconv copies loaded, exactly"
    echo "                                  ONE report for one fault           OK"
else
    echo "  ON  : expected exactly 1 report, got $on_reports — the per-copy"
    echo "        duplication is NOT fixed"
    fail=1
fi

if [ "$off_reports" -ge 2 ]; then
    echo "  OFF (kill switch):              the SAME fault reported"
    echo "                                  $off_reports times — duplication reproduces  OK"
else
    echo "  OFF : expected >=2 reports with M64_FAULT_REPORT_MULTI_INSTALL=1, got"
    echo "        $off_reports — this guard is NOT exercising the fix"
    fail=1
fi

# Both arms: an image loaded AFTER arming must still symbolize, or the snapshot
# is frozen and the reporter has gone blind to exactly the late-loaded images a
# translated bundle is full of.
for arm in "ON:$on_out" "OFF:$off_out"; do
    if ! printf '%s\n' "${arm#*:}" | grep -q "rip.*bad.dylib+0x"; then
        echo "  ${arm%%:*}: the faulting rip did NOT resolve to bad.dylib — an image"
        echo "        loaded after arming is invisible; the snapshot is not maintained"
        fail=1
    fi
done

# The report must be COMPLETE: a truncated one is the failure mode that would
# make this whole tool untrustworthy.
printf '%s\n' "$on_out" | grep -q "======== END ========" || {
    echo "  ON  : report has no END marker — it did not run to completion"; fail=1; }
printf '%s\n' "$on_out" | grep -q "NESTED FAULT" && {
    echo "  ON  : the handler re-entered itself — something it touches is not fault-safe"
    fail=1; }

# Static wiring in the BUILT dylib, so a stale build cannot pass inertly.
for s in M64_FAULT_REPORT_MULTI_INSTALL M64_FAULT_REPORT_UNSAFE_SYMS; do
    strings -a "$LIBABICONV" 2>/dev/null | grep -q "$s" || {
        echo "  kill-switch string $s absent from the built libabiconv"; fail=1; }
done

if [ "$fail" = 0 ]; then echo "fault-report-once: PASS"; else echo "fault-report-once: FAIL"; fi
exit $fail
