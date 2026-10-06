# Current implementation and release inventory

This is the current inventory for the source-defined implementation groups
through the TextPairs source refactor transfer/menu restoration and application-local Windows access keys. Older component reports preserve executed
evidence, including failures; their former remaining-work lists are historical.
Full parity and publication are **not declared complete** by this inventory.

Implementation status, defects and verification limits are separate fields.
An untested format variant or physical input is not automatically an omitted
command. An upstream empty body, commented experiment or E_NOTIMPL is not a
portable Windows feature that needs to be invented in this port.

## Source groups connected to the application

| Group | Current implementation | Latest source/evidence |
|---|---|---|
| Open / Inside / parser / Outside / View / Edit | Original multi-item order, extension profile, native selection, editor-process observation, guarded nested write-back and shared panel lifetime connected | panel-open-port.md, open-profile-port.md, external-process-port.md, archive-refresh-port.md, panel-key-port.md |
| Copy / Move / overwrite | Original Copy resources, focused dispatch, six overwrite choices, native copy names and exact Agent input enumeration connected; filesystem transfer uses macOS file operations | copy-workflow-port.md, copy-projection-port.md, filesystem-transfer.md |
| Archive mutation | Original Agent CreateFolder, Delete, Rename, Comment and UpdateOneFile connected using real item identities and prefix boundaries | native-agent-item-updates.md, native-agent-comments.md, native-agent-replacement.md, native-folder-update.md, archive-prefix-updates.md |
| Extraction | Original pre-stream overwrite/Skip, ordered publication, link/path mapping, metadata finalization, partial failures and process/shutdown recovery connected | normal-extraction-integration.md, extraction-path-combinations.md, extraction-lifecycle.md |
| Listing / columns / Properties | Original proxy graph, typed/raw schemas, sorting, names, filesystem column policy, native index operations and large-list/icon workers connected | native-agent-properties.md, native-agent-item-updates.md, panel-sort-port.md, panel-listing-port.md |
| Selection / keys / address / creation | Original selection and full list-key dispatcher, focused F keys, header navigation/dropdown, original relative/absolute filesystem creation and rename policies connected | panel-selection-port.md, panel-key-port.md, address-workflow-port.md |
| Menus / context / drag | Original File-menu filtering, optional VerCtrl, drag effects, two-panel transfer and asynchronous archive drag-out connected | panel-menu-drag-port.md, version-menu-time-port.md |
| Add / Extract / Options | Portable settings, format tables/calculations, enable/reset rules, resource coordinates, language IDs and Apply/Cancel connected | settings-coverage.md, compression-controls-port.md, compression-options-port.md, dialog-resources-port.md |
| Progress / Test / CRC | Original presentation, completion policy, error formatter, Test/Hash results, Pause/Cancel/Background connected | progress-presentation-port.md, progress-completion-port.md |
| Help / About / temporary browser / Benchmark | Original resource/body adaptations, reusable Help, search/print, live temporary-file protections and native benchmark callback connected | dialog-text-help-port.md, temporary-files-port.md, benchmark-port.md |
| Mac Open With | Configurable source-style command menu and final File Manager entry, queued file-open dispatch and registered document types connected; dialog/context presentation and CRC builders shared and verified as one group | finder-integration.md, archive-open-modes.md, desktop-follow-up.md |
| Icons / engine / formats | Official File Manager ICO converted to ICNS; official handlers/codecs/crypto used; 61 handlers, 138 extensions, 151 registration pairs | archive-formats.md, format-coverage.md, licenses/NOTICE.md |

These rows describe connected code. The available consolidated automated gate
is covered in [release-consolidation.md](release-consolidation.md), including
initial failures and affected repairs. Its combined counts are not a claim of
one uninterrupted run or physical desktop verification.

## Remaining implementation differences

| Item | Classification | Concrete difference |
|---|---|---|
| Port-only explanation/error text | Partially implemented | Official 92 language files and resource IDs are used; some additional Mac explanations and raw process/OS diagnostics are English. This does not mean an Options setting is unavailable. |
| Malformed/ambiguous filesystem comment data | Partially implemented | Original TextPairs parsing, heap ordering, binary lookup, pair updates and saving are imported. Guarded descript.ion writes reject ambiguous duplicate IDs, unrepresentable names and unreadable data rather than silently rewriting it as Windows may do. These protections are documented in file-comments-spec.md. |
| Archive update safeguards | Partially implemented | Original Agent tail/layer rules and handler capabilities are distinct from added error/path/transaction safeguards. Writable handler warnings are now accepted; the exact owners and current signed/unsigned ZIP behavior are listed in [update-safeguards.md](update-safeguards.md). |
| Windows host mechanisms | macOS difference | Registry/Explorer, MAPI, PE SFX, large-page privilege, host NTFS security/ADS and Zone.Identifier. Portable NTFS/PE readers are included. |
| Filesystem deletion / startup / input | macOS difference | Trash, macOS permissions/title bar/signature and OS function-key settings. |
| Finder Extension / Quick Action / Services | Not implemented | Separate optional integrations; the requested configurable Open With replacement is implemented. |

Known defects are identified by a reproducible failure in connected code, not
by these historical limitations. At the start of the consolidated release run,
no earlier scoped failure is being left open as a known crash/data-loss defect.
New release failures must be recorded and repaired before release completion.

The latest complete release run executed 35 suites (32 initial passes and three
failed groups repaired by affected follow-ups), plus all 151 format registrations.
Transfer, address, keys and native menus passed after the final panel-state repair.
The initial failures and test-driver failures remain recorded in release-consolidation.md.

