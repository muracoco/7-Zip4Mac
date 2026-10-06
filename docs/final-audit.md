# Windows版との機能監査（現行整理: 2026-10-06）

**判定: 利用可能な自動統合確認とローカルの公開用ソース整理は実行済み。物理操作の確認と公開は未完了。** [一括確認と初回失敗・修正結果](release-consolidation.md)、[公開用ソース・新規ビルド確認](publication-review.md)に記録する。65コマンドの接続と、上流・追加の制限を区別する。現在の作業一覧は [current-status.md](current-status.md)。過去のコンポーネントレポートは実行証拠として保持するが、そこに記載された旧残作業を再実装しない。

下表の実装分類は主機能の接続状況を示す。物理入力・画面の完全一致や全codec variantを検証済みという意味ではない。

現行の対象は、上流65静的コマンド、動的メニュー・設定・キー処理と、その後に接続した元Agent更新、展開callback、Open／Copy／一覧／dialogの機能群。下部に保持した古いcheckpointの対象・残作業とは区別する。Windows実機の全操作・画面の比較や、全archive handlerの全組合せを検証したという意味ではない。

## 上流の確認

ユーザー提供の`7z2603-src.tar.xz`は、取得済み公式26.03ソースとSHA-256が一致した。

- Source: `9cbde5099c6deb73691b0579063da5827522ccbbcba3f0020fd04e8c8c16c0d4`
- 提供されたMac配布archive: `5ca87677072c59f5602e5c49baa27d4694bacd2259b4e507f0094249d4281480`

Mac配布archiveの構成を確認したが、そのbinaryへ置き換えていない。アプリは公式ソースからApple Clangでビルドした26.03の7zzを同梱する。Qtは6.11.3固定、ローカル修正版Cocoa pluginを動的に使用する。

参照: `UI/FileManager/resource.rc` / `MyLoadMenu.cpp` / `PanelKey.cpp` / `PanelSelect.cpp`、Options各ページ、`UI/GUI/CompressDialog.cpp` / `.rc` / CompressOptionsDialog、ExtractDialog、ProgressDialog2、`UI/Explorer/ContextMenu.cpp` / `ArchiveName.cpp` / `FileManager/MenuPage.cpp`。詳細なファイル一覧は[windows-parity.md](windows-parity.md)。

## 有効な静的メニュー65コマンド

`resource.rc`からコメントとWindows CE専用Benchmark2を除外した65コマンドを照合した。CRC等の同じ状態の行はまとめ、件数を示す。動的メニューとキー処理は次節で別に確認する。Implementedはそのコマンドの主機能を提供する意味で、全UI・全metadataの完全一致を保証しない。

