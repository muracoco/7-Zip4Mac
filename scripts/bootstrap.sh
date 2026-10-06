#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
[[ $(uname -s) == Darwin ]] || { echo 'This build requires macOS.'; exit 1; }
xcode-select -p >/dev/null || { echo 'Run xcode-select --install to install Command Line Tools.'; exit 1; }
mkdir -p "$DEPS"
# No sudo, Homebrew installation, or global Python package changes.
if ! command -v cmake >/dev/null || ! command -v ninja >/dev/null || [[ ! -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
  echo 'Installing user-local tools: CMake 3.31.6, Ninja 1.11.1.4, aqtinstall 3.3.0; Qt qtbase 6.11.3 (official binary).'
  [[ -x "$DEPS/tools/bin/python3" ]] || python3 -m venv "$DEPS/tools"
  "$DEPS/tools/bin/python3" -m pip install 'cmake==3.31.6' 'ninja==1.11.1.4' 'aqtinstall==3.3.0'
  if [[ ! -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    "$DEPS/tools/bin/aqt" install-qt mac desktop 6.11.3 clang_64 -O "$DEPS/Qt" --archives qtbase
  fi
fi
ARCHIVE="$DEPS/7z2603-src.tar.xz"
[[ -f "$ARCHIVE" ]] || curl -fL --retry 3 https://github.com/ip7z/7zip/releases/download/26.03/7z2603-src.tar.xz -o "$ARCHIVE"
echo '9cbde5099c6deb73691b0579063da5827522ccbbcba3f0020fd04e8c8c16c0d4  '"$ARCHIVE" | shasum -a 256 -c -
if [[ ! -f "$SEVENZIP_SOURCE/CPP/7zip/MyVersion.h" ]]; then
  mkdir -p "$SEVENZIP_SOURCE"
  tar -xf "$ARCHIVE" -C "$SEVENZIP_SOURCE"
fi
MACOSX_DEPLOYMENT_TARGET=15.0 make -C "$SEVENZIP_SOURCE/CPP/7zip/Bundles/Alone2" -f "../../cmpl_mac_$ZARCH.mak" O="b/m_${ZARCH}_15" -j"$(sysctl -n hw.logicalcpu)"
"$SEVENZIP_BINARY" i | sed -n '1,10p'
"$ROOT/scripts/build-zip-comment.sh"
"$ROOT/scripts/build-progress.sh"
"$QT_PREFIX/bin/qmake" --version
echo 'Ready. Run ./scripts/build.sh'
