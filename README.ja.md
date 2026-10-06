# 7-Zip Mac Port

[English](README.md)

Windows版「7-Zip File Manager」の操作感をQt 6 Widgetsで再現する、非公式のmacOSアプリです。C++ / Apple Clang / CMake + Ninjaを使用し、圧縮エンジンは公式7-Zip 26.03のソースからビルドした `7zz` を同梱します。Xcode.app / `.xcodeproj` は不要です。

[0.2.0のローカル候補版](docs/release-0.2.0.md)を作成しました。OS非依存のコマンド・設定群は実装済みです。Windows固有の処理、見た目・入力の未確認事項は[差分表](docs/windows-parity.md)へ記録しています。公開用ソースとライセンス文書の確認は済んでいます。ソースは [GitHub](https://github.com/muracoco/7-Zip4Mac) で公開します。

[2026-10-06の一括確認](docs/release-consolidation.md)に、空ディレクトリからのビルド・全151形式登録・初回の失敗と対象を絞った修正確認を記録しています。0.2.0ではアドレス文字の統一とZIPの未指定日時の修正を加え、再度クリーンビルドしています。

[desktop確認](docs/desktop-follow-up.md)ではOptions、AppKitメニュー、GUIで作成した48 MiB入り7zを確認済みです。[Finder経由](docs/finder-desktop-acceptance.md)でも7z／ZIP作成、GUI Test／展開、最下段からFile Managerを開く操作を確認できました。物理的なFinder drag、ハードウェアの修飾キー、実プリンター、Windows画面とのピクセル比較は未確認です。自動・ネイティブイベントによる検証結果とは分けて記録しています。

## 動作環境

- 実機確認: macOS 26.6.2、Apple M3、arm64、Command Line Tools / Apple Clang 21.0.0。
- ビルドの最低macOS: 15.0。15.xでの実機確認は未実施。
- x86_64用の上流ビルド設定も選択できますが、このプロジェクトのIntel Mac実機確認は未実施。
- GUI依存: **Qt 6.11.3固定**のQtBase（Widgets / Gui / Core / Concurrent / PrintSupport）。テストはQtTest。Cocoa pluginを対応する公式ソースから修正ビルドします。
- CMake 3.31.6、Ninja 1.11.1.4、Python 3、make、curl、Git。

一覧の並べ替え・初回方向・同値時の順序とFS列定義はWindows元コードを移植しています。列設定はプロパティIDで保存し、Flat／2パネルと旧設定の移行に対応します。[実装範囲・一括検証](docs/panel-sort-port.md)。

## ビルド・起動・テスト

```bash
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/test.sh
./scripts/run.sh
```

`bootstrap.sh` はユーザー領域 `~/.cache/7zip-mac-port` に不足するツール、公式Qtバイナリ、公式7-Zipソースを取得します。sudo、Homebrew本体のインストール、システムPythonの変更を行いません。Apple Command Line Toolsがなければ `xcode-select --install` を実行し、OSの確認画面を操作してください。既存のQt 6.11.3を利用するときは `QT_PREFIX` を指定できます。Cocoa pluginがQt内部APIを使うため、異なるQtバージョンとの混用を禁止しています。

`build.sh` は公式QtBase対応ソースに上流の修正案とローカルのセル所有権修正を適用し、Cocoa pluginを再ビルドします。詳細は [Qtクラッシュ修正](docs/qt-cocoa-fix.md)。ビルド・package前に対象 `.app` を終了してください。起動中のbundle更新はスクリプトが拒否します。

通常は `build/7-Zip Mac.app` が生成されます。SMB共有上のリポジトリでは、ロック・署名・拡張属性の問題を避けるため、スクリプトがローカルの下記パスを選びます。

```text
~/.cache/7zip-mac-port/build/7-Zip Mac.app
```

実行例:

```bash
open "$HOME/.cache/7zip-mac-port/build/7-Zip Mac.app"
./scripts/run.sh /absolute/path/to/example.7z          # File Managerを直接開く
./scripts/run.sh --open-with /absolute/path/to/file.txt  # 操作メニューを開く
```

空の別ディレクトリからのクリーンビルド:

```bash
./scripts/build.sh "$HOME/.cache/7zip-mac-port/build-clean"
./scripts/test.sh "$HOME/.cache/7zip-mac-port/build-clean"
```

CMakeを直接使用する場合は、公式ソースからビルドした `7zz` と修正版Cocoa pluginのディレクトリを指定します。後者は `scripts/build-qt-cocoa.sh` で生成します。通常は `scripts/build.sh` が両方を検出します。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.11.3/macos" \
  -DSEVENZIP_BINARY="/path/to/source-built/7zz" \
  -DPORT_COCOA_PLUGIN_DIR="/path/to/patched-cocoa/build/plugins/platforms"
cmake --build build
./scripts/package.sh build
./scripts/test.sh build
```

GUIテストはログイン済みMacのデスクトップで実行します。テストデータは専用一時ディレクトリに作成します。現在の一括確認は [release-consolidation.md](docs/release-consolidation.md)、過去の実機確認は [test-results.md](docs/test-results.md) に分けて記録しています。

上流ソースで定義される機能群をまとめて実装した後、影響するsuiteを一括確認します。修正後の再確認は、登録済みのCTest名で対象を絞れます。該当suiteがない指定はエラーにします。既定の `./scripts/test.sh` は全suiteを実行します。

```bash
PORT_TEST_REGEX='^(copy_workflow|address_workflow|panel_key)$' ./scripts/test.sh /absolute/build/path
```

変更のない形式matrix・他の合格済みsuiteは繰り返しません。初回の失敗は保存し、製品の不具合とテスト操作の不備を区別して記録します。


## 現在利用できる機能

- File / Edit / View / Favorites / Tools / Helpと、Add / Extract / Test / Copy / Move / Delete / Infoツールバー。
- 通常フォルダー、7z / ZIP等のアーカイブ内フォルダーの閲覧、親へ移動、複数選択、ソート。Large Icons / Small Icons / List / Details、Flat View、2 Panels、列幅・列順・表示列・日時精度・UTC・自動更新・フォルダー履歴を保存できます。
- 7z / ZIP / TAR / WIM / XZ / gzip / bzip2の作成とTest・展開。Hash形式でSHA-256 / SHA-1のチェックサムファイル作成・Test。XZ / gzip / bzip2の入力は通常ファイル1件。RAR圧縮は提供しません。
- 辞書・Word size / PPMd order・solid・スレッド数・圧縮メモリー・分割・圧縮プロパティ・更新モード・相対／Full／絶対パスを指定。詳細Optionsで日時精度・日時の保存・最新ファイル日時・アクセス日時保持、対応形式でsymbolic / hard link保存を設定できます。
- 全体／選択項目の展開、出力先履歴と名前付きサブフォルダー、Full / No / Absolute pathnames、root folder重複除去、Ask / Overwrite / Skip / Auto rename / Auto rename existing。絶対パス展開は確認付きです。
- Test、暗号化7z（ファイル名暗号化を含む）、ZIP AES-256 / ZipCrypto。ZIPのパスワードは上流と同じくASCIIに制限。
- 非同期処理、進捗・ログ・Cancel、終了コードを含む結果表示。圧縮／展開後の不完全な新規出力はステージングから片づけます。
- 通常ファイルから、別パネルで開いているarchiveと内部folderへCopy／Moveできます。WindowsのCopyFromと同じ既定圧縮・同名置換を使い、Moveは検証後にTrashへ移します。異なる親から内部folderへのdropにも対応します。[仕様と残差](docs/archive-transfer.md)。
- 通常ファイルのCopy / Move / Trash / Rename / Create Folder / Create File / Properties。コピー・移動と展開で、Yes / No / Yes to All / No to All / Auto Rename / Cancelの個別上書き確認を共用します。スキップした移動元は保持します。[仕様と制限](docs/overwrite-dialog-spec.md)。
- アーカイブ内の削除・リネーム。削除前に確認します。
- 通常フォルダー／Flat Viewの列挙とmetadata取得をworkerで行い、一覧の行生成も分割します。読み込み中の移動・Escキャンセル、旧一覧保持、2panelに対応。表示文字列は必要時に取得し、大量項目の比較とnative icon取得はworkerへ移しました。最後の行挿入／並べ替え反映・metadata解析には同期処理が残ります。[元実装・検証・制限](docs/panel-listing-port.md)。
- F3で通常フォルダーの合計サイズ・子フォルダー数・ファイル数を集計します。複数folder選択、数値ソート、非同期Pause／Cancelに対応。[仕様・制限](docs/folder-statistics.md)。
- Propertiesは上流に近い2列modalで、単体／複数／未選択、通常／Flatのfolder集計・CRC、ネストしたarchive階層、Ctrl+A・コピー・全文表示に対応。型付き／raw属性、形式別の列、集計は公式handler・formatter・Agent proxyを再利用します。nested保存後は親の属性も更新します。[実装と残差](docs/native-metadata.md)。
- archive一覧とPropertiesは、公式Agentのフォルダー・項目番号と、移植した属性取得処理を使います。暗黙folder、Flat順序、archive内の代替ストリーム閲覧を、同名ZIP項目と実NTFS imageで検証しました。[移植・テスト方法・残差](docs/native-agent-properties.md)。同名項目のRename／Deleteもnative indexで処理します。
- 選択展開・Test・archiveハッシュ・focused項目の一時展開も、公式Agentの選択処理を移植しました。同名項目の個別処理、通常／Flatのフォルダー選択、64MiB展開のPause／Cancelと再実行を検証しました。[移植と残る更新処理](docs/native-agent-selection.md)。
- 7z／ZIP／TAR／WIM内のDelete／Renameは公式Agentの処理を移植し、native項目番号で操作します。同名ZIP、Flat folder、複数image WIM、暗号化solid再圧縮、Cancel時の元データ保持を検証しました。[移植範囲と制限](docs/native-agent-item-updates.md)。
- gzip／bzip2／XZのDelete／Renameも公式Agentへ接続しました。名前を保存しない形式や削除失敗、XZの空streamは上流と同じ挙動です。編集中のRename、2panelで開いた子archiveの親側Renameと書き戻し、選択・focus保持も検証しました。[仕様表とGUI／console比較](docs/single-stream-updates.md)。
- editor／nested書き戻しは公式AgentのUpdateOneFileを移植し、同名項目もnative番号で更新します。更新後のsession／選択を引き継ぎ、置換した内容のサイズとSHA-256を検証します。7形式・暗号化・先頭prefixを含む101件の対象検証が成功しました。[移植範囲と実行記録](docs/native-agent-replacement.md)。
- Open／Open Outsideの複数選択処理を公式bodyから移植しました。20項目の上限、フォルダーで停止する順序、同名ZIP項目の個別起動・編集・書き戻しを検証しました。[範囲と残差](docs/panel-open-port.md)。
- 不明な拡張子・拡張子なしでも、通常のOpenから内容を判定してアーカイブを開きます。外部起動の拡張子一覧・紛らわしい名前の警告は公式bodyを移植。権限エラーとpassword取消では外部起動しません。[移植と検証](docs/open-profile-port.md)。
- アーカイブ内ファイルの一時展開と外部アプリでのOpen Outside／View／Edit。終了後の変更を確認して書き戻し、失敗・Cancelでは回復用copyを残します。実行中editorはManager終了後も停止・削除しません。
- Tools → Optionsから設定を保存・変更できます。System / 7-Zip / Folders / Editor / Settings / Languageの順は上流と同じです。OKで保存して閉じ、Applyで即時反映、Cancelで最後のApply以降の変更を破棄します。
- OptionsのSettingsで親項目、実ファイルアイコン、行全体選択、グリッド、単一クリック、代替選択、Test／展開のメモリー上限を指定。Foldersで作業用フォルダーを指定し、EditorでF3 View／F4 Edit／2ファイルのDiffに使う外部アプリやコマンドを設定できます。設定は再起動後も保持します。
- Languageで公式の92翻訳＋Englishを選択して即時反映できます。圧縮・展開設定も保存しますが、パスワードは保存しません。圧縮後削除は確認後、完成アーカイブのTestと元データ照合が成功したものをmacOSのTrashへ移動します。
- 右クリックは公式Fileメニューの通常filterを移植し、CRC／Diffと作成項目の重複を修正。パネル間ドラッグのCopy／Move・対象folder・右ボタンmenuと、暗号化archiveの非同期drag-outを自動検証しました。受け取り側のため、承認済み一時fileはManager終了後も保持します。[移植範囲・116件の検証・未確認事項](docs/panel-menu-drag-port.md)。
- FinderからのファイルドロップによるAdd、アーカイブのドロップによるOpen。アーカイブ内ドラッグは一時展開後にファイルURLを渡す実装がありますが、Finderへの実ドロップは未検証です。
- F2 / F3 / F4 / F5 / F6 / F7、Enter、Backspace、Ctrl+A、Ctrl+R、Ctrl+PgDown、Alt+Enter等。物理Controlを維持し、Commandへの自動置換を抑止。
- 通常ファイルと選択したarchive内部のCRC-32 / CRC-64 / XXH64 / MD5 / SHA-1 / SHA-256 / SHA-384 / SHA-512 / SHA3-256 / BLAKE2sp / 全方式。folderも対象になり、archive内部は公式engineで展開ファイルを作らず計算します。暗号化7z／ZIPも自動検証しました。
- Ctrl+Cで選択した名前をCRLF区切りのテキストへコピー。上流同様、Ctrl+X／Ctrl+Vは空の処理でファイルを転送しません。
- 書き込み可能な7z／ZIP／TAR／WIM内へ空folderを作成。公式AgentのCreateFolderを移植し、内部subfolder・複数image WIM・暗号化に対応。既存packed streamを保持し、コピー上で追加・一覧照合後に元を置換します。作業folder設定、SSD／SMB間の更新、権限・拡張属性保持を確認しました。旧データのpassword要求も上流と同じ必要時のみです。[移植と検証](docs/native-folder-update.md)。ネスト内の変更も、退出時の確認を経て階層ごとに親へ書き戻します。先頭prefix付きarchiveは公式Agent／consoleのstream処理を再利用して更新し、置換前にprefixの一致を検証します。[仕様と検証](docs/archive-prefix-updates.md)。tail／複数層の更新はWindows Agent自体も禁止しています。
- Finderの「このアプリケーションで開く」で選ぶと、Windowsの7-Zipコンテキストメニューに対応する操作メニューを表示します。圧縮・展開・Test・CRC SHAの表示をOptions → 7-Zipで選択できます。最下段の「7-Zip ファイルマネージャーで開く」は常に表示します。[使い方](docs/finder-integration.md)。
- `.7z` / `.zip`等と通常ファイル・フォルダーをAlternate ViewerとしてInfo.plistに宣言。既定アプリのシステム設定は変更しません。

- 通常ファイルのSplit / Combine、hard / file symbolic / directory symbolic link作成、既存symlinkのguard付き編集。raw targetと両folder browse、Flat／2panelの既定値を再現し、通常file／folderは保護します。[仕様と制限](docs/link-dialog-spec.md)。分割番号はWindows同様001から始め、1000件以上なら桁数を増やします。
- Select by Type / Deselect by Type、Alt＋数字のFavorite呼出、Alt＋Shift＋数字の保存、RightCtrl＋数字のキー処理。物理RightCtrlは未確認ですが、ネイティブキーコードを含む自動検証は成功しました。
- Tools → Benchmarkは公式のcallback経路でCurrent／Resulting、Size・CPU使用率・GIPS・CPU／OS情報を表示します。中間辞書サイズ、丸める前の累積、初期10パス、Restart／Stopに対応。[実装・検証](docs/benchmark-port.md)。Help検索には履歴選択とAND／OR／NOT／NEARの挿入メニューもあります。
- 圧縮設定の`*`自動値とメモリー見積もりに、Windows版の計算処理を利用します。自動／手入力の選択を保存します。Helpは公式英語70ページ、目次・索引・履歴と各dialogの対応ページを開けます。[仕様・検証・残差](docs/compression-help-port.md)。
- Helpの非同期全文検索、topic／subtopic印刷、上流に近いAboutを追加しました。印刷の自動試験は専用PDFへの出力です。実プリンターは未確認です。[実装と確認範囲](docs/native-progress-help.md)。

## Windows版との既知の差

現行の実装・残差・不具合・実機未確認は [current-status.md](docs/current-status.md)、全コマンドは [final-audit.md](docs/final-audit.md)、設定は [settings-coverage.md](docs/settings-coverage.md) に整理しています。native item操作、親／Flat移動の復元、事前上書き確認・順次展開・終了回復、一覧キーの全dispatch、Copy出力名と列挙は後続の機能群で実装済みです。過去のレポートの旧残作業を再実装対象と扱わないでください。Port独自文言の翻訳、文書化した安全上の制限、model挿入／parseの同期区間と物理入力・外観比較は残ります。Windows固有のOS機構はmacOSの仕組みに置き換えています。利用可能な自動統合確認と公開用ソース整理は完了しています。物理操作の確認と公開は未完了です。

Finder Open Withと通常FS右クリックには、上流のOpen archive >（`*` / `#` / `#:e` / `7z` / `zip` / `cab` / `rar`）を追加しました。Optionsで通常Openとは独立して表示を切り替えます。更新前に明示保存した表示設定は維持するため、新項目はOptionsからONにしてください。最下段の「7-Zip ファイルマネージャーで開く」は常に表示されます。

メニューのグレーの項目は、現在の選択・処理状態で実行できない項目です。ToolsのOptionsとBenchmarkは処理中以外は選択できます。Delete Temporary Filesも操作でき、公式の一時folder集計・サイズ表示を再利用して一覧／folder移動／設定／右クリックを提供します。削除は確認後にTrashへ移し、使用中・表示後に変化した項目は保護します。[範囲と検証](docs/temporary-files-port.md)。FileのRename / Copy To / Move To等はファイル選択後、Diffは外部コマンドの設定と通常ファイル2件の選択後に有効になります。

OptionsのSystemはFinderでの関連付け手順を表示します。Windows Explorerの登録、システムメニュー・large memory pages、macOS filesystemへのNTFS security / ADS適用は対象外です。archive内のstream閲覧は実装済みです。Explorerの項目選択とアイコン設定はMacのOpen With操作メニューに置き換えています。Finder Extension / Quick Actionは未実装です。代替選択は公式File Managerの処理を移植し、ピンクの選択マークとフォーカス行を独立させています。Insert、Ctrl／Shiftクリック、Shift＋矢印、2パネルと更新後の保持を自動検証しました。[移植範囲と検証](docs/panel-selection-port.md)。未設定のView / EditorはmacOS既定アプリを使います。翻訳に対応する上流ラベルを置き換え、Port固有の説明・処理エラーの一部は英語です。

アーカイブ進捗は公式エンジンのcallbackから件数、Processed、Compressed size、圧縮率を取得します。Remaining／Speedは完了byte数と経過から算出し、取得できない値は `—` を表示します。情報channelが使えない場合だけ推定表示へ戻ります。source削除前の検証workerもPause／Cancelと進捗に対応しました。通常Moveは同一volumeでrenameを先に試し、別volumeやSMBでは安全なコピーへ切り替えます。[実装・OS差・検証](docs/filesystem-transfer.md)。Split／Combineも実byte数を表示します。通常FSの列順・表示設定とcontext menu filterは上流の定義から移植しています。末尾のType列はMac Portの追加列で、取得できるOSメタデータにも差があります。通常FS／Flatの列挙・metadataは非同期になりました。表示文字列の遅延取得・worker sort／native icon取得を接続しました。最後のQt model挿入／並べ替え反映とmetadata解析には同期処理が残ります。

危険なアーカイブの `../` と出力先の親symlinkを拒否します。同名・No pathnames・大小文字の衝突は公式callbackの完了順と指定したoverwrite方式で扱います。公式callbackが許可した安全なsymlink／hard link、folderの権限・日時、取得可能な作成日時を復元します。既存leaf symlinkは参照先を変更せず置換／Skipできます。絶対パスはAbsolute pathnamesを明示した場合に確認して配置します。元の事前overwrite／decode Skip、主なfile／directory衝突、Cancel／crash時の出力保持と終了回復は接続済みです。[統合確認](docs/normal-extraction-integration.md)、[終了回復](docs/extraction-lifecycle.md)を参照してください。SMBでwriter終了後に属性が変わる問題も修正し、APFSとの往復移動と関連4群の検証を通過しました。未知のarchive variantは未確認事項として扱います。配置中のCancel／I/O失敗では配置済みfileが残り、結果にその状態を表示します。

## 設計とライセンス

`GUI → ArchiveBackend → SevenZipProcessBackend → bundled 7zz`。QProcessはシェルを介さず起動し、UTF-8を増分デコードします。パスワードは標準入力で渡し、argv・環境変数・設定ファイルに保存しません。ネイティブC++エンジンを将来接続できるよう、操作要求と結果をGUIから分離しています。

- Port: LGPL 3.0以降。[LICENSE](LICENSE) / [GPL本文](licenses/Qt/GPL-3.0-only.txt)。
- 7-Zip: LGPL 2.1以降、BSD 3-clause / BSD 2-clause、RAR解凍部分のunRAR制限。[licenses/License.txt](licenses/License.txt)。RAR圧縮アルゴリズムの再実装には利用できません。
- ツールバーとアプリアイコンは公式7-ZipソースのFileManager資産（LGPL）。DockアイコンはWindowsのresource.rcで指定されるFM.icoを未改変で保持し、macOS用icnsに変換しました。Microsoftのフォント／システム画像は同梱しません。
- 翻訳は公式26.03 Windows配布物の未改変Langファイル（LGPL）。取得元とSHA-256、保持したライセンスは [設定対応表](docs/settings-coverage.md) を参照。
- Qt: LGPL 3.0の動的framework。本文・第三者の著作権表示は [licenses/Qt](licenses/Qt)。現在の使用バージョンの対応ソース取得は `./scripts/fetch-qt-source.sh`。frameworkを差し替えて再署名することを制限しません。
- RARテストデータ: libarchive v3.8.2の公式テストfixture、BSD系ライセンス。[licenses/libarchive-COPYING.txt](licenses/libarchive-COPYING.txt)。

`.app/Contents/Resources` にライセンス文書、未改変7-Zip 26.03と公式QtBaseの対応ソース、Qt修正patchを含むPortのビルド可能なソース一式を格納します。初回buildはQtソースを取得してキャッシュします。正式なDeveloper ID署名・notarization・Mac App Store対応は行っていません。現在のMacでad-hoc署名と起動を確認します。

7-Zipの上流確認先: [公式ダウンロード](https://www.7-zip.org/download.html)、[26.03公式リリース](https://github.com/ip7z/7zip/releases/tag/26.03)。ソースの取得URL・SHA-256は `scripts/bootstrap.sh` に固定しています。[公開前の確認](docs/publication-review.md)にライセンス・由来・個人情報の確認と公開用コピーの作成方法を記録しています。公開は残る完成条件を満たした後に行います。

## 今後の改善

現在の実装・差分は [実装一覧](docs/current-status.md) を参照してください。過去の機能群の文書は、当時の検証記録として保持しています。元Agentの更新／編集書き戻し、選択／キー、事前overwrite／展開の終了回復は後続の実装で接続済みです。

残る実機確認はネイティブメニュー、FinderのOpen With／右クリック／ドラッグ、Fn／RightCtrl／Option、印刷と見た目の照合です。Port固有の英語説明、大量一覧の最終model反映／metadata解析の遅延、文書化した安全上の制限は差分として記録しています。Finder Extension／Quick ActionはOpen Withとは別の将来機能です。

機能群をまとめて実装してから一括検証し、修正後は失敗・影響箇所だけを再確認します。再現用の全体確認は `./scripts/verify-release.sh --no-focus /absolute/build/path`。デスクトップを使える場合は `--no-focus` を外します。結果は `test-results/release-*` に保存し、物理操作の未確認は別に記録します。
