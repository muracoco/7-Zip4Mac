# Automatic-refresh flicker fix — 0.2.4

Watcher notifications previously called the same full Refresh path as a manual
refresh: the toolbar/status entered Working, the entire list/header was replaced,
and native icons were requested again even when the folder contents had not
changed. The focused reproduction on 0.2.3 observed one model reset after a
notification with unchanged files; the zero-reset regression failed as expected.
This reproduces the redraw mechanism, not a claim to have identified every
possible external source of notifications on the user's disk.

Automatic filesystem refresh now reads a candidate with the existing asynchronous
scanner without changing the visible model or busy presentation. It compares
path, Flat mode, entry order and all displayed metadata (including exact time
fractions, links, comments and sizes) with the last installed snapshot. Unchanged
candidates are discarded. Changed/error candidates use the existing committed
read/error path; current selection is captured when the candidate commits.
Navigation/close cancels pending probes. Modal menus/dialogs, rename editing and
active jobs defer a changed candidate instead of changing its view mid-operation.

The filesystem watch is retained while its path remains registered, instead of
removing and re-adding it after every refresh. Actual list replacement suppresses
painting of the shared Details/icon view until the batch is installed. Manual
Refresh still reloads explicitly, and the Auto Refresh preference is honored.
Archive relisting and the existing archive update/exit protections are retained.

## Executed verification

An affected incremental Release/Ninja build was used on macOS 26.6.2 / arm64,
with Qt 6.11.3 and official 7-Zip 26.03. The baseline failing regression is retained
locally. The affected six-suite batch passed: `auto_refresh`, `directory_scanner`,
`cocoa_presentation`, `cocoa_open_with_progress`, and standalone 7z/ZIP exit checks.
The 30,000-entry asynchronous directory/cancellation/error checks are part of the
existing directory suite. A second full archive-format matrix and another empty
build were unnecessary for this UI refresh change and are not claimed.

The new regression checks five unchanged notifications, actual background scan
completion, zero model resets/toolbar busy changes, stable item identity,
selection and scroll. Native macOS filesystem notifications reflect creation,
overwrite, rename and deletion of generated files with Japanese/space names.
Auto Refresh off leaves a new file undisplayed until manual Refresh.
All fixtures/settings are isolated in temporary directories. No existing user
file is overwritten or deleted. The six suites passed in 35.79 seconds; the directory suite reported 13 passing
cases and the automatic-refresh suite reported two passing cases plus setup/teardown.

The self-contained 0.2.4 package launched on the actual Mac desktop at `/`.
Two native accessibility observations 68 seconds apart retained the same 21 row
identities and showed no Working state. The list/toolbar display was inspected.
This is a sampled desktop observation, not a continuous video/frame-rate test.
The direct model-reset/busy-state measurements are from the isolated regression.
Packaging verified 15 Mach-O files with system/@rpath dependencies only and a
strict ad-hoc signature. [Machine-readable evidence](idle-refresh-evidence.json).

```bash
PORT_TEST_REGEX='^(auto_refresh|directory_scanner|cocoa_presentation|cocoa_open_with_progress|cocoa_open_with_exit_7z|cocoa_open_with_exit_zip)$' ./scripts/test.sh /absolute/build/path
./scripts/install.sh /absolute/build/path
```
