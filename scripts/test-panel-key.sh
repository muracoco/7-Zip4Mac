#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
LOGS=${2:-"$ROOT/test-results/panel-key"}
mkdir -p "$LOGS"
ensure_app_closed "$BUILD/7-Zip Mac.app"
"$ROOT/scripts/build-progress.sh" 2>&1 | tee "$LOGS/engine-build.log"
cmake --build "$BUILD" --target SevenZipMac panel_key_tests command_entry_tests statistics_tests panel_selection_tests copy_workflow_tests agent_selection_tests \
  2>&1 | tee "$LOGS/gui-build.log"
ctest --test-dir "$BUILD" --verbose --output-on-failure \
  -R '^(panel_key|command_entry|folder_statistics|panel_selection|copy_workflow)$' 2>&1 | tee "$LOGS/group.log"
"$ROOT/scripts/test-agent-tree.sh" "$BUILD" agent_selection_tests nativeUpdatesAcrossWriters guiSameNameRenameAndDelete \
  2>&1 | tee "$LOGS/native-rename.log"
