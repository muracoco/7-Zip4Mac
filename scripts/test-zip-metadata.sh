#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
LOGS=${2:-"$ROOT/test-results/zip-metadata"}
mkdir -p "$LOGS"
ensure_app_closed "$BUILD/7-Zip Mac.app"
# Complete the feature group before testing. These are affected metadata,
# native-row restoration and editor update paths, not a full format regression.
"$ROOT/scripts/build-progress.sh" 2>&1 | tee "$LOGS/progress-build.log"
"$ROOT/scripts/build-zip-comment.sh" 2>&1 | tee "$LOGS/comment-build.log"
cmake --build "$BUILD" --target SevenZipMac zip_metadata_tests comment_tests panel_listing_tests editor_tests progress_completion_tests copy_workflow_tests agent_selection_tests port_tests \
  2>&1 | tee "$LOGS/gui-build.log"
ctest --test-dir "$BUILD" --verbose --output-on-failure \
  -R '^(zip_metadata|file_comments|panel_listing|editor_writeback|progress_completion|copy_workflow)$' 2>&1 | tee "$LOGS/group.log"
"$ROOT/scripts/test-agent-tree.sh" "$BUILD" agent_selection_tests \
  guiSameNameEditorWriteBack guiSameNameComment guiSameNameRenameAndDelete \
  2>&1 | tee "$LOGS/native-rows.log"
PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" "$BUILD/port_tests" "$SEVENZIP_BINARY" \
  updateRenameDelete encryption encryptedUpdate archiveFileReplacement guiArchiveFolderCreation \
  2>&1 | tee "$LOGS/updates.log"
