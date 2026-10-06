# Official Agent folder identity and property methods

Baseline: official 7-Zip 26.03 `CPP/7zip/UI/Agent/Agent.cpp`, SHA-256
`337ec0bdca12a34c53e361bc6cf42b6adb572c29a97e53b13e833fe838c772b0`.
The existing original `AgentProxy.cpp` supplies both normal and tree proxies.

`scripts/import-agent-properties.py` imports the bodies of `GetName`,
`GetPrefix`, `GetFullPrefix`, `GetProperty` and `GetFolderProperty`, including
their original index macros. The class/COM boundary is a small read-only
facade around the opened handler and proxies. Agent host/update ReadOnly and
IsHash branches are excluded from this property snapshot; existing update
checks remain separate. Handler, codec, crypto and archive parsing code stays
official. Original notices and source distribution are retained.

The bridge serializes every proxy directory, its parent/alternate directory,
owner archive index, child order and cached totals. Each row retains the
`(directory ID, local item index)` reference, real archive index, child/alternate
directory and actual proxy name. It no longer reconstructs native rows by
splitting paths and coalescing folder names. Implicit normal folders and
native tree folders are displayed directly; Flat View follows directory-first
traversal and excludes alternate streams unless that folder is explicitly
bound, as in the default Agent setting.

Original GetProperty supplies synthesized Name/IsDir, folder Size/Packed Size,
counts and CRC, including the separate Flat size semantics. Handler and raw
properties retain their native item index. Single/multiple selection
Properties uses row identities. Folder Properties requests Path and the
original five `kFolderProps`, matching `PanelMenu.cpp::Properties`; Path is
displayed under Name. Root GetName is not queried, and nullable BSTR values
are formatted as empty.

File → Alternate streams and the File Manager right-click menu bind native
archive stream folders. Up uses the proxy parent identity; a file's stream
folder returns to the file's containing directory. Stream rows keep their
real names and attributes. NTFS **image/archive browsing is portable**;
applying NTFS ADS/security to the macOS filesystem is an OS difference.

## Executed verification

Mac 26.6.2, Apple M3/arm64, Apple Clang, Qt 6.11.3. Release builds succeed.
The final affected suites pass **216 checks** including setup/cleanup:

- Properties: 19; native formatter: 11; open-mode/prefix/nested tests: 24.
- Existing multi-image WIM folder/update/GUI regression: 3.
- Agent tree: 4, including genuine duplicate ZIP entries with different sizes,
  implicit and empty folders, Flat traversal, independent multi-selection
  totals, native folder binding and protected ambiguous rename/delete.
- A genuine unmounted NTFS image created by mkntfs/ntfscp contains a named
  stream. Native binding, attributes, normal-Flat exclusion and Up pass.
  The stream command is clicked through the actual Qt context-menu geometry.
- The changed shared listing bridge passes all 151 genuine format/extension
  registrations, 155 Qt checks, including right-click 7z/ZIP creation, Test,
  extraction and Japanese-path SHA-256 comparisons. The final context route
  is checked again after adding the stream action.

Commands (fixture tools are isolated local tools, with no mounts/admin rights):

```bash
./scripts/bootstrap-fixture-tools.sh
./scripts/test-agent-tree.sh /absolute/path/to/build
ctest --test-dir /absolute/path/to/build -V \
  -R '^(properties|native_metadata_formatter|archive_open_modes)$'
./scripts/test-formats.sh --build /absolute/path/to/build
```

The packaged app passes dependency checks for 15 Mach-O files, with a valid
ad-hoc signature. Its bundled helper lists Japanese/implicit-folder metadata.
An isolated LaunchServices instance survives three seconds and lsof verifies
the bundled patched Cocoa plugin. Physical desktop clicks remain unverified.

[Final execution logs](distribution.md).
The helper install now atomically replaces executable inodes, avoiding macOS
retaining an old signature cache after overwriting an executed Mach-O.

## Remaining scope

Directory IDs are listing-snapshot identities, not permanent archive IDs.
Relisting rebinds by directory path; full collision-aware selection restoration
is still incomplete. Selected Extract/Test/hash and focused temporary opening
now use the imported real-index methods; see [selection port](native-agent-selection.md).
Native-index Delete/Rename for 7z/ZIP/TAR/WIM now reuse official Agent bodies;
see [item updates](native-agent-item-updates.md). Comment/replacement and
single-stream update parity remain incomplete. Ambiguous legacy paths are disabled
rather than applying one selected row's command to several entries. Creating
folders inside stream views is not provided. These are **portable omissions**,
not macOS differences. Long-path relative-prefix details, very large list/model
insertion and sorting, full extraction metadata/link restoration, physical
desktop checks and the remaining release requirements remain unfinished.
This checkpoint does not establish complete Windows parity or publication.
