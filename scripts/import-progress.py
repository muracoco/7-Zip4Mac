#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Create a pinned adapter overlay without editing the official source tree."""
from upstream_support import VERSION, source_hash, aggregate_hash, artifact_hash, output_root, write_generated
import hashlib
import pathlib
import sys

source, output = map(lambda value: pathlib.Path(value).resolve(), sys.argv[1:])
base = "CPP/7zip/UI/"
hashes = {
    "../Archive/Zip/ZipUpdate.cpp": source_hash('CPP/7zip/Archive/Zip/ZipUpdate.cpp'),
    "Common/LoadCodecs.cpp": source_hash('CPP/7zip/UI/Common/LoadCodecs.cpp'),
    "../Common/FilePathAutoRename.cpp": source_hash('CPP/7zip/Common/FilePathAutoRename.cpp'),
    "Common/ArchiveExtractCallback.cpp": source_hash('CPP/7zip/UI/Common/ArchiveExtractCallback.cpp'),
    "Console/BenchCon.cpp": source_hash('CPP/7zip/UI/Console/BenchCon.cpp'),
    "Agent/AgentProxy.cpp": source_hash('CPP/7zip/UI/Agent/AgentProxy.cpp'),
    "Console/List.cpp": source_hash('CPP/7zip/UI/Console/List.cpp'),
    "Console/Main.cpp": source_hash('CPP/7zip/UI/Console/Main.cpp'),
    "Console/UpdateCallbackConsole.cpp": source_hash('CPP/7zip/UI/Console/UpdateCallbackConsole.cpp'),
    "Console/ExtractCallbackConsole.cpp": source_hash('CPP/7zip/UI/Console/ExtractCallbackConsole.cpp'),
    "Console/ExtractCallbackConsole.h": source_hash('CPP/7zip/UI/Console/ExtractCallbackConsole.h'),
    "Console/HashCon.cpp": source_hash('CPP/7zip/UI/Console/HashCon.cpp'),
    "Common/Extract.cpp": source_hash('CPP/7zip/UI/Common/Extract.cpp'),
    "Common/Update.cpp": source_hash('CPP/7zip/UI/Common/Update.cpp'),
    "Common/UpdateCallback.cpp": source_hash('CPP/7zip/UI/Common/UpdateCallback.cpp'),
}
files = {}
for path, expected in hashes.items():
    data = (source / base / path).read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise SystemExit("Unsupported upstream source: " + path)
    files[(source / base / path).resolve().relative_to(source).as_posix()] = data.decode("utf-8").replace("\r\n", "\n")

def replace(path, old, new):
    key = (source / base / path).resolve().relative_to(source).as_posix()
    if files[key].count(old) != 1:
        raise SystemExit("Expected one upstream anchor: " + path + ": " + old)
    files[key] = files[key].replace(old, new)

def enter(path, signature, body):
    replace(path, signature + "\n{", signature + "\n{\n  " + body)

# Metadata-only ZipCrypto updates must retain the descriptor bit and DOS time:
# the existing encrypted header checks that time, rather than the CRC, when
# bit 3 is set (ZipHandler.cpp). Copy only packed bytes and let the original
# writer regenerate the descriptor, including canonical 32/64-bit widths.
# All compression/encryption routines remain the original implementation.
zip_update = "../Archive/Zip/ZipUpdate.cpp"
replace(zip_update, "  UInt64 rangeSize;\n\n  RINOK(archive.ClearRestriction())",
        "  UInt64 rangeSize;\n"
        "  const bool portKeepCryptoDescriptor = ui.NewProps && itemEx.IsEncrypted()\n"
        "      && !itemEx.IsAesEncrypted() && itemEx.HasDescriptor();\n\n"
        "  RINOK(archive.ClearRestriction())")
replace(zip_update, "    if (item.HasDescriptor())\n    {\n      // we know compressed",
        "    if (item.HasDescriptor() && !portKeepCryptoDescriptor)\n    {\n      // we know compressed")
