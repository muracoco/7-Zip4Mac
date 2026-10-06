# Extraction lifecycle batch

The normal backend already connected the original pre-stream overwrite/Skip
and ordered publication callbacks. Earlier component reports describing those
connections as unfinished are historical. This batch addresses a separate
defect: abrupt engine termination could discard an opened, unfinished stream
when private staging was removed.

The pinned importer now registers the original GetExtractStream output immediately
after `_outFileStream = outFileStream_Loc` in official 26.03
`UI/Common/ArchiveExtractCallback.cpp`. The engine still owns archive decoding,
overwrite decisions, name generation, links and normal CloseArc metadata.
The added callback reports only the item index, private path and inode identity;
it sends no file contents or passwords.

The sequential host session retains that identity and the original public
destination approval. After process termination it publishes an unfinished
regular stream using the guarded installer and its actual filesystem metadata.
It does not apply archive metadata that the original CloseArc never reached.
Completed outputs retain their existing behavior. Reopened split streams retain
their first approval. Native successful publication removes the open-stream
record, preventing duplicate publication at shutdown.

If a private stream or approved public target changed, recovery does not overwrite
the changed file. The staging folder is kept, the previous approved output is
restored where possible, and unresolved previous data remains in its guarded
recovery folder. Diagnostics identify those locations. Backend destruction also
closes the session after stopping the owned engine and waiting for its worker,
covering shutdown before the finished signal is dispatched.

## Grouped verification

The grouped build succeeded with the pinned source import and Qt application.
Latest distinct outcomes: **360 passed, 0 failed, 0 skipped**, including
setup/cleanup: extraction metadata 322, native progress 15, completion 15 and
selected integration 8. The twelve added cases cover original-console
comparisons for interrupted 7z/ZIP streams with new/overwrite/auto-rename/
rename-existing outputs, changed public/private recovery targets and real
backend destruction immediately after the original stream-open callback.
Original and recovered unfinished bytes are verified as source prefixes;
protected previous outputs and subsequent Test reuse are checked.

The first grouped run exposed one missing retained-staging diagnostic and six
fixture counter failures: the new begin event had been counted as finish.
Recovery diagnostics now accumulate instead of dropping previous locations;
the fixture counts begin and finish separately. Only failed/affected paths and
the added shutdown checks were rerun, followed by selected integration checks.
Unrelated green suites and the complete format matrix were not repeated.
[Captured initial run and targeted reruns](distribution.md).

```bash
source scripts/env.sh
./scripts/build-progress.sh
cmake --build "$DEPS/build-comments-20261004" --target SevenZipMac extract_metadata_tests native_progress_tests progress_completion_tests port_tests -j6
export PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins"
ctest --test-dir "$DEPS/build-comments-20261004" -V -R '^(extraction_metadata|native_progress|progress_completion)$'
"$DEPS/build-comments-20261004/port_tests" "$SEVENZIP_BINARY" roundtrip cancellationAndResponsiveness archivePauseResumeAndCancel eliminateRootAndRenameExisting absoluteExtractionAndDialogSettings
```

This batch does not establish physical Finder/Fn/menu verification or the final
clean-build/all-format release gate. Process death before a stream is registered
cannot expose decoded bytes because registration precedes handing the stream to
the decoder. A crash of the whole application or OS needs a separate persistent
recovery protocol; it is not claimed by this engine-process recovery.

## Packaged application

The updated application was bundled with the rebuilt native helper. All 15
Mach-O files pass system/bundle-relative dependency checks and deep/strict
ad-hoc signature verification. Bundled native executable text and corresponding
source match the current implementation. A three-second isolated application
startup succeeds with development Qt/DYLD variables removed; stdout/stderr are
empty. This is startup verification, not physical Finder/menu interaction.
[Packaging and startup evidence](distribution.md).
