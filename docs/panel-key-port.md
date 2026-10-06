# Focused keys, relative Rename and two-panel navigation

This source-port group implements related commands before its grouped checks.
It does not declare full Windows parity or the final release complete.

## Official source and implementation

The importer pins official 7-Zip 26.03 PanelKey.cpp, PanelOperations.cpp and
BrowseDialog.cpp by SHA-256. PanelKeyCommands.inc retains the original F2–F7,
Alt+F1/F2 and Alt+Up/Left/Right dispatch blocks. Qt supplies key flags and the
existing command callbacks. Selection/Shift-arrow routines retain their already
imported PanelSelection bodies; the new adapter does not invoke them twice.

- F2 calls RenameFile on the focused row, independently of marks and File-menu
  enable rules. F3/F4 also dispatch to the original focused View/Edit workflow.
- Ctrl+Insert copies exact selected names, including literal POSIX backslashes;
  Shift+Insert preserves the original empty EditPaste behavior.
- Shift+F4 opens Create File. F5/F6 retain ordinary and Shift-focused Copy/Move.
  F7 opens Create Folder. Alt+F1/F2 focuses the first/second address field.
- Alt+Up sets the opposite panel to the source's current folder. Alt+Left/Right
  binds the source's focused folder or parent in the opposite panel. Native
  archive folder identity and shared nested temporary lifetime are retained.
  Destination nested write-back uses the established confirmation workflow.
  Original Alt-arrow dispatch returns false; normal list focus movement follows.
- Rename validates the final path component with the original IsCorrectFsName.
  Official non-Windows CorrectFsPath is an identity operation: trailing dots,
  spaces and a literal backslash are legitimate POSIX name characters.
  Relative filesystem destinations are resolved from the selected row's actual
  parent, including Flat rows. Parents must exist and installation stays exclusive.
- Archive Rename accepts safe multiple-component relative names, passing them to
  the imported original Agent RenameItem. Unsafe absolute/traversal/empty/dot
  components remain rejected. Payload and unrelated entries are preserved.
- Asynchronous list scans/operations restore displaced list focus after enabling,
  so Enter followed by Backspace keeps working. User input in another control
  cancels restoration; the opposite-panel active branch also enables both views.
- Original EditItem's folder statistics use operated selections. The previous
  unselected-focus directory fallback was removed; ordinary focused files still
  open through Viewer/Editor. Directory symlink browsing was already implemented
  and is verified in this group, not claimed as newly added functionality.

## Classification

| Area | State | Evidence / boundary |
| --- | --- | --- |
| Focused F2, F3/F4, Shift+F4, F5/F6/F7 | Implemented | Original dispatch blocks and existing guarded commands |
| Alt+F1/F2 address focus | Implemented | Source panel number preserved |
| Alt-arrow opposite-panel folder binding | Implemented | Filesystem/native archive/nested graph adapters |
| Relative FS and archive Rename | Implemented | Original name policy and original Agent updater |
| Unsafe archive names | Port safety policy | Original arbitrary unsafe names are refused to meet archive-path safety requirements |
| NTFS reparse and Win32 name normalization | macOS difference | POSIX symlinks and official non-Windows path behavior |
| Actual Fn/Option/Finder/AppKit input | Unverified | Qt events do not prove physical desktop interaction |
| Final clean build, all-format acceptance, source/licenses and publication | Not complete | Retained release requirements |

## Grouped validation

Run `./scripts/test-panel-key.sh <build-directory> <log-directory>`. The group
uses private INI settings and owned temporary fixtures only. Its new cases cover
unmarked focused Rename in details/icons, preserved independent marks, Flat
relative filesystem Rename, failures without overwrite, four writable native
archive handlers, Shift+F4, both address fields, filesystem/archive/nested panel
navigation and directory symlink browsing/Rename. Affected selection, statistics,
Copy/Move, exclusive filesystem Rename and duplicate native-row Rename checks are run together afterward.

Executed on 2026-10-06, macOS 26.6.2 / Apple M3 / Qt 6.11.3, with the
existing build directory. The latest distinct affected results are **112 passing,
zero failures and zero skips**, counting setup/cleanup once per suite:

| Suite | Latest passing checks | Evidence |
| --- | ---: | --- |
| Focused keys / relative Rename / panel navigation | 26 | [Final input log](panel-key-final-input.log) |
| Copy workflow | 23 | [Final input log](panel-key-final-input.log) |
| Command entry / exclusive Rename | 27 | [Affected group](panel-key-group.log) |
| Original selection | 16 | [Affected group](panel-key-group.log) |
| Folder statistics | 11 | [Affected group](panel-key-group.log) |
| Native writer / duplicate row Rename and Delete | 9 | [Native log](panel-key-native-rename.log) |

The [first grouped run](panel-key-initial.log) exposed the old filesystem
same-parent restriction and WIM validation of handler-created ancestor folders.
Both were corrected. ZIP/TAR seeds incorrectly selected the 7z default method;
test fixtures now use original archive defaults. A recursive two-panel test lookup
then selected the other panel's child, and the new test helper crashed; the
[repair log](panel-key-repair.log) retains that failed run. Widget lookup now
checks the owning MainWindow. This was a test-helper crash, not proof of an
application crash. Subsequent input failures are retained in
[remaining input](panel-key-remaining-input.log) and
[scan focus failure](panel-key-focus-failure.log).

Logical Qt window activation was made explicit in tests. That revealed an actual
asynchronous scan focus loss: Enter disabled the list and Backspace could no
longer reach its list-scoped shortcut. Focus restoration was fixed, and the five
affected suites were run together. A final affected input/Copy run additionally
verified that an explicit address edit keeps its focus. Successful suites are not
counted twice. Qt event delivery and logical activation do **not** establish
physical Fn/Option/Finder/AppKit behavior on a locked desktop.

The GUI and test executables built successfully. This milestone does not claim a
new standalone package or final clean build: packaging is deferred until the
following related creation/address/key implementation batch is complete.

## Follow-up source boundaries

Absolute/parent-relative **creation** names retain the previously documented
safe-relative restriction (command-entry-port.md); that restriction is separate
from the relative Rename support added here. The current address field is a
QLineEdit and does not yet reproduce the source header combo's folder dropdown.
Alt+F1/F2 focus is connected, including the single-panel fallback; the dropdown
is still portable implementation work, not an OS difference. These concrete
remaining paths must be completed before the final parity audit.

The following address/creation/key batch now implements these recorded gaps.
See [address-workflow-port.md](address-workflow-port.md) for its separate source
scope and execution evidence; this milestone's counts and failure logs are unchanged.
