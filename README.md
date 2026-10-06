# 7-Zip Mac Port

[日本語](README.ja.md)

An unofficial macOS port of the Windows **7-Zip File Manager** interface, written in C++ with Qt 6 Widgets. The official 7-Zip **26.03** source-built `7zz` is bundled as the archive engine. It uses Apple Clang, CMake and Ninja; Xcode.app and Xcode projects are unnecessary.

**Status:** local 0.2.0 candidate for the tested Mac. The portable command/settings groups are implemented; Windows host mechanisms and remaining visual/input verification limits are documented in the [command audit](docs/final-audit.md) and [parity inventory](docs/windows-parity.md). Engine capability, implemented UI and tested format coverage are separate claims. Source repository: [muracoco/7-Zip4Mac](https://github.com/muracoco/7-Zip4Mac).

The [2026-10-06 consolidated checkpoint](docs/release-consolidation.md) records the empty build, 151 format registrations, initial failures and affected repairs. The [0.2.0 candidate](docs/release-0.2.0.md) adds aligned address typography, actual versioned Finder routing, and a repaired ZIP omitted-timestamp path, with another empty build and affected verification. [Local publication review](docs/publication-review.md) covers notices, corresponding source and privacy.

The [desktop follow-up](docs/desktop-follow-up.md) fixes an empty Options tab and records passing affected/AppKit menu checks and a GUI-created 48 MiB 7z content comparison. [Actual Finder operations](docs/finder-desktop-acceptance.md) now include quick 7z/ZIP creation, GUI Test/extraction and File Manager opening. Physical Finder drag, hardware modifiers, printer hardware and a Windows pixel comparison remain unverified; automated/native event evidence is identified separately.

## Requirements

- Tested host: macOS 26.6.2, Apple M3, arm64, Apple Command Line Tools / Clang 21.0.0.
- Deployment target: macOS 15.0. macOS 15 and Intel hardware have not been tested. The upstream x86_64 build configuration is selectable.
- Qt **6.11.3 exactly**, dynamically linked QtBase Widgets / Gui / Core / Concurrent / PrintSupport; QtTest for tests. The Cocoa plugin is patched and built from corresponding official source.
- CMake 3.31.6, Ninja 1.11.1.4, Python 3, make, curl and Git.

## Build, run and test

```bash
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/test.sh
./scripts/run.sh
```

Bootstrap downloads missing tools and official Qt/7-Zip dependencies into `~/.cache/7zip-mac-port`. It does not use sudo, install Homebrew itself or modify system Python. Install Command Line Tools with `xcode-select --install` if necessary and handle the macOS installer prompt. An existing matching Qt installation can be selected with `QT_PREFIX`.

The Cocoa plugin uses Qt private APIs, so mixing Qt versions is rejected. Its two ownership fixes and source provenance are documented in [qt-cocoa-fix.md](docs/qt-cocoa-fix.md). Close the target application before building or packaging; scripts refuse to overwrite a running bundle.

On a local checkout, output is `build/7-Zip Mac.app`. On an SMB checkout, scripts select a local build directory to avoid filesystem signing/locking problems:

```text
~/.cache/7zip-mac-port/build/7-Zip Mac.app
```

```bash
./scripts/run.sh /absolute/path/to/example.7z
./scripts/run.sh --open-with /absolute/path/to/file.txt

# A separate, initially empty build directory:
./scripts/build.sh "$HOME/.cache/7zip-mac-port/build-clean"
./scripts/test.sh "$HOME/.cache/7zip-mac-port/build-clean"
```

Direct CMake usage is also supported:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.11.3/macos" \
  -DSEVENZIP_BINARY="/path/to/source-built/7zz" \
  -DPORT_COCOA_PLUGIN_DIR="/path/to/patched-cocoa/build/plugins/platforms"
cmake --build build
./scripts/package.sh build
./scripts/test.sh build
```

Run native GUI tests on an unlocked, logged-in Mac desktop. While locked, `./scripts/test.sh --no-focus` runs all registered independent suites, including external-editor write-back, F3 folder statistics, filesystem/Flat loading, file/ZIP comments, Open Inside modes, Open As, links, Properties and Cocoa ownership checks, leaving native focus/menu checks pending. Fixtures are confined to dedicated temporary directories. [test-results.md](docs/test-results.md) records automated and actual GUI verification separately. The package includes Qt frameworks/plugins and the source-built 7zz; bundle checks reject development-prefix runtime dependencies and validate its ad-hoc signature.

The combined release gate runs the available suites, native Agent fixtures, all format registrations and isolated bundle startup once:

```bash
./scripts/verify-release.sh --no-focus /absolute/build/path
# Omit --no-focus on an unlocked desktop for native focus/AppKit checks.
```

Reports are stored in `test-results/release-*`; unavailable physical checks are reported separately.

Complete each source-defined implementation group before running its affected
suite group. Use the registered CTest names to limit a follow-up; the default
command still runs the complete suite. A filter matching no suites is an error.

```bash
PORT_TEST_REGEX='^(copy_workflow|address_workflow|panel_key)$' ./scripts/test.sh /absolute/build/path
```

Passing unrelated suites and the full format matrix are reused until a shared
implementation change requires checking them again. First failures remain in
the consolidated report, with application defects and test-driver errors
classified separately.

Additional format tests (cache-local optional generators, no global installation):

```bash
./scripts/bootstrap-fixture-tools.sh
./scripts/test-formats.sh
```

See [archive-formats.md](docs/archive-formats.md) for fixture provenance, strict coverage checks and test limits.

## Available functionality

- File / Edit / View / Favorites / Tools / Help menus and official Add / Extract / Test / Copy / Move / Delete / Info toolbar assets.
- Shared official registry: 61 handlers / 138 extensions, declared as alternate document types. [All 151 registration tests](docs/format-coverage.md) distinguish successful operations from upstream checksum limitations.
- Filesystem and archive-folder browsing, nested archives in the same panel, virtual-path Favorites, parent navigation, sorting, multiple selection, four view modes, Flat View, two panels, saved columns, timestamps, automatic refresh and history. Nested changes support confirmed, one-level-at-a-time parent write-back on Up, address navigation and close. Failed updates retain a recoverable edited copy.
- Original natural/typed/raw sorting, first-sort directions and stable row ties, complete filesystem metadata columns, and property-keyed column preferences across Flat View and both panels. [Source reuse, grouped checks and limits](docs/panel-sort-port.md).
- Create 7z / ZIP / TAR / WIM / XZ / gzip / bzip2, plus SHA-256 / SHA-1 checksum manifests; Test and extract. Stream-format creation accepts one regular file. **RAR compression is not provided.**
- Compression method, dictionary, word/order, solid size, threads, memory, update/path modes, volumes, parameters, timestamps, supported link-storage flags and encryption. Passwords are neither logged nor saved.
- Automatic compression choices and memory estimates reuse official Windows GUI arithmetic; manual/automatic choices persist separately. Official English Help provides all 70 pages, Contents/Index, navigation and contextual dialog topics. [Scope and tests](docs/compression-help-port.md).
- Compression controls now reuse official format/method tables and numeric builders, with format drafts, level/dictionary resets, method-aware restore, hardware-bounded threads and last-format/history behavior. [Source reuse and tests](docs/compression-controls-port.md).
- Progress receives real official-engine callbacks through bundled `7zz-progress`: file counts, processed/packed bytes, compression ratio, speed and remaining time. Help adds asynchronous full-text Search, topic/subtopic printing and the upstream-style About dialog. [Implementation, checks and limits](docs/native-progress-help.md). The original Progress presentation, control and completion paths are reused through Qt adapters. Successful jobs close automatically; Test shows original statistics and CRC/SHA opens the original property list. See the [completion batch and executed validation](docs/progress-completion-port.md).
- 7z encryption including encrypted filenames; ZIP AES-256 / ZipCrypto. Upstream ZIP passwords are restricted to ASCII.
- Extraction destination/history, selection/all, three path modes, five overwrite modes, a shared six-choice per-file replacement dialog and root-folder elimination. Absolute extraction requires explicit confirmation.
- Extraction paths and metadata now come from the original archive callback. Safe symbolic/hard links, folder modes/times and available creation times are retained during guarded installation. [Source reuse, tests and remaining gaps](docs/extraction-metadata-port.md).
- Nonblocking archive jobs, progress/logs, Pause / Continue, cancellation and detailed exit-code failures. New archive/extraction output is staged. Generic Split / Combine report actual bytes and clean staged files on cancellation. [Pause and nested-archive scope](docs/nested-archives-progress.md).
- Filesystem copy/move, Trash deletion, rename, new folder/file and properties; existing transfer outputs prompt individually, with Yes/No, All and Auto Rename choices. Skipped move sources are retained. [Overwrite behavior and limits](docs/overwrite-dialog-spec.md). Archive deletion/rename require confirmation. Empty folders can be created in writable 7z/ZIP/TAR/WIM, including internal subfolders and multiple WIM images. The official Agent CreateFolder body retains existing packed streams; staged folder properties are checked before replacement. Working-folder preferences and cross-volume installation are supported; archive permissions and extended attributes are retained. [Official folder updates and tests](docs/native-folder-update.md).
- Split / Combine with `.001`-style numbering, wider numbering when needed, multiple sizes and repetition of the last size; missing parts are errors instead of silently truncated output.
- Hard links and relative/absolute file or directory symbolic links, guarded existing-symlink editing, raw-target display and both folder browse controls. Ordinary existing files/directories are protected. [Link behavior and limits](docs/link-dialog-spec.md).
- Select / Deselect by Type, dynamic Favorites and archive-folder bookmarks. Alt+digit / Alt+Shift+digit and native RightCtrl handling preserve the upstream keys; physical-key validation remains pending.
- Official-engine Benchmark GUI with the Windows callback path, 3/2 dictionary choices, live Current/Resulting values, full-precision accumulation, Size/GIPS/CPU/system display and Restart / Stop. A fresh dialog defaults to 10 passes; existing port choices are retained. [Implementation and checks](docs/benchmark-port.md).
- Tools → Delete Temporary Files browses the original temporary-name patterns and this port's Qt names, using the imported bounded counter/size formatter. Navigation, sorting, context actions, settings and confirmed Trash deletion are available; live/changed data is protected. [Scope and checks](docs/temporary-files-port.md).
- Filesystem / Flat View enumeration and metadata run on a worker, with cancellable GUI row batches and protection against stale navigation. Text is formatted on demand, large-list comparisons and native icons run on workers, and original names remain distinct from display markers. Final row insertion/reordering and metadata parsing still include GUI work. See [the callback display batch](docs/panel-listing-port.md).
- Open Inside * and # match the official one-level detection and parser-region modes, retaining the chosen mode through Refresh, extraction, Test, CRC and nested parent navigation. See [mode semantics and limits](docs/archive-open-modes.md).
- Focused-item Comment / Ctrl+Z: compatible UTF-8 `descript.ion` comments and ZIP entry comments, including ZIP64 and supported encrypted ZIPs. ZIP updates now import the official Agent CommentItem body and real-index selection, including same-name siblings and Flat descendants. Packed data is retained without an unrelated password requirement. Saves are staged and guarded against external changes. See [source reuse and tests](docs/native-agent-comments.md) and [limits](docs/file-comments-spec.md).
- F3 recursively fills filesystem folder Size / Folders / Files, with asynchronous Pause / Cancel and numeric sorting. See [folder statistics](docs/folder-statistics.md).
- Upstream-style two-column Properties: one/many/no selection, cached folder/Flat totals and CRC, nested archive layers, Ctrl+A/copy and full-value display. Typed/raw properties, format-specific columns and folder totals now reuse official handlers, formatters and Agent proxies. Parent metadata is refreshed after nested write-back. [Native bridge and remaining scope](docs/native-metadata.md).
- Native archive rows and Properties now use original Agent directory/item identities and imported property methods. Implicit folders, Flat ordering and archive alternate-stream navigation are verified with genuine duplicate ZIP entries and an NTFS image. [Source reuse, test command and limits](docs/native-agent-properties.md).
- Selected Extract/Test/archive hashes and focused temporary opening now reuse the official Agent selection methods, including independent same-name entries and normal/Flat folder policies. A 64 MiB selected operation verifies Pause/Cancel and reuse. [Selection port and remaining update scope](docs/native-agent-selection.md).
- Delete/Rename in 7z/ZIP/TAR/WIM import the official Agent update bodies and select native item indices, including same-name ZIP siblings, Flat folders and multi-image WIM. Atomic installation, encrypted solid repacking and Cancel/original retention are tested. [Source reuse and limits](docs/native-agent-item-updates.md).
- Official Agent UpdateOneFile now handles editor/nested file replacement by native item identity, including same-name siblings and single streams. Pending sessions, ancestor/Flat folder addresses and native row selection are rebound after supported mutations ([refresh checks](docs/archive-refresh-port.md)); decoded replacement size/SHA-256 are verified without an unrelated whole-archive Test. [Source reuse and checks](docs/native-agent-replacement.md).
- Single-stream Rename/Delete also use original Agent operations and handler behavior. Renamed pending editors and nested parents retain their native targets, selection and focus. [Behavior table and GUI/console comparison](docs/single-stream-updates.md).
- Open/Outside import the original multiple-item policy, including the 20-item limit, folder stopping rule and single-item internal attempt. Separate native-index temporary extractions allow same-name ZIP siblings to open and write back independently. [Source and checks](docs/panel-open-port.md).
- Default Open detects archive contents with an unknown or absent extension. The official external-opening extension table and misleading-filename warnings are imported. Password cancellation and permission errors do not fall back externally. [Source and checks](docs/open-profile-port.md).
- External editing imports the official same-executable process-discovery loop, covering independent launcher hand-off and confirmed ZIP write-back; existing seven-format and encrypted/nested checks pass. [Source and observation limits](docs/external-process-port.md).
- Configurable viewer/editor/diff, confirmed archive-file editor write-back, persistent six-page Options and 92 official translations plus English. The official language parser and translator/missing-entry information are ported; bundled Lang files are editable. Menus and primary dialog controls use official resource IDs. Apply/Cancel behavior is tested; some Port-specific text remains English. [Source scope and tests](docs/language-settings-port.md).
- All 11 upstream CRC/hash menu choices for filesystem files and selected archive contents, including folders and the all-methods choice. Archive hashing streams through the official engine without writing extracted files. Encrypted 7z/ZIP inputs are tested.
- Alternative selection imports the original File Manager mark, Insert, Ctrl/Shift-click and Shift-arrow bodies. Pink operation marks and native focus remain independent in both views and panels; refresh and Options Apply preserve them. [Source and checks](docs/panel-selection-port.md).
- Ctrl+C copies marked names as CRLF-separated text, matching the upstream File Manager; alternative mode does not fall back to an unmarked focused row. Upstream Ctrl+X/Ctrl+V handlers are empty; they do not transfer files.
- File Manager right-click menus now import the official ordinary File-menu filtering, with CRC/Diff and one creation group. Configured 7-Zip shell commands remain shared with Open With. Internal filesystem drag Copy/Move, right-drag menus, target subfolders and asynchronous encrypted archive drag-out are tested; accepted temporary files survive Manager exit. [Source, checks and native interaction limits](docs/panel-menu-drag-port.md).
- Finder **Open With** displays a configurable 7-Zip action menu. Its bottom File Manager entry is always available. [Finder integration](docs/finder-integration.md) explains scope and known association behavior.
- Original Windows File Manager application icon: unmodified official `FM.ico`, PNG/ICNS format conversion reproduced by `scripts/make-icon.sh`.

- Filesystem Copy / Move can target the other open archive panel and its internal folder, using official handler defaults. Archive-folder drops accept different input parents; verified Move uses Trash. [Behavior and limitations](docs/archive-transfer.md).
- Single-layer archive updates preserve leading prefixes using the original Agent/console stream boundary and official handlers. Folder creation, replacement, ZIP comments and nested write-back verify prefix bytes before replacement. [Source reuse and tests](docs/archive-prefix-updates.md).
- Same-volume filesystem Move now tries rename first, including folders, links and all overwrite choices. Cross-device/SMB transfers have a guarded copy fallback. Delete-after verification supports Pause/Cancel and callback/local progress. [Scope, OS limits and executed tests](docs/filesystem-transfer.md).

## Known differences and remaining work

The audited portable command/settings groups are implemented; documented behavior differences and verification limits remain. [windows-parity.md](docs/windows-parity.md), [settings-coverage.md](docs/settings-coverage.md) and the [command audit](docs/final-audit.md) identify implemented, partial, platform-specific and unimplemented behavior.

The [current implementation/release inventory](docs/current-status.md) separates connected source groups, implementation differences, defects and physical verification. Native-index operations, ancestor/Flat restoration, extraction overwrite/publication/lifecycle, complete list-key dispatch and Copy projection have been implemented in the later source groups. Historical component remaining-work lists must not be used as the current backlog. Port-only text localization and documented safety restrictions still differ; large-list model insertion/parsing remains a performance limit. The tested 0.2.0 package and public source publication are complete; physical/pixel equivalence is not claimed. See [publication closeout](docs/publication-complete.md).

Like official Windows 26.03, archive-internal context menus provide Open / Open Inside / Open Inside * / Open Inside # / Open Outside. The format-specific Shell Open As submenu belongs to filesystem context menus; upstream `PanelMenu.cpp::CreateFileMenu` explicitly omits Shell menus inside archives. [Source comparison](docs/archive-open-modes.md).

Open With and filesystem context menus offer the official Open archive > modes: `*`, `#`, `#:e`, `7z`, `zip`, `cab`, and `rar`, with an independent Options checkbox. Previously saved explicit command lists retain their choices; enable the new item in Options when upgrading. [Open mode semantics and limits](docs/archive-open-modes.md).

The [format matrix](docs/archive-formats.md) covers all 151 registered handler/extension pairs and the Qt right-click path. It does not prove every codec/version variant or complete application-specific package semantics. Native AppKit menu/right-button checks and actual Finder Open With compression/Test/extraction are verified separately. Physical Finder drag, hardware function/modifier keys and Windows pixel equality remain unverified.

Windows registry/Explorer registration, applying NTFS permissions/ADS/Zone.Identifier to host files, PE self-extracting executables, Windows sharing locks, large-page privilege and MAPI are platform differences. **Reading NTFS or PE archive/image formats is not excluded.** Native macOS window controls, Trash, permissions and Gatekeeper apply. The main menu is within the window; the title bar is macOS standard. Default file associations are not forcibly changed.

Extraction rejects unsafe traversal and symbolic-link output parents. Regular duplicate/flat/case-only names follow callback completion order and the selected overwrite policy. Safe links accepted by the original callback are restored; destination leaf links can be replaced without following them. Absolute archive paths require the explicit absolute-path workflow. Selected hard links to existing output files pass TAR/RAR5 comparisons. Regular collision and tested TAR link-chain comparisons pass for 7z/ZIP/TAR. Normal engine failures retain callback-recorded outputs, and installation failure finalizes folder metadata (see [scope and tests](docs/partial-extraction-port.md)). Pre-stream overwrite/Skip, principal conflicts and process/shutdown output retention are connected and covered by the later integration groups. Unknown archive variants remain verification limits. If cancellation/I/O failure occurs during final installation, already-installed files can remain and the result reports that condition.

## Architecture and licenses

`GUI → ArchiveBackend → SevenZipProcessBackend → bundled 7zz`. QProcess uses no shell; stdout/stderr are decoded incrementally as UTF-8. Passwords are supplied on stdin, not argv, environment variables or preferences. Bundled `7zz-progress` uses the unchanged engine with an instrumented console frontend, a nonblocking Unix datagram callback channel and imported official Agent operation bodies. ZIP Comment uses this native update path; `7zip-comment` remains a structured-property reader for listing fallback. Bootstrap/build generate the helpers from pinned official sources beside 7zz before CMake configuration. Other native integration can remain behind the same request/result interface.

- Port code: **LGPL-3.0-or-later**. [LICENSE](LICENSE), [GPL text](licenses/Qt/GPL-3.0-only.txt).
- Official 7-Zip: LGPL-2.1-or-later, BSD-2-Clause / BSD-3-Clause and the unRAR restriction for RAR decompression. See [License.txt](licenses/License.txt), copying and unRAR texts. Do not use RAR-derived code to recreate RAR compression.
- Official toolbar/icons and unchanged language files: upstream 7-Zip LGPL assets, with notices retained. No Microsoft fonts or system artwork are bundled.
- Qt: LGPL-3.0 dynamic frameworks, with corresponding source, patches, copyright and third-party notices. See [licenses/Qt](licenses/Qt) and [NOTICE.md](licenses/NOTICE.md). Replacement/rebuilding/re-signing of the dynamic Qt components is not restricted.
- CAB / LHA / RAR / RAR5 / RPM fixtures: libarchive v3.8.2, with [individual notices](licenses/libarchive-fixtures.txt) and [COPYING](licenses/libarchive-COPYING.txt). SZDD test data retains [Wine attribution](licenses/wine-szdd-fixture.txt). Optional fixture generators/data are not bundled executables.

`Contents/Resources` includes license texts, unchanged official 7-Zip source, corresponding official QtBase source, and buildable Port source including the Cocoa patches. Source URLs/checksums are pinned in bootstrap. Developer ID signing, notarization and Mac App Store distribution are outside the current scope; this build uses ad-hoc signing.

Official upstream: [7-Zip download](https://www.7-zip.org/download.html), [26.03 release](https://github.com/ip7z/7zip/releases/tag/26.03). This is an unofficial port, not an official 7-Zip product. [Publication preparation](docs/publication-review.md) records component notices, source/privacy review and the local-only source exporter. Public source publication is complete; [publication closeout](docs/publication-complete.md) records the verified snapshot and retained limits.

Implementation proceeds in related original-source feature groups, followed by grouped checks and failed/affected reruns; see [development-workflow.md](docs/development-workflow.md).