| 上流コマンド | 件数 | 分類 | 現状 |
|---|---:|---|---|
| Open | 1 | Implemented | 元multi-item／extension profile、nested書き戻しと共有panel寿命を接続。panel-open-port.md、open-profile-port.md、panel-key-port.md。外部processの観測限界と実機入力は別記。 |
| Open Inside | 1 | Implemented | 拡張子に関係なくengineへ渡し、ネストarchiveも安全な一時展開後に同じpanelで閲覧する |
| Open Inside * / Open Inside # | 2 | Implemented | 非再帰の外側open／parser領域open。modeをRefresh・展開・Test・CRC・親復帰で保持し、領域byteと暗号化親を検証。archive-open-modes.md |
| Open Outside | 1 | Implemented | 一時展開後に外部アプリへ渡す |
| View | 1 | Implemented | focused fileのViewer／外部open・確認付き書き戻し、folder F3集計とsymlink folder移動を接続。panel-key-port.md、folder-statistics.md。 |
| Edit | 1 | Implemented | 元UpdateOneFile／process探索、7書込形式・nested・暗号化・同名row identityを接続。external-process-port.md、native-agent-replacement.md。 |
| Rename | 1 | Implemented | 通常FS／Flatの相対移動と7z／ZIP／TAR／WIM内部の安全な相対pathを接続。focused F2と排他的失敗・内容保持を検証。panel-key-port.md |
| Copy To... / Move To... | 2 | Implemented | 元Copy dialog／focused dispatch／copy name／正確なAgent入力列挙を接続。6択上書き、FS／archive間と2panel。元archive MoveはE_NOTIMPL。copy-projection-port.md、filesystem-transfer.md。 |
| Delete | 1 | macOS difference | FSは確認後Trash、archive内部は確認後削除 |
| Split file... / Combine files... | 2 | Implemented | 上流の連番・複数サイズ・最後のサイズ反復。非同期・byte進捗・Cancel・同名保護。欠番は切り詰めずエラーにする |
| Properties | 1 | Implemented | 元2列modal・native proxy graph・typed/raw formatter・実項目番号・単体／複数／folder／Flat／CRCとコピーを接続。native-agent-properties.md、native-agent-item-updates.md。 |
| Comment... | 1 | Partially implemented | focused FS／ZIP実項目の編集・削除、Ctrl+Z、選択保持を実装。ZIPは公式Agent CommentItemを移植し、同名／Flatの実項目をnative indexで更新。packed dataを保持し、無関係なpasswordを要求しない。signed ZipCrypto descriptorと日時保持を修正。unsigned更新はWindows元handlerのE_NOTIMPLへ一致。特殊名／legacy codepage／複数volume等の制限は別途保持。native-agent-comments.md / file-comments-spec.md / zip-metadata-port.md |
| CRC-32 / CRC-64 / XXH64 / MD5 / SHA-1 / SHA-256 / SHA-384 / SHA-512 / SHA3-256 / BLAKE2sp / * | 11 | Implemented | 通常FSと選択したarchive内部。全11方式を7z／ZIP、7z data/header暗号、ZIP AESで自動検証。folder選択とliteral wildcardも確認 |
| Diff | 1 | Implemented | 設定した外部コマンドへ通常ファイル2件を渡す |
| Create Folder / Create File | 2 | Implemented | 元FS絶対／親相対GetAbsPathと、Agent空folder作成を接続。native index／prefix／複数image WIM／暗号化／nestedを検証。archive Create File／tail／複数open層は元実装も非対応。address-workflow-port.md、native-folder-update.md。 |
| Link... | 1 | Implemented | hard / file symbolic / directory symbolic作成と既存symlink編集。raw相対target、両folder browse、選択1件・Flat・2panel既定値を実装し28件自動検証。上流の選択処理を再確認して未選択focusでの実行を除去。NTFS reparseの削除・空file変換とPOSIX型情報にはOS差、ACL独立fixtureとnative操作は未確認。link-dialog-spec.md |
| Alternate streams | 1 | Implemented | 元native stream folder graphと実番号で閲覧・Properties・選択処理を接続。NTFS image fixtureで検証。hostへのNTFS ADS適用はOS差。native-agent-properties.md、native-agent-item-updates.md。 |
| Exit | 1 | Implemented | 通常終了。処理中はCancel |
| Select All / Deselect All / Invert Selection / Select... / Deselect... | 5 | Implemented | 全体・反転・wildcard |
| Select by Type / Deselect by Type | 2 | Implemented | focused itemの最後のdot以降を大小文字区別なしで比較。folder群／拡張子なし群も上流同様 |
| Large Icons / Small Icons / List / Details | 4 | Implemented | 4表示、保存・復元 |
| Arrange: Name / Type / Date / Size / Unsorted | 5 | Implemented | 元自然順・型付き／raw比較・tie・初回方向を移植。動的archive列でもproperty IDで選択、Unsortedはload順と反転。FS／format別／2panel別に保存（panel-sort-port.md） |
| Flat View / 2 Panels | 2 | Implemented | 保存、フォーカスpanelへキーを配送 |
| Time | 1 | Implemented | 上流FILETIME formatter・精度表を移植。日付submenuと現在時刻の5精度sample、UTC Z、初期の分表示・保存をQtで検証。version-menu-time-port.md |
| Archive Toolbar / Standard Toolbar / Large Buttons / Show Buttons Text | 4 | Implemented | 個別切替・保存、公式画像 |
| Open Root Folder / Up One Level / Folders History... / Refresh / Auto Refresh | 5 | Implemented | 通常FS／archiveの移動・更新。上部editable comboの階層dropdown、選択／Enter／Escape／Tabとnested復帰を接続（address-workflow-port.md） |
| Options... | 1 | Implemented | 6ページのportable設定・Apply／Cancel・保存を接続。実機Toolsから表示し、7-Zipタブの空白を修正。93言語・AppKit Applyを確認。Port独自説明の翻訳は別にPartially。settings-coverage.md、desktop-follow-up.md。 |
| Benchmark / Delete Temporary Files... | 2 | Implemented | 公式Benchmark callbackと元temp browserの列挙・Properties再集計・layout・Helpを接続。benchmark-port.md、temporary-files-port.md、dialog-text-help-port.md。物理確認は別記。 |
| Contents... | 1 | Implemented | 元70ページ・目次／索引、検索・履歴／operator・印刷・context F1と単一Help windowを接続。Windows HtmlHelpのOS表示／検索品質はOS差、物理印刷は未確認。dialog-text-help-port.md。 |
| About 7-Zip... | 1 | Implemented | 元resource配置・実寸logo・版数／日付／copyright／翻訳info・homepage／OK／F1と独立Helpを実装。非公式Portとlicense表示を併記。dialog-text-help-port.md |
| **合計** | **65** | | |

