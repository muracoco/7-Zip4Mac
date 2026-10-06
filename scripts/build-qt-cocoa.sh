#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
[[ $("$QT_PREFIX/bin/qmake" -query QT_VERSION) == 6.11.3 ]] || { echo 'The Cocoa ownership fix requires exactly Qt 6.11.3.' >&2; exit 1; }
"$ROOT/scripts/fetch-qt-source.sh"
if [[ ! -f "$COCOA_WORK/source/.patched" ]]; then
  mkdir -p "$COCOA_WORK/source/src/plugins/platforms" "$COCOA_WORK/source/tests/auto/other"
  cp -R "$DEPS/QtSources/6.11.3/Src/qtbase/src/plugins/platforms/cocoa" "$COCOA_WORK/source/src/plugins/platforms/"
  cp -R "$DEPS/QtSources/6.11.3/Src/qtbase/tests/auto/other/qaccessibilitymac" "$COCOA_WORK/source/tests/auto/other/"
  patch -d "$COCOA_WORK/source" -p1 < "$ROOT/qt-cocoa/accessibility-ownership.patch"
  patch -d "$COCOA_WORK/source" -p1 < "$ROOT/qt-cocoa/widget-cell-ownership.patch"
  touch "$COCOA_WORK/source/.patched"
fi
mkdir -p "$COCOA_WORK/project"
if ! cmp -s "$ROOT/qt-cocoa/CMakeLists.txt" "$COCOA_WORK/project/CMakeLists.txt"; then
  cp "$ROOT/qt-cocoa/CMakeLists.txt" "$COCOA_WORK/project/CMakeLists.txt"
fi
cmake -S "$COCOA_WORK/project" -B "$COCOA_WORK/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=14.0 -DCMAKE_PREFIX_PATH="$QT_PREFIX" \
  -DCOCOA_SOURCE="$COCOA_WORK/source/src/plugins/platforms/cocoa"
cmake --build "$COCOA_WORK/build" --parallel 6
