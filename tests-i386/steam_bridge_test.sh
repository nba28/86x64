#!/bin/bash
#
# steam_bridge_test.sh — A/B guard for the Steamworks bridge (src/99_steam_bridge.c
# has the story). The translated fixture reaches a fake NATIVE libsteam_api
# through the bridge's proxies and checks the i386 return conventions.
#
# ON  = ABICONV_STEAM_API=<fake>: exit 42.
# OFF = ABICONV_STEAM_API=/nonexistent: no interfaces, exit 3.
set -u
cd "$(dirname "$0")"
BIN=build/99_steam_bridge.x86_64
FAKE=build/libsteam_api_fake.dylib
[ -x "$BIN" ] || { echo "steam-bridge: FAIL ($BIN missing)"; exit 1; }
clang -arch x86_64 -dynamiclib -O1 -o "$FAKE" src/steam_bridge_fake.c || { echo "steam-bridge: FAIL (fake lib build)"; exit 1; }

ABICONV_STEAM_API="$PWD/$FAKE" "$BIN" >/dev/null 2>&1; on=$?
ABICONV_STEAM_API=/nonexistent "$BIN" >/dev/null 2>&1; off=$?
[ "$on" = 42 ] && [ "$off" = 3 ] && { echo "steam-bridge: PASS (on=$on off=$off)"; exit 0; }
echo "steam-bridge: FAIL (on=$on off=$off)"; exit 1
