# Detailed functionality / 機能の詳細

[English README](../README.md) · [日本語README](../README.ja.md)

For the current implementation classifications and verification limits, see
[current-status.md](current-status.md). Version changes are in
[CHANGELOG.md](../CHANGELOG.md) / [CHANGELOG.ja.md](../CHANGELOG.ja.md).

## English

- File / Edit / View / Favorites / Tools / Help menus and official Add / Extract / Test / Copy / Move / Delete / Info toolbar assets.
- Shared official registry: 61 handlers / 138 extensions, declared as alternate document types. [All 151 registration tests](format-coverage.md) distinguish successful operations from upstream checksum limitations.
- Filesystem and archive-folder browsing, nested archives in the same panel, virtual-path Favorites, parent navigation, sorting, multiple selection, four view modes, Flat View, two panels, saved columns, timestamps, automatic refresh and history. Nested changes support confirmed, one-level-at-a-time parent write-back on Up, address navigation and close. Failed updates retain a recoverable edited copy.
- Original natural/typed/raw sorting, first-sort directions and stable row ties, complete filesystem metadata columns, and property-keyed column preferences across Flat View and both panels. [Source reuse, grouped checks and limits](panel-sort-port.md).
- Create 7z / ZIP / TAR / WIM / XZ / gzip / bzip2, plus SHA-256 / SHA-1 checksum manifests; Test and extract. Stream-format creation accepts one regular file. **RAR compression is not provided.**
- Compression method, dictionary, word/order, solid size, threads, memory, update/path modes, volumes, parameters, timestamps, supported link-storage flags and encryption. Passwords are neither logged nor saved.
- Automatic compression choices and memory estimates reuse official Windows GUI arithmetic; manual/automatic choices persist separately. Official English Help provides all 70 pages, Contents/Index, navigation and contextual dialog topics. [Scope and tests](compression-help-port.md).
- Compression controls now reuse official format/method tables and numeric builders, with format drafts, level/dictionary resets, method-aware restore, hardware-bounded threads and last-format/history behavior. [Source reuse and tests](compression-controls-port.md).
- Progress receives real official-engine callbacks through bundled `7zz-progress`: file counts, processed/packed bytes, compression ratio, speed and remaining time. Help adds asynchronous full-text Search, topic/subtopic printing and the upstream-style About dialog. [Implementation, checks and limits](native-progress-help.md). The original Progress presentation, control and completion paths are reused through Qt adapters. Successful jobs close automatically; Test shows original statistics and CRC/SHA opens the original property list. See the [completion batch and executed validation](progress-completion-port.md).
- 7z encryption including encrypted filenames; ZIP AES-256 / ZipCrypto. Upstream ZIP passwords are restricted to ASCII.
- Extraction destination/history, selection/all, three path modes, five overwrite modes, a shared six-choice per-file replacement dialog and root-folder elimination. Absolute extraction requires explicit confirmation.
- Extraction paths and metadata now come from the original archive callback. Safe symbolic/hard links, folder modes/times and available creation times are retained during guarded installation. [Source reuse, tests and remaining gaps](extraction-metadata-port.md).
- Nonblocking archive jobs, progress/logs, Pause / Continue, cancellation and detailed exit-code failures. New archive/extraction output is staged. Generic Split / Combine report actual bytes and clean staged files on cancellation. [Pause and nested-archive scope](nested-archives-progress.md).
- Filesystem copy/move, Trash deletion, rename, new folder/file and properties; existing transfer outputs prompt individually, with Yes/No, All and Auto Rename choices. Skipped move sources are retained. [Overwrite behavior and limits](overwrite-dialog-spec.md). Archive deletion/rename require confirmation. Empty folders can be created in writable 7z/ZIP/TAR/WIM, including internal subfolders and multiple WIM images. The official Agent CreateFolder body retains existing packed streams; staged folder properties are checked before replacement. Working-folder preferences and cross-volume installation are supported; archive permissions and extended attributes are retained. [Official folder updates and tests](native-folder-update.md).
- Split / Combine with `.001`-style numbering, wider numbering when needed, multiple sizes and repetition of the last size; missing parts are errors instead of silently truncated output.
- Hard links and relative/absolute file or directory symbolic links, guarded existing-symlink editing, raw-target display and both folder browse controls. Ordinary existing files/directories are protected. [Link behavior and limits](link-dialog-spec.md).
- Select / Deselect by Type, dynamic Favorites and archive-folder bookmarks. Alt+digit / Alt+Shift+digit and native RightCtrl handling preserve the upstream keys; physical-key validation remains pending.
- Official-engine Benchmark GUI with the Windows callback path, 3/2 dictionary choices, live Current/Resulting values, full-precision accumulation, Size/GIPS/CPU/system display and Restart / Stop. A fresh dialog defaults to 10 passes; existing port choices are retained. [Implementation and checks](benchmark-port.md).
- Tools → Delete Temporary Files browses the original temporary-name patterns and this port's Qt names, using the imported bounded counter/size formatter. Navigation, sorting, context actions, settings and confirmed Trash deletion are available; live/changed data is protected. [Scope and checks](temporary-files-port.md).
- Filesystem / Flat View enumeration and metadata run on a worker, with cancellable GUI row batches and protection against stale navigation. Text is formatted on demand, large-list comparisons and native icons run on workers, and original names remain distinct from display markers. Final row insertion/reordering and metadata parsing still include GUI work. See [the callback display batch](panel-listing-port.md).
- Open Inside * and # match the official one-level detection and parser-region modes, retaining the chosen mode through Refresh, extraction, Test, CRC and nested parent navigation. See [mode semantics and limits](archive-open-modes.md).
- Focused-item Comment / Ctrl+Z: compatible UTF-8 `descript.ion` comments and ZIP entry comments, including ZIP64 and supported encrypted ZIPs. ZIP updates now import the official Agent CommentItem body and real-index selection, including same-name siblings and Flat descendants. Packed data is retained without an unrelated password requirement. Saves are staged and guarded against external changes. See [source reuse and tests](native-agent-comments.md) and [limits](file-comments-spec.md).
- F3 recursively fills filesystem folder Size / Folders / Files, with asynchronous Pause / Cancel and numeric sorting. See [folder statistics](folder-statistics.md).
- Upstream-style two-column Properties: one/many/no selection, cached folder/Flat totals and CRC, nested archive layers, Ctrl+A/copy and full-value display. Typed/raw properties, format-specific columns and folder totals now reuse official handlers, formatters and Agent proxies. Parent metadata is refreshed after nested write-back. [Native bridge and remaining scope](native-metadata.md).
- Native archive rows and Properties now use original Agent directory/item identities and imported property methods. Implicit folders, Flat ordering and archive alternate-stream navigation are verified with genuine duplicate ZIP entries and an NTFS image. [Source reuse, test command and limits](native-agent-properties.md).
- Selected Extract/Test/archive hashes and focused temporary opening now reuse the official Agent selection methods, including independent same-name entries and normal/Flat folder policies. A 64 MiB selected operation verifies Pause/Cancel and reuse. [Selection port and remaining update scope](native-agent-selection.md).
- Delete/Rename in 7z/ZIP/TAR/WIM import the official Agent update bodies and select native item indices, including same-name ZIP siblings, Flat folders and multi-image WIM. Atomic installation, encrypted solid repacking and Cancel/original retention are tested. [Source reuse and limits](native-agent-item-updates.md).
- Official Agent UpdateOneFile now handles editor/nested file replacement by native item identity, including same-name siblings and single streams. Pending sessions, ancestor/Flat folder addresses and native row selection are rebound after supported mutations ([refresh checks](archive-refresh-port.md)); decoded replacement size/SHA-256 are verified without an unrelated whole-archive Test. [Source reuse and checks](native-agent-replacement.md).
- Single-stream Rename/Delete also use original Agent operations and handler behavior. Renamed pending editors and nested parents retain their native targets, selection and focus. [Behavior table and GUI/console comparison](single-stream-updates.md).
- Open/Outside import the original multiple-item policy, including the 20-item limit, folder stopping rule and single-item internal attempt. Separate native-index temporary extractions allow same-name ZIP siblings to open and write back independently. [Source and checks](panel-open-port.md).
- Default Open detects archive contents with an unknown or absent extension. The official external-opening extension table and misleading-filename warnings are imported. Password cancellation and permission errors do not fall back externally. [Source and checks](open-profile-port.md).
- External editing imports the official same-executable process-discovery loop, covering independent launcher hand-off and confirmed ZIP write-back; existing seven-format and encrypted/nested checks pass. [Source and observation limits](external-process-port.md).
- Configurable viewer/editor/diff, confirmed archive-file editor write-back, persistent six-page Options and 92 official translations plus English. The official language parser and translator/missing-entry information are ported; bundled Lang files are editable. Menus and primary dialog controls use official resource IDs. Apply/Cancel behavior is tested; some Port-specific text remains English. [Source scope and tests](language-settings-port.md).
- All 11 upstream CRC/hash menu choices for filesystem files and selected archive contents, including folders and the all-methods choice. Archive hashing streams through the official engine without writing extracted files. Encrypted 7z/ZIP inputs are tested.
- Alternative selection imports the original File Manager mark, Insert, Ctrl/Shift-click and Shift-arrow bodies. Pink operation marks and native focus remain independent in both views and panels; refresh and Options Apply preserve them. [Source and checks](panel-selection-port.md).
- Ctrl+C copies marked names as CRLF-separated text, matching the upstream File Manager; alternative mode does not fall back to an unmarked focused row. Upstream Ctrl+X/Ctrl+V handlers are empty; they do not transfer files.
- File Manager right-click menus now import the official ordinary File-menu filtering, with CRC/Diff and one creation group. Configured 7-Zip shell commands remain shared with Open With. Internal filesystem drag Copy/Move, right-drag menus, target subfolders and asynchronous encrypted archive drag-out are tested; accepted temporary files survive Manager exit. [Source, checks and native interaction limits](panel-menu-drag-port.md).
- Finder **Open With** displays a configurable 7-Zip action menu. Its bottom File Manager entry is always available. [Finder integration](finder-integration.md) explains scope and known association behavior.
- Original Windows File Manager application icon: unmodified official `FM.ico`, PNG/ICNS format conversion reproduced by `scripts/make-icon.sh`.

