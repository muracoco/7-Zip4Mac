# Detailed build, run and test guide / ビルド・起動・テストの詳細

[English README](../README.md) · [日本語README](../README.ja.md)

Run commands from the repository root.
以下のコマンドはリポジトリのルートで実行します。

Developer tests require a full checkout or developer-profile source archive.
For the app's build-source archive, use `BUILD_TESTING=OFF` with direct CMake and
run the normal build commands; optional test runners/fixtures are excluded.
開発者用テストには完全なcheckoutまたは開発者向けソースarchiveが必要です。
アプリ内のビルド用ソースでは直接CMakeの `BUILD_TESTING=OFF` と通常のビルド手順を使います。
任意のテストrunner / fixtureは含めません。

## English

```bash
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/run.sh
# Optional developer regression suite / 開発者用テスト（任意）
PORT_BUILD_TESTS=ON ./scripts/build.sh
./scripts/test.sh
```

Install or update the app after building:

```bash
./scripts/install.sh
# Custom build directory and optional user-local destination:
./scripts/install.sh /absolute/build/path "$HOME/Applications/7-Zip Mac.app"
# Focused installer/registration check on macOS:
python3 tests/finder-registration.py
```

The installer preserves a rollback copy, rejects downgrades, validates signatures/dependencies and registers the installed app. Build/package copies declare `LSHandlerRank=None`; use the installer to enable Finder opening. Existing default apps remain unchanged. See [registration and rollback](finder-registration.md).

Bootstrap downloads missing tools and official Qt/7-Zip dependencies into `~/.cache/7zip-mac-port`. It does not use sudo, install Homebrew itself or modify system Python. Install Command Line Tools with `xcode-select --install` if necessary and handle the macOS installer prompt. An existing matching Qt installation can be selected with `QT_PREFIX`.

The Cocoa plugin uses Qt private APIs, so mixing Qt versions is rejected. Its two ownership fixes and source provenance are documented in [qt-cocoa-fix.md](qt-cocoa-fix.md). Close the target application before building or packaging; scripts refuse to overwrite a running bundle.

On a local checkout, output is `build/7-Zip Mac.app`. On an SMB checkout, scripts select a local build directory to avoid filesystem signing/locking problems:

```text
~/.cache/7zip-mac-port/build/7-Zip Mac.app
```

```bash
./scripts/run.sh /absolute/path/to/example.7z
./scripts/run.sh --open-with /absolute/path/to/file.txt

# A separate, initially empty build directory:
PORT_BUILD_TESTS=ON ./scripts/build.sh "$HOME/.cache/7zip-mac-port/build-clean"
./scripts/test.sh "$HOME/.cache/7zip-mac-port/build-clean"
```

Direct CMake usage is also supported:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.11.3/macos" \
  -DSEVENZIP_BINARY="/path/to/source-built/7zz" \
  -DPORT_COCOA_PLUGIN_DIR="/path/to/patched-cocoa/build/plugins/platforms" \
  -DBUILD_TESTING=ON
