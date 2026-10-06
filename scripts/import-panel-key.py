#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Import official focused-key dispatch and label-name validation boundaries."""
from upstream_support import VERSION, source_hash, aggregate_hash, artifact_hash, output_root, write_generated
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
args = parser.parse_args()
files = {
    'PanelKey.cpp': source_hash('CPP/7zip/UI/FileManager/PanelKey.cpp'),
    'PanelOperations.cpp': source_hash('CPP/7zip/UI/FileManager/PanelOperations.cpp'),
    'BrowseDialog.cpp': source_hash('CPP/7zip/UI/FileManager/BrowseDialog.cpp'),
    'App.cpp': source_hash('CPP/7zip/UI/FileManager/App.cpp'),
    'PanelFolderChange.cpp': source_hash('CPP/7zip/UI/FileManager/PanelFolderChange.cpp'),
    'Panel.cpp': source_hash('CPP/7zip/UI/FileManager/Panel.cpp'),
    'FSFolder.cpp': source_hash('CPP/7zip/UI/FileManager/FSFolder.cpp'),
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
          f'// Generated from official 7-Zip {VERSION} by scripts/import-panel-key.py.\n'
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
root = output_root() / 'src/upstream'
for name, text in [('PanelKeyCommands.inc', body), ('RenameName.inc', validation), ('FilesystemCreationPath.inc', creation)]:
    output = root / name
    value = header + '\n' + text.rstrip() + '\n'
    if not output.exists() or output.read_text() != value:
        write_generated(output, value)
    print(output)
