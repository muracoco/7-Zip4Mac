#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Check Git identities without printing private addresses; configure this checkout only."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import sys

NOREPLY = re.compile(r'(?:[1-9][0-9]*\+)?[A-Za-z0-9][A-Za-z0-9_-]*(?:\[bot\])?@users\.noreply\.github\.com\Z')
IDENT = re.compile(r'<([^<>]+)> [0-9]+ [+-][0-9]{4}\Z')
TRAILER = re.compile(r'^(?:Co-authored-by|Signed-off-by):.*<([^<>]+)>\s*$', re.I | re.M)
PUBLIC_URL = 'https://github.com/muracoco/7-Zip4Mac.git'


def git(root, *args, input=None):
    result = subprocess.run(['git', '-C', str(root), *args], input=input,
                            capture_output=True, text=True)
    if result.returncode:
        # Git diagnostics can include identities, URLs or credential helpers.
        raise ValueError('Git operation failed: ' + args[0] + ' (details withheld)')
    return result.stdout.strip()


def safe(email, role='author'):
    return bool(NOREPLY.fullmatch(email)) or role == 'committer' and email == 'noreply@github.com'


def identity(root):
    for role in ('author', 'committer'):
        match = IDENT.search(git(root, 'var', 'GIT_' + role.upper() + '_IDENT'))
        if not match or not safe(match[1]):
            raise ValueError('Unsafe effective ' + role + ' email; configure your GitHub noreply identity and remove environment overrides')


def resolve(root, revision, suffix='^{commit}'):
    return git(root, 'rev-parse', '--verify', '--end-of-options', revision + suffix)


def commits(root, base, head):
    head = resolve(root, head)
    arguments = [head]
    if base:
        arguments += ['--not', resolve(root, base)]
    revisions = git(root, 'rev-list', *arguments).splitlines()
    failures = []
    for oid in revisions:
        _, author, committer, message = git(root, 'show', '-s', '--no-show-signature',
                                          '--format=%H%x00%ae%x00%ce%x00%B', oid).split('\0', 3)
        fields = [role for role, email in [('author', author), ('committer', committer)]
                  if not safe(email, role)]
        if any(not safe(match[1]) for match in TRAILER.finditer(message)):
            fields.append('contributor trailer')
        if fields:
            failures.append(oid[:12] + ': ' + ', '.join(fields))
    if failures:
        raise ValueError('Unsafe commit email metadata (values withheld):\n' + '\n'.join(failures))
    return len(revisions)


def tag_identity(root, oid):
    while git(root, 'cat-file', '-t', oid) == 'tag':
        headers = git(root, 'cat-file', '-p', oid).split('\n\n', 1)[0].splitlines()
        tagger = next((line[7:] for line in headers if line.startswith('tagger ')), '')
        match = IDENT.search(tagger)
        if not match or not safe(match[1]):
            raise ValueError('Unsafe tagger email: ' + oid[:12] + ' (value withheld)')
        oid = next(line[7:] for line in headers if line.startswith('object '))


def pre_push(root, stream):
    count = 0
    for line in stream:
        fields = line.split()
        if len(fields) != 4:
            raise ValueError('Invalid pre-push input')
        _, local_oid, _, remote_oid = fields
        if set(local_oid) == {'0'}:  # Ref deletion sends no new objects.
            continue
        tag_identity(root, local_oid)
        base = None if set(remote_oid) == {'0'} else remote_oid
        # Existing legacy metadata is reported by a full audit, not rewritten.
        # New refs have no baseline and therefore check their complete history.
        count += commits(root, base, local_oid)
    return count


