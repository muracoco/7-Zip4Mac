# User guide / 使い方

[English README](../README.md) · [日本語README](../README.ja.md)

[English](#english) · [日本語](#日本語)

## English

### Browse files and archives

Launch **7-Zip Mac** to open the File Manager. Enter a folder path in the address bar and press Enter to navigate.
Double-click folders to open them; use the Up button or Backspace to return to the parent folder.

Double-click a `.7z`, `.zip`, or other archive to browse it without extracting everything.
You can also open folders and nested archives inside it.
To open an archived file in another application, use **File → Open Outside**.
The file is extracted to a temporary folder first.

Use Ctrl-click or Shift-click to select multiple items. Click a column heading to sort the list.
The **View** menu controls display modes and columns. **Flat View** shows subfolder contents together;
**2 Panels** displays two locations side by side. Save frequently used locations in **Favorites**.

### Create an archive

1. Select the files or folders to compress.
2. Click **Add** on the toolbar.
3. Enter the archive's filename and destination, and choose a format.
4. Adjust the compression settings if needed, then click **OK**.

| Format | What you can create |
|---|---|
| 7z, ZIP | Archives containing multiple files and folders; password protection is available. |
| TAR, WIM | Archives containing multiple files and folders. |
| XZ, gzip, bzip2 | A compressed stream from one regular file. |

RAR can be opened and extracted, but cannot be created.
See the [format coverage](archive-formats.md) for other readable formats and tested variants.

The Add dialog provides compression level, method, dictionary size, solid block size, CPU threads,
update mode, path mode, and volume sizes. Unavailable controls are disabled for the chosen format and method.
The dialog's **Options** button provides timestamp and supported link settings.

### Passwords and encryption

Choose 7z or ZIP in the Add dialog, then enter and confirm a password.
For 7z, enable **Encrypt file names** to hide the names as well as the contents.
ZIP offers AES-256 and ZipCrypto; ZIP passwords are restricted to ASCII characters.

When opening, testing, or extracting an encrypted archive, enter the password when requested.
Passwords are not saved in preferences or written to logs.

### Extract and test

Select an archive and click **Extract**. Choose a destination, then review how folder paths and existing files will be handled.
You can preserve paths, omit them, or request absolute paths. Absolute-path extraction requires confirmation.
Existing files can prompt, be overwritten, be skipped, or be renamed automatically.

To extract only part of an archive, open it, select the items you need, and click **Extract**.
Click **Test** to check the selected archive or selected items inside it for corruption.
The result reports success or the errors found. Test does not save extracted files.
The progress dialog shows the current file and progress and offers **Pause** and **Cancel**.

### Use Finder's action menu

Right-click a file in Finder and choose **Open With → 7-Zip Mac**, then choose an action:

| Action | What it does |
|---|---|
| Extract files… | Lets you choose the destination and extraction settings. |
| Extract Here | Extracts into the archive's folder. |
| Extract to "name/" | Extracts into a subfolder named after the archive. |
| Test archive | Checks the archive for corruption. |
| Add to archive… | Opens the Add dialog. |
| Add to "name.7z" / "name.zip" | Compresses using that format's saved compression settings. |
| CRC SHA | Calculates checksums or hashes. |
| Open in 7-Zip File Manager | Opens archive contents, or selects an ordinary file in its parent folder. |

The quick 7z/ZIP actions do not reuse password, volume-splitting, or delete-after settings.
For password protection or split volumes, choose **Add to archive…**.

The actions shown depend on the selected files and your settings.
Choose which actions to show in **Tools → Options → 7-Zip**.
The File Manager action always appears at the bottom.
This menu opens in the app's own window; a direct Finder right-click submenu is not yet implemented.

### Files, settings, and other tools

- **Copy / Move:** Select items and use the toolbar buttons to choose a destination. With two panels,
  you can transfer files to the other panel, including into a writable archive.
- **Delete:** Ordinary files go to the macOS Trash. Deleting items inside an archive updates the archive after confirmation.
- **Rename / New Folder / Properties:** Use the File menu or the item's right-click menu.
- **View / Edit:** For files, F3 and F4 use the applications configured under **Tools → Options → Editor**.
  After an archived file is edited, the app asks whether to save changes back to the archive.
  F3 on an ordinary folder calculates its size and file count.
- **Split / Combine:** Split an ordinary file into numbered parts, or combine an existing set of parts.
- **CRC SHA:** Calculate checksums and hashes, including SHA-256, for files or archive contents.
- **Benchmark:** Use **Tools → Benchmark** to measure compression performance.
- **Help:** Open the bundled official English 7-Zip help.

**Tools → Options** changes language, file-list behavior, working folders, external applications, and Finder menu items.
**Apply** saves without closing the dialog; **OK** saves and closes it; **Cancel** discards changes made since the last Apply.
Some app-specific explanations remain in English even when another language is selected.

Shortcuts retain Windows-style Control keys. F2 renames, F5 copies, F6 moves, and F7 creates a folder.
Ctrl+C copies selected filenames as text; Ctrl+X and Ctrl+V do not transfer files.
Depending on macOS keyboard settings, function keys may require Fn.
See the [Windows comparison](windows-parity.md) for limitations.

## 日本語

### ファイルやアーカイブを見る

7-Zip Macを起動すると、ファイルマネージャーが開きます。
アドレスバーにフォルダーのパスを入力し、Enterを押すと移動できます。
フォルダーはダブルクリックで開き、上へ移動するボタンやBackspaceで1つ上の階層へ戻ります。

`.7z`や`.zip`をダブルクリックすると、全体を展開せずに中身を確認できます。
アーカイブ内のフォルダーや、その中にある別のアーカイブも開けます。
中のファイルを別のアプリで開くときは、「ファイル → 関連付けで開く（Open Outside）」を使います。
必要なファイルを一時フォルダーに取り出してから開きます。

Ctrlを押しながらクリックすると複数の項目を選べます。Shiftを押しながらクリックすると範囲を選べます。
列の見出しをクリックすると、名前やサイズなどで並べ替えられます。
「表示」メニューでは、表示方法や列を変更できます。
「フラット ビュー」はサブフォルダーの中身をまとめて表示し、「2 分割画面」は2つの場所を並べて表示します。
よく使う場所は「お気に入り」に登録できます。

### 圧縮する

1. 圧縮したいファイルやフォルダーを選びます。
2. ツールバーの「追加（Add）」を押します。
3. 作成するアーカイブの名前、保存先、形式を指定します。
4. 必要に応じて圧縮設定を変え、「OK」を押します。

| 形式 | 作成できるもの |
|---|---|
| 7z・ZIP | 複数のファイルやフォルダーをまとめたアーカイブ。パスワードも設定できます。 |
| TAR・WIM | 複数のファイルやフォルダーをまとめたアーカイブ。 |
| XZ・gzip・bzip2 | 通常ファイル1つを圧縮したもの。 |

RARは開いたり展開したりできますが、作成はできません。
そのほかの読み取り形式と、テストした範囲は[形式対応表](archive-formats.md)にあります。

圧縮レベル、圧縮方式、辞書サイズ、ソリッドブロックのサイズ、CPUスレッド数などを指定できます。
更新方法、保存するパス、分割するサイズも選べます。
選んだ形式や圧縮方式で使えない設定は無効になります。
ダイアログ内の「オプション」では、日時の保存方法や、対応する形式でのリンクの保存方法を設定できます。

### パスワードを設定する

圧縮するときに7zかZIPを選び、パスワードを入力して、確認用にもう一度入力します。
7zで「ファイル名を暗号化」を有効にすると、ファイル名もパスワードなしでは見えなくなります。
ZIPではAES-256とZipCryptoを選べます。ZIPのパスワードに使える文字は半角英数字と記号です。

暗号化されたアーカイブを開く・テストする・展開するときは、入力を求められたらパスワードを入力します。
パスワードは設定に保存せず、ログにも記録しません。

### 展開する・破損を調べる

アーカイブを選んで「展開（Extract）」を押し、保存先を指定します。
続いて、フォルダー構成を残すか、同名のファイルがあったときにどうするかを選びます。
フォルダー構成を省いて取り出すこともできます。絶対パスを使う展開には確認が必要です。

同名のファイルがある場合は、確認する・上書きする・スキップする・自動で名前を変える、から選べます。
一部のファイルだけ必要なときは、アーカイブを開いて対象を選び、「展開」を押してください。

破損を調べるには、アーカイブか、その中の項目を選んで「テスト（Test）」を押します。
成功したか、どんなエラーがあったかを結果画面に表示します。テストでは展開先にファイルを保存しません。
時間のかかる処理では進捗画面が開き、処理中のファイルや進み具合を確認できます。
「一時停止（Pause）」や「キャンセル（Cancel）」も使えます。

### Finderから操作する

Finderでファイルを右クリックし、「このアプリケーションで開く → 7-Zip Mac」を選びます。
表示されたウインドウで、次のような操作を選べます。

| 操作 | 内容 |
|---|---|
| 展開… | 保存先や展開方法を選びます。 |
| ここに展開 | アーカイブと同じフォルダーに展開します。 |
| 「名前/」に展開 | アーカイブ名のサブフォルダーを作って展開します。 |
| アーカイブをテスト | 破損がないか調べます。 |
| 圧縮…（Add to archive…） | 圧縮設定の画面を開きます。 |
| 「名前.7z」「名前.zip」に圧縮 | その形式の保存済み圧縮設定を使って圧縮します。 |
| CRC SHA | チェックサムやハッシュ値を計算します。 |
| 7-Zip ファイルマネージャーで開く | アーカイブの中身を表示します。通常ファイルの場合は、親フォルダーでそのファイルを選びます。 |

「名前.7z」「名前.zip」に圧縮する操作では、パスワード・分割・圧縮後の削除設定は引き継ぎません。
パスワードや分割を指定する場合は、「圧縮…（Add to archive…）」を選んでください。

選んだファイルや設定によって、表示される操作が変わります。
「ツール → オプション → 7-Zip」で、表示する操作を選べます。
ファイルマネージャーで開く項目は、常に一番下に表示します。
このメニューはアプリ側のウインドウで開きます。Finderの右クリックへ直接追加する機能はまだありません。

### ファイル操作・設定・その他の機能

- コピー・移動: 対象を選び、ツールバーのボタンから保存先を指定します。
  2分割画面では、もう一方の場所へ転送できます。書き込み可能なアーカイブへの追加もできます。
- 削除: 通常ファイルはmacOSのゴミ箱に移します。アーカイブ内の項目は、確認してからアーカイブを更新して削除します。
- 名前変更・新しいフォルダー・プロパティ: ファイルメニューや右クリックメニューから操作します。
- 表示・編集: ファイルを選んでF3・F4を押すと、「ツール → オプション → 外部ツール」で指定したアプリを使います。
  アーカイブ内のファイルを編集した後は、変更を書き戻すか確認します。
  通常フォルダーでF3を押すと、サイズやファイル数を集計します。
- ファイル分割・結合: 通常ファイルを連番のファイルに分割したり、分割済みのファイルを結合したりできます。
- CRC SHA: 通常ファイルやアーカイブの中身について、SHA-256などのチェックサム・ハッシュ値を計算できます。
- ベンチマーク: 「ツール → ベンチマーク」で圧縮性能を測定します。
- ヘルプ: 同梱している公式7-Zipの英語ヘルプを開きます。

「ツール → オプション」では、表示言語、一覧の動作、作業用フォルダー、外部アプリ、Finderの操作メニューを設定できます。
「適用（Apply）」は画面を閉じずに保存し、「OK」は保存して閉じます。
「キャンセル」は最後の適用以降に行った変更を取り消します。
日本語を選んでも、このアプリ独自の説明の一部は英語で表示されます。

ショートカットはWindows版と同じくControlキーを使います。
F2は名前変更、F5はコピー、F6は移動、F7は新しいフォルダーの作成です。
Ctrl+Cは選んだファイル名を文字列としてコピーします。Ctrl+X・Ctrl+Vではファイルを転送しません。
macOSのキーボード設定によっては、Fキーを使う際にFnキーも押す必要があります。
詳しい制限は[Windows版との比較](windows-parity.md)を参照してください。

## Implementation and verification records / 実装・検証の記録

Implementation details and test limits are in the [current inventory](current-status.md),
[Windows comparison](windows-parity.md), and [test results](test-results.md).
Version changes are in the [changelog](../CHANGELOG.md).

移植したコードや実際の確認範囲は、[実装一覧](current-status.md)、[Windows版との比較](windows-parity.md)、
[テスト結果](test-results.md)に記載しています。バージョンごとの変更は[更新履歴](../CHANGELOG.ja.md)を参照してください。
