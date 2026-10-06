# External archive-file editing

Specification: official 7-Zip 26.03 `PanelItems.cpp::EditItem/OpenSelectedItems`,
`PanelItemOpen.cpp::MyThreadFunction/CTmpProcessInfo` and `AgentOut.cpp::UpdateOneFile`.
For regular-file viewing/editing, F3/F4 operate on the focused file even with multiple selection.
F3's separate filesystem-folder operation is described in [folder statistics](folder-statistics.md).

## Launch and write-back

A safely extracted file gets a size/mtime baseline, exact entry, virtual folder,
physical archive, format, confirmed extraction password and read-only parent
state. Passwords remain in memory and reach 7zz through stdin, never through
editor arguments, environment, preferences or logs.

Configured commands are split without a shell. Mac apps use
`open -W -a application file --args application-arguments`; the default uses
`open -W file`. This waits for the application rather than its launch helper.
CLI editors spawn in a separate session. `waitpid` and libproc follow ordinary
group descendants and observed children that change session/group. PID birth
times prevent confusing reused PIDs. Unknown observation errors defer completion.
Observers never kill editors and outlive a closed panel to reap launchers.
The original CChildProcesses::Update body now discovers independently running
processes with the same executable basename after a short unchanged launcher,
using libproc in place of Windows snapshot/handle APIs. [Source port and checks](external-process-port.md).

After completion, changed size/mtime shows the translated upstream
`File '{0}' was modified.\nDo you want to update it in the archive?` question,
default Yes. No **and** Cancel discard, as in Windows. Read-only edits warn and
retain the copy. Yes requires the same physical archive/virtual folder, an
existing regular entry and a currently writable chain. Navigation cannot silently
redirect an update. Failure/Cancel retains the edit and reports its recovery path,
target, exit code and available details.

`ReplaceFile` copies on the archive's volume, updates only the exact existing
native item through the imported official Agent UpdateOneFile body, verifies
listings/untouched fields and decoded selected-entry size/SHA-256, rechecks both sources, preserves filesystem metadata and atomically
installs. No unrelated whole-archive Test is required. Same-name siblings, native
row restoration and pending-session rebinding are covered by
[the source port and checks](native-agent-replacement.md). Official 7zz may recompress solid blocks with a different Method;
Method/Packed Size/Block/Offset may change. Unrelated size, CRC, encryption,
timestamps and stored attributes must remain unchanged. Tests independently
extract and compare unrelated bytes, including a stored 4 MiB entry recompressed
by the official engine. The final check/rename interval is not a global lock.

Editors run without blocking browsing. Decisions wait for other jobs/modals/
popup menus. Both panels share busy state and serialize edits of one archive.
Debounced refresh waits during a modal decision or another job. Explicit archive
Refresh re-lists the physical archive while retaining its internal prefix and
confirmed password. Beginning Manager closure stops background Refresh and editor
decisions, including the queued-close interval after parent write-back. Nested
Edit updates the extracted child; exiting its level separately confirms parent
write-back. Successful sibling sessions cannot delete a group's recovery copy.

## Lifetime, differences and limits

Closing a panel/Manager keeps running editors and their files, even unchanged
files. An unchanged launcher finishing under two seconds enters upstream complex
mode: retain it during the Manager session without claiming a later automatic
question. On exit this port conservatively keeps even unchanged complex copies,
because an unobserved editor may still use them; Windows deletes unchanged ones.
An observed unchanged editor lasting at least two seconds cleans up on completion.

Polling cannot prove the lifetime of arbitrary daemons escaping/reparenting before
first observation. Use `open -W` for Mac apps. Original same-executable discovery
now covers independent IPC hand-off to matching executable names. Unrelated
executables without an observable relationship are not guaranteed by the Windows
algorithm either. Changes preserving size and mtime
are undetected, as in the upstream baseline. Folder Open Outside/drag-out retain
directories without regular-file write-back. Multiple-item Open now imports the original policy and uses separate native-row
extractions, including same-name ZIP siblings and confirmed write-back.
[Source and tests](panel-open-port.md). Unknown-extension default filesystem
detection and filename warnings now use the [official opening profile](open-profile-port.md).
Link traversal, unsupported layouts, mixed passwords
and reopening a nested level into a new physical temporary context remain incomplete.

## Tests

`editor_writeback` runs in both normal and `--no-focus` scripts. Disposable CLI
editors and a disposable background `.app` exercise real LaunchServices without
quitting user applications or changing default associations. Cases cover seven
formats, 7z data/header and ZIP AES/ZipCrypto, Unicode/spaces/literal wildcards,
Yes/No/Cancel, focused-only selection, read-only/context/missing-entry recovery,
nested encrypted write-back, Cancel, two panels, descendants including observed
new-session children, close survival, short launchers, missing programs and
nonzero exits. See [execution results](test-results.md).

The user's default/already-running application, physical menus/Fn keys and Finder
interaction remain separate unlocked-desktop checks.
