# Windows 7-Zip 26.03との照合

Current implementation and release inventory: [current-status.md](current-status.md).
Historical checkpoint remaining-work lists are superseded by the later connected source groups; implementation gaps, defects and unverified physical input are tracked separately.

基準は未改変の公式 `7z2603-src.tar.xz`。英語UIの表示名はソース／`.rc`から確認した。Windows実機画面のピクセル比較は未実施。分類は **Implemented / Partially implemented / macOS difference / Not implemented**。

| 項目 | 分類 | 実装と残る差 |
|---|---|---|
| メニューの大分類・順序 | Implemented | File → Edit → View → Favorites → Tools → Help |
| Fileメニュー | Implemented | 有効な静的コマンドと元CFileMenu条件・optional VerCtrlを接続。全65コマンドの現行分類はfinal-audit.md。追加の安全制限はcurrent-status.md。 |
| Editメニュー | Implemented | Select All / Deselect All / Invert / wildcard Select / Deselect、Select by Type / Deselect by Type。Ctrl+Cは名前のテキストコピー。上流のCtrl+X／Ctrl+Vは空実装で、resourceのclipboardメニューもコメントアウト |
| Viewメニュー | Implemented | 4表示方式、Name / Type / Date / Size / Unsorted、Flat、2 Panels、日時精度・UTC、Toolbar、Root / Up / history / Refresh / Auto Refresh。保存・再読込を検証。Timeメニューの動的sample・元formatter・分初期値は移植済み（version-menu-time-port.md） |
| Favorites | Implemented | 動的path・0–9移動／保存・元modifier dispatchを接続。物理Alt／RightCtrl確認は別に未確認。 |
| Tools | Implemented | Options、元Benchmark callbackとDelete Temporary Files browser／Properties／Helpを接続。benchmark-port.md、dialog-text-help-port.md。 |
| Options構成・保存 | Implemented | 元portable設定、6ページ、OK／Apply／Cancel・保存と各操作への接続を実装。settings-coverage.md。 |
| Options System | macOS difference | Info.plistの宣言を表示し、Finder Get Info → Open withの手順を案内。Windowsのレジストリ操作を行わず、既定関連付けは強制変更しない |
| Options 7-Zip | Implemented | Explorer設定のMac置換としてOpen Withの項目・icons・root除去を保存。公式Shell Open Asと常時最後のManagerを接続。Windows登録／MAPIはOS差。finder-integration.md。 |
| Options Folders | Implemented | System temp / Current / Specified、For removable only。新規圧縮のステージング、7zz更新用working directory、一時展開による外部openへ反映。指定先を検証。物理リムーバブル媒体での実機確認は未実施 |
| Options Editor | Implemented | View / Editor / Diffの設定と保存。F3 / F4と通常ファイル2件のDiffへ反映。引数・日本語・空白はshellを介さず渡す。macOS .appのarchive編集はopen -W -aと--argsを使用。終了後は確認付き書き戻し。通常FSの外部起動とは分離 |
| Options Settings | Implemented | 元選択処理、dots／real icons／full row／grid／single click／alternative marksを接続。native iconsはworker。物理入力・外観は未確認。panel-selection-port.md、panel-listing-port.md。 |
| Options unpacking memory | Implemented | 上限の有効／無効とGB指定を保存し、Test / Extractへ上流7zzの-smemxスイッチを渡す |
| Options Windows専用設定 | macOS difference | Windows shell system menuとlarge memory pagesの権限設定は利用不可。理由を表示して無効 |
| Options Language | Partially implemented | 公式Lang2 parser／LangPage情報formatterを移植。公式92言語＋English、翻訳者・割合・不足／追加行、bundleのLang編集、resource IDによる設定／主要dialog／menu表示を実装・検証（language-settings-port.md）。保存済み設定を優先する初回OS言語選択・地域／scriptの対応も実装・検証。Port固有の説明とbackendエラーの一部は英語 |
| Help | Implemented | 公式70ページ、目次／索引・検索／履歴／operator・context・印刷・単一windowを接続。Windows HtmlHelpのOS実装と検索品質はOS差、物理印刷は未確認。dialog-text-help-port.md。 |
| メニューバー配置 | macOS difference | ウィンドウ内にQtメニュー。macOSのアプリメニューも存在 |
| 無効項目の表示・操作条件 | Implemented | 無効なメニュー・ボタン文字はグレー。現在提供するRename / Copy等は選択に応じて有効化し、未実装コマンドは無効。マウスによるメニューOpen / Rename / Properties / About / Optionsを回帰検証。提供機能の差は各メニュー行で分類 |
| ツールバー順・ラベル・画像 | Implemented | Add / Extract / Test / Copy / Move / Delete / Info。公式24×24 BMPを使用し、上流のマゼンタ透明色をマスク |
| Toolbar設定・余白 | Partially implemented | Archive / Standardの独立切替、公式24×24 / 48×36、文字ON/OFFを保存。Fusionと9pt Arial。厳密なピクセル一致は未実装 |
| 通常FSの一覧列 | Implemented | 公式FSFolderの列順・初期表示・幅・alignを移植。Accessed / Change Time / Attributes / Packed Size / iNode / Linksを追加しmacOS statへ接続。Folders / FilesはF3、Commentはdescript.ion。Flat Prefixと追加Type、プロパティID別の設定保存・旧設定移行。OS属性差・物理確認・大量modelの残差は別記（panel-sort-port.md） |
| アーカイブ列 | Implemented | 元handler schema／typed/raw値・両Agent proxy graph／native indexと実番号操作を接続。大量一覧は性能・未確認の欄で区別。native-agent-properties.md、native-agent-item-updates.md。 |
| ディレクトリ移動・親・複数選択・ソート | Implemented | Windows元コードの自然順・typed/raw比較・Name / Prefix / load順tie・親／folder優先・初回方向・Unsorted反転を移植。Viewと列クリックはproperty IDで接続。Qt eventと実7z／ZIPの確認結果はpanel-sort-port.md。Unicode casingの全OS比較と物理キーは未確認 |
| 大量ファイル一覧 | Implemented | workerの列挙・元callback表示・sort／icon worker・世代照合を接続。model挿入／parseは同期でlatency制限あり。元owner-dataはコメントアウト。panel-listing-port.md。 |
| コンテキストメニュー | Implemented | 元File menu／filter／optional VerCtrlとMac Open With共用7-Zip submenuを接続。archive内部のShell省略は元仕様。物理クリックは未確認。panel-menu-drag-port.md、version-menu-time-port.md。 |
| accelerator | Implemented | 元PanelKeyの全dispatch／property table、focused Fキー、modifier優先順位、header／2panel Tab／Ctrl+W／Alt+F1/F2を接続。物理Fn／修飾キーは未確認。address-workflow-port.md。 |
| macOSキーボード | macOS difference | QtのControl/Meta交換を抑止。FキーはOS設定によってFn併用が必要。ハードウェア上の全キーボード設定は未検証 |
| Add画面構成・表記 | Partially implemented | 公式`.rc`の座標・固定client寸法・tab順・OK / Cancel / Help順を取り込み。詳細Optionsも同様。圧縮メモリー・Parameters・自動値／推定・候補／enable／draft／reset・resource翻訳・元Options要約formatterを移植。翻訳／fallback fontの文字寸法に応じたresource単位の調整を追加。物理比較は未確認。compression-controls-port.md / dialog-text-help-port.md |
| Add形式 | Implemented | 7z / ZIP / TAR / WIM / XZ / gzip / bzip2 / Hash。HashはSHA256 / SHA1作成・Test、他の各形式の作成＋Test＋展開を検証。stream形式は通常ファイル1件。RAR作成なし |
| Addの初期値 | Implemented | 元table／numeric builder／自動値／RAM依存threads／draft／reset／history20／形式sortとresource翻訳を接続。compression-controls-port.md、compression-options-port.md。 |
| 辞書・Word・Solid・Threads | Implemented | LZMA、PPMdのmem / order、BZip2、Deflate系の設定を反映。ZIP等のlevel制限と方式別enableを実装。対応値は手入力可 |
| Update mode | Implemented | add / update / fresh / sync。freshで新規fileを追加せず、syncで消えたfileを除去して追加することを検証 |
| Add Path mode | Implemented | Relative / Full / Absolute。macOSの/varや/tmpの親symlinkをcanonical化して7zzへ渡す。Windows drive pathの復元は対象外 |
| 分割アーカイブ | Partially implemented | 新規volume作成・先頭volumeからTestを検証。既存分割archive更新を拒否 |
| SFX / shared | macOS difference | Windows PE SFX、Windows sharing-lock指定を無効。POSIX OpenSharedは通常Openと同じ |
| 詳細Options / Parameters | Implemented | compression memory、プロパティ、compiled handler flagsに基づく日時精度・保存／既定値、TAR GNU/POSIX・ZIP精度条件、archive最新日時の指定pair、アクセス日時保持、TAR / WIMのsymbolic / hard link。動的level／solid翻訳も検証。NTFS security / ADSはWindows専用。compression-options-port.md |
| Delete-after | macOS difference | 確認後、最終出力配置・Test・元データ照合が成功したものをTrashへ移動。失敗時は保持。CRCのない形式では一時展開してSHA-256照合 |
| Encryption | Implemented | 7z AES-256とファイル名暗号化、ZIP AES-256 / ZipCrypto、Show Password、再入力。正誤password、暗号化更新、展開を検証。ZIP初期ZipCryptoは上流と同じ |
| Passwordの受け渡し | Implemented | stdout/stderrと分離した標準入力。読み取りでは `-p` を省略し、圧縮／更新だけ空の `-p` で入力要求する。秘密をargvに含めない |
| Extract画面 | Implemented | 出力先履歴、名前分離、Full / No / Absolute、Overwrite、Password、root duplication除去、選択／全体。非秘密設定を保存。元resource座標・client寸法・ボタン順・名前欄のshow/hideを取り込み、widgetで検証。追加の選択／全体欄はPort固有 |
| Extract overwrite | Implemented | 全5方式。Askは既存／incomingの情報と6択で1件ずつ確認。Auto rename existingは旧fileを_1等へ退避。複合拡張子・dotfileは上流命名に対応 |
| Absolute / file security / linksの展開 | Partially implemented | 明示・確認付きAbsoluteを実装。公式callbackで安全なsymbolic／hard linkを生成し、実output path／native番号でmetadataを配置。folder権限・日時、取得可能な作成日時を保持。危険target／親symlinkは拒否、leaf symlinkは参照先を変更せず置換。既存公開targetへの選択hard linkはTAR／RAR5で検証済み。通常衝突・対象TAR chainは236件検証済み。native事前overwrite／decode Skipと順次配置は接続済み（normal-extraction-integration.md）。root／current-folderと主要file／directory衝突も検証済み（extraction-path-combinations.md）。engine異常終了と終了時の出力保持はextraction-lifecycle.mdを参照。全variant／物理操作の網羅確認は未実施。NTFS securityはWindows専用。extraction-metadata-port.md |
| Test・結果 | Implemented | 選択項目は公式Agentの番号処理を利用。同名ZIPの片方だけ壊れたCRCを区別し、Qt右クリックから検証。元のTest集計とno-errors message、Insideは元HashBundle要約。個別エラーは番号一覧／Close、fatal errorは終了message。内容・対象・7-Zip終了コードを保持。progress-completion-port.md。壊れたarchiveやpassword失敗後の再実行、以前の一覧とアドレスの保持を検証 |
| CRC/SHA結果画面 | Implemented | 元HashGUI formatter・ShowHashResultsとListView／Edit resources／resizeを使用。2列、元property順、選択Copy、表示行Delete、全文の読み取り専用表示、single-click設定と元accelerator。15件のcompletion suiteと選択／暗号化archiveの関連検証。物理操作は未確認。progress-completion-port.md |
| Progress | Implemented | 元表示／制御／success auto-close／Test・Hash結果とerror Closeを接続。物理外観比較は未確認。progress-presentation-port.md、progress-completion-port.md。 |
| Cancel | Implemented | terminate→必要時kill。UIイベントループの生存と、GUIのCancelボタン、後続操作を検証。新規archiveと未配置の展開データはステージングを片づける。配置段階のCancelでは完成済み出力を保持 |
| コピー・移動 | Implemented | 元Copy resources／focused dispatch／copy names／exact Agent input vector、6択・2panel・FS／archive・symlink Moveを接続。host metadata／上流Archive Move制限は別記。copy-projection-port.md、filesystem-transfer.md。 |
| Delete | macOS difference | 通常ファイルは確認後QFile::moveToTrash。archive内部は確認付き削除。7z／ZIP／TAR／WIMは公式Agentのindex選択とDeleteItemsを移植し、同名項目・暗号化solid・Cancelを検証。gzip／bzip2／XZも公式Agentへ接続。gzip／bzip2の削除エラーと元保持、XZの空streamをGUI／console比較で検証。single-stream-updates.md |
| Rename / Create Folder / Create File | Implemented | 元Rename名／native item操作／Agent空folder／FS GetAbsPathを接続。暗号化・prefix・WIM・nested・Flat復元も検証。元archive Create File等のE_NOTIMPLを不足機能と数えない。address-workflow-port.md、native-folder-update.md。 |
| Link | Implemented | hard／file symbolic／directory symbolic作成と既存symlink編集、raw target、両folder browse、選択1件・Flat／2panel既定値とselection復元。28件自動検証。上流のGet_ItemIndices_Operatedに合わせ未選択focusでは無効。NTFS reparse変更のOS差とmetadata／race制限はlink-dialog-spec.md |
| Comment | Partially implemented | focused itemの単行編集、Ctrl+Z、選択／focus保持。FS descript.ionとZIP実項目・削除を実装。ZIPは公式Agent CommentItemを移植し、同名項目／Flatのnative indexで更新。無関係な暗号化データのpassword要求を解消。ZIP64、AES／ZipCrypto、日本語、先頭prefix保持を検証。ZipCryptoのsigned descriptor更新は対応済み。unsigned descriptorはWindowsの元handlerと同じE_NOTIMPLで元archiveを保持。曖昧なFS名、複数volume等は更新拒否。legacy codepageは完全一致未確認。native-agent-comments.md / file-comments-spec.md |
| Properties / Info | Implemented | 元2列modal、native graph／typed/raw formatter・単体／複数／folder／Flat／CRC／archive層・Copy／全文表示を接続。native-agent-properties.md、dialog-text-help-port.md。 |
| archive内Open / Edit | Implemented | 元multi-item Open／extension profile／process探索／UpdateOneFileとnative identity／nested parent書き戻し・共有temp寿命を接続。上流観測限界とPort保護は別記。panel-open-port.md、external-process-port.md、panel-key-port.md。 |
| Finderからドロップ | Implemented | 元drag effect／hover／FS Copy・Move／archive CopyFrom・folder dropを接続。Qt経路は検証、物理Finder／right-dragは未確認。panel-menu-drag-port.md。 |
| archiveからドラッグ展開 | Implemented | 選択の非同期temp展開、Cancel／寿命管理とfile URL dragを接続。32MiB・暗号化・Cancel・受領後保持を検証。物理Finderは未確認。panel-menu-drag-port.md。 |
| Finder Open With操作メニュー | Implemented | FileOpenイベントで別メニューを表示。10項目とiconsをOptionsで保存。圧縮／展開／Test／CRC・形式別Open As、最下段Manager。Qtイベント経路は検証済み、実Finderの物理操作は未確認 |
| Finder Extension / Quick Action / Services | Not implemented | Open Withとは別。Finderのコンテキストメニューそのものへ挿入するextensionは提供しない |
| 書類タイプ関連付け | Implemented | 全138拡張子とdata／folderをViewerで宣言。0.2.2では開発・rollbackコピーをNone、インストールした最新版だけをAlternateにする。実Finderで1件表示と起動を確認。既定設定は変更しない。finder-registration.md |
| タイトルバー・ウィンドウボタン | macOS difference | macOS標準 |
| ファイル権限・署名・sandbox | macOS difference | macOS権限、ad-hoc署名、sandboxなし。Developer ID / notarizationは対象外 |

