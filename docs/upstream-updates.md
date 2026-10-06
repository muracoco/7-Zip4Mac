# Incremental upstream updates / 上流の差分更新

The official source tree stays outside the repository and is never edited by
the Port. `upstream/7zip.json` pins the release URL/archive checksum, all 1,292
official source-file checksums, importer inputs and historical adaptation dates.
The 25 importers read this lock instead of maintaining separate release hashes.
Build/bootstrap/package scripts also use its version and archive name. Generated
Qt adapters and the helper overlay remain separate from unmodified upstream.

## Compare and stage a new stable release

Download the source archive from the official [7-Zip releases](https://github.com/ip7z/7zip/releases).
Check that it is a stable release and confirm the publisher/download URL and its
checksum. A checksum supplied to the tool only verifies those bytes; it does not
authenticate an arbitrary download.

```bash
source scripts/env.sh
python3 scripts/upstream.py compare \
  --source "$SEVENZIP_SOURCE" --archive /path/to/7zNNNN-src.tar.xz \
  --sha256 VERIFIED_SHA256 --output /path/to/new-review-directory

SEVENZIP_UPSTREAM_LOCK=/path/to/new-review-directory/candidate-lock.json \
python3 scripts/upstream.py stage \
  --source /path/to/new-review-directory/source \
  --output /path/to/new-staging-directory
```

Both commands require a new directory outside the checkout. They do not change
the accepted lock, installed app or user files. Archive traversal, links,
duplicate entries and wrong checksums are rejected. The comparison produces a
full source diff, changed-file inventory, affected importers and a candidate
lock. Staging regenerates all 55 adapters plus the modified helper overlay.
Changed anchors or resource schemas stop generation; accepting new hashes alone
does not prove compatible behavior.

Review only the changed upstream groups and their dependent Port adapters. If
necessary, adjust those importers; there is no need to repeat the entire port.
Review codec/handler interfaces and makefiles even when no GUI importer changed.
Review licenses, UI resources, Lang files and toolbar/icon assets when their
upstream paths changed. RC preprocessing has transitive headers: the affected
importer list is a direct-input guide, not a substitute for build verification.

After that review, copy the candidate lock to `upstream/7zip.json`, copy reviewed
generated files from the staging `port/` tree, and update each changed group's
`adaptation.last` to the actual modification date. Put the verified archive in
the user-local dependency cache, then run bootstrap/build against the new pin.
Use a clean build directory: original engine objects and overlay outputs are
isolated by release version, and the Qt patch cache is separate.

`import-help.py` uses a separately pinned official Windows CHM. Obtain/review the
matching Windows help package and update `artifacts.windows-help` and Help
resources separately. CHM, imported original assets and license documents are
not silently replaced by source staging. Update their version notices and the
English/Japanese READMEs before publishing. Keep original license headers and
the unRAR restriction intact. Qt updates require their own patch/source review;
this workflow does not automatically upgrade Qt.

For changed compression/extraction/Agent paths, build with
`PORT_BUILD_TESTS=ON` and run the relevant regression groups plus real 7z/ZIP
round trips, encryption and Cancel. For a codec/format change, regenerate
`resources/formats.json` with `update-formats.py` after rebuilding both helpers,
then run format coverage. Finish with the app bundle dependency/signature/source
inventory checks. Commit the reviewed source diff and lock together. The tools
never push, tag or publish a Release.

## 日本語

上流の公式ソースは編集せず、移植側のアダプターとオーバーレイを分離しています。
バージョン・入手先・ハッシュは `upstream/7zip.json` に集約しました。
候補版の比較と再生成は上のコマンドで別ディレクトリに出力します。
変更されたソース群と依存するアダプターを確認・修正してから、候補の lock と
生成物を採用します。ハッシュだけ更新して互換性を確認済みにはしません。

既存の55アダプター全体を毎回作り直す作業は不要ですが、上流のAPI・リソース・
処理手順が変わった箇所にはレビューと修正が必要です。公式のHelp、翻訳、画像、
ライセンス文書、Qtの更新は、それぞれの変更がある場合に確認します。
変更群のテストと7z/ZIPの実ファイル確認を行い、lockとコードを一緒にcommitします。

## Validation of this workflow

The current official 26.03 archive and its entire source inventory are the
baseline. All staged adapter bodies match the previous checked-in bodies after
removing the three new modification-notice lines. Candidate-update tests use
clearly synthetic local archives; they do not claim that a future upstream
release has been validated. See [maintenance verification](maintenance-verification.md).
