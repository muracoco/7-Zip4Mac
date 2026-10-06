# Project instructions

- Report to the owner in Japanese, with the result first. Preserve existing work
  and original third-party license/author notices.
- Before a project commit, configure this checkout's own GitHub noreply identity
  with `python3 scripts/git-privacy.py configure --name NAME --email NOREPLY`.
  Check both effective author and committer. Do not change global Git or account
  settings. Never print private addresses or credentials in reports.
- Publish reviewed sources from a separate checkout of the public repository.
  Never push the internal development history. Use
  `python3 scripts/git-privacy.py publish` after reviewing/staging/committing the
  public snapshot. It permits a normal fast-forward only and checks actual
  outgoing and fetched remote author/committer metadata, not just tree equality.
- Do not create commits with a GitHub connector/API that cannot explicitly set
  and verify both author and committer noreply identities. Local Git config does
  not control API-created commits. File-creation/update and merge APIs can also
  create commits. If safe publishing credentials are unavailable, finish local
  implementation/tests/commits and report the authentication requirement.
- Do not rewrite published history, force-push or delete refs without explicit
  authorization for that operation. Existing private metadata needs a separate
  approved repair; `.mailmap` or new config does not erase old commits.
- Run `python3 tests/git-privacy.py` for identity/publishing policy changes.
  These developer tools/tests are excluded from the app's build-source archive.
