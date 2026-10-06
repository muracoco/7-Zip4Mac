#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
LOGS=${2:-"$ROOT/test-results/address-workflow"}
mkdir -p "$LOGS"
ensure_app_closed "$BUILD/7-Zip Mac.app"
python3 "$ROOT/scripts/import-panel-key.py" "$SEVENZIP_SOURCE"
cmake --build "$BUILD" --target SevenZipMac address_workflow_tests panel_key_tests command_entry_tests panel_listing_tests panel_selection_tests statistics_tests copy_workflow_tests \
  2>&1 | tee "$LOGS/build.log"
ctest --test-dir "$BUILD" --verbose --output-on-failure \
  -R '^(address_workflow|panel_key|command_entry|panel_listing|panel_selection|folder_statistics|copy_workflow)$' \
  2>&1 | tee "$LOGS/group.log"
