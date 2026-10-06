#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
SUITE=${2:-agent_tree_tests}
case "$SUITE" in agent_tree_tests|agent_selection_tests) ;; *) echo 'Unknown Agent test suite.' >&2; exit 1 ;; esac
[[ -x "$DEPS/fixture-tools/mkntfs" && -x "$DEPS/fixture-tools/ntfscp" ]] || { echo 'Run scripts/bootstrap-fixture-tools.sh for unmounted NTFS fixtures.' >&2; exit 1; }
FIXTURES=$(mktemp -d "${TMPDIR:-/tmp}/7zip-agent-tree.XXXXXX")
trap 'python3 -c "import shutil,sys; shutil.rmtree(sys.argv[1])" "$FIXTURES"' EXIT
python3 "$ROOT/tests/make-agent-tree-fixtures.py" "$FIXTURES/data" "$DEPS/fixture-tools"
PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" "$BUILD/$SUITE" "$SEVENZIP_BINARY" "$FIXTURES/data" "${@:3}"
