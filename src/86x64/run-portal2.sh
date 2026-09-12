#!/usr/bin/env bash
# Launch the translated Portal 2 and capture where it dies.
#
# usage: run-portal2.sh [--timeout SECS] [--dyld-apis] [--lldb] [-- <extra game args>]
#
# Runs the x86_64 tree in bin/osx64 WITHOUT swapping bin/osx32: dyld finds the
# translated dylibs through the wrapper's baked @rpath plus DYLD_LIBRARY_PATH.
# ABICONV_RUN_INITS=1 is required — dyld4 does not call our wrapped
# __mod_init_func stubs, so libabiconv runs the translated C++ static
# initializers itself (see the Portal 2 translation notes s3b).
#
# Writes a timestamped log and, if the process died, prints the exit status plus
# any fresh crash report. Exit code is the GAME's, so callers can bucket it
# (139 = SIGSEGV, 134 = SIGABRT, 137 = SIGKILL/our own timeout).
set -uo pipefail

P2="${P2:-$HOME/Library/Application Support/Steam/steamapps/common/Portal 2}"
OUTDIR="${OUTDIR:-${TMPDIR:-/tmp}}"
TIMEOUT=60
DYLD_APIS=0
USE_LLDB=0
EXTRA=()

while [ $# -gt 0 ]; do
  case "$1" in
    --timeout) TIMEOUT="$2"; shift 2;;
    --dyld-apis) DYLD_APIS=1; shift;;
    --lldb) USE_LLDB=1; shift;;
    --) shift; EXTRA=("$@"); break;;
    -h|--help) sed -n '2,12p' "$0"; exit 0;;
    *) echo "unknown arg: $1" >&2; exit 2;;
  esac
done

[ -x "$P2/bin/osx64/portal2_osx" ] || { echo "FATAL: no translated exec at $P2/bin/osx64/portal2_osx" >&2; exit 1; }
[ -f "$P2/steam_appid.txt" ] || echo "!! no steam_appid.txt — Steam init may refuse" >&2

stamp=$(date '+%Y%m%d-%H%M%S')
LOG="$OUTDIR/portal2-run-$stamp.log"

# Portal 2 is a singleton; a stale copy holds the Steam pipe and the GL context.
# -x (exact name), never -f: a -f pattern would also match a concurrent
# `m64 translate .../portal2_osx` in another session. See the heavy-translate-in-background gotcha.
pkill -9 -x portal2_osx 2>/dev/null
sleep 1

# Note the crash-report watermark BEFORE launching, so we only report OUR crash.
CRASHDIR="$HOME/Library/Logs/DiagnosticReports"
before=$(ls -1 "$CRASHDIR" 2>/dev/null | grep -c '^portal2_osx' || true)

echo "==> log      $LOG"
echo "==> timeout  ${TIMEOUT}s"

cd "$P2" || exit 1
env_args=(ABICONV_RUN_INITS=1 "DYLD_LIBRARY_PATH=$P2/bin/osx64")
[ "$DYLD_APIS" -eq 1 ] && env_args+=(DYLD_PRINT_APIS=1)

if [ "$USE_LLDB" -eq 1 ]; then
  env "${env_args[@]}" lldb -b \
      -o 'file ./bin/osx64/portal2_osx' -o run \
      -k 'thread backtrace all' -k 'register read' -k quit \
      >"$LOG" 2>&1
  rc=$?
else
  # A watchdog keeps an idle-park (the s18 pthread_cond_timedwait state) from
  # hanging an unattended run forever; rc 137 means WE killed it, so the game was
  # still ALIVE rather than crashed. Hand-rolled because macOS ships no
  # timeout(1)/gtimeout unless coreutils is installed.
  env "${env_args[@]}" ./bin/osx64/portal2_osx "${EXTRA[@]+"${EXTRA[@]}"}" \
    >"$LOG" 2>&1 &
  game_pid=$!
  ( sleep "$TIMEOUT"; kill -9 "$game_pid" 2>/dev/null ) &
  watchdog_pid=$!
  wait "$game_pid"; rc=$?
  kill "$watchdog_pid" 2>/dev/null
  wait "$watchdog_pid" 2>/dev/null
fi

echo "==> EXIT=$rc$([ "$rc" -eq 137 ] && echo '  (SIGKILL = our timeout: it was still ALIVE, not crashed)')"
echo "--- last 25 log lines ---"
tail -25 "$LOG"

after=$(ls -1 "$CRASHDIR" 2>/dev/null | grep -c '^portal2_osx' || true)
if [ "$after" -gt "$before" ]; then
  newest=$(ls -1t "$CRASHDIR"/portal2_osx* 2>/dev/null | head -1)
  echo "--- FRESH CRASH REPORT: $newest ---"
  # ⚠ A sandboxed `ls` of DiagnosticReports returns EMPTY WITH NO ERROR, which
  # fakes "no crash report" — so only trust a POSITIVE find here.
  grep -E '"(exception|termination|signal)"|faultingThread' -A 4 "$newest" 2>/dev/null | head -30
else
  echo "--- no new portal2_osx crash report (had $before) ---"
fi
exit "$rc"
