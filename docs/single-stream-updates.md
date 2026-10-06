# Single-stream updates and renamed sessions

The GUI and backend now send gzip, bzip2 and XZ Rename/Delete through the
existing imported `CAgent::RenameItem` / `CAgent::DeleteItems` bodies. The
official handler decides the result. The earlier four-format adapter restriction
has been removed for these operations; Create Folder retains its directory-format
constraint. No compression or stream algorithm is reimplemented.

Baseline: official 7-Zip 26.03 `CPP/7zip/UI/Agent/AgentOut.cpp` and
`CPP/7zip/Archive/{GzHandler,Bz2Handler,XzHandler}.cpp`.

| Handler | Rename | Delete selected logical item |
| --- | --- | --- |
| gzip | Updates the stored filename, retaining decoded bytes | Original handler returns E_INVALIDARG; original archive is retained |
| bzip2 | Succeeds with unchanged bytes; displayed name remains derived from the outer filename | Original handler returns E_INVALIDARG; original archive is retained |
| XZ | Succeeds with unchanged bytes; displayed name remains derived from the outer filename | Original `Xz_EncodeEmpty` writes an empty stream; the reader still exposes one zero-size item |

For XZ deletion the adapter requires exactly one zero-size regular logical item
and a successful official Test before guarded installation. It does not claim
that the reader's placeholder disappears. Failed gzip/bzip2 deletion retains
the original bytes and returns the operation, target, exit code and available
error text. All three GUI commands preserve these upstream behaviors.

## Session identities and selection

Rename verification now returns old-to-new indices for renamed entries as well
as unchanged entries. Pending external editors and nested parent write-back
resolve the new native row and path through that mapping. Renaming one genuine
same-name ZIP sibling during editing writes back to the renamed sibling, while
retaining the other payload. A parent archive's renamed child remains the
write-back target when the child is open in another panel.

`showUpdatedArchive` is shared by Rename, editor completion and another panel's
refresh. It restores native selection/focus from the verified mapping and avoids
an unnecessary post-Rename list operation. This removes the previous lost-focus
behavior. Full ancestor/Flat-folder address, history and directory restoration
are still part of the remaining UI/list work; complete navigation parity is not
claimed by these root-item tests.

## Executed checks

On the current arm64 Mac, the Qt Release build and **80 affected checks** pass,
including setup/cleanup:

- Native selection/update suite: 24. Each single-stream Rename/Delete is executed
  both by the backend and through Qt GUI actions/confirmation. Output archive
  bytes are compared with an independently invoked pristine official console;
  decoded bytes, Test, ignored names and failed-operation original retention are
  checked. The fixture filename includes Japanese characters and a space.
- The same suite covers 7z/ZIP/TAR/WIM, leading prefixes, same-name GUI operations,
  a pending editor renamed before release, two-panel nested parent Rename and
  subsequent child write-back, stale/collision refusal, encrypted updates,
  multi-image WIM and paused cancellation with original bytes retained.
- Existing editor write-back: 32; existing Open modes/prefix/nested checks: 24.

An early new GUI assertion dereferenced a missing focus item and crashed the
**test executable**. The app's lost-focus behavior and the assertion's missing
null check were corrected. Final runs pass; this is not a claim that the earlier
test crash never occurred. No full-format or empty-build release gate was
repeated for this operation-specific change.

```bash
./scripts/test-agent-tree.sh /path/to/build agent_selection_tests \
  singleStreamUpdates nativeUpdatesAcrossWriters guiSameNameRenameAndDelete \
  guiSameNameEditorWriteBack nestedParentRenameWriteBack \
  updateRejectsStaleSelectionAndCollision encryptedNativeUpdates \
  multiImageWimNativeSelection nativeUpdatePauseCancelRetainsOriginal
ctest --test-dir /path/to/build \
  -R '^(editor_writeback|archive_open_modes)$' --output-on-failure
```

[Final execution output](single-stream-updates-test.log). Qt action tests do not
replace physical native-menu/Finder/Fn-key checks. Extraction staging/link gaps,
intermittent SMB change detection, remaining portable UI/Help/list commands,
final refactoring/clean verification and publication remain open.

The normal `.app` was rebuilt and packaged. Dependency/signature checks passed
for all 15 bundled Mach-O files. LaunchServices started a private-fixture app
instance, which remained alive and loaded the bundled Cocoa plugin; its bundled
helper exited zero. Only the owned verification instance was then closed. This
is startup confirmation, not physical menu/Finder verification.
