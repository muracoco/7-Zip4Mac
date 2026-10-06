#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Import official TextPairs operations and their original pointer heap sort."""
from upstream_support import VERSION, source_hash, aggregate_hash, artifact_hash, output_root, write_generated
import hashlib
import pathlib
import sys

root = output_root()
upstream = pathlib.Path(sys.argv[1]) / 'CPP'

def pinned(relative, expected):
    raw = (upstream / relative).read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise SystemExit(f'Not the pinned official {VERSION} ' + relative)
    return raw.decode('utf-8').replace('\r\n', '\n')

cpp_hash = source_hash('CPP/7zip/UI/FileManager/TextPairs.cpp')
h_hash = source_hash('CPP/7zip/UI/FileManager/TextPairs.h')
sort_hash = source_hash('CPP/Common/MyVector.h')
cpp = pinned('7zip/UI/FileManager/TextPairs.cpp', cpp_hash)
header = pinned('7zip/UI/FileManager/TextPairs.h', h_hash)
vector = pinned('Common/MyVector.h', sort_hash)
declaration = header[header.index('class CPairsStorage'):header.index('\n#endif')].rstrip()
declaration = declaration.replace('  CObjectVector<CTextPair> Pairs;',
    'public:\n  CObjectVector<CTextPair> Pairs;\n  std::shared_ptr<OperationControl> control;\nprivate:')
body = cpp[cpp.index('static const wchar_t kNewLineChar'):].rstrip()
body = body.replace('wchar_t', 'QChar').replace('UString', 'QString')
body = body.replace('(QChar)0xFEFF', 'QChar(0xFEFF)')
body = body.replace("s.RemoveChar(L'\\x0D');", "s.remove(QChar('\\r'));")
body = body.replace('result.Trim();', 'result = trimmed(result);')
body = body.replace('c == 0', 'c.isNull()')
body = body.replace('(const QChar *)srcString + pos', 'srcString.constData() + pos')
body = body.replace('.Len()', '.size()').replace('.IsEmpty()', '.isEmpty()').replace('.Empty()', '.clear()')
body = body.replace('.ID', '.name').replace('.Value', '.value')
body = body.replace("pair.name.Find(L' ')", "pair.name.indexOf(QChar(' '))")
body = body.replace('text.Add_Char(', 'text.append(QChar(')
for statement in ['text.append(QChar(\'\\"\');', "text.append(QChar('\\x0D');"]:
    body = body.replace(statement, statement[:-2] + '));')
body = body.replace('text.Add_Space();', "text.append(QChar(' '));").replace('text.Add_LF();', "text.append(QChar('\\n'));")
body = body.replace('CObjectVector<CTextPair> &pairs)', 'CObjectVector<CTextPair> &pairs, const std::shared_ptr<OperationControl> &control)')
body = body.replace('while (pos < srcString.size())\n  {', 'while (pos < srcString.size())\n  {\n    if (stopped(control)) return false;')
body = body.replace('::GetTextPairs(text, Pairs)', 'GetTextPairs(text, Pairs, control)')
declaration = declaration.replace('UString', 'QString')
sort_start = vector.index('  static void SortRefDown(T*')
# Stop immediately after Sort, before any following template helper.
sort_body = vector[sort_start:vector.index('\n\n', vector.index('    while (size > 1);', sort_start))].rstrip()
assert body.count('text.append(QChar(') == 5
assert sort_body.endswith('  }')

def write(name, content, origin, digest):
    prefix = ('// ' + origin + '\n// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.\n'
        f'// Generated from official 7-Zip {VERSION} by import-file-comments.py.\n'
        '// QString UTF-16/method adapters; worker cancellation only.\n'
        '// Original source SHA-256: ' + digest + '\n\n')
    target = root / 'src/upstream' / name
    data = (prefix + content + '\n').encode('utf-8')
    if not target.exists() or target.read_bytes() != data:
        write_generated(target, data)
    print(target)

write('TextPairsDeclarations.inc', declaration, 'TextPairs.h', h_hash)
write('TextPairs.inc', body, 'TextPairs.cpp', cpp_hash)
write('TextPairsSort.inc', sort_body, 'Common/MyVector.h', sort_hash)