- Filesystem Copy / Move can target the other open archive panel and its internal folder, using official handler defaults. Archive-folder drops accept different input parents; verified Move uses Trash. [Behavior and limitations](archive-transfer.md).
- Single-layer archive updates preserve leading prefixes using the original Agent/console stream boundary and official handlers. Folder creation, replacement, ZIP comments and nested write-back verify prefix bytes before replacement. [Source reuse and tests](archive-prefix-updates.md).
- Same-volume filesystem Move now tries rename first, including folders, links and all overwrite choices. Cross-device/SMB transfers have a guarded copy fallback. Delete-after verification supports Pause/Cancel and callback/local progress. [Scope, OS limits and executed tests](filesystem-transfer.md).

## 日本語

- File / Edit / View / Favorites / Tools / Helpと、Add / Extract / Test / Copy / Move / Delete / Infoツールバー。
- 通常フォルダー、7z / ZIP等のアーカイブ内フォルダーの閲覧、親へ移動、複数選択、ソート。Large Icons / Small Icons / List / Details、Flat View、2 Panels、列幅・列順・表示列・日時精度・UTC・自動更新・フォルダー履歴を保存できます。
- 7z / ZIP / TAR / WIM / XZ / gzip / bzip2の作成とTest・展開。Hash形式でSHA-256 / SHA-1のチェックサムファイル作成・Test。XZ / gzip / bzip2の入力は通常ファイル1件。RAR圧縮は提供しません。
- 辞書・Word size / PPMd order・solid・スレッド数・圧縮メモリー・分割・圧縮プロパティ・更新モード・相対／Full／絶対パスを指定。詳細Optionsで日時精度・日時の保存・最新ファイル日時・アクセス日時保持、対応形式でsymbolic / hard link保存を設定できます。
- 全体／選択項目の展開、出力先履歴と名前付きサブフォルダー、Full / No / Absolute pathnames、root folder重複除去、Ask / Overwrite / Skip / Auto rename / Auto rename existing。絶対パス展開は確認付きです。
- Test、暗号化7z（ファイル名暗号化を含む）、ZIP AES-256 / ZipCrypto。ZIPのパスワードは上流と同じくASCIIに制限。
- 非同期処理、進捗・ログ・Cancel、終了コードを含む結果表示。圧縮／展開後の不完全な新規出力はステージングから片づけます。
- 通常ファイルから、別パネルで開いているarchiveと内部folderへCopy／Moveできます。WindowsのCopyFromと同じ既定圧縮・同名置換を使い、Moveは検証後にTrashへ移します。異なる親から内部folderへのdropにも対応します。[仕様と残差](archive-transfer.md)。
- 通常ファイルのCopy / Move / Trash / Rename / Create Folder / Create File / Properties。コピー・移動と展開で、Yes / No / Yes to All / No to All / Auto Rename / Cancelの個別上書き確認を共用します。スキップした移動元は保持します。[仕様と制限](overwrite-dialog-spec.md)。
- アーカイブ内の削除・リネーム。削除前に確認します。
- 通常フォルダー／Flat Viewの列挙とmetadata取得をworkerで行い、一覧の行生成も分割します。読み込み中の移動・Escキャンセル、旧一覧保持、2panelに対応。表示文字列は必要時に取得し、大量項目の比較とnative icon取得はworkerへ移しました。最後の行挿入／並べ替え反映・metadata解析には同期処理が残ります。[元実装・検証・制限](panel-listing-port.md)。
- F3で通常フォルダーの合計サイズ・子フォルダー数・ファイル数を集計します。複数folder選択、数値ソート、非同期Pause／Cancelに対応。[仕様・制限](folder-statistics.md)。
- Propertiesは上流に近い2列modalで、単体／複数／未選択、通常／Flatのfolder集計・CRC、ネストしたarchive階層、Ctrl+A・コピー・全文表示に対応。型付き／raw属性、形式別の列、集計は公式handler・formatter・Agent proxyを再利用します。nested保存後は親の属性も更新します。[実装と残差](native-metadata.md)。
- archive一覧とPropertiesは、公式Agentのフォルダー・項目番号と、移植した属性取得処理を使います。暗黙folder、Flat順序、archive内の代替ストリーム閲覧を、同名ZIP項目と実NTFS imageで検証しました。[移植・テスト方法・残差](native-agent-properties.md)。同名項目のRename／Deleteもnative indexで処理します。
- 選択展開・Test・archiveハッシュ・focused項目の一時展開も、公式Agentの選択処理を移植しました。同名項目の個別処理、通常／Flatのフォルダー選択、64MiB展開のPause／Cancelと再実行を検証しました。[移植と残る更新処理](native-agent-selection.md)。
- 7z／ZIP／TAR／WIM内のDelete／Renameは公式Agentの処理を移植し、native項目番号で操作します。同名ZIP、Flat folder、複数image WIM、暗号化solid再圧縮、Cancel時の元データ保持を検証しました。[移植範囲と制限](native-agent-item-updates.md)。
- gzip／bzip2／XZのDelete／Renameも公式Agentへ接続しました。名前を保存しない形式や削除失敗、XZの空streamは上流と同じ挙動です。編集中のRename、2panelで開いた子archiveの親側Renameと書き戻し、選択・focus保持も検証しました。[仕様表とGUI／console比較](single-stream-updates.md)。
- editor／nested書き戻しは公式AgentのUpdateOneFileを移植し、同名項目もnative番号で更新します。更新後のsession／選択を引き継ぎ、置換した内容のサイズとSHA-256を検証します。7形式・暗号化・先頭prefixを含む101件の対象検証が成功しました。[移植範囲と実行記録](native-agent-replacement.md)。
- Open／Open Outsideの複数選択処理を公式bodyから移植しました。20項目の上限、フォルダーで停止する順序、同名ZIP項目の個別起動・編集・書き戻しを検証しました。[範囲と残差](panel-open-port.md)。
- 不明な拡張子・拡張子なしでも、通常のOpenから内容を判定してアーカイブを開きます。外部起動の拡張子一覧・紛らわしい名前の警告は公式bodyを移植。権限エラーとpassword取消では外部起動しません。[移植と検証](open-profile-port.md)。
- アーカイブ内ファイルの一時展開と外部アプリでのOpen Outside／View／Edit。終了後の変更を確認して書き戻し、失敗・Cancelでは回復用copyを残します。実行中editorはManager終了後も停止・削除しません。
- Tools → Optionsから設定を保存・変更できます。System / 7-Zip / Folders / Editor / Settings / Languageの順は上流と同じです。OKで保存して閉じ、Applyで即時反映、Cancelで最後のApply以降の変更を破棄します。
- OptionsのSettingsで親項目、実ファイルアイコン、行全体選択、グリッド、単一クリック、代替選択、Test／展開のメモリー上限を指定。Foldersで作業用フォルダーを指定し、EditorでF3 View／F4 Edit／2ファイルのDiffに使う外部アプリやコマンドを設定できます。設定は再起動後も保持します。
- Languageで公式の92翻訳＋Englishを選択して即時反映できます。圧縮・展開設定も保存しますが、パスワードは保存しません。圧縮後削除は確認後、完成アーカイブのTestと元データ照合が成功したものをmacOSのTrashへ移動します。
- 右クリックは公式Fileメニューの通常filterを移植し、CRC／Diffと作成項目の重複を修正。パネル間ドラッグのCopy／Move・対象folder・右ボタンmenuと、暗号化archiveの非同期drag-outを自動検証しました。受け取り側のため、承認済み一時fileはManager終了後も保持します。[移植範囲・116件の検証・未確認事項](panel-menu-drag-port.md)。
- FinderからのファイルドロップによるAdd、アーカイブのドロップによるOpen。アーカイブ内ドラッグは一時展開後にファイルURLを渡す実装がありますが、Finderへの実ドロップは未検証です。
- F2 / F3 / F4 / F5 / F6 / F7、Enter、Backspace、Ctrl+A、Ctrl+R、Ctrl+PgDown、Alt+Enter等。物理Controlを維持し、Commandへの自動置換を抑止。
- 通常ファイルと選択したarchive内部のCRC-32 / CRC-64 / XXH64 / MD5 / SHA-1 / SHA-256 / SHA-384 / SHA-512 / SHA3-256 / BLAKE2sp / 全方式。folderも対象になり、archive内部は公式engineで展開ファイルを作らず計算します。暗号化7z／ZIPも自動検証しました。
- Ctrl+Cで選択した名前をCRLF区切りのテキストへコピー。上流同様、Ctrl+X／Ctrl+Vは空の処理でファイルを転送しません。
- 書き込み可能な7z／ZIP／TAR／WIM内へ空folderを作成。公式AgentのCreateFolderを移植し、内部subfolder・複数image WIM・暗号化に対応。既存packed streamを保持し、コピー上で追加・一覧照合後に元を置換します。作業folder設定、SSD／SMB間の更新、権限・拡張属性保持を確認しました。旧データのpassword要求も上流と同じ必要時のみです。[移植と検証](native-folder-update.md)。ネスト内の変更も、退出時の確認を経て階層ごとに親へ書き戻します。先頭prefix付きarchiveは公式Agent／consoleのstream処理を再利用して更新し、置換前にprefixの一致を検証します。[仕様と検証](archive-prefix-updates.md)。tail／複数層の更新はWindows Agent自体も禁止しています。
- Finderの「このアプリケーションで開く」で選ぶと、Windowsの7-Zipコンテキストメニューに対応する操作メニューを表示します。圧縮・展開・Test・CRC SHAの表示をOptions → 7-Zipで選択できます。最下段の「7-Zip ファイルマネージャーで開く」は常に表示します。[使い方](finder-integration.md)。
- `.7z` / `.zip`等と通常ファイル・フォルダーをAlternate ViewerとしてInfo.plistに宣言。既定アプリのシステム設定は変更しません。

