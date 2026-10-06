# Official default Open and filename profile

Single-file default Open now attempts content-based archive opening with an
unknown or absent extension. `scripts/import-open-profile.py` pins official
7-Zip 26.03 `PanelItemOpen.cpp`, `Explorer/ContextMenu.cpp` and `MyString.cpp`
by SHA-256. It retains the original extension tables, `DoItemAlwaysStart`,
`FindExt`, ASCII word matcher and `CPanel::IsVirus_Message` bodies and notices
in `src/upstream/OpenProfile.inc`. Qt supplies strings and the plain-text message
box; the archive job remains asynchronous.

The opening policy follows `OpenItem` and `OpenItemInArchive`: ordinary Open
tries an archive unless the official start-extension table requests external
opening; Open Inside bypasses that table. Only a not-archive result falls back
to external opening. Password requests, cancellation and I/O errors remain
internal outcomes. The process adapter recognizes official console not-archive
diagnostics only with exit code 2 and excludes password-required results. This
diagnostic boundary is a process adapter, not a direct HRESULT interface.

The imported matcher accepts ASCII, uses ASCII case conversion and limits the
suffix to 32 characters. The filename guard rejects five or more consecutive
space characters and U+202E right-to-left override before external opening.
It uses original message resource 3012, displays `[RLO]` and preserves literal
text. Open Inside bypasses this guard as in the original. The `_WIN32`
trailing-dot/space normalization branch is excluded on macOS, where those are
valid POSIX filenames.

## Executed checks

Affected suites: **87 passed, 0 failed, 0 skipped**, including setup/cleanup:
panel Open 27, archive open modes 24 and editor write-back 36.
[Execution log](distribution.md).

New cases cover a real unknown-extension 7z, an extensionless ZIP, ordinary-file
fallback, permission failure, Open Inside refusal to fall back, encrypted
unknown-extension opening and password cancellation, the exact extension
profile, and a misleading-name archive opened only via Inside. Existing
multi-item LaunchServices and independent same-name ZIP write-back checks are
retained. The initial extensionless-ZIP fixture failed because creation appended
`.zip`; it now creates a ZIP then renames it before exercising Open.

```bash
source scripts/env.sh
cmake --build "$DEPS/build-comments-20261004" --target SevenZipMac panel_open_tests editor_tests open_mode_tests
ctest --test-dir "$DEPS/build-comments-20261004" -R '^(panel_open|archive_open_modes|editor_writeback)$' -V
```

Physical Finder/mouse/Fn checks remain pending while the desktop is locked.
Remaining link/context/drop behavior and final full-parity release gates are
separate work. This increment does not declare complete parity.

The app was repackaged. All 15 Mach-O dependency checks and deep/strict ad-hoc
signature verification passed. An owned bundled process survived a three-second
startup with isolated preferences and development Qt/DYLD variables removed.
This verifies startup, not physical desktop interaction.
