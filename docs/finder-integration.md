# FinderのOpen With操作メニュー

Finderでファイルを選び、**このアプリケーションで開く → 7-Zip Mac Port**を選ぶと、Windowsの7-Zip shellメニューに対応する操作メニューが開く。リストにまだ出ない場合は「その他…」でビルドした`7-Zip Mac.app`を選ぶ。「常にこのアプリケーションで開く」は不要。既定関連付けの強制変更は行わない。

0.2.2以降は `./scripts/install.sh` で `/Applications/7-Zip Mac.app` を更新する。開発・package・rollback用コピーはFinderの自動候補へ出さない。旧版の実行ファイル・署名は復元可能に保持し、既定アプリの設定は変更しない。[登録の修正と復元](finder-registration.md)。

最下段は常に**「7-Zip ファイルマネージャーで開く」**。通常ファイルは親フォルダーを表示して対象を選択し、アーカイブは内部を表示する。アプリ単独起動は従来どおりFile Managerを開く。起動中にFinderから届いたファイルも操作メニューへ送る。

## 表示設定

Tools → Options → 7-Zipで次の10項目とアイコン表示を変更し、ApplyまたはOKで保存する。全てOFFでも最下段のManagerは残る。

| 項目 | 動作・表示条件 |
|---|---|
| Open archive | アーカイブ候補1件をFile Manager内で開く |
| Open archive > | `*` / `#` / `#:e` / `7z` / `zip` / `cab` / `rar`を指定して開く。単一file候補だけ表示 |
| Extract files... | 出力先・path・上書き・passwordを指定して展開 |
| Extract Here | 同じフォルダーへ展開。上書きはAsk |
| Extract to "name/" | 名前付きサブフォルダーへ展開 |
| Test archive | 整合性検証。暗号化は必要に応じてpassword入力 |
| Add to archive... | Addダイアログで形式・圧縮・暗号化等を指定 |
| Add to "name.7z" | 形式別設定を使うクイック圧縮。password・volume・削除を引き継がない |
| Add to "name.zip" | ZIPのクイック圧縮。同名archiveの更新は確認する |
| CRC SHA | 11方式、SHA-256チェックサムファイル作成、Checksum : Test |

上流26.03の`Explorer/ContextMenu.cpp` / `ArchiveName.cpp` / `FileManager/MenuPage.cpp`を参照した。Extract / Testは公式138拡張子を優先し、それ以外へ上流の非archive拡張子の除外規則を適用する。未知の形式はメニュー表示後にengineがエラーを返すことがある。通常`.txt`では圧縮とCRCだけが出る。複数archiveは順番に処理し、失敗・Cancelで後続処理を停止する。メニュー／処理中に届いた別のOpen With要求はキューに保持する。

Open archiveとOpen archive >は独立設定。前者をOFFにした場合だけ、形式選択の先頭に通常Open archiveを追加する。既存の明示保存済み表示リストは維持するため、更新前に設定を保存した場合はOptionsで新しいOpen archive >をONにする。`#:e`は重なる内部signatureも探索するparser指定、`rar`は旧RAR handlerの強制指定でありRAR5へ自動切替しない。詳しくは[archive-open-modes.md](archive-open-modes.md)。

ファイル名・引数をシェルへ連結せず配列としてengineへ渡す。passwordはstdin、保存しない。キャンセルだけで起動したアプリは表示中のウィンドウと処理がなくなれば終了する。

```bash
./scripts/run.sh --open-with /absolute/path/to/file.txt /absolute/path/to/another.txt
./scripts/run.sh --file-manager /absolute/path/to/archive.7z
```

通常のパス引数もFile Managerを直接開く。Finderの書類openイベントはOpen With経路へ送る。フォルダーはInfo.plistで宣言しCLIでも受け付けるが、Finder自身がフォルダーにOpen Withを出さない状況がある。通常ファイルの経路とは区別する。

## macOSの差と未実装

