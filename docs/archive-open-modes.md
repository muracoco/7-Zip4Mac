# Open Inside and Shell Open As modes

Baseline: unmodified official 7-Zip 26.03. File → Open Inside, Open Inside *
and Open Inside # operate on the focused item. Folders navigate normally;
files are opened internally without an external-application fallback. Only
normal Open Inside has Ctrl+PgDn; the two variants have no accelerator.

## Source-confirmed semantics

- `FileManager/resource.rc`, `MyLoadMenu.cpp::ExecuteFileCommand` and
  `PanelItems.cpp::OpenFocusedItemAsInternal`: dispatch null, `*` and `#`,
  in that order. Translated variant labels derive from normal Open Inside,
  removing its mnemonic and appending the suffix.
- `Common/OpenArchive.cpp::ParseType` and `OpenArchive.h::COpenType`:
  normal detection recursively follows available MainSubfile streams;
  `*` disables that automatic traversal while retaining format detection.
  A generic ZIP inside another ZIP is not automatically followed.
- `#` disables returning an ordinary archive and enables the parser.
  Detected regions and gaps become numbered entries with Type, Offset and
  Size. Extraction copies their raw byte ranges through the official engine.
  A whole-file ordinary archive or one unknown region alone is not a successful
  parser result. `#:e` enables EachPos: it scans inside recognized regions
  instead of skipping to their ends, so overlapping signatures are visible.
  This is not generic recursive opening. `#:a` is commented out of the
  official Shell menu and is not offered here.
- `Agent/Agent.cpp` and `Console/Main.cpp` both use `ParseOpenTypes`.
  The console options are `-t*` and `-t#`; console reports parser Type `#`,
  while the Windows Agent's display label is `Parser`.

The verified distinction is a generic Split `.bin.001` containing a ZIP:
normal detection opens the contained ZIP, while `*` shows the joined
`container.bin`. On this engine ordinary tar.gz/tar.xz fixtures show their
outer stream in both modes; they are not used as evidence of automatic traversal.

## Shell Open archive >

`Explorer/ContextMenu.cpp::kOpenTypes` defines the exact order: normal, `*`,
`#`, `#:e`, `7z`, `zip`, `cab`, `rar`. When standalone Open archive is enabled,
normal is omitted from the submenu; when disabled it appears first.
The two commands have independent, default-enabled settings in
`ContextMenuFlags.h`, `ZipRegistry.cpp` and `FileManager/MenuPage.cpp`.
Only one non-directory archive-candidate item receives the submenu.

The explicit `rar` name selects only the legacy Rar handler. A RAR5 fixture
fails that mode with code 2 but opens normally as Rar5. The port retains this
upstream behavior rather than silently selecting another handler.

Finder Open With and the filesystem 7-Zip context submenu share the choice
order and visibility settings. Options has ten commands. New profiles enable
Open archive >; previously saved explicit lists are preserved and require
checking the new item to enable it. Changing the mode explicitly re-lists the
physical archive, even if it is already open. Successful reopening resets
the inner folder to the root; failure preserves the existing view and mode.

## Port implementation

`ArchiveRequest::readMode` is separate from the writable compression format.
It is applied to listing, generated-name resolution, extraction preflight and
execution, Test and archive-content CRC/hash. Explicit-mode failure cannot
retry as an extension-derived ordinary archive. Invalid mode strings fail
without modifying the target. Password retry preserves the request mode.

Each current archive and nested parent frame keeps its own mode. Extracting
a child uses the parent mode; listing that child uses the newly requested
mode. Refresh and Up restore the correct mode, and returning to the filesystem
clears it. File and local context menus expose both variants; an unselected
focused item is still actionable.

Technical-list header layers are counted. A recursively opened ZIP inside a
Split or another container must not be handed to the single-file ZIP comment
helper as if the outer bytes were ZIP. Such collapsed multilayer views retain
console comment display and are read-only for mutation until a supported
container write-back path exists. Explicitly extracted nested archives use
the existing independent staging and parent-writeability checks.

## Verification and limits

`open_mode_tests` / CTest `archive_open_modes` verifies normal/Split `*`
listing and exact extraction, parser regions/gaps and independent byte-slice
comparison, Test and SHA-256, explicit failure without extension fallback,
focused GUI commands, Refresh, Up, temporary cleanup, selection, encrypted
parents, GUI extraction and CRC. Existing ordinary ZIPs opened with `*` are
also tested for Comment, Create Folder and editor-style ReplaceFile updates.
Results and executed clean-build commands are in [test-results.md](test-results.md).

`open_as_tests` / CTest `open_as` additionally exercises menu order, standalone
Open on/off, candidate visibility, saved-list migration, Apply/Cancel, icons,
Qt mouse clicks on Open With and the actual file-list context submenu,
same-archive normal/explicit reopening and root reset, failure recovery,
password retry, invalid-command refusal, forced 7z/ZIP/CAB/RAR Test and selected
extraction, and overlapping parser regions compared with independent raw bytes.
Unchanged libarchive CAB/RAR fixtures retain their BSD license notice.

Full format-specific columns remain incomplete; parser Offset is available
in Properties rather than a dedicated initial column. Like upstream,
Favorites/address-only navigation stores paths rather than explicit modes.
Reusing a currently open archive retains its mode, while opening after closing
or restarting uses normal detection. This is shared upstream behavior:
`PanelFolderChange.cpp::SetBookmark/OpenBookmark`, `AppState.h::CFastFolders`
and `ViewSettings.cpp::SaveFastFolders` store only folder path strings.
Official `PanelMenu.cpp::CreateFileMenu` (lines 938–945) creates the Shell
7-Zip/System menus only when `!IsArcFolder()`. Archive-internal context menus
instead load the File commands from `resource.rc`: Open / Open Inside / Open
Inside * / Open Inside # / Open Outside / View / Edit. The port provides these
commands and passes the child mode through nested extraction/listing. The
earlier audit's format-specific archive-internal Shell Open As gap was
incorrect; adding that submenu would extend the upstream interface.
`PanelItems.cpp::OpenFocusedItemAsInternal` uses the focused row independent
of selection count, as do the port's Inside actions. Physical menu clicks,
the new Finder submenu and
Ctrl+PgDn require an unlocked desktop and have not been claimed by the Qt tests.
