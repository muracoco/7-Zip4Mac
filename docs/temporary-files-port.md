# Temporary-file browser

Tools → Delete Temporary Files is now enabled. The reference is official 7-Zip
26.03 `UI/FileManager/BrowseDialog2.cpp`, `BrowseDialog2.rc` and the size formatter
in `BrowseDialog.cpp`. `scripts/import-temp-browser.py` checks both original
SHA-256 values and imports the bounded enumerator and size formatter into
`src/upstream/TempBrowseEnumerator.inc`. The only enumerator-body substitution
is the POSIX symbolic-link accessor. A small wrapper supplies the original
Windows enumerator interface using the official POSIX file-finding API.

## Behavior

- Delete / Refresh, parent button and read-only path, six report columns,
  disabled filter combo, Close and contextual Help follow the upstream resource.
- The columns are Name, Modified, Size, Files, Folders and Name-2. Initial sorting
  is Modified ascending, with directories first. Column clicks and Ctrl+F3/F5/F6
  use the original sorting rules; Ctrl+R, Ctrl+A, Backspace, Enter, Shift+Enter,
  Alt+Enter, Delete and Esc retain their purposes. Physical Fn-key checks remain.
- Root filtering accepts precisely case-insensitive `7z[E/O/S]` plus eight hex
  digits. It also recognizes this port's explicit Qt temporary-name patterns.
  Inside a displayed directory all children are shown. Navigation above the
  temporary root and opening symbolic links are refused.
- The original 200-directory / 2,000-file limits, interrupted `+` values,
  single-child Name-2 and size-unit thresholds are retained. Permission failures
  produce incomplete per-item counters and available error details without
  hiding the other readable temporary items.
- Show Dots, Single Click, Full Row, Grid and native-icon preferences apply.
  The title and Name-2 use the official language strings. The right-click menu
  includes Delete, Open Outside, Open Outside : 7-Zip and Properties, using the
  upstream selected/multiple/no-selection conditions. The 7-Zip action launches
  the application separately with its existing `--file-manager` route.

## macOS adaptation and protection

Deletion prompts with No as the default and moves confirmed items to the Trash.
The Windows implementation permanently removes them. No permanent-delete
command has been added. The listing retains no-follow file snapshots; confirmed
items that changed after listing are refused. The same-process live editor,
nested, retained drag and backend directories are protected. Active operations
also protect the system temporary root. Other application instances retain
Windows' need for manual review; moving to Trash is reversible.

As upstream documents, temporary data stored outside the system temporary
folder is not shown by this browser. Opening outside uses Finder/default apps;
filesystem link types and attributes retain POSIX meanings.

Reading runs in the bundled helper and deletion on the existing file worker.
Closing a reader disconnects it from the UI, kills it and disposes it after
termination without waiting in the dialog. An early Qt test exposed an actual
lifetime crash: QProcess destruction emitted its finished callback after the
list widget was destroyed. The dialog's shutdown path was corrected. Five rapid
close/delete operations followed by a fresh successful browse now pass.

## Executed verification

On the current arm64 Mac, **57 affected checks** pass, including setup/cleanup:

- Temporary browser: 12. Owned fixtures verify the exact root filter, Japanese
  and space names, counts/limits/units, dots on/off, navigation and selection,
  sorting and keyboard actions, right-click request/menu order, single click,
  Japanese labels, link/root refusal, active and changed-item retention,
  default-No/Cancel, real multi-item Trash movement and retained bytes,
  unrelated-file preservation, missing-helper recovery, unreadable children,
  menu availability and close-while-reading reuse.
- Shared normal-engine progress/cancellation: 13; normal-directory scanner:
  13; Properties: 19. These source paths were checked after the initial shared
  integration. Later repairs are limited to the new temporary browser/helper.

Actual Trash tests remove only the returned, identity-checked test-owned Trash
entries. No user's pre-existing Trash contents are removed.

```bash
cmake --build /path/to/build --target temporary_files_tests \
  native_progress_tests directory_tests property_tests -j6
ctest --test-dir /path/to/build \
  -R '^(temporary_files|native_progress|directory_scanner|properties)$' \
  --output-on-failure
```

[Final execution logs](temporary-files-test.log). These are Qt action/event
checks, not physical native-menu/Finder verification. Exact native dimensions,
grid/name-only selection painting remain in the UI parity batch. Properties
fresh rescan/full attribute formatting is now imported and verified; see
[temporary-properties-port.md](temporary-properties-port.md). Other extraction/list/Help gaps,
final refactoring, the clean release gate and publication remain incomplete.

The normal `.app` was rebuilt and packaged. All 15 Mach-O dependency/signature
checks passed. LaunchServices started the owned `--file-manager` instance,
which remained alive, loaded the bundled Cocoa plugin and ran the bundled
helper successfully. The private verification instance was then stopped.
