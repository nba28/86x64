#!/bin/bash
# seed_halo_eula.sh — deploy-time pre-seed of Halo's first-launch EULA acceptance.
#
# WHY THIS EXISTS (maintainer-approved, narrowly scoped)
# ------------------------------------------------
# The translated Halo (Bungie, Mac) launches and runs its C++/ObjC init cleanly,
# but on first launch it gates behind a **legal EULA acceptance** screen — a
# Carbon 'EULA' dialog materialized from Contents/Resources/EULA.rsrc. Our Carbon
# WindowManagement/windowing bridge cannot yet display+run that modal window, so
# Halo gives up and voluntarily exit(0)s at the gate. Building the full windowing
# bridge is a large, separate, display-gated effort. To let development advance
# into Halo's renderer/window/input init (the real remaining work), we mark the
# EULA as already-accepted so Halo skips the un-renderable gate.
#
# This is a genuine legal-acceptance pre-seed — the exact preference Halo itself
# writes after a user clicks "Accept". It is NOT a general auto-accept mechanism:
# it touches ONLY the one 'EULA' key in ONLY the com.macsoft.halo domain. It does
# not affect gameplay, difficulty, settings, or any choice a user would deliberate
# on.
#
# VERIFIED GATE MECHANISM (from i386 disassembly of Halo)
# -------------------------------------------------------
#   Domain : com.macsoft.halo               (the app's CFBundleIdentifier)
#   Key    : "EULA"
#   Type   : CFData, 4 bytes  ->  01 00 00 00   (little-endian 1 == accepted)
#
#   The check (i386 fn @0x2ac944) calls a ReadPref helper (@0x2ab7a4) that does:
#       CFStringCreateWithCStringNoCopy(NULL, "EULA", ...)
#       CFPreferencesCopyAppValue(cfKey, kCFPreferencesCurrentApplication)
#   If a CFData value is returned (pref present) -> the acceptance check returns
#   TRUE and Halo skips the EULA dialog entirely. If absent, Halo shows the modal
#   EULA dialog (@0x2a866c, loads EULA.rsrc) and, on accept, writes back exactly
#   this 4-byte CFData via CFPreferencesSetAppValue (WritePref @0x2ab852). We
#   simply pre-write that same value.
#
# Because the read goes through the real (modern) CoreFoundation preferences,
# CFPreferencesCopyAppValue(...kCFPreferencesCurrentApplication) resolves to
# ~/Library/Preferences/com.macsoft.halo.plist. Seeding it here is the cleanest,
# most maintainable pre-seed: no shim, no runtime interception, just the real
# "already accepted" state. Idempotent + safe to re-run at deploy time.

set -euo pipefail

DOMAIN="com.macsoft.halo"
KEY="EULA"
# 4-byte CFData 01 00 00 00 — byte-for-byte what Halo writes on real acceptance.
ACCEPTED_HEX="01000000"

current="$(defaults read "$DOMAIN" "$KEY" 2>/dev/null || true)"
if printf '%s' "$current" | grep -q '0x01000000'; then
  echo "[seed_halo_eula] $DOMAIN $KEY already seeded (accepted); nothing to do."
  exit 0
fi

defaults write "$DOMAIN" "$KEY" -data "$ACCEPTED_HEX"

# Verify readback is a 4-byte CFData == 01 00 00 00.
if defaults read "$DOMAIN" "$KEY" 2>/dev/null | grep -q '0x01000000'; then
  echo "[seed_halo_eula] seeded EULA acceptance -> $DOMAIN $KEY = <01000000> (CFData)"
else
  echo "[seed_halo_eula] ERROR: readback did not confirm the seed" >&2
  exit 1
fi
