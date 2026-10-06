# F3 folder statistics

Official 7-Zip 26.03 `PanelItems.cpp::EditItem(false)` first calls
`IFolderCalcItemFullSize` for operated filesystem folders. If any folders were
calculated, it redraws the list and does not launch the focused file's viewer.
`FSFolder.cpp::CalcItemFullSize` / `CFsFolderStat::Enumerate` provide the total
Size and recursive Folders / Files fields. The selected root is not counted
as a child folder. The interface is provided by FSFolder, not ArchiveFolder.

This port follows that selection rule. Selected folders take precedence over a
focused regular file, even in a mixed selection. With no selection, the focused
folder is used. F4 continues to operate on the focused regular file. Filesystem
folders initially have undefined statistics; F3 fills Size / Folders / Files
without resetting selection, current item, columns or sorting. These columns
sort numerically. Folders / Files remain hidden by default and can be shown
through the existing header menu. Refresh reloads the folder and clears the
calculated values, as in the upstream filesystem folder object.

The scan runs in a worker with Pause / Continue / Cancel and an indeterminate
Progress dialog. The total is unknown until traversal completes; discovered
bytes are displayed without a guessed total or remaining time. Both panels
share the busy state. Closing while scanning cancels the job and keeps the
Manager available after completion. Destruction wakes a paused worker and waits
for it; it never deletes or changes filesystem items.

macOS traversal uses directory descriptors, `fstatat(...AT_SYMLINK_NOFOLLOW)`
and `openat(...O_DIRECTORY|O_NOFOLLOW)`. A replaced child directory is rejected
when its identity no longer matches. Symbolic links, including loops, dangling
links and links to directories, count as files using the link's own length;
their targets are not traversed. A symbolic-link root is rejected. Hard links
count once per directory entry. Hidden files and empty folders are included,
sparse files use logical size, and special nodes are counted without opening
their content. These link rules deliberately differ from Windows reparse-point
traversal. Permission/enumeration errors include the affected path and errno;
incomplete or cancelled totals are not installed. Previously completed sibling
roots may still display their valid totals. Excessive nesting/file-descriptor
limits produce an error rather than risking stack exhaustion.

The `folder_statistics` suite uses owned temporary trees and isolated settings.
It covers Unicode/spaces, hidden files, empty folders, hard/symbolic links,
FIFO, a 32 MiB sparse file, missing/non-directory/unreadable roots, completed
siblings, 30,000 entries, Pause / Continue / Cancel / reuse, Details / Icons /
Flat selection behavior, numeric sorting, GUI error recovery and two-panel
busy/close behavior. No native activation or user-folder fixtures are required.
Actual executed results are recorded in [test-results.md](test-results.md).

This worker covers the explicit F3 operation. Filesystem / Flat View loading
has its own asynchronous worker and GUI batching; see
[directory-scanning.md](directory-scanning.md) for its remaining synchronous
model/icon work.
Physical F3/Fn behavior remains an unlocked-desktop check.
