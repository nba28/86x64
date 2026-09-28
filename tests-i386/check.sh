#!/bin/bash
# check.sh — run EVERYTHING: the core/objc/cpp suites and every guard target in
# the Makefile, each under a deadline (a hung OFF arm counts as a failure, it
# never blocks the run). Usage: make check  |  bash check.sh [target...]
# Logs: build/check/<target>.log; summary on stdout; exit 1 if anything failed.
cd "$(dirname "$0")"
LOGDIR=build/check
mkdir -p "$LOGDIR"
DEADLINE=${CHECK_DEADLINE:-600}

# Run "$@" in its own process group; kill the whole group at the deadline.
with_deadline() {
    perl -e '
        my $t = shift; my $pid = fork;
        if (!$pid) { setpgrp(0, 0); exec @ARGV or exit 127 }
        $SIG{ALRM} = sub { kill "KILL", -$pid; exit 124 };
        alarm $t; waitpid($pid, 0); exit($? & 127 ? 128 + ($? & 127) : $? >> 8)' "$DEADLINE" "$@"
}

META='all clean list run-only build-only objc cpp check sysroot sysroot-objc sysroot-cpp sysroot-gl'
if [ $# -gt 0 ]; then
    targets="$*"
else
    targets="run-only objc cpp $(grep -o '^[a-z][a-z0-9-]*:' Makefile | tr -d : | sort -u |
        grep -vxF -f <(tr ' ' '\n' <<<"$META"))"
fi

pass=0; fail=0; failed=()
for t in $targets; do
    start=$SECONDS
    with_deadline make -s "$t" >"$LOGDIR/$t.log" 2>&1
    ec=$?
    dt=$((SECONDS - start))
    if [ $ec = 0 ]; then
        st=PASS; pass=$((pass + 1))
    else
        [ $ec = 124 ] && st="TIMEOUT(${DEADLINE}s)" || st="FAIL($ec)"
        fail=$((fail + 1)); failed+=("$t")
    fi
    printf '%-32s %-14s %4ss\n' "$t" "$st" "$dt"
done
echo
echo "===== check: $pass passed, $fail failed ====="
[ $fail = 0 ] || { printf '  %s\n' "${failed[@]}"; echo "logs: $LOGDIR/<target>.log"; exit 1; }
