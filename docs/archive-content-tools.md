# Clipboard and archive-content CRC / SHA

The specification is the actual official 7-Zip 26.03 implementation, including method bodies rather than only menu/key declarations.

## Clipboard

`CPP/7zip/UI/FileManager/PanelKey.cpp:283–305` dispatches Ctrl+X/C/V. In `PanelMenu.cpp:427–458`, `EditCut()` and `EditPaste()` contain no active implementation. `EditCopy()` obtains selected item names, joins them with CRLF, and calls `ClipboardSetText`. The corresponding clipboard resource-menu entries are commented out. The port implements name-text copying, including empty selection, folders, Unicode names, both panels and icon views. It consumes Ctrl+X/V without changing files or clipboard contents. This corrects the earlier audit's assumption that all three dispatch targets were functional file transfers.

The integration test preserves the pre-test clipboard's MIME data in memory and restores it if the clipboard still contains the test's owned value. No existing clipboard data is logged or stored in the repository.

## Archive-content hashing

`PanelCrc.cpp:338` uses `CApp::CalculateCrc2` and `CopyTo` with `streamMode=true` and `hashMethods`; `PanelCopy.cpp:257` supplies the hash-stream callback. The console equivalent is `7zz t -scrcMETHOD -- archive selected-items`, through the official decompression and checksum code. It writes no extracted files and needs no custom hash/decompression algorithms.

`ArchiveOperation::HashArchive` is separate from filesystem `h`. It retains operation, archive target, exit code, errors, progress hint, cancellation and the existing stdin-only password handling. GUI password retry caches the successful password only in the current panel's memory. Passwords are not stored in preferences or process arguments. Explicit selections use `-spd` so names containing `[*]` remain literal. Folder selections include descendants. The same action is available when browsing a nested archive; nested changes use confirmed parent write-back.

Automated verification covers all 11 menu choices against the official filesystem hash command for 7z, ZIP, data-encrypted 7z, header-encrypted 7z and ZIP AES. SHA-256 is also checked independently with Qt against the original bytes. Wrong-password retry, folder aggregate hashes, Japanese/space/literal-wildcard names and unchanged source archive SHA-256 are asserted. This does not establish every codec/format variant or physical keyboard/menu behavior.

## Transfer and mutation audit corrections

`UI/Agent/ArchiveFolder.cpp:32` rejects `moveMode` with `E_NOTIMPL`. `FileManager/App.cpp:565` passes the Move To flag to that method. Thus archive-source Move To is not a working upstream feature. The port disables it. Filesystem-to-archive Copy / Move uses the separate `ArchiveFolderOut.cpp::CopyFrom` path and is now implemented with the official portable enumerator. [Behavior, tests and remaining link/layout limits](archive-transfer.md).

Archive Create Folder is a real upstream feature (`PanelOperations.cpp:363`, `ArchiveFolderOut.cpp:410`, `AgentOut.cpp:537`), subject to update/read-only conditions. Archive Create File is not implemented upstream. Nested updates now use the shared verified ReplaceFile transaction and confirmation on each level exit.

## Empty folders in existing archives

The shared transaction in `src/ArchiveMutation.cpp` now imports the official
Agent CreateFolder body through the bundled helper. Writable 7z/ZIP/TAR/WIM,
including multiple WIM images, internal prefixes and data/header encryption
are covered. NoChange packed streams avoid the previous unrelated full-Test
password prompt; mixed-data-password folder updates pass. Working-folder
preferences apply to folder/replacement/comment staging, with cancellable
cross-volume final installation. New-folder callback times match the original
100 ns FILETIME input, while serialization follows each handler's defaults.

See [the imported operation, constraints and tests](native-folder-update.md).
Original identity/SHA-256, permissions and extended attributes are checked or
retained. Leading prefixes now use the official stream boundary; see
[archive-prefix-updates.md](archive-prefix-updates.md). Windows Agent refuses
tails and multiple open layers. Full extraction metadata and mixed-password
verification in other operations remain incomplete. Native desktop clicks and
foreign-owner/ACL fixtures are not established by the scoped automation.
