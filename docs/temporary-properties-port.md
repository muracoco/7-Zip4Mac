# Original temporary-file Properties port (2026-10-05)

Temporary Properties now runs a fresh helper rescan instead of displaying the
cached list cells. import-temp-browser.py pins BrowseDialog2.cpp and
OverwriteDialog.cpp and imports PrintFileProps, PrintProps helpers and
AddSizeValue into TempBrowseProperties.inc. Original names, attribute text,
folder/file counts, raw byte sizes, KiB/MiB/GiB thresholds, modification time,
interrupted '+' markers and single-child detail blocks are retained.

The class name, POSIX attribute/link accessors and FILETIME conversion are
adapted. POSIX directory storage size is excluded, matching Windows directory
FindFile size. Official property labels and the '{0} bytes' resource are
provided by the current language; names and values are not translated.

The rescan runs asynchronously in the bundled helper. Missing listed items
refresh the browser with a diagnostic. Links and replaced parent/item
identities are refused. Reader teardown disconnects callbacks and kills only
owned processes without waiting in the dialog. Permission failures retain
incomplete counters and an available error message.

Show_FileProps_Window uses a plain MB_OK message, unlike the main manager's
2-column Properties dialog. The port now supplies a plain Widgets dialog with
the Properties title and a centered single OK button. QMessageBox silently
removes its title on macOS, so it was replaced after an actual regression
failure. Text stays selectable and literal, including '<b>' in filenames.

## Executed checks

Temporary browser: **16 pass, 0 fail/skip**, including setup/cleanup. Four new
cases cover fresh counts after contents change, directory/read-only file
attributes, Japanese labels and size resource, the 2,000-file limit, missing
item refresh, and five closes during Properties followed by successful reuse.
The native metadata formatter's 11 checks and shared progress's 15 checks
also pass. Neither of those passing suites was repeated after the title-only
Widgets correction. Native helper/application/test builds pass without new
compiler warnings. [Captured output](temporary-properties-test.log).

Exact native dimensions, grid/name-only selection painting and physical
click/Fn checks remain in the UI inventory. Final clean build, license/privacy
review, complete portable parity and public release remain unfinished.
