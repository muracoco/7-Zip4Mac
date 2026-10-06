#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/env.sh"
python3 "$ROOT/scripts/upstream.py" verify "$SEVENZIP_SOURCE"
WORK="$DEPS/zip-comment-$SEVENZIP_VERSION-$ZARCH"
mkdir -p "$WORK"
OVERLAY="$DEPS/progress-$SEVENZIP_VERSION-$ZARCH/overlay"
python3 "$ROOT/scripts/import-progress.py" "$SEVENZIP_SOURCE" "$OVERLAY"
MACOSX_DEPLOYMENT_TARGET=15.0 make -C "$SEVENZIP_SOURCE/CPP/7zip/Bundles/Alone2" -f "../../cmpl_mac_$ZARCH.mak" -f "$ROOT/scripts/zip-comment.mak" O="b/m_${ZARCH}_15" \
  COMMENT_HELPER_SOURCE="$ROOT/src/ZipCommentHelper.cpp" COMMENT_HELPER_OBJECT="$WORK/ZipCommentHelper.o" \
  COMMENT_HELPER_BINARY="$WORK/7zip-comment" COMMENT_SOURCE_ROOT="$SEVENZIP_SOURCE" \
  COMMENT_ZIP_SOURCE="$OVERLAY/CPP/7zip/Archive/Zip/ZipUpdate.cpp" COMMENT_ZIP_OBJECT="$WORK/ZipUpdate.o" port_zip_comment -j6
python3 - "$WORK/7zip-comment" "$(dirname "$SEVENZIP_BINARY")/7zip-comment" <<'PY'
import os, pathlib, shutil, sys, tempfile
source, target = map(pathlib.Path, sys.argv[1:])
fd, temporary = tempfile.mkstemp(prefix='.7zip-comment-', dir=target.parent)
os.close(fd)
try:
    shutil.copy2(source, temporary)
    os.replace(temporary, target)
finally:
    if os.path.exists(temporary): os.unlink(temporary)
PY
