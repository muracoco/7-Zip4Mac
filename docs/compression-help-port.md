# Official compression calculations and HTML Help

The engine remains the official, source-built 7-Zip 26.03. This increment
additionally reuses Windows GUI arithmetic and original Help content.

## Compression

`scripts/import-compression-math.py <official-source-directory>` verifies
`CPP/7zip/UI/GUI/CompressDialog.cpp` by SHA-256 and generates
`src/upstream/CompressMath.inc`. `Get_Lzma2_ChunkSize` and
`GetMemoryUsage_Threads_Dict_DecompMem` retain their arithmetic bodies.
Automatic thread/solid arithmetic retains its body with Win32 combo plumbing
removed. `CompressionMath.cpp` adapts the input accessors, level-dependent
dictionary/word defaults and choice ranges from `SetDictionary2` / `SetOrder2`.

Add shows `*` automatic choices for method, dictionary, word/order, solid block,
threads and the 80% memory limit, plus compression/decompression estimates and
hardware thread count. Changes recalculate the displayed values; the memory
limit can reduce automatic LZMA2/ZIP threads. The 64-bit Windows GUI caps level-9
LZMA/LZMA2 automatic dictionary at 256 MiB, and the port uses that cap. Values are
GUI estimates rather than measured allocation.

Automatic properties remain unspecified: empty dictionary/word/solid/memory,
zero threads and `methodAutomatic`. The backend omits those CLI switches so the
original engine normalizes defaults. Manual values use the original property
names. Both states are saved per format; existing saved manual values are
retained. Passwords are never saved.

OK rejects invalid sizes/counts, insufficient selected memory limit, non-ASCII
ZIP passwords and AES ZIP passwords exceeding the upstream 99-byte limit.
Show Password permits proceeding without matching hidden reentry, as upstream
does. Public `accept()` is also used by programmatic callers; the backend still
reports malformed engine settings as operation failures.

The remaining control/state work was subsequently ported using the official
format tables and numeric combo builders; see [compression-controls-port.md](compression-controls-port.md).
Dynamic level/solid labels and advanced time/link conditions now follow the original resources and compiled handler flags; see [compression-options-port.md](compression-options-port.md). Native dimensions and other translation coverage remain. Upstream stores one
last method/settings set per format, not separate per-method histories.
Parameters are not a measurement of actual memory. Tested round trips do not
establish every compression-property combination.

## Help

`scripts/import-help.py <7zz> <official-7-zip.chm>` verifies the pinned official
Windows 26.03 CHM hash and extracts its fixed inventory without executing the
installer. All **70 HTML / 8 CSS / TOC / index** files retain original bytes,
encoding, CRLF and notices. `resources/help/manifest.json` records every hash,
70 Contents and 69 Index entries. The retained Windows-bundle license covers
files other than 7z.dll under LGPL 2.1 or later.

Qt Help provides Contents, filtered Index and Back/Forward. F1 opens
`fm/index.htm`; Add, Extract, advanced Options, Benchmark and the six Options
pages use their upstream context topics/fragments.

Only manifest-listed HTML/CSS inside `qrc:/help` can load. Arbitrary files,
remote resources, queries and unknown schemes are blocked. Clicks on the three
original 7-zip.org destinations can open the system browser. CHM paths resolve
case-insensitively. Two incorrect Add-page relative links are resolved explicitly
without modifying HTML. Missing/inconsistently capitalized upstream method
anchors retain their original behavior; the containing page opens.

Full-text Search, topic/subtopic printing and About F1 were subsequently added;
see [current implementation and checks](native-progress-help.md). Search-engine
linguistics/ranking and temporary-cleanup context Help
and native desktop checks remain. Original pages are English and describe
Windows; complete HTML Help UI parity is not claimed.

## Executed validation

New suite: **13 passed / 0 failed / 0 skipped**. It checks source defaults and
memory reduction, automatic/manual persistence, normal/Store 7z/ZIP round trips
with SHA-256 and Japanese/space names, automatic encrypted 7z including wrong
password, GUI memory/password errors, all 80 hashes, all 70 pages/links and
Contents/Index entries, blocked resources and contextual Qt buttons/actions.

Selected compression integration: **10 passed / 0 failed / 0 skipped**. 32 MiB
round trips, existing 7z/ZIP methods, TAR/WIM/stream formats, working-folder/memory
settings, verified Trash/update modes, Open With and Hash output.

Run `ctest --test-dir <build> -V -R '^compression_help$'` after building, or
`scripts/test.sh` (including `--no-focus`). Native clicks remain pending while
locked. Final full regression, format matrix and clean build remain scheduled;
unrelated suites were not repeated for this checkpoint.
