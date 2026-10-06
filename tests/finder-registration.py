#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Exercise real macOS registration/install without touching user documents."""
import ctypes as c
import importlib.util
import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('finder_registration', ROOT / 'scripts/finder-registration.py')
port = importlib.util.module_from_spec(spec)
spec.loader.exec_module(port)


def candidates(document):
    cf = c.CDLL('/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation')
    ls = c.CDLL('/System/Library/Frameworks/CoreServices.framework/CoreServices')
    cf.CFStringCreateWithCString.argtypes = [c.c_void_p, c.c_char_p, c.c_uint32]
    cf.CFStringCreateWithCString.restype = c.c_void_p
    cf.CFURLCreateWithFileSystemPath.argtypes = [c.c_void_p, c.c_void_p, c.c_long, c.c_bool]
    cf.CFURLCreateWithFileSystemPath.restype = c.c_void_p
    cf.CFURLCopyFileSystemPath.argtypes = [c.c_void_p, c.c_long]
    cf.CFURLCopyFileSystemPath.restype = c.c_void_p
    cf.CFStringGetCString.argtypes = [c.c_void_p, c.c_void_p, c.c_long, c.c_uint32]
    cf.CFStringGetCString.restype = c.c_bool
    cf.CFArrayGetCount.argtypes = [c.c_void_p]
    cf.CFArrayGetCount.restype = c.c_long
    cf.CFArrayGetValueAtIndex.argtypes = [c.c_void_p, c.c_long]
    cf.CFArrayGetValueAtIndex.restype = c.c_void_p
    cf.CFRelease.argtypes = [c.c_void_p]
    ls.LSCopyApplicationURLsForURL.argtypes = [c.c_void_p, c.c_uint32]
    ls.LSCopyApplicationURLsForURL.restype = c.c_void_p
    string = cf.CFStringCreateWithCString(None, str(document).encode(), 0x08000100)
    url = cf.CFURLCreateWithFileSystemPath(None, string, 0, False)
    cf.CFRelease(string)
    array = ls.LSCopyApplicationURLsForURL(url, 0xffffffff)
    cf.CFRelease(url)
    paths = []
    if array:
        for index in range(cf.CFArrayGetCount(array)):
            string = cf.CFURLCopyFileSystemPath(cf.CFArrayGetValueAtIndex(array, index), 0)
            buffer = c.create_string_buffer(32768)
            if not cf.CFStringGetCString(string, buffer, len(buffer), 0x08000100):
                raise ValueError('Cannot decode candidate path')
            paths.append(Path(buffer.value.decode()).resolve())
            cf.CFRelease(string)
        cf.CFRelease(array)
    return paths


def make_app(parent, name, version, rank, identifier=port.BUNDLE_ID):
    app = parent / name
    (app / 'Contents/MacOS').mkdir(parents=True)
    executable = app / 'Contents/MacOS/7-Zip Mac'
    shutil.copyfile('/usr/bin/true', executable)
    executable.chmod(0o755)
    data = {'CFBundleExecutable': '7-Zip Mac', 'CFBundleIdentifier': identifier,
            'CFBundlePackageType': 'APPL', 'CFBundleShortVersionString': version,
            'CFBundleVersion': version, 'CFBundleName': '7-Zip Registration Test',
            'CFBundleDocumentTypes': [{'CFBundleTypeName': '7-Zip Archive',
                                      'CFBundleTypeRole': 'Viewer',
                                      'CFBundleTypeExtensions': ['7z', 'zip'], 'LSHandlerRank': rank}]}
    (app / 'Contents/Info.plist').write_bytes(plistlib.dumps(data))
    port.run('/usr/bin/codesign', '--force', '--sign', '-', app)
    return app


def own_candidates(root, document):
    return sorted(path for path in candidates(document) if path.is_relative_to(root))


def main():
    # Launch Services excludes applications under the OS temporary directory
    # from document candidates even when explicitly registered.
    cache = Path.home() / '.cache/7zip-mac-port'
    cache.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='finder-registration-test-', dir=cache) as directory:
        root = Path(directory).resolve()
        document = root / '日本語 空白.7z'
        document.write_bytes(b'fixture')
        installed = make_app(root, 'Installed.app', '0.2.1', 'Alternate')
        source = make_app(root, 'Build.app', '0.2.2', 'None')
        legacy = make_app(root, 'Older.app', '0.1.0', 'Alternate')
        incomplete = make_app(root, 'Incomplete.app', '0.0.1', 'Alternate')
        (incomplete / 'Contents/MacOS/7-Zip Mac').unlink()
        unrelated = make_app(root, 'Other.app', '9.0', 'None', 'org.example.other-registration-test')
        original = {name: (installed / name).read_bytes() for name in port.signing_files(installed)}
        unrelated_bytes = (unrelated / 'Contents/Info.plist').read_bytes()
        try:
            for app in [installed, source, legacy]:
                port.run(port.REGISTER, '-f', app)
            assert own_candidates(root, document) == sorted([installed, legacy]), candidates(document)
            receipt = port.install(source, installed, [root], ROOT / 'scripts/check-bundle.sh')
            assert port.version(installed) == (0, 2, 2)
            assert own_candidates(root, document) == [installed]
            assert (unrelated / 'Contents/Info.plist').read_bytes() == unrelated_bytes
            assert port.info(incomplete)['CFBundleDocumentTypes'][0]['LSHandlerRank'] == 'None'
            # Explicit re-registration/rescanning must not resurrect any copy.
            for app in port.managed_apps([root]):
                if (app / 'Contents/MacOS/7-Zip Mac').exists():
                    port.run(port.REGISTER, '-f', app)
            previous = Path(receipt['previousBundle'])
            port.run(port.REGISTER, '-f', previous)
            assert own_candidates(root, document) == [installed]
            port.restore_signing(previous, Path(receipt['previousSigningBackup']))
            for name, content in original.items():
                assert (previous / name).read_bytes() == content
            port.run('/usr/bin/codesign', '--verify', '--strict', previous)
            port.hide(previous)
            current = (installed / 'Contents/MacOS/7-Zip Mac').read_bytes()
            try:
                port.install(legacy, installed, [root], ROOT / 'scripts/check-bundle.sh')
            except ValueError as error:
                assert 'newer' in str(error)
            else:
                raise AssertionError('Downgrade accepted')
            assert (installed / 'Contents/MacOS/7-Zip Mac').read_bytes() == current
            assert port.version(installed) == (0, 2, 2)
            assert own_candidates(root, document) == [installed]
            print(json.dumps({'passed': ['build-hidden', 'latest-only-install', 'rescan-stays-single',
                                         'unrelated-bundle-retained', 'rollback-byte-identity',
                                         'rollback-signature', 'downgrade-rejected',
                                         'incomplete-build-hidden']}, indent=2))
        finally:
            for app in port.managed_apps([root]):
                port.unregister(app)
            for path in candidates(document):
                if path.is_relative_to(root):
                    port.unregister(path)
            canonical = Path('/Applications/7-Zip Mac.app')
            if canonical.exists():
                port.run(port.REGISTER, '-f', canonical)


if __name__ == '__main__':
    main()
