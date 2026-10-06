# Consolidated build and verification — 2026-10-06

The available automated release gate is covered. This is a development
checkpoint, not a declaration of complete Windows parity or a public release.
Native desktop acceptance remains pending; the later local publication review
is a separate recorded batch.

The subsequent [publication preparation](publication-review.md) records the
license/source/privacy review, Qt notice and checkout-independent cache repair.
It also completed a fresh build of the repaired candidate from a Git-free,
normalized source copy, followed by affected UI/Cocoa and bundled startup checks.
This supersedes the earlier empty-build limitation below; it does not replace
the still-pending physical desktop acceptance.

## Latest candidate and grouped repairs

A later initially empty Release/Ninja build from `1771756` completed 252
application tasks and packaged 801 corresponding-source files. Its complete
available release run executed all **35** registered application suites:
**32 passed / 3 failed**, 623.82 seconds. Native Agent tree and selection,
**151/151** format registrations, signature/dependencies and private-profile
bundle startup passed independently. This supersedes the earlier 33-suite
inventory for the current candidate, without rewriting that historical run.

The source-defined repair group is now implemented and its affected checks pass:

- Original `App.cpp::OnCopy` clears source marks even after Cancel, restores the
  lists and then restores source focus. The asynchronous adapter now remains
  busy through both panel reloads, retaining password retries and preventing
  an idle gap before selection restoration.
- Original `CApp::LastFocusedPanel` determines menu dispatch. On Cocoa, closing
  a modal progress window can leave no active Qt window and `setFocus` emits
  no change. Explicit list focus now also records the requested panel, so the
  menu cannot keep operating on the destination after source selection returns.
- Legacy GUI drivers now address the owning panel, explicitly choose Details
  before input, use typed directory roles and the original selection API,
  and await asynchronous rename/create/navigation completion. Link folder
  choice types the actual folder path and queues acceptance, avoiding the
  retained filename in Qt's directory picker. The app uses Qt directory pickers;
  desktop mouse choice is still a separate observation.

| Affected verification | Actual result |
|---|---|
| Copy and address group after the final focus repair | 2/2 suites, 55.98 seconds; Qt totals 41 and 25, zero failures/skips |
| List-key and native AppKit menu group | 2/2 suites, 16.92 seconds; all checks passed |
| Repaired integration cases | GUI navigation/Add/Test/Extract passed; encrypted open/drop, Cancel and Options persistence/viewer/editor/Diff passed in the selected follow-up |
| Link folder choices | Both From/To cases passed; 4 Qt totals including setup/cleanup |

The first seven-suite follow-up also passed selection, context/drag, address and
AppKit checks. Initial failures are retained. One later Integration driver run
was deliberately stopped after finding a wrong action name in the harness;
Link's retained filename triggered a Qt item-not-found dialog and timeout.
Those are recorded test-driver/execution failures, not hidden application
successes. The final active-panel defect was repaired in application code.

[Candidate report summary](distribution.md) and
[original-log hashes/case IDs](distribution.md) preserve these
results. Coverage is a union of the initial run and affected repairs, **not** a
claim that all 35 suites passed in one uninterrupted run. The engine and
registered format matrix did not change during the GUI repair and are not run
again. The repaired `8ba8a7a` tree subsequently completed another initially empty
252-task Release/Ninja build, 803 corresponding-source files, strict signature/
dependency checks and private-profile startup. Its reviewed local public-source
copy contains 803 files, with internal history excluded and 120 documentation
files normalized. Physical desktop/Finder/modifier/print/appearance acceptance and
conditional public publication remain pending.

### Desktop operations and access-key repair

That new packaged candidate's actual desktop Open With buttons created 7z and
ZIP from a Japanese/spaced filename; both contents matched the original SHA-256.
ZIP Test displayed one archive/one file and no errors. Extract to created the
named folder and matching file. The final Manager button displayed the ZIP path
and its member in File Manager. F5 opened the original Copy dialog and Cancel
returned to the list. These observations started with CLI `--open-with`, not
Finder routing. Owned app processes exited normally after closing them.

External coordinate right-click delivery still failed with `noWindowsAvailable`,
while AX retrieval showed the running list. AX right-click selected a row but
no popup was observed. Close-state retrieval sometimes timed out after the
owned process exited 0. These are separately recorded tool limitations; the
executed AppKit right-button regression passed.

