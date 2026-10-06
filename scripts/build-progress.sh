#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
WORK="$DEPS/progress-26.03-$ZARCH"
mkdir -p "$WORK"
python3 "$ROOT/scripts/import-progress.py" "$SEVENZIP_SOURCE" "$WORK/overlay"
python3 "$ROOT/scripts/import-benchmark.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-folder-update.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-agent-properties.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-copy-name.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-agent-selection.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-temp-browser.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-panel-selection.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-panel-key.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-process-discovery.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-open-profile.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-panel-menu-drag.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-version-control.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-time-menu.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-panel-sort.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-panel-display.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-language.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-ui-resource-ids.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-dialog-geometry.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-progress-presentation.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-progress-completion.py" "$SEVENZIP_SOURCE"
python3 "$ROOT/scripts/import-file-comments.py" "$SEVENZIP_SOURCE"
MACOSX_DEPLOYMENT_TARGET=15.0 make -C "$SEVENZIP_SOURCE/CPP/7zip/Bundles/Alone2" -f "../../cmpl_mac_$ZARCH.mak" -f "$ROOT/scripts/progress.mak" O="b/m_${ZARCH}_15" \
  PROGRESS_WORK="$WORK" PROGRESS_OVERLAY="$WORK/overlay" PROGRESS_SOURCE_ROOT="$SEVENZIP_SOURCE" PROGRESS_PORT_SOURCE="$ROOT/src" port_progress port_metadata_test port_benchmark_test -j6
# Replace the inode rather than overwrite an executed Mach-O in place. macOS
# may retain the old code-signature cache even when the new file verifies.
python3 - "$WORK" "$(dirname "$SEVENZIP_BINARY")" <<'PY'
import os, pathlib, shutil, sys, tempfile
source, target = map(pathlib.Path, sys.argv[1:])
for name in ('7zz-progress', '7zip-metadata-tests', '7zip-benchmark-tests'):
    fd, temporary = tempfile.mkstemp(prefix='.' + name + '-', dir=target)
    os.close(fd)
    try:
        shutil.copy2(source / name, temporary)
        os.replace(temporary, target / name)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)
PY
python3 "$ROOT/scripts/update-formats.py" "$SEVENZIP_SOURCE" "$SEVENZIP_BINARY"