cmake --build build
./scripts/package.sh build
./scripts/test.sh build
```

Run native GUI tests on an unlocked, logged-in Mac desktop. While locked, `./scripts/test.sh --no-focus` runs all registered independent suites, including external-editor write-back, F3 folder statistics, filesystem/Flat loading, file/ZIP comments, Open Inside modes, Open As, links, Properties and Cocoa ownership checks, leaving native focus/menu checks pending. Fixtures are confined to dedicated temporary directories. [test-results.md](test-results.md) records automated and actual GUI verification separately. The package includes Qt frameworks/plugins and the source-built 7zz; bundle checks reject development-prefix runtime dependencies and validate its ad-hoc signature.

The combined release gate runs the available suites, native Agent fixtures, all format registrations and isolated bundle startup once:

```bash
./scripts/verify-release.sh --no-focus /absolute/build/path
# Omit --no-focus on an unlocked desktop for native focus/AppKit checks.
```

Reports are stored in `test-results/release-*`; unavailable physical checks are reported separately.

Complete each source-defined implementation group before running its affected
suite group. Use the registered CTest names to limit a follow-up; the default
command still runs the complete suite. A filter matching no suites is an error.

```bash
PORT_TEST_REGEX='^(copy_workflow|address_workflow|panel_key)$' ./scripts/test.sh /absolute/build/path
```

Passing unrelated suites and the full format matrix are reused until a shared
implementation change requires checking them again. First failures remain in
the consolidated report, with application defects and test-driver errors
classified separately.

Additional format tests (cache-local optional generators, no global installation):

```bash
./scripts/bootstrap-fixture-tools.sh
./scripts/test-formats.sh
```

See [archive-formats.md](archive-formats.md) for fixture provenance, strict coverage checks and test limits.

## 日本語

```bash
./scripts/bootstrap.sh
./scripts/build.sh
./scripts/run.sh
# Optional developer regression suite / 開発者用テスト（任意）
PORT_BUILD_TESTS=ON ./scripts/build.sh
./scripts/test.sh
```

ビルド後のインストール・更新:

```bash
./scripts/install.sh
# 別のbuildディレクトリ、必要ならユーザー領域へのインストール:
./scripts/install.sh /absolute/build/path "$HOME/Applications/7-Zip Mac.app"
# インストール・登録の対象テスト:
python3 tests/finder-registration.py
```

最新版を同じ場所へ置き換え、旧版を保持し、Finderの候補を1つにします。古い版への上書きを拒否し、署名・依存先を検証します。開発・package用コピーは `LSHandlerRank=None` なので、Finder経由で使うときはinstallerを通してください。既定アプリは変更しません。[詳細と復元](finder-registration.md)。

`bootstrap.sh` はユーザー領域 `~/.cache/7zip-mac-port` に不足するツール、公式Qtバイナリ、公式7-Zipソースを取得します。sudo、Homebrew本体のインストール、システムPythonの変更を行いません。Apple Command Line Toolsがなければ `xcode-select --install` を実行し、OSの確認画面を操作してください。既存のQt 6.11.3を利用するときは `QT_PREFIX` を指定できます。Cocoa pluginがQt内部APIを使うため、異なるQtバージョンとの混用を禁止しています。

`build.sh` は公式QtBase対応ソースに上流の修正案とローカルのセル所有権修正を適用し、Cocoa pluginを再ビルドします。詳細は [Qtクラッシュ修正](qt-cocoa-fix.md)。ビルド・package前に対象 `.app` を終了してください。起動中のbundle更新はスクリプトが拒否します。

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
PORT_BUILD_TESTS=ON ./scripts/build.sh "$HOME/.cache/7zip-mac-port/build-clean"
./scripts/test.sh "$HOME/.cache/7zip-mac-port/build-clean"
```

CMakeを直接使用する場合は、公式ソースからビルドした `7zz` と修正版Cocoa pluginのディレクトリを指定します。後者は `scripts/build-qt-cocoa.sh` で生成します。通常は `scripts/build.sh` が両方を検出します。

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/path/to/Qt/6.11.3/macos" \
  -DSEVENZIP_BINARY="/path/to/source-built/7zz" \
  -DPORT_COCOA_PLUGIN_DIR="/path/to/patched-cocoa/build/plugins/platforms" \
  -DBUILD_TESTING=ON
cmake --build build
./scripts/package.sh build
./scripts/test.sh build
```

GUIテストはログイン済みMacのデスクトップで実行します。テストデータは専用一時ディレクトリに作成します。現在の一括確認は [release-consolidation.md](release-consolidation.md)、過去の実機確認は [test-results.md](test-results.md) に分けて記録しています。

上流ソースで定義される機能群をまとめて実装した後、影響するsuiteを一括確認します。修正後の再確認は、登録済みのCTest名で対象を絞れます。該当suiteがない指定はエラーにします。既定の `./scripts/test.sh` は全suiteを実行します。

```bash
PORT_TEST_REGEX='^(copy_workflow|address_workflow|panel_key)$' ./scripts/test.sh /absolute/build/path
```

変更のない形式matrix・他の合格済みsuiteは繰り返しません。初回の失敗は保存し、製品の不具合とテスト操作の不備を区別して記録します。
