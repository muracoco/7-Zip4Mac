## Official multiple-item Open (2026-10-05)

公式OpenSelectedItemsを移植し、複数選択・20項目・parent条件・folder停止・単一時だけの内部openをQt command queueへ接続。7z／ZIPと同名ZIPの実LaunchServices起動・個別展開・2項目の確認付き書き戻し・byte比較、独立mark／focusのnested openを検証。新規Open7、選択16、editor36、open modes24の **83件成功、失敗／skipなし**（setup／cleanup含む）。system temp内で登録されなかった検証用receiverを、専用のuser Applications一時folderへ移して再確認した。既存形式の関連付けは変更せず、所有するランダム拡張子のhandlerだけを登録・解除。[範囲と残差](panel-open-port.md)、[初期失敗を含むログ](distribution.md)。不明拡張子の既定FS Open・物理操作・最終clean build・全機能同等・公開は未完了。

## Official external-process discovery (2026-10-05)

公式CChildProcesses::Updateを移植し、短時間・未変更の起動processから同名の常駐processへの引き渡しをlibprocへ接続。独立serverへのIPC後の待機、実ZIP Edit・1回の確認・公式UpdateOneFile・展開byte比較、changed／long launcherの除外と従来の7形式・暗号化・nested・2panel・Cancel・close／descendantsを含む **editor36件成功、失敗／skipなし**（setup／cleanup含む）。初期ビルドのMach FALSEマクロ衝突は修正済み。[範囲](external-process-port.md)、[実行ログ](distribution.md)。デスクトップは再確認してlocked。物理操作・multi-item Open・最終clean build・全機能同等・公開は未完了。

## Official File Manager selection (2026-10-05)

公式PanelSelect／PanelItemsの選択bodyを移植し、alternative selectionのQt MultiSelection近似を置換。独立mark／focus、Insert／Ctrl・Shift click／Shift arrow、sort／refresh／2panel／Options Apply、Ctrl+Cを検証。実7z／ZIPにマークした日本語／ASCIIだけを圧縮・展開してbyte比較。最終の影響範囲 **222件成功、失敗／skipなし**（setup／cleanup含む）。最初のclipboard実装差は修正済み。2panel fixtureの誤ったpane検索を直してselection16件を再実行し、他8suite206件の成功は再実行せず保持した。[範囲](panel-selection-port.md)、[初期失敗を含むログ](distribution.md)。梱包後の15 Mach-O依存・ad-hoc署名検査と、開発Qt／DYLD環境変数を外した独立設定での3秒起動も成功し、所有processだけ停止した。物理操作・最終clean build・全機能同等・公開は未完了。

## Compression Options and dynamic labels (2026-10-05)

公式のlevel/resource IDとcompiled handler capabilitiesを使い、日時精度・既定値・TAR GNU/POSIX／ZIP条件・最新archive日時の指定pair・動的翻訳を修正。compression46・progress15・native formatter11、計 **72件成功、失敗／skipなし**（setup／cleanup含む）。CLIが`-stl-`を拒否する初期失敗は、false時にスイッチを省略することで修正し、保存・実ファイルのbytes／日時まで再確認。[範囲](compression-options-port.md)、[ログ](distribution.md)。最終clean build・物理操作・全機能同等・公開は未完了。

## Temporary Properties source port (2026-10-05)

公式のPrintFileProps／PrintProps／AddSizeValueを移植し、一覧キャッシュからの表示を非同期の再集計へ変更。属性・変更後の集計・日本語・上限・missing項目・終了中のreader破棄を含む **temporary16件成功**（setup／cleanup含む）。native formatter11・progress15も成功。macOSでQMessageBoxのタイトルが消える失敗を修正し、上流の単一OKメッセージをWidgets dialogで表示する。タイトル修正後は影響しない成功済みsuiteを再実行していない。[移植範囲](temporary-properties-port.md)、[ログ](distribution.md)。物理操作・最終clean build・公開は未完了。

## Extraction path and conflict combinations (2026-10-05)

ルート除去＋既存hard link参照の対応を修正。新規20ケースを含むmetadata310、progress15、選択9、基本path／roundtrip5の **339件成功、失敗／skipなし**（setup／cleanup含む）。file／directory衝突16ケースを未改変公式consoleの終了コード・構造・bytesへ照合。Cancelの旧rollback前提を公式の途中出力保持へ修正し、独立したconsole SIGTERMとGUI heartbeat・再使用を確認した。[実装・残範囲](extraction-path-combinations.md)、[ログ](distribution.md)。新規梱包・物理操作・最終版の完了は主張しない。

## Normal asynchronous extraction integration (2026-10-05)

通常backendに公式callbackの事前overwrite／順次配置を接続。対象metadata290・progress15、共有transfer30・editor32・Open modes24の **391件成功、失敗／skipなし**（setup／cleanup含む）。大文字小文字の違う退避名の作成日時、Cancel表示、未配置の以前の出力保持とfixtureのcanonical path比較を修正して再確認した。全機能同等・物理クリック・最終clean build・公開の完了を意味しない。[実装と残範囲](normal-extraction-integration.md)、[ログ](distribution.md)。

## Native pre-stream overwrite channel checkpoint (2026-10-05)

公式CheckExistFileからQtへAskを送る通信componentを追加。6択・連続回答を未改変consoleに照合し、切断・遅延・重複・旧processの回答拒否・CRC破損ZIPのdecode Skipを実ファイルで確認。metadata263／progress15、計278件成功（setup／cleanup含む）。[移植範囲](native-overwrite-channel.md)、[実行ログ](distribution.md)。これは単体のnative接続検証であり、通常アプリの展開前overwrite／Skipは未完成。安全な公開出力の状態照会と、公式callback順での配置が次の接続対象。

## Partial extraction failure checkpoint (2026-10-05)

通常失敗時の出力保持、配置失敗後のfolder metadata復元、Qt Progressのエラー表示／Close／再実行を追加。17の新規scoped casesを含む **320 checks passed, 0 failures/skips**（setup／cleanup含む）：metadata252、progress14、transfer30、Open modes24。7z／ZIP CRC、ZIP非対応方式、3暗号形式の誤password／正password再実行を未改変公式7zzに照合。既存出力のoverwrite／auto-rename-existingと日本語／空folderも比較。[source・残差](partial-extraction-port.md)、[実行ログ](distribution.md)。engine Cancel／crash出力、事前overwrite／decode Skipと残りのlink／conflict組合せは未完了。全format matrix・最終clean build・物理クリックの完了を意味しない。

# 実行済み検証記録

## Ordered extraction checkpoint (2026-10-05)

公式callbackの完了ごとの内容をprivate clone／copyで保持し、同名・No pathnames・大小文字・濁点を含む日本語名を実際の処理順で配置した。公式エンジンのa／rnで生成した同名7z、ZIP／TAR、native／通常選択、既存出力・自動名の衝突、4方式、6択Ask・混在回答とCancel／再利用、TARのdeferred link chain、従来metadata／TAR／RAR5選択linkを含む236件が成功。実catalog名でcaseを復元し、Rename existingのbackupへ移った生成物の作成日時も保持した。関連4suite99件、選択／祖先／nested／Pause／Cancel18件、共有変更による1回の全151登録／155件も成功（setup／cleanup含む）。作成日時はpristine consoleの独立したarchive property表示を基準にし、macOS consoleのMTimeによるbirth time低下とWindowsの独立したCTimeを区別した。初期の接続・検証の失敗は修正し、最終成功だけを計上した。

[移植範囲と未完了項目](ordered-extraction-port.md)、[最終実行ログ](distribution.md)。native事前overwrite／decode Skip、file／directory衝突、部分失敗の出力／metadata、間欠的SMB変更検出、物理操作・最終リリースは未完了。

通常.appのビルド・梱包と15 Mach-Oの依存／ad-hoc署名検査、独立設定・開発Qt環境変数なしの3秒起動、同梱7zz 26.03とhelper code領域の一致を確認した。所有processだけ停止。最終empty build／物理操作の確認ではない。

## Selected hard-link checkpoint (2026-10-05)

公式CreateHardLink2／SetLink2を再利用し、選択したhard linkの参照先が既存の出力folderにだけある場合を接続した。公開inodeをengine準備中に変更せず、参照先の変更・symlinkを検査して配置する。TARと実RAR5 fixtureを含むmetadata33件、影響するtransfer30件・progress13件・editor32件・Open modes24件、選択／祖先Rename／nested／Pause／Cancel18件が成功（setup／cleanup含む）。共有展開処理の変更に伴い全151登録を一度再確認し、format155件・Qt右クリックの7z／ZIP圧縮・Test・展開も成功。初期TAR fixtureはpayloadを選択していたため失敗し、実Hard Link属性を確認するfixtureへ修正した。完全な絶対参照・root除去の組合せ、衝突順序・部分失敗のmetadata・間欠的SMB変更検出、物理操作・最終リリースは未確認／未完了。

[移植範囲](extraction-metadata-port.md)、[最終実行ログ](distribution.md)。

通常の.appへ反映し、15 Mach-Oの依存／ad-hoc署名検査と、独立設定・開発Qt環境変数なしの3秒起動、同梱7zz 26.03の起動を確認。署名前後のbinary全体比較は不適切だったため、実行code領域を比較して一致を確認した。停止は所有test processだけ。物理menu／Finderと最終clean buildの確認ではない。

## Archive refresh checkpoint (2026-10-05)

公式Agent更新結果をQtのdirectory／row identityへ接続し、implicit ZIPとexplicit 7zの通常／Flat・二画面でancestor Rename後のaddress／選択／focusとDelete後の親復帰を確認した。実際の外部editor終了後の書き戻し、ancestor Rename後のnested更新・親復帰も展開したbytesまで比較。既存の同名項目、TAR／WIM、先頭prefix、single-streamの対象回帰を含め、最終26件が成功（setup／cleanup含む）。初期session検証ではテストが別panelのaddress widgetを再取得して3件失敗し、panelごとのwidget保持を修正した。native物理操作・最終クリーンbuild・全機能同等は未確認／未完了。

[移植範囲](archive-refresh-port.md)、[最終実行ログ](distribution.md)。全形式と無関係なsuiteは再実行していない。

## Temporary-file browser checkpoint (2026-10-05)

Tools → Delete Temporary Filesを有効化し、上流の件数制限付き集計／サイズ表示を移植。専用fixtureで、一覧・folder移動・設定・右クリック・日本語・symlink拒否・変更／使用中保護・default No／Cancel・実Trash移動と内容保持・permission error・読込中の終了と再利用を確認した。新規12件、影響する進捗13件・通常一覧13件・Properties19件の計57件が成功（setup／cleanup含む）。初期検証では読込中のdialog破棄が破棄済み一覧へcallbackを返してクラッシュしたが、終了時の接続／プロセス寿命を修正し、最終再実行は成功。Propertiesの再集計／全属性・native操作／寸法・最終リリースgateは未了。

[移植範囲](temporary-files-port.md)、[実行ログ](distribution.md)。

## Single-stream / Rename session checkpoint (2026-10-05)

gzip／bzip2／XZのRename／Deleteを公式Agentへ接続し、Qt GUIとbackendの各操作を公式consoleの出力bytesと比較。名前を保存しない形式、gzip／bzip2削除時の上流エラー・元保持、XZの空streamを確認した。選択suite24件、既存editor32件、Open modes24件の80件が成功（setup／cleanup含む）。編集中Rename、2panelで親側の子archiveをRenameした後のnested書き戻し、選択・focus、暗号化、Pause／Cancelを検証。初期の新規検証は失われたfocusをnull確認せず参照してtest実行ファイルが一度終了した。選択保持と検証側のnull確認を修正し、最終実行は成功。物理操作・全機能一致・最終クリーンビルド・公開は未完了。

[仕様と残課題](single-stream-updates.md)、[最終ログ](distribution.md)。

## Official extraction callback checkpoint (2026-10-05)

公式ArchiveExtractCallbackを外部overlayで再利用し、実output path／native item番号とmetadataを配置へ接続。属性・安全なlinkの公式console比較13件、選択展開9件、progress13件、editor32件、Open modes24件、transfer30件、Link28件、全151登録／155件のformat検証が成功（setup／cleanup含む）。overwriteは19件成功・別volume未指定で1件Skip。実SMBでの追加確認は1回変更検出エラーとなり、診断表示を分けた後の対象3件は成功した。間欠条件は未解決として保持する。全機能一致・物理操作・最終クリーンビルド・公開は未完了。

[移植範囲と残課題](extraction-metadata-port.md)、[成功と失敗を含む実行ログ](distribution.md)。

## Official Agent UpdateOneFile checkpoint (2026-10-05)

公式AgentOutのUpdateOneFileを移植し、editor／nested書き戻しをnative item番号へ接続。新規・影響する選択suite14件、editor32件、comments31件、Open modes24件の計101件が成功（setup／cleanup含む）。同名ZIPの片方だけのGUI編集、未選択payload保持、single-streamを含む7形式、暗号化、先頭prefix、nested、2panel、Cancelと回復copyを検証した。bzip2の未定義Sizeと、TAR／WIMの正常なoffset通知の誤判定を修正。置換内容のサイズ／SHA-256をnative番号で確認し、無関係なwhole-archive Testは除去した。最終クリーンビルド・物理操作・全機能一致・公開は未完了。

