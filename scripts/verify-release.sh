#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
NO_FOCUS=0
if [[ ${1:-} == --no-focus ]]; then NO_FOCUS=1; shift; fi
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
REPORT=${PORT_RELEASE_REPORT:-"$ROOT/test-results/release-$(date +%Y%m%d-%H%M%S)"}
mkdir -p "$REPORT"
: > "$REPORT/results.tsv"
RESULT=0
step() {
  local name=$1 status=0
  shift
  echo "Release verification: $name"
  if python3 "$ROOT/scripts/run-checked.py" --timeout "${PORT_RELEASE_TIMEOUT:-900}" "$@" > "$REPORT/$name.log" 2>&1; then
    status=0
  else
    status=$?
    RESULT=1
    tail -n 40 "$REPORT/$name.log"
  fi
  printf '%s\t%s\n' "$name" "$status" >> "$REPORT/results.tsv"
  echo "$name: exit $status ($REPORT/$name.log)"
}
# Independent stages all run once, even if a preceding stage fails. This does
# not rebuild, retry cases or turn unavailable physical checks into successes.
if [[ $NO_FOCUS -eq 1 ]]; then
  step application "$ROOT/scripts/test.sh" --no-focus "$BUILD"
else
  step application "$ROOT/scripts/test.sh" "$BUILD"
fi
step agent-tree "$ROOT/scripts/test-agent-tree.sh" "$BUILD" agent_tree_tests
step agent-selection "$ROOT/scripts/test-agent-tree.sh" "$BUILD" agent_selection_tests
step formats "$ROOT/scripts/test-formats.sh" --build "$BUILD"
step bundle-startup python3 "$ROOT/scripts/test-bundle-startup.py" "$BUILD/7-Zip Mac.app"
if [[ $NO_FOCUS -eq 1 ]]; then
  echo 'Pending: AppKit native-menu/focus, physical Finder/Open With/drop and Fn/modifier checks.'
  printf 'native-input\tPENDING\n' >> "$REPORT/results.tsv"
fi
echo "Release reports: $REPORT"
exit "$RESULT"
