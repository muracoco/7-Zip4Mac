#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Import arithmetic bodies from the pinned official Windows GUI, without Win32."""
import argparse
import hashlib
import pathlib
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("source", type=pathlib.Path, help="Official 7-Zip 26.03 source directory")
args = parser.parse_args()
source = args.source / "CPP/7zip/UI/GUI/CompressDialog.cpp"
raw = source.read_bytes()
if hashlib.sha256(raw).hexdigest() != "53d673d78e7936880481be1017c37ca95a1e4b64419d74d32035ce703d06c7d4":
    parser.error("CompressDialog.cpp is not the pinned official 26.03 source")
text = raw.decode("utf-8").replace("\r\n", "\n")

def function(signature):
    start = text.index(signature)
    opening = text.index("{", start)
    depth = 1
    cursor = opening + 1
    while depth:
        depth += (text[cursor] == "{") - (text[cursor] == "}")
        cursor += 1
    return text[start:cursor]

chunk = function("static UInt64 Get_Lzma2_ChunkSize(UInt64 dict)")
memory = function("UInt64 CCompressDialog::GetMemoryUsage_Threads_Dict_DecompMem(")
threads = function("void CCompressDialog::SetNumThreads2()")
threads = threads[threads.index("  const int methodID = GetMethodID();"):threads.index("  _auto_NumThreads = autoThreads;")]
threads = "UInt32 CCompressDialog::AutoThreads(UInt32 numCPUs, UInt32 numHardwareThreads)\n{\n" + threads + "  return autoThreads;\n}"
maximum = threads[threads.index("  const int methodID = GetMethodID();"):threads.index("  UInt32 autoThreads = numCPUs;")]
maximum = "UInt32 CCompressDialog::MaximumThreads(UInt32 numHardwareThreads)\n{\n" + maximum + "  return numAlgoThreadsMax;\n}"
solid = function("void CCompressDialog::SetSolidBlockSize2()")
solid = solid[solid.index("  const UInt64 cs = Get_Lzma2_ChunkSize(dict);"):solid.index("  _auto_Solid = blockSize;")]
solid = "UInt64 CCompressDialog::AutoSolid(UInt64 dict)\n{\n  const bool is7z = format == \"7z\";\n" + solid + "  return blockSize;\n}"
output = pathlib.Path(__file__).resolve().parent.parent / "src/upstream/CompressMath.inc"
output.parent.mkdir(parents=True, exist_ok=True)
header = """// CompressDialog.cpp
// Copyright (C) 1999-2026 Igor Pavlov. GNU LGPL-2.1-or-later.
// Generated from official 7-Zip 26.03 by scripts/import-compression-math.py.
// Original arithmetic bodies retained. Thread/solid bodies omit Win32 combo
// plumbing; signatures return results through the adapter in CompressionMath.cpp.
// Original source SHA-256: """ + hashlib.sha256(raw).hexdigest() + "\n\n"
output.write_text(header + "\n\n".join([chunk, memory, threads, maximum, solid]) + "\n", encoding="utf-8")
print(output)

# Retain the official format/method tables and numeric combo builders. Registry
# lookup and Win32 geometry are boundaries: the adapter supplies saved values.
tables = text[text.index("enum EMethodID"):text.index("static bool IsMethodSupportedBySfx")]
resources = (source.parent / "CompressDialogRes.h").read_bytes()
if hashlib.sha256(resources).hexdigest() != "a100dd6a66b7b225c34d4dc94cb16f65e4770a88d0aee3774f2a800daba70c81":
    parser.error("CompressDialogRes.h is not the pinned official 26.03 source")
ids = dict(re.findall(r"^#define\s+(\w+)\s+(\d+)\s*$", resources.decode(), re.M))
levels = text[text.index("static const UInt32 g_Levels[]"):text.index("enum EMethodID")]
levels = re.sub(r"IDS_METHOD_\w+", lambda match: ids[match[0]], levels)
tables = levels + tables
dictionary = function("void CCompressDialog::SetDictionary2()")
dictionary = dictionary[dictionary.index("  if (methodID < 0)"):]
dictionary = "void CCompressDialog::SetDictionary2(UInt32 defaultDict)\n{\n  m_Dictionary.ResetContent();\n  _auto_Dict = (UInt32)(Int32)-1;\n  const int methodID = GetMethodID();\n  const UInt32 level = GetLevel2();\n" + dictionary
order = function("void CCompressDialog::SetOrder2()")
order = order[order.index("  const int methodID = GetMethodID();"):]
order = "void CCompressDialog::SetOrder2(UInt32 defaultOrder)\n{\n  m_Order.ResetContent();\n  _auto_Order = 1;\n" + order
output = output.with_name("CompressControls.inc")
output.write_text(header + "// Numeric combo bodies omit registry lookup and Win32 geometry; saved values are arguments.\n\n" + dictionary + "\n\n" + order + "\n", encoding="utf-8")
print(output)
output = output.with_name("CompressTables.inc")
output.write_text(header + tables, encoding="utf-8")
print(output)