[移植範囲とコマンド](native-agent-replacement.md)、[最終実行ログ](distribution.md)。

## Official Agent Delete / Rename checkpoint (2026-10-05)

公式AgentOutのDeleteItems／RenameItemを移植し、7z／ZIP／TAR／WIMのGUI更新をnative item indexへ接続。拡張した選択suite32件、既存Open mode24件、Properties19件、folder更新14件、tree4件が成功（setup／cleanup含む93件）。同名ZIPの片方だけの更新とbyte一致、破損した非選択payloadの保持、通常／Flat folder、複数image WIM、先頭prefix、日本語、暗号化solidの再圧縮・誤password・Cancel後の元SHA-256一致・backend再利用を確認。Qt GUIのRenameと確認付きDeleteも検証。TARの公式既定Renameで変わるPAX属性／日時精度は文書化した。全機能同等・最終クリーンビルド・物理操作・公開は未完了。

[移植範囲とコマンド](native-agent-item-updates.md)、[実行ログ](distribution.md)。

## Official Agent selection checkpoint (2026-10-05)

公式GetRealIndex／GetRealIndicesのbodyを移植し、選択展開・Test・archiveハッシュとfocused項目の一時展開へ接続。新規12件、既存tree 4、Open mode／先頭prefix／nested 24、editor書き戻し32、Properties 19、native進捗往復4、複数image WIM 3が成功（setup／cleanupを含む）。同名ZIPの片方だけのbyte一致／SHA-256、片方だけ壊れたCRCのTest、NTFS stream、日本語・暗号化・選択path安全性・64MiBのPause／Cancelとbackend再利用を検証。Qt右クリックメニューの実mouseイベント経路を含む。更新候補のsnapshotをインストール先へ引き継ぐ不具合を既存prefix回帰で検出・修正し、再検証した。物理デスクトップ操作・最終クリーンビルド・全機能一致・公開は未完了。

[移植範囲とコマンド](native-agent-selection.md)、[実行ログ](distribution.md)。

## Official Agent directory / Properties checkpoint (2026-10-04)

Official Agent property bodies and both proxy directory graphs now feed native
rows, implicit folders, Flat ordering, item-index Properties and archive stream
navigation. The final affected 216 checks pass (Properties 19, native formatter
11, open modes 24, tree 4, multi-image WIM 3, format matrix 155 / all 151 registrations), including
setup/cleanup. Genuine duplicate ZIP payload sizes and unmounted NTFS named
streams are tested; the stream command accepts a Qt context-menu mouse click.
Full native-index operations, complete metadata/UI, final release checks and
physical desktop verification remain pending. The normal app passes 15-Mach-O
dependency/signature checks and an isolated LaunchServices startup; lsof
confirms its bundled patched Cocoa plugin. Only the owned instance was stopped.

[Source and limits](native-agent-properties.md),
[execution logs](distribution.md).

## Official prefix-update checkpoint (2026-10-04)

Imported the original Agent `CommonUpdateOperation` stream block, preserving
`ArcStreamOffset` separately from each handler's own `Offset`. Console Add /
CopyFrom / update / rename / delete already reuse the same official boundary.
The panel's blanket leading-offset restriction is removed. Staged folder,
replacement and comment updates verify the prefix position and bytes before
replacement. Tail/multiple-layer refusal matches Windows Agent itself.

- Final affected checks: **114 passed / 0 failed / 0 skipped**, including
  setup/cleanup: open modes **24**, official-folder integration **14**,
  external edit **32**, comments **31** and native progress **13**.
- Genuine original-engine 7z, ZIP, TAR and WIM plus inert leading data pass
  CreateFolder, ReplaceFile, internal CopyFrom, Rename, Delete, Test and
  extraction/content comparisons. Prefix bytes remain identical after each
  operation. Rows include 7z data/header encryption and ZIP AES; ZIP comments
  also retain the prefix and payload.
- Qt GUI actions are enabled for supported prefixed parents. Both 7z and ZIP
  nested modification / Yes confirmation / parent write-back / independent
  extraction / reopening preserve the new directory and original prefix.
- Tail and Split-chain updates are refused; original bytes remain unchanged.
  Original no-prefix folder cancellation/concurrent-change and mixed-password
  tests also pass. Physical Finder/menu/Fn behavior remains unverified.
- Final focused tests ran after the helper and Qt targets built successfully.
  The full format matrix and final-release clean build were not repeated for
  this scoped change. Full portable parity/publication remain incomplete.
- Packaging/dependency checks passed for 15 Mach-O files. The normal bundle
  launched through LaunchServices with isolated preferences; owned PID 39566
  survived three seconds and loaded its bundled patched Cocoa plugin. Only
  that instance was terminated. Physical desktop checks remain pending.

[Source reuse, commands and limits](archive-prefix-updates.md),
[execution transcript](distribution.md).

## Official Agent folder/update checkpoint (2026-10-04)

Imported the original `CAgent::CreateFolder` body and retained its NoChange
pairs and official archive update callback. Native handler metadata now records
auxiliary flags and raw parent identity; generated WIM XML is identified through
that parent interface. Multi-image WIM update guards, unnecessary CreateFolder
data-password prompts and ignored working-folder preferences are resolved.
Directory-link archive Move verifies followed descendants and trashes only the
selected link/root, retaining linked targets.

- Folder/update integration: **18 passed / 0 failed / 0 skipped**, including
  setup/cleanup. Cases cover four writable formats, data/header encryption,
  mixed 7z/ZIP data passwords, genuine two-image WIM folder/Copy/Replace/Rename/
  Delete and GUI actions, original payload SHA-256, 100 ns native time precision,
  12,000-item TAR, pause/cancel and injected concurrent destination changes.
- Working-folder verification uses actual **SSD archive / SMB work storage**.
  CreateFolder, replacement and ZIP Comment install successfully, compare
  extracted data, clean staging, and retain originals on an invalid work path.
  The real Options preference also reaches the GUI CreateFolder route.
- Final affected CTest suites: **6/6 passed**, consisting of archive transfer
  **30**, native progress **13**, editor write-back **32**, ZIP/FS comments **31**,
  Properties **19**, and native metadata formatting **11** passing checks.
  Transfer checks include four directory-link formats, unchanged external
  targets, two-panel refresh, and a real source-change verification failure.
- The **154** total includes Qt setup/cleanup and native formatter checks;
  it is not 154 distinct commands. No physical Finder/native-menu/Fn verification
  is claimed. Full clean/full-format release verification remains scheduled
  after the remaining portable functionality.

The Release helper and application builds succeed. Handler/codec algorithms
remain official code; the unrelated full 151-registration matrix was not rerun.
[Imported source, behavior and limits](native-folder-update.md),
[final execution logs](distribution.md).

The normal `~/.cache/7zip-mac-port/build/7-Zip Mac.app` is updated. Packaging
and signature verification pass for **15 Mach-O** files with system/bundle
runtime paths. An owned LaunchServices instance **PID 34731**, using an isolated
CFFIXED_USER_HOME and disposable folder, survived three seconds and loaded the
bundled Cocoa plugin verified by vmmap. Only that owned PID was terminated;
physical rendering remains unverified.

## Official compression controls checkpoint (2026-10-04)

Imported the original format/method tables and numeric combo builders, keeping
upstream arithmetic and nearest-choice behavior. Qt now implements format
in-memory drafts, OK/Cancel persistence, level/dictionary resets, method matching,
count-based enablement, hardware-bounded thread choices, XZ solid and memory
choices, last-format/history behavior and stable translated solid commands.

- Final compression/Help suite: **43 passed / 0 failed / 0 skipped**, CTest **1/1**.
  Nineteen GUI-selected method data rows cover 7z/ZIP normal methods and Store,
  TAR GNU/POSIX, WIM, XZ, gzip, bzip2 and SHA256/SHA1 Hash. Archives are created,
  tested, extracted and SHA-256 compared using Japanese/space names. Hash output
  is tested and its digest compared, rather than treated as an extractable archive.
  Existing encrypted 7z, wrong-password, memory/error and Help checks remain green.
- Affected existing integration functions: **5 passed / 0 failed / 0 skipped**,
  including setup/cleanup, additional formats/links, checksums/Hash and Open With.
- The **48** total includes setup/cleanup. Native menu/Finder clicks, dimensions,
  final clean regression and full parity/publication are not established by it.

The incremental Release build passes without reported compiler warnings. Handler
and listing code are unchanged; the unrelated complete 151-registration matrix
was not rerun. The prior valid-format evidence remains recorded below.
[Imported source and limitations](compression-controls-port.md),
[executed logs](distribution.md).

The normal `~/.cache/7zip-mac-port/build/7-Zip Mac.app` was updated.
Packaging verifies 15 Mach-O files with system/bundle dependencies and a valid
ad-hoc signature. An owned LaunchServices instance with isolated
CFFIXED_USER_HOME survived three seconds and loaded the bundled Cocoa plugin.
Only that fixture instance was terminated; physical rendering was not verified.



## Benchmark / Help controls checkpoint (2026-10-04)

Benchmark now uses the same official callback-driven single-dictionary path as
Windows File Manager. Imported GUI/common arithmetic provides unrounded
accumulation, full sizes, GIPS/log formatting and memory estimates. The dialog
adds 3/2 dictionary steps, live Current/Resulting, CPU/system information and
its confirmed fresh **10-pass** default. Help adds recent-query selection and
AND/OR/NOT/NEAR insertion without changing its search worker semantics.

- Final native Benchmark formatter and Qt suites: **4 + 9 passed**, CTest **2/2**.
  Real 384-KB/two-pass input, intermediate callbacks, cumulative sizes, heartbeat,
  system/version output, twenty exact memory comparisons, UInt64/protocol checks,
  failures/reuse, controls, Restart/Stop/destruction and initial defaults.
- Shared progress regression: **13 passed** (in the earlier targeted CTest **3/3**),
  including real 7z/ZIP/encryption, Pause/Cancel/reuse and dropped consumers.
- Existing Benchmark integration: **3 passed** including setup/cleanup.
- Final Help/compression suite: **18 passed**, CTest **1/1**. Literal history,
  bounds/reordering/reopening, operator insertion, Enter, invalid/oversized
  queries and Japanese UI are covered alongside existing HTML/search/PDF checks.
- Incremental Release helper/UI builds pass without reported warnings.

The **47** total includes setup/cleanup and four native formatter checks. No
unrelated full regression or format matrix was repeated: listing/handler code
is unchanged, and the previous 151-registration evidence remains recorded.
The twenty-pass history retention is source-matched rather than tested through
an actual twenty-pass GUI run. Physical menu/printing/pixel comparison and full
portable parity/publication remain incomplete.
[Source comparison and limits](benchmark-port.md),
[executed logs](distribution.md).

The normal `~/.cache/7zip-mac-port/build/7-Zip Mac.app` was updated.
Packaging verifies **15 Mach-O files** with system/bundle dependencies and
an ad-hoc signature. An isolated CFFIXED_USER_HOME LaunchServices instance
survived three seconds and loaded bundled Cocoa; only that owned fixture
instance was terminated. Physical rendering was not verified.

今回のBenchmarkとHelpの対象検証は上記の47件が成功しています。実測経路と
Qtの自動操作を区別し、未確認の物理操作を完了扱いにしていません。


## Native handler metadata checkpoint (2026-10-04)

Properties and archive columns now read the complete ordered typed/raw handler
schema from the official open archive. Property/NT security/reparse formatters
and both Agent folder proxies are reused. UInt64, FILETIME precision, literal
Unicode/CR/LF filenames, archive-layer duplicates and item indices are retained.
A POSIX fallback BSTR lifetime in the tree proxy was corrected in the overlay.

- Final Properties/Flat suite: **19 passed / 0 failed / 0 skipped**.
- Native raw formatter: **11 passed**; targeted CTest **3/3** also covers an
  earlier 18-case Properties run and **19 passed / one skipped** overwrite cases.
  The optional cross-volume parameter was unset; prior actual SSD/SMB evidence
  is recorded separately rather than counted as executed in this run.
- Targeted integration initially had **18 passed / one failed**; the read-only
  replacement guard was then fixed. Its dedicated split/extraction rerun has
  **3 passed / 0 failed / 0 skipped**, preserving source and old destination.
- Affected transfer/progress/Open Inside/Open As suites: **25 / 13 / 14 / 19
  passed**. The comment column transition is rerun separately after correcting
  its obsolete fixed-column expectation.
- Final genuine format/extension matrix: **151/151 registrations**, **155 passed
  / 0 failed / 0 skipped**, including Qt context-menu Add/Test/Extract and
  Japanese/SHA-256 checks. Initial NTFS virtual-directory duplication and
  image-handler CR path failures were corrected before this final run.

Incremental Release builds pass. This is scoped automated verification, not a
final clean build or physical desktop verification. Native tree/alternate
streams, very large model insertion/sort, full extraction metadata and other
portable parity work remain. No public repository is created at this checkpoint.
[Implementation and limits](native-metadata.md),
[executed logs](distribution.md).

The normal `~/.cache/7zip-mac-port/build/7-Zip Mac.app` was updated. Packaging
passes system/bundle dependency checks for **15 Mach-O files** and the ad-hoc
signature check. LaunchServices startup with owned fixtures and an isolated
CFFIXED_USER_HOME profile survived three seconds and loaded bundled Cocoa.
Only that owned instance was terminated. Physical rendering was not verified.

