# Official language, settings and dialog resource batch

The Lang2 parser, language-page information and resource bindings were implemented as one functional group before running their related tests. The reference is the unchanged official 7-Zip 26.03 source. This checkpoint does not claim full Windows parity or a final release.

## Source reuse

`scripts/import-language.py` pins `Common/Lang.cpp`, `Common/StringToInt.cpp` and `FileManager/LangPage.cpp/.h`. The original parser, numeric converters, missing/extra-entry comparison and information formatter are retained in `src/upstream/Language.inc`. Qt adapts file I/O, Unicode strings and the information control. `scripts/import-ui-resource-ids.py` pins ten official resource headers and imports their numeric identities. Both importers run through `build-progress.sh`; original sources, generator scripts, LGPL notices and adaptations are distributed with the app.

## Implemented

- Official UTF-8 Lang2 loading, BOM/CRLF handling, escapes, comment and blank-line identity behavior, order validation, embedded-NUL termination and 1 MiB limit.
- English plus 92 official translations, English first, other names sorted without case sensitivity. The Language page shows translator comments, translated line counts, percentages, missing and extra lines with the original formatter. Invalid language files are excluded and reported by filename.
- Packaged `Contents/Resources/Lang/*.txt` can be edited or extended. The app reads this directory when present; developer test binaries fall back to embedded files. English fallback remains the compiled official `en.ttt` reference.
- Numeric resource binding for six Options tabs, portable/disabled Windows settings, editor labels, main menus, toolbar, Add, Extract and Progress controls. Main sort commands use `MyLoadMenu.cpp`'s property-resource mapping; toolbar labels strip menu mnemonics.
- Standard dialog buttons use the original OK/Cancel/Yes/No/Close/Help IDs. Progress Continue/Pause/Close keeps its current state across language refresh. Targets, filenames, numeric progress values and error output remain literal.
- Apply persists and changes language; Cancel leaves persisted settings unchanged. Editable manual compression values survive repeated language changes.

## Executed validation

Apple Silicon macOS 26.6.2, Apple Clang, Qt 6.11.3. Incremental CMake/Ninja build succeeded. Latest results for the selected case set are **87 passed, 0 failed, 0 skipped**, counting setup/cleanup once per suite: language/settings 10, compression/help 46, native progress 15, version/menu/time 11, and three integration cases plus setup/cleanup (5). The initial grouped run had two stale expectations: `Missing` versus the official `Missing lines` heading and `Close` versus the official `&Close` label. These assertions were corrected to the source contract; only failed cases and affected integration cases were rerun. The log preserves the initial failures and targeted successes.

The language suite loads every supplied translation and refreshes bound controls across all 93 choices. It also executes an owned relocated-binary witness with an editable bundle language directory and an invalid file. Integration covers Japanese Apply/Cancel, Open With 7z/ZIP compression/Test/extraction, and archive folder creation. Native progress verifies real failure recovery and Pause/Cancel; the existing compression group verifies actual codec/encryption round trips.

Commands:

```bash
ctest --test-dir "$BUILD" -R '^(language_settings|compression_help|native_progress|version_menu_time)$' --output-on-failure
PORT_TEST_PLUGIN_ROOT="$COCOA_WORK/build/plugins" "$BUILD/port_tests" "$SEVENZIP_BINARY" officialLanguagesAndRootOptions openWithArchiveCommands guiArchiveFolderCreation
```

`BUILD`, `COCOA_WORK` and `SEVENZIP_BINARY` come from `scripts/env.sh` (choose the desired build directory). `scripts/test.sh --no-focus` includes the new suite.

## Remaining scope and classifications

| Item | Classification | Remaining work |
|---|---|---|
| Original language parser/information | Implemented | Validated for official files and owned malformed fixtures |
| Resource-bound controls listed above | Implemented | Automated resource/refresh verification; physical desktop comparison is separate |
| First-run OS-language selection | Implemented | Added in the subsequent [dialog resource batch](dialog-resources-port.md); saved explicit choices take precedence |
| LCID-based preferred-language markers | macOS difference | QLocale supplies regional/base-language matches; Windows LCIDs are unavailable |
| Port-specific explanatory text and some errors | Partially implemented | English text without an official resource counterpart remains |
| Dialog dimensions/spacing and long-label layout | Partially implemented | Original Add/Extract/Options coordinates and Progress resizing are now connected; font-dependent clipping and physical comparison remain ([subsequent batch](dialog-resources-port.md)) |
| Physical Finder clicks, function keys and final release gate | Partially implemented | Desktop unlock, full acceptance and empty-directory clean build remain pending |

Do not classify these remaining portable gaps as platform differences. This batch does not repeat all archive-format tests or the final clean build.

パッケージ化後の15 Mach-O依存はsystem／@rpathのみ。`codesign --verify --deep --strict`成功。開発Qt／DYLD環境変数を外し独立設定で3秒起動、生存・stderr空を確認し、所有processのみ停止した。bundleに公式92言語と英語参照を格納。これは物理Finderクリックや最終clean buildの確認ではない。
