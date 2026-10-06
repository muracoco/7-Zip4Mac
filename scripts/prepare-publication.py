#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Prepare a reviewed source snapshot locally; never publish or rewrite Git history."""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
from source_privacy import SECRET, USER_PATH, VOLUME_PATH, TEMP_PATH, normalized

specification = importlib.util.spec_from_file_location('source_archive', Path(__file__).with_name('source-archive.py'))
source_archive = importlib.util.module_from_spec(specification)
specification.loader.exec_module(source_archive)

REQUIRED = {'LICENSE', 'README.md', 'README.ja.md', 'CMakeLists.txt',
            'licenses/NOTICE.md', 'licenses/License.txt', 'licenses/copying.txt',
            'licenses/unRarLicense.txt', 'licenses/Qt/LGPL-3.0-only.txt',
            'licenses/Qt/GPL-3.0-only.txt', 'licenses/Qt/THIRD-PARTY-NOTICES.txt'}


def git(root, *arguments):
    return subprocess.check_output(['git', '-C', str(root), *arguments])


def inventory(root, revision, index):
    if index:
        records = git(root, 'ls-files', '--stage', '-z')
    else:
        records = git(root, 'ls-tree', '-rz', '--full-tree', revision)
    entries = {}
    for record in records.split(b'\0'):
        if not record:
            continue
        fields, raw_name = record.split(b'\t', 1)
        first, second, third = fields.split()
        mode, oid = (first, second) if index else (first, third)
        name = raw_name.decode('utf-8')
        path = PurePosixPath(name)
        if (mode not in {b'100644', b'100755'} or
                index and third != b'0' or not index and second != b'blob' or
                path.is_absolute() or '..' in path.parts or name in entries or
                not source_archive.permitted(name)):
            raise ValueError('Non-regular, conflicted or unsafe source entry: ' + name)
        entries[name] = (oid, 0o755 if mode == b'100755' else 0o644)
    if not REQUIRED.issubset(entries):
        raise ValueError('Required source/license files are missing')
    return entries


def blobs(root, entries):
    process = subprocess.Popen(['git', '-C', str(root), 'cat-file', '--batch'],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    try:
        for name, (oid, mode) in sorted(entries.items()):
            process.stdin.write(oid + b'\n')
            process.stdin.flush()
            header = process.stdout.readline().split()
            if len(header) != 3 or header[0] != oid or header[1] != b'blob':
                raise ValueError('Cannot read source blob: ' + name)
            size = int(header[2])
            data = process.stdout.read(size)
            if len(data) != size or process.stdout.read(1) != b'\n':
                raise ValueError('Incomplete source blob: ' + name)
            yield name, mode, data
    finally:
        process.stdin.close()
        process.stdout.close()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
    if process.returncode != 0:
        raise ValueError('Source reader failed')




def prepare(root, destination, revision, index=False):
    revision = git(root, 'rev-parse', '--verify', '--end-of-options',
                   revision + '^{commit}').decode().strip()
    entries = inventory(root, revision, index)
    contents, changes, failures = {}, [], []
    for name, mode, source in blobs(root, entries):
        if SECRET.search(source):
            failures.append({'path': name, 'kind': 'credential-pattern'})
            continue
        data = normalized(root, name, source)
        if USER_PATH.search(data) or VOLUME_PATH.search(data) or TEMP_PATH.search(data):
            failures.append({'path': name, 'kind': 'unreviewed-machine-path'})
            continue
        if data != source:
            changes.append(name)
        contents[name] = (mode, data)
    if failures:
        # Never disclose matched values or export a partly reviewed tree.
        print(json.dumps({'blocked': failures}, indent=2), file=sys.stderr)
        raise ValueError('Publication snapshot refused; no destination was created')
    # Exclusive creation protects an existing directory, symlink or user file.
    destination.mkdir(mode=0o700)
    manifest = {'schema': 1, 'files': []}
    for name, (mode, data) in contents.items():
        target = destination / name
        target.parent.mkdir(parents=True, exist_ok=True)
        with target.open('xb') as stream:
            stream.write(data)
        target.chmod(mode)
        manifest['files'].append({'path': name, 'mode': oct(mode),
                                  'sha256': hashlib.sha256(data).hexdigest()})
    report = {'schema': 1, 'baseCommit': revision, 'stagedIndex': index,
              'files': len(contents), 'normalizedDocumentation': changes,
              'credentialPatternCandidates': 0, 'remainingMachinePaths': 0,
              'originalNonDocumentationBytesRetained': True,
              'gitHistoryIncluded': False, 'published': False}
    for name, value in [('SOURCE-MANIFEST.json', manifest), ('PUBLICATION-REVIEW.json', report)]:
        target = destination / name
        with target.open('x', encoding='utf-8', newline='\n') as stream:
            stream.write(json.dumps(value, ensure_ascii=False, indent=2) + '\n')
        target.chmod(0o644)
    # Verify the actual written bytes and modes, not only the in-memory plan.
    for entry in manifest['files']:
        path = destination / entry['path']
        if (path.is_symlink() or path.stat().st_mode & 0o777 != int(entry['mode'], 8) or
                hashlib.sha256(path.read_bytes()).hexdigest() != entry['sha256']):
            raise ValueError('Snapshot verification failed: ' + entry['path'])
    destination.chmod(0o755)
    print(f'Local snapshot verified: {len(contents)} files; {len(changes)} documentation files normalized; no Git history or publication')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--revision', default='HEAD')
    parser.add_argument('--index', action='store_true', help='review the staged candidate instead of committed bytes')
    args = parser.parse_args()
    try:
        prepare(args.root.resolve(), args.destination.absolute(), args.revision, args.index)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, str(error) + '\n')
