#!/bin/bash
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail
source "$(dirname "$0")/env.sh"
[[ $(uname -s) == Darwin ]] || { echo 'Fixture generators require macOS.' >&2; exit 1; }
mkdir -p "$DEPS/fixture-tools"
echo "Building optional fixture generators in $DEPS only (no sudo, mounts or global installation)."
fetch() {
  local name=$1 url=$2 digest=$3 archive="$DEPS/$1"
  [[ -f "$archive" ]] || curl -fL --retry 3 "$url" -o "$archive"
  echo "$digest  $archive" | shasum -a 256 -c -
}
fetch pkgconf-2.4.3.tar.xz https://distfiles.ariadne.space/pkgconf/pkgconf-2.4.3.tar.xz 51203d99ed573fa7344bf07ca626f10c7cc094e0846ac4aa0023bd0c83c25a41
if [[ ! -x "$DEPS/fixture-tools/bin/pkgconf" ]]; then
  [[ -d "$DEPS/pkgconf-2.4.3" ]] || tar -xf "$DEPS/pkgconf-2.4.3.tar.xz" -C "$DEPS"
  (cd "$DEPS/pkgconf-2.4.3"; ./configure --prefix="$DEPS/fixture-tools" --disable-shared; make -j4; make install)
fi
fetch e2fsprogs-1.47.2.tar.gz https://www.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.2/e2fsprogs-1.47.2.tar.gz 7a959221c1b1cc6e28b7d7a4e204a2ffd8ec6d8a2de4461c482b64c5f4463cca
if [[ ! -x "$DEPS/e2fsprogs-1.47.2/local-build/misc/mke2fs" ]]; then
  [[ -d "$DEPS/e2fsprogs-1.47.2" ]] || tar -xf "$DEPS/e2fsprogs-1.47.2.tar.gz" -C "$DEPS"
  mkdir -p "$DEPS/e2fsprogs-1.47.2/local-build"
  (cd "$DEPS/e2fsprogs-1.47.2/local-build"; PKG_CONFIG="$DEPS/fixture-tools/bin/pkgconf" ../configure --disable-nls --disable-fsck --disable-e2initrd-helper --enable-libuuid --enable-libblkid; make -j4)
fi
fetch squashfs-tools-4.6.1.tar.gz https://codeload.github.com/plougher/squashfs-tools/tar.gz/refs/tags/4.6.1 9c4974e07c61547dae14af4ed1f358b7d04618ae194e54d6be72ee126f0d2f53
if [[ ! -x "$DEPS/squashfs-tools-4.6.1/squashfs-tools/mksquashfs" ]]; then
  [[ -d "$DEPS/squashfs-tools-4.6.1" ]] || tar -xf "$DEPS/squashfs-tools-4.6.1.tar.gz" -C "$DEPS"
  make -C "$DEPS/squashfs-tools-4.6.1/squashfs-tools" -j4 mksquashfs XATTR_SUPPORT=0 XZ_SUPPORT=0 LZO_SUPPORT=0 ZSTD_SUPPORT=0 LZ4_SUPPORT=0 GZIP_SUPPORT=1
fi
fetch ntfs-3g_ntfsprogs-2022.10.3.tgz https://tuxera.com/opensource/ntfs-3g_ntfsprogs-2022.10.3.tgz f20e36ee68074b845e3629e6bced4706ad053804cbaf062fbae60738f854170c
if [[ ! -x "$DEPS/ntfs-3g_ntfsprogs-2022.10.3/ntfsprogs/mkntfs" ]]; then
  [[ -d "$DEPS/ntfs-3g_ntfsprogs-2022.10.3" ]] || tar -xf "$DEPS/ntfs-3g_ntfsprogs-2022.10.3.tgz" -C "$DEPS"
  (cd "$DEPS/ntfs-3g_ntfsprogs-2022.10.3"; ./configure --disable-ntfs-3g --disable-mount-helper --disable-nfconv --without-fuse; make -j4)
fi
fetch nsis-3.12-setup.exe https://downloads.sourceforge.net/project/nsis/NSIS%203/3.12/nsis-3.12-setup.exe 3bc2b06253a7e4957111be152ac6a536e0c7478a706e19da814038db5d706495
fetch nsis-3.12-src.tar.bz2 https://downloads.sourceforge.net/project/nsis/NSIS%203/3.12/nsis-3.12-src.tar.bz2 f3ed7a8e4aa2cf4e8cf47d3b563a02559e0cb4934db2662b2f9661b824e2b186
python3 - "$DEPS" <<'PY'
import pathlib, shlex, sys
deps=pathlib.Path(sys.argv[1]); tools=deps/'fixture-tools'
for name, target in {'mke2fs':'e2fsprogs-1.47.2/local-build/misc/mke2fs','mksquashfs':'squashfs-tools-4.6.1/squashfs-tools/mksquashfs','mkntfs':'ntfs-3g_ntfsprogs-2022.10.3/ntfsprogs/mkntfs','ntfscp':'ntfs-3g_ntfsprogs-2022.10.3/ntfsprogs/ntfscp'}.items():
    path=tools/name
    if path.is_symlink(): path.unlink()
    path.write_text('#!/bin/sh\nexec '+shlex.quote(str(deps/target))+' "$@"\n'); path.chmod(0o755)
PY
echo 'Ready. Run ./scripts/test-formats.sh --available-only until all fixtures are covered.'