replace(zip_update, "    // if we don't change Comment, we keep Comment from OldProperties\n    Copy_From_UpdateItem_To_ItemOut(ui, item);",
        "    // if we don't change Comment, we keep Comment from OldProperties\n"
        "    Copy_From_UpdateItem_To_ItemOut(ui, item);\n"
        "    if (portKeepCryptoDescriptor) item.Time = itemEx.Time;")
replace(zip_update, "    rangeSize = item.GetPackSizeWithDescriptor();",
        "    rangeSize = portKeepCryptoDescriptor ? item.PackSize : item.GetPackSizeWithDescriptor();")
replace(zip_update, "  archive.MoveCurPos(rangeSize);\n  return res;",
        "  archive.MoveCurPos(rangeSize);\n"
        "  if (res == S_OK && portKeepCryptoDescriptor)\n"
        "    archive.WriteLocalHeader_Replace(item);\n"
        "  return res;")

for path in ("Console/UpdateCallbackConsole.cpp", "Console/ExtractCallbackConsole.cpp", "Console/HashCon.cpp", "Common/Extract.cpp"):
    replace(path, '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "NativeProgress.h"')

replace("Console/List.cpp", '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "NativeMetadata.h"\n#include "NativeFolderUpdate.h"')
replace("Console/BenchCon.cpp", '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "NativeBenchmark.h"')
replace("Console/Main.cpp", '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "NativeTempFiles.h"\n#include "NativeProgress.h"')
replace("Console/Main.cpp", '#include "../../../Common/MyInitGuid.h"', '#include "../../../Common/MyInitGuid.h"\n#include "NativeMetadata.h"\n#include "NativeAgentSelection.h"')
# Official static console loading omits the time flags that DLL loading copies.
# Retain the registered handler's flags for the Windows Options adapter.
replace("Common/LoadCodecs.cpp", '    item.Flags = arc.Flags;', '    item.Flags = arc.Flags;\n    item.TimeFlags = arc.TimeFlags;')
replace("Console/Main.cpp", '  if (options.Command.CommandType == NCommandType::kInfo)\n  {',
        '  if (options.Command.CommandType == NCommandType::kInfo)\n  {\n    if (PortMetadata::WriteFormats(codecs) != S_OK) return NExitCode::kFatalError;')
replace("Console/Main.cpp", '      if (!options.HashMethods.IsEmpty())', '      if (eo.TestMode && PortSelection::Requested()) hashCalc = &hb;\n      if (!options.HashMethods.IsEmpty())')
replace("Console/Main.cpp", '          hashCalc, errorMessage, stat);', '          hashCalc, errorMessage, stat);\n      PortProgress::Completion(&stat, hashCalc ? &hb : nullptr);')
replace("Console/Main.cpp", '  #if defined(MY_CPU_SIZEOF_POINTER)', '  if (PortTempFiles::Requested()) return PortTempFiles::Run();\n  #if defined(MY_CPU_SIZEOF_POINTER)')
replace("Console/BenchCon.cpp", "  CPrintBenchCallback callback;", "  if (PortBenchmark::Requested())\n    return PortBenchmark::Run(EXTERNAL_CODECS_LOC_VARS props, numIterations);\n  CPrintBenchCallback callback;")
replace("Console/List.cpp", "void CFieldPrinter::AddProp(const wchar_t *name, PROPID propID, bool isRawProp)",
        "UString PortMetadata::PropertyName(PROPID id, const wchar_t *name)\n{\n  AString a; UString u; GetPropName(id, name, a, u);\n  if (!a.IsEmpty()) u = a.Ptr();\n  return u;\n}\n\nvoid CFieldPrinter::AddProp(const wchar_t *name, PROPID propID, bool isRawProp)")
replace("Console/List.cpp", "    const CArc &arc = arcLink.Arcs.Back();", "    if (PortFolderUpdate::Requested())\n      return PortFolderUpdate::Run(arcLink, codecs, openCallback);\n    RINOK(PortMetadata::Write(arcLink, codecs))\n    const CArc &arc = arcLink.Arcs.Back();")
# Only the newly synthesized Windows folder item uses FILETIME precision.
# All existing properties still come directly from the opened handler.
replace("Common/UpdateCallback.cpp", '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "NativeFolderUpdate.h"')
for kind in ('C', 'A', 'M'):
    original = f'case kpid{kind}Time:  PropVariant_SetFrom_FiTime(prop, di.{kind}Time); break;'
    replace("Common/UpdateCallback.cpp", original,
            original.replace(' break;', ' if (PortFolderUpdate::Requested()) prop.Set_FtPrec(k_PropVar_TimePrec_100ns); break;'))
