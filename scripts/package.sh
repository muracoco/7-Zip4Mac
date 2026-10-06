#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
APP="$BUILD/7-Zip Mac.app"
ensure_app_closed "$APP"
[[ -f "$COCOA_PLUGIN" ]] || "$ROOT/scripts/build-qt-cocoa.sh"
"$QT_PREFIX/bin/macdeployqt" "$APP" -always-overwrite -no-codesign
cp "$COCOA_PLUGIN" "$APP/Contents/PlugIns/platforms/libqcocoa.dylib"
install_name_tool -delete_rpath "$QT_PREFIX/lib" -add_rpath '@loader_path/../../Frameworks' "$APP/Contents/PlugIns/platforms/libqcocoa.dylib"
mkdir -p "$APP/Contents/Resources/Licenses"
mkdir -p "$APP/Contents/Resources/Lang"
cp "$ROOT/resources/Lang/"*.txt "$ROOT/resources/Lang/en.ttt" "$APP/Contents/Resources/Lang/"
cp -R "$ROOT/licenses/." "$APP/Contents/Resources/Licenses/"
cp "$SEVENZIP_SOURCE/DOC/License.txt" "$APP/Contents/Resources/Licenses/7-Zip-License.txt"
cp "$SEVENZIP_SOURCE/DOC/copying.txt" "$APP/Contents/Resources/Licenses/7-Zip-LGPL-2.1.txt"
cp "$SEVENZIP_SOURCE/DOC/unRarLicense.txt" "$APP/Contents/Resources/Licenses/unRarLicense.txt"
echo "$SEVENZIP_SHA256  $DEPS/$SEVENZIP_ARCHIVE" | shasum -a 256 -c -
cp "$DEPS/$SEVENZIP_ARCHIVE" "$APP/Contents/Resources/$SEVENZIP_ARCHIVE"
QT_SOURCE_VERSION=$("$QT_PREFIX/bin/qmake" -query QT_VERSION)
QT_SOURCE_ARCHIVE="$DEPS/qtbase-$QT_SOURCE_VERSION-src.tar.gz"
if [[ ! -f "$QT_SOURCE_ARCHIVE" ]]; then
  "$ROOT/scripts/fetch-qt-source.sh"
  COPYFILE_DISABLE=1 tar -czf "$QT_SOURCE_ARCHIVE" -C "$DEPS/QtSources/$QT_SOURCE_VERSION/Src" qtbase
fi
cp "$QT_SOURCE_ARCHIVE" "$APP/Contents/Resources/"
python3 "$ROOT/scripts/source-archive.py" "$ROOT" "$APP/Contents/Resources/7zip-mac-port-src.tar.gz"
python3 "$ROOT/scripts/check-distribution.py" "$APP"
# Repackaging a formerly installed bundle must not create a second opener.
python3 "$ROOT/scripts/finder-registration.py" configure "$APP" --rank None
chmod -R u+rwX,go+rX "$APP"
xattr -cr "$APP"
codesign --force --deep --sign - "$APP"
"$ROOT/scripts/check-bundle.sh" "$APP"
echo "$APP"
