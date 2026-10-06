# Normal extraction integration (2026-10-05)

The normal SevenZipProcessBackend now enables the imported official
CheckExistFile / ArchiveExtractCallback decision and publication channels.
Each required message runs on one sequential QtConcurrent worker. Replies
return on the GUI thread; process completion waits for pending work and
metadata finalization before releasing private staging. Native process exit
codes and Cancel are retained.

Overwrite / Skip / Auto Rename / Auto Rename Existing are passed to the
original engine before opening streams. Native six-answer Ask uses the
existing Qt broker. The original callback publishes each completed stream
before observing the next output. Public hard-reference seeds are validated
against the captured source fingerprint before use. Approved retired outputs
are restored if publication never completed; conflicting recovery data is
retained with a diagnostic.

Rename Existing now follows filesystem case sensitivity when rebinding an
output spelling. Creation metadata follows the moved inode rather than a
superseded same-name output. The external-change regression now compares
canonical existing paths, so /var and /private/var refer to the same fixture.

## Executed checks

- Extraction metadata: 290 passed, 0 failures/skips.
- Native progress: 15 passed, 0 failures/skips.
- Archive transfer: 30 passed, 0 failures/skips.
- External editor write-back: 32 passed, 0 failures/skips.
- Archive Open modes: 24 passed, 0 failures/skips.

Total: **391 passing checks**, including setup/cleanup. Application and affected
test targets build successfully. [Captured output](distribution.md).
The first activated scoped run failed; case-sensitive spelling rebinding,
Cancel reporting, retained previous outputs and the fixture's canonical path
comparison were corrected before these final runs.

This is a functional integration checkpoint, not complete Windows parity.
Remaining directory/link/root-elimination/absolute-selection combinations,
engine crash outputs, intermittent SMB change detection, physical Finder/menu
operation, remaining portable UI/settings, final empty-directory build and
license/privacy/publication review remain in the completion inventory.