## 参照した上流領域

形式登録は公式Windows／console build・登録ソースを照合し、61ハンドラー／138拡張子／151組へ統合しました。[全151組の結果](format-coverage.md)、[範囲と制限](archive-formats.md)。読取登録と一覧経路はImplemented、全codec／variant・Windows実行比較はPartially implementedです。Hash callbackの制限・未対応digestをmacOS differenceへ分類しません。

右クリックの7-ZipサブメニューはOpen With設定を共用します。7z／ZIP作成・Test・Extract toをQtイベントで確認し、実AppKit／Finderクリックは未確認です。元の条件・コマンド接続と物理検証は別分類です。

- `CPP/7zip/UI/FileManager/resource.rc`: メニュー・表記。主acceleratorは `PanelKey.cpp` / `App.cpp`、`.rc`のacceleratorだけで判断しない。
- `FileManager/App.cpp` / `App.h`: toolbar順、初期の小ボタン・文字表示。
- `FileManager/FSFolder.cpp`, `PanelItems.cpp`, `PropertyName.rc`, `ViewSettings.h`: FS列・表示順・ラベル。
- `Archive/7z/7zProperties.cpp` / `UI/Agent/Agent.cpp`: archive列とAgent追加プロパティ。
- `UI/GUI/CompressDialog.rc`, `CompressDialog.cpp`, `CompressDialog.h`, `CompressOptionsDialog.rc`: Add構造、方式と初期値、enable条件。
- `UI/Common/ZipRegistry.cpp`: 圧縮履歴と初期設定。
- 設定の全対応表、除外したWindows設定と根拠、公式Lang資産の取得情報は [settings-coverage.md](settings-coverage.md) を参照。
- `UI/FileManager/OptionsDialog.cpp`, `SystemPage.rc`, `MenuPage2.rc`, `FoldersPage2.rc`, `EditPage2.rc`, `SettingsPage2.rc`, `LangPage.rc`: Optionsのページ順、ラベル、コントロール。
- `UI/FileManager/RegistryUtils.cpp`, `PanelItemOpen.cpp`, `UI/Common/WorkDir.cpp`, `WorkDir.h`: 一覧設定の初期値、外部アプリ起動、作業用フォルダーの適用条件。
- `UI/Common/ArchiveCommandLine.cpp`, `UI/Console/ExtractCallbackConsole.cpp`: unpacking memoryの-smemx指定と処理。
- `UI/GUI/ExtractDialog.rc`, `Extract.rc`, `UI/Common/Extract.h`: Extract配置とpath / overwrite初期値。
- `UI/Explorer/ContextMenu.cpp`, `UI/Explorer/ArchiveName.cpp`, `UI/FileManager/MenuPage.cpp`: shellメニューの順序、表示条件、命名、設定ビット。MacのOpen Withへ置き換える。
- `UI/FileManager/ProgressDialog2a.rc`, `ProgressDialog2.cpp`: Progress配置・表記。
- `UI/Console/UserInputUtils.cpp`, `Main.cpp`: password stdinの仕様。通常readコマンドの空 `-p` は空パスワード扱いになる。

