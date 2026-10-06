# Filesystem transfer and source verification

Baseline: official Windows 7-Zip 26.03. This checkpoint implements the missing
rename-first filesystem Move path and Pause during Delete-after verification.
Filesystem-to-archive Copy/Move is a separate operation and remains incomplete.

## Official operation sequence

`UI/FileManager/FSFolderCopy.cpp::CopyFile_Ask` rejects self-copy/move before
`AskWrite`, applies its output name/decision, and calls `MoveFile_Sys` for Move.
`CopyFolder` first tries moving the whole folder, then creates/merges the output
and walks children when that is unavailable. These Win32 system calls are not
portable; the port follows their operation sequence using POSIX directory handles
and the existing shared overwrite/installation layer.

## Move

Same-volume files, raw symbolic links and new destination folders first use
rename. Files retain their inode, creation time, permissions, ACL and xattrs;
whole-folder moves retain child identities and relative links. Existing folder
destinations merge, and skipped children remain at the source. All six overwrite
answers apply to Move as well as Copy/extraction. A self-transfer fails before
asking for overwrite approval.

The source is captured in an owned private directory. Public installation uses
Darwin exclusive rename or swap. Post-install validation protects concurrent
destination changes; rollback restores the source/previous destination when
possible. A reused source path is retained and the original is reported at its
recovery location. Cancel before installation restores the source; after the
commit point a completed move remains completed. Cross-device transfers fall
back to copy followed by guarded source removal.

## Filesystems without exclusive rename/swap

The current Windows-hosted SMB volume returns ENOTSUP for Darwin RENAME_EXCL /
RENAME_SWAP and hard links. Installation therefore has a regular-file fallback:

- Capture an existing output in an owned private directory and validate it.
- Create the public output with O_CREAT | O_EXCL | O_NOFOLLOW; a concurrently
  appearing public entry is never overwritten by this creation.
- Copy prepared bytes/metadata, flush and close, then revalidate the known open
  file identity and capture the final post-close change time. SMB can commit
  metadata at close; that change must not be confused with an external edit.
  The timing diagnosis is consistent with Microsoft's [file-time guarantees](https://learn.microsoft.com/en-us/windows/win32/sysinfo/file-times);
  the precise SMB cache transition was not independently traced.
- Rename Existing creates its backup exclusively. Previous data is removed only
  after successful validation; errors report retained recovery files.

This path makes the new output visible during copying and cannot provide APFS's
atomic swap. Failure/cancellation after capturing an old output may leave that
old data in the reported recovery directory. Owned partial outputs are removed
when they can be captured and validated; permission/interference failures may
leave an output or recovery data requiring inspection. Public files are never
installed with an unconditional overwrite rename. Ordinary rename is used only
into freshly created private directories. Unsupported filesystem link types
remain errors, with source/previous data retained.

## Verification before Trash

Delete-after still verifies the committed archive and source payloads before the
macOS Trash substitution. `WorkerProcess` owns its QProcess on the worker thread;
the same thread sends SIGSTOP/SIGCONT, waits, kills and reaps it. A child PID never
crosses to the GUI thread. Cooperative Pause also covers enumeration and local
CRC/SHA checks. Cancel wakes paused work and retains sources before Trash starts.
If cancelled while moving several roots to Trash, completed moves can remain.

Verification uses the bundled instrumented official engine when available.
Native Test/extract snapshots are drained without a worker event loop. Local
checks report source-byte totals/counts and sampled completion. Generation/control
checks reject stale queued updates. The decompression memory limit is forwarded.
There is no fixed duration limit for normal verification; explicit test timeouts
exclude paused time. UTF-8 stdout/stderr are bounded and passwords redacted; secrets
are supplied on stdin. Engine failures retain their exit code and diagnostics in
the worker result/error message.

Mixed-password archives, full archive metadata/link restoration and remaining
progress layout/wording still have the gaps recorded in the broader inventories.
Blocked kernel I/O must return before a cooperative disk operation can pause.

## Executed evidence

- Native progress/worker: **13 passed / 0 failed / 0 skipped**. Real child
  heartbeat freeze/resume, cancel while paused, timeout pause accounting, UTF-8
  errors/secret redaction, GUI verification Pause/Cancel, encrypted source
  verification/native callbacks, round trips and existing protocol cases.
- Overwrite/Move: **18 passed / 0 failed / 0 skipped**. All six choices now also
  cover Move. Inode/birth-time/xattr/read-only metadata, whole folders/links,
  changed/new destinations, source reuse/recovery, cancellation and backups.
  Actual SSD-to-SMB and SMB-to-SSD files/folders, Japanese/empty folders,
  same-name replacement and Rename Existing pass.
- Affected integration: **11 passed / 0 failed / 0 skipped**. 32 MiB 7z/ZIP,
  SHA-256, selected overwrite, errors/recovery, both Pause/Cancel paths, filesystem
  Copy/Move and verified Trash across archive formats including CRC-less data.

Totals include setup/cleanup. Targeted CTest **2/2** and incremental Release build
pass. The SMB test first found unsupported rename flags, then stale metadata
snapshots around handle closing; the results above are after both fixes.
[Executed logs](filesystem-transfer-test.log).

```bash
PORT_TEST_SECOND_VOLUME=/writable/folder/on/another/volume \
  ctest --test-dir <build> -V -R '^(native_progress|overwrite)$'
```

The cross-device test uses only its own temporary directories. Without a second
volume it reports that subcase as skipped. Full clean/regression/format checks
remain scheduled for the final feature checkpoint; native desktop clicks remain
pending while the session is locked.
