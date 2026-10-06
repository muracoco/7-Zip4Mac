# Official external-process discovery

The macOS observer now imports `CChildProcesses::Update` from official 7-Zip
26.03 `CPP/7zip/UI/FileManager/PanelItemOpen.cpp`. Its original body is retained
in `src/upstream/ProcessDiscovery.inc`, including copyright and LGPL notices.
`scripts/import-process-discovery.py` verifies SHA-256
`e21c806419c66314bed2def3e4e91562991d323bf4530bd403e364b0104251a2`.

Qt supplies the string/vector boundary. `libproc` supplies process snapshots,
parents, executable names and birth times in place of Windows Toolhelp and
process handles. Birth-time checks reject reused PIDs. The imported loop follows
known parents and, when requested, matching executable basenames without regard
to case. Observation never terminates an editor or another user's application.

The original `MyThreadFunction` requests same-name discovery when the initial
process exits in less than two seconds without changing the extracted file.
The macOS observer now applies that condition and waits for the independently
running process it finds. Existing group/child observation remains active, so
ordinary forked children and observed children that change session/group are
still followed. A changed file or a longer-lived initial process does not enable
same-name discovery, matching the original condition.

For Mac applications, `open -W` is the LaunchServices substitute for the Windows
application process handle. `/usr/bin/open` itself is excluded from same-name
discovery; waiting for other unrelated `open` helpers would not represent the
application selected by the user.

This closes the previous missing same-executable discovery path. Arbitrary IPC
to a different executable with no observable parent/child relationship is not
guaranteed by the official Windows algorithm either. Same-name discovery can
wait for another instance of that executable, as in Windows. The previously
documented conservative preservation of unknown temporary copies on Manager
exit remains an explicit port behavior. Multi-item Open was subsequently imported and tested; see
[the command port](panel-open-port.md).

## Executed checks

The application and editor test targets build on the target Mac. Final
`editor_writeback` results: **36 passed, 0 failed, 0 skipped**, including
setup/cleanup. See [the execution log](distribution.md).

A disposable executable starts a server before the observed launcher. It
communicates the Japanese/space-containing filename through an owned temporary
file, exits, then the independently running server modifies the file. The
observer waits for the server; actual ZIP Edit shows one confirmation, uses
official UpdateOneFile, and independent extraction verifies the modified and
untouched bytes. Changed/long-lived clients verify the original exclusions.
Existing seven-format, encrypted, nested, two-panel, cancellation, close-survival,
descendant, LaunchServices and failure checks also pass.

```bash
source scripts/env.sh
cmake --build "$DEPS/build-comments-20261004" --target SevenZipMac editor_tests
ctest --test-dir "$DEPS/build-comments-20261004" -R '^editor_writeback$' -V
```

The fixtures never quit user applications or change associations. The desktop
was rechecked and remains locked. Physical Finder/default-application checks,
the final empty-directory release build and full parity/publication are pending.
The unchanged archive engine does not require another complete format-matrix
run for this process-observation change.