File Managerの通常ファイルの右クリックにも7-Zipサブメニューを追加しました。上記10項目とicons設定を共用し、Add／Extract／Test／CRCを同じbackend経路で実行します。未選択行の右クリックはその行を選択し、既に選択した行では複数選択を保持します。実submenuへのQt mouse eventで7z／ZIP往復、日本語・空白path、SHA-256一致を確認済みです。物理クリックは画面解除後に確認します。形式の表示条件は公式138拡張子を優先し、それ以外へ上流の除外規則を適用します。

メニューはアプリ側の小さなQtダイアログとして表示する。Finderの右クリックメニュー内に階層項目を注入するFinder Extensionではない。Quick Action、Services登録も未実装。Xcode.app・管理者権限・OS設定変更は不要。

Open WithとFile Manager右クリックの表示条件・名前・アイコン・存在確認・CRC submenu生成を共通化しました。関連4グループの一括確認で、設定／キュー／暗号化／Cancel、7z／ZIPのQtメニュー操作、CRCのSHA-256表示・checksum作成・Test、AppKitメニュー回帰が成功しています。Finderの「その他…」で現行候補を明示して操作メニューと最下段のManager項目が出るところまで実機確認しました。コマンドの選択完了、Finder drag/drop、物理キーは未確認です。外部GUI操作ツールの拒否やcapture失敗は、アプリの不具合・成功とは分けて記録しています。[範囲と実行証拠](desktop-follow-up.md)。

WindowsのExplorer登録、32bit extension、cascaded shell menu、NTFS Zone.Identifier、MAPIメール操作は移植しない。Open Asの形式別／parser指定は上記メニューへ実装した。上流PanelMenu.cppはarchive内部でShellメニューを生成しない。File → Open Inside／*／#はarchive内部の右クリックでも使用できる。CRC配置の2つのWindows設定は、Macメニュー内の1つのCRC SHA項目に置き換えている。

Info.plistに`public.data` / `public.folder`とarchive拡張子をViewerとして宣言する。インストール済みコピーはAlternate、開発用コピーはNoneにして、QFileOpenEventを受ける。[Qt QFileOpenEvent](https://doc.qt.io/qt-6/qfileopenevent.html)、[Apple CFBundleDocumentTypes](https://developer.apple.com/library/archive/documentation/General/Reference/InfoPlistKeyReference/Articles/CoreFoundationKeys.html)。実機確認と自動検証の区別は[test-results.md](test-results.md)を参照。

## Finderの右クリックへ直接追加する方法（2026-10-06の調査）

技術的にはFinder Sync拡張の`menuForMenuKind:`で、監視対象folder内の
選択fileへ「7-Zip」submenuを追加できます。`selectedItemURLs`で対象を
取得し、本体の共通コマンド／表示設定へ渡す構成が候補です。
現在のOpen WithダイアログやFile Manager内の右クリックとは別の実装です。

AppleはFinder Syncを同期folder向けとし、一般的なFinder UI変更用ではないと
説明しています。監視対象folderに限定され、任意の場所でWindows Explorerと
同じ配置になることは保証できません。拡張の署名・登録とmacOS側でのuser有効化も
必要です。設定画面での権限付与が発生する段階では人間の操作が必要です。
[Apple Finder Sync Programming Guide](https://developer.apple.com/library/archive/documentation/General/Conceptual/ExtensibilityPG/Finder.html)。

Quick Action / Action ExtensionはAppleが提供する別の連携経路ですが、表示先は
Quick Actions等であり、希望する右クリック直下のsubmenuとは異なります。
[Apple Finder Action Extensions](https://developer.apple.com/documentation/appkit/add-functionality-to-finder-with-action-extensions)。

今回の成果物はFinder拡張を含みません。このMacのCommand Line Tools SDKには
FinderSync.frameworkと`NSExtensionMain`のlink symbolが存在するため、Xcode.appが
ないことだけを実装不能の理由にはしていません。CLT/CMakeだけでの`.appex`生成・
署名・Finder読込は未検証です。実装する場合は本体から分離し、既存backendと
表示設定を共有して選択・複数file・日本語path・Cancelを確認します。
