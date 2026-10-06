#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Owned ZIP container variants and independent packed-byte verification.

Encryption/compression comes exclusively from official 7zz. This script only
changes container headers/descriptors, never encrypted/compressed payloads.
"""
import hashlib
import os
import pathlib
import struct
import sys
import zipfile
import zlib

TARGET = "日本語 space.txt"
RENAMED = "renamed 日本語.txt"
COMMENT = "descriptor コメント\nsecond line"


def extra_fields(data):
    pos = 0
    while pos < len(data):
        tag, size = struct.unpack_from("<HH", data, pos)
        end = pos + 4 + size
        assert end <= len(data), "invalid extra field"
        yield tag, data[pos:end]
        pos = end


def payload(data, info):
    name, extra = struct.unpack_from("<HH", data, info.header_offset + 26)
    start = info.header_offset + 30 + name + extra
    return data[start:start + info.compress_size]


def prepare(root, variant):
    path = root / "fixture.zip"
    data = path.read_bytes()
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        central_pos = archive.start_dir
        local = bytearray()
        central = bytearray()
        for info in infos:
            header = bytearray(data[info.header_offset:info.header_offset + 30])
            name_len, extra_len = struct.unpack_from("<HH", header, 26)
            start = info.header_offset + 30
            name = data[start:start + name_len]
            extra = data[start + name_len:start + name_len + extra_len]
            cd = bytearray(data[central_pos:central_pos + 46])
            cn, ce, cc = struct.unpack_from("<HHH", cd, 28)
            cd_name = data[central_pos + 46:central_pos + 46 + cn]
            cd_extra = data[central_pos + 46 + cn:central_pos + 46 + cn + ce]
            cd_comment = data[central_pos + 46 + cn + ce:central_pos + 46 + cn + ce + cc]
            central_pos += 46 + cn + ce + cc
            descriptor = b""
            if not info.is_dir() and (info.flag_bits & 9):
                if not info.flag_bits & 8:
                    # Official ZipAddCommon rejects sequential Store. Its
                    # seekable ZipCrypto Store output checks the CRC byte.
                    # Set DOS check byte equal to that existing byte, preserving
                    # all ciphertext, to construct a valid descriptor fixture.
                    assert info.flag_bits & 1 and info.compress_type == 0
                    check = info.CRC >> 24
                    assert check < 192, "DOS hour must remain valid"
                    for target_header, flag_offset, time_offset in ((header, 6, 10), (cd, 8, 12)):
                        flags = struct.unpack_from("<H", target_header, flag_offset)[0]
                        time = struct.unpack_from("<I", target_header, time_offset)[0]
                        struct.pack_into("<H", target_header, flag_offset, flags | 8)
                        # Zero the three low minute bits so any CRC high
                        # byte with hour < 24 also yields a valid minute.
                        struct.pack_into("<I", target_header, time_offset, (time & ~0xFFE0) | check << 8)
                    struct.pack_into("<III", header, 14, 0, 0, 0)
                if variant == "forced64" or variant == "unsigned64":
                    extra = b"".join(field for tag, field in extra_fields(extra) if tag != 1)
                    extra += struct.pack("<HHQQ", 1, 16, info.file_size, info.compress_size)
                    struct.pack_into("<H", header, 4, 45)
                    struct.pack_into("<II", header, 18, 0xFFFFFFFF, 0xFFFFFFFF)
                    descriptor = struct.pack("<IIQQ", 0x08074B50, info.CRC, info.compress_size, info.file_size)
                else:
                    descriptor = struct.pack("<IIII", 0x08074B50, info.CRC, info.compress_size, info.file_size)
                if variant.startswith("unsigned"):
                    descriptor = descriptor[4:]
            if variant == "time-skew" and info.filename == TARGET:
                # NTFS/UT time differs from DOS time used by the already
                # encrypted ZipCrypto check byte. Keep DOS/encrypted bytes.
                unix_time = 1609503000
                ft = (unix_time + 11644473600) * 10000000
                cd_extra = b"".join(field for tag, field in extra_fields(cd_extra) if tag not in (0xA, 0x5455))
                cd_extra += struct.pack("<HHIHHQQQ", 0xA, 32, 0, 1, 24, ft, ft, ft)
                extra = b"".join(field for tag, field in extra_fields(extra) if tag not in (0xA, 0x5455))
                extra += struct.pack("<HHBI", 0x5455, 5, 1, unix_time)
            if info.header_offset == 0:
                if variant == "warning-time":
                    # ZipIn::ReadLocalItem accepts an invalid DOS timestamp
                    # as HeadersWarning; central/NTFS times stay unchanged.
                    struct.pack_into("<I", header, 10, 0xFFFFFFFF)
                elif variant == "warning-extra":
                    # The optional unknown field is shorter than its declared
                    # size. ZipIn::ReadExtra warns and skips it, not payload.
                    extra += struct.pack("<HH", 0xCAFE, 16) + b"x"
            struct.pack_into("<H", header, 28, len(extra))
            struct.pack_into("<I", cd, 42, len(local))
            struct.pack_into("<H", cd, 30, len(cd_extra))
            local += header + name + extra + payload(data, info) + descriptor
            central += cd + cd_name + cd_extra + cd_comment
        global_comment = b"owned archive comment\nPath = not-an-entry"
        end = struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, len(infos), len(infos), len(central), len(local), len(global_comment))
        path.write_bytes(local + central + end + global_comment)
        (root / "original.zip").write_bytes(path.read_bytes())


def verify(root, operation):
    before = (root / "original.zip").read_bytes()
    after = (root / "fixture.zip").read_bytes()
    with zipfile.ZipFile(root / "original.zip") as old, zipfile.ZipFile(root / "fixture.zip") as new:
        expected_names = [i.filename.replace(TARGET, RENAMED) if operation == "rename" else i.filename for i in old.infolist()]
        assert new.namelist() == expected_names
        assert old.comment == new.comment
        for left, right in zip(old.infolist(), new.infolist()):
            assert left.CRC == right.CRC and left.file_size == right.file_size
            assert left.compress_type == right.compress_type and left.date_time == right.date_time
            assert payload(before, left) == payload(after, right), "packed/encrypted payload changed"
            if left.filename == TARGET:
                assert right.comment.decode() == COMMENT
                if left.flag_bits & 1 and left.compress_type != 99:
                    assert right.flag_bits & 8, "ZipCrypto descriptor bit lost"
                    # Independently inspect both headers and the serialized
                    # descriptor, rather than relying on tolerant decoders.
                    flags = struct.unpack_from("<H", after, right.header_offset + 6)[0]
                    time = struct.unpack_from("<I", after, right.header_offset + 10)[0]
                    old_time = struct.unpack_from("<I", before, left.header_offset + 10)[0]
                    assert flags & 8 and time == old_time
                    name, extra = struct.unpack_from("<HH", after, right.header_offset + 26)
                    start = right.header_offset + 30 + name + extra + right.compress_size
                    sig, crc, packed, size = struct.unpack_from("<IIII", after, start)
                    assert (sig, crc, packed, size) == (0x08074B50, right.CRC, right.compress_size, right.file_size)
            else:
                assert left.comment == right.comment and left.extra == right.extra
        print("Independent ZIP check passed: names, comments, DOS time, packed/encrypted bytes and descriptor")


def compare_extraction(root, renamed=True):
    source, output = root / "input", root / "extracted"
    for entry in source.rglob("*"):
        name = entry.relative_to(source).as_posix()
        if renamed:
            name = name.replace(TARGET, RENAMED)
        restored = output / name
        if entry.is_dir():
            assert restored.is_dir()
        else:
            assert hashlib.sha256(entry.read_bytes()).digest() == hashlib.sha256(restored.read_bytes()).digest()
    print("Pristine official 7zz extraction matches source SHA-256 and directories")


def verify_warning_updates(root):
    before, after = (root / "original.zip").read_bytes(), (root / "fixture.zip").read_bytes()
    with zipfile.ZipFile(root / "original.zip") as old, zipfile.ZipFile(root / "fixture.zip") as new:
        left, right = old.getinfo("keep.txt"), new.getinfo("keep.txt")
        assert payload(before, left) == payload(after, right)
        assert (left.CRC, left.date_time, left.extra, left.comment) == (right.CRC, right.date_time, right.extra, right.comment)
        assert old.comment == new.comment
        assert new.read(RENAMED) == (root / "replacement.txt").read_bytes()
        assert new.read("added 日本語.txt") == (root / "added 日本語.txt").read_bytes()
        assert "folder/child.txt" not in new.namelist()
        assert "new 日本語 folder/" in new.namelist() and "empty/" in new.namelist()
    assert (root / "extracted" / RENAMED).read_bytes() == (root / "replacement.txt").read_bytes()
    assert (root / "extracted" / "keep.txt").read_bytes() == (root / "input" / "keep.txt").read_bytes()
    assert (root / "extracted" / "new 日本語 folder").is_dir()
    print("Warning-bearing ZIP: six updates, unchanged packed keep item, replacement/add bytes and directories verified")


if __name__ == "__main__":
    mode, root = sys.argv[1], pathlib.Path(sys.argv[2])
    if mode == "store-input":
        for entry in (root / "input").rglob("*"):
            if entry.is_file():
                data = entry.read_bytes()
                while zlib.crc32(data) >> 24 >= 192:
                    data += b"x"
                entry.write_bytes(data)
                os.utime(entry, (1583298368, 1583298368))
    elif mode.startswith("prepare-"):
        prepare(root, mode.removeprefix("prepare-"))
    elif mode in ("comment", "rename"):
        verify(root, mode)
    elif mode in ("extraction", "extraction-original"):
        compare_extraction(root, mode == "extraction")
    elif mode == "warning-updates":
        verify_warning_updates(root)
    else:
        raise SystemExit("unknown owned fixture mode")