今回のProperties・raw formatter・上書き保護・全形式の結果は上記のとおりです。
151形式は実際の有効なfixtureで確認しています。初回失敗と修正後の結果を
区別して残しました。ロック中の物理Finder／menu／Fn操作と完全なWindows
機能一致は未確認・未完了です。



## FS to archive Copy / Move checkpoint (2026-10-04)

Official portable enumeration and update pairing now implement CopyFrom folder
prefixes, handler defaults and same-name replacement. Two-panel Copy/Move and
internal-folder drops are connected to the destination panel backend.

- Final archive transfer suite: **25 passed / 0 failed / 0 skipped**.
- Affected native progress / verification: **13 passed / 0 failed / 0 skipped**.
- Final affected integration: **19 passed / 0 failed / 0 skipped**.
- Total **57**, including setup/cleanup. Incremental Release build passes.

Seven writable formats, encryption inheritance, genuine Qt right-click Copy /
destination dialog and drop events, Unicode/empty/nested inputs, duplicate /
unsafe paths, followed file links, 32 MiB Pause/Cancel and failure refresh are
covered. BZIP2 unknown-size verification and the cancellation refresh regression
were corrected before the final run. [Detailed scope](archive-transfer.md),
[executed logs](distribution.md). Native physical desktop checks,
remaining portable features, final clean/full-format runs and publication remain
incomplete.


The packaged app passes dependency/signature verification for **15 Mach-O**
files, using system/bundle runtime paths. The normal output app was updated.
An owned LaunchServices instance **PID 8396**, with an isolated
CFFIXED_USER_HOME profile and an owned fixture directory, survived three seconds
and loaded the bundled Cocoa plugin. Main code sections and all inspected helper /
framework/plugin bytes match the working bundle. Only that owned instance was
terminated. Physical rendering was not verified.

## Filesystem Move / verification checkpoint (2026-10-04)

Rename-first filesystem Move, cooperative/child-process verification Pause,
native/local verification progress and the decompression memory limit are now
connected. SSD and the actual Windows-hosted SMB volume were tested with owned
temporary directories only.

- Native progress/worker: **13 passed / 0 failed / 0 skipped**.
- Overwrite/Move: **18 passed / 0 failed / 0 skipped**.
- Affected integration: **11 passed / 0 failed / 0 skipped**.
- Targeted CTest **2/2**; incremental Release build passes without warnings.

The **42** total includes setup/cleanup. It covers all six Move overwrite
choices, source/destination concurrency and rollback/recovery, inode/birth-time/
xattr/link preservation, SSD/SMB files/folders in both directions, replacement/
backup, real child freeze/resume/cancel and timeout pause accounting, UTF-8
errors/secret redaction, verification GUI Pause/Cancel, encrypted source checks,
32 MiB 7z/ZIP/SHA-256, selected overwrite and CRC-less verification formats.

The first cross-device run found SMB's unsupported exclusive rename/swap.
The fallback uses exclusive public file creation and private old-data capture.
A later run rejected metadata snapshots around write-handle close; post-close
identity/data/metadata validation now captures the final time. The results above
are after the fixes. On that fallback, failed replacement can require restoring
old data from its reported recovery directory; it cannot provide APFS atomic swap.

The normal output `~/.cache/7zip-mac-port/build/7-Zip Mac.app` was updated.
The package passes **15 Mach-O** system/bundle dependency and ad-hoc signature
checks. Isolated HOME/CFFIXED_USER_HOME LaunchServices instance **PID 3407**
survived three seconds and loaded bundled Cocoa. Main code sections, engine,
helpers, QtPrintSupport and Cocoa match the working build. Only that owned
fixture instance was terminated. This verifies startup, not physical rendering.

No full clean build, unrelated suites or format matrix was repeated. Native
physical desktop clicks remain pending while locked. Full portable parity and
public publication remain incomplete. [Scope and limits](filesystem-transfer.md),
[executed logs](distribution.md).

## Official console progress / Help checkpoint (2026-10-04)

Official 26.03 callback telemetry replaces percent-based estimates when available.
The frontend reuses unchanged engine objects, with an external source overlay.
Help adds asynchronous Search, topic/subtopic Print and upstream-style About.

- Help/compression: **16 passed / 0 failed / 0 skipped**, CTest **1/1**.
  Original asset hashes/pages, query semantics/errors, real-page search/display,
  owned topic/subtopic PDFs, About website signal and contextual F1.
- Native progress: **9 passed / 0 failed / 0 skipped**, CTest **1/1**.
  32 MiB 7z/ZIP round trips/SHA-256, Japanese/space paths, empty directories,
  callback totals/ratios/counts, selected extraction, Test/hash, encrypted 7z,
  wrong password/recovery, Pause/Continue/Cancel/reuse, GUI heartbeat,
  closed receiver and exact UInt64/invalid-message handling.
- Affected integration: **13 passed / 0 failed / 0 skipped**.
  Update/rename/delete, encrypted update, overwrite, error recovery,
  compression methods, TAR/WIM/stream and Open With.
- Context/RAR: **5 passed / 0 failed / 0 skipped**.
  Qt context-menu 7z/ZIP Add/Test/extract with SHA-256 and genuine RAR/RAR5.
- Original and instrumented format/codec/hash tables match byte for byte.
  Incremental Release builds passed; final relink had no compiler warnings.
  The updated bundle passes dependency/signature checks for **15 Mach-O** files.

The normal output `~/.cache/7zip-mac-port/build/7-Zip Mac.app` was updated.
An isolated HOME/CFFIXED_USER_HOME LaunchServices instance (PID **99189**)
survived three seconds and loaded bundled Cocoa. Main code sections, both helpers,
7zz, QtPrintSupport and Cocoa match the working build. Only the owned fixture
instance was terminated. This verifies startup, not physical rendering/clicks.

The **43** total includes setup/cleanup. New tests initially exposed a wildcard
query parsing bug, which was corrected. ZIP complexity also includes header work,
so its expected callback total was corrected against the official source.
Windows speed/ETA mapping uses completion bytes independently of ratio bytes;
the corrected mapping has a dedicated assertion. These results are after fixes.

Full clean build/regression/151-registration matrix is deferred to the final
feature checkpoint. Native desktop clicks, physical printing and verification
worker Pause/telemetry remain unverified or unimplemented as documented.
[Executed logs](distribution.md), [source/scope/limits](native-progress-help.md).

## Official compression math / Help checkpoint (2026-10-04)

公式26.03 `CompressDialog.cpp` のメモリー・threads・solid計算を取り込み、
辞書／word・orderの自動値と`*`表示、圧縮／展開メモリー見積もり、設定保存を
接続しました。自動指定はCLIで固定値にせず公式engineへ委ねます。公式Windows
CHMの70 HTML・8 CSS・目次／索引を原文のまま取り込み、F1と各dialogへ接続。

- 新規compression / Help: **13 passed / 0 failed / 0 skipped**、**CTest 1/1**。
  自動／手入力保存、RAM制限、通常／Storeの7z・ZIP往復とSHA-256、日本語／空白、
  filename-encrypted 7z・誤password・Test・展開、GUIのメモリー不足／ZIP AES制限、
  全80file hash・全70ページとlinks・目次70／索引69・resource境界・context Help。
- 影響する既存integration: **10 passed / 0 failed / 0 skipped**。
  32 MiB 7z／ZIP往復、既存全圧縮method、TAR／WIM／stream、working folder／memory、
  検証後Trash／update mode、Open WithのAdd／Extract／Test、Hash。
- Release incremental build成功。最初の新規test期待値は上流の64bit辞書上限
  （256 MiB）とZIP Storeの表示名へ訂正しました。manifest読み込みのnodiscard警告を
  修正し、最終17-step再linkにはcompiler warning／errorがありません。
- 更新bundleの **13 Mach-O** はsystem／bundle相対依存のみ。ad-hoc署名も成功。
  通常成果物 `~/.cache/7zip-mac-port/build/7-Zip Mac.app` を更新しました。
  専用HOME／CFFIXED_USER_HOMEとfixtureによるLaunchServices instance **PID 93541**
  は3秒生存し、同梱Cocoa pluginをロード。所有を照合したそのinstanceだけを終了。
  更新元と通常成果物のmain code sections／engine／helper／Cocoa hashが一致。

今回の対象外の全suite・format matrix・clean buildは繰り返していません。
最終feature checkpointで全体を確認する方針です。ロック中の物理描画／native clicksは
未確認。圧縮controlの細部、Help全文検索／印刷、Progress詳細などの残差と公開準備は
未完了です。[仕様・上流・残差](compression-help-port.md)、[実行ログ](distribution.md)。

```bash
ctest --test-dir /DEPS/build-comments-20261004 -V -R '^compression_help$'
```

## Individual overwrite checkpoint (2026-10-04)

The incremental Release build `build-comments-20261004` succeeds without compiler
warnings/errors. This checkpoint changes the common filesystem/extraction output
path, so validation is scoped to that path and dependent UI/editor operations:

- Overwrite: **14 passed / 0 failed / 0 skipped**; individual and All decisions
  across filesystem, 7z and ZIP; rename naming, Skip/Move source retention,
  changed destination/rollback, raw links, timestamp/xattr preservation,
  waiting Cancel/destruction, stale answer rejection, all six Qt buttons and
  the MainWindow dialog connection.
- Selected integration: **17 passed / 0 failed / 0 skipped**. 32 MiB 7z/ZIP
  round trips and SHA-256 comparison, Japanese/space names, encrypted 7z and ZIP,
  selected extraction/overwrite, unsafe paths, error recovery, responsiveness,
  Pause/Continue/Cancel, ordinary Copy/Move, root elimination/Rename Existing
  and explicit Absolute extraction.
- Dependent editor **32 passed** and Properties **10 passed**, **CTest 2/2**.
  The separate overwrite CTest also passed **1/1**.

The first new-test run failed because the fixture set an xattr after making its
source read-only. Fixture setup now sets the xattr before the read-only mode.
The final results above include the corrected fixture. Independent read review
caught an occupied Auto Rename candidate risk; the installer now requires that
candidate to remain absent and uses exclusive rename. It also returns the actual
post-install snapshot for subsequent Move verification.

The updated bundle passes the 13-Mach-O system/bundle dependency and ad-hoc
signature check. An isolated HOME/CFFIXED_USER_HOME LaunchServices instance
(PID 88930) survived three seconds and loaded the bundled Cocoa plugin.
Only that owned fixture instance was terminated. This confirms startup, not
physical rendering or native clicks.

No full clean build or format matrix was repeated for this feature checkpoint.
The preceding Properties checkpoint retains their recorded results. Physical
native menu/Finder/keyboard checks remain pending while the desktop is locked.
Full portable Windows parity and public publication remain incomplete.

Re-run the new suite after building with `ctest --test-dir <build> -V -R '^overwrite$'`.
It is also included in `scripts/test.sh` and `scripts/test.sh --no-focus`.
[Executed logs](distribution.md), [behavior and remaining limits](overwrite-dialog-spec.md),
[bounded completion workflow](completion-plan.md).

## Propertiesのクリーン検証（2026-10-04）

空の `build-properties-verified-20261004` から **98ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64。native focus不要の45関数は **82 passed / 0 failed / 0 skipped**、外部editor **32 passed**、F3集計 **11 passed**、directory scanner **13 passed**、Comment **31 passed**、Open Inside **14 passed**、Open As **19 passed**、Link **28 passed**、新規Properties **10 passed**。Cocoa ownershipを含む **CTest 9/9成功**（93.93秒）。全形式matrixも **151/151登録組、155 passed / 0 failed / 0 skipped**。Qtの実context-menu hit-testingを通る7z／ZIP圧縮・Test・展開・日本語path・SHA-256比較が成功しました。

