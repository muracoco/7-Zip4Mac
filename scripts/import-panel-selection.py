#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Retain the official File Manager selection bodies behind Qt list adapters."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
args = parser.parse_args()
base = args.source / 'CPP/7zip/UI/FileManager'
hashes = {'PanelSelect.cpp': '8c64e1607e84be906cd73755bcfeeb4b2ec20ae18e38ef1646811b6b19b0053e',
          'PanelItems.cpp': '2dc5869e794788b366b1dc7fb6485b362e7b72352c1549303b944693765e2f73'}
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
header = '''// PanelSelect.cpp / PanelItems.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-panel-selection.py.
// Original bodies retained; Qt supplies the vector/list/notification boundary.
'''
for name, digest in hashes.items():
    header += '// ' + name + ' SHA-256: ' + digest + '\n'
output = Path(__file__).resolve().parents[1] / 'src/upstream/PanelSelection.inc'
output.write_text(header + '\n' + '\n\n'.join(body) + '\n')
print(output)
output = output.with_name('PanelOpen.inc')
output.write_text(header + '\n' + function(texts['PanelItems.cpp'], 'void CPanel::OpenSelectedItems(bool tryInternal)') + '\n')
print(output)

wildcard_path = args.source / 'CPP/Common/Wildcard.cpp'
wildcard_raw = wildcard_path.read_bytes()
wildcard_digest = '8c009138344c21eb8aea64334e892cb4949417e7bbc30cd75941692cd595f122'
if hashlib.sha256(wildcard_raw).hexdigest() != wildcard_digest:
    parser.error('Unsupported original Wildcard.cpp')
wildcard = wildcard_raw.decode().replace('\r\n', '\n')
output = output.with_name('WildcardMatch.inc')
output.write_text('// Common/Wildcard.cpp\n// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.\n// Original EnhancedMaskTest; SHA-256: ' + wildcard_digest + '\n' + function(wildcard, 'static bool EnhancedMaskTest(') + '\n')
