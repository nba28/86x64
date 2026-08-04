#!/bin/bash
# carbon_alias_test.sh — A/B guard for the classic ALIAS MANAGER.
#
# NewAlias / NewAliasMinimal / NewAliasMinimalFromFullPath / ResolveAlias /
# ResolveAliasFile / MatchAlias / FSMatchAliasNoUI / QTNewAlias were NULL-jump
# orphans: the legacy shim pass emits a bridge that calls a native Apple deleted
# from 64-bit Carbon in 2009, so it could only `call 0`.
#
# ON : an alias really behaves like an alias. ★The load-bearing assertion is
#      FOLLOWED_RENAME / FOLLOWED_MOVE — the target is renamed and then moved to
#      another directory through the classic File Manager, and resolving the SAME
#      handle still names it. A stored path string cannot do that, and neither can
#      the surviving native FSResolveAlias (measured: the CNID search is gone from
#      modern CarbonCore), which is why a minted record carries a CFURL bookmark
#      appendix after its -1 terminator.
# OFF: ABICONV_ALIAS_LEGACY=1 — every call declines with a File Manager error,
#      out-params stay DEFINED, nothing is minted and nothing resolves. That is
#      the pre-fix behaviour minus the jump to zero, so the arms must differ or
#      the guard is proving nothing.
set -u
cd "$(dirname "$0")"
BIN=build/99_carbon_alias.x86_64
if [ ! -x "$BIN" ]; then
  echo "carbon-alias: SKIP (build/99_carbon_alias.x86_64 missing)"; exit 0
fi
fail=0
has() { printf '%s\n' "$1" | grep -q "^$2\$"; }

on=$("$BIN" 2>/dev/null); rc=$?
want='survived=1 findfolder=1 setup=1 newalias=1 newalias_ishandle=1 resolve=1
resolve_same=1 resolve_wasChanged_defined=1 rename=1 rename_seen=1
rename_old_gone=1 resolve_after_rename=1 FOLLOWED_RENAME=1 wasChanged=1 move=1
FOLLOWED_MOVE=1 match=1 match_needsUpdate_defined=1 fsmatch=1
newaliasminimal=1 qtnewalias=1 fromfullpath=1 rafile_plain=1
rafile_outparams_defined=1 rafile_folder=1 badinput_defined=1
badinput_count_zero=1 cleanup=1 done=1'
miss=""; for k in $want; do has "$on" "$k" || miss="$miss $k"; done
if [ "$rc" = 0 ] && [ -z "$miss" ]; then
  echo "  ON  (shims armed):  an alias survives a RENAME and a MOVE of its target and"
  echo "                      still resolves to it, confirmed by FSMakeFSSpec         OK"
else
  echo "  ON  (shims armed):  rc=$rc missing:$miss"
  printf '%s\n' "$on" | sed 's/^/      /'; fail=1
fi

off=$(ABICONV_ALIAS_LEGACY=1 "$BIN" 2>/dev/null); orc=$?
# Must still SURVIVE (declining is not crashing), must mint nothing and resolve
# nothing, and must still leave every out-param defined.
if [ "$orc" = 0 ] && has "$off" 'survived=1' \
   && ! has "$off" 'newalias=1' && ! has "$off" 'resolve=1' \
   && ! has "$off" 'FOLLOWED_RENAME=1' && ! has "$off" 'FOLLOWED_MOVE=1' \
   && ! has "$off" 'match=1' && ! has "$off" 'qtnewalias=1' \
   && has "$off" 'resolve_wasChanged_defined=1' \
   && has "$off" 'rafile_outparams_defined=1' \
   && has "$off" 'badinput_count_zero=1'; then
  echo "  OFF (kill switch):  every call declines with a File Manager error, nothing is"
  echo "                      minted or resolved, out-params still defined — arms differ OK"
else
  echo "  OFF (kill switch):  expected every call to decline (rc=$orc):"
  printf '%s\n' "$off" | sed 's/^/      /'; fail=1
fi


# ★THIRD ARM, narrower than the kill switch. ABICONV_ALIAS_NO_BOOKMARK=1 keeps the
# bridge onto Apple's surviving engine and omits ONLY the CFURL bookmark appendix.
# Without it, "FOLLOWED_RENAME=1" above could in principle be the native engine
# doing the work and the hybrid record would be decoration. So this arm must show
# BOTH halves of the design at once:
#   * resolve=1                — the bridge alone still resolves a target in place,
#   * FOLLOWED_RENAME absent   — but it cannot follow a renamed one.
# Measured on this OS: native FSResolveAlias returns fnfErr after a rename because
# the CNID search is gone from modern CarbonCore.
nb=$(ABICONV_ALIAS_NO_BOOKMARK=1 "$BIN" 2>/dev/null); nrc=$?
if [ "$nrc" = 0 ] && has "$nb" 'newalias=1' && has "$nb" 'resolve=1' \
   && has "$nb" 'resolve_same=1' \
   && ! has "$nb" 'FOLLOWED_RENAME=1' && ! has "$nb" 'FOLLOWED_MOVE=1'; then
  echo "  NO-BOOKMARK arm  :  the bridge alone still resolves IN PLACE but can no longer"
  echo "                      follow a rename/move — the appendix is load-bearing     OK"
else
  echo "  NO-BOOKMARK arm  :  expected in-place resolve to work and rename-following to"
  echo "                      stop (rc=$nrc):"
  printf '%s\n' "$nb" | sed 's/^/      /'; fail=1
fi

[ "$fail" = 0 ] && echo "carbon-alias: PASS" || echo "carbon-alias: FAIL"
exit $fail
