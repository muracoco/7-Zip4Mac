#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Retain the official File Manager selection bodies behind Qt list adapters."""
from upstream_support import VERSION, source_hash, aggregate_hash, artifact_hash, output_root, write_generated
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
args = parser.parse_args()
base = args.source / 'CPP/7zip/UI/FileManager'
hashes = {'PanelSelect.cpp': source_hash('CPP/7zip/UI/FileManager/PanelSelect.cpp'),
          'PanelItems.cpp': source_hash('CPP/7zip/UI/FileManager/PanelItems.cpp')}
texts = {}
for name, expected in hashes.items():
    raw = (base / name).read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        parser.error('Unsupported upstream source: ' + name)
    texts[name] = raw.decode().replace('\r\n', '\n')

def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    cursor, depth = opening + 1, 1
    while depth:
        depth += (text[cursor] == '{') - (text[cursor] == '}')
        cursor += 1
    return text[start:cursor]

names = ['OnShiftSelectMessage()', 'OnArrowWithShift()', 'OnInsert()',
         'UpdateSelection()', 'SelectAll(bool selectMode)', 'InvertSelection()',
         'KillSelection()', 'OnLeftClick(MY_NMLISTVIEW_NMITEMACTIVATE *itemActivate)']
body = [function(texts['PanelSelect.cpp'], 'void CPanel::' + name) for name in names]
for name in ['Get_ItemIndices_Selected', 'Get_ItemIndices_Operated']:
    body.append(function(texts['PanelItems.cpp'], 'void CPanel::' + name + '(CRecordVector<UInt32> &indices) const'))
header = f'''// PanelSelect.cpp / PanelItems.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip {VERSION} by scripts/import-panel-selection.py.
// Original bodies retained; Qt supplies the vector/list/notification boundary.
'''
for name, digest in hashes.items():
    header += '// ' + name + ' SHA-256: ' + digest + '\n'
output = output_root() / 'src/upstream/PanelSelection.inc'
write_generated(output, header + '\n' + '\n\n'.join(body) + '\n')
print(output)
output = output.with_name('PanelOpen.inc')
write_generated(output, header + '\n' + function(texts['PanelItems.cpp'], 'void CPanel::OpenSelectedItems(bool tryInternal)') + '\n')
print(output)

wildcard_path = args.source / 'CPP/Common/Wildcard.cpp'
wildcard_raw = wildcard_path.read_bytes()
wildcard_digest = source_hash('CPP/Common/Wildcard.cpp')
if hashlib.sha256(wildcard_raw).hexdigest() != wildcard_digest:
    parser.error('Unsupported original Wildcard.cpp')
wildcard = wildcard_raw.decode().replace('\r\n', '\n')
output = output.with_name('WildcardMatch.inc')
write_generated(output, '// Common/Wildcard.cpp\n// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.\n// Original EnhancedMaskTest; SHA-256: ' + wildcard_digest + '\n' + function(wildcard, 'static bool EnhancedMaskTest(') + '\n')
