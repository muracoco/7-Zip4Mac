#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Create genuine ZIP duplicates/implicit directories and unmounted NTFS ADS."""
import pathlib
import io
import shutil
import stat
import struct
import subprocess
import sys
import warnings
import tarfile
import zipfile

root = pathlib.Path(sys.argv[1])
tools = pathlib.Path(sys.argv[2])
root.mkdir()  # Refuse existing directories; never modify user test data.
warnings.filterwarnings('ignore', message='Duplicate name')
with zipfile.ZipFile(root / 'duplicate.zip', 'w') as archive:
    archive.writestr('same.txt', b'one')
    archive.writestr('same.txt', b'a different second payload')
    archive.writestr('implicit/sub/deep.txt', b'deep payload')
    archive.writestr('empty/', b'')
with zipfile.ZipFile(root / 'duplicate-comments.zip', 'w') as archive:
    for data, comment in ((b'one', b'first comment'), (b'a different second payload', b'second comment')):
        item = zipfile.ZipInfo('same.txt')
        item.comment = comment
        archive.writestr(item, data)
    archive.writestr('implicit/sub/deep.txt', b'deep payload')
    archive.writestr('empty/', b'')
shutil.copyfile(root / 'duplicate.zip', root / 'corrupt-duplicate.zip')
with zipfile.ZipFile(root / 'corrupt-duplicate.zip') as archive:
    second = archive.infolist()[1]
with (root / 'corrupt-duplicate.zip').open('r+b') as archive:
    archive.seek(second.header_offset + 26)
    name_length, extra_length = struct.unpack('<HH', archive.read(4))
    archive.seek(second.header_offset + 30 + name_length + extra_length)
    byte = archive.read(1)
    archive.seek(-1, 1)
    archive.write(bytes([byte[0] ^ 1]))  # Only the second sibling has a bad CRC.
with zipfile.ZipFile(root / 'selected-safety.zip', 'w') as archive:
    archive.writestr('safe.txt', b'safe selected payload')
    archive.writestr('../escaped.txt', b'unsafe traversal')
    link = zipfile.ZipInfo('unsafe-link'); link.create_system = 3
    link.external_attr = (stat.S_IFLNK | 0o777) << 16
    archive.writestr(link, '../outside')
with tarfile.open(root / 'pax.tar', 'w', format=tarfile.PAX_FORMAT) as archive:
    for name, payload in [('long/' + 'x' * 110 + '.txt', b'PAX long path'), ('after.txt', b'unchanged PAX entry')]:
        item = tarfile.TarInfo(name); item.size = len(payload); item.mtime = 1750000000.123456
        archive.addfile(item, io.BytesIO(payload))
(root / 'payload.txt').write_bytes(b'ordinary data')
(root / 'stream.txt').write_bytes(b'alternate data')
image = root / 'streams.ntfs'
with image.open('wb') as stream:
    stream.truncate(64 * 1024 * 1024)
for args in [
    [tools / 'mkntfs', '-F', '-Q', '-s', '512', image],
    [tools / 'ntfscp', '-f', image, root / 'payload.txt', '/payload.txt'],
    [tools / 'ntfscp', '-f', '-N', 'details', image, root / 'stream.txt', '/payload.txt'],
]:
    subprocess.run(list(map(str, args)), check=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
print(root)
