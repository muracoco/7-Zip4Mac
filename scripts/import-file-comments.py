#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Import official TextPairs operations and their original pointer heap sort."""
import hashlib
import pathlib
import sys

root = pathlib.Path(__file__).resolve().parent.parent
upstream = pathlib.Path(sys.argv[1]) / 'CPP'

def pinned(relative, expected):
    raw = (upstream / relative).read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise SystemExit('Not the pinned official 26.03 ' + relative)
    return raw.decode('utf-8').replace('\r\n', '\n')

cpp_hash = '578937fb064effd9977944d42d0a5602ab10ccc81b8724959b8063073d8f6478'
h_hash = 'ee3653ed58c3ea74225ef41fb2bdcfea35fe198562d60124faad6c8f94d641d1'
sort_hash = '9d0b00aedc02551a75ad643d4c4d57d9868a23abd5e4c70e9a3e1662da4b9a8b'
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
        '// Generated from official 7-Zip 26.03 by import-file-comments.py.\n'
        '// QString UTF-16/method adapters; worker cancellation only.\n'
        '// Original source SHA-256: ' + digest + '\n\n')
    target = root / 'src/upstream' / name
    data = (prefix + content + '\n').encode('utf-8')
    if not target.exists() or target.read_bytes() != data:
        target.write_bytes(data)
    print(target)

write('TextPairsDeclarations.inc', declaration, 'TextPairs.h', h_hash)
write('TextPairs.inc', body, 'TextPairs.cpp', cpp_hash)
write('TextPairsSort.inc', sort_body, 'Common/MyVector.h', sort_hash)
