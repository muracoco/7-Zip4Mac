# Official Agent selection for archive read operations

Baseline: official 7-Zip 26.03 `CPP/7zip/UI/Agent/Agent.cpp`, SHA-256
`337ec0bdca12a34c53e361bc6cf42b6adb572c29a97e53b13e833fe838c772b0`.
`scripts/import-agent-selection.py` imports `GetRealIndex` and `GetRealIndices`
without rewriting their bodies; only the adapter class name changes. Original
copyright/license notices are retained. Both original Agent proxies are reused.

Selected Extract, Test, archive hashes, focused nested opening and temporary
external/drag extraction now pass `(directory ID, local item index)` references
to the opened official handler. Filename strings are display/legacy fields,
not the selection authority. The native adapter rebuilds the same proxy graph,
checks the selected names/real indices and item count, and calls the imported
selection methods. The original console extraction callback, hard-link preparation,
handler Extract, close/result handling, passwords and progress remain in use.

The Agent's policies are preserved: ordinary folder rows include descendants;
Flat folder rows do not expand descendants for Extract/Test. An implicit Flat
folder has no archive item and can legitimately select no files. Native tree
items include their alternate streams. The Mac Flat view does not load streams
into the ordinary Flat list. Repeated references to the same real index are
normalized at the process boundary; distinct same-name indices remain distinct.
Current-folder path parts and tree base-parent IDs feed the original extraction
callback. Temporary opening retains root paths for its existing file handoff.

Requests use a versioned little-endian private file in a mode-0700 temporary
directory. The file is mode 0600, opened without following symlinks, owner/type/
size checked, and removed at completion. It contains no password. The native
reader verifies an inode/size/mtime/ctime token for the physical archive and its
volumes; this is a change detector, not a global lock or a content checksum.
Atomic single-file archive installation refreshes the candidate's token to the
installed file. The existing prefix/nested regression caught that boundary and
now verifies immediate reopening after parent write-back.

## Executed checks (2026-10-05)

Mac 26.6.2, Apple M3/arm64, Apple Clang, Qt 6.11.3. Native helper and Qt Release
builds succeed. The affected **98 checks** pass, including setup/cleanup:

- Selection suite: 12. Genuine duplicate ZIP siblings independently extract
  their different payloads and produce independently checked SHA-256 hashes.
  A ZIP with only the second sibling's bad CRC proves selected Test succeeds
  for the first, fails for the second and recovers afterwards.
- Ordinary/Flat implicit and empty folders, current-folder prefix removal,
  genuine unmounted NTFS stream selection/extraction/hash, invalid/stale row
  refusal, selected traversal/link guards, encrypted filename-protected 7z,
  Japanese/space names and wrong-password recovery are covered.
- A Qt context-menu mouse click runs Test on one same-name sibling. A 64 MiB
  selected extraction pauses, the GUI event loop continues, Cancel removes
  staged output, and the same backend then runs Test successfully.
- Existing tree 4, open-mode/prefix/nested 24, editor write-back 32, Properties
  19, native-progress round trips 4 and multi-image WIM 3 also pass. The full
  151-registration format matrix is not repeated for this optional selection
  path; its previous successful run remains documented separately.

```bash
./scripts/test-agent-tree.sh /absolute/path/to/build agent_selection_tests
./scripts/test-agent-tree.sh /absolute/path/to/build
ctest --test-dir /absolute/path/to/build -V \
  -R '^(archive_open_modes|editor_writeback|properties)$'
```

The fixture runner also accepts QtTest function names after the suite argument.
The normal `.app` passes the 15-Mach-O dependency check and deep ad-hoc
signature verification. A separate LaunchServices instance remains alive and
loads the bundled patched Cocoa plugin; its bundled helper also executes.
Only that owned test instance is stopped. Physical clicks remain unverified.
See [execution logs](native-agent-selection-test.log).

## Remaining scope

Rename/Delete for 7z/ZIP/TAR/WIM now reuse the official Agent update bodies;
see [the item-update checkpoint](native-agent-item-updates.md). Comment,
replacement and single-stream update parity still need complete native-index
dispatch. Ambiguous legacy path commands remain disabled. Extracting
multiple selected files to the same output path is still refused rather than
providing the Windows per-entry overwrite sequence. Bookmark addressing and
selection restoration after relisting are still path based. Full extraction
metadata/link restoration, large-list model work, remaining UI parity, physical
desktop checks and final release/publication gates remain unfinished.
These portable omissions are not classified as macOS differences.
