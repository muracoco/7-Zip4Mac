# Official extraction callback and metadata installation

Baseline: official 7-Zip 26.03
`CPP/7zip/UI/Common/ArchiveExtractCallback.cpp`, SHA-256
`f8bc44e06d292d82213652a6ee5b776310998e097c8c475036fe217b4d7fbc5f`.
The generator retains the complete original source and headers in an external
overlay. Handlers, codecs, streams, link validation and metadata interpretation
remain official code. `NativeExtraction.*` only reports successful physical
output paths and real item indices to an owner-only JSON-lines file. It records
neither payloads nor passwords. The unmodified `7zz` remains bundled separately.

Actual callback paths bind staged files to native properties, including tree
handlers and alternate streams; the Qt adapter does not reconstruct those paths
from displayed names. Both post-links and inferred hard-link outputs are reported.
All records must refer to the private extraction directory and valid item indices.
Unreported non-directory outputs are refused.

`ArchiveExtract.cpp` separates installation from the process lifecycle. Files
move from staging where possible, retaining metadata and inode relationships.
Explicit archive hard links use the original desired target name: when that name
was skipped or auto-renamed, a pre-existing target can remain the link source,
matching official TAR extraction. Inferred inode groups retain the installed
primary's actual output name. Guarded `linkat` installation uses the existing
overwrite broker and exclusive/swap replacement protections.

Safe relative, dangling and directory symlinks are retained. Absolute links that
the original callback rewrites into staging are relocated to the final output
root. Dangerous parent targets still fail through the original callback, with
its exit code and error text. A destination leaf symlink can be replaced or
skipped without following it; symlink output parents remain refused.

Folders restore staged permissions and timestamps after their children, in
deepest-first order, using retained descriptors. macOS creation time uses native
FILETIME properties and is restored after hard-link aliases: setting an older
modification time can otherwise lower birth time on APFS. Implicit/new folders
retain staged attributes. Existing folders only apply attributes defined by the
archive. Creation-time representability and POSIX ownership differ from Windows;
NTFS security metadata remains a platform exclusion.

## Executed checks

On the current arm64 Mac:

- 13 extraction metadata/security checks, including setup/cleanup. Independent
  pristine-console 7z/ZIP/TAR/WIM outputs are compared for bytes, modes, modification
  times, available creation times, raw symlink targets and hard-link identities.
  Four explicit TAR overwrite policies, protected leaf links, dangerous targets,
  output-parent links and backend recovery are covered.
- Affected progress (13), editor (32), Open modes (24), archive transfer (30),
  link dialog (28) and native selection (9) checks pass. Native selection includes
  duplicate ZIP names, ordinary/Flat folders, NTFS streams, Japanese/encryption,
  path safety and selected 64 MiB Pause/Cancel/reuse.
- All 151 format/extension registrations pass 155 checks after this shared
  extraction change. This is a justified full-format rerun, not a clean-release
  gate.
- The initial overwrite run passes 19 checks and skips the cross-volume case
  without a second-volume setting. With the actual SMB workspace enabled, a
  folder-child copy reports a concurrent-change error once; its source remains
  protected. A diagnostic-only change and one targeted repeat pass 3 checks.
  The intermittent SMB condition is **unresolved**, not counted as fixed.

```bash
ctest --test-dir /path/to/build -R '^extraction_metadata$' --output-on-failure
./scripts/test-agent-tree.sh /path/to/build agent_selection_tests \
  sameNameExtractAndHash normalAndFlatFolderRules alternateStreamSelection \
  selectedPathsRemainGuarded encryptedJapaneseSelection \
  selectedExtractionPauseCancelAndReuse
./scripts/test-formats.sh --build /path/to/build
```

[Execution output](extraction-metadata-test.log).

## Remaining scope

Selected public-target links and regular ordered collision outputs are now
implemented and locally tested. See [ordered-extraction-port.md](ordered-extraction-port.md)
for the callback/snapshot bridge, 7z/ZIP/TAR cases, six Ask choices, casing and
explicit creation times. This is not complete Windows parity. Native pre-stream
overwrite/decode Skip, file/directory conflicts, partial engine output and
partial-install directory metadata remain. Fully absolute stored references,
root-elimination link combinations, intermittent SMB detection, physical
interaction and final UI/list/Help/clean-build/license/publication gates remain.

## Selected existing hard-link targets (2026-10-05)

The original `CreateHardLink2` still performs link creation, and `SetLink2` still
interprets and validates the archive reference. A narrow host hook binds its
already-resolved target to the public output namespace. Missing private targets
can receive an empty, metadata-only reference placeholder. Every public parent
and leaf is opened without following links; only regular files qualify. No
public payload is copied into staging, and the original public inode is not
modified during engine preparation.

The original callback records both the private target and desired public
reference. Final installation ignores private placeholders, checks their inode
and zero size, validates the retained public change token, then creates the
actual hard link through the guarded installer. Explicit references keep the
original desired name when another output was skipped or auto-renamed. Alias
permissions/times follow the original callback. Tokens are advanced after the
installer's own link/metadata changes so multiple aliases do not cause a false
concurrent-change error.

Absolute mode uses an owner-only encoded mapping of declared reference names;
paths are not reconstructed from visible list labels. The executed Absolute
cases contain relative archive references. Fully absolute archive link
references and root-elimination combinations are not claimed verified here.
The mapping and output records include filenames/change tokens, never passwords
or payload bytes, and are removed with the owned temporary directories.

The final metadata suite passed **33 checks including setup/cleanup**. It covers
Full / No / Absolute pathnames, native and legacy selections, two aliases,
Japanese/space names, read-only existing targets, four overwrite policies,
missing references, leaf/parent links, a deliberately changed reference and
backend reuse after failure. Bytes, inode relationships, permissions and
available timestamps are compared with the pristine official console.

RAR5 uses the unchanged [libarchive v3.8.2 fixture](https://github.com/libarchive/libarchive/blob/v3.8.2/libarchive/test/test_read_format_rar5_hardlink.rar.uu),
retaining its SHA-256 and original BSD notice in `licenses/libarchive-fixtures.txt`.
It is extraction-only; no RAR compressor was introduced. An initial TAR test
fixture selected the writer's primary payload rather than its link because of
filename sort order. The corrected fixture asserts the actual Hard Link
property before testing; that initial failure is not counted as passing.

The affected progress, editor, Open modes and archive-transfer CTest suites
pass, and 18 native-selection/editor/ancestor/nested/Pause/Cancel checks pass.
Because the shared extraction records changed, all 151 format/extension
registrations were rechecked once: **155 passed, no failures/skips**. The same
run also verifies Qt context-menu compression, Test and extraction for 7z/ZIP.
[Executed output](selected-hardlink-test.log).

The normal bundle was rebuilt and packaged. Its 15 Mach-O files pass dependency
and ad-hoc signature checks; only system/relative framework dependencies remain.
With private preferences and development Qt variables removed, the app remains
alive after three seconds and its bundled official 7zz reports 26.03. The
packaged native helper's code section matches the tested build. A first check
incorrectly compared whole binaries across ad-hoc signing; the corrected code
section comparison passes. Only the owned test process was stopped. This is
startup evidence, not a physical menu/Finder test or final clean build.

This closes the documented existing-public-reference adapter gap. It does not
close the remaining ordered-collision, partial-failure metadata, SMB, physical
desktop, UI/list/Help, final clean-build or publication gates.
