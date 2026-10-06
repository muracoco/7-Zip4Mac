# Official Progress presentation and control batch

This checkpoint imports the official 7-Zip 26.03 Progress implementation as one feature group. Statistics, file/status text, titles, numbered errors, Copy, Background and Cancel were connected before running their related validation group. It is not the final Windows-parity release.

## Source reuse

`scripts/import-progress-presentation.py` checks the pinned upstream source hashes and generates `ProgressPresentation.inc`, `ProgressConverter.inc` and `ProgressOperationText.inc`. The imports retain the original LGPL attribution and are supplied with the original sources and Port adapters in the app source archives.

The reused bodies include `UpdateStatInfo`, filename/status reduction, title/Pause/priority text, progress range conversion, size formatting, message grouping and column resizing, selected/all-message Copy, the update-operation resource table, `MyFormatNew` and `SetExtractErrorMessage`. Original undefined UInt64 sentinels remain distinct from zero. The original overflow-aware `MyMultAndDiv` helper also replaces three unchecked products for speed, percentage and compression ratio.

Qt adapters supply Unicode strings, widgets, the GUI-thread monotonic clock and queued worker snapshots. POSIX directory separators and clipboard line endings replace Windows separators/CRLF. The Windows taskbar API is omitted on macOS. The original callback bodies still perform compression and extraction; the added optional JSON telemetry does not implement a codec or change archive algorithms.

## Connected behavior

- Native callbacks transmit archive title, current file, directory flag and operation status separately. Scanning, compressing, extracting, testing, reading and the original update-operation states use their source resource IDs. Routine per-file telemetry remains throttled.
- Filename text uses the original two-line directory/name arrangement and middle abbreviation. The original progress calculation and title prefix update both the dialog and its parent File Manager window.
- Typed callback errors carry operation result, encryption flag, file and archive. The original GUI formatter produces the localized message. The source message list numbers each logical error and leaves continuation lines unnumbered. Copy selects rows in visual order or copies all rows when no rows are selected. Raw console diagnostics remain separately retained.
- Background toggles process priority and the original Foreground/Background label; the window stays visible. Owned engine processes and verification workers follow the toggle, including while paused. A lease restores the GUI process's prior policy after completion/destruction and supports overlapping operations. macOS uses reversible `PRIO_DARWIN_PROCESS` background policy instead of Windows process priority classes; no administrator privilege is needed on the tested Mac. The platform implementation is documented in [Apple XNU](https://github.com/apple-oss-distributions/xnu/blob/main/bsd/kern/kern_resource.c).
- Cancel uses the source Yes/No/Cancel question and pauses an available running operation during confirmation. No/Cancel resumes the previous running state; an already paused operation stays paused. Yes requests cancellation once. Completion during the modal question is deferred and prevents a late cancellation of a completed operation. Escape and window close use the same path.
- Passwords are redacted from file/title/status/error telemetry at the GUI boundary and remain absent from argv and diagnostic logging.

## Validation

Environment: Apple Silicon macOS 26.6.2, Apple Clang, Qt 6.11.3. CMake/Ninja built the full app and native callback helper. The [raw log](progress-presentation-test.log) preserves the initial grouped failures and targeted reruns.

Latest result per selected case, counting setup/cleanup once per suite: **122 passed, 0 failed, 0 skipped**. Progress presentation: 15; dialog resources: 8; language/settings: 10; native progress: 15; compression/help: 46; selected integration: 9; affected Open As: 19. This aggregate is not a claim that all passing cases were repeated after each change, a final all-format acceptance run, or physical mouse/Finder verification.

The initial group found a real duplicate-error bug: backend Test retried a failed CRC operation with a forced extension handler. Typed file-operation errors now prevent that retry, while archive-handler fallback remains available for an unrecognized archive. The independent unmodified `7zz` produced one CRC error; the final Qt typed-error test now also records one. Existing all-language and Pause expectations were updated for the original two-line filename and accelerator-bearing Continue label.

Background fixture corrections are retained in the log. Apple's Python launcher introduced a separate interpreter PID, so the final fixture is a directly owned native C++ process. A second false assertion used `getpriority` to inspect a child: Darwin distinguishes internal and externally enforced background policies, so the witness now inspects public process flags (`PROC_FLAG_DARWINBG` / `PROC_FLAG_EXT_DARWINBG`). The final case verifies Background/Foreground transitions, GUI policy restoration, cancellation/source preservation, and worker-child priority changes while paused. It passed in 1.08 seconds. It does not measure compression throughput or physical desktop latency.

Real 7z/ZIP round trips include Japanese/spaced filenames, empty directories and 32 MiB data with SHA-256 comparison. The related existing suites cover encryption/codecs, corrupt archives, Pause/Cancel, GUI event-loop responsiveness, Open With compression/Test/extraction, and forced-handler recovery. Cancellation confirmation verifies all Yes/No/Cancel choices in running/paused states and completion during the question. Tests use owned temporary files and stop only their owned processes; message-copy formatting does not overwrite the user's clipboard.

The initial grouped invocation was:

```bash
source scripts/env.sh
BUILD="$DEPS/build-comments-20261004" # or another configured build
export PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins"
ctest --test-dir "$BUILD" --verbose --output-on-failure -R '^(progress_presentation|dialog_resources|language_settings|compression_help|native_progress)$'
"$BUILD/port_tests" "$SEVENZIP_BINARY" roundtrip cancellationAndResponsiveness nestedArchivePasswordsAndCancel archivePauseResumeAndCancel openWithArchiveCommands openWithCancelAndBusyQueue
```

Only failed cases and the affected `open_as` suite were rerun after repairs. `scripts/test.sh --no-focus` now includes `progress_presentation`. Packaging completed for `/DEPS/build-comments-20261004/7-Zip Mac.app`. The checker found 15 Mach-O files with only system/@rpath dependencies; deep/strict ad-hoc signature verification passed. With development Qt/DYLD paths removed and owned temporary preferences, the bundled executable stayed alive for three seconds with empty stdout/stderr; only that owned process was stopped. The [bundle log](progress-presentation-bundle.log) records these checks. This is not a final empty-directory clean build or a physical Finder check.

## Remaining parity

| Item | Classification | Limit |
|---|---|---|
| Listed imported presentation bodies and connected control paths | Implemented | Verified against source expectations, real Qt widgets/callbacks and owned native child processes |
| macOS process priority, POSIX separators and native title bar | macOS difference | Platform adapters replace OS-specific APIs |
| Windows taskbar progress overlay | macOS difference | No Win32 taskbar API on macOS |
| Successful-operation auto-close/message-box sequence | Partially implemented | The Port keeps its existing result and Close button; original completion-window policy is not yet fully reproduced |
| Full error transport under a saturated optional telemetry socket | Partially implemented | Operation result and raw console diagnostics remain retained, but optional error packets can be dropped |
| Physical Finder, keyboard/function-key, focus and visual comparison | Partially implemented | Dedicated widget tests do not establish physical desktop operation |
| Final clean build, all-format acceptance and publication | Partially implemented | Still required for the final release; unrelated previously passing suites are not repeated for each UI edit |

The subsequent [completion/result group](progress-completion-port.md) implements the original auto-close, Test summary and checksum list policies previously left open at this checkpoint. Physical comparison and the final clean-build release checkpoint remain pending.