## 静的メニュー以外の差

| 領域 | 分類 | 判定 |
|---|---|---|
| 圧縮形式 | Implemented | 上流GUIの有効な7z / ZIP / gzip / bzip2 / XZ / TAR / WIM / Hashを提供。Hashは今回追加したSHA256 / SHA1。g_FormatsのZSTD / Swfcはコメントアウトされており、有効なGUI形式に数えない |
| 圧縮設定 | Implemented | 元format／method／数値combo・自動値／memory・enable／draft／reset・Options・resource翻訳とfont extentを接続。compression-options-port.md、dialog-text-help-port.md。 |
| 展開設定 | Implemented | path／overwrite／password／root／選択設定、元事前overwrite／Skip・ordered publication・link/path／metadata・partial failureと終了回復を接続。normal-extraction-integration.md、extraction-path-combinations.md、extraction-lifecycle.md。 |
| Progress | Implemented | 元表示／制御／成功auto-close／Test・Hash結果／個別error Closeを接続。progress-presentation-port.md、progress-completion-port.md。 |
| 列・属性 | Implemented | 元FS列・property設定・native graph／schema／typed/raw列・native item操作を接続。panel-sort-port.md、native-agent-item-updates.md。 |
| 一覧読み込み | Implemented | FS／archive worker、元callback表示・sort／icon worker・世代照合を接続。model挿入／parseは同期の性能限界。元owner-dataはコメントアウト。panel-listing-port.md。 |
| 一覧context menu | Implemented | 元CFileMenu filter／optional VerCtrl／順序／条件とMac Open With共用submenuを接続。AppKit右button回帰は成功、外部desktopからの操作は未確認。panel-menu-drag-port.md、version-menu-time-port.md、desktop-follow-up.md。 |
| Optional VerCtrl | Implemented | VerCtrl.cppの本体とCFileMenu後半を移植。root QSettingsの7vcとDiffで表示。Edit／Commit／Revert／Diff、履歴・bytes／time／readonly、GUI確認を実ファイルで検証。POSIX属性・安全な配置への置換はversion-menu-time-port.md参照 |
| Favorites | Implemented | 0–9、元Alt／RightCtrl dispatch・動的path／folderとShift保存を接続。物理修飾キーは未確認。 |
| Clipboard | Implemented | Ctrl+Cは選択した名前をCRLF区切りのテキストへコピー。PanelMenu.cppのEditCut／EditPaste本体は空実装で、Ctrl+X／Ctrl+Vによる転送は上流に存在しない。実装本体を確認して旧監査の記載を訂正 |
| その他accelerator | Implemented | 元PanelKeyの全dispatchとproperty tableを移植。header側Alt+F1/F2／Tab／Escape／F9／Ctrl+W、一覧2panel Tab・modifier優先順位・keypad selectionを接続（address-workflow-port.md）。実Fn／RightCtrl／Option入力はQt自動検証とは別に未確認 |
| Options / Language | Partially implemented | 6ページ・Apply / Cancel・92言語＋English。独自mark操作は公式処理へ移植・検証済み。Port説明・動的エラーの翻訳は不足 |
| Finderからのdrop / archive drag-out | Partially implemented | 公式effect／right-drag tableを移植。FS Copy／Move・hover folder・同panel拒否・全view、archive Copy確認・非同期drag-outを接続。32 MiB・暗号化・Cancel・受領fileの終了後保持を検証。実Finder／native右button開始は未確認。panel-menu-drag-port.md |
| Mac Open Withメニュー | Implemented | 10項目・icons設定、条件表示、CRC、最下段Manager、FileOpenの待機・キュー |
| Shell Open As choices | Implemented | Finder Open With／FS右クリックで上流7選択・#:e、独立表示設定、同じarchiveの再openを追加。上流PanelMenu.cppはIsArcFolder時にShellメニューを生成せず、archive内部ではFileメニューのInside／*／#を表示。旧監査のarchive内部形式選択の不足判定を訂正。archive-open-modes.md |
| Finder Extension / Quick Action / Services | Not implemented | Open Withとは別の連携方式 |
| Windows OS機構 | macOS difference | registry関連付け・Explorer登録・32bit/cascade・shell system menu・NTFS security/ADS/Zone.Identifier・PE SFX・large pages・sharing lock・MAPIメール |
| タイトルバー / Trash / Gatekeeper / function keys | macOS difference | OS標準機構。既定関連付けや権限を強制変更しない |

