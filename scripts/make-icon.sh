#!/bin/bash
set -euo pipefail
# Official FileManager/resource.rc selects FM.ico (not the About-box logo).
# Preserve that upstream ICO and convert it to the macOS bundle format.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d "${TMPDIR:-/tmp}/7zip-icon.XXXXXX")
trap 'rm -rf "$WORK"' EXIT
mkdir "$WORK/7zip.iconset"
sips -s format png "$ROOT/resources/icons/FM.ico" --out "$WORK/source.png" >/dev/null
cp "$WORK/source.png" "$ROOT/resources/icons/FM.png"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" "$WORK/source.png" --out "$WORK/7zip.iconset/icon_${size}x${size}.png" >/dev/null
  double=$((size * 2))
  sips -z "$double" "$double" "$WORK/source.png" --out "$WORK/7zip.iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$WORK/7zip.iconset" -o "$ROOT/resources/7zip.icns"
