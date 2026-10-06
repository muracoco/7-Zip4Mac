# Security and public-source privacy

Keep API credentials, private keys, local configuration and private diagnostic
records out of the public source. Use environment variables for credentials;
only empty or clearly synthetic examples belong in documentation and tests.
Retain upstream copyright notices and attribution.

Before committing to this repository, set the maintainer's public commit identity:

```sh
git config user.email "207765797+muracoco@users.noreply.github.com"
git var GIT_AUTHOR_IDENT
git var GIT_COMMITTER_IDENT
```

Inspect staged changes and commit metadata before publishing. GitHub email
privacy and push protection should remain enabled. A new email setting affects
future commits only; changing `.gitignore` does not remove tracked data or history.
After a privacy history rewrite, use a fresh public clone rather than merging
or force-pushing an older public clone. Keep private recovery backups offline.

Report suspected exposures privately to the maintainer. If a credential is
exposed, revoke or rotate it before removing the affected files and history.
