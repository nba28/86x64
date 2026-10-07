#!/bin/bash
# usage: configure-worktree.sh [worktree-root]
# Configure a worktree's build tree with the same ABICONV_MODERN_TARGETS as
# the main checkout (m64 resolves tools script-relative, so a worktree needs its
# own build or it silently deploys master's libabiconv).
set -e
W="${1:-$(git rev-parse --show-toplevel)}"   # worktree root (default: the one we are in)
MAIN=$(dirname "$(git -C "$W" rev-parse --path-format=absolute --git-common-dir)")   # main checkout
MT=$(grep '^ABICONV_MODERN_TARGETS:STRING=' "$MAIN/build/CMakeCache.txt" | cut -d= -f2-)
cmake -S "$W" -B "$W/build" -DCMAKE_BUILD_TYPE=Debug -DABICONV_MODERN_TARGETS="$MT"
