#!/bin/bash
# Retranslate every previously-translated binary in iPhoto.app (+ iPhotoAccess
# framework) with the freshly fixed translator. Seeds ~/.86x64-backups entries
# from in-place .bak files where the backup root lacks them.
set -u
APP=$HOME/Downloads/iLife11/Applications/iPhoto.app
FW=$HOME/Downloads/iLife11/Library/Frameworks/iPhotoAccess.framework
TB=$HOME/projects/86x64/src/86x64/translate-bundle
APP_HASH_ROOT="$HOME/.86x64-backups/$(echo "$APP" | shasum -a 256 | cut -c1-16)"
FW_HASH_ROOT="$HOME/.86x64-backups/$(echo "$FW" | shasum -a 256 | cut -c1-16)"

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
    if bash "$TB" "$bundle" "$rel" > /tmp/tb_one_iphoto.log 2>&1; then
        pass=$((pass+1)); echo "OK  $rel"
    else
        fail=$((fail+1)); failed_list="$failed_list $rel"
        echo "FAIL $rel"; tail -8 /tmp/tb_one_iphoto.log | sed 's/^/    /'
    fi
}

# Boot-critical first: main exec + helpers + top-level frameworks.
RELS=(
"Contents/MacOS/iPhoto"
"Contents/MacOS/photocd"
"Contents/MacOS/dbRepair"
"Contents/Frameworks/ProKit.framework/Versions/A/ProKit"
"Contents/Frameworks/Tessera.framework/Versions/A/Tessera"
"Contents/Frameworks/Tellus.framework/Versions/A/Tellus"
"Contents/Frameworks/MobileMe.framework/Versions/A/MobileMe"
"Contents/Frameworks/UpgradeChecker.framework/Versions/A/UpgradeChecker"
"Contents/Frameworks/AccountConfigurationPlugin.framework/Versions/A/AccountConfigurationPlugin"
"Contents/Frameworks/QTKit.framework/Versions/A/QTKit"
"Contents/Frameworks/iLifeMediaBrowser.framework/Versions/A/iLifeMediaBrowser"
"Contents/Frameworks/iLifeFaceRecognition.framework/Versions/A/iLifeFaceRecognition"
"Contents/Frameworks/iLifePageLayout.framework/Versions/A/iLifePageLayout"
"Contents/Frameworks/iLifePageLayout.framework/Versions/A/Frameworks/iLifeImageAnalysis.framework/Versions/A/iLifeImageAnalysis"
"Contents/Frameworks/iLifeSlideshow.framework/Versions/A/Frameworks/iLifeSlideshowRenderer.framework/Versions/A/iLifeSlideshowRenderer"
"Contents/Frameworks/iLifeSlideshow.framework/Versions/A/Frameworks/iLifeSlideshowProducer.framework/Versions/A/iLifeSlideshowProducer"
"Contents/Frameworks/iLifeSlideshow.framework/Versions/A/Frameworks/iLifeSlideshowExporter.framework/Versions/A/iLifeSlideshowExporter"
"Contents/Frameworks/Python.framework/Versions/2.6/Resources/Python Launcher.app/Contents/MacOS/Python Launcher"
"Contents/Frameworks/Python.framework/Versions/2.6/Extras/lib/python/py2app/bundletemplate/prebuilt/main"
)
# PyObjC AppKit .so shims (bundle pruned to Python 2.6 only)
for v in 2.6; do
  for so in _inlines _appmain _nsview _nswindow _carbon _nsbezierpath _nsbitmap _nsfont _nsquickdrawview; do
    RELS+=("Contents/Frameworks/Python.framework/Versions/$v/Extras/lib/python/PyObjC/AppKit/$so.so")
  done
done

for rel in "${RELS[@]}"; do
    do_one "$APP" "$rel" "$APP_HASH_ROOT"
done
do_one "$FW" "Versions/A/iPhotoAccess" "$FW_HASH_ROOT"

echo ""
echo "===== retranslate: $pass ok, $fail failed ====="
[ -n "$failed_list" ] && echo "failed:$failed_list"
exit 0
