# Command-entry workflow port

The implementation is based on official 7-Zip 26.03, rather than a collection
of generic Qt input prompts. Related workflows are implemented as a group,
then tested together. A failed implemented workflow is recorded as a bug;
a source-defined workflow still missing is recorded as unimplemented.

| Workflow | Status | Source / implemented behavior |
| --- | --- | --- |
| Comment, Select / Deselect, Create Folder / File input | Implemented | `ComboDialog.rc`, `.cpp`, `PanelOperations.cpp`, `PanelSelect.cpp`, `BrowseDialog.cpp::Dlg_CreateFolder`; editable combo, source labels/defaults, empty choices unless the caller supplies them, OK/Cancel, tab order and source `OnSize` |
| Wildcard selection | Implemented | Unchanged `Common/Wildcard.cpp::EnhancedMaskTest`, imported with a pinned hash; `*` / `?`, case-insensitive UTF-16 comparison and literal brackets, with normal and alternative selection |
| Rename entry | Implemented | `PanelOperations.cpp::RenameFile`, `OnBeginLabelEdit`, `OnEndLabelEdit`; focused-row name editor in all four views, original raw filename, Enter/Escape and deferred model reload |
| Filesystem rename | Implemented | Worker operation with exclusive installation and identity checks; files, directories, links, case-only names, existing-output refusal, metadata preservation, cancellation and errors |
| Nested folder creation | Implemented | `FSFolder.cpp::CreateFolder` uses `CreateComplexDir` after a simple create fails; safe relative multi-component names are accepted by the Port, and 7z/ZIP native folder operations are retained |
| Create File | Implemented | Source default `New File` and exclusive creation; existing files/links are never overwritten, and missing parents are not implicitly created |
| Folder history | Implemented | `PanelFolderChange.cpp::FoldersHistory` and `ListViewDialog.cpp`; first-row focus, multi-row Delete, Ctrl+A / Ctrl+C / Ctrl+Insert, Enter/double-click selection, Alt+Enter detail, accepted deletions persisted and cancelled deletions discarded |
| Filesystem / ZIP comments | Partially implemented | Existing strict UTF-8 `descript.ion` and native ZIP comment operations are retained and connected to the source combo. Flat filesystem comment editing and the documented encrypted ZIP descriptor restriction remain separate work |
| Copy / Move destination dialog | Partially implemented | Safe asynchronous transfer is already available; the separate original `CopyDialog` workflow has not been fully ported in this group |
| Absolute / parent-relative creation names | Implemented | Subsequent address-workflow batch imports original FSFolder::GetAbsPath and accepts explicit absolute / parent-relative FS creation; entered symlink parents remain a safety policy. See address-workflow-port.md for current validation |
| Native function keys / Finder clicks | macOS difference | Qt event delivery can be tested locally; physical Cocoa/Finder input and Fn-key behavior require an unlocked desktop and remain unconfirmed here |

The Qt boundary does not copy Win32 APIs, Microsoft fonts or proprietary OS
assets. `ComboLayout.inc` retains the original `CComboDialog::OnSize` body;
`DialogGeometry.inc` expands the original numeric resource with its pinned
SHA-256. `WildcardMatch.inc` retains the original algorithm. These sources
and generators are included with the Port, keeping LGPL attribution.

Tests use owned temporary directories and separate preferences. The grouped
run covers command entry, existing archive selection/update/comment paths,
file listing, selection, dialog resources, overwrite/move and checksum result
controls. Tests respond to the actual combo dialogs and in-list editors; they
do not substitute a modal rename dialog or count unavailable native clicks
as successes. Full-format and fresh clean-build acceptance remains the final
release gate, rather than being repeated after every local change.

## Verification

Latest distinct results: **146 passed, 0 failed, 1 skipped** across the nine
suite selections below. Setup/cleanup are counted once per suite; repeated
runs are not added to the count. The cross-volume move case requires
`PORT_TEST_SECOND_VOLUME` and was skipped, not counted as a pass.

| Suite | Passed | Skipped | Latest evidence |
| --- | ---: | ---: | --- |
| Command entry | 27 | 0 | [command-entry-test.log](command-entry-test.log) |
| File listing | 11 | 0 | same log |
| Selection | 16 | 0 | same log |
| Native Agent GUI selection/update | 11 | 0 | [command-entry-agent-test.log](command-entry-agent-test.log) |
| File-operation selections | 6 | 0 | [command-entry-fileops-test.log](command-entry-fileops-test.log) |
| Comments | 31 | 0 | [first grouped run](command-entry-first-test.log) |
| Dialog resources | 10 | 0 | same log |
| Progress completion / result controls | 15 | 0 | same log |
| Overwrite / move | 19 | 1 | [repair run](command-entry-repair-test.log) |

The first grouped run exposed a file-worker completion gap: `busy()` became
false before the GUI handled its completion and refreshed the list. The
worker now holds an active flag through the completion notification. The
next run also exposed creation through macOS `/var` aliases; resolving the
explicit browsed base fixes it while retaining no-follow checks for entered
relative components. A test result error box caused a timeout during that
run; the existing owned-result driver now dismisses it, so failure assertions
complete normally. The ZIP fixture's writer method was corrected to Deflate.

Native GUI regression checks exposed partial-row Qt selection being dropped
by the source list adapter: `QTreeWidgetItem::isSelected()` and
`selectedRows()` do not cover cell selection with Full Row disabled. The
adapter now converts Qt's selected items into the original item-status
vector and selected-item count. The same-name external-editor/rename/
write-back checks then passed. [Original Agent results](command-entry-agent-first-test.log)
and [diagnostic failure](command-entry-agent-repair-test.log) are retained.

Confirmed: English source defaults, editable combo geometry/resizing,
literal-bracket/case/UTF-16 wildcard semantics, all four rename views with
both selection modes, Escape and closing during label editing, partial-row
selection, case-only rename, symlink/directory/read-only metadata retention,
existing output and hard-link refusal, parent permission and cancellation,
nested FS/7z/ZIP folders, actual archive Test/extraction and file-content
comparison, and accepted/cancelled history deletion.

Reproduce the entire affected group after building:

```bash
./scripts/test-command-entry.sh /absolute/path/to/build
```

Only failed/affected selections were repeated after repairs. Full formats,
physical AppKit/Finder/Fn input and the final fresh clean build are not claimed
by this milestone. Bundle/startup evidence is recorded separately.


The rebuilt local app is
`/DEPS/build-comments-20261004/7-Zip Mac.app`.
[Bundle and owned-startup evidence](command-entry-bundle.log) confirms 15
Mach-O files with system / bundle-relative dependencies, valid deep strict
ad-hoc signing, and an owned startup alive after three seconds with empty
stdout/stderr. Development Qt/DYLD paths were removed and a temporary
Japanese/space folder was supplied. Existing user preference files remained
unchanged. This is startup evidence, not physical menu/Finder verification.

Full portable parity and publication are still incomplete. The previously
recorded QtSvg / image-plugin corresponding-source packaging review remains
a final release gate; this source/command batch does not declare that review
complete.
