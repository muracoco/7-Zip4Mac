# Copy / Move / Combine source-defined implementation group

This group is implemented as a whole before its grouped validation. Incomplete
features are tracked separately from failures in implemented behavior. Repeated
full-format or clean-build tests belong to the final release gate.

## Authoritative upstream

Official 7-Zip 26.03 `CPP/7zip/UI/FileManager/CopyDialog.{cpp,h,rc}`,
`CopyDialogRes.h`, `App.cpp::OnCopy` and `GetItemsInfoString`,
`PanelKey.cpp` F5/F6 cases, `PanelSplitFile.cpp::Combine`, and
`CPP/7zip/UI/Agent/ArchiveFolder.cpp::CAgentFolder::CopyTo`.

`scripts/import-dialog-geometry.py` pins the original resource and imports
`CCopyDialog::OnSize` unchanged. `scripts/import-copy-info.py` pins App.cpp and
PanelSplitFile.cpp and imports the original summary formatting bodies unchanged.
Qt adapters supply controls, translated strings and already-listed properties.
The compression and archive-handler algorithms remain official code.

## Scope and acceptance evidence

| Behavior | Implementation | Validation |
| --- | --- | --- |
| IDD_COPY 96: label, editable destination/history, browse, source summary, OK/Cancel | Implemented, original 336×160-DLU resource and original resizing | Passed; see results below |
| Original folder/file counts, optional sizes, source folder, first five names, ellipsis | Implemented using imported formatting bodies | Passed; see results below |
| F5/F6; Shift+F5/F6 operates on focused row, even when other rows are marked | Implemented | Passed; see results below |
| Copy/Move destination history, shared maximum 20, unique recent first | Implemented | Passed; see results below |
| Filesystem to filesystem, explicit filename / slash-terminated folder, multiple inputs | Implemented; existing guarded FileOperations installer reused | Passed; see results below |
| Filesystem to open destination archive with handler defaults and archive prefix | Existing implementation retained; unified panel-state restoration | Passed; see results below |
| Archive Copy to filesystem uses Copy dialog, current paths or Flat mode | Implemented; original Agent selection and native extraction callback reused | Passed; see results below |
| Archive to archive Copy through temporary extraction then CopyFrom | Implemented; both panel states locked through both phases and refreshed sequentially | Passed; see results below |
| Archive Move | Windows source returns E_NOTIMPL, including writable archives; display the source-defined unsupported error, retain original data | Passed; see results below |
| Combine input, labels, summary and resize | Implemented with the same IDD_COPY; no invented Combine-history dropdown | Passed; see results below |
| Browse button | macOS difference: OS folder picker replaces ordinary Windows Shell folder browser; trailing slash retained | Qt dialog structure only; physical OS picker pending |

Cancellation before accepting the destination leaves output and history untouched.
Native process/password/cancellation/error handling stays in ArchiveBackend.
The archive-to-archive temporary directory is owned through both phases and removed
when the operation ends. Failure leaves archive inputs intact; no archive Move
extraction/deletion workaround is introduced.

At this historical checkpoint, same-name archive rows were refused before
temporary extraction. That early guard was Partially implemented. The following
[Copy projection group](copy-projection-port.md) replaces it with the original
temporary-extraction/overwrite interaction and exact CopyFrom input vector;
consult that report for current implementation and executed evidence.

The platform path adapter preserves entered trailing slashes rather than losing
directory intent during QDir normalization. Relative destinations are based on
the source panel's filesystem or virtual archive path, matching OnCopy. A single
panel opened inside an archive defaults to the outer archive's real containing
directory. An archive destination is selected by matching an open panel address,
not by guessing from a filename extension.

For filesystem transfers, Copy history is stored after the worker reports that
destination preparation succeeded, before copying individual inputs. Initial
destination-directory errors therefore do not insert history entries, matching
the original OnCopy ordering while keeping directory creation asynchronous.

## Reproduction

`./scripts/test-copy-workflow.sh /absolute/build/path /absolute/log/path` runs the
new UI/workflow suite, existing archive-transfer and overwrite suites, resource
dialog checks, and selected real filesystem Copy/Move/Combine/Cancel integration
checks. These use owned temporary files and isolated test preferences. Qt event
injection is not proof of physical Finder, AppKit menu clicks or hardware Fn keys.

