# ZIP metadata update implementation group

Comment, focused-row Rename, folder Rename and existing-file update share the
official ZIP handler. This group finishes the common writer and both helper
bindings before grouped validation. The already imported Agent operation
bodies and GUI bindings remain in use.

## Source and bounded change

Official 7-Zip 26.03 `CPP/7zip/UI/Agent/AgentOut.cpp::CommentItem`,
`RenameItem`, `UpdateOneFile`, and `CommonUpdate` define item indices and update
pairs. `CPP/7zip/Archive/Zip/ZipHandlerOut.cpp::UpdateItems` defines name/comment
encoding and the actual 16-bit encoded comment limit. On the Mac build,
`CPP/Common/StringConvert.cpp::g_ForceToUTF8` defaults to true. The existing
encoded comment field is therefore normally UTF-8 on this Mac. The GUI/native
helper's private UTF-8 transport limit is 4 × 65,535 bytes; the original handler
enforces the actual 65,535-byte encoded field and supplies the real process error
when it is exceeded. Windows OEM encoding depends on the Windows code page;
raising a transport limit does not reproduce that platform locale. The legacy
standalone helper retains its 65,535-byte input limit and is not the normal GUI
comment path. No compression/encryption implementation is replaced.

`ZipUpdate.cpp::UpdateItemOldData` clears the data-descriptor bit for NewProps
updates and copies new DOS time. For existing ZipCrypto data, the encryption
header was created with the check byte from the old DOS time when bit 3 was set.
`ZipHandler.cpp` checks that time when bit 3 is set, or CRC when it is clear.
Clearing the bit without re-encrypting breaks the existing password check.
The previous port refused descriptor comments as a protective workaround.

`scripts/import-progress.py` pins SHA-256
`3db0ee77bd56217722cab13582958964bf76071d481e69447b238aea9113bc7f`
for the original ZipUpdate.cpp and applies a narrowly bounded overlay:

- Metadata-only ZipCrypto updates retain the descriptor bit and original DOS
  time. AES/strong encryption and new-data updates keep the original paths.
- The original exact-size copier copies the existing packed/encrypted bytes.
- The original `WriteLocalHeader_Replace` / `WriteDescriptor` regenerates the
  descriptor for the output header's 32/64-bit size mode. A forced Zip64 input
  with small sizes may be canonicalized to a 32-bit output descriptor.
- Both `7zz-progress` (native Agent operations) and `7zip-comment` (legacy
  unambiguous entry helper) link this same overlay; stock `7zz` stays untouched
  as an independent decoder and format reference.
- The application and helper refusal guards are removed. Source-bound identity,
  staging, semantic verification, cancellation and exclusive commit checks
  remain enabled.

`NativeZipMetadata.h` asks the original `ISetProperties` interface to enable
existing ATime/CTime writes for Comment/Rename. The default handler would write
zero values despite the Agent callback exposing the original timestamps. Add,
ordinary Update and new-data replacement options are not changed by this helper.
The test with a different NTFS time and ZipCrypto DOS check time confirms both
properties can be retained without modifying ciphertext.

The full local-record reader in `ZipIn.cpp::CheckDescriptor` explicitly refuses
old descriptors without their signature during updates. Stock 7zz Rename also
returns E_NOTIMPL for the owned unsigned fixtures, while Test/extraction succeeds.
The port retains that source-defined error and the original archive; it does not
label this upstream limitation as missing Mac functionality or a platform change.

The grouped editor check exposed a GUI defect: the failed subprocess attempt
used to present its final failure before requesting the missing password, even
when a later retry succeeded. Password retry now precedes final display in the
normal, external-edit, nested-write-back and transfer paths. An accepted retry
ends the old progress attempt quietly before replacing it; a declined retry
still retains and displays the real result. Backend result codes and diagnostics
are not rewritten as cancellation. Existing finished/error dialogs retain their
normal completion policy.

An older path-only replacement test assumed every supplied password must first
authenticate the old archive's data. That contradicts the already imported
`UpdateOneFile` pairs: the selected item's new data can be encrypted with a new
write password while unrelated `NoChange` blocks retain the old one. The test
now checks both selected/new-password and unrelated/old-password decoded bytes,
and still requires failure/original retention when encrypted headers cannot be
opened. This is a corrected source-contract test, not a new blanket archive
Test or a relaxed payload comparison.

This is a documented correctness patch to the official handler, not a claim
that the modified function body is imported unchanged. Upstream source files
and the official source archive are not modified; existing notices remain.

## Feature and validation inventory

