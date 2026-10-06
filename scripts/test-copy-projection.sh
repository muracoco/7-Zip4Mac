#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
LOGS=${2:-"$ROOT/test-results/copy-projection"}
mkdir -p "$LOGS"
ensure_app_closed "$BUILD/7-Zip Mac.app"
# Finish the complete source group before running its affected checks once.
"$ROOT/scripts/build-progress.sh" 2>&1 | tee "$LOGS/native-build.log"
cmake --build "$BUILD" --target SevenZipMac copy_workflow_tests archive_transfer_tests overwrite_tests property_tests \
  2>&1 | tee "$LOGS/build.log"
ctest --test-dir "$BUILD" --verbose --output-on-failure \
  -R '^(copy_workflow|archive_transfer|overwrite|properties|native_metadata_formatter)$' \
  2>&1 | tee "$LOGS/group.log"
