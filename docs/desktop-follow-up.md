# Desktop menu follow-up — 2026-10-06

The menu/Options group is implemented and its affected automated and AppKit
checks pass. This checkpoint does not declare complete physical Finder/input
acceptance or publication. Existing all-format and archive-workflow evidence
is reused; the engine and archive operations did not change.

## Observed defect and source-based repair

Native desktop navigation opened Tools → Options and all six pages. The fixed
**7-Zip** tab caption was blank: `UiLanguage::apply` replaced it with the empty
translation for `IDD_MENU`. The original `MenuPage2.rc`, included by
`MenuPage.rc`, supplies this caption; `OptionsDialog.cpp` requests an optional
language title, and the Win32 property sheet retains the resource caption when
that title is absent. `LangUtils.cpp` likewise changes a resource control only
when its translation exists.
The Qt tab adapter now retains the original caption when lookup is empty,
including repeated language application. This is a defect in a connected
setting page, not an omitted Options command.

The language suite checks all 93 official choices (English and 92 language
files): six nonempty captions and the fixed 7-Zip caption. The rebuilt app was
also opened through the native desktop UI; Tools → Options was clicked and the
7-Zip caption was visibly present. Cancel returned to File Manager without
applying preference changes.

## Completed grouped verification

| Stage | Executed result | Scope |
|---|---|---|
| Initially empty Release/Ninja build | Passed, 252 application steps | Qt/engine deployed, ad-hoc signed; 15 Mach-O files use system/@rpath dependencies only |
| Affected UI group | 3/3 suites, 8.16 seconds | `dialog_resources`, `language_settings`, `compression_help`; Qt totals 10/10/47 including setup/cleanup, zero failures/skips |
| AppKit menu group | 1/1 suite, 3.75 seconds | Native event delivery inside the owned test process: six top menus; About, Rename, Properties, right-button file context menu, Options Apply; FileOpen with Manager already visible and native key-window/modal recovery |
| New bundled startup | Passed | Alive after three seconds, isolated INI profile, no stdout/stderr, existing preferences unchanged, invalid relative profile rejected |
| Desktop-created archive | Passed independent content verification | Native Add dialog created a 7z from six roots containing five files/two directories, including Japanese, spaces, an empty folder and a 48 MiB file; pristine 7zz Test/extraction exited 0; all five SHA-256 values and both directories matched |

The native menu test now includes right-button NSEvent delivery and the
already-visible File Manager FileOpen case. These are compiled **and executed**,
not Qt mouse-event-only checks. They still do not prove physical keyboard input
or Finder's LaunchServices routing.

The desktop archive was created before the caption-only repair, using the prior
packaged candidate. Test and extraction in this checkpoint were independent
CLI checks of that GUI-created archive; they are not claimed as new desktop GUI
Test/extraction or ZIP observations. Earlier 7z/ZIP/right-click/151-registration
automated evidence remains in [release-consolidation.md](release-consolidation.md).

## Incomplete observations and first failures

An initial raw-shell test invocation exited 127 because `ctest` was not on PATH,
before executing a suite. Sourcing `scripts/env.sh` in Bash corrected the
launcher; the affected group then passed once. The failed invocation is retained
alongside the passes.

The desktop relocked during the first observation batch; the native suite was
run only after it became available again. A computer-use service crash and a
later `noWindowsAvailable` error interrupted external UI delivery. Application
processes remained alive; these tool failures are not counted as application
crashes or successful interactions. No lock/security settings were changed.

Finder right-click opened the real Open With submenu. Selecting the explicit
candidate through Other did not yield a completed operation-menu observation;
a later retry was interrupted by a changed Finder state. A File Manager AX
right-click selected the row without an observed popup. The AppKit right-button
regression passed, but these incomplete external interactions remain pending.
No further feature scope or speculative repair is inferred from them.

| Remaining acceptance | Status |
|---|---|
| Current packaged app Tools → Options and fixed tab caption | Observed through native UI |
| Native test-process menu commands/right-button/FileOpen/Options Apply | Passed AppKit group |
| Finder → Open With → explicit current app → operation menu | Observed through native Finder UI; see follow-up below |
| Open With command completion and final Manager command | Pending complete desktop observation |
| External desktop file context commands and Finder drag/drop | Pending complete desktop observation |
| Physical Fn, RightCtrl and Option input | Pending; virtual keys do not establish hardware behavior |
| Print output and Windows pixel comparison | Pending |

