# Filesystem to archive Copy / Move

## Official behavior reused

The specification is official 7-Zip 26.03, not an invented destination syntax.
`UI/FileManager/App.cpp::OnCopy` initially uses the other panel's `GetFsPath()`;
it selects `CopyFrom` only when the normalized destination equals that open
panel's address and that panel supports operations. A relative destination is
resolved against the source panel. A path merely containing an archive extension
does not select an archive handler.

`UI/Agent/ArchiveFolderOut.cpp::CopyFrom` uses `k_ActionSet_Add` and
`AgentOut.cpp` resets unspecified properties with `SetProperties(NULL,NULL,0)`.
Same-name entries are replaced even when the source timestamp is older or its
size/time matches. There is no Add dialog or individual overwrite question on
this route. Unrelated archive entries remain. Cached destination passwords are
passed through the existing stdin password channel; no `mhe` or ZIP `mem`
override is added, so the official handlers retain existing encryption state.

`EnumDirItems.cpp::EnumerateItems2` separates physical and logical parents.
An explicitly selected `sub/file` is stored as `<archive-folder>/file`; a selected
folder retains its basename and descendant hierarchy. This includes Flat view
selections and inputs from different filesystem parents. Same logical names in
one input batch are an error, not successive updates where the last file wins.

## Adapter and GUI

`ArchiveRequest::useArchiveDefaults` and `archivePrefix` separate File Manager
CopyFrom from AddDialog properties. Explicit absolute source arguments preserve
physical paths while the original portable `EnumerateItems()` adds a logical
prefix. The seven-file hash-checked console overlay adds this argument and makes
the original archive-item censor match Agent's CopyFrom behavior. The original
engine, archive handlers, update pairing and compression algorithms are reused.
No upstream option structure is enlarged, avoiding ABI changes to existing
engine objects. Original upstream files and headers are retained.

The adapter receives a validated UTF-8 relative folder through dedicated process
environment values. They contain no password, are cleared for all other commands,
and are validated again by the adapter. It refuses traversal, absolute/control
paths and unsupported path modes. A missing adapter produces an explicit error
instead of silently adding at the archive root. Telemetry remains optional.

The Copy / Move dialog defaults to the other open archive folder. Both operations
run through that destination panel's backend. Archive folder drops use the same
CopyFrom path without AddDialog, including multiple filesystem parents. Normal
filesystem Add no longer rejects different parent directories. Destination
prefixes are preserved across re-listing. Both panel lists refresh after Move,
including with Auto Refresh disabled. A committed Add followed by failed source
verification also re-lists the archive; cancelled filesystem compression keeps
its previous immediate idle behavior.

## Move and source verification

Windows deletes processed source files and then empty requested directories.
The authorized macOS substitute moves verified source roots to Trash. The full
archive is tested, and original source bytes are compared using stored CRC or
temporary extraction plus SHA-256 before any source root is moved. Logical
prefixes and input basenames are mapped back to the actual physical sources.
An absent BZIP2 unpacked Size is treated as unknown, and its extracted bytes are
compared instead of interpreting an empty property as a zero-byte file.

CopyFrom follows filesystem links as the original enumerator does. Moving a file
symlink verifies the followed file payload and moves only the source link to
Trash; its target remains. Directory-link Move now follows and verifies descendant payloads, rechecks
source-link identities and moves only selected roots to Trash. Targets and
nested links outside those roots remain. Four writable-format cases and the
two-panel GUI route pass; see [native-folder-update.md](native-folder-update.md). Ordinary Add's
explicit symbolic-link storage option retains its separate behavior.

GZIP/BZIP2/XZ allow one logical item. Copy / Move can replace that existing name.
Adding a different name would retain the old item and add a second item, which
the official `GzHandler.cpp`, `Bz2Handler.cpp` and `XzHandler.cpp::UpdateItems`
reject with `E_INVALIDARG`. The original archive and sources remain in the tested
cases; there is no invented automatic rename/replacement of a different item.

Warnings/errors do not start source deletion. Cancel/Pause reuse the asynchronous
official child and cooperative verification worker. Source deletion can fail
after archive commit, or Cancel can occur after some source roots reached Trash;
the result reports that state. This route uses the official archive update
workflow, not the separate guarded ReplaceFile transaction. Arbitrary concurrent
external changes and every ACL/metadata combination are not claimed verified.

