# Official desktop dialog resource batch

Add, compression Options, Extract and Progress were implemented together from unchanged official 7-Zip 26.03 resources before running the related validation group. This is a development checkpoint, not the final Windows-parity release.

## Source reuse and implementation

`scripts/import-dialog-geometry.py` pins the original `.rc`, resource headers, `CompressDialog.cpp` and `ProgressDialog2.cpp` hashes. It expands the desktop resources with Clang and minimal temporary button-ID definitions; no Windows SDK or proprietary fonts are copied. The generated files retain upstream attribution. Original sources, importer and adapters are distributed with the app.

- Add, compression Options and Extract use their original dialog-unit coordinates, fixed client dimensions, control order and OK / Cancel / Help button placement. Qt adapts dialog units to the installed macOS font. Only the explicit construction layouts are removed; Qt's internal combo/button layouts are retained.
- Original `CProgressDialog::OnSize` is reused through a Qt window adapter. Columns, progress bar, message area and buttons resize using the original calculations. Processed and total file counts occupy their original separate rows.
- Original elapsed/remaining time, size and speed formatters are reused, including unit thresholds and values above 99 hours. Large-value arithmetic is bounded before converting to UInt64.
- Original `ShowOptionsString` and bool-pair formatters summarize compression options. Format choices follow the resource's sorted order. Backend identifiers remain literal during translation.
- The Extract name checkbox shows/hides its subfolder field as upstream does. The selected/all-files row is an additional Port control in unused resource space.
- Without a saved language, the supported Qt system locale selects the supplied language, including regional/script variants. Saved choices override this selection; explicit upstream `-` means English. Windows LCID lookup is replaced by Qt locale mapping.

## Executed validation

Environment: Apple Silicon macOS 26.6.2, Apple Clang, Qt 6.11.3. CMake/Ninja built the app and test executables. The [raw log](distribution.md) retains both initial failures and targeted reruns.

Latest result for each selected case, counting setup/cleanup once per suite: **96 passed, 1 failed due to unavailable Cocoa focus, 0 skipped**. Dialog resources: 8 passed; language/settings: 10; compression/help: 46; native progress: 15; version/menu/time: 11; selected integration: 6 passed and 1 focus failure. This is not a claim that every case was rerun after every change or that physical desktop input passed.

The grouped run exposed a real stale `QFormLayout` reference after switching to resource geometry. Format-dependent visibility now uses the surviving label widgets, fixing the crash. A new modal test's wrong object name and a stale fixed-speed-unit assertion were corrected. The language test exposed Portuguese/Brazil precedence; regional matching now precedes the base language. Only failed cases and affected cases were rerun.

`Integration::advancedCompressionSettings` completed its backend compression/property checks, then failed `activateTestWindow` before its physical mouse-click checks. It remains unconfirmed; the new modal widget test separately verifies compression Options acceptance and the original summary. The independent integration cases passed Japanese settings, absolute extraction, Open With compression/Test/extraction and archive folder creation. Existing compression/help and progress cases exercise real codecs/encryption, failures, Pause/Cancel and recovery.

To repeat the grouped validation on an available desktop:

```bash
source scripts/env.sh
BUILD="$DEPS/build-comments-20261004" # or choose another configured build
export PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins"
ctest --test-dir "$BUILD" --output-on-failure -R '^(dialog_resources|language_settings|compression_help|native_progress|version_menu_time)$'
"$BUILD/port_tests" "$SEVENZIP_BINARY" officialLanguagesAndRootOptions advancedCompressionSettings absoluteExtractionAndDialogSettings openWithArchiveCommands guiArchiveFolderCreation
```

The ordinary `scripts/test.sh --no-focus` selection also includes `dialog_resources`. Test executables explicitly select English as their fixture locale; production first-run detection is unchanged.

Packaging completed for `/DEPS/build-comments-20261004/7-Zip Mac.app`. The bundle checker found 15 Mach-O files with only system/@rpath dependencies; deep/strict ad-hoc signature verification passed. With development Qt/DYLD paths removed and owned temporary preferences, the bundled executable remained alive for three seconds with empty stderr. Only that owned process was stopped. The [packaging/startup log](distribution.md) records this check; it does not establish physical Finder operation or a final clean build.

## Remaining scope

| Item | Classification | Remaining work |
|---|---|---|
| Imported numeric geometry, Progress resize and listed formatters | Implemented | Verified on Qt widgets against source coordinates and formatting boundaries |
| First-run language and saved-language precedence | Implemented | Verified with supplied locale variants and owned preferences |
| Font and macOS title bar | macOS difference | Installed Arial/system fallback and standard macOS title bar; no Microsoft assets bundled |
| Long translated labels and physical visual comparison | Partially implemented | Font/translated-text sizing was added in the subsequent dialog-text-help-port.md batch; physical comparison remains |
| Progress filename abbreviation, detailed status/title text | Implemented | Subsequently connected to the original UpdateStatInfo/title bodies; see progress-presentation-port.md for the later batch and remaining completion policy |
| Port-specific text translation | Partially implemented | Some descriptions/errors have no upstream resource binding |
| Physical click/focus test and final release gate | Partially implemented | Desktop availability, final acceptance, empty-directory clean build and remaining portable parity work are required |

Do not classify the remaining portable gaps as macOS differences. Full-format tests and the final clean build were not repeated for this UI batch.
