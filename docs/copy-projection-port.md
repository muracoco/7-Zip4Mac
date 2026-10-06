# Copy output names and exact CopyFrom input sequence

This source-defined group closes the earlier early-duplicate guard and temporary
directory enumeration approximation. Implementation and bindings are finished
together before a single affected validation group. Failed/affected paths alone
are rerun after repairs; the final complete format/clean-build gate remains separate.

## Source and behavior

Official 26.03 `App.cpp::OnCopy`, `PanelItems.cpp::GetItemName_for_Copy`,
`Agent.cpp::Extract`, `ArchiveFolder.cpp::CopyTo`,
`ArchiveFolderOut.cpp::CopyFrom` / `CAgent::SetFiles`, and
`AgentOut.cpp::DoOperation` define this workflow.

- `scripts/import-copy-name.py` pins PanelItems.cpp and imports the complete
  output-name body: kpidOutName, fallback Name and Get_Correct_FsFile_Name.
  The native folder substitutes the panel property boundary and non-const
  getter; the policy body is otherwise retained. Copy names travel as a separate
  native row field, independent of visible filename markers and display columns.
- Archive-to-archive Copy first performs ordinary selected temporary extraction
  with original pre-stream overwrite decisions. It then supplies **every selected
  original Copy name**, in order, to CopyFrom. Auto-renamed extras are not added
  merely because they exist in the temporary folder. Duplicate inputs are not
  silently collapsed or rejected before the user's overwrite interaction.
- CopyFrom now calls the official `CDirItems::EnumerateItems2` used by AgentOut,
  through a private binary input vector with a physical base and logical archive
  prefix. Normalized absolute physical inputs use an empty physical base, as
  original Agent drop callers do; the scanner concatenates rather than resolves
  absolute paths. The CLI wildcard censor formerly merged identical arguments and lost
  this original behavior. The reader checks owner, permissions, size, version,
  UTF-8, counts and complete input consumption. It contains names, never passwords.
- Filesystem relative/Flat selections retain their physical source identity.
  Official EnumerateItems2 deliberately discards every selected path's directory
  prefix in the logical archive name. Distinct basenames copy successfully;
  equal basenames from different directories produce the original duplicate
  update failure. Absolute drop inputs use the same original behavior.
- Copy refresh returns focus to the visible list, including original List mode 2.

Source-defined behavior is distinguished from missing implementation: archive
Move returns E_NOTIMPL. Agent::Extract treats an entered single-file Copy
destination as a **directory even without its final slash**; explicit filename
renaming is provided by FSFolder Copy, not archive CopyTo. The implementation
retains these original behaviors rather than inventing archive Move/rename.

Get_Correct_FsFile_Name uses official platform conditions: POSIX names retain
characters valid on macOS; Windows device names/illegal characters and NTFS
stream application are OS differences. Existing unsafe archive-path, symbolic
parent, guarded overwrite, input preservation and asynchronous cancellation
protections stay in place. Real physical Finder/input checks remain separate.

## Validation

Run `./scripts/test-copy-projection.sh /absolute/build/path /absolute/log/path`.
The group covers real 7z/ZIP Copy, normal and Flat same-name collisions, all six
overwrite choices before update, retained archive bytes on duplicate failure or
Cancel, FS Flat physical selection / original logical basename policy, Unicode, empty folders, normal failure/reuse and
the directly affected native metadata/transfer/overwrite paths. Executed results
and raw logs are appended after this group; code alone is not a passed test.

This is an intermediate group. Full parity, final refactor/clean build,
complete-format acceptance, license/privacy review and public release remain
required; no completion/publication declaration is made here.

## Executed result (2026-10-06)

Latest valid cases: **120 passed, 0 failed, 1 skipped**. Setup/cleanup are counted
once per suite; repeated affected cases are not counted twice.

| Suite | Latest passing cases | Skipped |
| --- | ---: | ---: |
| CopyWorkflow: existing workflow plus all six duplicate overwrite choices, Flat collisions, FS logical basenames and single archive destination behavior | 41 | 0 |
| ArchiveTransfer: 7z/ZIP/TAR/WIM, gzip/bzip2/XZ, inherited encryption, link Copy/Move, input preservation, Pause/Cancel and GUI refresh/drop | 30 | 0 |
| Properties: native schema/tree/control names, raw WIM, nested update and teardown | 19 | 0 |
| Overwrite: existing six-choice, extraction/FS, metadata, read-only and Cancel paths | 19 | 1 |
| Native metadata formatter | 11 | 0 |

The skip requires an explicitly configured writable second volume; it is not a
pass. Qt logical event/dialog checks are not physical Finder/AppKit/Fn evidence.
The full format matrix and final empty-directory release build were not repeated.

The first grouped run found one integration bug across 24 ArchiveTransfer cases:
EnumerateItems2 concatenates its physical base and file path. Combining a
nonempty base with already absolute API inputs produced nonexistent paths.
The adapter now binds normalized absolute physical names to an empty base, as
original Agent drop callers do, while retaining every requested input. All 30
transfer cases pass after this common-boundary repair. The original failed scan
could rewrite an unchanged archive with a warning; these failures used owned
fixtures and were never treated as successes or applied to user data.

Two new Flat test expectations were also corrected against original EnumDirItems:
its explicit comment and implementation discard each selected directory prefix
in logical names. The former expectation of preserved archive child prefixes was
wrong. The corrected tests prove unique basename Copy and duplicate-basename
failure/byte preservation for both 7z and ZIP. The two retired failing data rows
are not included in the latest result count. The complete changed CopyFrom paths
were rerun (23 checks including repeated setup/cleanup), while unaffected
Properties/native formatter/Overwrite successes were retained.

Evidence: [first grouped run, including failures](distribution.md),
[repaired and directly affected Copy paths](distribution.md),
[affected existing transfer paths](distribution.md).
No completed-feature failure remains in this group. Final functional inventory,
refactor/release, physical input and authorized publication remain unfinished.

## Bundled app confirmation

The rebuilt app is at
`/DEPS/build-comments-20261004/7-Zip Mac.app`.
Deployment and strict ad-hoc signature/dependency checks passed: 15 Mach-O files
have system or bundle-relative dependencies only. The owned bundled executable
remained alive for three seconds with zero stdout/stderr bytes, created one
private INI file, and left matching native user preferences byte-identical.
An invalid explicit settings profile returned exit 2 without native fallback.
Only owned processes were terminated. This is startup/deployment confirmation,
not physical mouse/Finder/Fn or complete Windows parity verification.
[Actual bundle/startup output](distribution.md).
