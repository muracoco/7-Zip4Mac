# Official Agent file replacement

Baseline: official 7-Zip 26.03 `CPP/7zip/UI/Agent/AgentOut.cpp`.
`scripts/import-folder-update.py` imports the complete `CAgent::UpdateOneFile`
body into `src/upstream/AgentUpdateOneFile.inc`, retaining its copyright notice.
Pinned source SHA-256:
`4acc6c5587847eef1e11497e0dea149818b6485811c05608302cf9fa09dc5335`.

The original operation resolves one real item with both ADS and Flat-folder
expansion disabled. Only that pair receives `NewData`/`NewProps`; all other
pairs are `NoChange`. `KeepOriginalItemNames` retains the original archive name.
The Agent host and GUI COM callback are adapted to the existing native helper;
enumeration errors and non-regular disk inputs are additionally rejected.
The official enumeration, update callback, handlers, codecs and crypto are reused.

External editors and nested archives retain the focused native row and source
snapshot, rather than identifying their update target by name alone. Path-only
backend requests must first resolve one unique real row. Same-name ZIP siblings
can be edited independently through the GUI. After guarded mutations, old-to-new
item mappings refresh pending editor/nested sessions, focus and selection,
including another panel showing the same archive. Changed or deleted identities
are not silently redirected to a different same-name item.

## Validation and passwords

The backend stages stable regular-file copies of both inputs. It compares the
output entry multiset with all unchanged original entries, then extracts the
new item by its new native index into an owner-only staging file. Exact size
and SHA-256 must match the replacement. bzip2's undefined listing size is not
treated as zero; decoded size remains mandatory. If multiple output items are
indistinguishable by semantic properties, all such candidates must match the
replacement bytes before permitting identity rebinding.

There is no unrelated whole-archive Test. The official handler may require old
data passwords to recompress a solid block, but an unrelated corrupt ZIP payload
can remain `NoChange`. Replacement success therefore does not assert that every
other payload passes Test. Passwords stay in memory and reach callbacks through
stdin; the helper's update-password environment flag contains only `1`, never
the password. Header encryption follows the opened official handler's state.

Before atomic installation, the original and replacement stat/SHA-256 are
rechecked. Leading prefix bytes, archive ownership, ACL, extended attributes
and mode are preserved. Native handler warning flags are authoritative: the
console's normal "open with offset" notice does not prohibit a safe prefixed
TAR/WIM update. Failures and cancellation retain the original and GUI recovery
copy. The final source-check/rename interval remains a concurrency limitation.

## Executed checks

On the current arm64 Mac, helper/Qt builds passed and **101 Qt checks passed,
0 failed, 0 skipped**, including setup/cleanup:

- Native selection: 14 (new replacement cases plus affected writers/GUI/WIM).
- External editor write-back: 32, including seven writable formats, encryption,
  nested editing, two-panel serialization, recovery and cancellation.
- Existing comments: 31.
- Open modes/prefixed updates/nested restoration: 24.

```bash
./scripts/test-agent-tree.sh /path/to/build agent_selection_tests \
  nativeReplacementSameName nativeReplacementUnrelatedCorruption \
  guiSameNameEditorWriteBack nativeUpdatesAcrossWriters \
  guiSameNameRenameAndDelete multiImageWimNativeSelection
ctest --test-dir /path/to/build \
  -R '^(editor_writeback|file_comments|archive_open_modes)$' --output-on-failure
```

[Raw final output](distribution.md). The normal app bundle was
rebuilt and packaged; dependency/signature checks and an isolated LaunchServices
startup passed. These automated checks do not replace physical native menu,
Finder, Fn-key or drag-out testing. Complete metadata/link restoration, remaining
portable UI/list behavior and final clean-release/publication gates remain open.
Single-stream replacement and the later [Delete/Rename comparison](single-stream-updates.md)
are covered. The full Windows parity objective is incomplete.