# The Windows raw-name fast path is not used on POSIX. Keep the fallback BSTR
# alive until AllocStringAndCopy; the original block-local variant dies first.
replace("Agent/AgentProxy.cpp", "      const wchar_t *s;\n      unsigned len;", "      NCOM::CPropVariant prop;\n      const wchar_t *s;\n      unsigned len;")
replace("Agent/AgentProxy.cpp", "        NCOM::CPropVariant prop;\n        RINOK(arc.Archive->GetProperty(i, kpidName, &prop))", "        RINOK(arc.Archive->GetProperty(i, kpidName, &prop))")

update = "Console/UpdateCallbackConsole.cpp"
enter(update, "HRESULT CUpdateCallbackConsole::SetNumItems(const CArcToDoStat &stat)", 'PortProgress::Begin("compress"); PortProgress::Files(stat.Get_NumDataItems_Total());')
enter(update, "HRESULT CUpdateCallbackConsole::StartArchive(const wchar_t *name, bool updating)", 'PortProgress::Begin("compress", name, true);')
enter(update, "HRESULT CUpdateCallbackConsole::StartScanning()", 'PortProgress::Begin("compress"); PortProgress::Status("scan", true);')
replace(update, 'const CDirItemsStat &st, const FString &path, bool /* isDir */', 'const CDirItemsStat &st, const FString &path, bool isDir')
enter(update, "HRESULT CUpdateCallbackConsole::ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)", 'PortProgress::Scanning(st.NumFiles + st.NumAltStreams, st.GetTotalBytes(), fs2us(path), isDir);')
enter(update, "HRESULT CUpdateCallbackConsole::FinishScanning(const CDirItemsStat &st)", 'PortProgress::Scanning(st.NumFiles + st.NumAltStreams, st.GetTotalBytes(), nullptr, true); PortProgress::Status("", true);')
enter(update, "HRESULT CUpdateCallbackConsole::ReportUpdateOperation(UInt32 op, const wchar_t *name, bool isDir)", 'PortProgress::UpdateOperation(op, name, isDir);')
enter(update, "void CCallbackConsoleBase::CommonError(const FString &path, DWORD systemError, bool isWarning)", 'PortProgress::Error(-1, false, fs2us(path), NError::MyFormatMessage(systemError));')
enter(update, "HRESULT CUpdateCallbackConsole::SetTotal(UInt64 size)", "PortProgress::Total(size);")
enter(update, "HRESULT CUpdateCallbackConsole::SetCompleted(const UInt64 *completeValue)", "PortProgress::Completed(completeValue);")
replace(update, "const UInt64 * /* inSize */, const UInt64 * /* outSize */", "const UInt64 *inSize, const UInt64 *outSize")
enter(update, "HRESULT CUpdateCallbackConsole::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize)", "PortProgress::Ratio(inSize, outSize);")
enter(update, "HRESULT CUpdateCallbackConsole::GetStream(const wchar_t *name, bool isDir, bool isAnti, UInt32 mode)", "PortProgress::Current(name, isDir);")
enter(update, "HRESULT CUpdateCallbackConsole::SetOperationResult(Int32 /* opRes */)", "PortProgress::FileDone();")
enter(update, "HRESULT CUpdateCallbackConsole::FinishArchive(const CFinishArchiveStat &st)", "PortProgress::Finish();")