def configure(root, name, email):
    if not NOREPLY.fullmatch(email) or not name.strip() or any(c in name for c in '\r\n<>'):
        raise ValueError('Provide a valid name and your GitHub noreply email')
    hook_dir = root / 'scripts/git-hooks'
    if any(not (hook_dir / name).is_file() or not os.access(hook_dir / name, os.X_OK)
           for name in ('pre-commit', 'pre-push')):
        raise ValueError('Executable project hooks are missing')
    current = subprocess.run(['git', '-C', str(root), 'config', '--get', 'core.hooksPath'],
                             capture_output=True, text=True)
    if current.returncode not in (0, 1):
        raise ValueError('Cannot inspect existing hook configuration')
    previous = current.stdout.strip()
    if previous:
        previous_dir = Path(previous)
        if not previous_dir.is_absolute():
            previous_dir = root / previous_dir
        if previous_dir.resolve() != hook_dir.resolve():
            raise ValueError('Existing hooksPath retained; integrate the privacy hooks manually')
    else:
        previous_dir = Path(git(root, 'rev-parse', '--git-path', 'hooks'))
        if not previous_dir.is_absolute():
            previous_dir = root / previous_dir
        if any(p.is_file() and os.access(p, os.X_OK) and not p.name.endswith('.sample')
               for p in previous_dir.glob('*')):
            raise ValueError('Existing custom hooks retained; integrate the privacy hooks manually')
    for key, value in [('user.name', name), ('user.email', email),
                       ('user.useConfigOnly', 'true'), ('core.hooksPath', 'scripts/git-hooks')]:
        git(root, 'config', '--local', key, value)
    identity(root)


def publish(root):
    """Publish the reviewed public main checkout, never private history or a force update."""
    if git(root, 'remote', 'get-url', 'origin').rstrip('/') not in {PUBLIC_URL, PUBLIC_URL[:-4]}:
        raise ValueError('Publication requires the dedicated 7-Zip4Mac public checkout')
    if git(root, 'symbolic-ref', '--short', 'HEAD') != 'main' or git(root, 'status', '--porcelain'):
        raise ValueError('Publication requires a clean main checkout with reviewed committed changes')
    identity(root)
    # Fetch the server baseline explicitly; a stale local tracking ref is insufficient.
    git(root, 'fetch', '--no-tags', 'origin', 'refs/heads/main')
    base, head = resolve(root, 'FETCH_HEAD'), resolve(root, 'HEAD')
    git(root, 'merge-base', '--is-ancestor', base, head)
    count = commits(root, base, head)
    if not count:
        return 'No new commits to publish'
    # No force or automatic merge. A concurrent upstream update is rejected by Git.
    git(root, 'push', 'origin', head + ':refs/heads/main')
    git(root, 'fetch', '--no-tags', 'origin', 'refs/heads/main')
    remote = resolve(root, 'FETCH_HEAD')
    if remote != head:
        raise ValueError('Remote main changed; inspect the new head before continuing')
    commits(root, base, remote)
    return f'Published and verified {count} new commit(s): {remote}'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    commands = parser.add_subparsers(dest='command', required=True)
    setup = commands.add_parser('configure', help='Set repository-local identity and opt-in hooks')
    setup.add_argument('--name', required=True)
    setup.add_argument('--email', required=True)
    commands.add_parser('identity', help='Check effective author and committer, including environment overrides')
    check = commands.add_parser('commits', help='Check new commits; omit --base for a full history audit')
    check.add_argument('--base')
    check.add_argument('--head', default='HEAD')
    commands.add_parser('pre-push', help='Read Git pre-push ref updates from stdin')
    commands.add_parser('publish', help='Fast-forward public main and verify actual remote metadata')
    args = parser.parse_args()
    try:
        root = Path(git(args.root.resolve(), 'rev-parse', '--show-toplevel'))
        if args.command == 'configure':
            configure(root, args.name, args.email)
            print('Repository-local noreply identity and privacy hooks configured')
        elif args.command == 'identity':
            identity(root)
            print('Effective author and committer use noreply identities')
        elif args.command == 'commits':
            print(f'Verified {commits(root, args.base, args.head)} commit(s); no unsafe email metadata')
        elif args.command == 'pre-push':
            print(f'Verified {pre_push(root, sys.stdin)} outgoing commit(s)')
        else:
            print(publish(root))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()
