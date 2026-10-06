# Official Agent Delete and Rename

The port now imports `CAgent::DeleteItems` and `CAgent::RenameItem` from
official 7-Zip 26.03 `CPP/7zip/UI/Agent/AgentOut.cpp`, pinned SHA-256
`4acc6c5587847eef1e11497e0dea149818b6485811c05608302cf9fa09dc5335`.
`scripts/import-folder-update.py` generates `AgentItemUpdate.inc` with the
original copyright/license notices. The update-pair loops, NoChange handling,
rename prefixes, main-item flag and alternate-stream/Flat policies are retained.

The boundary replaces the Windows COM host with the opened archive/proxy and
the official console `IUpdateCallbackUI`. Delete notifications use
`ShowDeleteFile(path,isDir)`, the same bridge used by the upstream
`UpdateCallbackAgent.cpp:205`. Repeated references to the same real item are
normalized; different indices sharing one name remain separate items.

## Selection and installation

GUI Delete/Rename for 7z, ZIP, TAR, WIM, gzip, bzip2 and XZ use native
directory/local-item identities and the imported Agent bodies. The same-name
guard is removed only for connected native writers. Single-stream behavior now
follows each original handler, including ignored names, failed gzip/bzip2
deletion and XZ's empty logical item. [Comparison and checks](single-stream-updates.md).

Delete uses `GetRealIndices(...,true,false)`: ordinary folders recurse, Flat
folder rows do not. An implicit Flat folder can legitimately select no real
items, producing a successful no-op. Rename uses `(...,true,true)`, including
folder descendants in Flat mode. One selected row is required for Rename.
The proxy/folder lifetime now extends through the original update body.

The cached source token must match the original regular file before a
cancellable copy/hash. Selection is then rebound to that verified candidate,
which is reopened with its known handler (including prefix search). Only
BeforeList and Update receive the selection protocol. AfterList does not reuse
indices from a handler that may have reordered its entries.

The original Agent stream block and official handler write a fresh candidate.
Verification compares a multiset of semantic entries, retaining duplicate
multiplicity and preserving unrelated items; it does not assume handler order.
WIM generated XML can be regenerated. Original bytes/identity and leading
prefix bytes are checked before atomic installation. Ownership, ACL, xattrs,
mode and the configured working-folder behavior use the existing transaction.
Failed/cancelled precommit work retains the original. This is not a global lock
against an external process racing the final identity check and rename.

The handler decides whether encrypted data needs reading. ZIP Rename/Delete
and 7z Rename copy packed data without an unrelated data-password requirement.
Deleting only part of an encrypted 7z solid block needs its password to repack.
Encrypted headers need a password at open. Passwords remain on redirected
stdin and do not enter the private selection request or environment.

The official default TAR NewProps writer rebuilds selected renamed headers as
GNU: second-resolution MTime, without PAX CTime/ATime or PAX extra-record text.
The test verifies this behavior and the preserved payload. Unselected NoChange
entries retain their metadata. This checkpoint does not claim arbitrary PAX
metadata preservation. ZIP's added zero timestamp precision is normalized
without accepting an actual time change.

## Verification (2026-10-05)

Apple M3/arm64, macOS 26.6.2, Apple Clang, Qt 6.11.3. The source-built native
helper and Qt Release build succeed. The expanded selection suite passes
**32 checks**, including setup/cleanup:

- Each genuine same-name ZIP sibling can independently be deleted or renamed;
  extraction compares the retained and renamed payload bytes.
- Unrelated bad-CRC packed data can be retained during Rename; deleting that
  exact bad entry leaves an archive that passes Test.
- Normal/Flat implicit folders, Japanese/space names, 7z/ZIP/TAR/WIM writers,
  leading 7z/ZIP prefixes and PAX headers are tested.
- Data/header encrypted 7z and AES ZIP, wrong-password original retention,
  duplicate destination/stale/invalid selection refusal are tested.
- Qt GUI Rename and confirmed Delete operate one same-name sibling. A genuine
  two-image WIM preserves image one while updating selected items in image two.
- A 32 MiB encrypted solid update pauses while the event loop responds, cancels
  with the original SHA-256 unchanged, then reuses the backend successfully.
  Existing 64 MiB extraction Pause/Cancel and selected context-menu Test also pass.

Affected regressions: Open modes/prefix/nested 24, Properties 19, folder updates
14 and native tree 4. These **93 checks** include setup/cleanup. The final
tree/GUI rerun updates the earlier temporary disabled-action expectation.
See [execution log](distribution.md).

```bash
./scripts/build-progress.sh
cmake --build /absolute/path/to/build
./scripts/test-agent-tree.sh /absolute/path/to/build agent_selection_tests
./scripts/test-agent-tree.sh /absolute/path/to/build
ctest --test-dir /absolute/path/to/build -V -R '^(archive_open_modes|properties)$'
```

The normal app is repackaged with the bundled adapter, Qt frameworks, patched
Cocoa plugin and corresponding sources/licenses. Dependency/signature and an
isolated LaunchServices startup are checked separately. Physical desktop
clicks, final empty clean build, final full-format matrix and publication were
pending at this scoped checkpoint. Writable alternate-stream update fixtures
were not verified in this batch. Later replacement indices, overwrite sequencing
and selection restoration are connected; see [current-status.md](current-status.md).
ZIP Comment imports the official operation; see [comments](native-agent-comments.md).
The later release/build evidence is in release-consolidation.md and
publication-review.md. Physical acceptance and public publication remain pending.
