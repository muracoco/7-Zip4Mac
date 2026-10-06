# Original pre-stream overwrite channel

This is an implemented and tested bridge component. Normal File Manager
extraction still uses its existing private-staging pipeline; public-output
integration is unfinished. It is not a completed pre-stream Ask/Skip feature
in the application.

`scripts/import-progress.py` verifies the original 26.03
`UI/Common/ArchiveExtractCallback.cpp` hash
`f8bc44e06d292d82213652a6ee5b776310998e097c8c475036fe217b4d7fbc5f`.
The imported `CheckExistFile` body retains all original Skip, six-answer Ask,
overwrite-mode transitions, auto-renaming and filesystem operations. Its
Ask call can now use `NativeOverwrite` instead of the console's stdin dialog.
No overwrite policy or compression algorithm is reimplemented.

The native callback sends a typed request over the inherited Unix datagram
channel. UTF-8 names, optional full-width sizes/FILETIMEs and directory flags
reach Qt. The GUI thread receives a signal and replies without blocking;
only the native callback waits. Password stdin remains separate. This code
does not log request paths or any password.

Replies are scoped to a channel generation and native request ID. Duplicate
requests do not create duplicate prompts, repeated replies are rejected,
and a reply from an earlier process cannot authorize a new request with the
same native ID. The callback retries a dropped request, observes the original
console's Cancel signal, and aborts on peer/parent loss. Optional progress
telemetry remains nonblocking and independent from required decisions.

The bridge is explicitly enabled by the internal `SEVENZIP_PORT_OVERWRITE_RPC`
flag. Application launches remove an ambient flag so ordinary extraction cannot
accidentally enter a callback for which its public-output adapter is not ready.

## Executed verification

Eleven new native callback cases run the rebuilt official helper against disposable actual output
directories; they do not use an inferred file-policy mock:

- Yes, No, Yes to All, No to All, Auto Rename, Cancel and No/Yes/No sequences
  match the untouched original console's names, bytes, exit code and prompt count.
- Closing the response channel or signaling Cancel aborts before writing the protected file.
- Delayed replies cause native retries without a second prompt; reusing the
  process/channel rejects the prior generation's response.
- A genuinely CRC-damaged ZIP fails original `Test`, but No to All at the
  original pre-stream callback succeeds without decoding/reporting that CRC
  failure and preserves the existing file.

All new cases pass. An additional protocol check verifies exact UInt64 sizes/FILETIMEs and rejects numeric/overflow/malformed fields. Full affected suites: extraction metadata **263**, progress
**15**: **278 passed, no failures/skips**, including setup/cleanup.
[Executed output](distribution.md).

## Application integration remaining

The original callback must see the combined private/public output state through
safe descriptor-based metadata queries. Rename/delete primitives must retain
approved output identity and use guarded installation, rather than modifying
user files through unprotected path calls. Reuse the original auto-name lookup
algorithm, not a separately recreated policy.

Install each callback-authorized completed output before the next stream's
decision where possible. This follows the upstream operation order and avoids
reconstructing all public changes after decompression. Deferred links, creation
times, directory finalization, cancellation and failed outputs must retain
their original callback order and the existing source/destination guards.
This publication/session integration is still to be implemented and verified;
the standalone component checks do not prove it.

The ordered installation session and original metadata-query adapters now have
separate passing checks. See [session](extraction-session.md) and
[output-state queries](native-output-state.md). They remain prerequisites;
application activation is still unfinished.