header = "Console/ExtractCallbackConsole.h"
replace(header, "  public IFolderArchiveExtractCallback,", "  public IFolderArchiveExtractCallback,\n  public ICompressProgressInfo,")
replace(header, "  Z7_COM_QI_ENTRY(IFolderArchiveExtractCallback2)", "  Z7_COM_QI_ENTRY(IFolderArchiveExtractCallback2)\n  Z7_COM_QI_ENTRY(ICompressProgressInfo)")
replace(header, "  Z7_IFACE_COM7_IMP(IProgress)", "  Z7_IFACE_COM7_IMP(IProgress)\n  Z7_IFACE_COM7_IMP(ICompressProgressInfo)")
extract = "Console/ExtractCallbackConsole.cpp"
replace(extract, '#include "NativeProgress.h"', '#include "NativeProgress.h"\n#include "NativeMetadata.h"')
replace(extract, "void PrintErrorFlags(CStdOutStream &so, const char *s, UInt32 errorFlags);", "UString PortMetadata::ErrorFlags(UInt32 flags)\n{\n  return UString(GetOpenArcErrorMessage(flags).Ptr());\n}\n\nvoid PrintErrorFlags(CStdOutStream &so, const char *s, UInt32 errorFlags);")
enter(extract, "HRESULT CExtractCallbackConsole::BeforeOpen(const wchar_t *name, bool testMode)", 'PortProgress::Begin(testMode ? "test" : "extract", name);')
enter(extract, "Z7_COM7F_IMF(CExtractCallbackConsole::SetTotal(UInt64 size))", "PortProgress::Total(size);")
enter(extract, "Z7_COM7F_IMF(CExtractCallbackConsole::SetCompleted(const UInt64 *completeValue))", "PortProgress::Completed(completeValue);")
enter(extract, "Z7_COM7F_IMF(CExtractCallbackConsole::PrepareOperation(const wchar_t *name, Int32 isFolder, Int32 askExtractMode, const UInt64 *position))", 'PortProgress::Current(name, isFolder != 0); PortProgress::Status(askExtractMode == NArchive::NExtract::NAskMode::kExtract ? "extract" : askExtractMode == NArchive::NExtract::NAskMode::kTest ? "test" : askExtractMode == NArchive::NExtract::NAskMode::kSkip ? "skip" : "read");')
enter(extract, "Z7_COM7F_IMF(CExtractCallbackConsole::SetOperationResult(Int32 opRes, Int32 encrypted))", "PortProgress::FileDone();")
replace(extract, '  else\n  {\n    NumFileErrors_in_Current++;', '  else\n  {\n    PortProgress::Error(opRes, encrypted != 0, _currentName);\n    NumFileErrors_in_Current++;')
enter(extract, "Z7_COM7F_IMF(CExtractCallbackConsole::MessageError(const wchar_t *message))", 'PortProgress::Error(-1, false, nullptr, message);')
enter(extract, "HRESULT CExtractCallbackConsole::ExtractResult(HRESULT result)", "PortProgress::Finish();")
replace(extract, 'static const char * const kTab = "  ";', "Z7_COM7F_IMF(CExtractCallbackConsole::SetRatioInfo(const UInt64 *inSize, const UInt64 *outSize))\n{\n  PortProgress::Ratio(inSize, outSize);\n  return CheckBreak2();\n}\n\nstatic const char * const kTab = \"  \";")

