# Dialog text, About, temporary files and Help window batch

This is one source-defined UI batch: finish the affected dialog geometry,
translation sizing and Help lifetime together, then run the related checks.
It does not mark the final Windows-parity/publication objective complete.

## Source and implementation

The official 7-Zip 26.03 `AboutDialog.rc`, resource header and initializer,
`BrowseDialog2.rc`/header and `CBrowseDialog2::OnSize`, and `HelpUtils.cpp`
provide the specification. `import-dialog-geometry.py` verifies the additional
upstream hashes. The original temporary-browser resize body is imported
unchanged into `TempBrowseLayout.inc`; a small Qt adapter handles the Win32
control operations and Qt button-box ownership. Anonymous static resource
controls have distinct generated identities, preserving both the logo and
copyright controls. Upstream attribution and source copies are retained.

- About uses the original 110×63 logo with SS_REALSIZEIMAGE behavior, version/date/copyright/info rows, homepage and
  default OK button at their resource coordinates. The original About/title/info
  language IDs are bound. F1 opens `start.htm`. An explicit unofficial Port and
  licensing notice occupies unused space beside the real-size logo, with the
  complete licensing/source location in its tooltip.
- Delete Temporary Files uses the original Delete/Refresh/parent/path/list/
  filter/Close/Help positions and original resize calculations. Close and Help
  have no default-button designation. The existing bounded enumerator, property
  formatter, navigation, selection, protected paths and confirmed Trash operation
  are retained. Its Port status/error area occupies unused top-row space; the
  complete message is available in the tooltip and failure signal.
- Resource dialog units still preserve the source's coordinate relationships.
  Polished-widget/paint-device font and translated-text metrics enlarge the shared unit only as
  needed to fit labels and multiline checkboxes. Literal paths never enlarge the
  window horizontally; known literal About labels/homepage participate in text
  fitting. Static text retains the source top alignment. Language/font changes recalculate geometry; resizable
  dialogs retain the user's size proportion. Labels use plain text so filenames
  and translated strings are not interpreted as HTML. No Microsoft font is
  copied or bundled.
- `HelpUtils.cpp` documents that the regular `HtmlHelp(NULL, ..., 0)` path uses
  one window; its alternative external-help path is not enabled in the regular
  build. All Help actions now reuse one independent Qt help window. Closing the
  caller leaves it open; closing Help permits a fresh window later. Topic
  navigation keeps Back/Forward history. A transient native window relationship
  allows use from a modal Qt caller without giving that caller QObject ownership.
- Temporary-file Help (`fm/temp.htm`) was already bundled and connected. Earlier
  inventory entries describing it as missing were stale and are corrected here.

## Validation

The grouped build/check results are recorded in [the raw log](dialog-text-help-test.log).
Environment: macOS 26.6.2 / Apple M3 arm64, Apple Clang, Qt 6.11.3.
CMake/Ninja built the app and every configured test target. The initial grouped
run passed five suites and exposed two dialog failures: button-box rounding and
an uninitialized/unsupported test icon resource. These were corrected. Rendered
widgets also exposed literal About text/real-icon clipping and label font/buddy
measurement differences; those were corrected together before rerunning the
three suites affected by shared geometry changes.

Latest per-case results, counting each suite's setup/cleanup once: **113 passed,
0 failed, 0 skipped**. Dialog resources 10; language/settings 10; compression/Help
47; temporary files 16; progress presentation 15; progress completion 15. This
combines the first grouped run and targeted/affected reruns; it does not imply all
six suites were repeated after every edit. The raw log preserves the failed run,
targeted repair and affected rerun. The passed CTest suites' initial Qt totals
were also observed from `Testing/Temporary/LastTest.log` before CTest replaced it.

Text checks run on polished, visible widgets, with their paint-device font
metrics, actual style content rectangles and source mnemonic behavior. They
cover 93 supplied language choices across Add, Extract, About, Progress and
compression Options, plus a larger substitute font. Retina-resolution widget
renders were inspected for Japanese Add and English About. This verifies text
extent/geometry, not the correctness of every script's shaping or physical
Windows pixel identity. Some fallback-font OpenType diagnostics were emitted;
complete glyph and physical display comparison remain unconfirmed.

The affected group is `dialog_resources`, `language_settings`,
`compression_help`, `temporary_files`, `progress_presentation` and
`progress_completion`. New checks cover official About/temporary geometry,
multiple resize sizes and button defaults, text fit across the 93 supplied
language choices, alternate font sizing, compression Options text and Help
reuse/parent lifetime/history. Existing group checks cover actual archive
operations, settings persistence, errors, cancellation, status and completion.
No unrelated complete-format matrix or final clean build is run for this batch.

```bash
source scripts/env.sh
BUILD="$DEPS/build-comments-20261004"
cmake --build "$BUILD" -j 6
PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" ctest --test-dir "$BUILD" \
  --output-on-failure -R '^(dialog_resources|language_settings|compression_help|temporary_files|progress_presentation|progress_completion)$'
```

## Parity classification and remaining work

| Item | Classification | Scope/limit |
|---|---|---|
| About resource arrangement and original actions | Implemented | Official control positions, language IDs, homepage, OK and F1 |
| Temporary-file dialog layout and resizing | Implemented | Original resources and unchanged OnSize; Port status is additional |
| Translated resource text sizing | Implemented | Font/label/checkbox metrics; physical pixel comparison remains unverified |
| One reusable Help window and contextual topics | Implemented | Independent lifetime, history and modal transient relationship |
| HTML Help window chrome and OS linguistic search/ranking | macOS difference | Windows supplies `HtmlHelp` from its OS; this is not a missing portable 7-Zip algorithm |
| Help content language | macOS difference | Official supplied CHM/HTML content is English; 7-Zip Lang files translate application controls, not these pages |
| Port-specific explanations/errors | Partially implemented | Some strings have no official resource translation |
| Physical Finder/click/Fn/print/pixel comparison | Partially implemented | Automated Qt events and PDF printing do not prove physical input or printing |
| Final release/publication | Partially implemented | Final clean build, acceptance, license/private-data review and publication still required |

The updated app is packaged at
`/DEPS/build-comments-20261004/7-Zip Mac.app`.
Packaging completed: 15 Mach-O files have only system/@rpath dependencies,
and deep/strict ad-hoc signature verification passed. With development
QT/DYLD/test-plugin paths removed and owned temporary preferences, the bundled
executable remained alive for three seconds while browsing an owned Japanese/
space-named folder; stdout/stderr were empty. Only that process was stopped.
See [the bundle log](dialog-text-help-bundle.log). This remains a development
checkpoint; the final clean build and full acceptance/publication gates are
separate. QtSvg/image-plugin corresponding-source coverage is still part of the
final license/package audit, not a claim established by this UI batch.
