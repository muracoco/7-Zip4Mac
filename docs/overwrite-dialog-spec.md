# Individual overwrite source comparison

Implemented portable behavior and source comparison. Filesystem Copy/Move
and archive extraction now share the six-choice dialog below. Automated
Qt mouse events exercise the dialog; physical desktop clicks remain pending.

Baseline: official 7-Zip 26.03 `UI/FileManager/OverwriteDialog.cpp/.rc`,
`ExtractCallback.cpp`, `FSFolderCopy.cpp`, `PanelCopy.cpp`,
`UI/Common/ArchiveExtractCallback.cpp` and `Common/FilePathAutoRename.cpp`.

## Dialog and choices

The old destination appears first, followed by the incoming file, each with
path, size, modification time and icons. Undefined size/time stays blank.
The upper button row is Yes / Yes to All / Auto Rename; the lower row is
No / No to All / Cancel. Normal overwrite callers leave
`DefaultButton_is_NO=false`: Yes is the first focusable button, with no
explicit DEFPUSHBUTTON in the resource. Version Control Revert is a separate
No-default caller that hides the additional choices. This focus inference
has not been checked on physical Windows.

| Choice | Current collision | Following collisions in this operation |
|---|---|---|
| Yes | Replace | Ask |
| No | Skip | Ask |
| Yes to All | Replace | Overwrite |
| No to All | Skip | Skip |
| Auto Rename | Rename the incoming file | Rename incoming files |
| Cancel / close / Escape | Abort | Abort |

Auto rename existing is an extraction setting, not a button in this dialog.
Each independent filesystem Copy/Move starts in Ask mode (`PanelCopy.cpp` 304).
`ExtractCallback.cpp::AskOverwrite` 201–231 supplies the common GUI to archive
extraction and filesystem transfer.

## Paths and transfer differences

`FilePathAutoRename.cpp::AutoRenamePath` separates the last extension:
`a.tar.gz` becomes `a.tar_1.gz`, and `.profile` becomes `.profile_1`.
The upstream binary search need not choose the smallest free suffix when
occupied suffixes are sparse. The shared helper now follows this rule, correcting the former
completeBaseName / completeSuffix duplication.

Archive extraction asks before opening the output stream
(`ArchiveExtractCallback.cpp::CheckExistFile` 1246 onwards). Existing directories
are merged without asking for directory entries. A file targeting a directory
can be offered for replacement: Yes only removes an empty directory; a nonempty
directory produces an error/skip. Rename Existing can move the directory aside.

Filesystem leaf files call `ExtractCallback.cpp::AskWrite` 710 onwards. No
does not delete the source. Existing directories merge recursively; file and
directory types are not mutually replaced. Move attempts directory rename
first, then falls back to recursive file moves. Skipped sources remain and
final `RemoveDir` can report the remaining nonempty directory. Never implement
this fallback with recursive source removal.

Archive-source Move To returns E_NOTIMPL in `Agent/ArchiveFolder.cpp::CopyTo`.
It is not a missing implemented Windows operation. Filesystem CopyTo can use
a single non-folder destination as a new name; this is now supported.

## Implemented installation and limits

The extraction staging is retained. A shared six-choice enum, conflict
metadata and a per-operation answer broker are used. A worker requests one collision
through a queued signal and waits on a condition variable; `QDialog::open()`
answers asynchronously. Cancel/destruction wake the waiting worker,
and operation/prompt generations must reject stale answers. Pause is unavailable
while waiting for a decision, while the operation remains busy.

All/Auto Rename change operation mode; Yes/No only affect one file. File
installation uses a fixed sorted order. A newly appearing destination is a collision and must
not receive an old decision for another file.

The installer captures destination identity/type/size/nanosecond mtime/ctime/mode
and the opened parent directory, and revalidates after the answer and before
installation. Detected changes fail while retaining previous or recovery data.

Existing regular files with no write permission bits are refused before candidate
installation and before rename-first Move replacement. The old destination and
Move source remain intact; Rename Existing can preserve the old file separately.
Dedicated read-only Copy and Move fixtures and read-only split/extraction pass.

Completed candidates are prepared on the destination volume. Darwin RENAME_EXCL
for a new path and guarded RENAME_SWAP for replacement can preserve the
displaced identity, following the tested link-editing transaction pattern.
Retain/report recovery data if identity verification or rollback fails;
never auto-delete an unknown displaced entry.

Filesystem copy uses that installer for leaf files and merges directories.
Move removes only successfully installed, identity-verified source leaves;
skipped leaves remain. Source directory cleanup uses rmdir, not recursive delete.
Existing filesystem symlink copying and archive link restrictions are separate.

## Verification and remaining differences

The overwrite suite passes 14 QtTest cases: mixed No/Yes and all choices for
filesystem/7z/ZIP, dotfiles/multiple extensions, waiting Cancel/destruction,
stale replies, changed destination and rollback, folder merge, skipped Move
sources, raw symbolic links, modification times, extended attributes, all six
buttons and the MainWindow connection. Related 32 MiB round trips, encryption,
unsafe paths, Pause/Cancel and ordinary copy/move also pass.

Extraction asks after the official engine finishes private staging, rather
than before decompression of each output stream. Cancelling installation may
retain completed files; unused engine staging and known candidates are removed.
A rollback that cannot safely complete retains a reported recovery location.
Existing archive-output links remain refused. Existing filesystem directories
merge; filesystem file/directory type collisions are errors. Empty-directory
replacement and Rename Existing use the archive installer paths but their
full Windows combination coverage remains pending.

Copy preserves mode, timestamps, ACL/xattrs via macOS copyfile APIs; timestamps
and xattrs have dedicated fixtures, independent ACL/birth-time/flags coverage
is incomplete. Moves now try same-volume rename before copying, including whole folders and
links. Cross-device and unsupported-primitive filesystems fall back to guarded
copy/removal. [Transfer implementation and SMB limits](filesystem-transfer.md). Metadata on existing merged directories is
retained. Same-user adversarial races and power-loss durability are not fully
eliminated. No recursive deletion of skipped source directories is performed.
