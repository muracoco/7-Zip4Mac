# Compression Options and dynamic resources (2026-10-05)

The port continues to use official 7-Zip 26.03 code, rather than reimplementing
compression algorithms. This increment closes a group of Windows dialog state
and translation differences; it does not declare the entire application complete.

## Source boundaries

- `CompressDialog.cpp::g_Levels` is imported verbatim with symbolic resource IDs
  resolved against the pinned official `CompressDialogRes.h`. Level prefixes and
  labels are translated separately. Solid/non-solid choices use resource IDs
  4072/4073 and keep stable command values across language/format changes.
- The word-size label remains resource 4007 for PPMd too, as in the original
  resource/dialog. The previously invented `Order` label was removed.
- `CompressOptionsDialog.rc`, `SetPrec`, `SetTimeMAC`, and the checkbox handlers
  define precision choices, defaults, visibility, explicit/default pairs, and
  TAR GNU/POSIX and ZIP precision conditions. The Widgets adapter follows those
  branches, including the original default MTime control condition. Inactive
  per-format values remain saved but are not sent to unsupported handlers.
- `NativeMetadata::WriteFormats` exports the compiled `CArcInfoEx` time/link
  capabilities to an exclusively created generation file. `update-formats.py`
  reconciles every compiled handler with official registration source and embeds
  these capabilities in `formats.json`. The static upstream console loader
  omits `arc.TimeFlags`, whereas DLL loading reads them; the pinned overlay adds
  that assignment. Pristine upstream source and the pristine console stay intact.
- Explicit false for latest archive time is preserved in settings. The official
  console rejects `-stl-`; false therefore omits `-stl`, using its false default.
  The initial integration test exposed this and the adapter was corrected.

Windows NTFS alternate streams/security controls are omitted on macOS. Supported
symbolic/hard-link controls follow handler flags. Native pixel geometry and
physical menu/Fn/Finder interaction remain outside this scoped verification.

## Executed validation

Apple Silicon macOS 26.6.2, Apple Clang 21, Qt 6.11.3, source-built 7-Zip 26.03.
Native helper and application/test targets built successfully. Final affected
results: **72 passed, zero failures/skips** (Qt setup/cleanup included):
compression/Help 46, native progress 15, native metadata formatter 11.

The compression suite includes real round trips for all offered methods/formats,
correct/wrong encrypted passwords, Japanese/space paths, GUI validation,
language/format switches, stable manual values, GNU/POSIX time conditions,
explicit latest-time false persistence and real output bytes/timestamps.
Progress covers failure/Cancel/Pause and reuse. Passing formatter/progress checks
were not repeated after the console-switch-only correction.

See [compression-options-test.log](distribution.md). This is an
incremental build and affected validation, not the final empty-directory release
build or the physical desktop acceptance gate.
