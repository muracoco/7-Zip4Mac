# Distribution contents / 配布内容

Normal `scripts/build.sh` builds the app only (`BUILD_TESTING=OFF`), without Qt
Test or the developer/native formatter test executables. Use
`PORT_BUILD_TESTS=ON ./scripts/build.sh` explicitly for regression development.
Tests and their fixtures remain available in the public development repository
so upstream updates can be checked; they are optional and are not installed.

The app's corresponding Port source archive uses the `build` profile:
runtime sources, imported adapters, resources, build/import scripts, Qt patches,
upstream lock, notices, README/change history and build/feature/parity/update/license
guides. It excludes `tests/`, test runners,
release-evaluation tools, historical evaluation documents and raw diagnostic outputs. `source-archive.py
--developer` can explicitly export the development suite. A source archive
without Git metadata can still rebuild using its SHA-256 inventory manifest.

The unmodified official 7-Zip archive and QtBase source archive remain included
because users must be able to rebuild/modify the supplied components. Original
QtBase source-package tests are upstream source contents, not Port test binaries;
the Port does not remove upstream notices or rewrite those archives.

## Historical evidence

On 2026-10-06, 138 raw `.log` files and 15 generated evidence/coverage JSON files
(2,394,077 uncompressed bytes) were removed from the current public source
snapshot and kept in a local historical-evidence archive. Git history remains
intact. Old documentation links to those raw outputs now point here; the
component documents retain their concise results, test commands and limitations.
Bundle source documentation is normalized to remove local account/volume paths,
and credential-pattern checks also apply to normal packaging. New logs belong in ignored `test-results/` or the local dependency cache.

## 日本語

通常ビルドではアプリだけを作り、テスト実行ファイルとQt Testは要求しません。
開発者用テストはGitHubに残しますが任意実行です。アプリ内の移植ソースには
テスト・fixture・評価ツール・生ログを含めず、ビルドと改変に必要なものを残します。
公式7-ZipとQtの対応ソース、原著作権・ライセンス文書は引き続き同梱します。
過去のログと機械生成証跡153ファイルは手元に保存し、現在の公開ツリーから外しました。

See [maintenance verification](maintenance-verification.md) for the actual
minimal-source rebuild and bundle inspection performed for this change.