| Item | Implementation | Validation |
| --- | --- | --- |
| ZIP Comment and Clear Comment, files and real folders | Implemented; original Agent body retained, common writer fixed | Passed; see executed results below |
| ZIP file/folder Rename and GUI row restoration | Implemented; original Agent body retained, common writer fixed | Passed; see executed results below |
| ZipCrypto Store/Deflate with data descriptors | Implemented | Passed; see executed results below |
| Signed 32/64-bit input descriptors | Implemented through original parser and output writer | Passed; see executed results below |
| Unsigned descriptors | Original handler's E_NOTIMPL for metadata updates; browse/Test/extraction supported, originals retained | Stock 7zz refusal and successful decode verified |
| AES/plain descriptors and no-descriptor archives | Original paths retained | Passed; see executed results below |
| Existing encrypted file replacement and editor write-back | Original paths retained | Passed; see executed results below |
| Password retry without a premature fatal dialog | Implemented across the shared GUI workflows | Passed; see executed results below |
| UTF-8 65,535-byte boundary, NUL, collision, failed/cancelled update retains original | Implemented | Passed; see executed results below |
| Mac/Windows comment encoding | macOS difference: official Mac UTF-8 vs Windows configured code page | Source verified |
| Physical Finder/AppKit/Fn input | Separate release gate | Not verified by this batch |

## Reproduction and verification scope

`./scripts/test-zip-metadata.sh /absolute/build/path /absolute/log/path` builds
both writers and affected Qt targets, then runs the complete affected group.
`tests/zip-metadata-fixtures.py` transforms only container headers/descriptors
of owned archives created by pristine official 7zz. It never implements crypto
or compression. Independent checks compare packed/encrypted bytes, names,
comments, DOS time and descriptor fields. Stock 7zz Test and extraction with
source SHA-256/directory comparison verify decoder compatibility. Passwords
are sent through stdin, not process arguments or logs.

At this group's checkpoint, the full-format matrix and new-directory build were
reserved for the release gate. Their later executed results are in
[release-consolidation.md](release-consolidation.md) and
[publication-review.md](publication-review.md). Physical acceptance/publication
remain pending; [current-status.md](current-status.md) owns the current inventory.

## Executed grouped results (2026-10-06)

Current arm64 Mac / Qt 6.11.3: **145 latest distinct Qt checks passed, zero failed,
zero skipped**. Setup/cleanup count once per suite; reruns do not inflate totals.

| Suite | Latest distinct passing cases | Evidence |
| --- | ---: | --- |
| Native ZIP metadata | 13 | zip-metadata-metadata.log |
| File comments | 31 | Initial 31 plus changed ZIP paths in zip-metadata-comments.log |
| Listing / raw-name Rename | 11 | zip-metadata-first-group.log |
| Editor write-back | 36 | Initial 32 plus four repaired paths in zip-metadata-editor.log |
| Progress completion | 16 | zip-metadata-repair-group.log |
| Archive Copy / transfer / Cancel | 11 | zip-metadata-copy.log |
| Native same-name GUI update identities | 7 | zip-metadata-native-rows.log |
| Selected update/encryption/replacement integration | 20 | Initial 17 plus contract-corrected cases in zip-metadata-replacement-contract.log |

The initial group had 14 failures. Ten were fixture setup/read-password issues:
read commands need to omit bare -p, and official sequential Store creation
returns E_NOTIMPL. Store fixtures now originate from stock seekable encryption
and change only container flags/check-time/descriptors. Four failures exposed
the premature GUI password-error notice and were repaired. The next affected
run found the source-defined unsigned-descriptor refusal and timestamp loss;
those are separately classified above. Three old replacement assertions were
corrected against the already imported Agent contract and now independently
check both write-password and unchanged sibling-password bytes. Failed runs
remain in the logs rather than being discarded.

Unsigned cases count as passing verification of an expected upstream refusal,
not successful metadata updates. All six supported signed/plain/AES variants
pass Comment/Rename, independent packed-byte/header checks, stock Test and
extraction/SHA-256. Forced Zip64 fixtures are small owned archives, not a claim
that this group wrote/extracted actual files larger than 4 GiB. Cancellation
checks retain the original; this group does not prove Cancel occurred midway
through a decoder. Previously recorded long-operation checks remain separate.

## Packaged application and startup

The normal app was rebuilt at
`/DEPS/build-comments-20261004/7-Zip Mac.app`.
macdeployqt, the patched Cocoa plugin, ad-hoc signing and strict verification
passed. All 15 Mach-O files have only system/@rpath runtime dependencies.
The owned bundle process stayed alive for three seconds with no Qt/DYLD
development hints, zero stdout/stderr, an explicit private INI profile and
byte-identical existing native user preferences. An invalid explicit profile
was rejected without native-preference fallback. Only owned PIDs were stopped.
See zip-metadata-bundle.log and zip-metadata-startup.log. This is executable
startup verification, not a physical Finder/Open With/Fn/menu click test.

Qt plugin corresponding-source/license completion, the final fresh build/full
acceptance and public repository gate remain required; packaging this development
app is not a declaration that those final release requirements are satisfied.
