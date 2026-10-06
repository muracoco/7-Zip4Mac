# Official Agent ZIP comments

Baseline: official 7-Zip 26.03, `CPP/7zip/UI/Agent/AgentOut.cpp`.
`scripts/import-folder-update.py` imports the complete `CAgent::CommentItem`
body into `src/upstream/AgentComment.inc`, with the original copyright notice.
The pinned source SHA-256 is
`4acc6c5587847eef1e11497e0dea149818b6485811c05608302cf9fa09dc5335`.
Only the Agent host and COM GUI callback boundary are adapted. The official
update callback, ZIP serializer, codecs and crypto remain upstream code.

The original operation calls `GetRealIndex` for exactly one focused item,
sets `NewProps` on that item, supplies `CommentIndex`/`Comment`, and leaves
every item's packed data unchanged. It does not expand a folder or its ADS.
Implicit directories have no real index and cannot receive a ZIP comment.
Qt captures the focused row and source snapshot when opening the dialog;
selection and focus are restored by item identity after saving.

The bundled native helper opens the same handler used by the listing and
invokes that original operation. Comments travel as UTF-8 in a private
owner-only file, with no NUL and at most 4 × 65,535 input bytes. The original
writer separately enforces the 65,535-byte encoded field limit. Empty input
removes the comment. Compatibility requests containing only a path must resolve to
one real row before updating; same-name requests need an explicit row identity.

The backend copies the original into private staging, validates the native
selection against the byte-identical candidate, and verifies the output's
entry count, order and semantic properties. Only the selected comment may
change. ZIP's official writer emits the update-pair order; requiring that
order also makes restoring the old GUI item identities safe. Original stat,
SHA-256, leading prefix, ownership, ACL, xattrs and mode are checked/preserved
before atomic replacement. Failure and cancellation retain the original.

There is no whole-archive Test in this metadata-only operation. Such a Test
previously imposed passwords for unrelated encrypted payloads and rejected
comments when an unrelated existing payload was corrupt. The handler now
copies those packed streams exactly as the Windows Agent does. Tests verify
both successful chosen payloads and continued failure of an already corrupt
unrelated payload; success of Comment is not a claim that the archive passes Test.

## Remaining limits

- Signed ZipCrypto descriptors are supported by the later common writer repair;
  the former refusal guard has been removed. Unsigned metadata updates retain
  the original handler's E_NOTIMPL. See [zip-metadata-port.md](zip-metadata-port.md).
- Single-layer leading prefixes are retained. Tail/multiple-layer restrictions
  come from the original Agent; warning rejection and transactional validation
  are added port safeguards. Volume/read-only restrictions belong to the update
  path/handlers, not a generic warning check in CAgent::CanUpdate. See
  [update-safeguards.md](update-safeguards.md).
- ZIP decoding/encoding uses official handler code. Exhaustive Windows OEM
  locale comparisons, every unknown extra field and exact dialog pixel parity
  remain unverified. The UI uses the source-defined single-line editing workflow.
- Filesystem `descript.ion` comments remain a separate TextPairs-compatible
  implementation. This source port covers ZIP entry comments.
- External editing/nested UpdateOneFile selection was completed in the later
  [replacement source port](native-agent-replacement.md). The authoritative
  remaining inventory is [current-status.md](current-status.md); desktop checks
  remain separate from connected commands.

## Scoped verification

Run the native selection cases with dedicated generated fixtures:

```bash
./scripts/test-agent-tree.sh /path/to/build agent_selection_tests \
  nativeCommentSameName nativeCommentNoUnrelatedTest \
  nativeCommentEncryptedPackedData guiSameNameComment \
  normalAndFlatFolderRules encryptedNativeUpdates
ctest --test-dir /path/to/build -R '^file_comments$' --output-on-failure
```

The following results describe this original scoped run, before the later ZIP
metadata and consolidated release batches. Its descriptor-refusal expectation
was superseded; preserved logs are not current missing-feature lists.

The checks cover chosen same-name siblings, clear/Japanese/multiline comments,
real and implicit directories, AES packed data without an unrelated password,
GUI focused-item editing with a different selected sibling, preserved corruption,
and affected original selection/update rules. The existing comment suite covers
ZIP64, BZip2/LZMA, ordinary ZipCrypto, descriptor refusal, maximum length,
independent Python byte checks, filesystem comments and failure/cancellation.
Executed on the current arm64 Mac: **42 passed / 0 failed / 0 skipped**,
including setup/cleanup (31 comment-suite cases and 11 native-selection cases).
Native helper and Qt builds exited zero. Raw output:
[native-agent-comments-test.log](native-agent-comments-test.log).
These Qt event-driven checks do not replace physical native-menu/Finder testing.
The normal app bundle was rebuilt/packaged, passed dependency/signature checks
and started through LaunchServices with the bundled patched Cocoa plugin and
the updated native helper. Later automated release/build results are in
[release-consolidation.md](release-consolidation.md) and
[publication-review.md](publication-review.md); physical acceptance remains pending.
