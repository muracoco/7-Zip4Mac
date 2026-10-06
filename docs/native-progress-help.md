# Official console progress and Help

## Reused source

The original source-built 26.03 `7zz` stays unchanged. Bundled `7zz-progress`
uses the same engine/handler/codec/crypto objects, command parser, passwords and
console operations. `scripts/import-progress.py` checks seven official hashes and
creates an external overlay, retaining upstream implementations and headers while
adding optional telemetry and the [CopyFrom adapter](archive-transfer.md).
The original source directory is never edited.

The extract console exposes `ICompressProgressInfo`, as the Windows GUI does;
the existing `CLocalProgress` forwards cumulative input/output. The original
selection loop additionally counts filtered nonfolder items. Unsupported directory
properties leave that count unknown without failing extraction. The included
generator recreates the native frontend from the bundled original source.

## Progress

Snapshots use an optional Unix datagram socket on child FD 3. Stdout/stderr/stdin
keep their original roles. The reporter never inspects passwords or arguments.
Current names use UTF-8 and the backend's existing password redaction. Sampling
is 200 ms, with forced phase/total/final snapshots. Nonblocking sends can drop
snapshots for full/closed/unavailable receivers without interrupting operations.
Records over 60,000 bytes retain metrics and omit the current name. UInt64 values
are decimal strings, preserving values above JSON double precision; unknown and
zero remain distinct. Invalid records are rejected. Final queued data is drained
before completion. Console percentage estimates remain a fallback if the channel
cannot be created, and retain the estimated label.

Windows `ProgressDialog2.cpp` mapping is retained:

- Compression: Processed = ratio input; Compressed size = ratio output.
- Extract/Test: Processed = ratio output; Compressed size = ratio input.
- No ratio: Processed falls back to the completion callback.
- Bar, speed and remaining time use completed / total separately, including
  update offsets.
- Files follows Windows callback counts, including failed/skipped nonfolder
  operations. Solid decoding can make the completed count exceed selected count.

These are callback values rather than percentages multiplied by input sizes.
ZIP total complexity includes header work; codec packed size does not always
equal physical archive size. Hash has no packed size. Missing handler information
stays unknown. Pause/Continue/Cancel retains QProcess signal control and the GUI
event loop.

Source-deletion verification worker Pause/detailed telemetry is now implemented;
see [the later transfer checkpoint](filesystem-transfer.md). Remaining: native desktop clicks, all handler/update combinations, dimensions and status wording.

## Help

The original 70 HTML pages, eight CSS files, Contents and Index remain unchanged.
Worker-based Search supports full text, phrases, `*`/`?`, uppercase AND/OR/NOT/NEAR,
parentheses, titles and previous-results filtering. Invalid/oversized expressions
produce an inline message. NEAR currently accepts eight intervening tokens.
Windows HTML Help's engine is outside 7-Zip source: similar-word matching uses
macOS NaturalLanguage and ranking uses occurrence count. Windows linguistics,
ranking and exact NEAR distance are unverified. Microsoft's
[Search-tab specification](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/htmlhelp/about-adding-a-search-tab-to-the-navigation-pane)
specifies full text and Boolean/wildcard/nested expressions without providing the
engine. Search history/operator insertion controls were subsequently implemented
and tested; see [benchmark-port.md](benchmark-port.md) for the current checkpoint.

Print offers the selected topic or TOC heading/subtopics, then the macOS print
dialog through Qt PrintSupport. Automated checks only write owned PDFs; native
print-dialog clicks and a printer are untested. About follows original
`AboutDialog.cpp`: logo/version/date/copyright, website button, F1 to `start.htm`,
plus unofficial Port identity. Temporary-cleanup context Help remains pending
because the cleanup command is unimplemented.

Qt PrintSupport belongs to the supplied LGPL QtBase source. NaturalLanguage is
a macOS system framework; no implementation or language assets are redistributed.

## Executed checks

- Help/compression: **16 passed / 0 failed / 0 skipped**. All original asset hashes,
  70 pages/links, query semantics/errors, real-page search, GUI topic selection,
  topic/subtopic PDFs, About website signal and contextual F1.
- Native progress: **9 passed / 0 failed / 0 skipped**. 32 MiB 7z/ZIP round trips,
  Japanese/space names, empty folder, callback values/counts, selected extract,
  Test/hash, encrypted 7z/wrong password, Pause/Continue/Cancel/reuse, GUI heartbeat,
  closed receiver, UInt64 transport and invalid values.
- Affected integration: **13 passed / 0 failed / 0 skipped**. Update/delete/rename,
  encrypted update, overwrite, error recovery, GUI Pause/Continue/Cancel, existing
  compression methods, TAR/WIM/streams and Open With.
- Context/RAR: **5 passed / 0 failed / 0 skipped**. Qt context-menu 7z/ZIP creation,
  Test/extraction/SHA-256 and genuine RAR/RAR5 fixtures.
- Original and instrumented `i` format/codec/hash tables match exactly.

Totals include setup/cleanup. Incremental Release builds passed. The final full
clean build/regression/151-registration matrix remains scheduled after remaining
functional batches. Physical Finder/menu/key checks wait for an unlocked desktop.

Run `ctest --test-dir <build> -V -R '^(native_progress|compression_help)$'`;
bootstrap/build/test scripts include the helper and suites.
