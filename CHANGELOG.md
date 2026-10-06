# Changelog

[日本語](CHANGELOG.ja.md) · [README](README.md)

Version numbers identify local application builds. Each entry links to its
implementation and verification record; distribution status is recorded there.
Current behavior and limits are described in the README and
[implementation inventory](docs/current-status.md).

## Unreleased

- Rewrite the English and Japanese README and user guide around installation and
  everyday use; keep development records and version history separate.
- Add repository-local GitHub noreply configuration, author/committer checks,
  commit/push hooks and verification of outgoing/remote metadata. Historical
  metadata repairs are tracked separately from future-commit checks.
  [Email privacy checks](docs/git-privacy.md).

## 0.2.5 — 2026-10-06

- Retain original notices, mark Port modifications with their dates and clarify
  the original `7zz` versus the modified helper.
  [License maintenance](docs/license-audit-follow-up.md).
- Add a shared upstream source lock, SHA-256 inventory and isolated comparison /
  regeneration / staging for incremental official-source updates.
  [Upstream updates](docs/upstream-updates.md).
- Make developer tests optional in normal builds. Exclude Port tests, fixtures
  and historical evaluation outputs from the app's corresponding build-source
  archive; retain the development tests in the repository.
  [Distribution contents](docs/distribution.md).
- Verify fresh source-only builds, bundled 7z/ZIP round trips and installed-app
  startup/About. [Executed verification](docs/maintenance-verification.md).

## 0.2.4 — 2026-10-06

- Fix repeated File Manager flicker caused by watcher notifications without a
  content change. Compare contents in the background and preserve the list,
  selection and icons when unchanged; real changes still refresh automatically.
  [Refresh fix and checks](docs/idle-refresh-fix.md).

## 0.2.3 — 2026-10-06

- Center newly opened normal windows on the pointer's display.
- Align toolbar labels with the file-list/address text.
  [Presentation changes and checks](docs/window-presentation.md).
- Document direct Finder context-menu extension requirements and alternatives.
  [Finder integration](docs/finder-integration.md).

## 0.2.2 — 2026-10-06

- Keep build and rollback copies out of automatic Finder Open With candidates.
- Add a guarded installer/update path for one registered application, retaining
  a recoverable old copy and rejecting downgrades.
  [Registration, installation and checks](docs/finder-registration.md).

## 0.2.1 — 2026-10-06

- Fix the blank File Manager window left after standalone Open With → Extract
  Here, including hidden-parent Cocoa sheet handling and idle-exit lifecycle.
  [Extraction hotfix and checks](docs/open-with-extract-here-fix.md).

## 0.2.0 — 2026-10-06

- Establish the packaged/verified Windows-style File Manager baseline with the
  portable command/settings groups and documented platform differences.
- Align address-bar text and correct versioned Finder Open With routing.
- Repair ZIP extraction when timestamps are omitted.
- Complete available desktop menu/Options and Finder compression / Test /
  extraction checks. Physical drag, hardware modifiers, real printers and exact
  Windows pixel comparison remain verification limits.
- Publish the reviewed source repository with English/Japanese documentation.

Records: [0.2.0 package](docs/release-0.2.0.md),
[consolidated verification](docs/release-consolidation.md),
[desktop follow-up](docs/desktop-follow-up.md),
[Finder acceptance](docs/finder-desktop-acceptance.md),
[source publication](docs/publication-complete.md).

When upgrading from an explicit saved shell-command list, enable the independent
**Open archive** submenu in Options if wanted; existing choices are retained.
