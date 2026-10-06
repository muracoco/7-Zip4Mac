# Address, creation and complete list-key source batch

This batch implements the remaining related portable workflows before one grouped
validation. Missing source behavior is implementation work; an implemented path
failing validation is a defect. Final full-format/clean-build acceptance remains a
separate release gate.

## Source reuse and Qt boundary

`scripts/import-panel-key.py` pins seven official 7-Zip 26.03 source files. The
complete `PanelKey.cpp` key dispatcher and its property/key table are imported,
including original modifier precedence. Qt supplies key numbers, flags, callback
commands and deferred close. The already imported `PanelSelection` handles plain
Insert and Shift-arrow state once. Original empty Cut/Paste and Ctrl+PageDown
stay empty operations. The native right-Control distinction retains the existing
macOS virtual-key adapter; Meta remains macOS-specific.

- The complete list dispatcher includes focused F2–F7, two-panel Tab, F9,
  Ctrl+F3–F7 sorting, Ctrl+W, selection keys, comments, history, views and bookmarks.
  Qt consumes dispatched sort/F9/Tab keys to avoid also invoking an QAction or
  default widget tabbing. Original Alt-arrows still allow normal list movement.
- The address field is now an editable combo. Official PanelFolderChange supplies
  the behavior: indented root/current ancestors, then Documents, Computer,
  drives and Network; paths are separate from displayed labels. It includes
  virtual archive/nested path components. Selecting a row binds its full path;
  Enter binds typed text and returns focus after successful loading; Escape
  restores the current location; Tab returns to the same panel's list.
- Original Panel.cpp / App.cpp define address Alt+F1/F2, F9 and Ctrl+W. Both
  list and address Alt+F1/F2 open the requested dropdown; one-panel fallback is
  retained. Existing tests are updated for this newly implemented popup contract.
- Official FSFolder::GetAbsPath is imported unchanged behind QString adapters.
  FS creation accepts explicit absolute, parent-relative and lexical-dot paths.
  Create Folder builds missing parents, while Create File uses exclusive NEW
  and does not create missing parents. Browsed path aliases and standard macOS
  /var, /tmp and /etc aliases are resolved; entered symlink parents remain
  refused by no-follow directory handles. Existing files/links are preserved. A filesystem scan also preserves a newer
  typed address draft while the address retains focus.
  Archive creation retains its safe relative path policy and original handler.

## Classification

| Area | State | Boundary |
| --- | --- | --- |
| Complete original list dispatch | Implemented | Original source with Qt/selection callbacks; native keys require separate confirmation |
| Editable address dropdown / navigation / Escape / Tab | Implemented | Qt adapter based on source callbacks, not a transplanted Win32 combo |
| Absolute / parent-relative FS creation | Implemented | Original path resolver and guarded asynchronous installation |
| Computer / drives / Network | macOS difference | Computer maps to /; actual mounted /Volumes drives replace drive letters; /Network is shown if provided by the OS |
| Windows shell namespace icons / NTFS paths | macOS difference | Qt/system icons and POSIX path names; no Microsoft assets |
| Unsafe archive paths / entered symlink output parents | Port safety policy | Explicitly retained safeguards, not an OS limitation |
| Physical Finder / Fn / AppKit input | Unverified | Qt logical activation and test events do not establish these |
| Full parity / final clean build / source/license / publication | Not complete | Remaining release scope retained |

## Grouped verification

`./scripts/test-address-workflow.sh <build-directory> <log-directory>` builds the
application and seven affected suites, then runs them as one group. Fixtures and
preferences are private temporary paths. New integration cases use actual Qt
commands, popup row events, real file creation/failures, nested archive navigation
and original modifier precedence. Previously successful suites are rerun only
because this batch changes their shared input/creation/address boundary. Subsequent
repairs repeat only failed or affected selections.

The [first group](distribution.md) ran after the entire batch.
Creation, nested popup navigation and ordinary dispatch passed. Two assertions
required fixture/contract corrections: Qt logical focus belongs to the editable
combo rather than its line-edit child; the second-panel icon-mode fixture had
not set its independently saved view mode. The source adapters were not changed
to satisfy those incorrect assertions. The explicit-address scan assertion now
also verifies that the typed draft survives completion.

The [affected repair](distribution.md) passed both input suites but
exposed memory corruption in the listing suite during archive-window destruction.
A selected raw-name Rename run passed, so passing reruns alone were not accepted
as a diagnosis. A separate empty AddressSanitizer build reproduced a precise
heap-use-after-free: QWidget's base destructor hid the new combo after the
MainWindow archive strings were destroyed, and hidePopup invoked its captured
location callback. [Diagnostic log](distribution.md).

MainWindow destruction now clears that callback, removes its application event
filter and disconnects application/child callbacks before destroying state. The
[same entire listing suite under ASan](distribution.md) has
**11 passing checks with no ASan diagnostic**. A final normal affected group is
recorded separately, because the lifetime boundary is shared by those windows.
Setup/cleanup and successful reruns are not added twice. No source behavior is
marked tested merely because its implementation exists.


## Latest executed results

On 2026-10-06, macOS 26.6.2 / Apple M3 arm64 / Qt 6.11.3 / official 7-Zip
26.03, the [final affected group](distribution.md) passed all seven
suites: **139 distinct checks, zero failures and zero skips**. Setup/cleanup are
counted once per suite; ASan repeats are supporting diagnostics, not extra cases.

| Suite | Passing checks |
| --- | ---: |
| Address / creation / complete dispatch | 25 |
| Focused keys / relative Rename / opposite panel | 26 |
| Copy / Move | 23 |
| Command entry / exclusive Rename | 27 |
| Listing / destruction / 15,000-row 7z and ZIP | 11 |
| Original selection | 16 |
| Folder statistics | 11 |

The additional ASan build started from an empty directory and instruments Port
C++/Objective-C++ code. Qt/system frameworks were not rebuilt with ASan. After
repair its same 11 listing checks pass without an ASan report. It is a diagnostic
build, **not** the final Release clean-build gate. The normal main bundle and
seven test executables were rebuilt after the lifetime correction. All-format
reruns are reserved for final acceptance; no codec/crypto/handler change is made
in this address batch. Physical Finder/AppKit/Fn/Option input remains unverified.


## Packaged application

The local standalone app is
`/DEPS/build-comments-20261004/7-Zip Mac.app`.
[Packaging evidence](distribution.md) checks 15 Mach-O files for
system/@rpath-only dependencies and verifies the ad-hoc signature. The actual
framework/plugin set is QtBase (including DBus, PrintSupport, native style and
GIF/ICO/JPEG plugins); this bundle does not contain QtSvg. Corresponding QtBase,
official 7-Zip and Port sources plus licenses/patches are included.

The [owned startup check](distribution.md) starts the actual bundled
executable without Qt/DYLD/test plugin environment overrides. It stays alive for
three seconds with zero stdout/stderr, creates one private INI profile and leaves
existing native user preferences byte-identical. An invalid explicit profile
exits with code 2 without falling back to user preferences. Only owned processes
are terminated. This confirms startup on this Mac, not physical Finder/Fn input.
