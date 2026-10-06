# Official Benchmark callback port

Baseline: official 7-Zip 26.03 `GUI/BenchmarkDialog.cpp/.rc`,
`Common/Bench.cpp/.h`, `Common/CompressCall2.cpp` and `Console/BenchCon.cpp`.
The bundled GUI now uses the Windows File Manager's callback-driven benchmark
path rather than parsing the console's rounded table and relaunching each pass.

## Reused implementation

The hash-checked console overlay selects `PortBenchmark::Run` only for a GUI
request with a private progress channel. Ordinary `7zz b` behavior is unchanged.
Each pass calls the original `Bench` with `IBenchCallback`, one iteration,
`multiDict=false` and the first-pass frequency callback, as in
`CThreadBenchmark::Process`. All coding, test data, CPU time and rating routines
remain the official engine implementation.

`scripts/import-benchmark.py` imports the unchanged Windows `CTotalBenchRes2`,
GIPS/usage/log formatting and the official memory-estimate bodies. Original
source hashes and license attribution accompany both generated includes.
`CTotalBenchRes::Generate_From_BenchInfo` and `Update_With_Res` are linked from
the official engine. Current encoding reduces the rating dictionary to the
processed input size when the callback is not final, following the Windows GUI.
Only final results enter the cumulative values. Full byte counts and unrounded
speed/usage/rating values are accumulated before presentation.

The POSIX helper adapts the Windows synchronization/post-message boundary to a
mutex and nonblocking datagrams. It retains the first five and latest fifteen
pass rows when the twenty-row log fills, with the upstream ellipsis position.
CPU, OS/features, process-thread information and version strings reuse the
official system routines. The POSIX affinity class lacks the Windows convenience
method; its existing Get/count/fallback routines provide the corresponding data.

## Controls and process lifecycle

- Dictionary choices include the 3/2 steps: 256 KB, 384 KB, 512 KB, 768 KB,
  through the architecture limit (4 GB on this arm64 Mac). The engine receives
  the exact selected size, avoiding the console sweep's power-of-two rounding.
- CPU thread choices use one and even counts through twice the hardware count;
  the initial count is normalized as in the Windows dialog.
- The fresh File Manager default is **10 passes**, confirmed through
  `CompressCall2.cpp` / `GUI.cpp` and `k_NumBenchIterations_Default`, rather
  than the earlier port default of one. Existing port choices are preserved.
- Compressing/Decompressing groups are stacked with Size, CPU Usage, Speed,
  Rating / Usage and Rating; Current and Resulting are separate. Total Rating
  shows usage, rating/usage and rating after the final pass. Ratings use GIPS
  with the upstream three decimal places. The final elapsed time has milliseconds.
- Restart/Stop, live control changes, Cancel and Help remain asynchronous.
  Stop terminates only the owned helper and has a bounded kill fallback.
  Destruction disconnects callbacks before process/member teardown.

`BenchmarkRunner` owns the process and progress channel; no shell interpolation
or password is used. Numeric protocol values are decimal strings and validated
before model updates. Invalid/overflowing records are ignored; nonzero exit,
failed startup and missing completion produce an operation/exit-code/diagnostic
message. A dropped consumer cannot block the engine. The formatter test
executable is a build dependency, not part of the distributed application.

## Executed checks

- Native Benchmark formatter: **4 passed**, including undefined values,
  accumulation before rounding, GIPS/usage/MB formatting and the 1-TB size
  threshold. The synthetic accumulation distinguishes the previous rounded
  pass-average behavior from the official calculation.
- Final Qt Benchmark suite: **9 passed / 0 failed / 0 skipped**. A real
  384-KB/two-pass run proves intermediate callbacks, GUI heartbeat, actual input
  size, cumulative counts and frequency/system output. Twenty memory cases
  compare the Qt import with the engine's original function. Other cases cover
  UInt64 precision, invalid records, exact arguments, failure/recovery, dictionary
  and custom pass controls, Current/Resulting/Size/GIPS labels, Restart, Stop,
  active destruction and the fresh ten-pass default.
- Existing integration Benchmark/Stop check: **3 passed** including setup and
  cleanup. The affected shared progress suite: **13 passed**, including real
  7z/ZIP/encrypted round trips, Pause/Cancel and a dropped consumer.

The twenty-row retention logic is source-matched; a twenty-pass physical GUI run
was not performed. Native pixel comparison and physical controls remain pending
while locked. The port retains its last selected Benchmark controls, an existing
port preference; the Windows File Manager initializes a fresh dialog from its
command defaults. These tests do not establish complete application parity.
See [benchmark-help-test.log](distribution.md).

## Help search controls in the same checkpoint

The Search tab now has an editable recent-query dropdown and an AND/OR/NOT/NEAR
insertion menu. Successful searches, including zero hits, move to the front;
invalid expressions leave the history unchanged. The port keeps twenty distinct
valid queries and restores them in another Help window. This capacity is a port
choice, not a verified Windows HTML Help capacity. Literal queries/operators
are preserved when changing the UI language. Search expressions still use the
existing worker and its limits; the new controls do not change search semantics.

The final Help/compression suite has **18 passing checks**. It includes history
selection/reordering/reopening/bounds, operator insertion at the cursor and over
a selection, Enter submission, invalid/oversized input and Japanese UI changes,
in addition to the original pages, search, context topics and PDF checks.
Microsoft's [advanced-search documentation](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/htmlhelp/adding-advanced-full-text-search-to-a-help-project)
describes the Windows viewer's Boolean/wildcard/nested search and three filtering
options. That OS engine is not provided in 7-Zip source; linguistic/ranking,
exact NEAR distance and translated help content remain documented differences.
