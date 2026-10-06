# SPDX-License-Identifier: LGPL-3.0-or-later
"""Shared, deterministic source pins and modification notices for importers."""
import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
LOCK_PATH = Path(os.environ.get('SEVENZIP_UPSTREAM_LOCK', ROOT / 'upstream/7zip.json'))
LOCK = json.loads(LOCK_PATH.read_text(encoding='utf-8'))
VERSION = LOCK['release']['version']


def source_hash(name):
    return LOCK['files'][name]


def artifact_hash(name):
    return LOCK['artifacts'][name]['sha256']


def aggregate_hash(name):
    return LOCK['aggregates'][name]['sha256']


def output_root():
    return Path(os.environ.get('SEVENZIP_PORT_OUTPUT', ROOT)).resolve()


def modification_notice(generator=None):
    name = Path(generator or sys.argv[0]).name
    dates = LOCK['importers'][name]['adaptation']
    return ('// Modified for 7-Zip Mac Port by the 7-Zip4Mac contributors.\n'
            f"// Port adaptation: {dates['first']} to {dates['last']} (recorded Git history).\n"
            '// Modification notice added on 2026-10-06; original notices retained below.\n')


def generated_bytes(data):
    """Keep upstream bodies/encoding/newlines; add a deterministic Port notice."""
    if isinstance(data, str):
        return modification_notice() + data
    # Source overlays retain the official CRLF convention and optional BOM.
    ending = '\r\n' if b'\r\n' in data else '\n'
    notice = modification_notice().replace('\n', ending).encode('utf-8')
    bom = b'\xef\xbb\xbf'
    return bom + notice + data[len(bom):] if data.startswith(bom) else notice + data


def write_generated(path, data, *args, **kwargs):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    content = generated_bytes(data)
    if isinstance(content, str):
        encoding = kwargs.get('encoding') or 'utf-8'
        content = content.encode(encoding)
    if not path.exists() or path.read_bytes() != content:
        path.write_bytes(content)


def verify_source(source):
    source = Path(source).resolve()
    failed = []
    for name, expected in LOCK['files'].items():
        path = source / name
        if (path.is_symlink() or not path.is_file() or
                not path.resolve().is_relative_to(source) or
                hashlib.sha256(path.read_bytes()).hexdigest() != expected):
            failed.append(name)
    if failed:
        raise ValueError('Upstream source differs from the reviewed lock: ' + ', '.join(failed[:12]))
