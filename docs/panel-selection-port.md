# Official File Manager selection port

The alternative-selection setting now uses the original 7-Zip 26.03
File Manager selection bodies, rather than Qt MultiSelection as an approximation.
The archive engine remains the bundled official engine.

## Source and adapter

`scripts/import-panel-selection.py` verifies these official source hashes before
generating `src/upstream/PanelSelection.inc`:

| Source | SHA-256 |
|---|---|
| `CPP/7zip/UI/FileManager/PanelSelect.cpp` | `8c64e1607e84be906cd73755bcfeeb4b2ec20ae18e38ef1646811b6b19b0053e` |
| `CPP/7zip/UI/FileManager/PanelItems.cpp` | `2dc5869e794788b366b1dc7fb6485b362e7b72352c1549303b944693765e2f73` |

The retained bodies are OnShiftSelectMessage, OnArrowWithShift, OnInsert,
UpdateSelection, SelectAll, InvertSelection, KillSelection, OnLeftClick,
Get_ItemIndices_Selected and Get_ItemIndices_Operated. Original copyright and
LGPL notices are retained. Qt supplies the list, event and vector boundary;
Win32 controls are not copied into the macOS build.

## Behavior

- Alternative mode has one native selected/focused row and independent pink
  operation marks, matching the original LVS_SINGLESEL plus status vector.
  The original marked background is RGB(255, 192, 192).
- Plain click changes focus without clearing marks. Ctrl-click toggles a mark;
  Shift-click selects the anchored range and clears marks outside it.
- Insert toggles the focused item and advances. Held Shift plus an arrow applies
  the original initial mark/unmark decision across subsequent moves. The parent
  row is excluded. No new Space shortcut is introduced.
- Operations use marks when present, otherwise the native selected focused row.
  Ctrl+C uses only marked names, in original item order, separated by CRLF. It
  does not copy an unmarked focused row.
- Details and icon views share one controller and selection model. Two panels
  maintain independent marks. Sort preserves original item identities. Refresh,
  Options Apply, comments and supported archive mutations restore marks and
  native focus separately.
- Ordinary selection continues to use Qt ExtendedSelection.

## Executed checks

Final affected results: **222 passed, 0 failed, 0 skipped**, including Qt
setup/cleanup: panel selection 16, archive transfer 30, editor write-back 32,
folder statistics 11, directory scanning 13, comments 31, open modes 24,
Properties 19 and compression/Help 46.

The tests dispatch Qt mouse/key events in both views and exercise actual Options
Apply, two panels, refresh and ZIP comments. The real Add/List/Extract path
creates both 7z and ZIP from marked ASCII/Japanese files, excludes the different
unmarked focused file, and compares extracted bytes. The initial clipboard
behavior was corrected against the original source. A subsequent two-panel
test incorrectly found the nested pane twice; its lookup was corrected and the
selection suite was rerun. The eight unaffected passing suites were retained.

Build and test commands for the existing local build:

```bash
source scripts/env.sh
cmake --build "$DEPS/build-comments-20261004" --target SevenZipMac panel_selection_tests
ctest --test-dir "$DEPS/build-comments-20261004" -R '^panel_selection$' --output-on-failure
```

See [execution evidence](panel-selection-test.log). Physical mouse/Fn/Finder
interaction remains pending while the desktop is locked. Qt event tests and
an isolated application startup do not establish that physical verification.
The complete format matrix and final empty-directory release build were not
repeated for this selection-only batch. Full portable parity and publication
remain unfinished.
