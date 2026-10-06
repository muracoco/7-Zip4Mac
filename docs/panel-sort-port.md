# Original panel sorting and filesystem columns

This batch replaces display-text/locale sorting and column-number-based View
commands with the original 7-Zip 26.03 ordering policy. It also brings the normal
filesystem column schema up to the original `FSFolder.cpp` definition. This is a
functional batch; it is not the final release acceptance or a claim of complete
Windows parity.

## Original code retained

`scripts/import-panel-sort.py` verifies the SHA-256 of every referenced upstream
file before extracting unchanged bodies:

- `PanelSort.cpp`: natural filename comparison, raw UTF-16 comparison, outer
  parent/folder/property/Name/Prefix/load-order comparator and sort-state changes.
- `PanelItems.cpp`: default visibility, width and alignment policies.
- `PropVariant.cpp` / `.h`: signed/unsigned-width/BOOL/FILETIME comparisons and
  `Get_Ns100`. The original reserved-field precision rule is retained verbatim.
- `MyString.cpp`: ordinal case-insensitive string comparison.
- `FSFolder.cpp`: the original column sequence, including `FS_SHOW_LINKS_INFO`.
- `FileFind.cpp` / `PropIDUtils.cpp`: POSIX attribute projection and formatting.
- `PropID.h`: the complete property-ID enumeration used by the GUI adapter.

The generated includes preserve Igor Pavlov's copyright and LGPL provenance.
Qt provides UTF-16 uppercase conversion and the widget API; the Mac adapter
supplies cached typed values rather than Win32 list-view callbacks. Numeric
sorting never passes through `double`, displayed commas or translated strings.
Raw bytes remain distinct from display text; reparse values compare their
original parsed target using UTF-16 ordinal order. Unsupported raw types or
retrieval errors fall through to the original name tie-breaker.

## Resulting behavior

Name sorting treats digits as numbers and ignores leading zeroes, then resolves
ties by Prefix and the original row loading order. Parents and folders stay
first in either direction. A new Size, Packed Size, Created, Accessed or Modified
sort starts descending; choosing the same property reverses direction. Other
properties start ascending. Unsorted retains folder precedence and load order;
choosing it again reverses that order. A nonexistent visible property (Type in
an archive, for example) can still be the active sort without a misleading
arrow on an unrelated column.

View Name/Type/Date/Size now use `kpidName`, `kpidExtension`, `kpidMTime`, `kpidSize`
rather than guessed columns. Header clicks use the same policy. Qt keeps
responsibility for resizing and moving sections. Marks and focused items remain
bound to the same model rows across sort and all four view modes.

The filesystem columns follow Name, Size, Modified, Created, Accessed,
Change Time, Attributes, Packed Size, iNode, Links, Comment, Folders, Files,
then Path Prefix in Flat mode. The existing Mac port Type column is an extra
column at the end. Visibility, widths and alignment follow the original policy.
`fstatat(..., AT_SYMLINK_NOFOLLOW)` supplies the entry's own size, timestamps,
mode, inode, link count and allocated bytes (`st_blocks * 512`). Directory links
remain navigable while Flat traversal does not follow child symlinks.

Widths, visibility, visual order and sorting persist by property ID plus the
raw/typed flag. Old nine-column filesystem preferences migrate once. Removing
Prefix when leaving Flat view preserves its settings and its position for the
next Flat view; it cannot apply them to Type. The two panels and native archive
format schemas keep independent preferences. F3 updates typed folder sizes and
counts as well as display values, so subsequent sorts use the computed values.

## Differences and limits

- macOS stat/birth/change/access times, modes, allocated blocks, inode and link
  counts substitute for Windows file information. NTFS filesystem streams and
  Windows-only shell attributes remain platform differences.
- Qt uppercase conversion substitutes for the Windows character API; locale
  collation is intentionally not used. An exhaustive cross-OS Unicode casing
  comparison has not been performed.
- Type is an existing extra Mac port display column. It does not change the
  original Arrange Type (extension) operation.
- Sorting computed filesystem Folders/Files is a retained port addition. The
  original `CFSFolder::CompareItems` falls through for these two properties.
- The legacy console listing fallback derives sort values from its parsed
  fields; complete typed/raw behavior requires the bundled native metadata
  helper, which is the normal app path.
- `QTreeWidget` still commits and sorts the complete model synchronously. This
  batch does not implement the outstanding virtual model or native-icon work.
- Qt event injection is automated GUI verification, not a physical mouse/Fn
  check in an unlocked macOS desktop.

## Verification

The affected build and grouped checks are recorded in
[panel-sort-test.log](panel-sort-test.log). Tests cover natural names, 64-bit and
signed values, BOOL, FILETIME, raw checksums/reparse targets, stable ties,
parent/folder precedence, original first-sort direction, live header clicks,
Date precision independent of display, marks/focus/view modes, Flat/2-panel
preferences and old settings migration. Real 7z/ZIP archives exercise dynamic
column schemas and are tested by the original engine. The WIM metadata regression
also checks that actual SHA-1 raw bytes reach the host comparison data intact.
Affected directory, F3, selection, language/time and core archive checks run as
one group. Passed unrelated suites and the full 151-format matrix are reserved
for the final release gate.

### Executed result (2026-10-05)

- Apple Clang/Ninja compiled the helper, UI, app and affected tests successfully.
- **116 distinct QtTest cases plus 11 original metadata formatter checks pass**.
  The Qt count includes initialization/cleanup; 27 cases are in the new panel
  sorting suite. Distinct counts merge the first grouped run and targeted rerun
  rather than counting repeated PASS lines or CTest's duplicated failure output.
- The first group found two adapter defects: Qt's release had already reversed
  the indicator before the imported same-property toggle, and hidden sections
  exposed width zero to persistence. The release now restores the prior state
  before invoking the original policy; hidden widths keep an explicit cache.
  The three failed panel cases pass after repair.
- Three older test executables timed out waiting for newly implemented original
  Progress result/cancel dialogs. Their fixtures now acknowledge only their own
  expected dialogs and assert the result text/cancellation. All timed-out and
  previously unreached cases pass. This was a fixture/API-behavior mismatch, not
  evidence of an application crash or a new list functionality defect.
- The affected 30,000-row normal/Flat tests pass again after the width repair:
  2.460/2.539 seconds total, maximum measured event-loop gaps 869/900 ms. These
  measurements confirm the test's current threshold, **not** elimination of
  synchronous commit/sort; the virtual-model requirement remains.
- Real 7z/ZIP round trips include 32 MiB, Japanese/spaced names, correct/wrong
  encrypted-password paths and recovery after failure. Actual WIM raw SHA-1
  transport is checked against the source file digest.
- No full-format matrix or unrelated green regression was repeated. Physical
  desktop checks, final clean build and public release are still pending.

Targeted rerun: `panel_sort_tests` (three failed GUI/persistence functions),
`statistics_tests` (error/cancel functions), `directory_tests` (changed width
checks and remaining startup/two-panel functions), and `property_tests`
(`destroyAfterClosedProgress`). See the full initial/final transcript above.

App packaging/startup evidence is in
[panel-sort-bundle.log](panel-sort-bundle.log).
