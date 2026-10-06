# Qt Cocoaのクラッシュ修正

2026-10-03、macOS 26.6.2 / arm64 / Qt 6.11.3の実機で発生した問題への対応。7-Zipエンジンを変更したものではない。

## 確認した原因

アプリ本体のクラッシュは、行を選択した一覧のmacOSアクセシビリティ読み取り中に発生した。スタック先頭は `QMacAccessibilityElement::accessibilitySelectedChildren`（同梱Cocoa plugin +554048）で、AppKit / AXCopyHierarchyから呼び出されていた。ファイル圧縮・展開処理のスタックではない。

Qtの合成行・列・セルは親テーブルのaccessibility IDを共有する。ネイティブ表現の解放時にそのIDのQt interfaceまで削除する処理があり、残った親ポインターへのアクセスでクラッシュした。上流の関連変更は [Qt commit b1ed5f6](https://github.com/qt/qtbase/commit/b1ed5f656f064e553b33752f8e87d2f5b9553e38)。公式ソースと実機スタックを照合した。

[Qt Gerrit 765434 / QTBUG-149612の修正案](https://codereview.qt-project.org/c/qt/qtbase/+/765434)を `qt-cocoa/accessibility-ownership.patch` として保持する。取得時点では未mergeの候補であり、公式リリース済み修正とは扱わない。対象revision: `c7fd3f34b997bb363be15650647665b3b6b8a5f4`。SHA-256: `f3409a797a4d68146f040749f099e975957f94a029b328f7242d860fd31dcda3`。

この候補のみを適用した検証プログラムでは、一覧の列数を減らしたときに別のクラッシュが発生した。2026-10-03 22:57と22:58の `cocoa_accessibility_tests` 報告は、この検証のもの。QtWidgets `QAccessibleTable::modelChange` が、Cocoa側で削除済みのセルIDを `childToId` から参照していた。繰り返し実行を止め、寿命の事前検査を追加した。検証はテスト用一覧のみで、ユーザーファイルの圧縮・削除操作を行っていない。

## 適用した変更

- 上流候補: 親が管理する合成要素の破棄で親interfaceを削除しない。
- ローカル追加patch: Qt Widgetsのitem viewが保持するCell / ListItem / TreeItemのinterfaceをCocoa側から独立に削除しない。Qt Widgets自身のmodel change / destructionに寿命を任せる。QtQuick等の他の要素にはこの追加条件を適用しない。
- 公式QtBase 6.11.3のCocoa pluginをApple Clang / CMake / Ninjaでビルドし、同じバージョンの公式frameworkに動的リンクする。Qt内部APIのためバージョンは固定する。
- packageで `Contents/PlugIns/platforms/libqcocoa.dylib` を修正版へ置換し、rpathをbundle内へ直してad-hoc署名する。開発用Qtのframeworkは変更しない。
- ビルド・packageで起動中の対象bundleの上書きを拒否する。

## 回帰検証

`tests/cocoa-accessibility.mm` は自分の検証プロセス内でCocoaのnativeアクセシビリティAPIを呼ぶ。親interfaceとWidgetsセルの所有権を最初に検査し、失敗時はクラッシュを誘発する列変更へ進まず通常の失敗終了にする。その後、選択行の読み取りを伴う列変更20回と、9列の一覧clear / 再構築20回を検査する。表示外セルはCocoaで無視されるため、9列を表示できるウィンドウ幅とレイアウト完了を確保して数を検査する。

`PORT_TEST_PLUGIN_ROOT` によりQtTestとnative検証双方が修正版pluginを明示的に読み込む。単に `QT_QPA_PLATFORM_PLUGIN_PATH` を指定すると、公式framework側の元pluginが先に選ばれたため、QApplication構築前にlibrary pathを設定する。

実機 `.app` では、以前クラッシュした「行選択 → F2 Rename → Cancel → native画面読み取り」、Ctrl+R、F7画面のCancel、暗号化7zの内部一覧／選択／Testを再確認した。全テストとクリーンビルドの確定記録は [test-results.md](test-results.md)。全macOS版・全アクセシビリティ製品に対する保証ではなく、このMacで再現した経路に対する修正・検証である。

## 再ビルドとライセンス

`./scripts/build.sh` が取得・patch・pluginビルド・bundle配置を行う。plugin単独は `./scripts/build-qt-cocoa.sh`。元のQtヘッダー／著作権表示を保持し、変更はLGPL条件で提供する。未改変公式QtBase archive、修正patch、対応ビルドスクリプトを `.app/Contents/Resources` のソースarchiveに含める。[NOTICE](../licenses/NOTICE.md)を参照。

公開用コピーからのビルドで、共通キャッシュのCMake source pathが元のcheckoutを指す問題を確認しました。プロジェクト定義をキャッシュ内の固定位置へコピーし、patch・プロジェクト定義・Qtの場所・architectureでキャッシュを識別します。元のキャッシュは保持し、異なるcheckoutやGitを含まない対応ソースからも同じビルド手順を利用できます。