設定項目の個別表は[settings-coverage.md](settings-coverage.md)、Open Withの操作は[finder-integration.md](finder-integration.md)。

## 初期18条件の現行確認表

「自動確認済み」は実際のMac上で専用fixture／Qtイベント／プロセスを実行した記録であり、すべての項目を人間がクリックした意味ではない。画面と物理入力は別欄に保持する。主な実行証拠は [release-consolidation.md](release-consolidation.md)、[publication-review.md](publication-review.md)、[desktop-follow-up.md](desktop-follow-up.md) と、それらからリンクした生ログ。

| 条件 | 実装・実行証拠 | 残る確認 |
|---|---|---|
| 1. macOS .app生成 | 新規空ディレクトリでRelease/Ninja、Qt・engine同梱、署名・依存確認済み | Intel／別macOSは未確認 |
| 2. .app起動 | 同梱候補をLaunchServicesで起動、プロセス・独立設定・標準出力／エラーを確認済み | 現行候補の物理操作 |
| 3. 通常FS閲覧 | 通常／Flat／非同期読み込み・移動の自動GUI確認済み | 実キー・画面 |
| 4–5. 7z／ZIP内部表示 | 元handler／proxy／項目番号で一覧・内部移動を自動確認済み | 実ダブルクリック |
| 6–7. 7z／ZIP圧縮 | 複数file、空／階層folder、日本語・空白、32 MiBで圧縮・内容比較済み。desktopのAddで48 MiB入り7zを作成し5file SHA-256一致 | 現行候補の物理右クリック／ZIP操作 |
| 8–9. 7z／ZIP展開 | 別folderへ実出力、SHA-256／階層比較、overwrite／失敗を確認済み | 現行候補の物理右クリック |
| 10. Test | 通常／選択、成功／破損・誤passwordの結果とGUI再使用を自動確認済み | 物理結果window操作 |
| 11. password付き7z | data／header暗号、正誤password、再試行を自動確認済み | 物理password入力 |
| 12. 失敗後にGUIが復帰 | engine失敗・I/O・password・CancelとCocoa ownership回帰確認済み | あらゆる未知条件の無故障を保証しない |
| 13. 長時間Cancel／非同期 | 32／64 MiBの進捗・Pause／Cancel・残存出力／再使用を確認済み | 物理Cancel／OS focus |
| 14. 日本語名 | 圧縮・一覧・展開・更新・Copy／Moveの実ファイルで確認済み | 未知のlegacy codepage variant |
| 15. Windowsに近い主画面 | 元menu／toolbar画像／列／resource配置／キーを接続し自動確認済み | 物理クリックとpixel照合は未完了 |
| 16. 未実装差分文書 | 現行表・設定表・上流対移植の保護条件を記録済み | 安全上の追加制限を完全同一と報告しない |
| 17. build／test手順 | 英日READMEとbootstrap／build／test、Gitなし別root再ビルドを確認済み | 現在のMac以外は未確認 |
| 18. 実行結果 | 初回失敗と修正、現行35suiteの一括実行と失敗群の個別修正記録、151登録組、対応するcase／log hashを保存済み | 合算結果を単一連続実行と報告しない |

