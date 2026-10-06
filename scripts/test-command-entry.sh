#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
LOGS=${2:-"$ROOT/test-results/command-entry"}
mkdir -p "$LOGS"
# Qt event-driven tests use the existing patched Cocoa plugin. This does not
# claim physical Finder, AppKit menu clicks or Fn-key verification.
ctest --test-dir "$BUILD" --verbose --output-on-failure \
  -R '^(command_entry|panel_listing|panel_selection|file_comments|dialog_resources|overwrite|progress_completion)$' \
  2>&1 | tee "$LOGS/group.log"
"$ROOT/scripts/test-agent-tree.sh" "$BUILD" agent_selection_tests \
  guiSameNameEditorWriteBack guiSameNameComment guiAncestorFolderRestoration guiSameNameRenameAndDelete \
  2>&1 | tee "$LOGS/agent.log"
PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" "$BUILD/port_tests" "$SEVENZIP_BINARY" \
  filePauseResumeAndCancel splitCombineAndFailures fileCopyMoveRenameFolder guiArchiveFolderCreation \
  2>&1 | tee "$LOGS/file-operations.log"
