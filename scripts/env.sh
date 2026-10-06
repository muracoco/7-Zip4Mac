#!/bin/bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DEPS=${SEVENZIP_DEPS_DIR:-"$HOME/.cache/7zip-mac-port"}
export PATH="$DEPS/tools/bin:$PATH"
QT_PREFIX=${QT_PREFIX:-"$DEPS/Qt/6.11.3/macos"}
if [[ ! -f "$QT_PREFIX/lib/cmake/Qt6/Qt6Config.cmake" ]] && command -v brew >/dev/null; then
  BREW_QT_PREFIX=$(brew --prefix qt 2>/dev/null || true)
  if [[ -x "$BREW_QT_PREFIX/bin/qmake" && $("$BREW_QT_PREFIX/bin/qmake" -query QT_VERSION) == 6.11.3 ]]; then
    QT_PREFIX="$BREW_QT_PREFIX"
  fi
fi
ARCH=$(uname -m)
case "$ARCH" in arm64) ZARCH=arm64;; x86_64) ZARCH=x64;; *) echo "Unsupported architecture: $ARCH" >&2; exit 1;; esac
UPSTREAM_ENV=$(python3 "$ROOT/scripts/upstream.py" shell)
eval "$UPSTREAM_ENV"
SEVENZIP_SOURCE="$DEPS/7zip-$SEVENZIP_VERSION"
SEVENZIP_BINARY="$SEVENZIP_SOURCE/CPP/7zip/Bundles/Alone2/b/m_${ZARCH}_15/7zz"
DEFAULT_BUILD="$ROOT/build"
if [[ $(df "$ROOT" | tail -n 1 | awk '{print $1}') == //* ]]; then
  DEFAULT_BUILD="$DEPS/build"
fi
COCOA_BUILD_HASH=$({ cat "$ROOT/qt-cocoa/accessibility-ownership.patch" "$ROOT/qt-cocoa/widget-cell-ownership.patch" "$ROOT/qt-cocoa/CMakeLists.txt"; printf '%s\n' "$QT_PREFIX" "$ARCH"; } | shasum -a 256 | awk '{print $1}')
COCOA_WORK="$DEPS/qt-cocoa-6.11.3-${COCOA_BUILD_HASH:0:12}"
COCOA_PLUGIN="$COCOA_WORK/build/plugins/platforms/libqcocoa.dylib"
ensure_app_closed() {
  python3 - "$1" <<'PY'
import pathlib, subprocess, sys
exe=str(pathlib.Path(sys.argv[1]).resolve()/'Contents/MacOS/7-Zip Mac')
for line in subprocess.check_output(['ps','-axo','pid=,command='],text=True).splitlines():
    parts=line.strip().split(maxsplit=1)
    if len(parts)==2 and (parts[1]==exe or parts[1].startswith(exe+' ')):
        print('Close the running app before rebuilding or packaging: '+sys.argv[1]+' (PID '+parts[0]+')',file=sys.stderr)
        sys.exit(1)
PY
}