## Evidence and continuation

Captured first-launch/build/test/archive logs are in
[desktop-follow-up-tests.log](distribution.md); original report and
changed-code SHA-256 values are in
[desktop-follow-up-evidence.json](distribution.md). Only
machine-specific path prefixes and trailing whitespace in the captured logs
are normalized. Private
desktop screenshots and crash reports are not included.

The new bundled candidate contains the repaired application and the working
source snapshot from its build. This later documentation is not claimed to
have existed in that earlier source archive. Do not rebuild or rerun passing
format/archive suites solely to incorporate this report. Complete the remaining
desktop acceptance as one group, repair only reproduced failures, then prepare
the final reviewed source/package and conditionally authorized publication.

## Shared Open With/context presentation refactor

The next source group removes duplicated command visibility, display names,
icons, target-existence checks and CRC submenu construction from the Open With
dialog and File Manager context menu. Both now consume the same presentation
and submenu builders. Original ordering, independent Open/Open As choices,
configured items/icons, callbacks and the final Manager entry are preserved.
Early FileOpen capture and the controller's request queue were inspected and
already connected; they were not speculatively rewritten.

The complete changed menu group passed once: four Open With operation/queue
functions (6 Qt totals including setup/cleanup), Open As ordering/visibility/
Apply-Cancel (9 totals), actual Qt context-menu 7z/ZIP compression/Test/extraction
and three CRC submenu operations (3 totals), plus the AppKit menu group (1/1,
3.38 seconds). There were no failures/skips. The context test uses an explicitly
empty **context-only** registration manifest; it does not rerun or claim new
coverage for the 151-format matrix. Actual Japanese fixture hashes, checksum
files, encrypted extraction, cancellation and queue recovery are checked.

Only the affected library/test targets were initially rebuilt (10 completed
Ninja tasks); the running application bundle was left intact during tests. The
owned app was then closed through its native close button and its process was
confirmed absent before updating the bundle. The application relink (one
task), source packaging (793 tracked regular files), ad-hoc signature and
strict 15-Mach-O dependency check passed. Isolated bundled startup again passed
with no stdout/stderr and no preference changes. Packaging/startup output for
this refactored candidate is retained with this group in the linked logs/evidence.
The full final empty-directory release gate remains deferred until desktop
acceptance is complete; this affected build is not called a new clean build.

The external desktop attempt again could not complete Finder/context/drag
acceptance: Finder had no capturable window (`cgWindowNotFound`), and coordinate
input into the visible Manager was rejected as `noWindowsAvailable`. AX row
click/key attempts did not change the observed selection. AppKit regression
events succeeded. These results do not establish a production input defect or
complete Finder routing; no alternate UI automation or OS setting change was
used to bypass the tool limit.

Resetting the computer-use bindings and selecting the updated app restored an
observed row selection through AX. Right-click did not produce an observed
popup, and subsequent Select All/Edit-menu attempts did not advance the UI.
These are incomplete external observations, not additional passing menu tests
or a proven application defect. Private app/browser inventory was not copied
into the project evidence.

## Explicit current-app Finder observation — 2026-10-06

File → Open Outside on the owned empty-directory fixture opened a real Finder
window. Finder navigation and right-click on the desktop-created `desktop.7z`
then exposed Open With. The ordinary named 7-Zip entry selected an older test
build with the same bundle ID. That older process is not evidence for the
current candidate and was stopped after verifying its exact executable/PID.
No LaunchServices registration or default association was changed.

Open With → Other, followed by the explicit current candidate path, displayed
the current operation menu while Manager was already running. The observed
menu included Open, Extract files/Here/to folder, Test, Add, quick 7z/ZIP and
CRC SHA, followed by **7-Zip ファイルマネージャーで開く**. Always Open With remained
unchecked. Open As was absent under the existing independently saved setting;
this is not a missing command.

A subsequent Test click was rejected before delivery by the computer-use
service's changed-app guard. Later Manager right-click used an invalidated AX
row. ScreenCaptureKit streaming and closed-Finder-panel capture also failed
during the attempt. These are incomplete observations, not successful Test or
context operations and not reproduced application crashes. Actual current-app
command completion, drag/drop, hardware keys, print and appearance remain
required. Private screenshots and the app inventory are not published.
