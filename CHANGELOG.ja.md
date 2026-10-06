# 更新履歴

[English](CHANGELOG.md) · [README](README.ja.md)

バージョン番号はローカルアプリのビルドを表します。各項目から実装・確認記録へリンクし、
配布状況もその記録で区別しています。現在の機能と制限はREADMEと
[実装一覧](docs/current-status.md)を参照してください。

## 未リリース

- プロジェクト概要、詳しい使い方・機能、バージョンごとの履歴を分離。
- このリポジトリだけのGitHub noreply設定、作者・コミッターの検査、commit / pushの
  hookと公開する実メタデータの検査を追加。過去の履歴修正は将来のcommit検査と分けて記録します。
  [メール情報の検査](docs/git-privacy.md)。

## 0.2.5 — 2026-10-06

- 原著作権表示を保持し、Portの改変者・日付を追記。未改変の `7zz` と修正版helperの
  説明を整理。[ライセンス整備](docs/license-audit-follow-up.md)。
- 上流ソースの共通lock、SHA-256一覧、独立した比較・再生成・stagingを追加し、
  本家の更新を差分で取り込む手順を整備。[上流更新](docs/upstream-updates.md)。
- 通常ビルドをアプリだけにし、開発者テストを任意に変更。アプリの対応ソースから
  Portのテスト・fixture・過去の評価出力を除外し、開発用テストはリポジトリへ保持。
  [配布内容](docs/distribution.md)。
- 空のソースからのビルド、同梱engineによる7z / ZIPの圧縮・Test・展開、
  インストール版の起動・Aboutを確認。[実行記録](docs/maintenance-verification.md)。

## 0.2.4 — 2026-10-06

- 内容に変化がない自動更新通知でも一覧を作り直し、File Managerがちらつく問題を修正。
  バックグラウンドで内容を比較し、変更がなければ一覧・選択・アイコンを維持します。
  実際のファイル変更は引き続き自動反映します。[修正と確認](docs/idle-refresh-fix.md)。

## 0.2.3 — 2026-10-06

- 起動時の通常ウインドウを、ポインターがある画面の中央へ配置。
- ツールバーの文字を一覧・アドレスの文字と揃えるよう修正。
  [表示の修正と確認](docs/window-presentation.md)。
- Finder右クリックへ直接表示する拡張の条件と代替方法を記録。
  [Finder連携](docs/finder-integration.md)。

## 0.2.2 — 2026-10-06

- ビルド用・復元用のアプリをFinderの自動Open With候補から除外。
- 登録対象を1つにするインストール・更新処理を追加。旧版を復元可能な形で保持し、
  古い版への上書きを拒否します。[登録・導入・確認](docs/finder-registration.md)。

## 0.2.1 — 2026-10-06

- Open With →「ここに展開」の後に空のFile Managerウインドウが残る問題を修正。
  非表示の親へのCocoa sheetと、単独操作後の終了処理も修正。
  [展開の修正と確認](docs/open-with-extract-here-fix.md)。

## 0.2.0 — 2026-10-06

- OS非依存のコマンド・設定群を備えたWindows風File Managerをパッケージ化し、
  確認済みの基準版として整理。OS固有の差と未確認事項を文書化。
- アドレスバーの文字サイズと、Finder Open Withのバージョン別起動を修正。
- ZIPの更新日時が省略された場合の展開を修正。
- 利用可能なデスクトップmenu / Options、Finder経由の圧縮・Test・展開を確認。
  物理的なdrag、ハードウェア修飾キー、実プリンター、Windowsとのピクセル比較は未確認。
- 英語・日本語の文書を含む、確認済みソースリポジトリを公開。

記録: [0.2.0のパッケージ](docs/release-0.2.0.md)、
[一括確認](docs/release-consolidation.md)、[desktop確認](docs/desktop-follow-up.md)、
[Finder確認](docs/finder-desktop-acceptance.md)、[ソース公開](docs/publication-complete.md)。

過去に表示するshellコマンドを明示保存している場合、その選択を保持します。
独立した **Open archive** submenuを表示したい場合はOptionsで有効にしてください。
