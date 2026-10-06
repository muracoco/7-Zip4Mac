# Reviewed distribution candidate — 2026-10-06

The local distribution archive and public-source Git snapshot are prepared and
verified. This is not a public release or a declaration of complete Windows
parity. External desktop acceptance remains incomplete; no repository, remote,
push, tag or GitHub Release has been created.

## Frozen source and packaged application

- Compiled application revision: `07968cd43fbc77e747df9b7bd3a4f9bc5582595d`.
- Documentation snapshot: `14a3ba2d4610f71d03e4a734887c1b5db98bb7e6`.
  Its difference from the compiled revision is documentation only.
- Application version 0.1.0; official 7-Zip 26.03 and QtBase 6.11.3; arm64;
  deployment minimum macOS 15.0, tested on macOS 26.6.2.
- Prepared archive: `7-Zip-Mac-Port-0.1.0-arm64.zip`, 98,621,961 bytes.
- SHA-256: `2cde0da8620699cbf15d496521791070ccdea5a55a41e452a24edb80ed18cd4e`.

The original compiled bundle is retained. A separate owned copy contains the
reviewed public corresponding source: all 803 files, with documentation-only
path normalization and no internal Git history. All 582 non-documentation
source/license/resource files match the original compiled bundle's manifest.
All 15 copied Mach-O files were byte-identical before re-signing; no reviewed
private home/workspace prefix occurs in them. Re-signing changes signatures
and the source resource seal, not application implementation.

The public-source checkout has exactly one English initial commit,
`34a460ce30da81855f1a9af8768a47322f7c92b0`, using the connected GitHub account's
public noreply identity. All 803 Git blobs match the reviewed source manifest,
including original CRLF/resource/license bytes. No credentials are included.
The source is local only and has no remote. English README and the separate
Japanese README are retained; the prepared repository description is bilingual.

## One distribution verification group

The archive was extracted to a separate owned directory. ZIP CRC passed; every
normalized corresponding-source hash matched the supplied 803-file manifest.
Strict ad-hoc signatures and all 15 Mach-O dependency/rpath checks passed.
Starting the extracted application with an isolated settings profile remained
alive after three seconds, produced no stdout/stderr and left native user
preferences unchanged. An invalid explicit profile was rejected without native
fallback. Only owned test processes were stopped.

The original Windows `FM.ico` and About `7zipLogo.ico` match the unmodified
26.03 source byte for byte. The current bundle selects and contains the project's
FM ICO-to-ICNS conversion; no replacement logo or Microsoft system artwork was
introduced. Hashes are in [distribution-evidence.json](distribution-evidence.json).

The application/engine implementation did not change in this batch. Existing
35-suite/affected-repair and 151-registration evidence is reused; none of those
suites was rerun merely to assemble the distribution archive. This report was
written after packaging and is not claimed to exist in that earlier source
archive or local public-source commit.

## External desktop limitation

The current attempt to acquire Finder returned `cgWindowNotFound`; the optional
computer-use launch API was unavailable. The current application's native
Alt menu input opened File, but AX row selection did not advance the selection.
Coordinate clicking was rejected before delivery as `noWindowsAvailable`.
These observations do not prove an application failure or completed Finder
routing, right-click command completion or drag/drop. The owned application
was closed through File → Exit before packaging. No OS security/lock/default
association setting or alternate UI automation was used.

Actual desktop Options, Open With operation completion, Manager and F5 evidence
is in [release-consolidation.md](release-consolidation.md). Test-process AppKit
and Qt context/drag checks remain separate from complete external desktop
observations. The outstanding gates in [completion-plan.md](completion-plan.md)
remain unchanged; conditional publication has not occurred.

Local artifacts are retained in the owned cache's `release-staging` directory:
`7-Zip Mac.app`, the ZIP, `SHA256SUMS`, `CANDIDATE.json`, `source` and completed
preparation/distribution logs. Original-log hashes and scope flags are recorded
in the evidence JSON. The check logs contain no credentials or desktop inventory.