## ソース構造と再利用の判断

`C/` と `CPP/7zip/Compress/` は圧縮・展開codec、`Archive/` は7z / ZIP / RAR等のhandler、`Crypto/` はAES等、`UI/Console/` と `Bundles/Alone2/` は公式7zz。`UI/FileManager/`、`UI/GUI/`、`UI/Common/`、`UI/Agent/` はWindows GUI、共有コマンド／設定、FSとarchiveの抽象化を含む。`CPP/Windows/`、Win32コントロール、`.rc`のDIALOG / MENU / STRINGTABLEを参照実装として読む。

Win32 UIの直接移植やC++ COM風内部APIへのリンクは採用しない。上流のOS非依存handler / codec / crypto / command生成と更新処理は公式console内で再利用する。最初の縦方向の機能を実装でき、独立プロセスの失敗をGUIから切り離せるため7zz方式を選んだ。ネイティブbackendへの切替用にArchiveBackendインターフェイスを維持する。

## 全機能の最終判定（2026-10-04）

**Windows版の全機能は実装されていない。** 現行上流の有効な静的メニュー65コマンド、動的メニュー、PanelKey.cppのキーボード、設定・ダイアログを再照合した。全項目の分類と優先順位は[final-audit.md](final-audit.md)。未実装をmacOS differenceとして隠さない。

