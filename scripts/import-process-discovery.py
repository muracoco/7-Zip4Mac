#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Retain official CChildProcesses::Update behind macOS process adapters."""
from upstream_support import VERSION, source_hash, aggregate_hash, artifact_hash, output_root, write_generated
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=Path)
args = parser.parse_args()
path = args.source / 'CPP/7zip/UI/FileManager/PanelItemOpen.cpp'
digest = source_hash('CPP/7zip/UI/FileManager/PanelItemOpen.cpp')
raw = path.read_bytes()
if hashlib.sha256(raw).hexdigest() != digest:
    parser.error('Unsupported official PanelItemOpen.cpp')
text = raw.decode().replace('\r\n', '\n')
start = text.index('  void Update(bool needFindProcessByPath')
opening = text.index('{', start)
cursor, depth = opening + 1, 1
while depth:
    depth += (text[cursor] == '{') - (text[cursor] == '}')
    cursor += 1
body = text[start:cursor]
header = ('// PanelItemOpen.cpp\n'
          '// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.\n'
          f'// Generated from official 7-Zip {VERSION} by scripts/import-process-discovery.py.\n'
          '// Original Update body retained; macOS supplies snapshot/handle adapters.\n'
          '// PanelItemOpen.cpp SHA-256: ' + digest + '\n')
output = output_root() / 'src/upstream/ProcessDiscovery.inc'
write_generated(output, header + body + '\n')
print(output)