```bash
./scripts/build.sh /DEPS/build-properties-verified-20261004
./scripts/test.sh --no-focus /DEPS/build-properties-verified-20261004
./scripts/test-formats.sh --build /DEPS/build-properties-verified-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

Propertiesは公式 `PanelMenu.cpp` / `Agent.cpp` / `AgentProxy.cpp` / `ListViewDialog.cpp/.rc`と照合しました。単体／複数／未選択、explicit／implicit／empty folder、root／prefix、通常／FlatのSize／Packed Sizeと件数、CRCの加算・欠落・overflow、layer属性の順序を検証。実ZIP、Split→ZIPの2層、7z内のZIP、ネスト保存後の親Size／CRC／Physical Sizeが一致しました。2列modal、未選択の初期状態、Ctrl+A／Ctrl+C／Ctrl+Insert、1024文字短縮と原文コピー、Enter／double-click／Single clickの全文表示、OK／Cancel、翻訳対象外の値、元archiveの不変性も確認しています。clipboard試験は試験自身の値が残っている場合だけ元のMIME内容へ戻します。

初期の保存試験は、確認ダイアログを `done(Yes)`で閉じており、Qtのstatic questionが参照するclickedButtonを設定せず、保存経路を通っていませんでした。実Yes buttonのクリックとReplaceFile結果の照合へ修正。その後の試験で **終了時のSIGSEGVを1件検出**しました。MainWindowのUI member破棄後にFolderStatisticsのdestructorが通知を送り、破棄済みprogress QPointerを参照していました。destructor冒頭でworker→window接続を解除し、Progressを閉じてからwindowを破棄する専用回帰を追加。上記のクリーン試験・全形式試験は修正後で、クラッシュはありません。double-click試験の座標も、長い行全体の中央から実際に見えるviewport内へ修正しました。独立した読取レビューでは阻害指摘はありませんでした。

Linkの旧記載「未選択のfocused itemを操作」は誤りでした。上流 `Get_ItemIndices_Operated`（PanelItems.cpp 984–1001）は完全未選択時に返り、Linkは選択1件だけを操作します。未選択ではdisabledとなるよう実装と28件の試験を修正し、再検証しました。F3／Comment等の別のfocused-item処理と混同しないよう仕様・監査表も訂正しています。

Propertiesのraw値はconsoleの64-byte省略とWindows nativeの256-byte表示に差があり、全型付きmetadata schema・全failed-open情報も未完了です。FS Propertiesは上流のWin32 shellページに対する既存列／F3値のOS代替です。ロック中の物理menu／Finder／Fn・pixel比較は未確認。全機能同等・公開完了とは判定していません。

通常成果物 `/DEPS/build/7-Zip Mac.app` も更新。専用HOME／CFFIXED_USER_HOMEとfixtureでLaunchServicesのbackground instance（PID 84698）を起動し、3秒の生存とbundle内Cocoa pluginのロードを確認しました。所有を照合したこのinstanceだけをSIGTERMで終了。mainの `__text`／`__const`／`__cstring` とengine／helper／Cocoa pluginはclean成果物と一致し、13 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。物理描画・操作の確認ではありません。

[回帰ログ](distribution.md)、[全形式ログ](distribution.md)、[Properties仕様・制限](properties-dialog-spec.md)。

## Link編集のクリーン検証（2026-10-04）

空の `build-links-verified-20261004` から **91ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64。native focus不要の45関数は **82 passed / 0 failed / 0 skipped**、外部editor **32 passed**、F3集計 **11 passed**、directory scanner **13 passed**、Comment **31 passed**、Open Inside **14 passed**、Open As **19 passed**、新規Link **28 passed**。Cocoa ownershipを含む **CTest 8/8成功**（91.54秒）。全形式matrixも **151/151登録組、155 passed / 0 failed / 0 skipped**。Qtの実context-menu hit-testingを通る7z／ZIP圧縮・Test・展開・日本語path・SHA-256比較が成功しました。

```bash
./scripts/build.sh /DEPS/build-links-verified-20261004
./scripts/test.sh --no-focus /DEPS/build-links-verified-20261004
./scripts/test-formats.sh --build /DEPS/build-links-verified-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

Linkは公式 `LinkDialog.cpp/.rc` と `CApp::Link()` に照合し、既存raw target、相対／絶対、日本語・空白、dangling／self-loop／mutual-loop、file／directory型、両folder browse、Flat／2panel既定値、selection復元、Cancelを検証しました。当時のfocused未選択試験は上流の選択条件と異なっており、上のProperties検証段階で実装・仕様・試験を訂正しました。リンク自体のowner／group／mode／nanosecond mtime／xattrを保持し、どちらのtarget dataも変更しません。ACLはAPIによるコピーを実装していますが、独立fixtureは未確認です。

外部変更・通常file／directoryへの置換・削除・親folder変更・hard-linked symlink・不正UTF-8を拒否し、元または外部データの保持を確認しました。7つの故障注入は交換前後の親変更、実install失敗、rollback成功、rollback権限失敗、後続の別entry、staging名置換を扱います。rollback不能時の回復データを実際に読み、他の処理が作成した同名stagingを削除しないことまで確認しました。同一userによる悪意ある競合を完全に排除するCAS／停電時の永続化を保証する意味ではありません。

初回の試験ではDarwinの`link()`がsymlinkを追うfixtureと、2panelの試験対象特定に誤りがありました。`linkat(..., 0)`と主panelの明示特定へ修正した後、故障注入を追加して上記クリーン検証を実行しています。独立した読み取りレビューでは阻害級の不具合は指摘されませんでした。今回の検証でアプリ／テストのクラッシュはありません。

通常成果物 `/DEPS/build/7-Zip Mac.app` も更新しました。専用HOME／CFFIXED_USER_HOMEとfixtureでLaunchServicesのbackground instance（PID 75225）を起動し、3秒の生存とbundle内Cocoa pluginのロードを確認。所有を照合したこのinstanceだけをSIGTERMで終了しました。mainの `__text`／`__const`／`__cstring` とengine／helper／Cocoa pluginはclean成果物と一致し、13 Mach-Oのbundle／system相対依存、ad-hoc署名も成功しました。ロック中の物理描画・menu／Finder操作は未確認です。

上流 `PanelMenu.cpp::CreateFileMenu` がarchive内部でShellメニューを作らないことを確認し、旧監査の「archive内部の形式別Open As不足」を訂正しました。FileメニューのInside／*／#は既に対応・自動検証済みです。全metadata／Properties、上書き等の実際の残差とnative操作確認は未完了で、全機能同等・公開完了とは判定していません。

[回帰ログ](distribution.md)、[全形式ログ](distribution.md)、[Link仕様・制限](link-dialog-spec.md)。

## Shell Open Asのクリーン検証（2026-10-04）

空の `build-open-as-verified-20261004` から **86ステップでクリーンビルド**し、レビューで追加した試験も再ビルドして実行しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64。native focus不要の45関数は **82 passed / 0 failed / 0 skipped**、外部editor **32 passed**、F3集計 **11 passed**、directory scanner **13 passed**、Comment **31 passed**、Open Inside **14 passed**、新規Open As **19 passed**。Cocoa ownershipと合わせて **CTest 7/7成功**（89.88秒）。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**で、右クリックsubmenuのQt mouse eventによる7z／ZIP圧縮・Test・展開・日本語path・SHA-256照合も成功しました。13 Mach-Oのsystem／bundle相対依存とad-hoc署名の確認も成功しました。

```bash
./scripts/build.sh /DEPS/build-open-as-verified-20261004
./scripts/test.sh --no-focus /DEPS/build-open-as-verified-20261004
./scripts/test-formats.sh --build /DEPS/build-open-as-verified-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

公式 `Explorer/ContextMenu.cpp::kOpenTypes` の順序・表示条件、別々のOpen／Open As設定と初期値、`ParseType`の意味を照合しました。Finder Open Withと通常FS右クリックに `*` / `#` / `#:e` / `7z` / `zip` / `cab` / `rar`を追加。通常OpenをOFFにした場合だけsubmenuの先頭に通常Openを表示します。既存の明示保存済み表示リストは維持し、新規項目はOptionsからONにできること、Apply後のCancel・icons OFFも検証しました。

強制指定の7z／ZIP／CAB／旧RARをList→Test→選択展開し、既知payloadと一致しました。RAR5の明示`rar`は上流どおりcode 2、通常検出ではRar5として展開・byte一致。prefix＋stored ZIP＋その内部stored ZIP＋suffixの実fixtureでは、`#`は3領域、`#:e`は重なる内部ZIPを含む4領域。Offset／Sizeによる独立byte-sliceと各展開結果が一致しました。

実QMenu hit-testingを通るQt mouse eventで、Open Withボタンの`*`とfile-list右クリックの`#:e`を実行しました。同じarchiveを通常／指定modeで開き直す処理、Refresh、内部folderからの成功時root復帰、失敗時prefix・旧mode保持、header暗号化7zのpassword retryと入力Cancel、無効mode／複数file要求の拒否を検証しました。元archiveは保持されます。初回試験はCABのmember名をfixtureの変数名と取り違えた1件と、popup表示前に試験timerを停止した1件が失敗しました。実member `dir1/file1`とpopup待機へ直して再実行。独立レビューで確定実装バグは指摘されず、提案された2つの失敗復帰試験を追加しています。上記は修正・追加後の結果で、今回の実行でアプリ／テストのクラッシュはありません。

通常の成果物 `/DEPS/build/7-Zip Mac.app` も更新しました。専用HOME／CFFIXED_USER_HOMEとfixtureでLaunchServicesのbackground新instance（PID 64882）を起動し、生存とbundle内Cocoa pluginの読み込みを確認。そのinstanceだけをSIGTERMで終了しました。クリーン／通常buildの__text／__const／__cstring、7zz・helper・Cocoa pluginのSHA-256はそれぞれ一致。画面内容や物理操作の確認ではありません。

Favoritesのmode保存も上流と再照合しました。Windowsもpath文字列だけを保存し、現在開いているarchiveを再利用する場合はmodeを維持、閉じた後は通常検出です。明示parser modeの永続化をMacだけの未実装とする根拠はなく、仕様文書を訂正しました。

[回帰ログ](distribution.md)、[全形式ログ](distribution.md)、[上流仕様・制限](archive-open-modes.md)。画面ロック中なので物理menu・新しいFinder submenu・Ctrl+PgDn／Fnは未確認。全metadata列等も未完了で、全Windows portable機能同等・公開完了とは判定していません。

## Open Inside *／#のクリーン検証（2026-10-04）

空の `build-open-modes-verified-20261004` から最新コードを **82ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64で、native focus不要の45関数は **82 passed / 0 failed / 0 skipped**。外部editor **32 passed**、F3集計 **11 passed**、directory scanner **13 passed**、Comment **31 passed**、Open Inside modes **14 passed**、Cocoa ownershipと合わせて **CTest 6/6成功**。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**。13 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。

```bash
./scripts/build.sh /DEPS/build-open-modes-verified-20261004
./scripts/test.sh --no-focus /DEPS/build-open-modes-verified-20261004
./scripts/test-formats.sh --build /DEPS/build-open-modes-verified-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

公式26.03のOpenType／File menu dispatchを照合し、非再帰の`*`とparser領域の`#`を実装しました。Split `.bin.001`内のZIPでは、通常openはZIPの内容、`*`は結合した外側fileを表示し、それぞれの展開byteを照合。prefix＋ZIP＋suffixは3つのparser項目を表示し、Offset／Sizeの元byte範囲と全展開結果が一致しました。Testと選択領域SHA-256も独立計算と一致しました。無加工ZIP・未認識fileの`#`はcode 2の失敗として保持し、普通のZIPへ黙ってfallbackしません。

Qt MainWindowのfocused未選択項目、modeごとのRefresh・Extract・CRC、parserから子ZIPへ入る／Upで戻る操作、temp cleanup、PropertiesのOffset、通常／header暗号化7z親の内部で`#`を開く操作とpassword retryを確認しました。FSへ戻るとmodeを消し、次の通常openへ漏れません。`*`で開いた通常ZIPのComment／Create Folder／ReplaceFileも検証しました。通常検出が複数層を開いたZIPはhelperへ外側fileを渡さず、未対応のcollapsed container更新を無効にします。

初回追加試験はSHA-256表示の大文字／小文字を厳密比較したため1件失敗しました。既存試験と同じ大小文字無視へ修正し、GUIのCRC経路も追加しました。独立レビューで確定バグは指摘されず、提案された`*`下のZIP更新回帰を追加しています。本クリーン結果は修正・追加後で、アプリ／テストのクラッシュはありません。

通常の成果物 `/DEPS/build/7-Zip Mac.app` も更新・packagingしました。新規の専用HOME／CFFIXED_USER_HOMEとfixtureを指定したLaunchServicesのbackground起動で、作成したPID 57548の生存とbundle内Cocoa pluginを確認し、そのinstanceだけをSIGTERMで終了しました。クリーン／通常buildの__text／__const／__cstring、7zz・helper・Cocoa pluginのSHA-256はそれぞれ一致しました。画面内容・物理操作を確認した意味ではありません。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[上流仕様・実装・制限](archive-open-modes.md)。Shell Open Asの形式選択／`#:e`、明示parser modeのFavorites復元、全形式別列は未完了です。物理menu／Ctrl+PgDn／Finder／Fnは画面解除後の検証が必要で、全Windows portable機能同等・公開完了ではありません。

## FS／ZIP Commentのクリーン検証（2026-10-04）

空の `build-comments-verified-20261004` から最新コードを **78ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64で、native focus不要の45関数は **82 passed / 0 failed / 0 skipped**。外部editor **32 passed**、F3集計 **11 passed**、directory scanner **13 passed**、追加のComment **31 passed**、Cocoa ownershipと合わせて **CTest 5/5成功**。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**。公式engineへリンクしたコメント用helperを含む13 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。

