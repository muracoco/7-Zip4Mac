# 7-Zip Mac Port

[English](README.md) · [更新履歴](CHANGELOG.ja.md) · [機能の詳細](docs/features.md#日本語)

Windows版 **7-Zip File Manager** の画面構成と操作手順を、C++ / Qt 6 Widgetsで
再現する非公式のmacOSアプリです。圧縮エンジンは公式 **7-Zip 26.03** のソースから
ビルドして同梱します。

**現在のローカルアプリ版: 0.2.5。** ソースは
[GitHub](https://github.com/muracoco/7-Zip4Mac)で公開しています。
バージョンごとの変更と確認記録は[更新履歴](CHANGELOG.ja.md)を参照してください。

## 動作環境

- 実機確認: **macOS 26.6.2、Apple M3、arm64**、Command Line Tools / Apple Clang 21.0.0。
- ビルドの最低macOS: **15.0**。macOS 15とIntel Macの実機確認は未実施です。
  上流のx86_64用ビルド設定は選択できます。
- GUI: **Qt 6.11.3固定**。QtBase Core / Gui / Widgets / Concurrent / PrintSupportを
  動的リンクし、対応する公式ソースから修正版Cocoa pluginをビルドします。
  QtTestは開発者用テストでのみ必要です。
- CMake、Ninja、Python 3、make、curl、Git。
  確認済みのツール版はCMake 3.31.6、Ninja 1.11.1.4です。
  **Xcode.app / `.xcodeproj` は不要です。**

## ビルド・起動

ソースディレクトリで実行します。

```bash
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/run.sh
```

`bootstrap.sh` は不足するツールと公式依存ソースを `~/.cache/7zip-mac-port` に取得します。
sudoやシステムPythonの変更は行いません。Command Line Toolsがなければ
`xcode-select --install` を実行してください。既存のQt 6.11.3は `QT_PREFIX` で指定できます。

通常の生成先は `build/7-Zip Mac.app` です。SMB共有上のcheckoutでは、ローカルの
`~/.cache/7zip-mac-port/build/7-Zip Mac.app` を使います。

Finderに登録する1つのアプリを `/Applications` へインストール・更新します。

```bash
./scripts/install.sh
```

再ビルド・package・更新前に対象アプリを終了してください。旧版は復元用に保持し、
既定のファイル関連付けは変更しません。[インストールと復元](docs/finder-registration.md)、
[ビルドの詳細](docs/building.md#日本語)に、別ディレクトリ、直接CMake、クリーンビルド、
packageの手順があります。

## 主な機能

- Windows版に近いメニュー、ツールバー、一覧、ショートカット、Add / Extract /
  Progress / Options。公式アイコンと92翻訳＋Englishを利用します。
- ファイルシステムとアーカイブ内部の閲覧、ネストしたアーカイブ、ソート、複数選択、
  Flat View、2パネル、Favorites、列設定、Properties。
- **7z / ZIP / TAR / WIM / XZ / gzip / bzip2** の作成、Test、展開。
  読み取り形式と実際の確認範囲は[形式対応表](docs/archive-formats.md)へ記録しています。
  **RAR圧縮はありません。**
- 圧縮・暗号化設定、分割volume、更新・path・overwrite方式。
  7zのファイル名暗号化とZIPのAES-256 / ZipCryptoに対応します。
- 非同期の進捗、Pause / Cancel、詳細なエラー表示と保護付きのアーカイブ更新。
  パスワードは保存・ログ出力しません。
- Copy / Move / Trash / Rename、ファイル・フォルダー作成、link、Split / Combine、
  Comment、外部閲覧・編集と確認付きのアーカイブ書き戻し。
- CRC / hash、Benchmark、一時ファイル管理、公式Helpの検索・印刷。
- Finderの「このアプリケーションで開く」から設定可能な7-Zip操作メニューを表示し、
  最下段に「7-Zip ファイルマネージャーで開く」を常に置きます。
  [Finder連携](docs/finder-integration.md)を参照してください。

個々の操作と実装・確認記録へのリンクは[機能の詳細](docs/features.md#日本語)にまとめています。

## テスト

開発者用テストは任意です。完全なリポジトリのcheckout、または開発者向けのソースarchiveで実行します。

```bash
PORT_BUILD_TESTS=ON ./scripts/build.sh
./scripts/test.sh
```

デスクトップを解除できない場合は `./scripts/test.sh --no-focus` を使います。
ネイティブのfocus・menu確認には解除済みのデスクトップが必要です。
fixtureは専用の一時ディレクトリで扱います。[実行結果](docs/test-results.md)では
自動検証と実際のデスクトップ確認を区別しています。[詳細手順](docs/building.md#日本語)には
対象を絞る方法、形式fixture、一括release確認のコマンドがあります。

## Windows版との違い・残る課題

- Registry / Explorer連携、MAPI、PE SFX作成、ホストへのNTFS security / ADS適用は
  OSの差です。NTFS / PEのアーカイブ読み取り機能は含みます。
- macOSのウインドウ操作、Trash、権限、署名、function key設定を使います。
  Finder Extension / Quick Action / Servicesは未実装です。
- Port固有の説明・エラーの一部は英語です。大量一覧の最終model反映とmetadata解析には
  GUIスレッドの処理が残ります。
- 追加のpath・更新保護はWindows版と動作が異なります。不正な `../` と出力先の親symlinkを
  拒否します。最終配置中のCancel / I/O失敗では配置済みのファイルが残り、結果へ表示します。
- 物理的なFinder drag、Fn / RightCtrl / Option、実プリンター、macOS 15 / Intel実機、
  Windows画面とのピクセル比較は未確認です。

分類と確認範囲は[現行の実装一覧](docs/current-status.md)、[Windows差分](docs/windows-parity.md)、
[コマンド監査](docs/final-audit.md)、[設定対応表](docs/settings-coverage.md)を参照してください。

## 設計・ライセンス

`GUI → ArchiveBackend → SevenZipProcessBackend → 同梱7-Zip実行ファイル`。
修正版 `7zz-progress` が公式のcallback / Agent処理を接続します。QProcessはシェルを使わず
起動し、UTF-8を増分デコードします。パスワードは標準入力で渡し、argvや設定へ保存しません。

- Port: **LGPL-3.0-or-later**。[LICENSE](LICENSE)。
- 7-Zip: LGPL-2.1-or-later、BSD-2-Clause / BSD-3-Clause、RAR展開部分のunRAR制限。
  [License.txt](licenses/License.txt)。
- Qt: LGPL-3.0の動的リンク。[Qtのライセンス文書](licenses/Qt)。
- 公式アイコン・翻訳と第三者fixtureの原著作権表示を保持しています。
  [構成要素のNOTICE](licenses/NOTICE.md)。Microsoftのフォント・システム資産は同梱しません。

アプリにはライセンス文書、7-Zip・QtBase・Portの対応ソースとQt patchを含めます。
Qtの差し替え・再ビルドを制限しません。現在のローカル版はad-hoc署名を使い、
Developer ID署名、notarization、App Store公開は今後の課題です。[配布内容](docs/distribution.md)を参照してください。

上流: [7-Zip公式](https://www.7-zip.org/download.html)、
[26.03のソース](https://github.com/ip7z/7zip/releases/tag/26.03)。

## 開発

[実装の進め方](docs/development-workflow.md) · [上流の差分更新](docs/upstream-updates.md) ·
[公開・メール情報の検査](docs/git-privacy.md) · [ライセンス確認](docs/license-audit-follow-up.md)
