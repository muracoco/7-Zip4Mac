# Commit email privacy / コミットメールの公開対策

## 公開履歴の対策完了、2026-10-06

公開 `main` を `68378540425daaf2fef8903012686b888951bf20` へ差し替え、既存8コミットの作者・コミッターの個人メールをGitHub noreplyへ変更しました。履歴の親関係・名前・日時・メッセージ・ファイルmodeを保持し、文書内の公開祖先SHA参照も更新しました。公開到達履歴に対象メールがないことを再検査しています。

GitHubアカウントのメール非公開と個人メールpush拒否、公開6リポジトリのSecret scanning / Push protectionを有効にしました。この共有作業コピーの私的開発履歴は保持し、公開準備に必要なprivacy差分だけを反映しました。公開時は引き続き専用checkoutとnoreplyメタデータ検査を使います。

旧SHAで取得できるGitHubキャッシュは残存を確認しており、Supportへの削除依頼が別途必要です。以下の点検・プレビュー記録は公開履歴差し替え前の経緯を示します。

## Findings, 2026-10-06

The scope is **muracoco/7-Zip4Mac only**. At public main
`09ed688826505581fe29a072a65ffbcc579146cc`, the one branch/no tags contain
eight commits with the same personal email in both author and committer fields.
An anonymous fresh clone reproduced the finding. The reported address was absent
from all 1,018 unique historical file blobs checked. No address is repeated here.
Other repositories, account settings and deleted/unreachable objects were not
inspected or changed. This is a targeted email check, not a new full security audit.

The publication checkout had a noreply configuration, but the GitHub connector
created server-side commits without explicit author/committer fields. GitHub's
Git Commit API defaults those fields to the authenticated user. Local Git config
does not affect that API. Earlier tree-byte verification missed this metadata.
See [GitHub's commit API](https://docs.github.com/en/rest/git/commits#create-a-commit)
and [commit email configuration](https://docs.github.com/en/account-and-profile/how-tos/email-preferences/setting-your-commit-email-address).

## Prevention

Configure each development/publication checkout explicitly with your own
[GitHub noreply address](https://docs.github.com/en/account-and-profile/reference/email-addresses-reference):

```sh
python3 scripts/git-privacy.py configure --name YOUR_LOGIN --email YOUR_NOREPLY_EMAIL
python3 scripts/git-privacy.py identity
python3 tests/git-privacy.py
```

This changes only repository-local `user.name`, `user.email`, `user.useConfigOnly`
and `core.hooksPath`. Existing custom hooks are retained by refusing automatic
replacement; integrate the supplied hook commands manually if needed. Build and
bootstrap scripts do not install hooks or change Git/global account settings.

- The pre-commit hook checks effective author **and** committer, including
  environment overrides and `--author`. Pre-push checks actual outgoing commit
  metadata, contributor trailers and annotated taggers, including commits that
  bypassed pre-commit. Diagnostic errors withhold detected addresses.
- Valid GitHub noreply identities are accepted, including legacy/bot addresses;
  GitHub's `noreply@github.com` is accepted only for a server-side committer.
  Original upstream author/contact/license text is preserved.
- A full-history check is `python3 scripts/git-privacy.py commits`. For a normal
  update, use `commits --base BASE --head HEAD`: already published legacy commits
  are reported separately, not silently rewritten. A new ref without a baseline
  checks its full history. Missing baseline objects require fetching first.
- Reviewed public-source exports still use `prepare-publication.py`. Commit them
  in a separate checkout descended from public `main`, never the private history.
  Once reviewed and committed with noreply metadata, publish with
  `python3 scripts/git-privacy.py publish`. It fetches the actual baseline,
  requires a clean fast-forward main, checks both outgoing email fields, pushes
  without force and verifies the fetched remote metadata afterward. Git CLI
  authentication is required. No GitHub Actions or paid service is needed.
- GitHub API/connector operations which create commits without explicit,
  verified noreply fields must not be used as an authentication fallback. The
  committed `AGENTS.md` records this rule for subsequent work.

Hooks are local opt-in safeguards, not server-side enforcement: `--no-verify`,
another checkout or a direct API can bypass them. The publication procedure and
post-push check are required even when hooks are installed. The hooks, instructions
and regression fixtures are developer files, excluded from the app's build-source
profile and not required to build/use the application.

## Past history and verification

Changing config affects future commits; at the original audit checkpoint, all
eight commits still contained the address. `.mailmap` does not remove original
metadata. A separate history repair has since replaced the public history.
A fresh public clone at `68378540425daaf2fef8903012686b888951bf20`
passed the author/committer check for all nine reachable main commits.
The prevention scripts do not rewrite public refs. Existing clones of the old
history must synchronize with its replacement; old URLs, caches and third-party
copies may remain. Deleted/unreachable objects were not checked, and no complete
erasure is promised. Use a fresh public checkout before publishing further work.

Local regression checks use disposable repositories and actual Git hooks/pushes.
`python3 tests/git-privacy.py` passed **12/12** cases on the working Mac.
They cover private author/committer overrides, explicit authors, amended legacy
authors, bypassed hooks, outgoing commit rejection with the remote retained,
legacy baselines/new refs, taggers/trailers, custom-hook protection, third-party
credits and exclusion from the app's build-source archive. Application/runtime
code is unchanged, so previous GUI/format verification is not rerun for this fix.
The first amend-fixture expectations were corrected: the pre-commit hook already
rejects a retained private author, and deliberately bypassing it for a test of
pre-push requires `--allow-empty` for an empty fixture commit.
The SMB working checkout does not infer Git executable modes reliably. Both
hooks are explicitly recorded as `100755`; the regression also checks their
exported inventory modes so a fresh clone can enable them without manual chmod.

## 日本語

公開8コミットの作者・コミッター欄に、報告された個人メールがあることを確認しました。
公開履歴内のファイル本文1,018個には同じメールは見つかりませんでした。
原因は、手元のnoreply設定がGitHub APIのコミット作成に適用されないことです。
今後はこのプロジェクトだけのGit設定、commit/push時の検査、公開後の実メタデータ確認で
防ぎます。他のリポジトリやグローバル設定、上流の作者・ライセンス表記は変更しません。
履歴修正は再発防止とは別に実施され、上記の公開mainの9コミットで作者・コミッターの
noreplyを確認しました。旧URL・cache・第三者copyまでの完全消去は確認していません。
