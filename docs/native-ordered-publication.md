# Original callback ordered publication

The initial checkpoint below tested the native component. The normal backend
now dispatches it on a sequential asynchronous worker; see
[normal extraction integration](normal-extraction-integration.md). Remaining
link/root-prefix combinations and release requirements are recorded there.

The imported `CheckExistFile` body retains its decisions and now delegates
its existing rename/remove primitives to `PortExtraction`. With the internal
bridge disabled, these call the original platform functions. With it enabled,
the host validates the original operation against an approved output snapshot,
then the helper mirrors it in descriptor-guarded private staging.

`ExtractionSession` owns one sequential worker's output state. The initial
CheckExistFile observation is distinct from subsequent existence probes;
answering Yes cannot authorize a file that replaced the initially shown item.
Original auto-name probes supply the destination token for rename-existing.
Approved previous outputs are moved into private preservation directories with
the existing guarded rename primitive. Failed operations retain recovery data.

After freezing a completed stream, the original callback sends a required
publication message and waits for acknowledgement. `ExtractionInstaller`
publishes it before the next original GetStream. Original Skip/mode transitions
therefore observe the preceding real outputs; the host does not reconstruct
these policy choices at process exit. CloseArc publication/finalization can
still complete after Cancel. Generation-scoped, one-shot replies and peer loss
handling apply to mutation, publication and finish messages.

The `SEVENZIP_PORT_EXTRACTION_RPC` flag requires the metadata decision channel
and output-record configuration. Normal application launches remove ambient
flags before enabling the required channels for an owned extraction session.

## Executed checks on 2026-10-05

Sixteen new actual-file cases use the rebuilt official helper and production
session against untouched console output:

- 7z and ZIP: Overwrite, Skip, Auto Rename and Auto Rename Existing, each with
  three colliding streams. The next CheckExistFile sees the previous published
  bytes. Final names and bytes match original console results.
- ZIP: all six Ask choices and a No/Yes/No sequence. Native prompt counts,
  normal/Cancel exit codes, output names and bytes match the original.
- Replacing the approved public file after the prompt causes ESTALE. The
  replacement and saved original remain unchanged; no incoming stream opens
  or publishes. Later raw existence probing cannot refresh that approval.

Affected checks: extraction metadata **290**, native progress **15** —
**305 passed / 0 failed / 0 skipped**, including setup/cleanup. Native helper
and Qt application targets build successfully.
[Executed output](distribution.md).

These results do not prove all directory/link/cancel/I/O combinations, normal
application activation, a new packaged-app launch or complete Windows parity.
