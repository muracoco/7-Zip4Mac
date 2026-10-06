#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
BUILD=${1:-"$DEFAULT_BUILD"}
[[ "$BUILD" = /* ]] || BUILD="$ROOT/$BUILD"
DESTINATION=${2:-'/Applications/7-Zip Mac.app'}
python3 "$ROOT/scripts/finder-registration.py" install "$BUILD/7-Zip Mac.app" "$DESTINATION" \
  --managed-root "$DEPS" --managed-root "$ROOT"
