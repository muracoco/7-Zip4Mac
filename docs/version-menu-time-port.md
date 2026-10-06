# Official version-store, menu and time port

This is one source-port batch: the four optional version-store commands,
complete File-menu filtering, and the dynamic Time menu/rendering. Implement
these related functions before running their affected tests together.

## Imported source

- Official 7-Zip 26.03 `VerCtrl.cpp`: all portable helper and `CApp::VerCtrl`
  bodies are retained by `scripts/import-version-control.py`, SHA-256 pinned.
  This is the original `_7vc` file backup system, not Git integration.
- `MyLoadMenu.cpp`: the full `CFileMenu::Load` body now includes its optional
  version-command tail and original command labels/order. The single adapter
  change copies source-action mappings when constructing the destination menu.
  The same filter also supplies main File-menu disabling when it opens.
- `PropVariantConv.cpp/.h`: the original char FILETIME formatter, precision
  constants and Time-menu precision array are retained by
  `scripts/import-time-menu.py`. Qt supplies calendar/time-zone conversion.

## Behavior

`Ver Edit` snapshots a read-only file and makes the original writable.
A differing previous snapshot moves to `_7vc/<filename>/001`, then the next
numeric name. `Ver Commit` follows the original timestamp flooring and marks
read-only; it does **not** overwrite the snapshot. `Ver Revert` restores its
bytes, times and available permissions. If bytes differ it asks Yes/No/Cancel,
without bulk choices, with **No** initially selected. `Ver Diff` passes snapshot
then original to the configured Diff command. Operation execution requires
exactly one marked item; unmarked focused-only invocation is the original no-op.

As upstream, these optional menu items appear only with Diff and version-store
configuration, one filesystem file under 2 GiB, and the appropriate read-only
state. The implementation itself retains upstream's 256 MiB whole-buffer limit.
No extra Options field is invented: Windows also configures the store only
through its File Manager registry string `7vc`. The equivalent is the root
QSettings string key `7vc`; normal Diff remains in Options / Editor. Use an
absolute local version-store folder. Existing Options saves preserve this key.
Qt's native preferences filename was inspected without changing its contents.
With the app closed, the equivalent optional configuration is:

```bash
defaults write 'com.sevenzipmacport.7-Zip Mac Port' 7vc -string "$HOME/Documents/7vc"
```

Choose your own store path, then configure Diff in Options / Editor. Tests use
isolated preferences and disposable files; they do not set this user preference.

The View submenu title is today's date; its five choices show current samples
at day, minute, second, 100 ns and 1 ns precision. UTC shows the original `Z`
suffix. The initial precision is **minutes**, matching `CPanel`'s constructor;
previously saved settings are kept. Date formatting for filesystem/archive
lists shares this source formatter. UTC remains a Mac-port setting, independent
of OS timezone preferences.

## Platform adapters and safeguards

File reads/writes, numeric history enumeration, file times and read-only flags
use POSIX and the existing guarded FileInstall handles. NTFS metadata has no
POSIX equivalent; Unix mode, access/modification and supported creation times
are retained. File symlinks are refused for this version-store operation to
avoid altering their targets. This is recorded as a protection policy, not
an unimplemented ordinary archive reader.

I/O runs asynchronously. Pause/Cancel and confirmation replies never block the
GUI thread. Writes stage beside the destination and publish through the existing
identity-checked installer; cancellation/I/O failure cannot truncate the approved
original. Already moved numbered history files remain recoverable. Adapter
errors stop the operation instead of continuing after original void WriteFile's
error dialog. Errors retain operation and target; there is no engine exit code
because these are File Manager file operations.

## Verification

The Apple Clang / Qt 6.11.3 build passed. Grouped affected checks are **112
passed, 1 skipped** (including setup/cleanup): version/menu/time 11, menu/drag 34,
selection 16, directory scanner 13, Properties 19, overwrite 19. The skip is the
existing cross-volume Move case, which requires `PORT_TEST_SECOND_VOLUME`; this
batch does not change that implementation. Initial Diff checks exposed duplicate
slashes and the standard `/var` → `/private/var` alias in the store path; Diff
arguments are now cleaned, and the corrected source/GUI checks passed.

Actual-file checks cover readonly Edit snapshots, Commit timestamp flooring,
numbered history, No/Yes Revert with bytes/time/mode restoration, the actual
external Diff argument order via an owned shell witness, same-data timestamp
recovery, missing snapshots, the original size limit, refused symlinks, changed
original protection, and 32 MiB Pause/Cancel with event-loop heartbeat and retry.
Qt GUI checks verify visibility conditions, marked-only execution, source-menu
cloning without duplicate version items, the three-button default-No Revert,
dynamic date samples, five precisions, UTC `Z`, and the minute default.
The self-contained `.app` was packaged after validation: all 15 Mach-O files
have only system / `@rpath` dependencies, and deep/strict ad-hoc signature
verification passes. With isolated preferences and no development Qt/DYLD
paths it stayed alive for 3 seconds; startup stderr was empty. Only that owned
process was terminated. Physical desktop interaction remains separate from
Qt event-based tests.
This batch does not declare the full portable-parity or final-release goal done.
