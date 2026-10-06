#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Verify, compare and stage official 7-Zip updates without editing the checkout."""
import argparse
import difflib
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shlex
import subprocess
import sys
import tarfile

import upstream_support as pins


def archive_files(path, expected):
    if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
        raise ValueError('Archive SHA-256 mismatch')
    files = {}
    with tarfile.open(path, 'r:xz') as archive:
        for member in archive:
            name = member.name.removeprefix('./')
            relative = PurePosixPath(name)
            if relative.is_absolute() or '..' in relative.parts or '\\' in name:
                raise ValueError('Unsafe archive path: ' + name)
            name = relative.as_posix()
            if name == '.':
                if member.isdir():
                    continue
                raise ValueError('Empty archive filename')
            if member.isdir():
                continue
            if not member.isfile() or name in files:
                raise ValueError('Non-regular or duplicate archive entry: ' + name)
            files[name] = archive.extractfile(member).read()
    if not {'C/7zVersion.h', 'CPP/7zip/MyVersion.h', 'DOC/License.txt'}.issubset(files):
        raise ValueError('Not an official-layout source archive')
    return files


def fresh_destination(path):
    path = path.resolve()
    if path.exists() or path == pins.ROOT or pins.ROOT in path.parents:
        raise ValueError('Use a new output directory outside the checkout')
    path.mkdir(parents=True)
    return path


def category(name):
    if name.startswith('DOC/') or 'License' in name or 'copying' in name:
        return 'licenses/documentation'
    if '/UI/FileManager/' in name or '/UI/GUI/' in name or '/UI/Explorer/' in name:
        return 'Windows UI/resources/assets'
    if name.startswith('CPP/Windows/'):
        return 'OS adapters'
    return 'engine/codecs/handlers/build'