## 次の完了対象

1. 現在の候補でFinder Open With／右クリック／drop、Fn／RightCtrl／Option、印刷・外観をまとめて確認する。native menu／OptionsのAppKit確認とdesktopでのOptions表示はdesktop-follow-up.mdに記録済み。確認待ちを未実装と扱わない。
2. 実機で再現した不具合だけを修正し、その影響範囲を確認する。合格済みの形式matrix／全suiteを理由なく繰り返さない。
3. 差分・未確認事項を最終判定へ反映し、条件を満たした時点で公開用コピーから公開する。上流の制約・追加保護は [update-safeguards.md](update-safeguards.md) に区別した。

Port固有文言の翻訳、最後のmodel挿入／parseの遅延、任意のcodec variant、別方式Finder連携は個別の差分・改善候補である。すでに接続済みのOpen／更新／展開／Progress機能を再実装する理由にしない。

## 過去の機能群と実行証拠

以下は各checkpoint時点の記録です。旧残作業の記載は上の現行表とcurrent-status.mdで置き換えられています。

### 初回の修正と検証

Open With経路、メニュー表示設定、公式engineのCRC11方式、Hash作成・Testを追加した。通常ファイルのOpen Insideが外部アプリへ流れていた問題を修正し、archiveの拡張子を`.txt`へ変更したケースで内容を開けることを確認した。

実機Finder確認で発見した「圧縮後の一覧更新がProgressの結果とCloseを初期状態へ戻す」問題は、翻訳の適用を同じウィンドウ内に限定して修正した。メニュー選択中に親File Managerから別操作が始まらないよう、非同期で表示するmodal dialogとした。

クリーンビルド先でQtTest36件とCocoa回帰検証3/3、7z/ZIP往復・32MiB・日本語・暗号化・失敗・Cancel・checksumsを検証した。実Finder経由の作成・展開・内部一覧は別に確認した。[実行記録](test-results.md)、[生ログ](open-with-test.log)。

未確認: Windows実機の全UI比較、Intel/macOS15実機、Finderへのarchive drag-out、全function-key設定、物理リムーバブル媒体、disk-full故障注入、全handler/全設定組合せ。自動GUI検証を人間による全項目のクリック検証として報告しない。

## Official Agent selection update (2026-10-05)

選択展開・Test・archiveハッシュとfocused項目の一時展開は、公式GetRealIndex／GetRealIndicesのbodyへ接続済み。同名項目・通常／Flat・NTFS stream・暗号化・安全性・64MiB Pause／Cancelを検証。原子的な更新後のsnapshotも更新し、先頭prefix／nested書き戻し後の再openを検証。7z／ZIP／TAR／WIMのRename／Deleteも公式AgentOutのbodyへ接続し、同名項目・複数image・暗号化solid・Cancelを含む93件を検証。ZIP Commentのnative index更新と選択保持も公式bodyへ接続した。[Comment移植](native-agent-comments.md)。editor／nestedのreplacementは公式UpdateOneFileへ接続し、同名項目とsingle-stream置換、session／選択復元を検証した。[書き戻し移植](native-agent-replacement.md)。single-stream Delete／Renameは公式handlerの挙動に接続し、GUI／console比較を検証した（single-stream-updates.md）。編集中Rename・親側Rename後のnested書き戻しとroot項目の選択／focus保持も検証。ancestor／Flat folderのaddress・選択／focus復元とDelete後の親復帰、ancestor Rename後のeditor／nested書き戻しは26件の対象検証を通過（archive-refresh-port.md）。同名出力の上書き順序と残るUI／一覧の差は未完了。[選択処理](native-agent-selection.md)、[更新処理と実行記録](native-agent-item-updates.md)。全機能同等・最終版とは判定しない。

