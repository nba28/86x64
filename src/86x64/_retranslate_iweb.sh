#!/bin/bash
# Retranslate every previously-translated i386-only binary in iWeb.app with the
# freshly fixed translator. Mirror of _retranslate_iphoto.sh. The translate
# seeds from ~/.86x64-backups (populated by the first translation pass) or an
# in-place .bak. iWeb shares the iWork "SF*" framework family; all 15 targets
# below are confirmed i386-only (lipo) in the backup root.
#
# After this, run (parameterized for iWeb):
#   python3 src/86x64/_rpath_sweep.py     $HOME/Downloads/iLife11/Applications/iWeb.app
#   python3 src/86x64/shimgen.py          <App> <rel>...   (see _shimgen_targets_iweb.txt)
#   resync libabiconv copies + _fix_bundle_signing.py <App> + codesign --identifier iWeb
set -u
APP=$HOME/Downloads/iLife11/Applications/iWeb.app
TB=$HOME/projects/86x64/src/86x64/translate-bundle
APP_HASH_ROOT="$HOME/.86x64-backups/$(echo "$APP" | shasum -a 256 | cut -c1-16)"

pass=0; fail=0; failed_list=""

do_one() {  # $1 = bundle, $2 = relpath, $3 = backup_root
    local bundle="$1" rel="$2" broot="$3"
    local bak="$bundle/$rel.bak" seed="$broot/$rel"
    if [ ! -f "$seed" ]; then
        if [ -f "$bak" ]; then
            mkdir -p "$(dirname "$seed")"
            cp "$bak" "$seed"
            echo "    seeded backup from .bak"
        else
            echo "!! $rel: no backup and no .bak — SKIP"
            fail=$((fail+1)); failed_list="$failed_list $rel(no-src)"
            return
        fi
    fi
    if bash "$TB" "$bundle" "$rel" > /tmp/tb_one.log 2>&1; then
        pass=$((pass+1)); echo "OK  $rel"
    else
        fail=$((fail+1)); failed_list="$failed_list $rel"
        echo "FAIL $rel"; tail -8 /tmp/tb_one.log | sed 's/^/    /'
    fi
}

# Boot-critical main exec first, then the shared SF* framework graph.
RELS=(
"Contents/MacOS/iWeb"
"Contents/Frameworks/SFUtility.framework/Versions/A/SFUtility"
"Contents/Frameworks/SFArchiving.framework/Versions/A/SFArchiving"
"Contents/Frameworks/SFLicense.framework/Versions/A/SFLicense"
"Contents/Frameworks/SFApplication.framework/Versions/A/SFApplication"
"Contents/Frameworks/SFRendering.framework/Versions/A/SFRendering"
"Contents/Frameworks/SFDrawables.framework/Versions/A/SFDrawables"
"Contents/Frameworks/SFStyles.framework/Versions/A/SFStyles"
"Contents/Frameworks/SFControls.framework/Versions/A/SFControls"
"Contents/Frameworks/SFInspectors.framework/Versions/A/SFInspectors"
"Contents/Frameworks/SFAnimation.framework/Versions/A/SFAnimation"
"Contents/Frameworks/SFProofReader.framework/Versions/A/SFProofReader"
"Contents/Frameworks/SFWordProcessing.framework/Versions/A/SFWordProcessing"
"Contents/Frameworks/FTPKit.framework/Versions/A/FTPKit"
"Contents/Frameworks/MobileMe.framework/Versions/A/MobileMe"
)

for rel in "${RELS[@]}"; do
    do_one "$APP" "$rel" "$APP_HASH_ROOT"
done

echo ""
echo "===== iWeb retranslate: $pass OK, $fail FAIL ====="
[ -n "$failed_list" ] && echo "failed:$failed_list"
exit 0
