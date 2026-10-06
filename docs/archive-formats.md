# Official archive formats and verification

The application's common registry contains **61 official 7-Zip 26.03 handlers,
138 distinct extensions, and 151 handler/extension registrations**. It is generated
from the source-built official `7zz i` and reconciled with registration macros in
`CPP/7zip/Archive` and `UI/Common/HashCalc.cpp`. File Manager opening, Open With
conditions and Info.plist share this registry. Default associations are untouched.

The Windows `Bundles/Format7zF/Arc.mak` and console `Bundles/Alone2/Arc_gcc.mak`
include the same registered reader families; Hxs/COFF/TE are implemented within
Chm/PE sources. AVB and LVM registrations are inactive upstream and are not claimed
as supported. Windows executable execution was not performed on this Mac; this
comparison uses official build/registration source and the compiled Mac engine.

Regenerate the inventory after an upstream upgrade:

```bash
source scripts/env.sh
python3 scripts/update-formats.py "$SEVENZIP_SOURCE" "$SEVENZIP_BINARY"
```

Run the full disposable fixture matrix:

```bash
./scripts/bootstrap-fixture-tools.sh
./scripts/test-formats.sh
# Or select another built tree:
./scripts/test-formats.sh --build /absolute/path/to/build
```

The optional bootstrap uses no sudo, devices or mounts. It downloads checksum-pinned
source/tools/data into the user cache, compiles local generators and changes no
system installation. Each run generates a new temporary fixture directory, then
deletes only that directory. Logs, XML and coverage reports remain in
`test-results/formats-<timestamp>/`. By default missing fixtures/generation failures
fail the command; `--available-only` is an explicitly partial development run.
`--manifest /path/to/manifest.json` reuses an existing owned fixture set.

The clean acceptance build passed **151/151 matrix cases** and the inventory/context
checks, **155 QtTest passes / 0 failures**, using real containers. Each matrix case
forces the intended official handler for independent identification, exercises the
backend's normal list/Test/extract route, compares extracted bytes with independent
SHA-256 references and invokes the File Manager's Open action. The right-click test
clicks the actual Qt submenu items for 7z/ZIP creation, Test and Extract to; this is
Qt-delivered input. Physical AppKit/Finder clicks remain pending while the desktop
is locked. Per-registration results: [format-coverage.md](format-coverage.md).

## Fixes discovered by the matrix

- BZip2, Zstandard, Base64 and Z can omit Path/Method/unpacked Size in technical
  listings. Such streams now appear in the list.
- The official console calculates names hidden from technical property enumeration,
  including FLV's `.pcm` suffix and compressed stream defaults. A second normal
  listing resolves nameless entries using the source-defined fixed columns. Counts,
  sizes and explicit paths must agree; ambiguous output fails safely.
- Mach-O can be excluded by automatic console discovery. On a normal code-2 failure,
  a single registered extension handler is retried. Signature detection remains the
  first attempt, so renamed ZIP data still opens correctly.
- Header-enabled listings retain format/archive metadata and each entry's properties.
  A terminal warning count is separated from item fields. POSIX Mode detects image
  directories and links. Properties displays available archive and selected-item
  information; complete Windows column/Properties parity remains pending.

## Limits of the evidence

The matrix tests registrations and valid container-reader paths. A ZIP-based
DOCX/EPUB alias is a ZIP-container test, not validation of its application semantics.
CHM/Hxs use valid stored test containers understood by the official readers, not
Microsoft-compiled viewer/Reader documents. `.swm` currently uses WIM data; a true
split-WIM set and broader volume/codec/version coverage remain additional work.
RAR4/RAR5 multi-volume sets are genuine libarchive fixtures, with names mapped to
`.r00/.r01/...`; original archived contents are unchanged. Generic `.001` and PKZIP
`.z01` use real multi-volume data.

All 23 checksum aliases are exercised with real digest manifests. The engine succeeds
for its supported digest methods. SHA224, SHA512-224/-256, SHA3-224/-384/-512,
BLAKE2s/BLAKE2b and POSIX cksum verification are unsupported by the compiled official
engine and correctly produce code 2. Hash `x` also returns E_NOTIMPL with the console
callback. These expected failures are labeled in coverage; they are not successful
hash verification/extraction or macOS platform exclusions. A native Hash callback
and Windows GUI behavior comparison remain part of the broader parity audit.

Nested stream navigation, every codec/encryption/version variant, Windows runtime
pixel/behavior comparison, physical menus and Finder drag-out are not proven by this
matrix. Full portable Windows feature parity is still incomplete.