## Normal extraction failures (2026-10-05)

公式callbackのCloseFile／SetOperationResult／SetPostLinks／CloseArcを使用し、通常の失敗終了でも記録済み出力を保持する。7z／ZIPのCRC不一致、ZIPの非対応方式、7z／ZIP AES／ZipCryptoの誤passwordと再実行、配置失敗後のfolder metadataを独立した未改変7zzへ照合した。Qt Progressの実widgetでcode・診断・Close操作・backend再使用も検証。全320件成功（setup／cleanup含む）。[範囲・残差](partial-extraction-port.md)。engine準備中のCancel／crash、展開前のoverwrite／decode Skip、残りのlink／conflict組合せは未完了。

## Version commands / Time source batch (2026-10-05)

4つのoptional version-storeコマンド、完全なFile-menu filter、動的Timeメニュー／日時formatterをまとめて移植し、関連6suiteで112件成功・1件skip（既存の別volume Move）を確認。初回のDiff path表記差を修正し、そのsuiteだけ再実行。全形式・全体clean buildの再実行は最終gateへ保持する。全機能同等・最終公開の判定は未達。[移植範囲と確認結果](version-menu-time-port.md)。

## Desktop dialog resource batch (2026-10-05)

Add／詳細Options／Extractの元resource座標・client寸法・tab順・ボタン順、Progressの元OnSize・時間／サイズ／速度formatter・files2行、Options要約・初回OS言語選択をまとめて移植。関連caseの最新結果は96成功、1件はCocoaフォーカス取得失敗でクリック確認前に停止（skipとして扱わない）。形式切替時の古いQForm参照によるcrashと地域言語の優先順位を修正。長いラベル・filename省略・status／title細部・物理確認は未完了。全形式と最終clean buildはこの機能群で繰り返していない。[移植範囲・生ログ・残課題](dialog-resources-port.md)。

### Progress presentation / control 機能群

元UpdateStatInfo、filename2行／省略、status／dialog・親title、numbered errors／Copy、元error formatterをまとめて移植し、公式callbackのtyped errorへ接続。Backgroundは優先度切替、Cancelは確認中PauseとYes／No／Cancel・完了競合を再現。CRCでの不要な形式再試行による二重表示を修正。対象caseの最新結果は122成功、失敗・skipは0（初回一括実行と失敗・影響箇所の再実行、setup／cleanupはsuiteごとに一度だけ計数）。元成功時auto-close／message-box方針、物理比較、最終clean build／全形式受入は残る。[範囲と全実行ログ](progress-presentation-port.md)。

## Command-entry workflow batch (2026-10-06)

共通Combo入力、元wildcard matcher、一覧内Rename、履歴の一覧／削除／Cancelをまとめて移植。通常FSの非同期・排他的Renameと複数階層のfolder作成、元Create File初期値も合わせた。Windowsの元実装と照合した項目・未移植項目は [command-entry-port.md](command-entry-port.md) に区別して記録。全機能完了や全形式の最終受入とは判定しない。関連機能群の一括検証後、失敗／影響箇所だけ修正・再検証する。

対象9 suite選択の最新結果は146成功・0失敗・1skip（別volume指定なし）。初回失敗・途中timeoutも生ログに保持し、修正後は失敗／影響する選択だけ再実行した。FS worker完了のbusy競合、`/var`基準path、Qtセル選択の項目statusへの変換を修正済み。詳細は [command-entry-port.md](command-entry-port.md)。
