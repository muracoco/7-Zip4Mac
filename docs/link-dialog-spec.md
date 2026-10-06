# Link dialog source comparison

Baseline: official 7-Zip 26.03 `UI/FileManager/LinkDialog.cpp`, `.rc` and
`CApp::Link()` in the same implementation file. The port now creates hard links
and file/directory symbolic links, edits existing POSIX symlink targets, and
provides both folder browse controls. Native desktop clicks remain unverified
during the locked session; Qt mouse-event and filesystem tests are separate.

## Upstream behavior

- New regular item: From is the other filesystem panel's folder, or the item's
  parent folder in one panel; To is the selected item's path. Regular files
  default to Hard Link; directories to Directory Symbolic Link when supported.
- An existing reparse link initializes From to the link itself, To to its raw
  stored target, and a separate current-target label. Its type is selected.
  A relative target is not converted to an absolute path.
- Both browse buttons call `MyBrowseForFolder`, regardless of link type.
  They select folders, normalize a trailing separator and do not append a
  source filename. File choosers would be a behavior change.
- From starts with a normalized folder prefix, including its trailing
  separator. Relative From and hard-link To are resolved against the panel's
  filesystem root, even when the selected Flat-view item is in a subfolder.
- `CApp::Link()` calls `Get_ItemIndices_Operated`: one selected item is
  required. A completely unselected focused item is not operated on.
- Link operates on one filesystem item. Hard Link creates a new link to a
  file. Symbolic types check file/directory mismatch. Nonempty non-reparse
  files are protected from hiding their data; existing reparse data can be
  updated. Empty To requests `DeleteReparseData`.
- Junction and WSL reparse types, modifying an empty ordinary Windows file
  into a reparse point, and deleting its reparse data are Windows mechanisms.
  The dialog does not create `.lnk` shortcuts or Finder aliases.

## Implemented portable behavior

`src/FileToolDialogs.cpp::LinkDialog` uses editable From/To combos, two
folder-only browse buttons, the raw current-target label and the original
three portable link types. `MainWindowTools.cpp::link` uses the actual item's
parent for a one-panel Flat selection, and the other filesystem panel when
present. Relative input resolution still uses the initiating panel root.
Selection and focus survive the asynchronous refresh after a successful edit.
Link is disabled without exactly one selected filesystem item. The earlier
focused-unselected fallback was removed after checking the actual
`PanelItems.cpp` 984–1001 method body; that behavior belonged to other commands,
not Link.

POSIX uses the same symlink representation for files and directories.
`lstat` and `readlink` are needed for raw relative, dangling and looping targets;
Qt's resolved `symLinkTarget()` is unsuitable for preserving the raw target.
The kind of a dangling or looping target cannot be inferred from POSIX symlink
bytes: File Symbolic Link is the initial choice, and the user can choose
Directory Symbolic Link. The raw target label is plain text and is excluded
from UI translation, including targets named `Cancel` or containing HTML.

## Guarded editing and limits

`src/FileLinks.cpp` captures the parent/link identities, metadata and raw
target, prepares a new symlink inside an owned same-volume directory, and
uses Darwin `renameatx_np(RENAME_SWAP)` for installation. `O_SYMLINK` file
descriptors and `fcopyfile(COPYFILE_METADATA)` copy metadata of the link itself,
without following either target. Parent/source changes detected before or
after exchange refuse the edit; a guarded second exchange restores the
displaced entry when possible. If restoration cannot safely finish, the
recovery directory and its actual path are retained and reported.

Cleanup is anchored to open directory descriptors, checks owned inode
identities and removes only known symlinks/empty staging directories. It
never recursively removes a staging pathname. Renamed staging directories
or unknown contents can remain for inspection. This guards tested concurrent
changes; it is not a compare-and-swap guarantee against an adversarial process
with the same user permissions, nor a power-loss durability guarantee.
Hard-linked symlinks and immutable/append-only links are refused. An existing
symlink cannot be converted to a hard link in place; a different From path
creates a new link.

Owner, group, mode, nanosecond mtime and a symlink-specific xattr have passing
fixtures. ACL copying uses the metadata API but has no independent ACL
fixture yet. Invalid UTF-8 raw targets remain untouched rather than being
silently replaced through a Unicode text field.

Keep ordinary files/directories protected rather than copying Windows
reparse mutation literally. Empty targets remain rejected until a concrete
POSIX equivalent of removing reparse data is defined and tested.

## Verification

`tests/file-links.cpp` has 28 passing QtTest checks including relative and
absolute target bytes, Japanese/space names, dangling/self/mutual loops,
type mismatch, target-content preservation, metadata, external replacement,
hard-linked symlinks, invalid bytes, Cancel, both folder browse controls,
unselected-focus disabling, selected-item operation, Flat defaults and two-panel routing. Seven
deterministic fault cases exercise parent changes before/after exchange,
installation failure, successful rollback, rollback permission failure,
a later foreign entry and a replaced staging name. Recovery files and
foreign data are checked rather than treating every failure as cleanup-only.
Build, regression and bundle evidence is recorded in [test-results.md](test-results.md).