def compare(args):
    pins.verify_source(args.source)
    data = archive_files(args.archive, args.sha256)
    version = re.search(rb'^#define MY_VERSION_NUMBERS "([0-9]+\.[0-9]+)"',
                        data['C/7zVersion.h'], re.M)
    if not version:
        raise ValueError('Unrecognized official version header')
    version = version[1].decode('ascii')
    expected_name = '7z' + version.replace('.', '') + '-src.tar.xz'
    if args.archive.name != expected_name:
        raise ValueError('Archive filename does not match the official version')
    hashes = {name: hashlib.sha256(raw).hexdigest() for name, raw in data.items()}
    changes = [name for name in sorted(set(pins.LOCK['files']) | set(hashes))
               if pins.LOCK['files'].get(name) != hashes.get(name)]
    affected = {name: sorted(set(group['inputs']) & set(changes))
                for name, group in pins.LOCK['importers'].items()
                if set(group['inputs']) & set(changes)}
    candidate = json.loads(json.dumps(pins.LOCK))
    candidate['release'] = {'version': version, 'archive': expected_name,
        'url': f'https://github.com/ip7z/7zip/releases/download/{version}/{expected_name}',
        'sha256': args.sha256}
    for key, macro in [('date', 'MY_DATE'), ('copyright', 'MY_COPYRIGHT_CR')]:
        match = re.search(rb'^#define ' + macro.encode() + rb' \"([^\"]+)\"', data['C/7zVersion.h'], re.M)
        if not match: raise ValueError('Unrecognized upstream metadata: ' + macro)
        candidate['release'][key] = match[1].decode('utf-8')
    candidate['files'] = hashes
    for group in candidate['aggregates'].values():
        digest = hashlib.sha256()
        for item in sorted(group['inputs'], key=lambda item: item['label']):
            digest.update(item['label'].encode() + b'\0' + data[item['path']])
        group['sha256'] = digest.hexdigest()
    destination = fresh_destination(args.output)
    source = destination / 'source'
    for name, raw in data.items():
        target = source / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
    (destination / 'candidate-lock.json').write_text(json.dumps(candidate, indent=2) + '\n')
    report = {'baseline': pins.VERSION, 'candidate': version, 'changes': changes,
              'affectedImporters': affected,
              'reviewRequired': sorted({category(name) for name in changes}),
              'note': 'A matching SHA verifies the supplied bytes, not publisher authenticity. '
                      'Verify the official stable release separately. No pins have been accepted.'}
    (destination / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
    with (destination / 'source.diff').open('w', encoding='utf-8') as output:
        for name in changes:
            old = (args.source / name).read_bytes() if name in pins.LOCK['files'] else b''
            new = data.get(name, b'')
            try:
                before, after = old.decode('utf-8').splitlines(True), new.decode('utf-8').splitlines(True)
                output.writelines(difflib.unified_diff(before, after, 'old/' + name, 'new/' + name))
            except UnicodeDecodeError:
                output.write(f'Binary changed: {name}\n')
    print(json.dumps({'baseline': pins.VERSION, 'candidate': version,
                      'changedFiles': len(changes), 'affectedImporters': list(affected)}))


def stage(args):
    pins.verify_source(args.source)
    destination = fresh_destination(args.output)
    generated = destination / 'port'
    (generated / 'src/upstream').mkdir(parents=True)
    environment = {**os.environ, 'SEVENZIP_UPSTREAM_LOCK': str(pins.LOCK_PATH.resolve()),
                   'SEVENZIP_PORT_OUTPUT': str(generated), 'PYTHONDONTWRITEBYTECODE': '1'}
    for importer in pins.LOCK['importers']:
        if importer == 'import-help.py':
            continue  # CHM comes from the separately pinned official Windows package.
        command = [sys.executable, str(pins.ROOT / 'scripts' / importer), str(args.source.resolve())]
        if importer == 'import-progress.py':
            command.append(str(destination / 'overlay'))
        subprocess.run(command, env=environment, check=True, stdout=subprocess.DEVNULL)
    files = sorted(path.relative_to(generated).as_posix() for path in generated.rglob('*') if path.is_file())
    (destination / 'stage.json').write_text(json.dumps({'version': pins.VERSION,
        'generatedFiles': files, 'status': 'Generated only; C++ build and functional review still required.'}, indent=2) + '\n')
    print(f'Staged {len(files)} generated files and the helper overlay; checkout unchanged')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('shell', help='Export the pinned release fields for build scripts')
    verify = sub.add_parser('verify'); verify.add_argument('source', type=Path)
    regenerate = sub.add_parser('regenerate'); regenerate.add_argument('source', type=Path)
    comparison = sub.add_parser('compare')
    comparison.add_argument('--source', type=Path, required=True)
    comparison.add_argument('--archive', type=Path, required=True)
    comparison.add_argument('--sha256', required=True)
    comparison.add_argument('--output', type=Path, required=True)
    staging = sub.add_parser('stage')
    staging.add_argument('--source', type=Path, required=True)
    staging.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == 'shell':
            for key in ('version', 'archive', 'url', 'sha256'):
                print('SEVENZIP_' + key.upper() + '=' + shlex.quote(pins.LOCK['release'][key]))
        elif args.command == 'verify':
            pins.verify_source(args.source)
            print(f'Official {pins.VERSION}: {len(pins.LOCK["files"])} pinned source files verified')
        elif args.command == 'compare':
            compare(args)
        elif args.command == 'regenerate':
            pins.verify_source(args.source)
            for importer in pins.LOCK['importers']:
                if importer not in {'import-progress.py', 'import-help.py'}:
                    subprocess.run([sys.executable, str(pins.ROOT / 'scripts' / importer),
                                    str(args.source.resolve())], check=True, stdout=subprocess.DEVNULL)
            print(f'Official {pins.VERSION}: generated adapters refreshed from reviewed pins')
        else:
            stage(args)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError, tarfile.TarError) as error:
        parser.exit(1, f'Upstream update stopped: {error}\n')


if __name__ == '__main__':
    main()
