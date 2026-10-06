#!/usr/bin/env python3
"""Owned ZIP fixtures; independent metadata and raw payload verification."""
import pathlib
import struct
import sys
import zipfile
import warnings

mode, root = sys.argv[1], pathlib.Path(sys.argv[2])
target = "日本語 space.txt"
if mode.startswith("create"):
    root.mkdir(parents=True, exist_ok=True)
    method = {"create-bzip2": zipfile.ZIP_BZIP2, "create-lzma": zipfile.ZIP_LZMA}.get(mode, zipfile.ZIP_DEFLATED)
    with zipfile.ZipFile(root / "fixture.zip", "w", compression=method) as archive:
        info = zipfile.ZipInfo(target, (2020, 3, 4, 5, 6, 8))
        info.compress_type = method
        info.comment = "original comment 日本語".encode()
        info.external_attr = 0o100644 << 16
        archive.writestr(info, b"target compressed payload\n" * 1000)
        info = zipfile.ZipInfo("keep.txt", (2021, 2, 3, 4, 5, 6))
        info.compress_type = method
        info.comment = b"two\nlines\nPath = bogus\n}"
        info.extra = struct.pack("<HH4s", 0xCAFE, 4, b"kept")
        archive.writestr(info, b"unrelated payload\n" * 100)
        archive.writestr("virtual/child.txt", b"virtual folder")
        if mode == "create-duplicate":
            warnings.filterwarnings("ignore", message="Duplicate name")
            for value in (b"first", b"second"):
                info = zipfile.ZipInfo("duplicate.txt")
                info.comment = value
                archive.writestr(info, value)
        if mode == "create-zip64":
            for n in range(65536):
                archive.writestr(f"empty-{n:05d}", b"")
        archive.comment = b"global archive comment\nsecond line"
elif mode == "verify-encrypted":
    before = (root / "original.zip").read_bytes()
    after = (root / "archive.zip").read_bytes()
    with zipfile.ZipFile(root / "original.zip") as old, zipfile.ZipFile(root / "archive.zip") as new:
        assert old.comment == new.comment and old.namelist() == new.namelist()
        for left, right in zip(old.infolist(), new.infolist()):
            assert left.CRC == right.CRC and left.file_size == right.file_size
            def payload(data, entry):
                name, extra = struct.unpack_from("<HH", data, entry.header_offset + 26)
                start = entry.header_offset + 30 + name + extra
                return data[start:start + entry.compress_size]
            assert payload(before, left) == payload(after, right)
        print("Independent raw ZIP payload verification passed (including encrypted bytes)")
elif mode == "verify":
    before = (root / "original.zip").read_bytes()
    after = (root / "fixture.zip").read_bytes()
    with zipfile.ZipFile(root / "original.zip") as old, zipfile.ZipFile(root / "fixture.zip") as new:
        assert old.comment == new.comment
        assert old.namelist() == new.namelist()
        assert new.getinfo(target).comment == "updated コメント".encode()
        old_bounds = [i.header_offset for i in old.infolist()] + [old.start_dir]
        new_bounds = [i.header_offset for i in new.infolist()] + [new.start_dir]
        for index, (left, right) in enumerate(zip(old.infolist(), new.infolist())):
            assert left.CRC == right.CRC and left.file_size == right.file_size
            assert left.compress_type == right.compress_type and left.date_time == right.date_time
            assert old.read(left) == new.read(right)
            if left.filename == target:
                def payload(data, entry):
                    name, extra = struct.unpack_from("<HH", data, entry.header_offset + 26)
                    start = entry.header_offset + 30 + name + extra
                    return data[start:start + entry.compress_size]
                assert payload(before, left) == payload(after, right)
            else:
                assert left.comment == right.comment and left.extra == right.extra
                assert before[old_bounds[index]:old_bounds[index + 1]] == after[new_bounds[index]:new_bounds[index + 1]]
        print(f"Independent ZIP verification passed: {len(old.infolist())} entries, raw payload/local records, comments and decoded bytes")
else:
    raise SystemExit("unknown fixture mode")
