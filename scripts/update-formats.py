#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Reconcile compiled official 7zz handlers with official registration source."""
import argparse
import json
import pathlib
import re
import subprocess
import os
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('source', type=pathlib.Path)
parser.add_argument('executable', type=pathlib.Path)
parser.add_argument('--helper', type=pathlib.Path)
parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1] / 'resources/formats.json')
args = parser.parse_args()
registrations = {}
for path in (args.source / 'CPP/7zip/Archive').rglob('*.cpp'):
    text = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(), flags=re.S)
    for call in re.finditer(r'(?m)^REGISTER_ARC\w*\(', text):
        fields = re.search(r'"([^"]+)"\s*,\s*"([^"]+)"\s*,\s*(NULL|"[^"]*")', text[call.end():call.end()+300])
        if not fields:
            raise SystemExit('Unrecognized registration: ' + str(path))
        name, extensions, extra = fields.groups()
        extensions = extensions.split()
        extra = extra.strip('"').split() if extra != 'NULL' else []
        registrations[name] = {'extensions': extensions, 'additionalExtensions': dict(zip(extensions, extra)), 'source': str(path.relative_to(args.source))}
hash_path = args.source / 'CPP/7zip/UI/Common/HashCalc.cpp'
hash_text = re.sub(r'/\*.*?\*/|//[^\n]*', '', hash_path.read_text(), flags=re.S)
hash_exts = re.search(r'item.AddExts\(UString\s*\((.*?)\),\s*UString', hash_text, re.S)
registrations['Hash'] = {'extensions': ''.join(re.findall(r'"([^"]*)"', hash_exts[1])).split(), 'additionalExtensions': {}, 'source': str(hash_path.relative_to(args.source))}
info = subprocess.check_output([str(args.executable), 'i'], text=True)
version = re.search(r'7-Zip[^\n]*\b(\d+\.\d+)\b', info)[1]
formats = []
for line in info.split('Formats:\n', 1)[1].split('\nCodecs:', 1)[0].splitlines():
    row = re.match(r'^\s+([C .A-Za-z+]+?)\s{2,}(\S+)\s+', line)
    if not row:
        continue
    flags, name = row.groups()
    if name not in registrations:
        raise SystemExit('Compiled handler missing from source: ' + name)
    formats.append({'name': name, 'canCreate': flags.strip().startswith('C'), **registrations[name]})
if not formats or len({f['name'] for f in formats}) != len(formats):
    raise SystemExit('Malformed compiled format inventory')
helper = args.helper or args.executable.with_name('7zz-progress')
with tempfile.TemporaryDirectory(prefix='7zip-handler-capabilities-') as temporary:
    output = pathlib.Path(temporary) / 'formats.json'
    subprocess.run([str(helper), 'i'], env={**os.environ, 'SEVENZIP_PORT_FORMATS_PATH': str(output)},
                   stdout=subprocess.DEVNULL, check=True)
    capabilities = {item.pop('name'): item for item in json.loads(output.read_text())}
for item in formats:
    if item['name'] not in capabilities:
        raise SystemExit('Helper is missing compiled handler: ' + item['name'])
    item['capabilities'] = capabilities[item['name']]
data = {'version': version, 'formats': formats, 'extensions': sorted({e for f in formats for e in f['extensions']})}
encoded = json.dumps(data, ensure_ascii=False, indent=2)+'\n'
if not args.output.exists() or args.output.read_text() != encoded:
    args.output.write_text(encoded)
print(f"{version}: {len(formats)} handlers, {len(data['extensions'])} extensions")
