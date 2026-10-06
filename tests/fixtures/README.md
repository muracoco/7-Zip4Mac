# Fixture provenance

Fixtures are extraction-only reference data. Never execute embedded programs or
install/mount generated disk images. Tests write only to new disposable folders.

The unchanged UU files come from **libarchive v3.8.2**,
https://github.com/libarchive/libarchive/tree/v3.8.2/libarchive/test.
Individual author notices, license conditions, original filenames and SHA-256
values are retained in `licenses/libarchive-fixtures.txt`; the distribution's
COPYING is `licenses/libarchive-COPYING.txt`.

- CAB: `test_read_format_cab.c`, fixture `_cab_1.cab.uu`.
- LHA: `test_read_format_lha.c`, fixture `_lha_header0.lzh.uu`.
- RAR4: `test_read_format_rar.c`, original RAR fixture and stored three-volume set.
- RAR5: `test_read_format_rar5.c`, stored fixture, eight-volume set and
  `test_read_format_rar5_hardlink.rar.uu` for selected-link comparisons.
- RPM: `test_read_format_cpio_svr4_gzip_rpm.c` and its RPM fixture.

`make-format-fixtures.py` generates other containers around original payloads.
It uses unmodified official 7-Zip codecs for writable archives and PPMd, system
tools for disk images, and writes stored container headers for read-only formats.
It does not implement compression algorithms or a RAR creation feature.

SZDD compressed bytes are derived from Wine's test data, with only the declared
uncompressed size corrected to 20 bytes. The original data was made using
Microsoft COMPRESS.EXE, not a compressor implemented by this project. Attribution,
the immutable source link and the modification are in
`licenses/wine-szdd-fixture.txt`.

Optional fixture tools build in the user cache. e2fsprogs 1.47.2, squashfs-tools
4.6.1, ntfsprogs 2022.10.3 and pkgconf 2.4.3 are not application dependencies or
bundled executables. Official NSIS 3.12 installer/source archives are downloaded
with pinned SHA-256 values for extraction comparison only. They are never run
or redistributed in the app.

Expected SHA-256 values come from original payloads, system/Python decoders of
stored source data, system libarchive for RAR multi-volume data, or NSIS's source
tarball for its COPYING file. 7-Zip's extracted output is not its own oracle.

An alias test proves the corresponding 7-Zip container reader/extension route.
It does not establish complete DOCX/EPUB package semantics, Microsoft Help viewer
compatibility, or every codec and disk variant. Hash manifest tests distinguish
successful verification from unsupported algorithms; console extraction reports
E_NOTIMPL. See `docs/format-coverage.md` and `docs/archive-formats.md`.
