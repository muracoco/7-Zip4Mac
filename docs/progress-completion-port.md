# Original completion, Test and checksum results

This feature group imports the official Windows 7-Zip 26.03 completion policy and result presentation. Backend data, Qt controls and the related workflows were connected before the grouped functional checks. This is a working milestone, not the final parity release.

## Original source reused

`scripts/import-progress-completion.py` verifies pinned upstream hashes and retains the original `CProgressFinalMessage` structs, `ProgressDialog2.cpp::OnExternalCloseMessage`, the Test summary from `ExtractGUI.cpp`, both `HashGUI.cpp::AddHashBundleRes` formatters, `ShowHashResults` and `ListViewDialog.cpp::CopyToClipboard`. `scripts/import-dialog-geometry.py` now also imports `ListViewDialog` and `EditDialog` resources and their original resize bodies. Original attribution, corresponding sources and Port adapters are supplied in the app's source archives.

- Successful Add, Extract and ordinary file jobs close Progress automatically. They do not retain an independent “Everything is Ok” result screen.
- Filesystem Test displays original archive/file/folder counts and packed/unpacked sizes, followed by the localized no-errors message. Archive-inside Test uses the original empty-hasher summary, including a single selected filename. Completion during a Cancel question continues to suppress a late cancellation.
- Individual file failures stay in the original numbered error list with Close. A fatal operation failure uses the original final-error message path. Operation, target, exit code and redacted console diagnostics remain in the backend result and operation log. The parent title prefix is restored when Progress closes.
- CRC/SHA results use the original two-column, resizable list. The original property order, data/name/alternate-stream digest groups, no initial selection, selected-row Copy, temporary row deletion and read-only detail view are retained. Ctrl+C/Ctrl+Insert and Ctrl+A keep the Windows accelerators. The list honors the single-click setting; the preview truncates at the original 1024 characters and the detail/Copy retain the complete value.

The official engine produces all counts and digests. The native helper serializes `CDecompressStat` and `CHashBundle` plus original `CHasherState::WriteToString` output to an owned private completion file; Qt does not infer totals from console text or reimplement digest algorithms. The bounded versioned parser detects missing/invalid results. Unlike optional progress datagrams, completion data is read after the child exits. Passwords are not placed in this transport; backend name redaction remains applied. The original `PanelCopy.cpp` single-item name seed is taken from the validated panel-relative selection, so an inside-folder Test and a selected-folder checksum retain their original Name property.

Qt adapts the original modal controls and message boxes; macOS keeps its native title bar. Windows taskbar integration is OS-specific. Physical desktop activation, Finder interactions and Windows screen comparison remain unverified here. Other known portable implementation gaps are still required work in [windows-parity.md](windows-parity.md); this group does not claim them complete.

## Executed validation

On macOS 26.6.2 / Apple Silicon, CMake/Ninja and Apple Clang built the app and updated native helper with Qt 6.11.3. The [execution record](progress-completion-test.log) contains initial grouped failures and only failed/affected reruns. Latest result per selected case, counting setup/cleanup once per suite: **79 passed, 0 failed, 0 skipped**.

| Suite | Latest selected cases |
|---|---:|
| Completion/result presentation | 15 |
| Existing Progress presentation/control | 15 |
| Dialog resources | 8 |
| Language/settings | 10 |
| Native progress/round trips/errors | 15 |
| Selected integration | 16 |

Actual files include Japanese/spaced names, empty folders and 32 MiB data. Checks cover native counts, SHA-256 against Qt's independent digest, 7z/ZIP compression/Test/extraction, inside selection Test, all supported checksum methods in selected encrypted/plain archives, GUI result capture/auto-close, error list Close and backend reuse, Background/Pause/Cancel and Open With workflows. Pure copy-formatting tests leave the user's clipboard intact. Owned Qt timers dismiss only this test application's named result dialogs; no production flag bypasses dialogs.

Initial test expectations incorrectly omitted the source Close mnemonic and two-line filename, and assumed uppercase SHA-256; expectations now match the original resource/formatter. The nested cancellation fixture now pauses its owned listing before confirmation and identifies the cancelled result separately from a later successful Auto Refresh. The long-operation case waits for the asynchronous filesystem refresh to finish. These were fixture/observation corrections, not changes to the engine's compression, extraction or checksum results. A final source comparison also connected the original selected-row name seed; its inside-file/folder case and the five related encrypted/plain archive checksum cases were rerun. The original completion branch tests also check that errors plus Cancel close correctly and that successful completion does not force the final progress value to 100.

The initial group was:

```bash
source scripts/env.sh
BUILD="$DEPS/build-comments-20261004" # or another configured build
export PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins"
ctest --test-dir "$BUILD" --verbose --output-on-failure -R '^(progress_completion|progress_presentation|dialog_resources|language_settings|native_progress)$'
"$BUILD/port_tests" "$SEVENZIP_BINARY" roundtrip cancellationAndResponsiveness nestedArchiveNavigation nestedArchivePasswordsAndCancel archivePauseResumeAndCancel openWithArchiveCommands openWithCancelAndBusyQueue archiveContentsHashes guiArchiveFolderCreation
```

`scripts/test.sh --no-focus` includes the new completion suite. The workflow in [development-workflow.md](development-workflow.md) keeps substantial source-based implementation groups together before validation. Bundle checks are recorded separately in [progress-completion-bundle.log](progress-completion-bundle.log); final empty-directory clean build and all-format acceptance remain release work.

The latest package is `/DEPS/build-comments-20261004/7-Zip Mac.app`. The dependency checker found 15 Mach-O files with only system/@rpath dependencies. Deep/strict ad-hoc signature verification passed. Bundled corresponding Port source files match the current result implementation. With development Qt/DYLD paths removed and owned temporary preferences, the bundled executable stayed alive for three seconds with empty stdout/stderr; only that owned process was stopped. This confirms local bundle startup, not physical menu/Finder acceptance or the final clean build.