## Archive refresh checkpoint (2026-10-05)

公式Agent更新結果からnative directory／rowを再接続し、implicit／explicit folderのancestor Rename、通常／Flat・二画面のaddress／選択／focus、Delete後の親復帰を検証。外部editorとnested書き戻しを含む最終26件が成功。全GUI同等とは判定しない。[実装・対象検証・残範囲](archive-refresh-port.md)。

## Normal extraction failures (2026-10-05)

公式callbackのCloseFile／SetOperationResult／SetPostLinks／CloseArcを使用し、通常の失敗終了でも記録済み出力を保持する。7z／ZIPのCRC不一致、ZIPの非対応方式、7z／ZIP AES／ZipCryptoの誤passwordと再実行、配置失敗後のfolder metadataを独立した未改変7zzへ照合した。Qt Progressの実widgetでcode・診断・Close操作・backend再使用も検証。全320件成功（setup／cleanup含む）。[範囲・残差](partial-extraction-port.md)。展開前のoverwrite／decode Skipと主要conflictは後続のnormal-extraction-integration.md／extraction-path-combinations.mdで実装・検証済み。異常終了／終了時の保持はextraction-lifecycle.mdを参照。未確認variantを未実装コマンドとして数えない。

## File context / drag source port (2026-10-05)

公式CFileMenu通常loop・drag effect・right-drag tableを取り込み、CRC／Diff欠落と作成重複を修正。パネル間Copy／Move・folder row全列・icon表示・同panel拒否、archive追加確認、32 MiB／暗号化drag-outと一時file寿命を接続した。対象 **116件成功**。この時点のoptional VerCtrl未移植は、後続のversion-menu-timeバッチで解消した。実Finder・native右button開始はロック中のため未確認。[範囲と証拠](panel-menu-drag-port.md)。

