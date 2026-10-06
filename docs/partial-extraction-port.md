# Original extraction failure outputs

This checkpoint ports normal extraction failure behavior; it is not full
Windows parity or a final release.

## Original code and host boundary

`scripts/import-progress.py` still verifies the pinned official 26.03
`UI/Common/ArchiveExtractCallback.cpp` hash:
`f8bc44e06d292d82213652a6ee5b776310998e097c8c475036fe217b4d7fbc5f`.
Original stream closing, decompression, timestamps, attributes, link creation
and directory finalization remain in the upstream function bodies.

- `SetOperationResult` closes the original stream and sets attributes even when
  its result is CRC/data/unsupported-method failure. The port now records those
  actual outputs rather than requiring `kOK`. A bad output remains a bad output;
  the engine's nonzero exit code and diagnostic are retained.
- `SetLink` registers its private empty placeholder. The port records the final
  link only after the original `SetPostLinks` creates it and applies metadata;
  unsuccessful placeholders cannot become public regular files.
- `CloseArc` records a still-open ordinary stream after original
  `CloseReparseAndFile`. It runs the pending split-output recorder after the
  original post-link/directory cleanup even when cleanup fails, without masking
  the original HRESULT.
- On a normal nonzero process exit after extraction begins, the Qt backend
  validates and installs recorded outputs using the existing guarded installer.
  Its result retains operation, archive target, native exit code, password
  classification and redacted stdout/stderr. Host installation errors are
  appended to the native failure message, not converted to success or a generic
  replacement exit code. A host-only installation error remains code `-1`.
- Folder metadata finalization runs after partial host installation as well as
  success, in deepest-first order. Identity guards remain active and restoration
  failures are appended to the operation error.

The public output tree is not exposed to the native callback. Existing source,
destination, parent-symlink and output identity guards continue to apply.

## Executed checks

On the current arm64 Mac, Qt 6.11.3 and official 7-Zip 26.03:

- 12 independent-output comparisons: genuine 7z CRC damage, ZIP CRC damage and
  ZIP unsupported method, native/legacy selection, overwrite and rename-existing.
  Compare names, bytes, file mode/mtime, explicit empty-folder mode/mtime and
  native failure details. Test subsequent bad and good requests on the same backend.
- 3 wrong-password output comparisons: 7z data encryption, ZIP AES and ZipCrypto;
  then retry with the correct password and compare original input bytes. Passwords
  go through stdin; results/logs must not expose the supplied secret.
- 1 host installation failure: preserve the first output and protected children
  while finalizing the archived folder's mode/mtime, compared to pristine console.
- 1 additional Cocoa/Qt progress case: actual CRC failure, visible diagnostic and
  exit code, enabled Close button, Qt mouse-click closure without a Cancel signal,
  and a successful next operation. This is synthetic widget interaction, not a
  physical AppKit/Finder mouse test.

Final affected checks: extraction metadata **252**, progress **14**, transfer
**30**, open modes **24**: **320 passed, no failures/skips**. Counts include each
suite's setup/cleanup. [Executed output](distribution.md).
The unchanged successful-format matrix and final empty-directory release build
were not repeated for this operation-specific checkpoint.

## Remaining work

Engine preparation still uses private `-aoa`; the original pre-stream overwrite
callback must be connected for decode Skip, prompt timing and all file/directory
conflicts. Cancellation during engine preparation and abnormal process exits
still discard private outputs. Successful-close recording is groundwork for
that integration, not evidence that cancellation output parity is finished.
Further mixed-link/overwrite/error combinations, absolute stored link/root
elimination and the intermittent SMB change-detection problem remain open.
Failed/deferred dangerous link placeholders are deliberately not published.
The overall UI/list/Help, physical interaction, clean-build, license/private
content audit and public repository gates remain unfinished.
