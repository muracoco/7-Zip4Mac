# Official File menu and drag policy

The context menu now follows the ordinary Windows File menu: Open variants,
View/Edit, Rename/Copy/Move/Delete, Split/Combine, Properties/Comment/CRC/Diff,
Create Folder/File, Link and Alternate streams. CRC contains all eleven original
methods. The filesystem 7-Zip shell submenu precedes those items; archives omit
the shell submenu. Duplicate creation commands and archive-internal toolbar
Add/Extract/Test extras were removed. Archive Test remains on its original
toolbar; filesystem shell Test remains in the configured 7-Zip submenu.

`scripts/import-panel-menu-drag.py` pins official 7-Zip 26.03
`MyLoadMenu.cpp`, `PanelDrag.cpp` and `resource.h` by SHA-256. It retains the
ordinary `CFileMenu::Load` loop, `NDragMenu` command/label table,
`GetEffect_ForKeys` and `CDropTarget::GetEffect` bodies. Qt adapters supply menu
items, state, modifiers and volume identities. The retained loop filters Exit
and unconfigured Diff, disables read-only/hash operations, checks one-file
Split/Combine and Link/alternate-stream eligibility, and omits disabled items
on a small screen. Context actions/submenus are owned copies forwarding to the
real commands, so context-specific disabling does not modify toolbar actions.

The optional trailing `_7vc` version-store block and `VerCtrl.cpp` commands
have now been imported in the subsequent [version/menu/time batch](version-menu-time-port.md).
The complete File-menu filter is retained, with its source-action adapter copied
into the destination adapter. The original labels, order and registry/Diff gates
supply both program and context menus.

## Drag behavior

- Both Details and icon/list views use one controller. Qt's
  `QListView::setMovement(Static)` disables drag/drop; the port now reenables file
  transfer while keeping fixed icon positions.
- Same-panel drops are rejected. Filesystem folder hit-testing uses column zero
  even when the cursor is over another column in a folder row.
- Internal filesystem drops use Copy/Move and the existing asynchronous worker,
  including target subfolders and source/destination refresh. Original key
  effects are retained: Ctrl Copy, Shift Move, same-volume default Move,
  different-volume default Copy; Alt and Ctrl+Shift link effects are rejected.
  macOS volume comparison uses inode device identities, including symlink nodes.
- External ordinary-file drops create an archive with AddDialog, including a
  hovered filesystem target folder. The previously requested Mac shortcut that
  opens a single dropped archive is preserved as an intentional port feature.
- Archive targets use original-style Copy confirmation and CopyFrom. Ordinary
  drops force Copy; right-button menus use the original table, whose archive
  Move command is commented out. No/Cancel retains the archive and sources.
- Right-button drag initiation is implemented from viewport mouse movement;
  internal MIME retains that button mode across the native bridge. The menu
  offers the applicable original Copy/Move/Add/Cancel choices. Unsupported
  archive-source Move is disabled, consistent with the original Agent's lack
  of that operation.
- Archive drag-out extracts asynchronously before starting a native file-URL
  drag. The Progress dialog closes and controls are restored before starting
  it. An ignored drag removes its temporary output. Accepted output survives
  Manager destruction because a Finder consumer can read after drag returns.
  Default temp roots appear in Delete Temporary Files; custom working-folder
  roots can be cleaned explicitly using the File Manager/Finder. This retained
  output lifetime is a macOS substitution. Active roots remain protected from
  temporary cleanup while the Manager owns them.

The internal origin is a typed QObject with a guarded source-window pointer,
not a pointer decoded from external MIME text. An external file-URL drag cannot
impersonate an internal panel merely by setting the format name. The controller
is destroyed before member-worker teardown, preventing late refresh callbacks
from accessing partially destroyed window state.

## Executed checks

**116 passed, 0 failed, 0 skipped**, including setup/cleanup: new menu/drag 34,
panel Open 27, panel selection 16, archive transfer 30, native stream context 3,
selected-row toolbar Test 3 and shell context round trips 3.
[Execution evidence](panel-menu-drag-test.log).

Checks cover actual Qt clicks in CRC and drag menus; right-button movement;
Details/icon Copy/Move; wide-row folder drops; same-panel refusal; default Move
of only a symlink while retaining its target; archive confirmation cancellation;
7z/ZIP and header-encrypted 7z drag-out; 32 MiB payload SHA-256, event-loop
heartbeat, cancelled-temp cleanup and accepted files after Manager destruction.
The existing shell-menu test creates 7z/ZIP, tests and extracts them with
Japanese/spaced names and independent SHA-256 comparison. It was run alone
with a minimal manifest; this is **not** a new all-format run.

Initial icon and wide-row failures exposed the two corrected production bugs.
The native-index Test fixture was corrected to click the real toolbar because
the upstream archive context menu does not contain Test. Historical logs remain
historical; these final totals exclude failed/development repetitions.

```bash
source scripts/env.sh
cmake --build "$DEPS/build-comments-20261004" --target SevenZipMac panel_menu_drag_tests archive_transfer_tests panel_selection_tests panel_open_tests
ctest --test-dir "$DEPS/build-comments-20261004" -R '^(panel_menu_drag|archive_transfer|panel_selection|panel_open)$' -V
./scripts/test-agent-tree.sh "$DEPS/build-comments-20261004" agent_tree_tests nativeAlternateStreamBinding
./scripts/test-agent-tree.sh "$DEPS/build-comments-20261004" agent_selection_tests guiToolbarTestUsesSelectedRow
```

The desktop is still locked (`CGSSessionScreenIsLocked=True`). Tests inject Qt
events and replace native drag execution with an owned receiver feeding another
production viewport. Real Finder drag-out, native right-button initiation and
function-key interaction remain unverified. Link traversal, remaining metadata,
list/dialog/language work and final refactoring/clean-build/publication gates
remain required. This increment does not declare complete Windows parity.

The app was repackaged. All 15 Mach-O files passed system/bundle-relative
dependency checks and deep/strict ad-hoc signature verification. An owned
bundled process survived a three-second startup with isolated preferences and
development Qt/DYLD variables removed; startup stderr was empty.
