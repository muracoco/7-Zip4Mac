# Extraction path and conflict checkpoint (2026-10-05)

The normal backend now maps existing hard-reference targets through duplicate
root elimination. Private callback paths retain their original prefix, while
public references use the reduced output root. The same mapping handles
native selection from an ordinary Agent subfolder. Archive decoding and link
creation remain the original official callback operations.

Twenty new actual-file cases compare with untouched official 7zz:

- Four hard-reference cases: legacy/native root elimination, native current
  folder selection, and current folder plus root elimination. Selected aliases
  use the existing public bytes and share the real target inode.
- Sixteen file/directory conflicts: empty/nonempty existing directories,
  incoming directories over files, and read-only existing files, across the
  four automatic overwrite modes. Exit codes, tree types, names and bytes
  match original output; protected children are retained.

The old selected Pause/Cancel regression assumed output rollback. It now
runs untouched 7zz, sends SIGTERM to that owned extraction, and verifies both
original and application retain a partial stream with correct source-prefix
bytes and exit code 255. The GUI heartbeat and subsequent Test reuse remain
verified. This is an upstream-behavior correction to the test.

Final affected checks: metadata 310, progress 15, selected Agent paths 9,
basic roundtrip/root/absolute integration 5: **339 pass, 0 fail/skip**, including
setup/cleanup. Application and affected test targets build successfully.
[Captured output](distribution.md).
The first Agent invocation omitted its fixture directory and failed setup;
the corrected test-agent-tree.sh invocation generated private fixtures.
The initial hard-reference cases exposed the root mapping error before repair.

These results do not prove every handler/link variant or crash/I/O combination.
Intermittent SMB change detection, physical native interaction, remaining UI,
final empty-directory release build, license/privacy review and publication
remain in the completion inventory. A new packaged-app launch was not repeated
for this scoped source checkpoint.
