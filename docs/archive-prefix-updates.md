# Official archive-prefix updates

The port reuses the official 26.03 update stream boundary instead of treating
every nonzero archive offset as read-only. Archive serialization, compressed
data copying and encryption still belong to the original handlers.

## Source reuse

`scripts/import-folder-update.py` pins and imports the stream block from
`CPP/7zip/UI/Agent/ArchiveFolderOut.cpp::CommonUpdateOperation` (lines 128–143).
Its original notices and body remain in `src/upstream/AgentUpdateStream.inc`.
Only the already-opened archive and output-file references are adapted.

The distinction between two offsets matters:

- `ArcStreamOffset` precedes the handler's input stream. The Agent copies these
  bytes and uses the original `CTailOutStream` to give the handler a zero-based
  output stream.
- `Offset` is inside the handler's input stream. The unmodified 7z handler
  (`Archive/7z/7zUpdate.cpp`, lines 1928–1943) and ZIP handler
  (`Archive/Zip/ZipUpdate.cpp`, lines 2099–2103 and 2141–2150) preserve their
  respective prefix/stub bytes.

Copying the sum (`GetGlobalOffset`) in the adapter would duplicate the bytes
owned by the handler. The imported Agent block copies only `ArcStreamOffset`.
The portable console already uses the same boundary in
`UI/Common/Update.cpp`, lines 784–801. Add, CopyFrom, update, rename and delete
therefore use the existing console implementation, without a new serialization
path. ZIP Comment uses the same official handler, with the later bounded
ZipCrypto metadata repair documented in [zip-metadata-port.md](zip-metadata-port.md).

The Qt panel permits supported single-layer archives with nonnegative offsets.
CreateFolder, ReplaceFile and Comment keep the existing staged transaction. It
now verifies both the resulting archive offset and every original prefix byte
before installation, in addition to the original file's identity/SHA-256,
item checks and operation-specific payload checks. Cancel during prefix
verification keeps the original. Ownership, mode, ACL and extended-attribute
preservation follow the existing transaction.

## Windows-supported boundaries

`UI/Agent/Agent.cpp::CAgent::CanUpdate`, lines 1611–1632, rejects devices,
multiple open layers and `ErrorInfo.ThereIsTail`. It does **not** reject a
nonzero leading offset. The port retains the multiple-layer, tail, volume and
read-only guards; preserving a trailing unrelated block is not a missing
Windows Agent function. CAgent::CanUpdate does not itself reject all warnings;
that additional port safeguard is separately identified in
[update-safeguards.md](update-safeguards.md).

Console `UpdateArchive` rejects tails and multiple volumes but can choose the
innermost archive from an opened chain. Its successful exit alone does not
prove that an outer PE or Split container was preserved. The panel's
single-layer guard therefore remains necessary. A self-executable stub opened
as a single archive is different from a PE → archive chain. Preserving stub
bytes does not repair executable signatures or make Windows code runnable on
macOS.

## Verification

The scoped tests are `OpenModeTests::prefixedUpdates`,
`prefixedGuiAndNestedWriteback` and `updateRejectedLayouts`. They use dedicated
temporary data, not user archives or executable payloads. The supported-format
rows include original 7z, ZIP, TAR and WIM files with inert leading bytes,
7z data/header encryption and ZIP AES. Prefix equality is checked after folder
creation, replacement, ZIP comments, internal CopyFrom, rename and delete,
followed by Test and extraction/content comparison. The GUI cases cover enabled
actions and nested write-back into prefixed 7z/ZIP parents. Tail and Split-chain
refusal preserve original bytes.

The final scoped result is **114 passed / 0 failed / 0 skipped**, including
setup/cleanup: archive open modes **24**, official-folder integration **14**,
editor write-back **32**, ZIP/FS comments **31** and shared native progress
**13**. The transcript is [archive-prefix-updates-test.log](distribution.md).
The stronger final tail/Split guard assertions were rebuilt and passed in a
final open-mode run; unrelated suites were not repeated after that test change.

Both packaged and normal bundles pass the dependency check: 15 Mach-O files,
with system or bundled `@rpath` dependencies. LaunchServices started the normal
bundle with isolated preferences and an owned temporary folder. PID 39566
survived three seconds, and `vmmap` confirmed the bundled patched Cocoa plugin.
Only that owned instance was terminated. This is startup evidence, not physical
menu/Finder/Fn verification.

Run after building:

```bash
ctest --test-dir <build> -V -R '^archive_open_modes$'
```

This is a scoped implementation checkpoint. Full portable parity, release
clean/full-format verification and publication remain separate completion
requirements. Physical desktop operations still require an unlocked session.
