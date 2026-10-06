# Nested archive browsing, write-back and Pause

This increment implements portable browsing and progress behavior from 7-Zip
26.03 `FileManager/PanelItemOpen.cpp`, `CFolderLink` / parent-folder handling,
`kStartExtensions` and `ProgressDialog2.cpp`. It does not complete all Windows
File Manager parity.

## Nested archives

- Open or double-click an archive inside another archive to browse it in the
  same panel. Open Inside also tries files with document/unknown extensions.
  The upstream external-open extension list is preserved for nested items.
- Each level is safely extracted into an independent owned temporary directory
  through the existing backend, then listed asynchronously. Unsafe archive
  paths/links remain refused. No source archive is modified during browsing.
- The address and Favorites store virtual paths such as
  `outer.7z/folder/middle.zip/inner.7z/data/`. Temporary physical paths remain
  internal. Address/Favorites navigation resolves successive nested levels.
- Up restores the containing archive folder, selects the child archive and
  releases its temporary directory. Refresh preserves the virtual folder.
- Parent and child passwords are independent. Missing/wrong passwords can be
  retried; declining a prompt, cancelling or failing preserves the old panel.
  The Extract → List transition retains an active Cancel button.
- View / Edit / Open Outside use independent extraction directories. Regular
  files have completion tracking and confirmed write-back; running editors keep
  their files even after Manager exit. Returning to a parent cannot remove them
  through deletion of a temporary ancestor. See [external editing](external-editing.md).

## Confirmed parent write-back

Writable nested archives support Rename, Delete, Create Folder and root-level
Add through the existing actions. Every containing archive must also be writable
and have a supported update layout; a read-only outer archive disables mutations
in the child. Changes stay in the extracted child until its level is closed.

The specification is official 26.03 `PanelItemOpen.cpp:598–625`,
`PanelFolderChange.cpp::CloseOneLevel/BindToPath`, `Panel.h::CFolderLink` and
`AgentOut.cpp::UpdateOneFile`. On Up from the archive root, address/Favorites
navigation away, panel removal or window close, size/mtime changes trigger:
`File '{0}' was modified.\nDo you want to update it in the archive?` with
Yes / No / Cancel and Yes as default. The original translated resource is used.
**Both No and Cancel discard the child and exit that level**, as the Windows
implementation does. Yes writes one child back to one parent. If that parent is
nested, its own confirmation occurs when it is subsequently closed. Navigation
reuses a common open ancestor rather than re-extracting and losing pending changes.

`ArchiveOperation::ReplaceFile` stages the parent on its original volume and
copies the edited child to its exact existing archive path. Official
`7zz u -up1q1r0x2y2z2w2` replaces that item regardless of timestamps and retains
unrelated entries. Listings verify the item count and untouched entry properties,
whole-archive Test checks integrity, and selected-entry SHA-256 must equal the
staged child's hash. The original parent's stat/SHA-256 and the child's stat/hash
are rechecked before atomic replacement. ACL, ownership, POSIX mode and extended
attributes use the same transaction as Create Folder. Pause and Cancel apply to
copying, verification and engine processes. The final check/rename interval is
not a global lock against unrelated processes.

7z, ZIP, TAR, WIM including multiple images, and single-file gzip/bzip2/XZ replacements are
supported. A compressed TAR is explicitly browsed as stream → TAR, matching these
26.03 handlers' lack of `kpidMainSubfile`; it is not blanket-classified as an
unwritable implicit chain. Parent and child passwords remain independent and are
passed via stdin. Single-layer leading prefixes now reuse the official Agent/
console stream boundary and are verified before parent replacement; see
[archive-prefix-updates.md](archive-prefix-updates.md). Split/ambiguous layouts,
tails, unsafe/colliding paths and linked targets remain refused. Windows Agent
also refuses tails and multiple open layers. Whole-archive verification can need an additional data
password, and mixed-password archives remain outside the single-password model.

Failure or cancellation keeps the original parent and retains the modified child
on disk with automatic temporary cleanup disabled. The warning includes its
recovery path, operation target, exit code and error details. The panel exits the
failed level as upstream does; the recoverable copy survives normal window/app
closure. No/Cancel's intentional discard remains distinct from a cancelled update.

External-editor completion/write-back is implemented, including nested panels,
with documented arbitrary-launcher limitations. Open Inside `*` / `#` parser modes
and adding into an archive's internal subfolder are now implemented; see
[archive-open-modes.md](archive-open-modes.md) and [archive-transfer.md](archive-transfer.md). The
process backend stages whole child files instead of the Windows Agent's seekable
in-memory substreams.

## Pause / Continue

Progress now offers Pause / Continue during running archive processes and
cooperative filesystem jobs. A paused GUI remains responsive and Cancel wakes
the worker or resumes the suspended process before terminating it.

- Official `7zz` processes use macOS `SIGSTOP` / `SIGCONT`; compression algorithms
  remain upstream code. Pause cannot suspend the GUI process.
- Copy, Move, Trash, Split, Combine and extraction installation use a shared
  condition-variable control at safe I/O/item boundaries. Stop always wakes it.
  Copy/move check again before an atomic move or source removal.
- Elapsed time, speed and remaining estimates exclude the paused interval, as
  the upstream progress dialog does. Completion disables Pause and restores Close.

Pause now also controls the extra source-verification worker used before
`deleteAfter` moves sources to Trash. The worker owns/pauses/reaps its child and
drains native progress; local checks report source bytes/counts. See
[transfer scope and tests](filesystem-transfer.md). A blocking OS I/O call must
return before a cooperative disk worker can pause. Official callback packed-size/
ratio reporting is integrated; see [native progress](native-progress-help.md).

## Verification

Tests use disposable files on the current arm64 Mac. They cover a three-level
7z → ZIP → 7z chain, Japanese/spaced names, address/Favorites reopening, refresh,
parent selection, SHA-256 extraction equality and unchanged outer archives.
Source-folder temporary settings exercise external-file lifetime explicitly.
Encrypted outer/inner headers, a wrong child password, prompt refusal, corrupt
children and GUI Cancel between extraction/listing are covered.

Qt mouse events drive actual MainWindow progress buttons for Pause, Continue and
Cancel while paused. A heartbeat verifies GUI responsiveness, resumed output
matches the source and cancelled new output is removed. Filesystem tests pause
Split / Combine / Copy / Move at worker boundaries, resume or cancel, then compare
outputs and retained sources. Physical AppKit/Finder clicks and native focus tests
remain pending while the desktop is locked.

Additional write-back regressions cover 7z → ZIP → 7z with Up, address navigation
and window close, distinct encrypted passwords, Yes/No/Cancel behavior, read-only
outer archives, cancellation and persistent recovery, all seven writable parent
formats, same-size older-timestamp replacements, late editor changes and FIFO
refusal. gzip/bzip2/XZ → TAR changes are extracted independently and hashed after
writing back. Native menu clicks and Finder interaction remain pending.

The read-only review identified a refresh race during the update question.
The whole exit, including the modal decision, is now busy; pending debounce
cannot start a competing List, and completion checks operation and target.
Tests expire the real debounce inside the question. Synchronous exits restore
action state, and close is deferred outside the original close event. No/Cancel,
unmodified close, common-ancestor navigation and panel removal are regressed.
See [clean results](test-results.md) and [raw log](distribution.md).