common = "Common/Extract.cpp"
replace(common, '#include "NativeProgress.h"', '#include "NativeProgress.h"\n#include "NativeAgentSelection.h"')
replace(common, "  CRecordVector<UInt32> realIndices;", "  CRecordVector<UInt32> realIndices;\n  const bool portSelecting = PortSelection::Requested();\n  PortSelection::Resolved portSelection;\n  if (portSelecting)\n  {\n    const HRESULT selectionResult = PortSelection::Resolve(arcLink, codecs, portSelection);\n    if (selectionResult != S_OK)\n    {\n      errorMessage = L\"Archive selection changed or is invalid. Refresh and select the items again.\";\n      return selectionResult;\n    }\n  }")
replace(common, "    UInt32 numItems;", "    UInt64 portNumFiles = 0; bool portFilesKnown = true;\n    UInt32 numItems;")
replace(common, "    for (UInt32 i = 0; i < numItems; i++)", "    for (UInt32 i = 0; !portSelecting && i < numItems; i++)")
replace(common, "      realIndices.Add(i);", "      if (PortProgress::Enabled())\n      {\n        bool directory = false;\n        if (Archive_IsItem_Dir(archive, i, directory) != S_OK) portFilesKnown = false;\n        else if (!directory) ++portNumFiles;\n      }\n      realIndices.Add(i);")
replace(common, "    if (realIndices.Size() == 0)", "    if (portSelecting)\n    {\n      realIndices = portSelection.indices;\n      for (unsigned i = 0; i < realIndices.Size(); ++i)\n      {\n        bool directory = false;\n        if (Archive_IsItem_Dir(archive, realIndices[i], directory) != S_OK) portFilesKnown = false;\n        else if (!directory) ++portNumFiles;\n      }\n    }\n    if (PortProgress::Enabled() && portFilesKnown) PortProgress::Files(portNumFiles);\n    if (realIndices.Size() == 0)")
replace(common, "  ecs->Init(\n      options.NtOptions,", "  if (portSelecting)\n  {\n    removePathParts = portSelection.pathParts;\n    elimIsPossible = false;\n  }\n  ecs->Init(\n      options.NtOptions,")
replace(common, "      removePathParts, false,", "      removePathParts, portSelecting && portSelection.alternateFolder,")
replace(common, "  ecs->Is_elimPrefix_Mode = elimIsPossible;", "  if (portSelecting && portSelection.tree && !portSelection.preservePaths)\n    ecs->SetBaseParentFolderIndex(portSelection.baseParent);\n  ecs->Is_elimPrefix_Mode = elimIsPossible;")

# Report actual paths from the original callback, including tree/ADS removal.
extract_callback = "Common/ArchiveExtractCallback.cpp"
replace(extract_callback, '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "NativeExtraction.h"\n#include "NativeOverwrite.h"')
replace(extract_callback, '#include "NativeOverwrite.h"', '#include "NativeOverwrite.h"\n#include "NativeFileState.h"')
# ZIP's NTFS extra field contains zero placeholders when access/creation time
# is omitted. The original console displays them as blank, but its POSIX
# extraction writes 1601-01-01. APFS clamps that out-of-range time to its
# minimum; retain the host-created time instead. Keep raw listing properties.
replace(extract_callback, '  if (prop.vt == VT_FILETIME)\n    ft.Set_From_Prop(prop);',
        '  if (prop.vt == VT_FILETIME)\n  {\n'
        '    if (prop.filetime.dwLowDateTime == 0 && prop.filetime.dwHighDateTime == 0)\n'
        '      return S_OK;\n'
        '    ft.Set_From_Prop(prop);\n  }')
replace(extract_callback, '  NFind::CFileInfo fileInfo;\n\n  if (fileInfo.Find(fullProcessedPath))',
        '  NFind::CFileInfo fileInfo;\n  bool portFound = false;\n'
        '  RINOK(PortFileState::Find(fullProcessedPath, fileInfo, portFound))\n\n  if (portFound)')
replace(extract_callback, 'MyMoveFile(fullProcessedPath, existPath)', 'PortExtraction::Move(fullProcessedPath, existPath)')
replace(extract_callback, 'RemoveDir(fullProcessedPath)', 'PortExtraction::RemoveDirectory(fullProcessedPath)')
replace(extract_callback, 'NFind::DoesFileExist_Raw(fullProcessedPath)', 'PortExtraction::ExistsRaw(fullProcessedPath)')
replace(extract_callback, 'DeleteFileAlways(fullProcessedPath)', 'PortExtraction::DeleteFile(fullProcessedPath)')
replace(extract_callback, '  _outFileStream = outFileStream_Loc;',
        '  _outFileStream = outFileStream_Loc;\n'
        '  RINOK(PortExtraction::Begin(index, fullProcessedPath))')
replace('../Common/FilePathAutoRename.cpp', '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "NativeFileState.h"')
replace('../Common/FilePathAutoRename.cpp', '  return NFile::NFind::DoesFileOrDirExist(path);',
        '  return PortFileState::Enabled() ? PortFileState::Exists(path) : NFile::NFind::DoesFileOrDirExist(path);')