Desktop Alt+T had no effect. Official QtBase `qkeysequence.cpp` explains that
macOS disables resource/menu mnemonics by default. `applyPortAppearance` now
turns that application-local feature on for original 7-Zip ampersand labels;
no OS keyboard setting is changed. The complete affected group passed once:
4/4 suites, 12.96 seconds (resources, 93 language choices, compression Help and
native AppKit menus). Native Option/Alt events opened all six original menus;
mouse menus/context/About/Rename/Properties/Options Apply/FileOpen also passed.
The first compile attempt lacked Qt's explicitly required function declaration;
that build failure is retained and the declaration repair built successfully.

The source-frozen `07968cd` candidate completed an initially empty 252-task
Release/Ninja build and packaged all 803 corresponding-source files. All 15
Mach-O files passed system/@rpath dependency and strict ad-hoc signature checks.
Private-profile bundled startup passed without changing native user preferences;
an invalid explicit profile was rejected without falling back to those preferences.
The same snapshot passed local public-source export, with 120 documentation files
normalized and internal Git history excluded. This was local preparation, not
publication.

In that final packaged application, actual desktop native input Alt+T opened
Tools and O opened Options. Clicking Cancel returned to the Japanese/spaced ZIP
listing; the owned process then exited 0. Qt emitted font-script fallback and
stale-cell accessibility warnings, retained as warnings rather than hidden or
reported as a crash. This is desktop automation input, not a hardware-key check.
All-format/engine checks are reused because this group does not change them.
Physical Finder routing of this final package, drag/drop, hardware modifier/Fn,
printing and Windows pixel comparison remain verification limits. Complete
parity and public publication are not declared.

## Implementation and workflow batch

The current source-defined command groups are recorded in
[current-status.md](current-status.md). The audit counts 65 enabled static
Windows menu commands; settings, dynamic menus and keyboard dispatch have
their own source inventories. Historical component backlog statements are
superseded by the connected groups, rather than treated as new missing features.

This batch consolidated the inventory, release runner and source packaging
before the first functional run. Failed and previously unexecuted cases were
then checked selectively. Passing format registrations were not rerun simply
because another component failed.

Source packaging now uses tracked regular working files, Git-normalized modes
and an embedded SHA-256 manifest. Ignored/untracked data is excluded. The source
archive also supports rebuilding without a Git checkout. The no-focus runner
includes every registered independent suite and continues after an earlier
failure. Each release stage has a bounded, owned process group.

## Executed results

Host: macOS 26.6.2, Apple M3 arm64, Apple Clang 21, Qt 6.11.3, official
7-Zip 26.03. An initially empty `build-release-20261006.0Hx1wx` completed
252 Ninja steps, deployment and signature/dependency checks. Subsequent repairs
were rebuilt in that directory. This is an empty-build check followed by
affected repairs, not a claim that the final repaired tree had a second empty
build.

| Gate | Actual result |
|---|---|
| Filesystem / 7z / ZIP workflows | Creation, listing, Test, extraction and SHA-256 comparisons passed, including Japanese/spaced names, empty/nested directories and 32 MiB data. |
| Encryption / updates | Correct/wrong passwords, header encryption, ZIP AES/ZipCrypto, original Agent mutation and nested write-back checks passed. |
| Registered independent application suites | 33 suites covered by the initial run and failed/unexecuted follow-ups. The first CTest process reported 30 passing / 3 failing suites; its nonzero result is retained. No second full run is claimed. |
| Native Agent tree / selection | All executed cases passed after the affected selection/cancellation and toolbar-result repairs. |
| Format registrations | 151/151 registrations across 61 handlers and 138 distinct extensions passed, combining 137 initial successes with 14 affected follow-ups. [Per-registration results](release-format-coverage.md). |
| Right-click command path | Qt mouse events exercised 7z/ZIP creation, Test, Extract to, Japanese output paths and SHA-256 comparison. Physical Finder clicks remain unverified. |
| File publication repair | Final affected CTest group: 4/4 suites, no failures/skips, 108.39 seconds. Qt totals include setup/cleanup: Copy 41, extraction metadata 322, links 28, overwrite 20. |
| Cross-volume move | Local APFS ↔ mounted SMB, overwrite, automatic backup, nested Japanese file and empty-folder preservation passed. This checks the mounted server, not every network filesystem. |
| Bundle | Latest repaired executable redeployed: 15 Mach-O files with system/@rpath dependencies only; strict ad-hoc signature verification. The bundle process stayed alive, created its private INI profile, left native user preferences unchanged and rejected an invalid relative profile. No development Qt environment was supplied. |

