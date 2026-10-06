# Windows版の設定項目との対応

Current implementation and release inventory: [current-status.md](current-status.md).
Historical checkpoint remaining-work lists are superseded by the later connected source groups; implementation gaps, defects and unverified physical input are tracked separately.

公式7-Zip 26.03のOptions各ページ、CompressDialog / CompressOptionsDialog、ExtractDialog、resource.rc / MyLoadMenu.cpp / RegistryUtils.cpp / ZipRegistry.cpp / UpdateGUI.cppを基準にした。Windows専用設定は無効表示し、理由を付けている。未実装の操作コマンドをWindows専用として分類しない。

| 設定 | 対応 | 反映先・制約 |
|---|---|---|
| Options: Show dots / real icons / full row / grid / single click / alternative selection | Implemented | 一覧と保存。alternative selectionは公式PanelSelect／PanelItemsを移植。mark／focus分離、Insert／Ctrl・Shift click／Shift arrow、2panelと設定切替を自動検証（panel-selection-port.md） |
| Options: unpacking memory | Implemented | Test / Extractの-smemx、GB指定 |
| Options: working folder / removable only | Implemented | 新規archive staging / 更新の-w / CreateFolder・置換・Comment staging / 外部open用一時展開。SSD／SMB間の原子的installationも検証。媒体判定はDiskArbitration |
| Options: View / Editor / Diff | Implemented | 外部アプリ・引数、F3 / F4 / 2ファイルDiff。秘密を設定に保存しない |
| Options: Open With items / icons | Implemented | 10項目（独立Open archive >を含む）の表示とアイコンを保存。FinderのFileOpenイベントで呼び出す。Manager項目は常に表示 |
| Options: eliminate root duplication | Implemented | 出力先basenameと全archive entryのroot一致を確認し、full path展開の重複を除去 |
| Options: Language | Partially implemented | English＋公式92言語、Apply時にラベルを置換。Port固有説明、動的結果、backendエラーの一部は英語 |
| View: Large / Small Icons / List / Details | Implemented | 各panelの表示モードを保存 |
| View: Name / Type / Date / Size / Unsorted | Implemented | FS / archive別にソート状態を保存 |
| View: Flat / 2 Panels | Implemented | 各panelのFlatとpath、2 Panels、splitter幅を保存。キーボード操作はフォーカスしたpanelへ配送 |
| View: folders history / auto refresh | Implemented | 履歴32件、ファイル／フォルダー監視。処理中は更新を抑止 |
| View: time / UTC | Implemented | 日／分／秒／100ns／1ns。FSはnative statのnanoseconds、archiveは取得できた精度まで |
| View: Archive / Standard toolbar / large / text | Implemented | 独立切替、公式24×24 / 48×36画像、表示設定を保存 |
| View: columns / width / order / window | Implemented | ヘッダー右クリックで列切替、幅・順番・geometryを保存。取得しないmetadata列の値は空欄 |
| Add: Archive / format / history | Implemented | 7z / ZIP / TAR / WIM / XZ / gzip / bzip2 / Hash、上流同様の履歴20件、最後の形式を保存。stream形式は通常ファイル1件 |
| Add: level / method / dictionary / word-or-order / solid / threads | Implemented | 公式format表・辞書／order本体を移植。候補数でenable、CPU×2の上限、形式切替のdraft保存、level変更時の自動復帰。手入力は既存Port拡張。compression-controls-port.md |
| Add: compression memory | Implemented | -mmemuse、割合／容量、Automaticは上流既定80%。公式GUI計算を使う圧縮／展開メモリー推定、RAM依存の自動threadsと`*`値表示を追加。compression-help-port.mdに残差を記載 |
| Add: update / path / volumes | Implemented | add / update / fresh / sync、Relative / Full / Absolute、新規volume。既存volume更新は拒否 |
| Add: Parameters | Implemented | compression propertyを-mへ変換。shell不使用、CLI switch / passwordを拒否 |
| Add: encryption / show password / encrypt names | Implemented | 7z AES、ZIP ZipCrypto / AES。設定値は保存するがpasswordは保存しない |
| Add: timestamps / latest archive time / preserve access time | Implemented | -mtp / -mtm / -mtc / -mta / -stl。POSIXで上流-sspが機能しないためmacOS utimensatでアクセス日時を保持 |
| Add: symbolic / hard links | Implemented | 上流GUIと同じTAR / WIMで設定可。-snl / -snh。7zのPOSIX link保存は従来の安全な既定を維持 |
| Add: delete after compression | macOS difference | 最終出力配置後にTestとデータ照合、確認付きでTrash。CRCのない形式は一時展開してSHA-256照合。検証失敗時は元を保持 |
| Extract: output history / named subfolder | Implemented | 履歴32件、name有効／無効、既定ON。名前のパスを検証 |
| Extract: Full / No / Absolute / eliminate root | Implemented | AbsoluteはOK時に確認、7zzへ-spfを渡して直接書き込ませず、stagingから検証して配置 |
| Extract: Ask / Overwrite / Skip / Auto rename / Auto rename existing | Implemented | 全5方式、existing renameは旧データを_1等へ退避。Askは既存／incoming情報付き6択で1件ずつ確認 |
| Extract: password / show password / selected-only | Implemented | passwordはstdin、保存しない。Show Passwordと非秘密の設定を保存 |

