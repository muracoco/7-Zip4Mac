#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Focused importer-update and distribution checks using real official sources."""
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import upstream_support as pins
import upstream
from source_privacy import checked

spec = importlib.util.spec_from_file_location('source_archive', ROOT / 'scripts/source-archive.py')
source_archive = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source_archive)
DEPS = Path(os.environ.get('SEVENZIP_DEPS_DIR', Path.home() / '.cache/7zip-mac-port'))
SOURCE = DEPS / ('7zip-' + pins.VERSION)
ARCHIVE = DEPS / pins.LOCK['release']['archive']


class MaintenanceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original_lock = pins.LOCK_PATH.read_bytes()
        cls.data = upstream.archive_files(ARCHIVE, pins.LOCK['release']['sha256'])
        pins.verify_source(SOURCE)

    def run_tool(self, *args, lock=None, success=True):
        environment = {**os.environ, 'PYTHONDONTWRITEBYTECODE': '1'}
        if lock:
            environment['SEVENZIP_UPSTREAM_LOCK'] = str(lock)
        result = subprocess.run([sys.executable, str(ROOT / 'scripts/upstream.py'), *map(str, args)],
                                env=environment, capture_output=True, text=True)
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        self.assertEqual(pins.LOCK_PATH.read_bytes(), self.original_lock)
        return result

    def archive(self, target, data):
        with tarfile.open(target, 'w:xz') as archive:
            for name, raw in data.items():
                entry = tarfile.TarInfo(name); entry.size = len(raw)
                archive.addfile(entry, io.BytesIO(raw))
        return hashlib.sha256(target.read_bytes()).hexdigest()

    def test_official_archive_inventory(self):
        self.assertEqual({name: hashlib.sha256(raw).hexdigest() for name, raw in self.data.items()}, pins.LOCK['files'])
        for name, group in pins.LOCK['importers'].items():
            self.assertLessEqual(group['adaptation']['first'], group['adaptation']['last'])
            self.assertTrue(set(group['inputs']).issubset(self.data), name)

    def test_identical_candidate(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'comparison'
            self.run_tool('compare', '--source', SOURCE, '--archive', ARCHIVE,
                          '--sha256', pins.LOCK['release']['sha256'], '--output', output)
            self.assertEqual(json.loads((output / 'comparison.json').read_text())['changes'], [])
            self.run_tool('compare', '--source', SOURCE, '--archive', ARCHIVE,
                          '--sha256', pins.LOCK['release']['sha256'], '--output', output, success=False)

    def test_stage_preserves_generated_bodies_and_notices(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'staged'
            self.run_tool('stage', '--source', SOURCE, '--output', output)
            actual = sorted((output / 'port/src/upstream').iterdir())
            self.assertEqual(len(actual), 55)
            for path in actual:
                self.assertEqual(path.read_bytes(), (ROOT / 'src/upstream' / path.name).read_bytes())
                self.assertIn(b'Modified for 7-Zip Mac Port', path.read_bytes())
            for name in pins.LOCK['importers']['import-progress.py']['inputs']:
                original = self.data[name]
                generated = (output / 'overlay' / name).read_bytes()
                self.assertFalse((output / 'overlay' / name).is_symlink())
                self.assertIn(original.splitlines()[0], generated)
                self.assertIn(b'2026-10-04 to 2026-10-06', generated)
                self.assertEqual(generated.count(b'\n'), generated.count(b'\r\n'))

    def test_synthetic_next_release_uses_central_pins(self):
        # This is a local synthetic archive, not a claimed official release.
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); data = dict(self.data)
            data['C/7zVersion.h'] = data['C/7zVersion.h'].replace(b'"26.03"', b'"26.04"').replace(b'MY_VER_MINOR 3', b'MY_VER_MINOR 4')
            data['C/7zCrc.c'] += b'\r\n/* synthetic maintenance-test change */\r\n'
            archive = root / '7z2604-src.tar.xz'; digest = self.archive(archive, data)
            comparison = root / 'comparison'
            self.run_tool('compare', '--source', SOURCE, '--archive', archive,
                          '--sha256', digest, '--output', comparison)
            report = json.loads((comparison / 'comparison.json').read_text())
            self.assertEqual(report['candidate'], '26.04')
            self.assertEqual(set(report['changes']), {'C/7zVersion.h', 'C/7zCrc.c'})
            self.run_tool('stage', '--source', comparison / 'source', '--output', root / 'staged',
                          lock=comparison / 'candidate-lock.json')
            self.assertIn(b'official 7-Zip 26.04', (root / 'staged/port/src/upstream/CompressMath.inc').read_bytes())

    def test_changed_anchor_stops_without_accepting_pins(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); data = dict(self.data)
            name = 'CPP/7zip/UI/GUI/CompressDialog.cpp'
            data[name] = data[name].replace(b'Get_Lzma2_ChunkSize', b'SyntheticChangedAnchor')
            archive = root / pins.LOCK['release']['archive']; digest = self.archive(archive, data)
            comparison = root / 'comparison'
            self.run_tool('compare', '--source', SOURCE, '--archive', archive,
                          '--sha256', digest, '--output', comparison)
            report = json.loads((comparison / 'comparison.json').read_text())
            self.assertIn('import-compression-math.py', report['affectedImporters'])
            self.run_tool('stage', '--source', comparison / 'source', '--output', root / 'staged',
                          lock=comparison / 'candidate-lock.json', success=False)

    def test_archive_rejects_unsafe_entries_and_wrong_checksum(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / 'input.tar.xz'
            for name, kind in [('../escape', tarfile.REGTYPE), ('/absolute', tarfile.REGTYPE),
                               ('link', tarfile.SYMTYPE), ('hard', tarfile.LNKTYPE)]:
                with tarfile.open(path, 'w:xz') as archive:
                    entry = tarfile.TarInfo(name); entry.type = kind; entry.linkname = '../escape'
                    archive.addfile(entry, io.BytesIO(b''))
                with self.assertRaises(ValueError):
                    upstream.archive_files(path, hashlib.sha256(path.read_bytes()).hexdigest())
            with self.assertRaises(ValueError):
                upstream.archive_files(ARCHIVE, '0' * 64)
            with tarfile.open(path, 'w:xz') as archive:
                archive.addfile(tarfile.TarInfo('a/duplicate')); archive.addfile(tarfile.TarInfo('a/./duplicate'))
            with self.assertRaises(ValueError):
                upstream.archive_files(path, hashlib.sha256(path.read_bytes()).hexdigest())

    def test_build_source_profile(self):
        for name in ['tests/integration.cpp', 'tests/fixtures/sample.rar', 'scripts/test.sh',
                     'scripts/test-formats.sh', 'scripts/verify-release.sh', 'docs/old-test.log',
                     'docs/release-candidate.md']:
            self.assertFalse(source_archive.build_source(name), name)
        for name in ['CMakeLists.txt', 'upstream/7zip.json', 'src/upstream/CompressMath.inc',
                     'scripts/upstream_support.py', 'qt-cocoa/accessibility-ownership.patch', 'licenses/NOTICE.md']:
            self.assertTrue(source_archive.permitted(name), name)
            self.assertTrue(source_archive.build_source(name), name)
        example = b'/Users/' + b'synthetic-account/cache'
        self.assertNotIn(example, checked(ROOT, 'README.md', example))
        with self.assertRaises(ValueError):
            checked(ROOT, 'src/private.cpp', example)


if __name__ == '__main__':
    unittest.main(verbosity=2)
