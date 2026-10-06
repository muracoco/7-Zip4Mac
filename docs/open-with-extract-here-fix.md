# Open With extraction window/lifecycle hotfix — 0.2.1

A user reported a failure after Finder Open With → Extract Here in Downloads.
The installed 0.2.0 application matched the reviewed distribution binaries.
The issue was reproduced using a copy of the user's official `7z2603-src.7z`
in a dedicated Downloads test directory. No extraction was performed into the
user's existing Downloads root, and no user output was replaced.

## Reproduced behavior and cause

The archive passed official console Test. Extract Here restored 1,292 files;
every SHA-256 matched extraction by the unchanged official console. However,
a blank native `7-Zip File Manager` window remained onscreen afterwards. Qt's
MainWindow stayed hidden, so its menu, toolbar and contents were not painted.
The existing desktop acceptance had exercised Extract to and Manager opening,
but had not verified this standalone Extract Here completion/lifetime. The
prior completion claim therefore missed a real desktop-path defect.

ProgressDialog always used Qt::WindowModal. In Qt's Cocoa backend,
`QCocoaWindow::setVisible` calls `beginSheet` on its native transient parent for
that modality. Attaching a sheet can order the hidden parent NSWindow onscreen.
The Open With idle-exit check also ran only once after a closing event; a longer
operation or the following filesystem refresh could outlive that check.

## Repair

- A visible File Manager retains its window-modal native progress sheet.
- A hidden manager uses an application-modal standalone progress dialog,
  retaining QObject ownership, progress callbacks and cancellation controls.
- The idle-exit check is coalesced by a single timer, retries while work remains,
  and observes top-level dialog Hide as well as Close. Visible windows or queued
  FileOpen requests prevent quitting. The archive engine/handlers are unchanged.
- Bundle/About versions are 0.2.1 to distinguish the installed repair from
  retained older development copies in Finder's Open With list.

## Executed affected checks

The new native regression failed against the original implementation:
`Open With progress ordered a hidden native File Manager window`.
The failed result is retained separately from the repaired run.

After repair, all four selected CTest suites passed (10.19 seconds):

| Test | Observed result |
|---|---|
| cocoa_open_with_progress | Hidden parent remains invisible during/after progress; a visible manager retains its native sheet and remains visible afterwards |
| cocoa_open_with_exit_7z | Real Open With menu, 513 files plus an empty directory extracted correctly; application event loop exits normally without ordering the hidden manager |
| cocoa_open_with_exit_zip | The same operation/content/lifetime checks pass for ZIP, including a Japanese/spaced filename |
| progress_completion | Existing successful/failed/Test completion behavior remains valid |

The affected integration functions `openWithArchiveCommands` and
`openWithCancelAndBusyQueue` passed together (4/4 Qt cases including
setup/cleanup, 2.05 seconds). They cover compression, Here/to/dialog extraction,
password retry, batch Test, nonblocking Cancel and queued FileOpen recovery.
Unchanged format/engine and unrelated portable-command groups were not rerun.

The repaired application implementation commit is
`ee951f055cbc7e33ff56ce621455d3aed48baa51`.
After packaging/signature/dependency verification, the installed app was upgraded
to 0.2.1 with a recoverable local copy of the previous installation. Actual
Finder Open With → 7-Zip Mac (0.2.1) → Extract Here completed on a fresh dedicated
Downloads child directory. All 1,292 output SHA-256 hashes matched the official
console reference; the original operation process exited without a blank window.
The installed bundle passed strict signing and all 15 Mach-O dependency checks.

The first post-click filesystem probe ran while progress was still active and
incorrectly asserted completion: its zero installed-file count is retained as
a premature test-driver observation, not a new application failure. After the
operation exited, the UI observation helper restarted the app into a fully
painted File Manager with a different PID. That normal restarted window was
closed; it is not the original ghost-window defect. The corrected completed-job
verification and both observations are retained in the local receipt.

The corresponding-source bundle predates this final desktop receipt but contains
the same repaired runtime implementation. Later verification text does not
require another application build or unrelated engine/format tests.

## Reproduce

```sh
./scripts/build.sh
PORT_TEST_REGEX='^(cocoa_open_with_progress|cocoa_open_with_exit_7z|cocoa_open_with_exit_zip|progress_completion)$' ./scripts/test.sh
```

Native window state and the actual Finder path are distinguished from physical
keyboard, drag, printer and Windows pixel-equivalence claims. Previous broad
release results remain in [release-consolidation.md](release-consolidation.md)
and [release-0.2.0.md](release-0.2.0.md).
