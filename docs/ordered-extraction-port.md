# Ordered extraction source bridge (2026-10-05)

This checkpoint replaces the blanket refusal of regular duplicate output names.
It is not complete Windows parity or a final release.

## Original processing and host adaptation

The pinned original `Common/ArchiveExtractCallback.cpp` still owns stream
creation, corrected paths, link interpretation, inferred hard links, deferred
post-links and metadata. Its source SHA-256 remains
`f8bc44e06d292d82213652a6ee5b776310998e097c8c475036fe217b4d7fbc5f`.
No compression/crypto/handler implementation was replaced.

Each completed output is recorded with its real archive index and actual path.
Private APFS clones, or private copies on other filesystems, retain earlier
payloads before the callback replaces a same-name staged file. Cached copies
have different inodes and do not change the original hard-link graph. Source
identity/change checks guard the copy. I/O errors include the operation, quoted
filename, system message and errno; passwords and payloads are never recorded.

The versioned `7ZOUT002` manifest preserves callback completion order, including
post-links after ordinary streams. Repeated notifications for the same native
item/path retain the final notification. Split-position output is captured only
after the original callback assembles it. Inferred WIM links have an observation
hook after the original SetAttrib because their handler need not send the normal
stream-result notification. Deferred link snapshots are taken after the original
post-link time/owner work.

Installation applies Overwrite / Skip / Auto rename / Rename existing and the
six Ask choices to this ordered list. It uses guarded directory handles and
verified destination identities. Unicode normalization is used for matching
reported staged names, preserving the logical output names. On a known
case-insensitive macOS volume, ATTR_CMN_NAME supplies the actual catalog spelling;
a guarded same-entry rename restores the requested casing after a swap. A scan
of the entire output directory is not needed. Case-sensitive volumes retain
separate names.

Rename existing now returns the preserved file identity from both atomic and
copy fallback installations. Earlier archive outputs moved to backup names
retain their explicit creation times. Creation time is also restored on each
surviving inode after aliases. The Windows `FileIO.cpp::COutFile::SetTime` passes
CTime/ATime/MTime independently to SetFileTime. macOS console extraction can lower
birth time when MTime is earlier than CTime; the port keeps the explicit archive
CTime. This comparison therefore uses independent pristine-console property
output for CTime, and pristine-console output files for names, bytes, permissions,
modified times and hard-link relationships.

## Executed checks

The final metadata suite passes **236 checks**, including setup/cleanup:

- Genuine original-engine 7z archives are created with a/add and rn/rename,
  including duplicate names and creation/access timestamps. ZIP/TAR fixtures use
  standard stored container writers; no compression algorithm was implemented.
- Full and No pathnames, ordinary and native-index selections, empty/existing
  output directories and occupied auto-name suffixes, four overwrite policies,
  exact duplicates, case-only collisions and Japanese voiced-character names.
- Six Ask answers, a mixed No / Yes / No sequence, cancel and backend reuse;
  original console receives the corresponding y/n/a/s/u/q answers on stdin.
- A TAR with repeated payload names and deferred hard-link chains compares bytes,
  modes, modified times and all pairwise inode relationships in the four modes.
- Existing 7z/ZIP/TAR/WIM metadata/link checks and selected TAR/RAR5 public-target
  checks continue to pass.

The affected archive-transfer (30), native-progress (13), editor (32) and open-mode
(24) suites pass: **99 checks**. Selected/ancestor/nested/Pause/Cancel regressions
pass **18 checks**. One full rerun is justified by the shared extraction change:
**all 151 handler/extension registrations / 155 checks** pass, including the Qt
right-click creation, Test and extraction route for 7z/ZIP. These are automated
checks, not physical Finder, desktop menu or function-key evidence.

Initial validation found and fixed the /var canonical-temporary-path rejection,
missing inferred-WIM notifications, explicit POSIX-link permission inheritance,
Unicode matching, swap-retained casing and creation times on renamed backups.
F_GETPATH was insufficient to determine the actual directory-entry spelling;
ATTR_CMN_NAME resolves it. A first birth-time comparison used macOS console's
clamped value; the final oracle distinguishes that OS effect from the explicit
Windows archive CTime. Only final successful checks are counted as passing.

[Execution transcript](distribution.md).

The normal app was rebuilt and packaged. All 15 Mach-O files pass dependency and
ad-hoc signature checks with system/relative framework dependencies. With private
preferences and development Qt variables removed, it remains alive after three
seconds; bundled 7zz reports 26.03 and the native helper code section matches the
tested build. Only the owned process was stopped. This is incremental-build and
startup evidence, not the final empty-directory build or physical interaction.

## Remaining extraction scope

Engine preparation still decompresses selected streams in private staging before
host overwrite decisions; it does not yet skip decode work or ask at the original
pre-stream point. Normal engine failures now retain callback-recorded outputs,
and installation failures finalize recorded directory metadata; see
[partial-extraction-port.md](partial-extraction-port.md). Cancellation during
engine preparation and abnormal exits still discard staged outputs and require
further integration/comparison. File/directory conflicts and all ordered
mixed-link/overwrite combinations are not declared complete by the regular-file
matrix. Fully absolute stored link references and root-elimination combinations
remain unverified. Intermittent SMB change-detection behavior is still unresolved;
this matrix runs in owned local temporary folders. The remaining UI/list/Help,
physical interaction, final clean-build/license and publication gates stay open.
