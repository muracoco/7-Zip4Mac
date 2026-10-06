#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
LOGS=${2:-"$ROOT/test-results/copy-workflow"}
mkdir -p "$LOGS"
cmake --build "$BUILD" --target SevenZipMac copy_workflow_tests archive_transfer_tests overwrite_tests dialog_resource_tests port_tests -j8
# Run once after the complete Copy/Move/Combine implementation group.
# Following repairs rerun only failed cases and directly affected suites.
ctest --test-dir "$BUILD" --verbose --output-on-failure \
  -R '^(copy_workflow|archive_transfer|overwrite|dialog_resources)$' \
  2>&1 | tee "$LOGS/group.log"
PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" "$BUILD/port_tests" "$SEVENZIP_BINARY" \
  filePauseResumeAndCancel splitCombineAndFailures fileCopyMoveRenameFolder \
  2>&1 | tee "$LOGS/file-operations.log"
