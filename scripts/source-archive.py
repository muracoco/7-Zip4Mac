#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Package tracked working sources, never ignored/untracked build or private files."""
import argparse
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import subprocess
import tarfile
import tempfile
from source_privacy import checked

DIRECTORIES = {'src', 'scripts', 'resources', 'tests', 'licenses', 'docs', 'qt-cocoa', 'upstream'}
FILES = {'CMakeLists.txt', 'README.md', 'README.ja.md', 'LICENSE', '.gitignore', '.gitattributes'}
DOCUMENTATION_FILES = {'CHANGELOG.md', 'CHANGELOG.ja.md'}
DEVELOPER_FILES = {'AGENTS.md', 'SECURITY.md'}
MANIFEST = 'SOURCE-MANIFEST.json'
BUILD_DOCS = {'windows-parity.md', 'upstream-updates.md', 'distribution.md',
              'license-audit-follow-up.md', 'maintenance-verification.md',
              'building.md', 'features.md'}


def permitted(name):
    path = PurePosixPath(name)
    return (not path.is_absolute() and '..' not in path.parts
            and (name in FILES | DOCUMENTATION_FILES | DEVELOPER_FILES
                 or len(path.parts) > 1 and path.parts[0] in DIRECTORIES)
            and not (path.parts[0] == 'docs' and path.suffix in {'.log', '.json'})
            and '__pycache__' not in path.parts and not name.endswith('.pyc'))


def build_source(name):
    """Corresponding source for the app, with developer-only fixtures excluded."""
    path = PurePosixPath(name)
    if name in DEVELOPER_FILES or name == 'scripts/git-privacy.py' or name.startswith('scripts/git-hooks/'):
        return False
    if path.parts[0] == 'tests':
        return False
    if path.parts[0] == 'docs' and path.name not in BUILD_DOCS:
        return False
    if path.parts[0] == 'scripts' and (path.name.startswith(('test-', 'test.'))
            or path.name in {'verify-release.sh', 'prepare-publication.py'}):
        return False
    return True


def sources(root):
    if (root / '.git').exists():
        output = subprocess.check_output(['git', '-C', str(root), 'ls-files', '--stage', '-z'])
        entries = {}
        for record in output.split(b'\0'):
            if not record:
                continue
            fields, filename = record.split(b'\t', 1)
            mode, _, stage = fields.decode('ascii').split()
            name = filename.decode('utf-8')
            if not permitted(name):
                continue
            if stage != '0' or mode not in {'100644', '100755'}:
                raise SystemExit('Cannot package conflicted or non-regular source: ' + name)
            entries[name] = 0o755 if mode == '100755' else 0o644
        return entries
    # The supplied source archive can rebuild/package without Git metadata.
    manifest = json.loads((root / MANIFEST).read_text(encoding='utf-8'))
    entries = {entry['path']: int(entry['mode'], 8) for entry in manifest['files']}
    if any(not permitted(name) or mode not in {0o644, 0o755} for name, mode in entries.items()):
        raise SystemExit('Invalid source manifest')
    return entries


def export(root, destination, developer=False):
    entries = {name: mode for name, mode in sources(root).items()
               if developer or build_source(name)}
    if not FILES.issubset(entries):
        raise SystemExit('Source inventory lacks required build/license files')
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as stream:
            temporary = Path(stream.name)
            with gzip.GzipFile(fileobj=stream, mode='wb', filename='', mtime=0) as compressed:
                with tarfile.open(fileobj=compressed, mode='w', format=tarfile.PAX_FORMAT) as archive:
                    manifest = {'schema': 1, 'profile': 'developer' if developer else 'build', 'files': []}
                    for name, mode in sorted(entries.items()):
                        source = root / name
                        if source.is_symlink() or not source.is_file():
                            raise SystemExit('Missing or non-regular tracked source: ' + name)
                        data = checked(root, name, source.read_bytes())
                        info = tarfile.TarInfo(name)
                        info.mode = mode
                        info.size = len(data)
                        archive.addfile(info, io.BytesIO(data))
                        manifest['files'].append({'path': name, 'mode': oct(mode),
                                                  'sha256': hashlib.sha256(data).hexdigest()})
                    data = (json.dumps(manifest, ensure_ascii=False, indent=2) + '\n').encode('utf-8')
                    info = tarfile.TarInfo(MANIFEST)
                    info.mode = 0o644
                    info.size = len(data)
                    archive.addfile(info, io.BytesIO(data))
        # Verify names, normalized modes and exact captured source bytes before publication.
        with tarfile.open(temporary, 'r:gz') as archive:
            if sorted(archive.getnames()) != sorted([*entries, MANIFEST]):
                raise SystemExit('Source archive inventory mismatch')
            for entry in manifest['files']:
                member = archive.getmember(entry['path'])
                if member.mode != int(entry['mode'], 8) or not member.isfile():
                    raise SystemExit('Source archive mode mismatch: ' + entry['path'])
                if hashlib.sha256(archive.extractfile(member).read()).hexdigest() != entry['sha256']:
                    raise SystemExit('Source archive checksum mismatch: ' + entry['path'])
        os.chmod(temporary, 0o644)
        os.replace(temporary, destination)
        temporary = None
        print(f'Source archive verified: {len(entries)} tracked regular files; normalized modes; SHA-256 manifest')
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path)
    parser.add_argument('destination', type=Path)
    parser.add_argument('--developer', action='store_true', help='Include optional tests and developer tools')
    args = parser.parse_args()
    export(args.root.resolve(), args.destination.resolve(), args.developer)
