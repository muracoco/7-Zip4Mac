# Original extraction output-state queries

This is a tested native bridge component. Normal application extraction still
uses private staging followed by guarded installation; the output-state bridge
is not enabled there. Publication and overwrite mutation integration remain
required before switching that pipeline.

The original 26.03 `CheckExistFile` now has a metadata-query adapter before its
existing branch. Its Skip, Ask, mode transitions and rename/delete bodies are
retained. The original `Common/FilePathAutoRename.cpp` is also imported, with
only its existence lookup adapted. Its extension/dotfile rules, binary search,
limits and suffix construction remain upstream code, rather than a separate
implementation. The importer verifies its SHA-256:
`92af840c16598d207a1280da01bb4b2c86f15863fd75724d1070ecfebff4c437`.
Upstream source files and the pristine console binary are unchanged.

The native callback sends an absolute UTF-8 path through the inherited decision
socket. Qt returns metadata from descriptor-based `FileInstall::inspect`, which
does not follow parent symlinks or create missing directories. Missing parents
mean an absent output; permission/symlink/I/O failures retain their numeric OS
error. Original `CFileInfoBase::SetFrom_stat` interprets that metadata, including
directory sizes, file types and original time conversion.

Replies are one-shot and scoped to request ID and channel generation. The child
waits and observes Cancel/parent/peer loss; the GUI event loop remains active.
The private response contains a zero-padded stat record plus a hexadecimal
filesystem leaf name. Both peers run on the same Mac/architecture; the native
parser checks the record size and leaf components. This is not a stored or
cross-platform wire format. No password or file data is transmitted.

`SEVENZIP_PORT_OUTPUT_STATE_RPC=1` enables the component only in explicit native
tests. Application launches remove an ambient flag. In particular, virtual
metadata must not activate the original public-file mutations before guarded
mutation plans and callback-order publication are connected.

## Executed checks on 2026-10-05

Nine new actual-file cases run the native helper, not a re-created policy:

- A ZIP with genuine CRC inconsistency fails pristine Test. Original Skip uses
  remote file/directory/symlink metadata, avoids decoding, returns success and
  preserves all protected files. Private staging has no incoming output.
- Original auto-renaming searches occupied public names, while writing only to
  private staging. Extension and dotfile names match untouched console output.
- A public parent symlink produces a numeric error and aborts before the native
  output stream opens. The outside test file remains unchanged. A missing parent
  is reported absent without being created.
- A sparse 5 GiB file's exact size and original 100 ns modification FILETIME
  reach the original Ask callback through the metadata adapter.
- Closing the peer or canceling during a query returns 255 before output creation.

Affected suites: metadata **274**, progress **15** — **289 passed, no failures
or skips**, including setup/cleanup. The native helper and Qt application target
build successfully. [Executed output](native-output-state-test.log).

This increment does not claim an application pre-stream overwrite switch,
new packaged-app launch, physical GUI verification or completed Windows parity.