## Version commands / Time source batch (2026-10-05)

Optional Ver Edit／Commit／Revert／DiffとCFileMenu末尾、通常Fileメニューの制御、動的な日時sampleと元formatterを移植。初期値はWindows同様の分表示、UTCはZ付き。関連112件成功、別volume既存case1件skip。物理操作と全体最終gateは未完了。[仕様・確認結果](version-menu-time-port.md)。

## Command-entry workflow batch (2026-10-06)

共通Combo入力、元wildcard matcher、一覧内Rename、履歴の一覧／削除／Cancelをまとめて移植。通常FSの非同期・排他的Renameと複数階層のfolder作成、元Create File初期値も合わせた。Windowsの元実装と照合した項目・未移植項目は [command-entry-port.md](command-entry-port.md) に区別して記録。全機能完了や全形式の最終受入とは判定しない。関連機能群の一括検証後、失敗／影響箇所だけ修正・再検証する。

対象9 suite選択の最新結果は146成功・0失敗・1skip（別volume指定なし）。初回失敗・途中timeoutも生ログに保持し、修正後は失敗／影響する選択だけ再実行した。FS worker完了のbusy競合、`/var`基準path、Qtセル選択の項目statusへの変換を修正済み。詳細は [command-entry-port.md](command-entry-port.md)。