The current 18-item acceptance map is in [final-audit.md](final-audit.md).
Old descriptor-refusal/input-limit notes in the component reports have been
corrected against the connected ZIP writer. Historical logs and result counts
are preserved; they are not rewritten to claim a later test ran earlier.

## Verification and release gates

The original 18-item acceptance list remains required. Scoped and consolidated evidence
covers application generation/startup, filesystem/7z/ZIP browsing, 7z/ZIP
creation/extraction/Test/password/Japanese names, recoverable errors,
nonblocking cancellation, source-style main UI, parity documentation and
documented build/test instructions. The empty-build and combined verification
results are recorded in release-consolidation.md.

The consolidated gate uses an initially empty Release/Ninja build, all registered
available application suites, the native Agent tree/selection fixtures, all 151
format registrations, strict bundle dependency/signature checks and isolated
bundle startup. Use `scripts/verify-release.sh --no-focus BUILD` while locked;
it omits only the explicitly unavailable native focus/AppKit suite and records
those as pending. Every independent release stage runs once and retains its log.
Rerun only failed or changed paths after repair.

The [desktop menu follow-up](desktop-follow-up.md) repaired an empty 7-Zip
Options tab and passed the three affected UI suites plus native AppKit
menu/right-button/Options Apply/visible-manager FileOpen checks. The new
packaged app's Tools → Options and repaired caption were observed through the
desktop UI. Finder Other → explicit current app now displays the operation menu
and final Manager entry. The later packaged candidate completed desktop Open With 7z/ZIP creation, ZIP Test/extraction, final Manager opening and F5 Copy/Cancel; its entry was CLI, not Finder.
Qt's macOS-default-disabled resource access keys are enabled; all six menus passed
native Alt delivery. The final `07968cd` package was rebuilt from an empty directory,
passed private startup and bundle checks, and visibly opened Tools → Options via
Alt+T then O in the actual desktop application. Cancel returned normally; the
owned process exited 0. These observations do not claim hardware keyboard input.
At that earlier baseline, final-package Finder routing/context commands/drag,
physical modifiers, printing and visual comparison were unverified. Later Finder
routing/command results are recorded below. Native AppKit delivery and Qt checks
remain distinct from physical hardware or Finder drag observations.

The source/provenance/license and privacy preparation is recorded in
[publication-review.md](publication-review.md). A local exporter preserves
original source/license bytes and keeps internal diagnostic history private.
Final desktop acceptance, the release commit and the conditionally authorized
public repository remain pending. The bundle uses QtBase
frameworks/plugins; QtSvg is not bundled. QtBase corresponding source, Cocoa
patches and original 7-Zip/Port sources are included, with normalized source
file modes and a source SHA-256 manifest.

## Distribution preparation

The reviewed local [distribution candidate](distribution-candidate.md) includes
a signed standalone app, ZIP/SHA-256 and a separate 803-file public-source Git
snapshot with one English commit and no internal history. Unpacking, CRC, all
source hashes, strict signatures/dependencies and private-profile startup passed.
Runtime code is unchanged from `07968cd`; no engine/format regression was repeated.
The latest external Finder/coordinate attempt was rejected by the UI service.
Complete desktop acceptance and conditional publication remain pending.

The subsequent [address text adjustment](address-font-adjustment.md) aligns the
combo/editor/popup with the actual file-list font (9pt/12pt → 13pt on this Mac).
The application built and native Qt runtime metrics matched. Its application
revision supersedes the earlier distribution snapshot for this visual change;
no engine/format code or previously verified command behavior was changed.

Its updated package (`5aba56d`) passed bundle checks and actual desktop launch;
a fresh screenshot confirmed the address/list alignment. The earlier distribution
ZIP remains an older snapshot. The current reviewed publication-source snapshot
has 807 files. During resumed desktop work, Open Outside also opened the owned
fixture folder in Finder successfully; that resolves the earlier Finder-window
acquisition limitation, without declaring the remaining desktop gates complete.

Actual Finder Open With → explicit current app completed quick 7z and ZIP
creation; both outputs passed bundled-engine Test/extraction/SHA-256 checks.
The generic Finder candidate selected an older 0.1.0 copy, and explicit ZIP
selection remained disabled. Candidate metadata/About versions now share the
CMake 0.2.0 value; the affected build/dialog suite passed. Routing correction
was subsequently observed through Finder's explicit versioned 0.2.0 entry:
GUI Test, Extract to with SHA-256 equality, final Manager and parent navigation
completed. This does not claim the earlier disabled Other chooser was fixed. See
[finder-desktop-acceptance.md](finder-desktop-acceptance.md).

The final desktop group also reproduced an actual ZIP omitted-timestamp defect:
the pristine upstream console writes an APFS-invalid access time for a zero
NTFS timestamp, and the port applied a zero creation property. The native
extraction adapter and guarded publication now leave those blank timestamps
unset. The regression and existing valid metadata cases passed, followed by the
complete affected extraction suite (19.95 seconds). See
[zip-omitted-times.md](zip-omitted-times.md). This isolated repair does not
invalidate unrelated consolidated command/format verification.

The repaired 0.2.0 candidate completed a new initially empty 252-task build,
signature/dependency checks, bundled regression and actual final-app desktop
address/listing/extraction. Its restored content and timestamps matched the
required behavior, and the address/list font alignment was visible. The exact
observed macOS input-method diagnostic is retained separately from unexpected
stderr; earlier failed checks are not relabeled as passes. See
[release-0.2.0.md](release-0.2.0.md). Conditional publication awaits GitHub
authentication; no public repository has been created.