```bash
./scripts/build.sh /DEPS/build-comments-verified-20261004
./scripts/test.sh --no-focus /DEPS/build-comments-verified-20261004
./scripts/test-formats.sh --build /DEPS/build-comments-verified-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

File → Comment／Ctrl+Zのfocused item操作、選択復元、FSの`descript.ion`読書き、Flatのrootコメントと子itemの編集禁止、Comment列を実MainWindowで確認しました。ASCII／日本語／空白、BOM／CRLF、空値の削除、未編集pairの保持を検証。read-only、symlink／hard link、外部変更、同名・trim衝突、無効UTF-8、表現できない名前は元を保持して拒否し、Pause→Cancel後の復旧も確認しました。POSIX mode／owner／group／xattr保持を検証し（ACLコピーは実装済みで独立fixtureは未確認）、setgid親folderで継承groupが変わる保存候補も修正後に元のgroupを保持しました。case-sensitive volume上の`A`／`a`実fixtureは未確認です。

ZIPは公式IOutArchiveのプロパティ更新で1entryのコメントを書き換え、更新前後のTest／SHA-256とguarded commitを行います。Deflate／BZip2／LZMA、AES／ZipCrypto、65,535 byteコメント、ZIP64 **65,539 entry**、virtual folder／同名ZIP項目／分割ZIPを検証しました。Pythonの独立読み取りで対象の生compressed／encrypted payload、非対象local record、展開byte、コメント、archive全体コメントを照合。誤password・Cancel・未対応のstreamed ZipCrypto＋Descriptorは元を保持し、その後のTestも成功しました。同名ZIP項目のコメント一覧は個別に保持し、曖昧な更新は拒否します。

開発中の失敗は、case-insensitive APFSでの同名fixture作成方法、公式ZIP更新で追加されたNTFS時刻のゼロ精度を文字列差として拒否するguard、Macの`getgroups`型指定でした。専用fixtureと時刻の同一instant比較、`gid_t`へ修正しました。独立レビューが指摘した継承groupと同名ZIPのJSON表現も修正・回帰確認しています。本クリーン結果はこれらの修正後で、本段階ではアプリ／テストのクラッシュは発生していません。

通常の成果物 `/DEPS/build/7-Zip Mac.app` も再ビルド・packagingしました。専用の`CFFIXED_USER_HOME`／HOMEと入力folderでLaunchServicesのbackground新instance（PID 50600）を起動し、生存とbundle内Cocoa pluginの読み込みを確認。確認で作成したinstanceだけをSIGTERMで終了しました。画面内容・物理操作の検証ではありません。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[実装・上流根拠・制限](file-comments-spec.md)。legacy OEM/code-page完全互換、特殊layout更新、ComboDialog履歴・pixel parityは未完了です。画面ロック中のため物理Ctrl+Z／nativeメニュー／Finder確認は保留し、全Windows portable機能同等・公開完了の判定にはしていません。

## 通常FS／Flat一覧の非同期化と最終クリーン検証（2026-10-04）

空の `build-directory-verified-20261004` から最新コードを **72ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64で、native focus不要の45関数は **82 passed / 0 failed / 0 skipped**。外部editor **32 passed**、F3集計 **11 passed**、追加のdirectory scanner **13 passed**、Cocoa ownershipと合わせて **CTest 4/4成功**。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**。12 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。

```bash
./scripts/build.sh /DEPS/build-directory-verified-20261004
./scripts/test.sh --no-focus /DEPS/build-directory-verified-20261004
./scripts/test-formats.sh --build /DEPS/build-directory-verified-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

通常FS／Flatの列挙とmetadata取得はworkerへ移し、GUIの行生成は128件／約8 msずつ分割しました。旧一覧を保持してから完成候補を差し替え、古い世代の結果と選択復元callbackを破棄します。Refresh、Options Apply、archive退出時の対象再選択、Open Withの複数入力選択、header復元を確認しました。アーカイブからFSへ移動中のRefresh／連続Upが旧archiveへ戻る競合も修正しました。

実3万entryでは通常一覧 **2,338 ms / heartbeat 127回 / 最大イベント間隔640 ms**、Flat **2,343 ms / 130回 / 最大631 ms**。途中のEsc取消、別folderへの移動、scanner破棄、2panelの読込・削除・closeを確認しました。FIFO／symlink loopを内容読取・追跡せずに一覧へ載せ、root symlinkは閲覧できます。権限0000で対象pathとerrnoを示し、旧一覧／選択／LastPathを保持し、権限復旧後の再実行も成功しました。最後のQt model挿入／sortとnative iconは同期であり、任意の巨大一覧やnetwork I/Oの最大応答時間を保証しません。

Finder/Open Withの操作メニューは、非表示の起動時folder走査を取消して表示します。前回パスが3万entry、2panel設定でも1秒以内のメニュー表示と、その後のFile Manager／両panelの復帰を自動検証しました。これはQtのQFileOpenEvent／button経路の試験で、Finderからの物理操作とは区別します。形式試験の7z／ZIP右クリックサブメニューでは実Qt mouse eventによる作成・Test・Extract toとSHA-256比較も再確認しました。

開発途中では、既存試験の即時一覧前提を完了待機へ変更しました。権限エラー通知で同期 `exec()` に入りtimerによる応答が止まる待ちを再現・sample採取し、非同期 `open()` に修正。通知callbackが戻ってから応答する回帰条件を追加しました。再表示の追加処理も、表示済み親へ2panel目を作る際にcontrols初期化前のShow eventへ入り、test binaryがSIGSEGVになりました。crash stackの `QLineEdit::setText → cancelFilesystemRead → showEvent → constructor/setWindowFlags` に照合し、初期化完了flagとnullable pointersで修正。実際に表示中のpanel再作成と、以前失敗したnested panel退出試験は修正後に成功しました。2panelのtree検索もownerを明示して試験対象を分離しました。失敗試験が残した専用fixtureだけを片づけ、ユーザーデータには変更していません。

通常の成果物 `/DEPS/build/7-Zip Mac.app` も再ビルド・packagingしました。LaunchServicesのbackground起動で新instance（PID 39803）の生存とbundle内Cocoa pluginの読み込みを確認し、確認で作ったinstanceだけをSIGTERMで終了しました。クリーン／通常buildの実行fileの__text／__const／__cstringはSHA-256一致、7zzとCocoa plugin全体も一致。画面ロック中のため、描画内容・物理操作を確認した意味ではありません。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[仕様・制限](directory-scanning.md)。nativeメニュー／Finder／Fnの実機確認とWindows portable全機能の差分解消は保留・継続で、公開完了の判定ではありません。

## F3フォルダー集計のクリーン検証（2026-10-04）

新規の空 `build-folder-statistics-final-20261004` から **66ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64で、native focus不要の45関数は **82 passed / 0 failed / 0 skipped**。外部editor suiteは **32 passed**、追加のfolder statistics suiteは **11 passed**、Cocoa ownershipと合わせて **CTest 3/3成功**。全形式matrixも **151/151登録組、155 passed / 0 failed / 0 skipped**。12 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。

```bash
./scripts/build.sh /DEPS/build-folder-statistics-final-20261004
./scripts/test.sh --no-focus /DEPS/build-folder-statistics-final-20261004
./scripts/test-formats.sh --build /DEPS/build-folder-statistics-final-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

F3は、上流のEditItem(false)と同じく、選択したFS folderの集計をfocused fileのviewer起動より優先します。Size／Folders／Filesの値、選択とfocused itemの保持、2と10の数値sort、未選択focused folder、Refreshで値を消す動作をDetails／Icons／Flatで確認しました。GUIでは実QActionから集計を起動し、Progress CancelへQt mouse eventを送りました。物理F3／Fnキーやnativeメニューのクリック検証とは区別します。

日本語・空白・隠しfile・空folder、hard link、file／directory／dangling／loop symlink、FIFO、32 MiB sparse fileを使い、logical sizeとentry数を照合。symlinkは追わずlink自体の長さをfilesへ数え、symbolic rootは拒否します。存在しないroot・通常file・権限0000のroot／内部folderの失敗では不完全な計数を表示せず、対象pathとerrnoを保持。権限を戻した後のGUI再実行も成功しました。

3万entryの実folderでPause／Continue／Cancel、GUI heartbeat、backend再利用を検証。2panelの片側の集計中は両側busyとなり、Cancel後に操作が復帰し、ManagerのcloseもPause中のworkerを取消できることを確認しました。集計はfile内容を読まず、作成済みfixtureを変更・削除しません。

初回Flat試験はテストのLastPath設定漏れでホームfolderを同期走査し、CTestの120秒timeoutになりました。初期パスを専用fixtureへ固定して修正し、timeoutが残したtest所有directoryだけを片づけました。2panel試験の最初の版も、メニューが実際に操作しているpanelとspyのpanelが異なり失敗したため、操作対象のchild panelへspyとCancel確認を合わせました。上記クリーン結果は両修正後のものです。アプリのFS／Flat一覧初期走査が同期である課題は残ります。

通常の `scripts/build.sh` の成果物 `/DEPS/build/7-Zip Mac.app` も再ビルド・packagingしました。LaunchServicesのbackground起動で新しいinstance（PID 18125）がbundle内の `libqcocoa.dylib` を読み込んで生存することを確認し、確認で作成したinstanceだけをSIGTERMで終了しました。ロック下のため画面内容・物理クリックは未確認です。クリーンbuildと通常buildの実行fileの__text／__const／__cstringはそれぞれSHA-256一致、7zzとCocoa plugin全体も一致しました。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[上流仕様・実装と制限](folder-statistics.md)。画面解除が必要な物理操作の確認は保留したままです。全Windows portable機能同等・公開準備完了の判定ではありません。

## 外部編集・Refresh・終了処理のクリーン検証（2026-10-04）

新規の空 `build-editor-release-check-20261004` から **60ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64で、native focus不要の45関数は **82 passed / 0 failed / 0 skipped**。別の外部editor suiteは **32 passed / 0 failed / 0 skipped**、Cocoa ownershipと合わせて **CTest 2/2成功**。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**。12 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。

```bash
./scripts/build.sh /DEPS/build-editor-release-check-20261004
./scripts/test.sh --no-focus /DEPS/build-editor-release-check-20261004
./scripts/test-formats.sh --build /DEPS/build-editor-release-check-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

7z／ZIP／TAR／単一image WIM／gzip／bzip2／xzの外部View／Edit後の確認付き書き戻し、7z data/header暗号とZIP AES／ZipCrypto、日本語・空白・literal wildcardを検証。Yesで対象1件だけ更新し、NoとCancelでは上流どおり破棄します。read-only・別folderへ移動・元entry削除・更新中Cancelでは回復copyがwindow破棄後も残ることを確認しました。nestedの別々のpasswordと親への書き戻しも、独立した再展開・SHA-256比較で確認しました。

専用CLI editorのfork子、観測済みのsetsid子、短命launcher、失敗終了、存在しないprogramを検証。Manager終了後も実行中editorを終了させず、その入力を保持することを実PIDとfileで確認しました。test所有のbackground `.app`を作り、実LaunchServicesの `open -W -a ... --args` と、通常FSのF4でも日本語・空白を含む引数が届くことを確認しました。既定関連付けやユーザーのeditorには変更を加えていません。

2panelで同一archiveを編集し、1件目のCopy中Pauseでは2件目の確認・更新を待機させ、再開後の両方の変更と他fileを確認しました。実7zzがsolid blockを再圧縮してMethodを変えるケースも正常更新として扱い、4 MiBの他fileの内容一致を別展開で検証しました。

archive内部folderでのRefreshは実Listを再実行し、prefixとheader暗号passwordを保持する回帰試験を追加。初回の全体回帰では、追加Listを置換として数える試験側の問題と、終了直前の自動Refreshでcloseが中断する実装の競合を検出しました。operation別の計数へ修正し、終了開始後は背景Refresh・編集確認を止めました。上記のクリーン回帰は修正後の結果です。今回の試験ではクラッシュは発生していません。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[仕様・制限](external-editing.md)。任意のdaemon／IPC委譲、既に起動中のユーザーアプリ、物理メニュー／Finder／Fnキーは未確認または制限が残ります。画面ロック下の自動検証を全Windows機能同等・公開準備完了と読み替えていません。

## Nested archive書き戻しのクリーン検証（2026-10-04）

本段階の修正を、新規の空 `build-nested-writeback-final-20261004` から **50ステップでクリーンビルド**しました。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64で、native focus不要の44関数が **80 passed / 0 failed / 0 skipped**。Cocoa ownershipは **CTest 1/1成功**。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**。12 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。物理メニュー／Finder／キーの未確認を成功へ読み替えていません。

```bash
./scripts/build.sh /DEPS/build-nested-writeback-final-20261004
./scripts/test.sh --no-focus /DEPS/build-nested-writeback-final-20261004
./scripts/test-formats.sh --build /DEPS/build-nested-writeback-final-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

7z→ZIP→7z内の空folder作成・Rename・Deleteを、Up／address／共通祖先への移動／終了から確認付きで書き戻し、外側から順に再展開して保存内容のSHA-256を確認しました。別々のouter／middle／inner password、Yes／No／Cancel、未変更の終了、2paneの片側を閉じる操作も検証。上流どおりNoとCancelは変更を破棄して退出します。gzip／bzip2／xz→TAR内の変更も書き戻して再展開・hash一致を確認しました。

7z／ZIP／TAR／単一image WIM／gzip／bzip2／xzの1file置換を、7z data/header暗号とZIP AES／ZipCryptoも含めて検証。既存item名、同じサイズで古いmtimeの更新、他file保持、誤password、symlink入力、不明target、FIFOを確認しました。追加のwhole Testとselected SHA-256、元と編集fileのstat/hash照合を経て差し替えます。更新中の編集file変更は拒否し、元archiveを保持しました。親Copy中のCancelでは元を変えず、編集済みcopyがwindow破棄後も残り、回復先から再展開できることを確認。test所有の回復copyは検証後に片づけました。

独立した読み取りレビューで、書き戻し確認中の自動Refreshが別Listを開始して編集copyを失う問題を発見し、退出全体のbusy状態とoperation／target照合で修正しました。実debounceを確認dialog中に発火する回帰試験を追加しています。同期退出後のaction再有効化と、closeEvent内の再帰closeを遅延させる修正も、No／Cancel／未変更／2pane試験で確認しました。

追加試験の初回実行には、テスト用timerが局所pointerを参照captureして`port_tests`がSIGSEGV終了したものがあります。診断stackの`QTimer::setInterval`とtest lambdaで原因を確認し、pointerを値captureするよう修正しました。修正後の対象11件と、このクリーン80件は成功しています。アプリ本体のクラッシュとして扱いません。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[実装範囲と残る制約](nested-archives-progress.md)。外部editor終了後の書き戻し、offset付き親、mixed-password等の未対応は文書に残しています。全Windows機能同等・最終版公開の完了判定ではありません。

## Archive Create Folderのクリーン検証（2026-10-04）

レビュー修正を含む本段階のソースを、新規の空 `build-content-tools-final-20261004` から **50ステップでクリーンビルド**しました。native focus不要の38関数は **55 passed / 0 failed / 0 skipped**。Cocoa ownershipは **CTest 1/1成功**。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64を使用し、12 Mach-Oのsystem／bundle相対依存とad-hoc署名を確認しました。全体のWindows機能同等・最終版完成の判定ではありません。

```bash
./scripts/build.sh /DEPS/build-content-tools-final-20261004
./scripts/test.sh --no-focus /DEPS/build-content-tools-final-20261004
./scripts/test-formats.sh --build /DEPS/build-content-tools-final-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