## Copy / Move / Combine batch (2026-10-06)

Original IDD_COPY 96, CCopyDialog::OnSize and App.cpp summary bodies are imported.
The shared editable destination history, Shift+F5/F6 focused-row behavior,
current/Flat archive Copy, archive-to-archive Copy through temporary extraction,
both panel-state restoration and Combine's shared Copy dialog are implemented.
The complete group was implemented before grouped testing; latest affected
outcomes are 87 pass / 0 fail / 1 explicit cross-volume skip. The normal
filesystem installer and native Agent selection/extraction/CopyFrom backend are
reused. See [source mapping and evidence](copy-workflow-port.md).

Archive Move is source-defined E_NOTIMPL, not a missing portable Windows feature.
The macOS OS folder browser replaces the normal Windows Shell folder browser.
Duplicate archive rows are rejected before staging rather than after Windows'
temporary extraction; this interaction is Partially implemented. Full metadata,
special handler layouts and physical Finder/Fn verification retain their separate
status. Complete parity, final clean/full-format validation and publication are
not implied by this batch.

## ZIP metadata group (2026-10-06)

Comment／Renameの元Agent bodyを保持し、公式ZipUpdateの既存ZipCrypto
descriptor/DOS check timeを保持する限定overlayを接続。両helperへ同じ
修正を適用し、ATime／CTimeは元ISetPropertiesで保持する。既存file更新と
password再入力のGUI経路も合わせて整備した。関連する最新の検証は
145成功・0失敗・0skip。更新不可のunsigned ZIPは、元7zzでもE_NOTIMPLに
なることとTest／展開成功を照合した。単純な未実装やmacOS differenceとは
分類しない。[実装範囲・初回失敗・最終記録](zip-metadata-port.md)。

