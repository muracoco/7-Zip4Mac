#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
QT_SOURCE_VERSION=$("$QT_PREFIX/bin/qmake" -query QT_VERSION)
if [[ ! -f "$DEPS/QtSources/$QT_SOURCE_VERSION/Src/qtbase/CMakeLists.txt" ]]; then
  if [[ ! -x "$DEPS/tools/bin/aqt" ]]; then
    [[ -x "$DEPS/tools/bin/python3" ]] || python3 -m venv "$DEPS/tools"
    "$DEPS/tools/bin/python3" -m pip install 'aqtinstall==3.3.0'
  fi
  "$DEPS/tools/bin/aqt" install-src mac desktop "$QT_SOURCE_VERSION" -O "$DEPS/QtSources" --archives qtbase
fi
echo "$DEPS/QtSources/$QT_SOURCE_VERSION/Src/qtbase"