圧縮設定は形式別に保存する。Delete-afterは誤操作を避けるため毎回OFFから始める。各ダイアログのCancelは変更を保存しない。Optionsは最後のApply以降の変更だけを破棄する。コントロールが形式に非対応の場合は上流と同様に無効になる。

## Windows専用として除いた設定

- Windows registryのファイル関連付け: macOSはInfo.plistで宣言し、Finderの「情報を見る → このアプリケーションで開く」を案内。既定を強制変更しない。
- Explorer shell extensionの登録、32bit版、cascaded menu、Zone.Identifier: Windows Explorer用。shell command一覧・アイコン表示はOpen With操作メニューへ置換。Open Asの形式／parser指定は共通操作メニューへ実装。上流はIsArcFolder時にShellメニューを生成しない。archive内部はFileメニューのInside／*／#に対応する。
- Show system menu: Windows shell context menu。macOS Finder menuの移植は行わない。
- Large memory pages: 上流のWindows lock-memory privilege設定。権限やOS設定を変更しない。
- NTFS filesystemへのalternate streams / file security、Extract security適用: Windows metadata。archive内stream閲覧はportableとして実装済み。macOS ACL / extended attributesの完全保存は別の未対応項目。
- Windows SFX executable: PE実行ファイル。macOS用の独自SFXを実装したことにはしない。
- Compress shared files: Windows sharing-lock指定。POSIXの上流OpenSharedは通常Openと同じ処理。

## 資産とライセンス

翻訳は[公式26.03リリース](https://github.com/ip7z/7zip/releases/tag/26.03)の`7z2603-x64.exe`に含まれるLangのみを抽出した。取得物のSHA-256は`0859c524b8a63551848f0c246abddcb1d0b7b656b0fbfe879f8d85e61a9e6edd`。Windows実行ファイルやMicrosoftのフォント・システム画像は同梱しない。

Langの著作権ヘッダーと改行を変更せず保持し、配布物の[License.txt](../licenses/7-Zip-Windows-assets.txt)も同梱する。翻訳資産はその「other files」のLGPL条件に従う。toolbar画像は公式ソースのLGPL資産。zlibはmacOSのシステムライブラリを利用し、再配布しない。

## 現行の残差と未確認事項

portable設定の主経路・Apply／Cancel・保存と各操作への接続は実装済みです。native item番号、icon worker、directory link元のarchive Move、Progress完了表示、address／Flat復元は後続の機能群で接続しました。古いコンポーネントの残作業を現在の未実装と扱わないでください。

Port独自文言・OS／process診断の全翻訳は未完了です。物理リムーバブル媒体、native外観・入力、最終統合確認は実装済みとは別に未確認です。model挿入／metadata parseの同期区間は性能上の制限です。具体的な実装残差・追加の安全制限・OS差・最終gateは[current-status.md](current-status.md)にまとめています。

## Official progress and Help checkpoint

Native engine callback counts/bytes/ratio and Help Search/Print/About are
implemented and tested. Verification-worker Pause/telemetry is now implemented in the
[transfer checkpoint](filesystem-transfer.md). Search history and operator controls
were subsequently implemented and tested in [benchmark-port.md](benchmark-port.md).
Printing was checked through owned PDFs,
not a physical printer. [Scope and evidence](native-progress-help.md).

Filesystem Copy/Move into the other open archive panel now uses upstream
CopyFrom defaults and logical prefixes, independent of AddDialog settings.
Internal-folder drops and multiple physical parents are connected. Directory
symlink source deletion, full metadata and special archive layouts remain
partial. [Source behavior and executed checks](archive-transfer.md).

Compression controls now reuse official format/method tables and numeric combo
builders, with format drafts, upstream resets, count-based enablement and bounded
thread choices. [Source comparison and scoped tests](compression-controls-port.md).
