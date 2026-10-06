# Official multiple-item Open policy

`scripts/import-panel-selection.py` now also retains official 7-Zip 26.03
`PanelItems.cpp::CPanel::OpenSelectedItems` in `src/upstream/PanelOpen.inc`.
The source SHA-256 is
`2dc5869e794788b366b1dc7fb6485b362e7b72352c1549303b944693765e2f73`.
Original copyright and LGPL notices are retained. `PanelOpen.cpp` supplies a
small Qt list/vector adapter and records actions; `MainWindowOpen.cpp` executes
those actions asynchronously.

## Behavior retained from the original body

- Open operates on the marked/native operated items in their original item
  order, rather than only the currently focused row.
- More than 20 operated items uses original message resource 3016. The parent
  row is considered after that limit check, according to the original native
  selected/focused-row conditions.
- Open attempts internal file opening only for a single operated item. Multiple
  files open externally, even if their bytes are valid archives.
- Internal folder opening stops the loop at that folder. Earlier file actions
  are retained. Open Outside instead records external folder actions and keeps
  processing subsequent items.
- Menu Open/Outside, Enter/Shift+Enter and both views' activation signals share
  the policy. Open Inside remains the focused-item operation.

Each archive-file action snapshots its native row identity before starting a
job. It receives a separate temporary extraction directory, so same-name ZIP
siblings cannot overwrite one another before launch. External edits use the
existing observed application lifetime, original UpdateOneFile and confirmed
write-back. The queue keeps automatic refresh and edit decisions out of the
interval between item extractions. Each extraction is asynchronous; the event
loop remains responsive. Failures can be reported per item. Manager close
clears the remaining owned queue and cancels the active job without terminating
external applications.

A single marked nested archive now passes that item's identity directly to
temporary opening, even when the independent native focus is on another item.
The previous selection-clearing workaround for single external opening is
removed. The external-extraction setup is shared by View/Edit, drag preparation
and the Open queue instead of being duplicated.

## macOS boundary and remaining scope

Finder cannot address a virtual archive folder. The port retains its existing
temporary-directory behavior for archive-folder Open Outside and opens the real
containing directory for an external parent-row action. Windows passes its
virtual path to ShellExecute. This is an explicit OS/port substitution.

Single filesystem-file default opening now attempts content-based archive
detection. The official extension profile and filename-warning bodies are also
imported; see [the following increment](open-profile-port.md). Link traversal
and remaining context/drop details are not declared complete by this batch. Physical Finder/mouse/Fn validation and final release/publication
gates remain pending.

## Executed checks

Final affected total: **83 passed, 0 failed, 0 skipped**, including Qt
setup/cleanup: panel Open 7, panel selection 16, editor write-back 36 and archive
open modes 24. [Execution evidence](distribution.md).

The new cases check original single/multiple/folder/parent/20-item policy;
actual 7z and ZIP multi-file launch; genuine same-name ZIP entries; preserved
marks and GUI heartbeat; a valid inner archive opened externally in a multiple
selection; both files' confirmed write-back; independent extracted bytes; and
single marked nested opening with a different unmarked focused file.

A disposable background app claims only a fresh random extension, is registered
and unregistered for each test, and records actual LaunchServices FileOpen
events. Existing formats/default associations are untouched. The receiver did
not register from the system temporary tree; placing it in an owned temporary
directory under the user's Applications directory made the real default launch
path pass. No user application is launched or quit by these fixtures. Initial
receiver-registration failures are kept in the log and excluded from final
success totals.

```bash
source scripts/env.sh
cmake --build "$DEPS/build-comments-20261004" --target SevenZipMac panel_open_tests editor_tests panel_selection_tests open_mode_tests
ctest --test-dir "$DEPS/build-comments-20261004" -R '^(panel_open|panel_selection|editor_writeback|archive_open_modes)$' -V
```

The unchanged engine does not require a complete format-matrix rerun for this
UI command batch. This is not the final empty-directory build or complete
portable Windows parity.
