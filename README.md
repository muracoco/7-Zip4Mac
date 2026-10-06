# 7-Zip Mac Port

[日本語](README.ja.md) · [Changelog](CHANGELOG.md) · [Detailed features](docs/features.md)

An unofficial macOS port of the Windows **7-Zip File Manager**, built with C++
and Qt 6 Widgets. It aims to preserve the Windows interface and workflow. The
bundled archive engine is built from official **7-Zip 26.03** source.

**Current local application build: 0.2.5.** Source:
[muracoco/7-Zip4Mac](https://github.com/muracoco/7-Zip4Mac).
Version changes and verification references are recorded in the
[changelog](CHANGELOG.md).

## Requirements

- Tested: macOS **26.6.2**, Apple M3, **arm64**, Command Line Tools / Apple Clang 21.0.0.
- Deployment target: **macOS 15.0**. macOS 15 and Intel hardware remain untested;
  the upstream x86_64 build configuration is selectable.
- **Qt 6.11.3**, dynamically linked QtBase Core / Gui / Widgets / Concurrent /
  PrintSupport. The patched Cocoa plugin is built from matching official source.
  QtTest is needed only for developer tests.
- CMake, Ninja, Python 3, make, curl and Git. Tested tool versions: CMake 3.31.6
  and Ninja 1.11.1.4. **Xcode.app and Xcode projects are unnecessary.**

## Build and run

Run from the source directory:

```bash
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/run.sh
```

Bootstrap downloads missing tools and official dependencies into
`~/.cache/7zip-mac-port`. It does not use sudo or change system Python.
Install Apple Command Line Tools with `xcode-select --install` if needed.
An existing Qt 6.11.3 installation can be selected with `QT_PREFIX`.

The app is generated at `build/7-Zip Mac.app`. For an SMB checkout, scripts use
`~/.cache/7zip-mac-port/build/7-Zip Mac.app` on the local disk.

Install/update the single Finder-registered copy in `/Applications`:

```bash
./scripts/install.sh
```

Close the target app before rebuilding, packaging or updating. The installer
retains a rollback copy and preserves existing default file associations.
See [installation and rollback](docs/finder-registration.md) and the
[detailed build guide](docs/building.md) for custom directories, direct CMake,
clean builds and packaging.

## Features

- Windows-style menus, toolbar, file list, shortcuts and Add / Extract /
  Progress / Options dialogs; official icons and 92 translations plus English.
- Filesystem and archive browsing, nested archives, sorting, multiple selection,
  Flat View, two panels, favorites, configurable columns and properties.
- Create **7z, ZIP, TAR, WIM, XZ, gzip and bzip2** archives; Test and extract.
  Supported read formats and tested variants are listed in the
  [format coverage](docs/archive-formats.md). **RAR compression is not provided.**
- Compression/encryption settings, split volumes, update/path modes and overwrite
  choices. 7z supports encrypted filenames; ZIP supports AES-256 and ZipCrypto.
- Asynchronous progress, Pause / Cancel, detailed errors and guarded archive
  updates. Passwords are not saved or logged.
- Copy / Move / Trash / Rename / new files and folders, links, Split / Combine,
  comments, external viewing/editing and confirmed archive write-back.
- CRC/hash calculation, Benchmark, temporary-file management and official Help
  with search and printing.
- Finder **Open With** shows a configurable 7-Zip operation menu, ending with
  **Open in 7-Zip File Manager**. See [Finder integration](docs/finder-integration.md).

[Detailed functionality](docs/features.md) describes individual operations and
links to their implementation and verification records.

## Tests

Developer tests are optional and require a full repository checkout or a
developer-profile source archive:

```bash
PORT_BUILD_TESTS=ON ./scripts/build.sh
./scripts/test.sh
```

Use `./scripts/test.sh --no-focus` while the desktop is locked. Native focus/menu
checks require an unlocked desktop. Fixtures stay in dedicated temporary
folders. [Recorded results](docs/test-results.md) distinguish automated and
actual desktop checks. The [detailed build/test guide](docs/building.md) includes
focused suites, format fixtures and the consolidated release gate.

## Known differences and remaining work

- Windows registry/Explorer integration, MAPI, PE SFX creation and host NTFS
  security/ADS are platform differences. NTFS/PE archive readers are included.
- macOS window controls, Trash, permissions, signing and function-key settings
  apply. Finder Extension / Quick Action / Services are not implemented.
- Some Port-specific explanations and errors remain English. Large-list final
  model insertion and metadata parsing can still use the GUI thread.
- Added path/update safeguards differ from Windows behavior. Extraction rejects
  unsafe traversal and symlink output parents; cancellation or I/O failure during
  final installation can leave already-installed files, reported in the result.
- Physical Finder drag, hardware Fn/RightCtrl/Option, real printer output,
  macOS 15/Intel hardware and exact Windows pixel comparison remain unverified.

See the [current inventory](docs/current-status.md),
[Windows parity](docs/windows-parity.md), [command audit](docs/final-audit.md)
and [settings coverage](docs/settings-coverage.md) for classifications and limits.

## Architecture and licenses

`GUI → ArchiveBackend → SevenZipProcessBackend → bundled 7-Zip executables`.
The modified `7zz-progress` helper integrates official callbacks/Agent operations.
QProcess launches without a shell and decodes UTF-8; passwords are supplied on
stdin instead of command-line arguments or stored preferences.

- Port: **LGPL-3.0-or-later** — [LICENSE](LICENSE).
- 7-Zip: LGPL-2.1-or-later, BSD-2-Clause / BSD-3-Clause and the unRAR decompression
  restriction — [License.txt](licenses/License.txt).
- Qt: dynamically linked LGPL-3.0 components — [Qt notices](licenses/Qt).
- Original 7-Zip icons/translations and third-party fixture notices are retained
  — [component notices](licenses/NOTICE.md). Microsoft fonts/system assets are
  not bundled.

The app includes license texts and corresponding 7-Zip, QtBase and Port source,
including Qt patches. Dynamic Qt replacement/rebuilding is permitted. Current
local builds use ad-hoc signing; Developer ID signing, notarization and App Store
publication are future work. See [distribution contents](docs/distribution.md).

Official upstream: [7-Zip](https://www.7-zip.org/download.html),
[26.03 source release](https://github.com/ip7z/7zip/releases/tag/26.03).

## Development

[Implementation workflow](docs/development-workflow.md) ·
[Incremental upstream updates](docs/upstream-updates.md) ·
[Publication/privacy checks](docs/git-privacy.md) ·
[License review](docs/license-audit-follow-up.md)
