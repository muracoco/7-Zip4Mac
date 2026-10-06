#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Keep one installed Finder opener; retain reversible development copies."""
import argparse
import json
import os
from pathlib import Path
import plistlib
import re
import shutil
import subprocess
import tarfile
import tempfile
import uuid

BUNDLE_ID = 'org.sevenzip.macport'
REGISTER = '/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'


def run(*args):
    subprocess.run([str(arg) for arg in args], check=True)


def unregister(app):
    result = subprocess.run([REGISTER, '-u', str(app)], capture_output=True, text=True)
    # -10814 is kLSApplicationNotFoundErr: incomplete/already-unregistered
    # bundles need no removal. Do not suppress permission or database errors.
    if result.returncode and not re.search(r': -10814(?:\s|$)', result.stdout + result.stderr):
        raise subprocess.CalledProcessError(result.returncode, result.args,
                                            result.stdout, result.stderr)


def info(app):
    app = Path(app)
    if app.is_symlink() or not app.is_dir():
        raise ValueError('Expected a real application directory: ' + str(app))
    plist = app / 'Contents/Info.plist'
    if plist.is_symlink():
        raise ValueError('Refusing a symbolic-link Info.plist')
    data = plistlib.loads(plist.read_bytes())
    if data.get('CFBundleIdentifier') != BUNDLE_ID or data.get('CFBundleExecutable') != '7-Zip Mac':
        raise ValueError('This is not a 7-Zip Mac Port bundle: ' + str(app))
    return data


def ensure_closed(app):
    exe = str(app.resolve() / 'Contents/MacOS/7-Zip Mac')
    for line in subprocess.check_output(['ps', '-axo', 'pid=,command='], text=True).splitlines():
        parts = line.strip().split(maxsplit=1)
        if len(parts) == 2 and (parts[1] == exe or parts[1].startswith(exe + ' ')):
            raise ValueError('Close this application first: ' + str(app))


def configure(app, rank):
    data = info(app)
    for document in data.get('CFBundleDocumentTypes', []):
        document['LSHandlerRank'] = rank
    plist = app / 'Contents/Info.plist'
    original = plist.read_bytes()
    fmt = plistlib.FMT_BINARY if original.startswith(b'bplist') else plistlib.FMT_XML
    plist.write_bytes(plistlib.dumps(data, fmt=fmt, sort_keys=False))


def signing_files(app):
    # A non-deep re-sign changes the outer executable, plist and resource seal;
    # it leaves all nested frameworks, plugins, engines and sources intact.
    names = ['Contents/Info.plist']
    executable = app / 'Contents/MacOS/7-Zip Mac'
    if executable.exists() or executable.is_symlink():
        names.append('Contents/MacOS/7-Zip Mac')
    signature = app / 'Contents/_CodeSignature'
    if signature.exists():
        for entry in signature.rglob('*'):
            if entry.is_symlink() or not (entry.is_dir() or entry.is_file()):
                raise ValueError('Unexpected signature entry')
            if entry.is_file():
                names.append(entry.relative_to(app).as_posix())
    return names


def backup_signing(app):
    directory = app.parent / '.7zip-finder-backups'
    directory.mkdir(mode=0o700, exist_ok=True)
    archive = directory / (app.stem + '-' + uuid.uuid4().hex + '.tar.gz')
    names = signing_files(app)
    with tarfile.open(archive, 'x:gz') as output:
        for name in names:
            source = app / name
            if source.is_symlink() or not source.is_file():
                raise ValueError('Expected a regular signing file: ' + name)
            output.add(source, arcname=name, recursive=False)
    with tarfile.open(archive, 'r:gz') as saved:
        if saved.getnames() != names:
            raise ValueError('Signing backup inventory mismatch')
        for name in names:
            if saved.extractfile(name).read() != (app / name).read_bytes():
                raise ValueError('Signing backup content mismatch: ' + name)
    return archive


def restore_signing(app, archive):
    # Never use extractall on an arbitrary archive or follow an output link.
    with tarfile.open(archive, 'r:gz') as saved:
        members = saved.getmembers()
        allowed = {'Contents/Info.plist', 'Contents/MacOS/7-Zip Mac'}
        if 'Contents/Info.plist' not in {member.name for member in members}:
            raise ValueError('Incomplete signing backup')
        for member in members:
            if (not member.isfile() or '..' in Path(member.name).parts or
                    not (member.name in allowed or member.name.startswith('Contents/_CodeSignature/'))):
                raise ValueError('Unsafe signing backup entry')
            target = app / member.name
            if target.is_symlink() or any(parent.is_symlink() for parent in target.parents):
                raise ValueError('Refusing a symbolic-link restore path')
        signature = app / 'Contents/_CodeSignature'
        if signature.exists():
            shutil.rmtree(signature)
        for member in members:
            target = app / member.name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(saved.extractfile(member).read())
            target.chmod(member.mode & 0o777)


