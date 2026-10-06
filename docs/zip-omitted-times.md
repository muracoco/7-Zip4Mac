# ZIP omitted timestamps on macOS

During final desktop verification, an owned extracted Japanese/spaced file had
an access timestamp at the minimum signed 64-bit nanosecond value and an
unusable creation timestamp. Its bytes still matched SHA-256. Fresh inputs
showed that both ordinary Add and the actual Add-dialog settings retained source
creation/modification times, including Preserve Access mode. Compression was
not the cause of the observed metadata change.

A fresh default ZIP created by official 26.03 reproduced the access-time problem
with the unmodified official console's extraction. Its NTFS extra field has zero
creation/access placeholders; the original technical listing displays these as
blank. The console converts zero FILETIME to 1601-01-01, which APFS cannot retain.
The port additionally applied the raw zero creation property during publication.

The generated native extraction callback now leaves a zero FILETIME undefined,
so the host's newly created time survives. The guarded publication layer likewise
does not apply a zero creation property. Raw archive listing properties remain
available. This is a macOS timestamp adaptation; handlers, codecs, compression,
valid timestamps and the pristine upstream source/binary remain unchanged.
The application uses the adapted `7zz-progress` extraction path. Direct use of
the unmodified bundled `7zz` console still has the upstream behavior.

The regression creates a real ZIP using the original console, with omitted
creation/access times, a Japanese/spaced file, an empty folder and a symbolic
link. It verifies sane restored timestamps, exact modification time, payload and
link target. The old packaged adapter failed the new access-time assertion.
After repair, this case and valid metadata/link cases for 7z, ZIP, TAR and WIM
passed (7 Qt cases including setup/cleanup, 208 ms). The complete affected
`extraction_metadata` suite then passed once in 19.95 seconds (323 Qt cases,
including setup/cleanup). The first failure
and diagnostic-driver failures are retained, rather than rewritten as successes.

The 35-suite consolidation and 151 format registrations remain earlier evidence;
they were not repeated for this isolated extraction timestamp repair. The source
fixture with bad timestamps was retained for diagnosis and was not silently
rewritten. All reproduction and output work used owned temporary directories.
