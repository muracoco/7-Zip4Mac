# Ordered extraction installation session

This is the historical component checkpoint. Its unconnected-callback notes
were superseded by [normal-extraction-integration.md](normal-extraction-integration.md).
Use that integration report and [extraction-lifecycle.md](extraction-lifecycle.md)
for the current implementation status.

The existing guarded output installer is now a reusable, stateful
`ExtractionInstaller`. Normal application extraction uses this session after
the engine finishes, retaining its completion order, hard-link identities,
rename-existing outputs and directory/creation-time finalization.

The session can also accept one completed output at a time and a previously
approved destination snapshot. Actual files are published before the next
call. A changed approved destination is rejected without overwriting it.
Equivalent macOS Unicode filename spellings are compared through QFile's
filesystem encoding, rather than raw QString equality.

This is a prerequisite for connecting the original pre-stream overwrite
callback. **The normal application still extracts to private staging first.**
Native per-file publication, safe public-output state queries and activation
of the decision channel remain unfinished; these checks do not establish
complete Windows parity.

## Verification on 2026-10-05

- Two new actual-file cases verify incremental output visibility,
  rename-existing history, creation times, idempotent finalization, rejection
  after finalization and a destination replaced after approval.
- Extraction metadata: 265 passed, no failures/skips, including setup/cleanup.
  Existing Unicode collision, partial failure, links and overwrite checks run
  through the production session.
- Native progress: 15 passed, no failures/skips, including setup/cleanup.
- The File Manager application target builds successfully in the existing
  local build directory. This increment is not a new clean-build/release gate.

[Executed output](distribution.md). The normal packaged application
remains at the previous verified checkpoint until the next application package.
