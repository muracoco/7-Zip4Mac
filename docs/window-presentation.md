# Window placement and toolbar typography — 0.2.3

Newly opened normal File Manager windows and standalone Open With menus are
centered in the available area of the display containing the pointer. The
menu bar and Dock are excluded. Restored window sizes are preserved when they
fit; normal windows are reduced to fit a smaller display. Saved maximized and
fullscreen states remain managed by Qt/macOS. A manual move during the same
File Manager session is preserved on hide/show; subsequent launches center again.

Previously the standalone menu was explicitly positioned beside the pointer,
which could place it at a screen corner. The File Manager restored an old corner
position or used the platform default. Both now use `WindowPlacement.h` at show.

The seven toolbar buttons now copy the resolved file-list font after widget
polishing, together with the existing address controls. Explicit font family and
size properties prevent Cocoa's smaller button font from being inherited again.
File-list font changes update these controls. Icons, action order and toolbar
visibility/size preferences retain the existing source-based implementation.

## Affected verification

Run on an unlocked, logged-in macOS desktop:

```bash
PORT_TEST_REGEX='^(cocoa_presentation|cocoa_open_with_progress|cocoa_open_with_exit_7z|cocoa_open_with_exit_zip)$' ./scripts/test.sh /absolute/build/path
```

`cocoa_presentation` observes actual AppKit window frames and available screen
geometry, checks all seven toolbar/address resolved fonts, Japanese labels and a
font change, retains manual placement on hide/show, and reopens a corner-position
saved geometry while preserving its size. The existing hidden-parent Progress
and standalone 7z/ZIP extraction/exit checks cover the affected show lifecycle.
Tests use private temporary settings and generated files; user archives and
preferences are excluded.

On 2026-10-06 the affected Release/Ninja build succeeded and all four selected
CTest suites passed (6.76 seconds total). `cocoa_presentation` measured matching
13 pt resolved fonts on this Mac, then matching 18 pt fonts after a list change.
Both native normal windows and the standalone Open With menu were centered;
saved corner geometry and in-session hide/show checks passed. Each standalone
7z/ZIP test extracted 513 generated files, including Japanese/space names and
an empty directory, checked contents, and exited without a blank parent.
No first-run failure occurred in this affected batch.

The self-contained packaged app launched on the actual Mac desktop. A native
screenshot showed all seven Japanese toolbar labels aligned with the list and
address text, with no clipping. Filesystem browsing was visible. Packaging
verified 15 Mach-O files with only system/@rpath dependencies and a strict ad-hoc
signature. The matching font/frame measurements come from the AppKit regression,
not a claim that a cropped screenshot shows the complete physical display.
The canonical installation uses the existing `scripts/install.sh` workflow to
retain rollback data and keep development/older copies out of Finder candidates.
See [machine-readable evidence](window-presentation-evidence.json). Multiple physical displays, display disconnection and fullscreen/maximized
transitions are not claimed as tested by a single-display normal-window check.
This is an affected incremental Release/Ninja build; the earlier empty build and
format matrix remain documented separately.