Results and actual packaging/startup evidence will be appended after the grouped
run. Neither complete portable Windows parity nor release/publication readiness
is claimed by this intermediate feature group.

## Actual verification (2026-10-06)

The latest outcome for each affected case is **87 passed, 0 failed, 1 skipped**.
Repeated cases are counted once; each suite includes its setup/cleanup cases.

| Suite / selection | Latest passing cases | Skipped |
| --- | ---: | ---: |
| CopyWorkflow: original dialog, summaries, history, filename/folder destinations, focused Shift operations, current/Flat archive extraction, archive-to-archive Copy, cancellation, unsupported Move, Combine, duplicate-row guard | 23 | 0 |
| Existing ArchiveTransfer suite; only affected GUI cases rerun after repairs | 30 | 0 |
| Existing Overwrite suite | 19 | 1 |
| Original resource-dialog suite | 10 | 0 |
| Selected filesystem Copy/Move, Split/Combine, Pause/Cancel integration cases | 5 | 0 |

The skip requires PORT_TEST_SECOND_VOLUME and is not counted as a pass. Archive
fixtures cover 7z/ZIP GUI transfers and existing 7z/ZIP/TAR/WIM/stream CopyFrom
cases. A 48 MiB fixture exercises cancellation while retaining both archives.
The final full-format matrix and empty-directory release build are not repeated
for this intermediate group.

The first grouped run found eight failed cases. Missing directory destinations
with a trailing slash exposed a real FileOperations path-resolution bug: an
empty leaf was rejected before creating the directory. This is fixed by
normalizing the explicit folder before walking its existing ancestor. Other
failures came from test assumptions: a template coordinate versus original
OnSize rounding, locating another panel's list, and implicit Qt current-item
selection. Tests now identify the owning panel and set selection explicitly;
the original resize body is unchanged. A source review additionally found that
staged-file enumeration could silently collapse duplicate selected names;
those rows now fail before updating the destination.

Evidence: [first grouped run](distribution.md),
[repaired workflow run](distribution.md),
[initial duplicate/selection check](distribution.md),
[final affected archive checks](distribution.md),
[affected existing transfer checks](distribution.md),
[filesystem integration checks](distribution.md).

Packaging/startup confirmation is recorded separately after deployment. These
results prove implemented and automated paths, not physical Finder/Fn input or
complete Windows parity.

## Bundle confirmation

The updated app is at
`/DEPS/build-comments-20261004/7-Zip Mac.app`.
Deployment and the dependency check passed for 15 Mach-O files: only system or
bundle-relative dependencies remain. Strict ad-hoc signature verification passed.
The first owned startup process remained alive with empty stdout/stderr, but
its CFPreferences redirection did not isolate application settings. View.LastPath
changed to the owned startup fixture. It was restored from the next pre-existing
recent-folder entry, and only the owned startup history entry was removed. This
is recorded as a failed isolation check, not a successful unchanged-settings
claim. The app now accepts an explicit SEVENZIP_PORT_SETTINGS_DIR for a private
INI profile; scripts/test-bundle-startup.py checks actual INI creation, unchanged
native preferences, and rejection of an invalid profile without fallback. The
result of that repaired startup check is recorded below. Physical input remains
unverified.
[Bundle and startup evidence](distribution.md),
[source-defined error dispatch check](distribution.md).

Source tar packaging excludes Python bytecode/cache artifacts. The deployed
framework/plugin set is QtBase only (including image plugins); no QtSvg module
is bundled. Its QtBase corresponding source and Cocoa patches are supplied.
Final license/provenance/privacy review remains required; this intermediate
bundle is not public-release/legal sign-off.

The repaired startup check passed: one private INI file was created, the bundle
process stayed alive for three seconds, stdout/stderr were empty, and native
user preferences were byte-for-byte unchanged. An invalid explicit profile
returned exit code 2 instead of falling back to native settings. Both owned
processes were checked; no physical input confirmation is implied. Run
`python3 scripts/test-bundle-startup.py /absolute/path/to/7-Zip\ Mac.app` to
repeat this check. [Startup repair evidence](distribution.md).
