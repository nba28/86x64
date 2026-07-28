#!/bin/bash
# Run the wake-up discriminator N times per arm and report a FREQUENCY.
# usage: wakeN.sh <N> [arm ...]      arms: none | move | click
SCR=${TMPDIR:-/tmp}/86x64-scratch
N="${1:-3}"; shift
ARMS="${*:-none move}"
for arm in $ARMS; do
  echo "############ ARM=$arm ############"
  for i in $(seq 1 "$N"); do
    echo "---- $arm run $i ----"
    bash "$SCR/wakeup.sh" "$arm" 2>&1 | grep -E "RESULT|parked in|STILL PARKED|MODAL RETURNED|PROCESS GONE|clicked|DELIVER|CONTROL"
  done
done
