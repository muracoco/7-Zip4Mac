#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Import official focused-key dispatch and label-name validation boundaries."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
args = parser.parse_args()
files = {
    'PanelKey.cpp': 'f0814d249a41d50ba1f4eb58ece67b2f4377d4df7bc59706e4acc4f73e180d7f',
    'PanelOperations.cpp': 'fe88eab47e41a17c1694b38f36a5798c5e49fa34710e7dc8decb408ddec37bb3',
    'BrowseDialog.cpp': '756dc2c4321e0343ae0649c36392acb522bceea9aa8f95c7f9a8e17000ba77db',
    'App.cpp': 'ffeb0cd42e88bd29038039857631e584410323864aeae8ed361aaed897f50a9b',
    'PanelFolderChange.cpp': '912bb44658a2efef68a47f42b43668bab70479f6d1bb41d6c96b797cd3c55acd',
    'Panel.cpp': '0fb68a77ff5079044c01ccbca1336100d26024689bfc4bb51b70bdb5455ddc78',
    'FSFolder.cpp': '59ff062ca1c186410c76d9d1f50992f865d37d7cce948d236f4a7e00a13823bd',
}
texts = {}
for name, digest in files.items():
    raw = (args.source / 'CPP/7zip/UI/FileManager' / name).read_bytes()
    if hashlib.sha256(raw).hexdigest() != digest:
        parser.error('Unsupported official source: ' + name)
    texts[name] = raw.decode().replace('\r\n', '\n')

def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    cursor, depth = opening + 1, 1
    while depth:
        depth += (text[cursor] == '{') - (text[cursor] == '}')
        cursor += 1
    return text[start:cursor]

header = ('// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.\n'
          '// Generated from official 7-Zip 26.03 by scripts/import-panel-key.py.\n'
          '// Original dispatch/name/path bodies; Qt supplies OS types and callbacks.\n')
for name, digest in files.items():
    header += '// ' + name + ' SHA-256: ' + digest + '\n'
key = texts['PanelKey.cpp']
# Import the complete original list key dispatcher. Win32 messages, handles,
# flags, property IDs and callbacks are supplied by the Qt boundary.
body = key[key.index('struct CVKeyPropIDPair'):]
creation = function(texts['FSFolder.cpp'], 'void CFSFolder::GetAbsPath')
validation = function(texts['PanelOperations.cpp'], 'static bool IsCorrectFsName')
# Official non-Windows CorrectFsPath is intentionally an identity operation.
validation += '\n\n' + function(texts['BrowseDialog.cpp'], 'bool CorrectFsPath(const UString & /* relBase */')
root = Path(__file__).resolve().parents[1] / 'src/upstream'
for name, text in [('PanelKeyCommands.inc', body), ('RenameName.inc', validation), ('FilesystemCreationPath.inc', creation)]:
    output = root / name
    value = header + '\n' + text.rstrip() + '\n'
    if not output.exists() or output.read_text() != value:
        output.write_text(value)
    print(output)
