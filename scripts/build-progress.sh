#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
WORK="$DEPS/progress-$SEVENZIP_VERSION-$ZARCH"
mkdir -p "$WORK"
python3 "$ROOT/scripts/import-progress.py" "$SEVENZIP_SOURCE" "$WORK/overlay"
python3 "$ROOT/scripts/upstream.py" regenerate "$SEVENZIP_SOURCE"
TARGETS=(port_progress)
if [[ ${PORT_BUILD_TESTS:-OFF} == ON ]]; then TARGETS+=(port_metadata_test port_benchmark_test); fi
MACOSX_DEPLOYMENT_TARGET=15.0 make -C "$SEVENZIP_SOURCE/CPP/7zip/Bundles/Alone2" -f "../../cmpl_mac_$ZARCH.mak" -f "$ROOT/scripts/progress.mak" O="b/m_${ZARCH}_15" \
  PROGRESS_WORK="$WORK" PROGRESS_OVERLAY="$WORK/overlay" PROGRESS_SOURCE_ROOT="$SEVENZIP_SOURCE" PROGRESS_PORT_SOURCE="$ROOT/src" "${TARGETS[@]}" -j6
# Replace the inode rather than overwrite an executed Mach-O in place. macOS
# may retain the old code-signature cache even when the new file verifies.
python3 - "$WORK" "$(dirname "$SEVENZIP_BINARY")" <<'PY'
import os, pathlib, shutil, sys, tempfile
source, target = map(pathlib.Path, sys.argv[1:])
names = ['7zz-progress']
if os.environ.get('PORT_BUILD_TESTS') == 'ON': names += ['7zip-metadata-tests', '7zip-benchmark-tests']
for name in names:
    fd, temporary = tempfile.mkstemp(prefix='.' + name + '-', dir=target)
    os.close(fd)
    try:
        shutil.copy2(source / name, temporary)
        os.replace(temporary, target / name)
    finally:
        if os.path.exists(temporary): os.unlink(temporary)
PY
python3 "$ROOT/scripts/update-formats.py" "$SEVENZIP_SOURCE" "$SEVENZIP_BINARY"
