# Archive update boundaries: upstream versus port

Baseline: official 7-Zip 26.03. This inventory distinguishes handler capabilities
from transaction protections around the connected commands.

`CPP/7zip/UI/Agent/Agent.cpp::CAgent::CanUpdate` rejects changed long paths in
its proxy, a device file, multiple opened layers and `ErrorInfo.ThereIsTail`.
It accepts an empty Agent used to create an archive. It has no blanket warning
check and does not reject a positive leading offset. The original operation
bodies are imported in `src/upstream/Agent*.inc`.

| Condition | Owner and current behavior |
|---|---|
| Multiple open layers / tail | Original Agent limitation. This differs from separately opening a nested item into a private editable file. |
| Positive prefix | Supported. Imported Agent stream code and original handlers preserve their separate prefix boundaries; installation verifies prefix bytes. |
| No output interface / handler read-only | Original handler capability. ZIP and 7z expose kpidReadOnly from their own CanUpdate; no RAR writer is introduced. |
| Multi-volume | Refused by the port's single-file update transaction and the original console update path. Do not attribute this to CAgent::CanUpdate's layer check alone. Browsing/extraction remains available. |
| Signed ZipCrypto descriptor | Supported by the documented bounded writer repair; former refusal removed. |
| Unsigned ZIP descriptor metadata update | Original ZipIn::CheckDescriptor E_NOTIMPL. Original Test/extraction can still succeed. |
| Actual handler warnings | Supported when the original handler is writable. Agent and ZIP CanUpdate have no blanket warning refusal. List, menu enable conditions and all native mutation commands now distinguish warnings from actual open errors; the original handler and output interface decide update capability. |
| Open errors / negative offset | Added conservative port refusal before updating. Normal positive-offset notices are distinguished using native CArc flags. |
| Traversal/unsafe names, changed source, failed semantic/payload/prefix verification | Added port protection. Failures/cancellation retain the original before installation; they are not classified as a macOS mechanism or an absent command. |
| Private source copy, original stat/SHA-256 recheck, exclusive/atomic publication, metadata preservation | Added transaction around original Agent/handler operations. Existing unrelated packed data is retained without an unrelated whole-archive Test/password. |
| Ambiguous/unreadable descript.ion | Separate filesystem-comment protection, documented in file-comments-spec.md. It is not a ZIP serialization limitation. |

Source locations for the port guards are `ArchiveMutation.cpp::mutationListing`
and `NativeFolderUpdate.cpp::PortFolderUpdate::Run`. Handler read-only reporting
is in the original `Archive/Zip/ZipHandler.cpp::GetArchiveProperty` and
`Archive/7z/7zHandler.cpp::GetArchiveProperty`.

The added protections remain a documented behavioral difference. Their normal
and failure paths have scoped/consolidated evidence; unknown malformed archive
variants are verification limits, not newly discovered missing commands. See
[current-status.md](current-status.md), [archive-prefix-updates.md](archive-prefix-updates.md),
[zip-metadata-port.md](zip-metadata-port.md) and
[release-consolidation.md](release-consolidation.md).

## Writable warning group (2026-10-06)

Source owners were checked as one group before implementation:
`Agent.cpp::CAgent::CanUpdate`, `ZipIn.h::CInArchive::CanUpdate`,
`ZipIn.cpp::ReadLocalItem` (invalid DOS time), `ZipIn.cpp::ReadExtra`
(truncated optional extra field), and `OpenArchive.h::CArcErrorInfo`.
The port previously rejected `AreThereWarnings()` although these nonfatal
headers leave the handler writable. That restriction has been removed from
the native adapter, mutation listing and GUI enable policy together. Error
flags, read-only handlers, tails/layers/volumes, path checks and verified
publication still protect the original file.

The grouped regression adds both source-defined warning fixtures and covers
Comment, Rename, Delete, CreateFolder, ReplaceFile and Add, official-handler
rename, menu enable state, collision refusal, independent packed-byte checks
and official Test/extraction. The completed affected group has 24 distinct
functional data cases (28 Qt totals including each executable's setup/cleanup),
combined across the initial run and failed-case repairs. It was not one
uninterrupted passing run. The new property assertion initially used console
names instead of native `Warnings` / `Errors`, and the GUI assertion initially
watched the filesystem-only signal rather than archive completion. Both test
assertions were corrected. The independently selected `errorsAndRecovery` case
also depended on another case's archive; it now creates its own small fixture.
All first failures remain in [writable-update-tests.log](distribution.md).

The application was relinked, bundled and ad-hoc signed. Strict dependency
verification passed for 15 Mach-O files and isolated bundled startup passed
without stdout/stderr or changed user preferences. This is an affected
incremental build, not a new empty final release build or physical Finder
acceptance. Source/payload hashes and report provenance are recorded in
[writable-update-evidence.json](distribution.md). The full format
matrix and unaffected native menu tests were not rerun for this update policy.