7z／ZIP／TAR／単一image WIMのroot・内部prefixへの空folder追加、7z data/header暗号、ZIP AES／ZipCryptoを検証。元payloadとのSHA-256一致、POSIX権限とtest拡張属性の保持、誤password・重複・file親・不正名・破損・read-only・symlinkの拒否を確認しました。Copy中のPause→Cancelと、Copy中／Test後の外部変更を専用fixtureへ注入し、元を変更せずstageを削除することを確認しました。

実MainWindowのCreate Folder actionから、password再入力、追加folderの再選択、Progressの完了・Close保持をQtで検証しました。`passwords.txt`との衝突がpassword dialogを誤表示しないことも確認。12,000-entry TARの2 MiB超listing、検証Testへのmemory limit引き渡し、WIM Link／L属性とlink親拒否も回帰確認しました。memory limitは実7zzを呼ぶwrapperで実argvを記録した検証で、OOM故障注入ではありません。WIM link拒否はtechnical-list replayで、Windows junctionを含む実imageの検証ではありません。

最初のGUI追加試験では、テストが存在しないQTextEditを参照して`port_tests`がSIGSEGV終了しました。QPlainTextEditの存在を確認してから読むようテストを修正し、追加19件と本クリーン実行が成功しました。アプリ本体のクラッシュとして扱いません。[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[実装と残る制約](archive-content-tools.md)。物理メニュー／Finder／キーは画面解除後の確認が必要なままです。

## Clipboard・archive内容hashの追加検証（2026-10-04）

通常buildを更新・packagingして、追加2関数を含むnative focus不要の33関数が **43 passed / 0 failed / 0 skipped**。Cocoa ownershipは **CTest 1/1成功**。既存の専用fixture原本を使用した全形式matrixも **151/151登録組、155 passed / 0 failed / 0 skipped**。12 Mach-Oの依存とad-hoc署名を確認しました。

```bash
./scripts/build.sh
./scripts/test.sh --no-focus
./scripts/test-formats.sh --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

Ctrl+Cの日本語・空白・folder・2panel・icon view・archive名コピー、Ctrl+X／Vの無処理をQt key eventで確認しました。テストは既存clipboardを記録せずにメモリー内で退避・復元します。archive内容hashは全11方式を7z／ZIP、7z data/header暗号、ZIP AESで元ファイルと照合。folder集計・literal wildcard・誤password再入力・元archive SHA-256の不変も確認しました。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[上流の実装根拠と範囲](archive-content-tools.md)。本項は増分buildの記録です。次の機能追加後に新規空buildで再確認します。画面解除を要する物理キー・nativeメニュー操作は引き続き未確認です。

## ネスト閲覧・Pauseの追加検証（2026-10-04）

新規の空ディレクトリ `build-nested-pause-clean-20261004` から **49ステップのクリーンビルド成功**。Qt 6.11.3 / source-built 7-Zip 26.03 / arm64で検証しました。

```bash
./scripts/build.sh /DEPS/build-nested-pause-clean-20261004
./scripts/test.sh --no-focus /DEPS/build-nested-pause-clean-20261004
./scripts/test-formats.sh --build /DEPS/build-nested-pause-clean-20261004 --manifest /DEPS/format-fixtures-eighth-20261004/manifest.json
```

native focus不要の31関数は **37 passed / 0 failed / 0 skipped**。Cocoa ownershipは **CTest 1/1成功**。全形式matrixは **151/151登録組、155 passed / 0 failed / 0 skipped**。12 Mach-Oのsystem／bundle相対依存とad-hoc署名も成功しました。

7z→ZIP→7zの同一panel閲覧、日本語・空白名、virtual address、Favoritesからの複数段再移動、refresh、親での再選択、SHA-256一致、元archiveの不変を確認しました。外側と内側のheader暗号化、誤password→正password、password拒否、破損inner archive、Extract→List切替中の**Progress Cancelボタン**も検証しました。source-folder設定下で、外部open用の展開ファイルがUp／FS移動後も保持されることを確認しました。

実MainWindowのProgressへQt mouse eventを送り、Pause→Continue→正常完成、Pause→Cancel→新規出力なしを検証しました。Pause中のGUI heartbeat継続、再開後のSHA-256一致とbackend再利用も確認しました。Split／Combine／Copy／Moveはworker境界でPauseし、再開・取消、source保持と出力一致を確認しました。

[回帰生ログ](distribution.md)、[形式別ログ](distribution.md)、[範囲と残る差](nested-archives-progress.md)。ログの行末空白だけを除去しています。画面はロックされたままのため、native focus・物理メニュー／Finder／キー操作を成功扱いしていません。ネストarchive更新と外部編集後の書き戻し、追加verification中のPauseは引き続き未実装です。

## 全拡張子・右クリックの追加検証（2026-10-04）

空の `/DEPS/build-format-acceptance-20261004` から48ステップのビルドが成功しました。Qt 6.11.3と公式source-built 7-Zip 26.03を使用し、12 Mach-Oすべてのsystem／bundle相対依存とad-hoc署名を検証しました。

```bash
./scripts/bootstrap-fixture-tools.sh
./scripts/build.sh /DEPS/build-format-acceptance-20261004
./scripts/test-formats.sh --build /DEPS/build-format-acceptance-20261004
./scripts/test.sh --no-focus /DEPS/build-format-acceptance-20261004
```

新規fixture生成からのstrict runは **151/151登録組、QtTest155 passed／0 failed／0 skipped**。61ハンドラー／138 distinct extensionsです。原本からのSHA-256、独立system libarchiveによるRAR multi-volume参照、NSIS source tarから読んだCOPYINGとの比較を使用しました。既存fixture指定のrunでも同数成功しました。

Hashの23拡張子はlist／Open／Test／extract結果まで確認しました。上流未対応のdigest方式はcode2、console Hash展開はE_NOTIMPL／code2として区別し、成功検証・成功展開とは報告しません。CHM／Hxsはstored container読取、DOCX等は正しいcontainerのalias試験です。全codec／専用文書の意味的完全性は未確認です。[全151組](format-coverage.md)、[範囲と出典](archive-formats.md)、[生ログ](distribution.md)。

右クリックは実FileListへQt context eventを送り、実submenu項目へmouse eventを配送しました。7z／ZIP作成→Test→Extract to→日本語・空白名のSHA-256一致が成功しました。context項目をQAction::triggerだけで実行した試験ではありません。AppKit／Finderでの物理クリックは別途未確認です。

native activation不要の27関数を明示選択し、data rows・init/cleanupを含め **33 passed／0 failed**。暗号化、32MiB往復、安全path／symlink、上書き、Cancel、通常ファイル操作、Split／Combine、Benchmark、追加形式、Open With、Hashを確認しました。Cocoa ownershipの20回列変更／20回一覧再構築も **CTest1/1成功**。[ログ](distribution.md)。full suiteのfocus確認を削除・skip・成功扱いせず、画面解除後のnative menu／keyboard確認を保留しています。

このcheckpointは全Windows portable機能の最終完成版ではありません。残りは[final-audit.md](final-audit.md)です。

実施日: 2026-10-03〜04（日本時間）。下記の初回安定版検証の対象実装commitは `33a4775622961e3e6b32197c5b11a614343bfbf5`、branch `main`。メニュー配色とOptionsの追加修正・検証は後述する。

## 環境とビルド

macOS 26.6.2 (25G83)、Apple M3 / arm64、Apple Clang 21.0.0、Command Line Tools `/Library/Developer/CommandLineTools`。Xcode.appは使用していない。CMake 3.31.6 / Ninja 1.11.1.4 / Qt 6.11.3はユーザー領域に導入した。初期PATHにはbrew / cmake / ninja / qmakeが存在しなかった。

公式7-Zip 26.03ソースSHA-256: `9cbde5099c6deb73691b0579063da5827522ccbbcba3f0020fd04e8c8c16c0d4`。公式 `cmpl_mac_arm64.mak` とApple Clangで7zzをビルド。GUI・7zzのdeployment targetは15.0。SDKはCLTのMacOSX27.0.sdk。

SMB上でCMake AUTORCCロックとcodesign拡張属性の問題が発生したため、リソースを明示的rccルールへ変更し、成果物をローカルへ配置した。既存ユーザーデータに変更は加えていない。

Qt Cocoaのクラッシュ修正とアドレス欄のキー競合修正後、空の新規 `/DEPS/build-acceptance-20261003` から以下を実行して成功した。

```bash
./scripts/build.sh /DEPS/build-acceptance-20261003
./scripts/test.sh /DEPS/build-acceptance-20261003
```

24ビルドステップが成功。QtTestは **21 passed / 0 failed / 0 skipped**（初期化・後処理を含む）、nativeアクセシビリティ回帰検証も成功。CTestは **2/2 passed**、12.43秒。標準出力記録（行末空白のみ整形）は [verified-test.log](distribution.md)。Mach-O 12本の全load dependencyとrpathはsystemまたはbundle相対参照。`codesign --verify --deep --strict`も成功。

Cocoa pluginはQt 6.11.3対応公式ソースから、上流候補とローカルの所有権修正を適用して46ステップで別途ビルド済み。アプリのクリーンビルドではこの検証済みpluginを同梱する。[原因・patch・再現条件](qt-cocoa-fix.md)。クリーンビルドした `.app` 自体も起動し、`lsof`でそのbundleの `Contents/PlugIns/platforms/libqcocoa.dylib` が読み込まれたことを確認した。

## 自動検証

| テスト | 実行結果 |
|---|---|
| 7z / ZIP往復 | ASCII、日本語、空白、複数、階層、空directory、32MiBの固定seed乱数データを作成 → 圧縮 → list → Test → 展開 → 全ファイルSHA-256一致 |
| 暗号化 | 7zデータ暗号化／ファイル名暗号化、ZIP AES-256／ZipCrypto。正しいpasswordでTestと展開、間違いは失敗として認識 |
| stdin secret | argvに秘密を含めず動作。出力・結果にpasswordが含まれないことを確認。ZIPの非ASCII passwordは上流の制約として入力拒否 |
| archive更新・Rename・Delete | updateで追加、archive内Rename、Delete後の一覧一致。ヘッダー暗号化archiveの更新とTestも成功 |
| 選択展開・上書き | 指定fileだけ展開、Skipで既存内容保持、Auto renameで別名、Ask→Cancelで既存保持、Overwriteで一致 |
| 不正pathとlink | `../`、絶対path、Windows drive、改行、symlink、重複entry、No pathnames同名衝突、出力subdirectory symlinkを拒否 |
| I/O・エラー | 壊れたarchive、存在しない7zz、不正出力directory、read-only出力を失敗として認識。既存内容保持と後続正常操作を確認 |
| RAR | libarchive v3.8.2 fixtureをlist／Test。リンクを除いた通常2file＋空folderを選択展開、内容とSHA-256一致 |
| volumes | 4MiB分割7z作成、.001/.002存在、先頭volumeからTest成功 |
| 非同期Cancel | 高圧縮中350msでCancel。GUIイベントループheartbeatを確認し、新規archiveが残らず、その後Test成功 |
| 通常Copy / Move | 階層と空folderを含む32MiBコピー、hash一致、Move後の移動元消滅。自分自身へのコピーはエラー |
| GUI操作 | 実CocoaデスクトップのQt WidgetsでEnter／Backspace、F2 Rename、F7 Create Folder、Addで7z／ZIP作成、内部一覧、Test、Extract後hash一致 |
| GUI password / Drop / Cancel / 失敗復帰 | missing passwordから入力し暗号化内部一覧、壊れたarchiveの警告をOKで閉じて以前のarchiveとアドレス表示を保持、file URLのドロップでZIPを開く、実Progress Cancelボタンで復帰 |
| アドレス欄のキー競合 | archiveのfolderを選択したままアドレス欄でBackspaceを入力編集に使用し、Enterで壊れたarchiveを開く。一覧のOpen / Upが割り込まず、失敗後のarchiveとアドレスを保持 |
| Cocoaアクセシビリティ | 合成要素とWidgetsセルの所有権を事前検査。native selected-child queryを伴う列変更20回と9列の一覧clear / 再構築20回で親interfaceと選択セル数を保持 |
| 圧縮方式 | 7z: Copy / LZMA2 / LZMA / PPMd / BZip2。ZIP: Copy / Deflate / Deflate64 / BZip2 / LZMA / PPMdの作成＋Test |

テストのGUIウィンドウは明示的にactivateし、ショートカット配送前にフォーカスを確認する。別アプリを操作しながら同時にGUIテストを走らせない。

## 実機GUI確認

同梱Qtと7zzを使用する `.app` をmacOS上で起動し、ネイティブ画面操作で専用一時フォルダーを閲覧した。Addダイアログから複数file／folderをファイル名暗号化7zへ圧縮。Password入力後の内部一覧、日本語・空白の表示、TestのEverything is Ok、Extractの完了を確認。展開後の4fileは全てSHA-256一致し、空folderと階層も存在した。

画面確認でツールバーのCopy / Move / Info表記、BMPのマゼンタ透明色、Add / Extractラベルのmnemonic、Windows順のOK / Cancel / Helpを調整した。

壊れた7zで対象path・終了コード2・Is not archiveを表示し、OK後に通常フォルダーへ戻れることを確認した。失敗時にアドレス欄だけが入力先へ残る問題は修正し、GUI回帰テストで一覧とアドレスの一致を確認した。回帰テストのmacOS native alert内ではQt timerが進まず停止したため、実装とテスト双方のダイアログをQt Widgetsへ統一し、最終実行では全件成功した。

最終の通常出力先 `.app` を再ビルドして起動し、Qtの警告を閉じた後に以前のpathと一覧を保持することを実機でも確認した。専用一時directoryの31byteのテストfileを選択し、Delete → Trash確認（初期No）→ Yes → Everything is Okを確認。移動元からfileが消え、一覧も空へ更新された。Trash内の内容照合はmacOSのプライバシー制限で拒否されたため未確認。権限変更やゴミ箱を空にする操作は行っていない。

その後の画面読み取りでQt Cocoaのクラッシュが見つかったため、完了扱いを取り消して修正した。上流候補のみのnative検証ではQt Widgetsセルの別の所有権エラーも再現し、追加修正した。修正版bundleで「行選択 → F2 Rename → Cancel → native画面読み取り」、Ctrl+R、F7 → Cancel、暗号化7zの内部一覧と選択、TestのEverything is Okを再確認し、クラッシュは再発しなかった。行選択中のアドレス欄Enterによる破損7zのエラー表示、OK後の元の一覧・アドレス復帰も実機で成功した。

## メニュークリック報告後の追加修正・検証

「開いたメニューの項目が押せない」という報告を受け、実装済みコマンドと無効コマンドの状態を照合した。有効コマンドのクリック失敗は再現できていないが、本番の配色設定では `QPalette::setColor(role, color)` がDisabledグループも黒くし、無効コマンドまで有効に見えていた。この表示不具合をDisabledのText / WindowText / ButtonTextをグレーへ戻して修正した。この時点ではTools全項目など未実装の機能は無効のまま保持した。後のユーザー指摘を受けてOptionsを追加した（次節）。

本番のPortStyle・palette・font・stylesheetをテストでも共用した。修正前は「無効項目が有効項目と異なる文字色」の回帰検証が失敗し、修正後は成功した。QtのQWindow経由のクリックで6メニューの表示、About、選択によるRenameの有効化と実ファイルのリネームを確認した。別の専用テストプロセス内でAppKitへdown / upイベントを配送する `cocoa_menu_tests` でも6メニューの表示、About / Rename / Propertiesの実行と選択条件を確認した。QAction::triggerを直接呼ぶテストではない。

空の `/DEPS/build-menu-check-20261003` で以下を実行した。

```bash
./scripts/build.sh /DEPS/build-menu-check-20261003
./scripts/test.sh /DEPS/build-menu-check-20261003
```

28ビルドステップ成功、QtTest **22 passed / 0 failed**、Cocoa ownership検証とCocoa menu検証も成功。CTest **3/3 passed**、15.03秒。記録は [menu-test.log](distribution.md)。依存確認は12 Mach-Oすべて成功。全メニュー項目の人間による物理クリックは未確認。画面操作ツールの座標クリックは `noWindowsAvailable` で使用できず、AppKit入力テストとアクセシビリティ経由の操作確認を区別して記録する。

## Optionsの追加実装（2026-10-04）

Tools → Optionsを有効化し、上流と同じ6ページを追加した。Settings、Folders、Editorの設定をQSettingsで保存し、Applyで即時反映、OKで保存・終了、Cancelで最後のApply以降の変更を破棄する。通常一覧の初期値は上流RegistryUtils.cppに合わせ、Show dots / Full row等をOFFにした。macOS dark appearanceでFusionのチェック枠が白くなる問題も、アプリのlight color scheme指定で修正した。

本番bundleのアクセシビリティ操作でTools → Options → Settingsを開き、Show grid linesをON → Apply → Cancel。一覧のグリッド表示を確認後、アプリを終了・再起動して設定がONのまま保持されることを確認し、検証前のOFFへ復元した。OSの関連付け・外部サービス・アクセス権は変更していない。

自動検証ではOptionsのメニュークリック、6ページの順序、Apply / Cancel、設定の再読込、単一クリック移動、外部View / Editor / Diffへ日本語・空白のパスと引数を渡す実行、指定フォルダーでの圧縮ステージングと更新、1GBメモリー指定でTest / Extractと32MiB SHA-256一致を確認した。AppKit経由の自プロセステストでもOptionsをクリックして開き、Applyを確認した。

`./scripts/build.sh && ./scripts/test.sh` が成功。QtTest **24 passed / 0 failed**、CTest **3/3 passed**、18.68秒。記録は [options-test.log](distribution.md)。12 Mach-Oの依存・署名検証も成功。空の新規build-options-check-20261004からの29ステップのビルド自体は成功したが、追加更新テストが以前の検証で改名済みのASCII fixtureを指定して失敗した。存在する日本語fixtureに修正した後の上記実行は全件成功した。設定項目の拡張後に再度クリーンビルドを行う。

## 18項目の完成条件

| 番号 | 条件 | 根拠 |
|---|---|---|
| 1–2 | .app生成・実起動 | 空directoryビルド、署名検証、実機GUI起動 |
| 3–5 | FS、7z、ZIP一覧 | GUIテスト・実機閲覧・backend list |
| 6–9 | 7z／ZIP圧縮・展開 | GUI経由の双方作成／展開、32MiBを含むhash往復 |
| 10–11 | Test・password 7z | 正誤password、自動／実機GUI、header encryption |
| 12 | 失敗時クラッシュしない | backend失敗後正常操作、実機GUIで失敗表示後復帰、Qt Cocoaの修正後のnative回帰検証 |
| 13 | 非同期Cancel | heartbeat、自動GUI Cancel、再実行成功 |
| 14–15 | 日本語・Windowsに近いメイン画面 | hash往復、公式メニュー／toolbar／BMPと実機画面確認 |
| 16–18 | 差分、手順、結果文書 | windows-parity.md、README、当記録と生ログ |

## 未確認・未対応

実Finderへのarchive drag-out、Finderの関連付け選択、Trash内の内容照合、全function-key設定、Intel Mac、macOS 15実機は未確認。外部アプリ編集後の書き戻し、Pause、Finder Extension / Quick Actionは未実装。Optionsの物理リムーバブル媒体判定と.app選択による外部起動は未確認（設定コマンドの実行は検証済み）。disk-fullの故障注入は行っていないが、QProcessの非成功終了とQtファイルI/O失敗を表示する構造を備える。2 Panels、圧縮の詳細Options、fresh / syncは下記追加検証で確認済み。

初期実験でのpasswordフラグ誤用、ZIP非ASCII password、GUI testのフォーカス競合、Qt Cocoa ownership、アドレス欄Enterの競合は対応済み。最終の新規directoryビルドでは全テストが通過した。Qt修正は上流merge済みではなく、このPortで適用・検証するローカル修正である。

## Windows設定の追加対応（2026-10-04）

空の`/DEPS/build-settings-check-20261004`から33ステップでビルドし、QtTest **32 passed / 0 failed**、CTest **3/3 passed**（22.72秒）。[settings-test.log](distribution.md)に実出力を保存した。12 Mach-Oの依存はsystem / bundle相対のみ、署名検証も成功。

```bash
./scripts/build.sh /DEPS/build-settings-check-20261004
./scripts/test.sh /DEPS/build-settings-check-20261004
```

| 追加検証 | 実行結果 |
|---|---|
| 言語・root設定 | 公式翻訳の読込、日本語File / Cancel、言語のApplyと保存、root duplication設定の反映 |
| root duplication / rename existing | 32MiBを含む一致rootの除去・hash一致、不一致rootは保持、既存内容を_1へ残して新規内容を配置 |
| 圧縮詳細 | -mmemuse / Parameters、MTime / CTime / ATime、日時精度、-stl、アクセス日時の秒＋ns保持、時刻の省略、詳細Optionsボタンと保存・再読込 |
| Full / Absolute | /var・/tmpの親symlinkを正規化してFull作成、絶対pathを保存した7zの通常展開は拒否、明示したAbsoluteで専用一時directoryへ復元してhash一致 |
| 辞書・order | PPMdのmem / order指定を修正。7z全7方式、ZIP全6方式（Storeを含む）の作成とTest |
| 追加形式 | TAR / WIM / XZ / gzip / bzip2のGUI設定由来requestで作成→Test→展開→SHA-256一致。TARのリンク設定を有効化してlistでlinkを確認 |
| 圧縮後Trash | 出力配置・Test・元データ照合後の移動、7zの復元内容一致、出力失敗時の元データ保持。TARのCRCなし照合、XZの名前なしstream、ファイル名暗号化7zも成功 |
| 更新モード | ZIPの圧縮メモリー指定、freshで新規fileを除外、syncで消えたfileを除去し新規fileを追加 |
| View保存 | Icons / Details / Flat、1ns表示、ボタン文字切替、auto refresh OFFとON、2 Panels、2番目panelへのF7配送、再起動後の2 Panels・列幅・列順・日時設定保持 |

試験中にPPMdの辞書プロパティ、Full指定とmacOS親symlink、TARの空linkフィールド、XZの名前なしlistで問題を検出して修正した。2 PanelsはQMainWindowをWidgetとして埋め込み、ショートカット配送を修正した。再起動検証の列幅失敗はテストが2番目の一覧を取得していたため、対象panelを特定するテストへ修正した。全修正後の空directory実行が上記32件成功である。

テストでは専用一時directoryだけを読み書きし、Trashを空にする操作は行っていない。OS関連付け、アクセス権、外部サービスは変更していない。GUI自動検証と人間の物理クリックによる全項目検証は区別する。

その後、言語数がEnglishを含めて93であることと、Ctrl+1 / Ctrl+4 / F9による表示切替のQtキーイベント検証を追加した。同じクリーンビルド先で再ビルド・全3テストを実行し、QtTest **32 passed / 0 failed**、CTest **3/3 passed**（21.51秒）。[settings-test.log](distribution.md)はこの最終実行の記録である。

通常出力先の本番 `.app` でもアクセシビリティ経由でTools → Optionsをクリックして開いた。Languageを日本語へ変更し、メニューとツールバーの翻訳、Viewから2 Panelsへの切替、終了・再起動後の日本語と2 Panelsの保持を確認した。検証後はEnglish・1 Panelと元のフォルダーへ戻した。

専用directoryのファイルからAdd → Optionsを開き、Automatic 80%のメモリー表示、日時のSetによる精度・保存項目の有効化、形式に応じたlink項目の無効化も実画面で確認した。詳細OptionsとAddはCancelで閉じた。この実画面確認では新規archiveの作成や既存データの変更は行っていない。

画面操作ツールからのF9では切替が確認できず、Qtで受信したF9の自動検証とメニュークリックの成功を区別する。物理function keyとOS側のキー設定との組合せは未確認のままである。またAddを開いた直後に画面操作ツールのScreenCaptureKitが一度エラー（-3811）を返したが、直後の画面状態取得ではダイアログが開いており、その後の操作も成功した。アプリのクラッシュはこの追加確認では再発していない。

## 全機能監査とOpen Withの追加（2026-10-04）

上流の有効な静的メニュー65コマンドと動的メニュー・キー・設定を再照合した。**全機能同等ではない**。未実装をWindows専用に含めず、[final-audit.md](final-audit.md)に分類した。提供された公式source archiveのSHA-256もキャッシュ済み上流と一致した。提供Mac binaryへの切替は行っていない。

新規の空`build-open-with-check-20261004`から34ステップでビルドした。Open With設定、イベント配送・キュー、実圧縮・展開、CRC11方式・Hash形式、Cancel後の復帰を追加検証した。実機Finder検証で見つかったProgress表示の巻き戻りを修正し、Closeと結果表示が一覧更新後も残ることを回帰検証した。メニューは非同期modal表示とし、選択中の親ウィンドウ操作を抑止した。

修正後はQtTest **36 passed / 0 failed / 0 skipped**（初期化・後処理を含む）、CTest **3/3 passed**。Cocoa所有権の20回の列変更・20回の一覧再構築、AppKit入力の6メニューとOptions Applyも成功。[open-with-test.log](distribution.md)は下記の最終クリーンビルド実行の記録（行末空白のみ整形）。TSMのCFMessagePortとフォントfallbackの情報は出力されたが、検証は全件成功し、アプリのクラッシュは再発していない。

| 新規自動検証 | 結果 |
|---|---|
| FileOpen・表示設定 | 日本語／空白の2pathをまとめ、通常ファイルでは展開を隠す。9項目・icons保存、全OFFでもManager、non-local URLを無視、表示中・処理中の次要求を保持 |
| 操作メニューの圧縮・展開 | 7z / ZIPのクイック作成、Add / Extractダイアログ、2archive連続Test、Here / To、空folder・日本語・hash一致 |
| 暗号化 | header encryptionのExtract Hereでpassword入力・再実行・内容一致 |
| Open Inside | archiveを`.txt`に改名しても内部を表示 |
| CRC / Hash | 11方式、SHA-256一致、公式Hash handlerで日本語のSHA256 / SHA1 manifest作成・Test、既存出力保持、改変後Test失敗。FSでTestを有効化し、Openからmanifest内部を表示 |
| Cancel | 32MiBの圧縮をボタンで中止、heartbeat、元データ保持、新規出力なし、待機中メニューの表示、後続hash成功 |

### 実Finderで確認した範囲

専用の`~/.cache/7zip-mac-port/gui-open-with-check-20261004`だけを使用した。Finderの「このアプリケーションで開く → その他…」で新bundleの完全パスを指定し、「常にこのアプリケーションで開く」はOFFのままにした。

通常の`日本語 sample.txt`で圧縮・CRC・最下段Managerのメニューを確認し、7zクイック圧縮を実行した。作成した7zをFinderから送り、展開・Test等のarchive用項目を確認。「名前/に展開」で実ファイルが存在し、元とのSHA-256一致を確認した。最下段から日本語fileが表示された内部一覧へ移動できた。

Options → 7-Zipの9チェック項目を表示し、ZIPをOFF → Apply → Cancel。アプリ再起動後のメニューではZIPだけ非表示、Managerは最下段のままであった。検証後は全9項目ON・icons ONへ復元した。File Manager起動中のFinder要求でも操作メニューが表示され、そこからTestのEverything is Okを確認した。完了画面とアプリを閉じ、プロセスが終了したことも確認した。

Finderの候補リストには以前の`build-acceptance-20261003`が同名で登録されており、そちらを選んだ試行では旧動作になった。以後は新bundleのパスを明示して検証した。OSの既定関連付けを変更しない方針を維持し、この注意点をFinder手順へ追加した。

実Finderの複数選択・folderのOpen With・drag-outは未確認。複数FileOpenと異なる親入力の圧縮は自動検証／実装経路の確認であり、Finderで全組合せを試したと報告しない。以前の「Finderの関連付け選択は未確認」は、今回確認した通常ファイル・7zのOpen With経路に限って解消した。

### 修正後の最終クリーンビルド

もう一つの空`build-open-with-release-20261004`から34ステップでビルドし、QtTest **36 passed / 0 failed / 0 skipped**、CTest **3/3 passed**（33.77秒）。チェックリストの行をクリックして選択しSpaceで変更する操作、SHA manifestの通常Openも追加で成功した。12 Mach-Oの全依存はsystem / bundle相対のみで、ad-hoc署名の検証も成功。

```bash
./scripts/build.sh /DEPS/build-open-with-release-20261004
./scripts/test.sh /DEPS/build-open-with-release-20261004
```

通常出力先を同じソースから`./scripts/build.sh`で更新し、依存・署名検証に成功した。その`build/7-Zip Mac.app`もFinderのOtherから実際に選択し、通常ファイルの操作メニューと最下段Manager、復元したZIP項目の表示を確認した。メニューの閉じるボタンでCancelするとFile Managerは開かず、アプリのプロセスも終了した。ソース・license・この監査記録を同梱する。

## File tools checkpoint (2026-10-04)

Split / Combine / Link / Select by Type / Favorites / official-engine Benchmark and the original Windows FM.ico have been added. A new empty `build-file-tools-clean-20261004` was built with Apple Clang/Ninja and packaged successfully. The bundle dependency and signature checks passed.

The earlier full run after the icon fix passed QtTest 40/40 and CTest 3/3. The final clean-directory rerun, including subsequent Progress and password-dialog wait corrections, passed 33 QtTest cases but failed 7 focus-dependent cases and the Cocoa menu test. `ioreg` confirmed `CGSSessionScreenIsLocked=Yes`; Cocoa reported application inactive, window visible but not key. These are **not reported as a passing final GUI rerun**. A repeat on an unlocked desktop is pending. The test activation helper explicitly activates only its own process and retains real native-focus assertions. Raw output: [file-tools-test.log](distribution.md).

New automated cases that passed in the clean directory: 32 MiB split/combine SHA-256 equality, varying size sequence, 1,001-part numbering, missing-volume and collision errors, cancel cleanup/recovery, hard-link inode identity, relative symlink target, directory link, type selection, folder/archive Favorites, official benchmark results and Stop/Restart.

Separately, before the screen lock, the packaged app was opened and its menus clicked through computer-use tools: a Japanese-named 4 MiB file was split into 5 volumes and combined; original and output SHA-256 were `ba9d292eb500bc95c8c314b8356da63bd091453a232ddcca69f80f2da482bb01`. Tools → Benchmark showed real engine measurements, and Stop interrupted a 100-pass run during pass 2. This native verification predates the latest minor corrections; it does not replace their pending final GUI rerun.

The final release and all-format/right-click acceptance remain unfinished.

## Official Agent CommentItem checkpoint (2026-10-05)

ZIP Comment now executes the imported official Agent operation using one focused
real item index. The bespoke comment-write mutation phases and whole-archive
password-verification requirement were removed. Same-name GUI selection and Flat
descendant comments are covered. Compatibility path requests resolve a unique
native row before writing; ambiguous requests retain the original.

Native helper and affected Qt builds exited zero. Final scoped results:
`file_comments` **31 passed / 0 failed / 0 skipped**, native selection/update
cases **11 passed / 0 failed / 0 skipped** (42 including setup/cleanup).
Cases include Japanese/multiline/clear comments, AES without an unrelated data
password, real/implicit folders, independent compressed/encrypted bytes,
ZIP64/BZip2/LZMA, ordinary ZipCrypto, descriptor refusal, maximum length,
filesystem comments, cancellation and original retention. Existing corrupt
packed data remains corrupt after a successful metadata-only comment update;
the chosen good sibling still passes Test. The upstream behavior is intentional.

```bash
./scripts/build-progress.sh
cmake --build /path/to/build --target comment_tests agent_selection_tests SevenZipMac -j6
ctest --test-dir /path/to/build -R '^file_comments$' --output-on-failure
./scripts/test-agent-tree.sh /path/to/build agent_selection_tests \
  nativeCommentSameName nativeCommentNoUnrelatedTest \
  nativeCommentEncryptedPackedData guiSameNameComment \
  normalAndFlatFolderRules encryptedNativeUpdates
```

[Source port and remaining limits](native-agent-comments.md),
[executed log](distribution.md). Physical AppKit/Finder clicks,
final full-parity refactoring and the empty-directory final-release build remain
pending. The complete format matrix was not unnecessarily repeated for this
operation-specific increment.

The normal `build/7-Zip Mac.app` was rebuilt and packaged. Bundle checks passed
for 15 Mach-O files with only system/bundle-relative dependencies and ad-hoc
signatures. LaunchServices started the owned app process (PID 67261), which
survived startup and loaded its bundled patched Cocoa plugin. Its bundled native
helper exited zero and had the same Mach-O UUID as the new source build. Only
that owned test process was stopped. This is startup confirmation, not physical
menu/Finder interaction.

## Ordered extraction installation session (2026-10-05)

既存のguarded installerを状態付きsessionへ分離し、通常の展開処理から利用するよう変更。完了fileの逐次配置、rename-existing履歴、creation time、finalizeの再実行、承認後に別fileへ置き換わった出力先の拒否を実fileで確認した。日本語Unicode表記差の既存collision試験も成功。metadata **265 passed / 0 failed**、progress **15 passed / 0 failed**（setup／cleanup含む）。File Manager targetの増分build成功。通常bundleは前回の検証済みcheckpointを保持しており、本変更のpackaging・実起動とnative callback接続は未完了。[範囲](extraction-session.md)、[実行log](distribution.md)。

## Original output-state query adapter (2026-10-05)

公式CheckExistFileとAutoRenamePathのbodyを維持し、metadata queryをguarded lookupへ接続するnative部品を追加。別の出力先のfile／directory／symlink情報によるCRC不一致ZIPのdecode Skip、extension／dotfileの公式auto-name、unsafe parentの数値error、5 GiB sparse fileのexact size／100ns FILETIME、query中のCancel／peer切断を9実file caseで検証。metadata **274 passed / 0 failed**、progress **15 passed / 0 failed**（setup／cleanup含む）。native helperとQt application targetの増分build成功。通常アプリの展開方式切替・新bundleのpackaging／実起動は本段階では行っていない。guarded mutation plan／逐次publicationの接続は引き続き未完了。[範囲](native-output-state.md)、[実行log](distribution.md)。

## Original ordered publication component (2026-10-05)

公式CheckExistFileのrename／remove primitiveと完了callbackへrequired replyを追加し、worker用sessionでpublic出力へ逐次配置するnative部品を実装。7z／ZIPのOverwrite・Skip・Auto Rename・Rename Existingと3同名stream、6択Ask・No/Yes/No、prompt後のfile置換を16実file caseで未改変consoleへ照合。次のCheckExistFileで前の出力が見えること、names／bytes／prompt count／exit codeの一致、承認済みfileが変わった時のESTALEと両方の保護を確認。metadata **290 passed / 0 failed**、progress **15 passed / 0 failed**（setup／cleanup含む）。native helperとQt application targetの増分build成功。通常アプリは従来pipelineのままで、hard reference／path mapping／directory conflict／async dispatchの接続と新bundleのpackaging／起動は未完了。[範囲](native-ordered-publication.md)、[実行log](distribution.md)。

## Official default Open profile (2026-10-05)

公式の外部起動拡張子表・ASCII照合・紛らわしい名前の警告を移植。不明拡張子7z／拡張子なしZIPの内容判定、一般fileへの外部fallback、permission error、password入力／取消、Insideの条件を検証。関連する外部編集・書き戻しとOpen modeを含め **87 passed / 0 failed / 0 skipped**（setup／cleanup含む）。最初のZIP fixtureは生成時の自動拡張子付加を見落として失敗し、生成後のrenameで修正した。[範囲と制限](open-profile-port.md)、[実行log](distribution.md)。実Finder／物理入力と最終release gateは引き続き未確認。

## Official File menu / drag policy (2026-10-05)

公式CFileMenu通常loop・drag effect／right-menu tableを移植。CRC／Diff欠落と作成重複、QtのStatic icon modeによるdrag無効化、folder行の別列でのtarget誤判定を修正。今回の最終対象 **116 passed / 0 failed / 0 skipped**（setup／cleanup含む）。内訳はmenu／drag34、Open27、selection16、transfer30、native stream3、native番号toolbar Test3、7z／ZIP shell圧縮・Test・展開3。32 MiB／暗号化／日本語／スペース／SHA-256、Cancel、Manager終了後の受領file保持を検証。実Finder／native右drag／Fnはロック中のため未確認。[範囲](panel-menu-drag-port.md)、[log](distribution.md)。全151登録・最終clean buildはこのUI batchでは再実行していない。

## Version-store / menu / time source batch (2026-10-05)

Official VerCtrl helpers/command body, complete CFileMenu filter and FILETIME formatter/time choices were ported together before the grouped run. **112 checks passed, 1 existing cross-volume Move case skipped**, including setup/cleanup. Initial Diff path spelling checks were corrected and that suite rerun. Actual-file bytes/time/mode/history, GUI Revert and external Diff arguments, 32 MiB Pause/Cancel, current samples/defaults, and affected menu/selection/scanner/Properties/overwrite checks passed. Bundle dependencies (15 Mach-O), strict signature and isolated 3-second startup passed. Physical input and the final full gate remain pending. [Evidence](distribution.md), [scope](version-menu-time-port.md).

## Official language/settings resource batch (2026-10-05)

公式Lang2／言語情報処理と設定・主要dialog・menuのresource ID接続をまとめて実装後、関連groupを検証。最終case結果は **87成功、失敗／skipなし**（suiteごとのsetup／cleanupは1回のみ算入）。93言語表示、翻訳者／不足／追加行、bundle Lang編集、不正UTF-8等、Apply／Cancel、入力値／Pause／Close保持、7z／ZIP Open With操作とarchive内folder作成を確認。初回2件は公式見出し／acceleratorに未追従の期待値を修正し、そのcaseだけ再実行。増分build成功。物理操作・初回OS言語・最終clean build・全機能同等・公開は未完了。[範囲](language-settings-port.md)、[初期失敗と再実行log](distribution.md)。

パッケージ化後の15 Mach-O依存はsystem／@rpathのみ。`codesign --verify --deep --strict`成功。開発Qt／DYLD環境変数を外し独立設定で3秒起動、生存・stderr空を確認し、所有processのみ停止した。bundleに公式92言語と英語参照を格納。これは物理Finderクリックや最終clean buildの確認ではない。