## Remaining scope

Multi-image WIM updates now use image-number prefixes. Single-layer leading
offsets use the existing official console update boundary; see
[archive-prefix-updates.md](archive-prefix-updates.md). Tails, multiple layers,
multivolume parents and unsupported writable layouts remain guarded. Windows
Agent itself refuses tails and multiple layers.
Full archive metadata, safe archive-link restoration, physical Finder drag-out
and native desktop/Fn checks remain incomplete. These limitations remain in the
full-parity inventory; this batch is not the final release.

## Previous transfer checkpoint (2026-10-04)

The newer native-folder batch has 30 passing transfer checks, including directory
links and an injected real source-change verification failure. The following
25-check results describe the earlier implementation. Latest evidence is in
[native-folder-update.md](native-folder-update.md).

macOS 26.6.2, arm64 Apple M3, Qt 6.11.3 and source-built 7-Zip 26.03.
Incremental Release build passes without warnings. Final transfer suite: **25
passed / 0 failed / 0 skipped**. Affected native progress/verification checks:
**13 passed**. Final selected integration regressions: **19 passed**. The **57**
total includes setup/cleanup, not 57 distinct user commands.

Cases cover 7z/ZIP/TAR/single-image WIM internal-folder updates, older same-name
sources, handler defaults independent of saved invalid Add settings, multiple
physical parents, selected-folder hierarchy and empty directories, Unicode and
spaces, existing 7z header encryption/ZIP AES inheritance, wrong-password Test,
verified Trash and restored payload comparisons. GZIP/BZIP2/XZ same-name Copy
and Move pass; a second logical name is rejected and original bytes/sources
remain. File-link Move keeps its target, while directory-link Move retains its
source and refreshes the already committed archive. Duplicate logical inputs,
invalid prefixes and Pause/Cancel of an existing archive with a 32 MiB random
source preserve original bytes. Native progress checks additionally compare
32 MiB round trips with SHA-256.

Qt hit-testing invokes right-click Copy and the actual Copy destination dialog;
Move and real Qt drop events use the same production routes. Archive and source
lists refresh with Auto Refresh disabled. This is automated Qt verification,
not proof of a physical Finder/native-menu/Fn click during the locked session.

Initial test expectations incorrectly assumed ZIP always compresses very small
files and rejects an Add solely because an old data password is wrong; upstream
can Store small files and copy existing encrypted streams without decoding.
The tests were corrected against actual handler behavior. A responder timer was
also corrected so its menu click could open a nested dialog without blocking
that same timer. The first stream tests incorrectly attempted a second logical
name. The revised BZIP2 Move test exposed the unknown-size verification issue
and the selected cancel regression exposed an unnecessary filesystem refresh;
both implementation issues were fixed before the final results above.

```bash
ctest --test-dir <build> -V -R '^(archive_transfer|native_progress)$'
<build>/port_tests <source-built-7zz> roundtrip errorsAndRecovery \
  archivePauseResumeAndCancel trashAfterVerifiedCompression \
  verifiedTrashOtherFormatsAndUpdateModes additionalFormatsAndLinks \
  archiveFolderCreation archiveRefreshPreservesFolder
```

`./scripts/test.sh --no-focus` also includes the new suite.
[Actual logs](archive-transfer-test.log). Final clean build/full regression and
all-format rerun remain scheduled after the remaining functional batches.

The packaged app passes dependency/signature verification for **15 Mach-O**
files, using system/bundle runtime paths. The normal output app was updated.
An owned LaunchServices instance **PID 8396**, with an isolated
CFFIXED_USER_HOME profile and an owned fixture directory, survived three seconds
and loaded the bundled Cocoa plugin. Main code sections and all inspected helper /
framework/plugin bytes match the working bundle. Only that owned instance was
terminated. Physical rendering was not verified.

## File-menu / drag policy integration

Archive targets now use the original Copy confirmation and right-menu table.
Internal FS drops use the shared asynchronous Copy/Move worker; same-panel
drops are rejected, folder targeting works across columns and all views, and
source/destination lists refresh. Selected archive-file URL drags are deferred
until extraction finishes and controls are restored. [Scope and 116 affected
checks](panel-menu-drag-port.md). Real Finder interaction remains pending.
