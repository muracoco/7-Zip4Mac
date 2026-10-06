# Local 0.2.0 candidate — 2026-10-06

The current candidate includes the address font adjustment, version metadata
and repaired ZIP omitted timestamps. Its compiled application revision is
`734235ca1ebe60283a2e73a1e76d74013ec91c9a`. Later startup-driver/documentation
changes do not change application, importer, engine or Cocoa implementation.
Public repository publication is pending GitHub browser authentication, not
additional portable feature implementation.

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

The disabled Finder Other chooser's cause remains unproven; its versioned
candidate route worked. Physical Finder drag, hardware modifier/printing
observations, macOS 15/Intel hardware and Windows pixel equality remain
unverified. Finder Extension/Quick Action/Services are optional unimplemented
integrations; the requested configurable Open With menu is implemented.
