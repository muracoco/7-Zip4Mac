# Original callback display and background listing work

This batch extends the original sorting/column port in `panel-sort-port.md`.
It does not complete the full Windows parity or release/publication objective.

## What the active Windows implementation actually does

The reference is the supplied, verified official 7-Zip 26.03 source:

- `CPP/7zip/UI/FileManager/Panel.cpp:409–411`: `LVS_OWNERDATA` is inside a
  commented block. `_virtualMode` is commented out in `Panel.h` and its active
  initialization. A fully virtual list is therefore not an enabled feature of
  this Windows version.
- `PanelItems.cpp:622–637, 775–785`: `USE_EMBED_ITEM` is not defined; the active
  list uses `LPSTR_TEXTCALLBACKW`, including the comment explaining its faster
  loading. Records still enter the list, and `SortItems` runs synchronously.
- `PanelListNotify.cpp:386–518`: names, prefixes, size-like unsigned properties
  and other properties are produced when requested. The normal name fast path
  hides RLO with an underscore, collapses at least five spaces and marks a final
  space. The prefix fast path preserves its text; other BSTR values flatten
  CR/LF. Size grouping uses spaces, not locale-dependent commas.
- `PanelItems.cpp` retrieves `IFolderGetSystemIconIndex`, then uses the original
  extension/attribute icon map as its fallback. Windows shell icon indexes
  cannot be copied as macOS assets.

`scripts/import-panel-display.py` validates SHA-256
`bd0b4f317539cc63799df8372f7de36d67df9d846e708f5ea1ed5c31fd64677a` and imports
unchanged `ConvertSizeToString`, `IsSizeProp` and the original name fast-path
body. Original copyright/license notices remain. Qt adapts UTF-16 units and
property storage; existing original TimeText/native-property formatters supply
other property text. This is reuse of enabled upstream behavior, rather than
implementation of the commented owner-data experiment.

## Implementation

- Ordinary filesystem and native archive rows retain property data. Display
  columns format and cache text on demand, instead of formatting every hidden
  cell while the list is built. Explicit F3 results override that cache.
- Display names and original names are separate. Paths, native item identities,
  sorting, Rename, selection masks/type selection, clipboard names and
  Properties keep original names. Five-space/RLO display markers never become
  archive-update targets or filesystem paths.
- At least 1,024 rows use immutable typed-property snapshots for worker sorting.
  The same imported comparator covers exact UInt64, FILETIME/raw values,
  folders/parent, direction, ties and Unsorted. Smaller lists sort immediately.
- Qt's automatic internal sorting stays disabled, preventing a second
  synchronous sort. The application keeps its own enable state and enables
  header clicks explicitly. The completed permutation is installed in one GUI
  operation; marks, native indexes, selected items and current item survive.
- Generation/cancellation flags discard stale sort/icon results after a newer
  request, navigation, row removal or panel destruction. Worker closures contain
  owned metadata, never widget/item/model-index pointers. Mutation actions wait
  for sorting; icons do not block normal operations.
- Native filesystem icons and archive extension/folder icons use NSWorkspace
  and Uniform Type Identifiers on a dedicated worker pool. Workers return copied
  QImage pixels; no native image/icon engine reaches paint callbacks. Results
  arrive in batches of 64 and use stable row IDs across sorting. A generic icon
  remains available while loading or if the OS cannot provide one.
- The icon service drains before Qt platform teardown. A removed panel cancels
  work without waiting on an OS icon call. An individual NSWorkspace request
  cannot be forcibly interrupted; unusually slow OS providers can delay final
  application shutdown. Physical custom-icon appearance remains unverified.

The thread choice follows Qt 6.11.3's own `QFileInfoGatherer` worker/provider
path, and the Cocoa integration supports threaded pixmaps. This adapter returns
QImage to make UI ownership explicit. Windows shell icon artwork is not bundled.

## Limits and classification

| Area | Classification | Result |
|---|---|---|
| Original name/size display policy | Implemented | Imported original bodies, original identity kept separately |
| Deferred property text | Implemented | FS and native archive rows; explicit F3 values remain authoritative |
| Background large-list comparison | Implemented | Same original comparator; cancellable generation-safe snapshots |
| System and extension icons | macOS difference | Native Mac icons replace Windows shell indexes; worker retrieval |
| Fully virtual owner-data model | Not enabled upstream | Not a missing Windows function; optional future performance architecture |
| All large-list GUI work | Partially implemented | Final row insertion/reordering/freeing and native metadata parsing still include synchronous work |
| Physical Finder/icon/function-key checks | Not verified | Requires an available unlocked desktop; Qt injections are separate evidence |

Native archive row creation is still a GUI operation. The archive metadata
reader also has synchronous parsing steps. There is no promise of constant-time
loading or a fully asynchronous model. Measurements below document those limits.

## Validation

The related implementation was completed before the grouped test run. Failures
were corrected and only failed or affected paths rerun. Actual results and
bundle checks are recorded in `panel-listing-test.log` and
`panel-listing-bundle.log`; the final result section follows the completed run.

The tests own dedicated temporary trees. Fixtures include Japanese/spaces/RLO
and trailing-space names, real 15,000-file 7z and ZIP archives, a 40,000-row typed
sort, exact UInt64 values, mark/focus/selection preservation, Unsorted,
superseding/clearing/destruction, actual NSWorkspace icon retrieval, and archive
Rename/Test using original names. The associated checks cover normal/Flat
30,000-file loading, two panels, navigation, Options, F3, Properties, comments
and external-opening warnings. No existing user file is a test target.

### Executed result on this Mac

- 166 distinct passing Qt test cases across the initial grouped run and targeted
  repair/identity checks (including each suite's init/cleanup). There were no
  skipped cases. All initially failed test paths were subsequently successful.
- The initial real product failure was header click enablement after disabling
  Qt automatic sorting; the adapter now controls click enablement explicitly.
  The new ZIP fixture had incorrectly retained the default 7z codec; its Copy
  method is now explicit. Older comment tests used fixed column indexes and
  were migrated to property IDs. A filename-warning test used `.first()` on an
  empty old-display-name match and crashed; the macOS report locates the fault
  in that test function. It now asserts one original-formatted display match
  and checks the actual path before invoking the action.
- New display identities were connected through original names for Rename and
  native selection before completion. Actual 7z/ZIP GUI Rename, preserved
  parent paths, original clipboard names and subsequent Test all pass.
- 30,000 filesystem rows: 2,504 ms normal / 2,446 ms Flat; largest event-loop
  gaps 648 / 639 ms during the initial grouped run.
- Final identity run: 15,000 7z rows 1,673 ms, maximum gap 794 ms; 15,000 ZIP
  rows 2,358 ms, maximum gap 1,034 ms. This is measured remaining synchronous
  loading work, not a claim that the entire model path is asynchronous.
- Actual native filesystem/archive-type icon retrieval, generation discard and
  panel destruction pass. Finder visual appearance and hardware events are
  not inferred from these checks.

Build uses Apple Clang, Qt 6.11.3, official 7-Zip 26.03 on macOS 26.6.2 arm64.
Reproduce this group after `scripts/build.sh` with:

```sh
ctest --test-dir <build-directory> -V -R '^(panel_listing|panel_sort|directory_scanner|folder_statistics|properties|panel_selection|panel_open|version_menu_time|file_comments)$' -j1
```

The final empty-directory build, all-format acceptance, license/privacy review
and authorized public repository publication remain release gates.
