#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
ensure_app_closed "$BUILD/7-Zip Mac.app"
[[ -x "$SEVENZIP_BINARY" ]] || { echo 'Run ./scripts/bootstrap.sh first.'; exit 1; }
"$ROOT/scripts/build-qt-cocoa.sh"
"$ROOT/scripts/build-zip-comment.sh"
"$ROOT/scripts/build-progress.sh"
cmake -S "$ROOT" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_PREFIX_PATH="$QT_PREFIX" -DSEVENZIP_BINARY="$SEVENZIP_BINARY" -DPORT_COCOA_PLUGIN_DIR="$COCOA_WORK/build/plugins/platforms" -DBUILD_TESTING="${PORT_BUILD_TESTS:-OFF}"
cmake --build "$BUILD" --parallel 6
"$ROOT/scripts/package.sh" "$BUILD"