全形式・新規空ディレクトリbuild・物理操作・公開の最終gateは別途残る。

## Focused keys / relative Rename / two-panel navigation (2026-10-06)

Original PanelKey F2–F7 and Alt path/arrow blocks, final-component Rename
validation and non-Windows CorrectFsPath are imported. F2 follows focus even
without marks; Shift+F4 and opposite-panel folder/address navigation are
connected. Relative filesystem destinations and safe relative archive names
reach the guarded installer/original Agent updater. F3 folder statistics use
operated selections, matching original EditItem. Physical Fn/Option input and
final release gates remain unverified. Scope and grouped evidence:
[panel-key-port.md](panel-key-port.md).


## Address / creation / complete list-key source batch (2026-10-06)

The full official PanelKey.cpp dispatcher and FSFolder::GetAbsPath are imported
with pinned hashes. Editable address dropdown ancestors/standard locations,
selection, Enter/Escape/Tab, two-panel Tab and address Alt+F1/F2/F9/Ctrl+W are
connected as one related batch. FS creation accepts explicit absolute and
parent-relative paths, retaining no-follow / exclusive output safeguards. Root,
Documents and mounted macOS volumes substitute Win32 shell locations; absent
Network namespace is an OS difference. Physical input and final release gates
remain unverified. Implementation scope and executed results:
[address-workflow-port.md](address-workflow-port.md).

## Copy output-name / CopyFrom sequence batch (2026-10-06)

Original GetItemName_for_Copy is imported at the native row boundary. Temporary
archive Copy preserves all selected names and the original overwrite interaction
before CopyFrom. The original AgentOut EnumerateItems2 receives a private exact
input vector, retaining duplicates and exact filesystem Flat physical inputs.
See [scope and executed validation](copy-projection-port.md); this supersedes the
earlier early-duplicate guard, without declaring complete parity or release.
