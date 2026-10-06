# Properties source comparison

Baseline: official 7-Zip 26.03. The ordered two-column dialog, archive folder
and selection totals, layer sections and full-value controls are now implemented.
Handler schemas, typed/raw formatting and both Agent folder proxies now reuse
official code. Native tree/model integration remains partial. Native desktop
clicks are pending while locked; automated Qt events are recorded separately.

## Actual upstream paths

`UI/FileManager/PanelMenu.cpp::CPanel::Properties` (172–421) uses
`CListViewDialog` for folders exposing `IGetFolderArcProps`. Otherwise it
invokes Win32 shell `properties` (57–75, 178). That filesystem shell path is
an OS mechanism; archive Properties is a portable File Manager feature.

`PanelItems.cpp::Get_ItemIndices_Operated` (984–1001) uses selected items.
An empty internal selection falls back only when the focused non-parent row
is itself selected in the native list. Completely unselected focus does not
produce an item section. F3's operated-item fallback is a separate behavior.

## Sections and values

1. One selected item: provider-ordered properties, raw properties, separator
   (`PanelMenu.cpp` 190–259).
2. Multiple selection: `N object(s) selected` (the English language resource), nonzero Folders/Files, Size,
   Packed Size, separator (260–295). Selected folders contribute themselves
   and their descendant counts/sizes; explicit and implicit folders matter.
3. Current archive folder: its path under the **Name** property, Size,
   Packed Size, Folders, Files and CRC (305–333). An empty root path omits Name.
4. Archive layers, inner to outer: Path, Type, Error Type, Error, Error Flags,
   Warning, Warning Flags, Offset, Physical Size, Tail Size, then the remaining
   provider properties. Item-layer sections use a shorter separator
   (158–170, 335–393). Failed-open properties follow (395–413).

`Agent/AgentProxy.cpp` builds descendant totals at open time (254–305, 535,
764–806, 1063–1067). `Agent.cpp` reads that cache (338–346, 372–381,
1232–1297). Properties does not start a recursive filesystem scan. Folder
Size/Packed Size normally use cached totals; Flat uses entry sizes, while
Folders/Files still describe descendants. FS F3 recursion is separately in
`FSFolder.cpp::CalcItemFullSize` (1017–1029).

Folder CRC is the modulo-2^32 sum of descendant file CRCs. A zero-size file with
a defined Size and no CRC contributes zero; other missing CRCs make the folder
CRC undefined. Flat selection can overlap folders and descendants and counts
each operated item, matching the upstream calculation.

## Dialog and controls

`ListViewDialog.h` 37–42 and `.cpp` 41–126 use two left-aligned columns with
visible empty-caption headers and automatic widths, initially no row selected,
and no deletion. Display values replace newlines with spaces and truncate to
1024 characters; original values remain available.

`ListViewDialog.cpp` 164–308 supports Ctrl+A and Ctrl+C/Ctrl+Insert. Copied
rows are `Name: original value`. Enter or double-click on one row opens its
full read-only value. `ListViewDialog.rc` 7–14 and `GuiCommon.rc` 72–74 define
a resizable modal with default OK and Cancel. `EditDialog.rc` 7–14 defines
the full-value viewer with default Close. Main accelerator is Alt+Enter.

## Implemented port behavior

`ArchiveBackend` retains the handler's ordered typed/raw schema, actual item
indices and separate outer-to-inner header layers alongside compatibility maps.
`NativeMetadata` reads these from the already-open official `CArchiveLink` and
uses official property, NT security/reparse and error formatters. Full Property
values and shorter list values are retained separately. UInt64 decimal strings
and FILETIME ticks/reserved precision fields avoid JSON number truncation.
Repeated layer properties remain separate. The text parser is retained only
for custom/fake executables without the bridge; bundled listings use the helper.

Both official Agent proxies provide explicit/implicit folder totals at open
time. `ArchiveFolderIndex` uses this native cache when available, retaining the
legacy calculation as fallback. `MainWindowProperties.cpp` assembles sections
without extraction or a filesystem scan. Nested navigation retains each
parent's index/layers, and successful parent write-back refreshes that snapshot.
Parent member sections use their original Path. Single-item metadata is selected
by archive index rather than name, preserving duplicate-file properties.

`PropertiesDialog` is a resizable modal with two left-aligned read-only columns,
blank visible headers, automatic widths, no initial selection and default OK /
Cancel. Ctrl+A, Ctrl+C/Ctrl+Insert, Enter, double-click and the configured
single-click activation are implemented. Display truncation/flattening never
changes copied or full-viewer values. Values are excluded from UI translation.
The filesystem substitute uses existing visible values and F3 totals in the
same dialog; Windows filesystem shell property pages are an OS difference.

## Verification and remaining gaps

The original ten QtTest checks cover one/many/no selection, explicit/implicit/empty
folders, root/subfolder, normal/Flat sizes, CRC overflow/missing CRC, actual
ZIP and split-ZIP layers, a nested 7z/ZIP parent, saved child changes and refreshed
parent Size/CRC/Physical Size, literal Japanese/newline/long values, clipboard,
Enter/double-click/single-click, modal buttons and source-byte preservation.
An additional shutdown check covers a completed, closed Progress dialog. It
reproduced a worker destructor callback into destroyed UI state; MainWindow
now disconnects worker callbacks before member teardown. The test clipboard
is restored only when it still contains the test's own value.

The current Properties suite has **19 passing checks**. Additional cases cover
native schemas and column order/default visibility, long/multiline ZIP comments,
real WIM raw SHA-1/folder totals, FILETIME precision and full UInt64 values,
encrypted listing/password redaction, CR/LF filenames, protected metadata paths
and Flat Name/Path Prefix. The native formatter has **11 passing checks** for
64/256-byte thresholds, hexadecimal rules and NT security validation.

The command remains **Partially implemented** overall. The complete handler
schema and native raw formatters are now present; folder path grouping, native
tree/alternate streams and normalized-colliding directory identities remain
incomplete. JSON parsing and final model insertion/sort still run on the GUI
thread; a large-list model remains a separate scalability task. Complex real
NT security fixtures and physical dialog comparison remain unverified.
See [native-metadata.md](native-metadata.md) for the process boundary, upstream
reuse, POSIX tree-proxy lifetime correction and executed evidence.

See [test-results.md](test-results.md) for executed clean-build/regression evidence.
Physical menu clicks and pixel comparison remain pending while locked.