ask = ('      RINOK(_extractCallback2->AskOverwrite(\n'
       '          fs2us(realFullProcessedPath), &ft1, &fileInfo.Size, _item.Path,\n'
       '          _fi.MTime.Def ? &_fi.MTime.FT : NULL,\n'
       '          _curSize_Defined ? &_curSize : NULL,\n'
       '          &overwriteResult))')
replace(extract_callback, ask,
        '      RINOK(PortOverwrite::Enabled() ? PortOverwrite::Ask(\n'
        '          fs2us(realFullProcessedPath), &ft1, &fileInfo.Size, _item.Path,\n'
        '          _fi.MTime.Def ? &_fi.MTime.FT : NULL,\n'
        '          _curSize_Defined ? &_curSize : NULL,\n'
        '          fileInfo.IsDir(), _item.IsDir, &overwriteResult) :\n'
        + ask.removeprefix('      RINOK('))
replace(extract_callback, '  link_was_Created = false;\n  if (_ntOptions.SymLinks_DangerousLevel',
        '  link_was_Created = false;\n  FString portTarget = existFilePath;\n'
        '  RINOK(PortExtraction::PrepareHardLinkTarget(portTarget))\n'
        '  if (_ntOptions.SymLinks_DangerousLevel')
replace(extract_callback, 'fi.Find(existFilePath)', 'fi.Find(portTarget)')
replace(extract_callback, 'MyCreateHardLink(newFilePath, existFilePath)', 'MyCreateHardLink(newFilePath, portTarget)')
replace(extract_callback, '  linkWasSet = true;\n  return S_OK;\n}\n\n\n// if file/dir is symbolic link',
        '  linkWasSet = true;\n  RINOK(PortExtraction::DeferLink(_index, fullProcessedPath_from))\n'
        '  return S_OK;\n}\n\n\n// if file/dir is symbolic link')
replace(extract_callback, '  return _extractCallback2->SetOperationResult(opRes, BoolToInt(_encrypted));',
        '  if (_extractMode && !_stdOutMode && !_diskFilePath.IsEmpty())\n'
        '  {\n    FString hardTarget;\n#ifdef SUPPORT_LINKS\n'
        '    if (_link.Is_HardLink() && !_link.LinkPath.IsEmpty())\n'
        '      if (!NName::GetFullPath(_dirPathPrefix_Full, us2fs(_link.LinkPath), hardTarget)) return E_FAIL;\n'
        '#endif\n    RINOK(PortExtraction::Record(_index, _diskFilePath, hardTarget, _isSplit))\n  }\n'
        '  return _extractCallback2->SetOperationResult(opRes, BoolToInt(_encrypted));')

replace(extract_callback, '          SetAttrib(_diskFilePath);\n          SET_NEED_SET_OWNER\n          SET_OWNER',
        '          SetAttrib(_diskFilePath);\n          SET_NEED_SET_OWNER\n          SET_OWNER\n'
        '          RINOK(PortExtraction::Record(index, fullProcessedPath, FString()))')
replace(extract_callback, '      RINOK(SetSecurityInfo(link.Index_in_Arc, link.fullProcessedPath_from))\n#endif',
        '      RINOK(SetSecurityInfo(link.Index_in_Arc, link.fullProcessedPath_from))\n#endif\n'
        '      FString hardTarget;\n'
        '      if (link.LinkInfo.Is_HardLink())\n'
        '        if (!NName::GetFullPath(_dirPathPrefix_Full, us2fs(link.LinkInfo.LinkPath), hardTarget)) return E_FAIL;\n'
        '      RINOK(PortExtraction::Record(link.Index_in_Arc, link.fullProcessedPath_from, hardTarget, false, true))')
replace(extract_callback, '  HRESULT res = CloseReparseAndFile();\n#ifdef SUPPORT_LINKS',
        '  const bool portOpenFile = (_outFileStream != NULL);\n'
        '  HRESULT res = CloseReparseAndFile();\n'
        '  if (res == S_OK && portOpenFile && _extractMode && !_stdOutMode && !_diskFilePath.IsEmpty())\n'
        '    res = PortExtraction::Record(_index, _diskFilePath, FString(), _isSplit);\n#ifdef SUPPORT_LINKS')
