# 7-Zip4Mac

[English](README.md) · [使い方](docs/features.md#日本語) · [更新履歴](CHANGELOG.ja.md)

7-Zip4Macは、Windows版7-Zipの使い方に合わせたMac用の圧縮・展開アプリです。
7zやZIPの作成に対応しています。圧縮ファイル（アーカイブ）の中身を確認し、必要なファイルだけ取り出すこともできます。
メニュー、ツールバー、圧縮設定の画面は、Windows版7-Zip File Managerに近づけています。

7-Zip公式とは別の、非公式のオープンソースプロジェクトです。
圧縮・展開には公式7-Zipのエンジンを使っています。

## できること

- 7z・ZIPの圧縮と展開。RARなど、7-Zipが読み取れる形式の展開。
- アーカイブの中身の閲覧、選んだファイルだけの展開、破損の検査。
- パスワード付きアーカイブの作成と展開。7zではファイル名の暗号化も可能。
- 圧縮レベルや圧縮方式の指定、大きなアーカイブの分割。
- ファイルのコピー・移動・名前変更、2分割画面、お気に入り。
- Finderから呼び出す、圧縮・展開用の操作メニュー。

**RARは展開のみで、RAR形式への圧縮はできません。**
そのほかの形式や操作は[使い方](docs/features.md#日本語)を参照してください。

## 対応環境

macOS 15以降を対象にしています。動作確認した環境は、macOS 26.6.2のApple Silicon Macです。
macOS 15とIntel Macでは、まだ実機で確認していません。

## インストール

現在は、ソースコードからアプリを作成して使います。ビルド済みアプリの配布はまだありません。
Apple Command Line Tools、Git、Python 3が必要です。Xcode.appは必要ありません。

ターミナルで次を実行してください。

```bash
git clone https://github.com/muracoco/7-Zip4Mac.git
cd 7-Zip4Mac
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/install.sh
```

初回は、必要なツールと7-Zip・Qtをダウンロードしてビルドします。
完了すると、アプリケーションフォルダーに7-Zip Mac.appが入ります。
7-ZipやQtを別途インストールする必要はありません。

Command Line Toolsがない場合は、先に `xcode-select --install` を実行してください。
別の場所へのインストールやビルドで困った場合は、[詳しいビルド手順](docs/building.md#日本語)を参照してください。

更新するときはアプリを終了し、ソースを更新してから上記の3つのスクリプトを実行します。

## 基本の使い方

### Finderから展開する

1. アーカイブを右クリックします。
2. **このアプリケーションで開く → 7-Zip Mac** を選びます。
3. 表示された操作メニューで、同じフォルダーに取り出すなら **ここに展開**、保存先を選ぶなら **展開…** を選びます。

中身だけ確認したいときは、メニューの一番下にある **7-Zip ファイルマネージャーで開く** を選びます。

### ファイルマネージャーで圧縮・展開する

アプリケーションフォルダーから7-Zip Macを起動します。

- 圧縮: ファイルやフォルダーを選んで **追加（Add）** を押し、保存先と7z・ZIPなどの形式を指定します。
- 展開: アーカイブを選んで **展開（Extract）** を押し、保存先を指定します。
- 中身を見る: 一覧のアーカイブをダブルクリックします。
- 破損を調べる: アーカイブを選んで **テスト（Test）** を押します。

操作の詳しい説明は[使い方](docs/features.md#日本語)にあります。

## Windows版との違い

Finderの右クリックメニューに、直接「7-Zip」を追加する機能はまだありません。
現在は **このアプリケーションで開く** から操作メニューを呼び出します。
ウインドウの操作やゴミ箱はmacOSの仕組みを使います。

一部の説明やエラーメッセージは英語です。Windows用の自己解凍ファイルの作成や、Explorer専用の機能には対応していません。
現在のビルドはDeveloper ID署名・Appleの公証を行っていません。
詳しい違いと未実装の機能は[Windows版との比較](docs/windows-parity.md)に記載しています。

## 不具合の報告

[GitHubのIssues](https://github.com/muracoco/7-Zip4Mac/issues)に、macOSとアプリのバージョン、操作手順、表示されたエラーを記載してください。
パスワードや個人情報を含むファイルは添付しないでください。

## 開発に参加する方へ

現在のアプリのバージョンは0.2.5、圧縮エンジンは7-Zip 26.03、GUIはQt 6.11.3です。
C++とQt Widgetsを使い、CMake・Ninja・Apple Clangでビルドします。
開発者用テストは、リポジトリのルートで次のコマンドを実行します。

```bash
PORT_BUILD_TESTS=ON ./scripts/build.sh
./scripts/test.sh
```

[ビルド・起動・テストの詳細](docs/building.md#日本語) · [開発の進め方](docs/development-workflow.md) ·
[7-Zip本家の更新を取り込む手順](docs/upstream-updates.md)

## ライセンス

このアプリの移植部分は [LGPL-3.0-or-later](LICENSE) です。
7-ZipとQtには、それぞれのライセンスが適用されます。
7-ZipのRAR展開コードにはunRARの利用制限も含まれます。

[7-Zipのライセンス](licenses/License.txt) · [Qtのライセンス](licenses/Qt) ·
[同梱する構成要素と著作権表示](licenses/NOTICE.md)
