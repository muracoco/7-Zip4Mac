# Official Agent folder updates

The implementation imports official 7-Zip 26.03
`CPP/7zip/UI/Agent/AgentOut.cpp::CAgent::CreateFolder`, rather than recreating
folder creation with a filesystem Add command. The pinned importer retains
the update-pair construction and original callback operation. Existing archive
items use `SetAs_NoChangeArcItem`; a single new directory has new properties,
new data, size zero and the full logical folder prefix.

The adapters replace the Windows UI callback with the existing official console
callback, accept the panel's logical proxy prefix, use POSIX `SetAsDir`, and
convert the original UTC `FILETIME` to the portable `CDirItem` time type.
All three new-item times are equal and retain Windows 100 ns precision in the
update callback. Serialization follows each official handler's defaults: for
example, WIM writes MTime by default and does not automatically enable optional
CTime/ATime. Existing items' properties still come directly from the handler.
No archive serialization, codec, encryption or update-pair algorithm is replaced.

## Process boundary and transaction

The existing bundled `7zz-progress` opens the archive through `CArchiveLink`,
then dispatches a private folder-update operation to the opened `IOutArchive`.
The logical name and a fresh output path use dedicated nonsecret environment
fields, cleared on unrelated operations. Names are validated at both boundaries.
The output uses Create_NEW. Missing adapters fail explicitly. Passwords remain
on redirected stdin and are copied from the official open callback only when
opening actually required one.

The Qt transaction still copies and hashes the original asynchronously, rejects
unsafe/colliding paths and file/link parents, checks the resulting directory and
unchanged user-item properties, then rechecks the original identity/hash before
installation. It retains ownership, ACL, extended attributes and POSIX mode.
CreateFolder no longer tests unrelated old payloads: the original Windows
operation also keeps NoChange packed streams, without a whole-archive Test.
Data-only and mixed-data-password archives can therefore add a folder without
an unrelated password prompt. Header encryption still requires its password.
Whole-archive Test/Extract of multiple differently encrypted items still has the
existing single-password process-model limitation; this change does not claim
that broader gap resolved.

Folders preferences now apply to CreateFolder, replacement and ZIP Comment as
well as Add. Working storage can be on another volume. A cancellable final copy
into an owned directory beside the original precedes atomic replacement when
the volumes differ. Failed or cancelled precommit work retains the original and
removes owned staging directories. This is not a global lock against external
processes racing the last identity check and rename.

## WIM and source links

The blanket multi-image WIM restriction is removed. Image-number prefixes such
as `2/dest/` reach the official handler; unsupported/solid/split WIM variants
remain governed by its read-only/update checks. Generated WIM XML is identified
using the official raw-property parent interface, not a filename guess. It can
change with image metadata without being mistaken for changed user payload.
Native metadata also retains auxiliary flags and parent index/type for later
tree/alternate-stream navigation work.

Filesystem-to-archive CopyFrom follows source links as the official enumerator
does. Move verification now walks followed directory links, verifies descendant
payloads and rechecks source-link identities. Only selected roots reach macOS
Trash; linked targets and nested links outside those roots are retained.
Ordinary Add's explicit stored-link option remains a separate operation.

Leading prefixes now use the imported official Agent stream boundary, with
prefix-byte verification before replacement; see [archive-prefix-updates.md](archive-prefix-updates.md).
Windows Agent itself refuses tails and multiple open layers. Full link/metadata
restoration, arbitrary external-editor hand-off and physical Finder/menu/Fn
checks remain open in the full parity inventory.

## Verification

Final scoped results and commands are recorded in
[test-results.md](test-results.md) and [the execution log](native-folder-update-test.log).
The 18 integration checks and six affected suites total 154 passing checks,
including setup/cleanup. Reproduce them after building with
`./scripts/test.sh --no-focus`, or run the affected suites alone:

```bash
ctest --test-dir <build> -V -R '^(archive_transfer|native_progress|properties|editor_writeback|file_comments|native_metadata_formatter)$'
```

The isolated integration functions are `archiveFolderCreation`,
`archiveFolderCancelAndProtection`, `archiveFolderNativeWimImagesAndTimes`,
`archiveFolderMixedDataPasswords`, `archiveFolderWorkingDirectory`,
`archiveFolderLargeListing`, `archiveFolderDiagnosticsAndLimits` and
`guiArchiveFolderCreation`. The test script creates the large TAR fixture.
Setting `PORT_ARCHIVE_WORK_ROOT` to an existing writable directory on another
volume additionally asserts and tests real cross-volume working storage.
This is a functional checkpoint; final clean/full-format verification and
publication still follow completion of the remaining portable features.
