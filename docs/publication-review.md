# Publication preparation — 2026-10-06

This records the review of the local release candidate, not a public release or
a declaration of complete Windows parity. Physical desktop acceptance remains
pending. No GitHub repository, remote, push, tag or Release has been created.

## Components and distribution conditions

The reviewed engine is official 7-Zip 26.03; the GUI uses dynamically linked
QtBase 6.11.3. The Port's own code is LGPL-3.0-or-later. Original imported
notices and the applicable LGPL, BSD and unRAR texts are retained. RAR code is
used for decompression; there is no RAR writer. The official File Manager ICO
and toolbar assets retain upstream licensing. No Microsoft system fonts or
system artwork are copied. [7-Zip distribution terms](https://www.7-zip.org/license.txt).

Qt licensing requires notices, corresponding source and the ability to replace
the library and run the modified combination. This candidate includes QtBase
source, Cocoa patches and build scripts, full GPL/LGPL texts and third-party
notices. About displays Qt's copyright and license; its tooltip identifies the
source/license location. Framework replacement and local re-signing are
permitted. [Qt's LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations).

| Reviewed component | Concrete evidence |
|---|---|
| 7-Zip license documents | License.txt, copying.txt and unRarLicense.txt match the pinned official source. |
| Original icons | FM.ico and 7zipLogo.ico match official FileManager files. ICNS is a format conversion; scripts/make-icon.sh reproduces it. |
| Bundled source | Official 7-Zip and QtBase source archives match their cache originals. Port source includes importers, native adapters and both Cocoa patches. |
| Qt deployment | The candidate contains Core, Gui, Widgets, Concurrent, DBus and PrintSupport, all from QtBase. QtSvg is not bundled. |
| Notices | Available Qt license texts match the original QtBase LICENSES files. All project notices are present unchanged in the app. |
| Fixtures | libarchive extraction fixtures retain individual license/checksum records; Wine SZDD data retains attribution and the declared-header change. Optional image/installer generators are not bundled executables. |

The byte audit completed 92 comparisons with zero mismatches. This is evidence
for those artifacts, not a claim that a matching hash alone proves every legal
condition. The original asset/source headers, import provenance, application
notice and rebuilding/replacement instructions form the rest of the review.
The reviewed Mach-O files contain no private developer-path strings matching
the original home/workspace prefixes.

### Rebuilding or replacing Qt

The supplied source can be built with Apple Command Line Tools and Ninja using
the commands in README. `scripts/build-qt-cocoa.sh` applies the supplied changes
to corresponding QtBase source in a separate cache and builds its platform
plugin. `scripts/package.sh` deploys dynamic frameworks and re-signs locally.
To use an interface-compatible modified Qt 6.11.3 build, set `QT_PREFIX` to that
installation, rebuild and package. Private Qt APIs require this matching
version. A user may also replace the bundle's compatible frameworks/plugin and
run `codesign --force --deep --sign - "7-Zip Mac.app"`; no Developer ID key is
required. Source and license texts reside in `Contents/Resources`.

## Privacy and source snapshot

At review start, commit `91f7f69` contained 785 tracked blobs. The scan found no
credential-pattern candidates; 106 documentation/README files contained private
home/workspace prefixes. Across 82 reachable commits and 2,129 unique blobs,
there were no credential-pattern candidates and 235 blobs contained those
prefixes. Original local history and diagnostic records are preserved.

The public repository will begin with the reviewed release source snapshot.
Internal diagnostic Git history is not included. The snapshot normalizes only
documentation copies; license texts, resources, imported source bodies, patches
and application code retain their original bytes. It accepts only the same
reviewed source paths as application source packaging, rejects symlinks,
conflicts and credential/path findings, and exclusively creates a fresh output
directory. SHA-256 values and normalized modes are checked after writing.

```bash
python3 scripts/prepare-publication.py /absolute/new-source-directory
```

Use `--index` only to inspect a staged release candidate. The generated
`SOURCE-MANIFEST.json` supports builds without Git; `PUBLICATION-REVIEW.json`
records redacted documentation paths and the input revision. Pattern scanning
does not replace inspection for arbitrary secrets or personal data. Nothing in
this command authenticates to a service or publishes files.
Generated root manifests are ignored by Git; they accompany the Git-free source
copy, while a normal checkout rebuilds its source manifest from tracked files.

Owned export acceptance checks passed for documentation normalization with
CRLF/UTF-8 preservation, unchanged source/license bytes, existing-output
protection and refusal of credential patterns, unreviewed local configuration,
symlinks and unmerged input before creating an output directory. The first
local source snapshot verified 787 files and normalized 118 documentation files.

The first build from that snapshot stopped before application configuration:
the shared Cocoa CMake cache referred to the original checkout's project path.
The build now copies its small project definition to a fixed cache location;
its cache identity includes patches, project configuration, Qt installation
and architecture. Original caches are retained. This removes the checkout-path
dependency without changing the Qt ownership fixes or engine implementation.

## Executed candidate verification

The repaired Git-free source copy completed an initially empty Release/Ninja
application build: 252 steps. Its bundled source manifest verified all 787
exported files against the actual archive bytes and modes. No original private
home/workspace prefixes remain in that source or the 15 Mach-O files. The app's
Info.plist selects the deployed official File Manager ICNS conversion.

The affected group passed once after that build: dialog resources, compression
Help and Cocoa accessibility (3/3 suites, 7.22 seconds). Qt totals including
setup/cleanup were 10 and 47 in the first two suites; the native ownership suite
passed its column/list cycles. The About notice fits all 93 language choices.
Bundled startup remained alive with a private INI profile, no stdout/stderr,
unchanged user preferences and rejection of an invalid relative profile.
Signature and system/@rpath dependency checks passed. Reconfiguration from the
original checkout also passed using the same stable Cocoa project cache.

The earlier all-format and application evidence is reused because the engine
and archive workflows did not change in this batch. No new full-format or
full-application run is claimed. First export/build failure logs and affected
passes are preserved in [publication-tests.log](distribution.md), with
hashes in [publication-evidence.json](distribution.md).

Repository description prepared for the eventual authorized publication:

> Unofficial Qt Widgets port of Windows 7-Zip File Manager for macOS. / Windows版7-Zip File ManagerをQt Widgetsで移植した非公式macOSアプリ。

The remaining release gates are physical native menu/Finder/modifier/printing
and appearance acceptance, followed by the final reviewed release commit and
the conditionally authorized public publication. Available automated feature
evidence is in [release-consolidation.md](release-consolidation.md).