The combined case record contains 1,180 distinct passing Qt case IDs, excluding
setup/cleanup and duplicate reruns. This is a union of recorded runs, not one
uninterrupted process. Non-Qt Cocoa ownership checks and bundle checks are
separate. Initial failures, timeouts and later passing results are retained in
[release-tests.log](distribution.md); case/report hashes are in
[release-evidence.json](distribution.md). Published log copies replace
machine-specific path prefixes; original logs are retained in the local cache.

## Findings and repairs

| Finding | Classification and resolution |
|---|---|
| Escape in the editable address field bypassed scan cancellation | Application defect. Cancel the pending filesystem read before restoring address/list focus; the cancellation/navigation and address cases passed. |
| SMB output timestamps changed when writer handles closed | Application defect. Close completed private writers before taking an authoritative read-handle snapshot. The exclusive fallback refreshes output attributes and verifies copied bytes with SHA-256 while retaining the known output identity. Existing race/rollback, link, overwrite and extraction-metadata checks passed. |
| Duplicate names waited for an unanswered overwrite prompt | Stale test condition. Exercise explicit overwrite and check actual output; duplicate regular archive names are not traversal paths. |
| Read-only overwrite was expected to fail | Stale test expectation. Original DeleteFileAlways and an independently executed official macOS 7zz both permit explicitly authorized replacement on a writable parent. |
| Copy assumed alphabetical/display order | Stale test expectation. Original Get_ItemIndices_Selected enumerates original folder indices; filesystem enumeration and native folder row order supply the expected names. Qt NoSort retains the directory's default sort; Unsorted requests raw enumeration. |
| Hidden windows prevented inline Rename | Stale fixture setup. Show each test window for the original inline editor. Nested Yes/No/Cancel and navigation cases passed. |
| Replacement test counted three unrelated console Tests | Stale test trigger. Observe the typed before-install boundary and verify concurrent edit refusal and original archive retention. |
| Cancellation paused before the output stream existed | Stale test phase. Pause after processed data starts, then compare the retained partial prefix and subsequent reuse. |
| Result dialogs blocked test drivers | Test harness defect. Dismiss only owned Test/CRC result dialogs; preserve overwrite/cancellation questions. Parser failure text is captured independently of final result notices. |
| 14 document/executable aliases were expected to open inside by default | Stale expectation. Preserve the original external-open profile and use explicit Open Inside for reader verification. No installer is launched by these tests. |
| Running release script was edited during its first execution | Execution error. The initial script terminated before bundle startup; startup was resumed as an unexecuted stage. Scripts were not edited during the follow-up execution. |
| Incremental relinking restored the development Qt rpath | Packaging state. The final dependency check refused the unrepackaged executable. Redeploy Qt, restore the modified Cocoa plugin, re-sign and check the latest bundle; dependency and isolated startup checks passed. Functional suites were not repeated for this packaging step. |

The first format XML was incomplete after its context-result wait. Coverage
retains only complete incidents and applies the affected follow-up XML. It
records that incomplete report rather than silently treating it as complete.

## Remaining gates

The desktop was locked during this run. Physical menu/popup clicks, Finder
Open With/drop/right-click, Fn/RightCtrl/Option input, native focus, print output
and pixel comparison remain unverified. Qt logical activation/events do not
close those items. Optional Finder Extension/Quick Action/Services are not
implemented; the requested configurable Open With replacement is connected.

At this checkpoint, license texts, original 7-Zip/QtBase source, modification
notices and Cocoa patches were packaged, but final provenance/license and
privacy review was not declared complete. This review and a later fresh build
are now recorded in [publication-review.md](publication-review.md). A preliminary tracked-text credential-pattern scan found no
candidates; 105 files contain machine-specific path prefixes and require
publication review, including historical commits. No public repository, push,
tag or Release was created by this checkpoint.

Reproduction: `scripts/build.sh BUILD`, then
`scripts/verify-release.sh --no-focus BUILD` while locked. On an available
desktop, omit `--no-focus` and perform the physical checks. Use
`PORT_TEST_SECOND_VOLUME` for a writable second-volume directory; test files
are created only in dedicated temporary directories.
