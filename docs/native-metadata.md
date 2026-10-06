# Official archive metadata bridge

Baseline: official 7-Zip 26.03. The port now reads handler metadata directly
from the same open archive used by the official console frontend. This replaces
technical-list text reconstruction for the bundled application.

## Upstream reuse

`Console/List.cpp::ListArchives` calls `PortMetadata::Write` while its
`CArchiveLink`, `CArc`, `IInArchive` and `IArchiveGetRawProps` remain alive.
The bridge enumerates the provider's complete ordered property schemas,
including duplicate property IDs, actual variant types and undefined values.
It retains archive item indices and decimal strings for 64-bit integers and
FILETIME ticks, including FILETIME's three reserved precision fields.

Formatting reuses `ConvertPropertyToString2(..., 9)`,
`ConvertNtSecureToString`, `ConvertNtReparseToString`, the console property-name
routine and its open-error formatter. Properties raw values use the upstream
256-byte threshold; list values use 64 bytes. Larger values display `data:N`.
CRC/checksum values of at most eight bytes use uppercase hexadecimal; other
raw values use lowercase hexadecimal. NT security descriptors use the upstream
portable formatter rather than a Windows API.

Both original `Agent/AgentProxy.cpp` folder proxies are compiled into the
helper. Their cached sizes, packed sizes, descendant counts and CRCs replace
the port's reconstruction where native metadata is available. Archive layers,
their child-item properties and failed-inner-open information are retained
separately. The GUI follows the Agent's Path-to-Name schema transformation,
folder count additions and Flat View's separate Name/Path Prefix columns.
Single-item Properties uses the actual archive index, including duplicate names.

The overlay also fixes a BSTR lifetime in the POSIX fallback of the tree proxy:
the `CPropVariant` holding Name must survive until its string is copied.
The existing Windows raw-property path normally avoids this fallback. A real
Japanese-name WIM fixture exposed it; the corrected proxy passes the fixture.
The upstream source tree and existing license headers are preserved. The
metadata overlay covered nine frontend/Agent files; the subsequent Benchmark
adapter extends it to ten. Compression
algorithms, archive handlers, codecs and crypto remain official engine code.

## Process boundary and safety

The asynchronous backend supplies `SEVENZIP_PORT_METADATA_PATH` inside an owned
temporary directory, after clearing any inherited value. The helper exclusively
creates a mode-0600 file with `O_EXCL`, `O_NOFOLLOW` and `O_CLOEXEC`. Existing
files or symlinks are refused. Completion, cancellation and password retries
release the owned directory. JSON is validated before replacing the result.
The legacy text parser remains a fallback for test/custom executables that do
not provide the bridge; the distributed app bundles the native helper.

Passwords are redacted from diagnostic output, not from legitimate filenames or
metadata. Structured strings preserve CR/LF and Unicode filenames. Extraction
still rejects NUL, traversal, unsafe absolute paths, files with colliding names
and unsupported links. Repeated equivalent directory records can merge, as
required by genuine image handlers; case or Unicode spelling collisions remain
rejected. This change does not introduce general link restoration.

Replacing an existing read-only output is refused before staging or rename-first
Move installation. Both the old destination and the Move source remain intact.
Rename Existing is handled separately because it preserves the old file.

## Executed checks

- Properties: **19 passed**, covering native schemas/default columns, ZIP,
  split/nested layers, WIM raw SHA-1/proxy totals, exact UInt64/FILETIME metadata,
  encryption, literal password-containing filenames, long/multiline comments,
  CR/LF filename round trips, protected metadata destinations and Flat columns.
- Native raw formatter: **11 checks passed**, including both size thresholds,
  hexadecimal rules and NT security descriptor validation.
- Overwrite/Move: **19 passed, one cross-volume check skipped** because its
  optional second-volume parameter was unset. Earlier SSD/SMB evidence remains
  in [filesystem-transfer.md](filesystem-transfer.md). The new read-only Copy
  and rename-first Move checks pass.
- The affected read-only split/extraction regression passes after the guard fix.
  The other 18 checks in the selected integration run passed; that initial run's
  one read-only failure and its correction are retained in the log.
- Transfer, native progress, Open Inside and Open As suites pass: **25, 13, 14
  and 19**, respectively, including setup/cleanup.
- The final format run has **155 checks passed**, covering all **151 genuine
  format/extension registrations** and Qt context-menu Add/Test/Extract paths.

See [native-metadata-test.log](distribution.md) and
[test-results.md](test-results.md). Physical desktop clicks and rendering remain
unverified while the session is locked.

## Remaining scope

Properties/list commands remain **Partially implemented** overall. Native rows
now use the original proxy directories and item references, with imported Agent
property bodies and archive alternate-stream navigation. See
[source reuse and executed tests](native-agent-properties.md). Complete
real-index selection for operations and collision-aware restoration remain.
JSON parsing,
final model insertion and sorting still use GUI-thread work for very large
archives. Complex real NT security fixtures have not been independently
exercised beyond formatter cases. Full extraction metadata/link preservation
and all Windows UI combinations are separate remaining requirements. This
checkpoint does not establish complete Windows parity or authorize publication.
