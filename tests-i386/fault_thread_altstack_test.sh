#!/bin/bash
#
# fault-thread-altstack — two-arm guard for the per-thread alternate signal
# stack of the fault reporter (src/abiconv/fault_report_shim.c fr_thread_hook).
#
# sigaltstack is per THREAD. The reporter installed one on the arming thread
# only, so a stack overflow on any worker thread could not be reported: the
# kernel pushes the signal frame below the exhausted rsp, that write faults too,
# and the process dies with no report.
#
# ARMS (a worker thread faults with rsp pointing at unmapped memory, so the
# signal frame cannot be pushed on its own stack; a plain recursive overflow is
# NOT enough under Rosetta, where the frame lands in whatever is mapped below):
#   ON  : M64_FAULT_REPORT=1                                  -> a complete report
#   OFF : + M64_NO_FAULT_THREAD_ALTSTACK=1 (no per-thread stack) -> no report
# Self-contained native x86_64; no i386 sysroot.
set -u
cd "$(dirname "$0")"
LIBABICONV="$(cd .. && pwd)/build/src/abiconv/libabiconv.dylib"
[ -f "$LIBABICONV" ] || { echo "FAIL fault-thread-altstack (libabiconv not built)"; exit 1; }
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cat > "$TMP/host.c" <<'C'
#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
/* rsp off the end of any mapping (Portal 2's shape): the signal frame
 * cannot be pushed on this stack at all. */
static void *worker(void *u) { (void)u; __asm__ volatile("mov $0x800, %rsp; push %rax"); return 0; }
int main(int argc, char **argv) {
    if (!dlopen(argv[1], RTLD_LAZY)) { printf("SKIP (dlopen: %s)\n", dlerror()); return 0; }
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);
    pthread_join(t, NULL);
    return 0;
}
C
cc -arch x86_64 -O0 -o "$TMP/host" "$TMP/host.c" -Wl,-pagezero_size,0x1000 2>"$TMP/cc.log" \
  || { echo "FAIL fault-thread-altstack (cc error)"; head "$TMP/cc.log"; exit 1; }

run_arm() {
    env M64_FAULT_REPORT=1 "$@" perl -e 'my $p = fork; if (!$p) { exec @ARGV or exit 127 }
        $SIG{ALRM} = sub { kill "KILL", $p }; alarm 10; waitpid($p, 0)' \
        "$TMP/host" "$LIBABICONV" 2>&1
}
off_out="$(run_arm M64_NO_FAULT_THREAD_ALTSTACK=1)"
on_out="$(run_arm)"                                   # ★ON arm last
case "$on_out" in SKIP*) echo "$on_out"; exit 1;; esac

fail=0
if printf '%s\n' "$on_out" | grep -q "FATAL FAULT" &&
   printf '%s\n' "$on_out" | grep -q "======== END ========"; then
    echo "  ON  : worker-thread broken-stack fault reported in full  OK"
else
    echo "  ON  : no complete report for a worker-thread broken-stack fault"; fail=1
fi
if printf '%s\n' "$off_out" | grep -q "======== END ========"; then
    echo "  OFF : a complete report without the per-thread stack — guard is inert"; fail=1
else
    echo "  OFF : no per-thread stack -> the fault dies unreported  OK"
fi
[ $fail = 0 ] && echo "PASS fault-thread-altstack" || echo "FAIL fault-thread-altstack"
exit $fail