replace(extract_callback, '  _arc = NULL;\n  return res;',
        '  const HRESULT portResult = PortExtraction::Finish();\n'
        '  if (res == S_OK) res = portResult;\n  _arc = NULL;\n  return res;')

# CopyFrom uses the same physical/logical separation as AgentOut.cpp.
# Its archive items are all censored, and k_ActionSet_Add preserves unrelated
# items while replacing selected same-name entries regardless of timestamps.
transfer = "Common/Update.cpp"
replace(transfer, '#include "StdAfx.h"', '#include "StdAfx.h"\n#include "ArchiveTransferPort.h"')
replace(transfer, "      const HRESULT res = EnumerateItems(censor,",
        "      const HRESULT res = PortArchiveTransfer::Enabled() ? PortArchiveTransfer::Enumerate(dirItems) : EnumerateItems(censor,")
replace(transfer, "          UString(), // options.AddPathPrefix,", "          PortArchiveTransfer::Prefix(), // logical path only; physical sources unchanged")
replace(transfer, "    if (allFilesAreAllowed)", "    if (allFilesAreAllowed || PortArchiveTransfer::Enabled())")

hashfile = "Console/HashCon.cpp"
enter(hashfile, "HRESULT CHashCallbackConsole::StartScanning()", 'PortProgress::Begin("hash"); PortProgress::Status("scan", true);')
enter(hashfile, "HRESULT CHashCallbackConsole::ScanProgress(const CDirItemsStat &st, const FString &path, bool isDir)", 'PortProgress::Scanning(st.NumFiles + st.NumAltStreams, st.GetTotalBytes(), fs2us(path), isDir);')
enter(hashfile, "HRESULT CHashCallbackConsole::FinishScanning(const CDirItemsStat &st)", "PortProgress::Files(st.NumFiles + st.NumAltStreams);")
replace(hashfile, "UInt64 /* numFiles */", "UInt64 numFiles")
enter(hashfile, "HRESULT CHashCallbackConsole::SetNumFiles(UInt64 numFiles)", 'PortProgress::Begin("hash"); PortProgress::Files(numFiles);')
enter(hashfile, "HRESULT CHashCallbackConsole::SetTotal(UInt64 size)", "PortProgress::Total(size);")
enter(hashfile, "HRESULT CHashCallbackConsole::SetCompleted(const UInt64 *completeValue)", "PortProgress::Completed(completeValue);")
enter(hashfile, "HRESULT CHashCallbackConsole::GetStream(const wchar_t *name, bool isDir)", "PortProgress::Current(name, isDir);")
enter(hashfile, "HRESULT CHashCallbackConsole::SetOperationResult(UInt64 fileSize, const CHashBundle &hb, bool showHash)", "PortProgress::FileDone();")
enter(hashfile, "HRESULT CHashCallbackConsole::AfterLastFile(CHashBundle &hb)", "PortProgress::Completion(nullptr, &hb); PortProgress::Finish();")

def overlay(relative=pathlib.Path()):
    destination = output / relative
    if destination.is_symlink():
        # An earlier version linked this entire unmodified directory. Expand
        # only our exact upstream link when a newly instrumented file is added.
        if destination.resolve() != source / relative:
            raise SystemExit("Refusing to write through an overlay symlink: " + str(destination))
        destination.unlink()
    destination.mkdir(parents=True, exist_ok=True)
    for child in (source / relative).iterdir():
        path = relative / child.name
        target = output / path
        if str(path) in files:
            content = files[str(path)].replace("\n", "\r\n").encode("utf-8")
            if target.is_symlink():
                target.unlink()
            if not target.exists() or target.read_bytes() != content:
                write_generated(target, content)
        elif any(value.startswith(str(path) + "/") for value in files):
            overlay(path)
        elif not target.exists() and not target.is_symlink():
            target.symlink_to(child, target_is_directory=child.is_dir())

if output == source or source in output.parents:
    raise SystemExit("The overlay must be outside official sources")
overlay()
print(f"Official {VERSION} overlay: {len(hashes)} files verified; ZipCrypto metadata descriptor fix; codecs/crypto unchanged")
