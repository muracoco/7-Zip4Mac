# Official compression controls and state transitions

Baseline: official 7-Zip 26.03 `GUI/CompressDialog.cpp/.h/.rc` and
`Common/ZipRegistry.h`. The engine remains source-built official 7-Zip.

## Imported source

`scripts/import-compression-math.py` checks SHA-256
`53d673d78e7936880481be1017c37ca95a1e4b64419d74d32035ce703d06c7d4`
before generating three includes. `CompressTables.inc` retains the upstream
method enum, names, format arrays, level masks and flags. `CompressControls.inc`
retains the numeric branches of `SetDictionary2` and `SetOrder2`; registry
lookup and Win32 rectangle manipulation are replaced by saved-value arguments
and a small numeric-combo adapter. `CompressMath.inc` retains the existing
memory/thread/solid arithmetic and now also supplies the algorithm thread limit.
Original comments, attribution and LGPL notices remain.

Dictionary/order defaults, choices, nearest saved selections, LZMA's 3.75-GiB
cap and PPMd's displayed-versus-real dictionary sizes consequently come from
the original routines. The command and memory estimate use each item's real
numeric value rather than its displayed size.

Qt adapts `SetMethod2`, `EnableMultiCombo`, `SetNumThreads2`,
`SetSolidBlockSize2`, `SetMemUseCombo` and `OnCommand`:

- Levels contain only supported values; Hash has an empty level control.
- The first method is one automatic entry, with no duplicated manual entry.
  The Windows GUI deliberately omits Copy/Deflate/Deflate64 from 7z's method
  list. Those codecs remain available in the official backend. Store clears the
  method list; WIM has no method selection. One-entry controls are disabled.
- Threads stop at the lesser of twice the hardware count and the algorithm
  maximum. PPMd's Auto 1 control is disabled. Automatic threads still use the
  official memory-aware calculation.
- XZ omits Non-solid. SevenZip/XZ solid choices and the memory flags follow
  the official format definitions. Store retains supported memory controls.
  The 64-bit memory list now reaches 16 TiB, as the upstream loop specifies.
- Format switches save drafts in memory and restore them when returning.
  OK persists all visited formats; Cancel persists none. Passwords are excluded.
- Changing level resets method/dictionary/order/solid/threads to automatic,
  following `CFormatOptions::ResetForLevelChange`. Parameters/memory remain.
- Changing dictionary resets a finite fixed solid size to automatic; Non-solid
  and Solid remain. Method-specific numeric choices restore only when the
  saved method matches, following `IsMethodEqualTo`.
- The last format is remembered for the ordinary toolbar Add operation.
  Explicit format/extension requests retain precedence. Selecting an archive
  history item changes the format using its known extension. Add history uses
  the upstream twenty-entry bound.
- Show Password hides the reentry control. Solid commands and persisted values
  use stable item data, retaining off/on semantics with a Japanese UI.

Upstream stores one last method/settings set **per format**, rather than
independent histories for every method. The earlier audit's per-method-history
requirement was corrected against `SaveOptionsInMem` and `ZipRegistry.h`.

## Executed evidence

The final compression/Help Qt suite has **43 passed / 0 failed / 0 skipped**,
including setup/cleanup. Nineteen data rows select methods through AddDialog:
7z (four GUI methods plus Store), ZIP (five plus Store), TAR GNU/POSIX, WIM,
XZ, gzip, bzip2 and both Hash methods. Each archive is created, tested and
extracted; Japanese/space names and SHA-256 content comparison are checked.
Hash output is tested and compared with the input digest, not extracted.
Existing encrypted 7z/wrong-password, low-memory errors and Help regressions
also pass. State tests cover format drafts, OK/Cancel, level/dictionary resets,
nearest choices, method matching, history, toolbar entry and Japanese solid modes.

Three affected existing integration functions additionally pass (**5 checks**
including setup/cleanup): additional formats/links, hash commands and Open With
archive operations. The total is **48**, not forty-eight physical clicks.
See [executed log](distribution.md).

## Remaining differences

Native dimensions/pixel comparison and physical menu/keyboard operation remain
unverified while the desktop is locked. Editable numeric fields are an existing
port extension: upstream combos are noneditable. Their word/order and thread
validation now respect the selected method/hardware limits. Some dynamic
translation and advanced Options layout details still differ. This checkpoint
covers compression control behavior, not full application parity, full metadata
preservation, final clean regression or publication.