- 通常ファイルのSplit / Combine、hard / file symbolic / directory symbolic link作成、既存symlinkのguard付き編集。raw targetと両folder browse、Flat／2panelの既定値を再現し、通常file／folderは保護します。[仕様と制限](link-dialog-spec.md)。分割番号はWindows同様001から始め、1000件以上なら桁数を増やします。
- Select by Type / Deselect by Type、Alt＋数字のFavorite呼出、Alt＋Shift＋数字の保存、RightCtrl＋数字のキー処理。物理RightCtrlは未確認ですが、ネイティブキーコードを含む自動検証は成功しました。
- Tools → Benchmarkは公式のcallback経路でCurrent／Resulting、Size・CPU使用率・GIPS・CPU／OS情報を表示します。中間辞書サイズ、丸める前の累積、初期10パス、Restart／Stopに対応。[実装・検証](benchmark-port.md)。Help検索には履歴選択とAND／OR／NOT／NEARの挿入メニューもあります。
- 圧縮設定の`*`自動値とメモリー見積もりに、Windows版の計算処理を利用します。自動／手入力の選択を保存します。Helpは公式英語70ページ、目次・索引・履歴と各dialogの対応ページを開けます。[仕様・検証・残差](compression-help-port.md)。
- Helpの非同期全文検索、topic／subtopic印刷、上流に近いAboutを追加しました。印刷の自動試験は専用PDFへの出力です。実プリンターは未確認です。[実装と確認範囲](native-progress-help.md)。
