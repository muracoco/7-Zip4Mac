#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Exercise actual Git commits/hooks/pushes only in disposable local repositories."""
import importlib.util
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
NOREPLY = '123456+fixture-user@users.noreply.github.com'
PRIVATE = 'private-fixture@example.test'
ZERO = '0' * 40

sys.path.insert(0, str(ROOT / 'scripts'))
spec = importlib.util.spec_from_file_location('source_archive', ROOT / 'scripts/source-archive.py')
source_archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_archive)
privacy_spec = importlib.util.spec_from_file_location('git_privacy', ROOT / 'scripts/git-privacy.py')
privacy = importlib.util.module_from_spec(privacy_spec)
privacy_spec.loader.exec_module(privacy)


class PrivacyTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='7zip-git-privacy-')
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name) / 'checkout'
        self.repo.mkdir()
        self.env = {key: value for key, value in os.environ.items() if not key.startswith('GIT_') and key != 'EMAIL'}
        self.env.update(GIT_CONFIG_NOSYSTEM='1', GIT_CONFIG_GLOBAL=os.devnull,
                        GIT_TERMINAL_PROMPT='0', GIT_ASKPASS='/usr/bin/false')
        self.git('init', '-q', '-b', 'main')
        scripts = self.repo / 'scripts'
        scripts.mkdir()
        shutil.copy2(ROOT / 'scripts/git-privacy.py', scripts)
        shutil.copytree(ROOT / 'scripts/git-hooks', scripts / 'git-hooks')
        for hook in (scripts / 'git-hooks').iterdir():
            hook.chmod(0o755)
        self.configure()

    def run_command(self, args, success=True, overrides=None, input=None):
        result = subprocess.run(args, cwd=self.repo, input=input, text=True,
                                capture_output=True, env={**self.env, **(overrides or {})})
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def git(self, *args, **kwargs):
        return self.run_command(['git', *args], **kwargs).stdout.strip()

    def tool(self, *args, **kwargs):
        return self.run_command([sys.executable, str(self.repo / 'scripts/git-privacy.py'), *args], **kwargs)

    def configure(self, **kwargs):
        return self.tool('configure', '--name', 'fixture-user', '--email', NOREPLY, **kwargs)

    def commit(self, message='Fixture', email=NOREPLY, committer=None, hooks=True):
        args = ([] if hooks else ['-c', 'core.hooksPath=/dev/null']) + ['commit', '-q', '--allow-empty', '-m', message]
        self.git(*args, overrides={'GIT_AUTHOR_EMAIL': email, 'GIT_COMMITTER_EMAIL': committer or email})
        return self.git('rev-parse', 'HEAD')

    def assert_redacted(self, result):
        self.assertNotIn(PRIVATE, result.stdout + result.stderr)

    def test_configure_is_repo_local_and_invalid_config_retains_hooks(self):
        self.assertEqual(self.git('config', '--local', 'user.email'), NOREPLY)
        self.assertEqual(self.git('config', '--local', 'user.useConfigOnly'), 'true')
        self.assertEqual(self.git('config', '--local', 'core.hooksPath'), 'scripts/git-hooks')
        self.tool('configure', '--name', 'fixture-user', '--email', PRIVATE, success=False)
        self.assertEqual(self.git('config', '--local', 'user.email'), NOREPLY)
        self.git('config', '--local', 'core.hooksPath', 'other-hooks')
        self.configure(success=False)
        self.assertEqual(self.git('config', '--local', 'core.hooksPath'), 'other-hooks')

    def test_effective_author_and_committer_environment_overrides(self):
        self.tool('identity')
        for role in ('AUTHOR', 'COMMITTER'):
            result = self.tool('identity', overrides={'GIT_' + role + '_EMAIL': PRIVATE}, success=False)
            self.assert_redacted(result)

    def test_actual_pre_commit_blocks_explicit_author(self):
        result = self.run_command(['git', 'commit', '--allow-empty', '-m', 'Blocked',
                                   '--author', 'Fixture <' + PRIVATE + '>'], success=False)
        self.assert_redacted(result)
        self.git('rev-parse', '--verify', 'HEAD', success=False)

    def test_actual_pre_commit_blocks_committer_and_accepts_noreply(self):
        result = self.run_command(['git', 'commit', '--allow-empty', '-m', 'Blocked'],
                                  overrides={'GIT_COMMITTER_EMAIL': PRIVATE}, success=False)
        self.assert_redacted(result)
        self.commit()
        self.tool('commits')

    def test_existing_legacy_history_is_reported_but_new_commits_pass(self):
        old = self.commit(email=PRIVATE, hooks=False)
        self.commit()
        result = self.tool('commits', success=False)
        self.assert_redacted(result)
        self.assertIn('author, committer', result.stderr)
        self.tool('commits', '--base', old)
        self.tool('pre-push', input=f'refs/heads/main HEAD refs/heads/main {old}\n')

    def test_actual_pre_push_catches_bypassed_hook_and_retains_remote(self):
        bare = Path(self.temporary.name) / 'remote.git'
        self.run_command(['git', 'init', '-q', '--bare', str(bare)])
        self.git('remote', 'add', 'origin', str(bare))
        good = self.commit()
        self.git('push', '-q', 'origin', 'main')
        self.commit(email=NOREPLY, committer=PRIVATE, hooks=False)
        result = self.run_command(['git', 'push', 'origin', 'main'], success=False)
        self.assert_redacted(result)
        remote = self.run_command(['git', '--git-dir', str(bare), 'rev-parse', 'refs/heads/main']).stdout.strip()
        self.assertEqual(remote, good)

    def test_amend_retained_author_is_caught_before_push(self):
        self.commit(email=PRIVATE, hooks=False)
        result = self.run_command(['git', 'commit', '-q', '--amend', '--no-edit'], success=False)
        self.assert_redacted(result)
        self.git('-c', 'core.hooksPath=/dev/null', 'commit', '-q', '--amend', '--no-edit', '--allow-empty')
        result = self.tool('commits', success=False)
        self.assert_redacted(result)
        self.assertIn('author', result.stderr)

    def test_tag_and_contributor_trailer_checks(self):
        good = self.commit()
        self.git('tag', '-a', 'bad', '-m', 'Fixture tag', overrides={'GIT_COMMITTER_EMAIL': PRIVATE})
        tag = self.git('rev-parse', 'refs/tags/bad')
        result = self.tool('pre-push', input=f'refs/tags/bad {tag} refs/tags/bad {ZERO}\n', success=False)
        self.assert_redacted(result)
        self.commit(message='Fixture\n\nCo-authored-by: Fixture <' + PRIVATE + '>')
        result = self.tool('commits', '--base', good, success=False)
        self.assert_redacted(result)
        self.assertIn('contributor trailer', result.stderr)

    def test_new_ref_checks_complete_history_and_delete_is_empty(self):
        oid = self.commit(email=PRIVATE, hooks=False)
        result = self.tool('pre-push', input=f'refs/heads/new {oid} refs/heads/new {ZERO}\n', success=False)
        self.assert_redacted(result)
        self.tool('pre-push', input=f'(delete) {ZERO} refs/heads/main {oid}\n')

    def test_developer_tools_do_not_enter_build_source_or_scrub_credits(self):
        inventory = source_archive.sources(ROOT)
        for hook in ['scripts/git-hooks/pre-commit', 'scripts/git-hooks/pre-push']:
            self.assertEqual(inventory[hook], 0o755, 'Hook must be executable in the exported Git inventory')
        for name in ['AGENTS.md', 'scripts/git-privacy.py', 'scripts/git-hooks/pre-commit',
                     'scripts/git-hooks/pre-push', 'tests/git-privacy.py', 'docs/git-privacy.md']:
            self.assertTrue(source_archive.permitted(name), name)
            self.assertFalse(source_archive.build_source(name), name)
        from source_privacy import checked
        original = b'Copyright upstream contributor <upstream@example.test>\n'
        self.assertEqual(checked(self.repo, 'licenses/credits.txt', original), original)

    def test_publish_refuses_unrelated_remote_and_dirty_tree(self):
        self.commit()
        self.git('remote', 'add', 'origin', 'https://example.test/other.git')
        self.tool('publish', success=False)
        self.git('remote', 'set-url', 'origin', 'https://github.com/muracoco/7-Zip4Mac.git')
        self.tool('publish', success=False)  # Untracked copied tools: no network mutation.

    def test_publish_checks_actual_local_remote_with_legacy_baseline(self):
        bare = Path(self.temporary.name) / 'remote.git'
        self.run_command(['git', 'init', '-q', '--bare', str(bare)])
        self.git('remote', 'add', 'origin', str(bare))
        self.commit(email=PRIVATE, hooks=False)
        self.git('-c', 'core.hooksPath=/dev/null', 'push', '-q', 'origin', 'main')
        self.git('add', 'scripts')
        good = self.commit(message='Reviewed source with noreply metadata')
        # Substitute only the test destination; real Git performs fetch/push/verification.
        with patch.object(privacy, 'PUBLIC_URL', str(bare)), patch.dict(os.environ, self.env, clear=True):
            result = privacy.publish(self.repo)
        self.assertIn(good, result)
        metadata = self.run_command(['git', '--git-dir', str(bare), 'show', '-s',
                                     '--format=%ae%x00%ce', good]).stdout.strip().split('\0')
        self.assertEqual(metadata, [NOREPLY, NOREPLY])


if __name__ == '__main__':
    unittest.main(verbosity=2)