def hide(app):
    app = app.resolve()
    data = info(app)
    if all(document.get('LSHandlerRank') == 'None' for document in data.get('CFBundleDocumentTypes', [])):
        unregister(app)
        return None
    ensure_closed(app)
    archive = backup_signing(app)
    try:
        configure(app, 'None')
        executable = app / 'Contents/MacOS/7-Zip Mac'
        if executable.is_file():
            run('/usr/bin/codesign', '--force', '--sign', '-', app)
        unregister(app)
        if executable.is_file():
            run(REGISTER, '-f', app)
    except Exception:
        restore_signing(app, archive)
        run(REGISTER, '-f', app)
        raise
    return str(archive)


def managed_apps(roots):
    result = set()
    for root in roots:
        root = Path(root).resolve()
        for parent, directories, _ in os.walk(root, followlinks=False):
            for name in list(directories):
                child = Path(parent) / name
                if name.endswith('.app'):
                    directories.remove(name)
                    try:
                        info(child)
                    except (ValueError, OSError, plistlib.InvalidFileException):
                        continue
                    result.add(child.resolve())
                elif (name.startswith('.') or child.is_symlink() or
                      name in {'Qt', 'QtSources', 'tools', 'fixture-tools', '7zip-26.03'} or
                      name.startswith('qt-cocoa-')):
                    directories.remove(name)
    return sorted(result)


def deduplicate(installed, roots):
    installed = installed.resolve()
    info(installed)
    receipts = []
    try:
        for app in managed_apps(roots):
            if app == installed:
                continue
            archive = hide(app)
            receipts.append({'app': str(app), 'signingBackup': archive})
    finally:
        run(REGISTER, '-f', installed)
    return receipts


def version(app):
    return tuple(int(part) for part in info(app)['CFBundleShortVersionString'].split('.'))


def install(source, destination, roots, check_bundle):
    source, destination = source.resolve(), destination.absolute()
    info(source)
    if source == destination or destination.is_symlink():
        raise ValueError('Source must differ from a non-symlink installation path')
    run(check_bundle, source)
    if destination.exists():
        info(destination)
        ensure_closed(destination)
        if version(source) < version(destination):
            raise ValueError('Refusing to replace a newer installed version')
    destination.parent.mkdir(parents=True, exist_ok=True)
    stage = Path(tempfile.mkdtemp(prefix='.7zip-install-', dir=destination.parent))
    staged = stage / destination.name
    previous = None
    moved = False
    try:
        run('/usr/bin/ditto', source, staged)
        configure(staged, 'Alternate')
        run('/usr/bin/codesign', '--force', '--sign', '-', staged)
        run(check_bundle, staged)
        if destination.exists():
            previous = stage / ('previous-' + info(destination)['CFBundleShortVersionString'] + '.app')
            os.rename(destination, previous)
            moved = True
        try:
            os.rename(staged, destination)
        except Exception:
            if moved:
                os.rename(previous, destination)
                moved = False
            raise
        # A retained previous bundle cannot become another Finder candidate.
        previous_backup = hide(previous) if previous else None
        hidden = deduplicate(destination, [*roots, stage, source.parent])
        return {'installed': str(destination), 'version': info(destination)['CFBundleShortVersionString'],
                'previousBundle': str(previous) if previous else None,
                'previousSigningBackup': previous_backup, 'hiddenCopies': hidden}
    finally:
        # Do not erase rollback data or partially completed install evidence.
        if stage.exists() and not any(stage.iterdir()):
            stage.rmdir()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    setup = sub.add_parser('configure', help='set a staged plist before signing')
    setup.add_argument('app', type=Path)
    setup.add_argument('--rank', choices=['None', 'Alternate'], required=True)
    cleanup = sub.add_parser('deduplicate', help='hide only same-ID copies under explicit managed roots')
    cleanup.add_argument('installed', type=Path)
    cleanup.add_argument('--managed-root', action='append', type=Path, required=True)
    update = sub.add_parser('install', help='replace one installation and preserve a hidden rollback copy')
    update.add_argument('source', type=Path)
    update.add_argument('destination', type=Path)
    update.add_argument('--managed-root', action='append', type=Path, default=[])
    update.add_argument('--check-bundle', type=Path, default=Path(__file__).with_name('check-bundle.sh'))
    restore = sub.add_parser('restore', help='restore original signing files to a retained copy')
    restore.add_argument('app', type=Path)
    restore.add_argument('archive', type=Path)
    args = parser.parse_args()
    if args.command == 'configure':
        ensure_closed(args.app)
        configure(args.app, args.rank)
    elif args.command == 'deduplicate':
        print(json.dumps(deduplicate(args.installed, args.managed_root), indent=2))
    elif args.command == 'install':
        print(json.dumps(install(args.source, args.destination, args.managed_root, args.check_bundle), indent=2))
    else:
        ensure_closed(args.app)
        info(args.app)
        restore_signing(args.app, args.archive)
        run('/usr/bin/codesign', '--verify', '--strict', args.app)


if __name__ == '__main__':
    main()
