#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD="$DEFAULT_BUILD"
MANIFEST=""
AVAILABLE_ONLY=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --available-only) AVAILABLE_ONLY=1; shift;;
    --manifest) MANIFEST=$2; shift 2;;
    --build) BUILD=$2; shift 2;;
    *) echo 'Usage: scripts/test-formats.sh [--build DIR] [--manifest FILE] [--available-only]' >&2; exit 1;;
  esac
done
[[ -x "$BUILD/format_tests" ]] || { echo 'Run scripts/build.sh first.' >&2; exit 1; }
REPORT="$ROOT/test-results/formats-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$REPORT"
if [[ -z "$MANIFEST" ]]; then
  FIXTURES=$(mktemp -d "${TMPDIR:-/tmp}/7zip-formats.XXXXXX")
  rmdir "$FIXTURES" # generator requires a new directory; this empty directory belongs to us
  trap 'rm -rf "$FIXTURES"' EXIT
  python3 "$ROOT/tests/make-format-fixtures.py" "$SEVENZIP_BINARY" "$FIXTURES" --sevenzip-source "$SEVENZIP_SOURCE" --fixture-tools "$DEPS/fixture-tools" | tee "$REPORT/generation.log"
  MANIFEST="$FIXTURES/manifest.json"
fi
set +e
PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" "$BUILD/format_tests" "$SEVENZIP_BINARY" "$MANIFEST" -o -,txt -o "$REPORT/qt.xml",xml > "$REPORT/qt.log" 2>&1
RESULT=$?
set -e
cat "$REPORT/qt.log"
python3 "$ROOT/scripts/format-coverage.py" "$MANIFEST" "$REPORT/qt.xml" "$REPORT/coverage.md" "$REPORT/coverage.json"
if [[ $AVAILABLE_ONLY -eq 0 ]] && ! python3 - "$MANIFEST" <<'PY'
import json, sys
data=json.load(open(sys.argv[1]))
if data['pending'] or data['generationFailures']:
    print(f"Incomplete: {len(data['pending'])} registration fixtures pending; {len(data['generationFailures'])} generation failures.")
    sys.exit(1)
PY
then RESULT=1; fi
echo "Format reports: $REPORT"
exit "$RESULT"
