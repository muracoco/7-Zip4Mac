#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Reject developer debris and verify the bundled build-source inventory."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import tarfile

from source_privacy import SECRET, USER_PATH, VOLUME_PATH, TEMP_PATH
from upstream_support import LOCK


def check(app):
    allowed = {'7-Zip Mac', '7zz', '7zz-progress', '7zip-comment'}
    if {p.name for p in (app / 'Contents/MacOS').iterdir()} != allowed:
        raise ValueError('Unexpected runtime executable inventory')
    for path in app.rglob('*'):
        if path.is_symlink():
            continue
        if (path.name in {'.git', '.DS_Store', '__pycache__', 'CMakeCache.txt'}
                or path.suffix in {'.log', '.pyc', '.o'}
                or path.name in {'tests', 'fixtures', 'QtTest.framework'}):
            raise ValueError('Developer output in bundle: ' + str(path.relative_to(app)))
    resources = app / 'Contents/Resources'
    official = resources / LOCK['release']['archive']
    if hashlib.sha256(official.read_bytes()).hexdigest() != LOCK['release']['sha256']:
        raise ValueError('Bundled official source archive mismatch')
    with tarfile.open(resources / '7zip-mac-port-src.tar.gz') as archive:
        manifest = json.load(archive.extractfile('SOURCE-MANIFEST.json'))
        if manifest.get('profile') != 'build':
            raise ValueError('Bundle must use the build source profile')
        names = {entry['path'] for entry in manifest['files']}
        required = {'CMakeLists.txt', 'scripts/build.sh', 'scripts/bootstrap.sh',
                    'scripts/upstream_support.py', 'upstream/7zip.json',
                    'licenses/NOTICE.md', 'qt-cocoa/accessibility-ownership.patch'}
        if not required.issubset(names) or set(archive.getnames()) != names | {'SOURCE-MANIFEST.json'}:
            raise ValueError('Incomplete or unexpected corresponding source inventory')
        for entry in manifest['files']:
            name = entry['path']; member = archive.getmember(name)
            if (not member.isfile() or PurePosixPath(name).is_absolute()
                    or '..' in PurePosixPath(name).parts or name.startswith('tests/')
                    or name.endswith(('.log', '.pyc'))):
                raise ValueError('Unsafe/developer source entry: ' + name)
            raw = archive.extractfile(member).read()
            if hashlib.sha256(raw).hexdigest() != entry['sha256']:
                raise ValueError('Corresponding source checksum mismatch: ' + name)
            if any(pattern.search(raw) for pattern in (SECRET, USER_PATH, VOLUME_PATH, TEMP_PATH)):
                raise ValueError('Unreviewed source privacy pattern: ' + name)
    print(f'Distribution check passed: four runtime executables; {len(names)} build-source files; no Port tests/logs/cache/private paths')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('app', type=Path)
    args = parser.parse_args()
    try:
        check(args.app)
    except (ValueError, OSError, KeyError, tarfile.TarError) as error:
        parser.exit(1, f'Distribution check failed: {error}\n')
