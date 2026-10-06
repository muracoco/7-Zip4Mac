# Local 0.2.0 candidate — 2026-10-06

The current candidate includes the address font adjustment, version metadata
and repaired ZIP omitted timestamps. Its compiled application revision is
`734235ca1ebe60283a2e73a1e76d74013ec91c9a`. Later startup-driver/documentation
changes do not change application, importer, engine or Cocoa implementation.
Public source publication completed on 2026-10-06 at
[muracoco/7-Zip4Mac](https://github.com/muracoco/7-Zip4Mac).
[Publication closeout](publication-complete.md) records the verified remote
snapshot; hardware/pixel limits below remain explicit.

## Actual checks

- Initially empty Release/Ninja build: 252 tasks, official 7-Zip 26.03 and
  dynamically bundled QtBase 6.11.3; arm64, deployment target macOS 15.0.
- Strict ad-hoc signature and dependency checks: 15 Mach-O files, only system
  and bundle-relative dependencies. No Homebrew/Qt development runtime path.
- Fresh bundled-engine omitted-timestamp regression: 3/3 Qt cases including
  setup/cleanup. The full affected extraction suite passed earlier against the
  repaired implementation: 323/323 cases in 19.95 seconds.
- Actual final-app desktop launch, Japanese/spaced ZIP address entry, listing
  and Extract dialog completion. Restored SHA-256 matched the original; access
  and creation timestamps were valid. Filesystem display showed normal dates.
- Fresh main-window screenshot confirmed address/list text alignment. Runtime
  font evidence is in [address-font-adjustment.md](address-font-adjustment.md).
- Actual Finder quick compression, versioned candidate route, GUI Test,
  extraction and final Manager evidence is in
  [finder-desktop-acceptance.md](finder-desktop-acceptance.md).
- The first reviewed 0.2.0 snapshot verified 810 files, preserving code/license
  bytes and normalizing only documentation. Updated review follows the final
  documentation commit; earlier exports and distribution archives are retained.

The original 35-suite run and 151 format registrations are recorded in
[release-consolidation.md](release-consolidation.md), including initial failures
and affected repairs. They are not presented as one uninterrupted green run or
repeated for these isolated changes. Official Windows icons remain unchanged.

## Reviewed distribution

The separate distribution app contains the final 812-file reviewed public
corresponding source. Its local source Git snapshot has one English initial
commit, `56f1eb2cb04ecf65aa7b5861c1a8ee837064799a`, the connected account's public
noreply identity and no remote or internal development history. Every Git blob
matches the normalized source manifest. All 15 copied Mach-O files matched the
clean build byte for byte before re-signing.

`7-Zip-Mac-Port-0.2.0-arm64.zip` is 98,622,223 bytes; SHA-256:
`0b8eb965012e660c48cada027189b2b256c67fd0a19c13c738cd94c212de224c`.
ZIP CRC, unpacked signatures/dependencies and all 812 corresponding-source
hashes/modes passed. The signed distribution app passed isolated startup with
zero stdout/stderr, unchanged native preferences and invalid-profile rejection.
This distribution receipt was written after packaging and is not claimed to
exist in that earlier corresponding-source snapshot.

## Retained failures and limits

The first isolated startup check rejected 111 stderr bytes. Reproduction showed
the exact macOS InputMethodKit line `error messaging the mach port for
IMKCFRunLoopWakeUpReliable`; the process stayed alive, and actual address input,
listing and extraction worked. The startup driver now records only that exact
OS message separately and continues to reject other stderr. This does not
suppress a Qt/engine error or claim physical IME composition was tested.

An initial synthesized selection/typing action produced a malformed path and
the normal Cannot open dialog. Explicit field selection/paste then opened the
Japanese ZIP successfully; no crash or user-data write occurred. The former
action is not counted as a successful input test. One publication exporter
attempt failed before writing because its parent directory was absent; the
directory was created and the unchanged exporter completed successfully.
The first read-back probe incorrectly assumed an enclosing tar directory;
correcting the probe to accept the archive's flat layout verified all 812 files.
That probe error did not change the archive or application.

The disabled Finder Other chooser's cause remains unproven; its versioned
candidate route worked. Physical Finder drag, hardware modifier/printing
observations, macOS 15/Intel hardware and Windows pixel equality remain
unverified. Finder Extension/Quick Action/Services are optional unimplemented
integrations; the requested configurable Open With menu is implemented.
