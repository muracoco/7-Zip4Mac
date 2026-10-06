# Completion and publication plan

The active request is portable Windows 7-Zip parity, retaining the requested Mac
Open With menu and necessary platform substitutions. The audited portable
source groups, tested local 0.2.0 package and public source publication are
complete; hardware/pixel equivalence is not claimed. Git history
and the linked component reports retain earlier milestones and first failures.
Superseded remaining-work lists in those reports must not create new work.

## Authoritative scope and status

- [current-status.md](current-status.md): connected implementation groups,
  genuine behavior differences and remaining release gates.
- [final-audit.md](final-audit.md): original 18 acceptance conditions and
  command inventory.
- [settings-coverage.md](settings-coverage.md) and
  [windows-parity.md](windows-parity.md): settings and Windows comparison.
- [release-consolidation.md](release-consolidation.md): executed consolidated
  build/test evidence, including initial failures and affected repairs.

These inventories preserve the original scope: all portable commands/settings,
original engine/handlers/codecs/crypto, Windows-style UI and icons, all registered
formats, asynchronous failure/cancel recovery, reproducible self-contained app,
licenses, final verification and conditional public publication.
Upstream empty/commented bodies and E_NOTIMPL are not enabled Windows features.
A format variant or physical input without evidence is unverified, not an
unimplemented command. Added protections remain behavior differences rather
than being renamed macOS limitations.

## Implementation and test sequence

For each source-defined feature group:

1. Compare the original command bodies, resources, state/enable rules and
   defaults. Identify missing implementation before editing.
2. Complete related commands, state, backend/GUI bindings and documentation
   together. Reuse portable official bodies before implementing adapters.
3. Run the affected group once after implementation. Preserve the first result;
   repair reproduced failures and rerun only failed or changed paths.
4. Record the result and commit the group. Passing unrelated suites and the full
   format matrix are not rerun for a small isolated change.

Use separate status fields: **Not implemented**, **Implemented but unverified**,
**Verified**, and **Reproduced defect**. Keep tool/launcher failures separate from
application defects. Do not invent further feature scope from arbitrary untested
combinations. New crash/data-loss defects require repair; optional architecture
or speculative cosmetic changes do not postpone an otherwise complete group.

## Latest completed groups

- Desktop Options caption repair and shared Open With/context/CRC presentation:
  [desktop-follow-up.md](desktop-follow-up.md).
- Original writable-handler warning policy, native mutations and GUI enable
  rules: [update-safeguards.md](update-safeguards.md).
- Original filesystem TextPairs parser, heap ordering, lookup, updates and saving,
  with guarded macOS file I/O: [file-comments-spec.md](file-comments-spec.md).
  The affected comment group passed once, 32/32 Qt cases including setup/cleanup.

- Transfer refresh/selection/focus now follows original OnCopy and LastFocusedPanel;
  the affected Copy/address/key/AppKit groups and failed integration/Link cases
  passed. The latest 35-suite release run and retained first failures are recorded
  in release-consolidation.md.

Other connected groups are indexed in current-status.md. Agent write-back,
extraction overwrite/lifecycle, listing/columns, full key/address/creation/Copy
projection, portable settings, dialogs, Help, Benchmark and drag support must not
be reimplemented from their historical checkpoint lists.

## Final completed stages and retained limits

1. The actual versioned Finder candidate now completed ZIP Test/Extract to,
   SHA-256 equality, final Manager and parent navigation; Finder text-file routing
   also completed quick 7z/ZIP creation. Native AppKit menu/right-button/input and
   Qt context/drop verification remain separate from physical Finder drag or
   hardware modifiers. The UI service rejected a coordinate event before
   delivery. Those environment observations and Windows pixel comparison remain
   explicitly unverified; they are not additional missing command bodies or
   reasons to repeat passed portable feature groups. See
   finder-desktop-acceptance.md. The reproduced ZIP omitted-timestamp defect was
   repaired and its complete affected extraction suite passed (323 Qt cases).
2. The frozen `07968cd` source has completed its final initially empty
   Release/Ninja build: 252 tasks, 803 source files, private bundled startup and
   strict signature/dependency checks. The original 35-suite run, affected repaired
   paths and 151 format registrations are retained; no new full run is needed
   without a shared implementation change. Actual desktop Alt+T → O opened Options
   and Cancel returned to the list in this final package. Record subsequent
   documentation separately from the compiled revision; packaging documentation
   does not require rebuilding or rerunning unrelated application tests. The
   subsequent address typography, version metadata and extraction timestamp
   repair completed a new 252-task empty-build 0.2.0 candidate, followed by
   signature/dependency checks, bundled regression and actual GUI verification.
   See release-0.2.0.md.
3. Final local source/license/privacy export against `07968cd` passed for 803
   files, preserving code/license bytes and excluding internal history. Matching
   corresponding source is bundled. The repaired 0.2.0 distribution now contains
   the final reviewed 812-file snapshot; ZIP/source/signature/dependency/startup
   checks passed. The earlier 0.1.0 archive remains a historical artifact.
4. The conditionally authorized [public repository](https://github.com/muracoco/7-Zip4Mac)
   is published with English project material, Japanese README and bilingual
   description. The first complete remote snapshot matched the reviewed local
   812-file tree exactly after an independent anonymous Git fetch.
   [publication-complete.md](publication-complete.md) records the completed
   publication, retained first authentication/CLI push failures and package
   snapshot distinction. Subsequent closeout edits are documentation only;
   passing implementation/test groups were not restarted.

Do not claim physical input from Qt/AppKit test events, or a complete Windows
pixel match from source/resource comparison alone. External UI automation errors
are verification limits unless a reproducible application failure is observed.
No OS lock/security/default-association changes are required for this plan.
