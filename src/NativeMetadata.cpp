// SPDX-License-Identifier: LGPL-3.0-or-later
// Read properties while the official ListArchives CArchiveLink is alive.
// Archive parsing, typed/raw formatting and both folder proxies are upstream.
#include "NativeMetadata.h"
#include "NativeAgentProperties.h"
#include "NativeAgentSelection.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/Windows/PropVariant.h"
#include "CPP/Windows/FileIO.h"
#include "CPP/7zip/UI/Common/PropIDUtils.h"
#include "CPP/7zip/UI/Common/ExtractingFilePath.h"
#include "CPP/7zip/UI/Agent/AgentProxy.h"
#include "CPP/7zip/UI/Console/ConsoleClose.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

using namespace NWindows;
using CProxyItem = PortProxyItem;
#include "upstream/AgentProperties.inc"
static const unsigned kParentIndex = unsigned(-1);
#include "upstream/CopyName.inc"

namespace {
using NWindows::NCOM::CPropVariant;
std::string utf8(const UString &s) { AString a; ConvertUnicodeToUTF8(s, a); return std::string(a.Ptr(), a.Len()); }
std::string quote(const std::string &s) {
  std::string result = "\"";
  for (unsigned char c : s) {
    if (c == '\\' || c == '"') { result += '\\'; result += char(c); }
    else if (c < 32) { char b[7]; std::snprintf(b, sizeof(b), "\\u%04x", c); result += b; }
    else result += char(c);
  }
  return result + '"';
}
std::string quote(const UString &s) { return quote(utf8(s)); }
struct Property { PROPID id; unsigned type; UString name; };
std::string schema(const std::vector<Property> &properties) {
  std::string result = "[";
  for (const auto &p : properties) {
    if (result.size() > 1) result += ',';
    result += "{\"id\":" + std::to_string(p.id) + ",\"type\":" + std::to_string(p.type) + ",\"name\":" + quote(p.name) + '}';
  }
  return result + ']';
}
HRESULT getSchema(IInArchive *archive, bool archiveProps, std::vector<Property> &properties) {
  UInt32 count = 0;
  RINOK(archiveProps ? archive->GetNumberOfArchiveProperties(&count) : archive->GetNumberOfProperties(&count))
  for (UInt32 i = 0; i < count; ++i) {
    CMyComBSTR name; PROPID id; VARTYPE type;
    RINOK(archiveProps ? archive->GetArchivePropertyInfo(i, &name, &id, &type) : archive->GetPropertyInfo(i, &name, &id, &type))
    properties.push_back({id, type, PortMetadata::PropertyName(id, name)});
  }
  return S_OK;
}
std::string value(const CPropVariant &p, PROPID id, HRESULT status = S_OK) {
  UString display;
  if ((id == kpidErrorFlags || id == kpidWarningFlags) && p.vt != VT_EMPTY)
    display = PortMetadata::ErrorFlags(GetOpenArcErrorFlags(p));
  else if (p.vt != VT_BSTR || p.bstrVal) ConvertPropertyToString2(display, p, id, 9);
  std::string result = "{\"id\":" + std::to_string(id) + ",\"type\":" + std::to_string(p.vt) + ",\"status\":" + std::to_string(Int32(status)) + ",\"value\":" + quote(display);
  // Decimal strings retain all 64 bits; FILETIME's precision lives in reserved fields.
  if (p.vt == VT_UI8) result += ",\"number\":" + quote(std::to_string(p.uhVal.QuadPart));
  else if (p.vt == VT_UI4) result += ",\"number\":" + quote(std::to_string(p.ulVal));
  else if (p.vt == VT_UI2) result += ",\"number\":" + quote(std::to_string(p.uiVal));
  else if (p.vt == VT_I8) result += ",\"number\":" + quote(std::to_string(p.hVal.QuadPart));
  else if (p.vt == VT_UI1) result += ",\"number\":" + quote(std::to_string(p.bVal));
  else if (p.vt == VT_I2) result += ",\"number\":" + quote(std::to_string(p.iVal));
  else if (p.vt == VT_I4) result += ",\"number\":" + quote(std::to_string(p.lVal));
  else if (p.vt == VT_BOOL) result += ",\"number\":" + quote(p.boolVal == VARIANT_FALSE ? "0" : "1");
  else if (p.vt == VT_FILETIME) {
    const UInt64 ticks = (UInt64(p.filetime.dwHighDateTime) << 32) | p.filetime.dwLowDateTime;
    result += ",\"ticks\":" + quote(std::to_string(ticks)) + ",\"precision\":[" + std::to_string(p.wReserved1) + ',' + std::to_string(p.wReserved2) + ',' + std::to_string(p.wReserved3) + ']';
  }
  return result + '}';
}
std::string rawDisplay(PROPID id, const Byte *data, UInt32 size, bool list) {
  if (!size) return {};
  if (id == kpidNtSecure) { AString s; ConvertNtSecureToString(data, size, s); return std::string(s.Ptr(), s.Len()); }
  if (list && id == kpidNtReparse) { UString s; if (ConvertNtReparseToString(data, size, s)) return utf8(s); }
  if (size > (list ? 64U : 256U)) return "data:" + std::to_string(size);
  const char *hex = size <= 8 && (id == kpidCRC || id == kpidChecksum) ? "0123456789ABCDEF" : "0123456789abcdef";
  std::string s; s.reserve(size * 2);
  for (UInt32 i = 0; i < size; ++i) { s += hex[data[i] >> 4]; s += hex[data[i] & 15]; }
  return s;
}
std::string rawSortData(PROPID id, const Byte *data, UInt32 size) {
  if (!size) return {};
  if (id == kpidNtReparse) {
    NFile::CReparseShortInfo info; info.Parse(data, size);
    if (info.Offset > size || info.Size > size - info.Offset) return {};
    data += info.Offset; size = info.Size;
  }
  const char hex[] = "0123456789abcdef"; std::string result; result.reserve(size_t(size) * 2);
  for (UInt32 i = 0; i < size; ++i) { result += hex[data[i] >> 4]; result += hex[data[i] & 15]; }
  return result;
}
template<class T> std::string folder(const UString &path, int index, const T &dir) {
  return "{\"path\":" + quote(path) + ",\"index\":" + std::to_string(index) +
    ",\"size\":" + quote(std::to_string(dir.Size)) + ",\"packed\":" + quote(std::to_string(dir.PackSize)) +
    ",\"folders\":" + quote(std::to_string(dir.NumSubDirs)) + ",\"files\":" + quote(std::to_string(dir.NumSubFiles)) +
    ",\"crcDefined\":" + (dir.CrcIsDefined ? "true" : "false") + ",\"crc\":" + quote(std::to_string(dir.Crc)) + '}';
}
std::string agentValues(PortAgentFolder &folder, unsigned index, bool folderValues, bool flat = false) {
  const PROPID items[] = {kpidName, kpidIsDir, kpidSize, kpidPackSize, kpidCRC, kpidNumSubDirs, kpidNumSubFiles};
  // PanelMenu::Properties requests Path and Agent's five kFolderProps. Root
  // GetName is intentionally not queried (normal proxy root has no Name).
  const PROPID dirs[] = {kpidPath, kpidSize, kpidPackSize, kpidNumSubDirs, kpidNumSubFiles, kpidCRC};
  const PROPID flatProps[] = {kpidSize, kpidPackSize};
  const PROPID *ids = folderValues ? dirs : flat ? flatProps : items;
  const unsigned count = folderValues ? Z7_ARRAY_SIZE(dirs) : flat ? Z7_ARRAY_SIZE(flatProps) : Z7_ARRAY_SIZE(items);
  std::string result = "[";
  for (unsigned i = 0; i < count; ++i) {
    CPropVariant prop;
    const HRESULT status = folderValues ? folder.GetFolderProperty(ids[i], &prop) : folder.GetProperty(index, ids[i], &prop);
    if (status != S_OK) prop.Clear();
    auto field = value(prop, ids[i], status); field.pop_back();
    if (i) result += ',';
    result += field + ",\"name\":" + quote(PortMetadata::PropertyName(ids[i], nullptr)) + '}';
  }
  return result + ']';
}
std::string agentRow(PortAgentFolder &normal, PortAgentFolder &flat, unsigned local,
                     int archiveIndex, int child, int alt) {
  flat.SetItem(local);
  return "{\"index\":" + std::to_string(archiveIndex) + ",\"dir\":" + std::to_string(child) +
    ",\"alt\":" + std::to_string(alt) + ",\"name\":" + quote(normal.GetName(local)) +
    ",\"path\":" + quote(normal.GetFullPrefix(local) + normal.GetName(local)) +
    ",\"copyName\":" + quote(normal.GetItemName_for_Copy(local)) +
    ",\"properties\":" + agentValues(normal, local, false) +
    ",\"flatProperties\":" + agentValues(flat, 0, false, true) + '}';
}
}
std::string PortMetadata::RawProperty(PROPID id, const Byte *data, UInt32 size, bool list) { return rawDisplay(id, data, size, list); }
HRESULT PortMetadata::WriteFormats(const CCodecs *codecs) {
  const char *path = std::getenv("SEVENZIP_PORT_FORMATS_PATH");
  if (!path || !*path) return S_OK;
  const int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return E_FAIL;
  FILE *out = ::fdopen(fd, "wb"); if (!out) { ::close(fd); return E_FAIL; }
  std::string text = "[";
  for (unsigned i = 0; i < codecs->Formats.Size(); ++i) {
    const auto &f = codecs->Formats[i];
    if (i) text += ',';
    auto boolean = [](bool value) { return value ? "true" : "false"; };
    text += "{\"name\":" + quote(f.Name) + ",\"precisionMask\":" + std::to_string(f.Get_TimePrecFlags()) +
      ",\"defaultPrecision\":" + std::to_string(f.Get_DefaultTimePrec()) +
      ",\"modificationTime\":" + boolean(f.Flags_MTime()) + ",\"creationTime\":" + boolean(f.Flags_CTime()) +
      ",\"accessTime\":" + boolean(f.Flags_ATime()) + ",\"defaultModificationTime\":" + boolean(f.Flags_MTime_Default()) +
      ",\"defaultCreationTime\":" + boolean(f.Flags_CTime_Default()) + ",\"defaultAccessTime\":" + boolean(f.Flags_ATime_Default()) +
      ",\"keepName\":" + boolean(f.Flags_KeepName()) + ",\"symbolicLinks\":" + boolean(f.Flags_SymLinks()) +
      ",\"hardLinks\":" + boolean(f.Flags_HardLinks()) + '}';
  }
  text += "]\n";
  const bool written = std::fwrite(text.data(), 1, text.size(), out) == text.size();
  const int closed = std::fclose(out);
  return written && closed == 0 ? S_OK : E_FAIL;
}
HRESULT PortMetadata::Write(const CArchiveLink &link, const CCodecs *codecs) {
  const char *path = std::getenv("SEVENZIP_PORT_METADATA_PATH");
  if (!path || !*path) return S_OK;
  const int fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (fd < 0) return E_FAIL;
  FILE *out = ::fdopen(fd, "wb"); if (!out) { ::close(fd); return E_FAIL; }
  struct Close { FILE *file; ~Close() { std::fclose(file); } } close{out};
  auto emit = [out](const std::string &text) { return std::fwrite(text.data(), 1, text.size(), out) == text.size(); };
  const CArc &arc = link.Arcs.Back(); IInArchive *archive = arc.Archive;
  std::vector<Property> properties, rawProperties;
  RINOK(getSchema(archive, false, properties))
  if (arc.GetRawProps) {
    UInt32 count = 0; RINOK(arc.GetRawProps->GetNumRawProps(&count))
    for (UInt32 i = 0; i < count; ++i) { CMyComBSTR name; PROPID id; RINOK(arc.GetRawProps->GetRawPropInfo(i, &name, &id)) rawProperties.push_back({id, 0, PropertyName(id, name)}); }
  }
  PortSelection::Resolved selected;
  const bool selecting = PortSelection::Requested();
  if (selecting) {
    const auto status = PortSelection::Resolve(link, codecs, selected);
    if (status != S_OK) { std::fputs("Archive selection changed or is invalid. Refresh the archive and select the items again.\n", stderr); return status; }
  }
  std::string selection;
  if (selecting) {
    selection = ",\"selectedIndices\":[";
    for (unsigned i = 0; i < selected.indices.Size(); ++i) selection += (i ? "," : "") + std::to_string(selected.indices[i]);
    selection += "] ,\"selectedPath\":" + quote(selected.selectedPath);
  }
  if (!emit("{\"version\":1,\"sourceSnapshot\":" + quote(PortSelection::Snapshot(link)) + selection +
      ",\"schema\":" + schema(properties) + ",\"rawSchema\":" + schema(rawProperties) + ",\"entries\":[")) return E_FAIL;
  UInt32 count; RINOK(archive->GetNumberOfItems(&count))
  for (UInt32 i = 0; i < count; ++i) {
    if (NConsoleClose::TestBreakSignal()) return E_ABORT;
    UString itemPath; RINOK(arc.GetItem_Path2(i, itemPath))
    bool directory = false; RINOK(Archive_IsItem_Dir(archive, i, directory))
    bool auxiliary = false; if (arc.Ask_Aux) { RINOK(Archive_IsItem_Aux(archive, i, auxiliary)) }
    UInt32 parent = UInt32(-1), parentType = 0;
    if (arc.GetRawProps) { RINOK(arc.GetRawProps->GetParent(i, &parent, &parentType)) }
    // WimHandler::GetParent has no parent only for generated XML/virtual roots;
    // real files always have an image or [Files] parent. Do not guess by name.
    const bool generated = !directory && arc.GetRawProps && parent == UInt32(-1) && UString(codecs->GetFormatNamePtr(arc.FormatIndex)).IsEqualTo("wim");
    std::string item = (i ? "," : "") + std::string("{\"index\":") + std::to_string(i) + ",\"path\":" + quote(itemPath) + ",\"directory\":" + (directory ? "true" : "false") + ",\"auxiliary\":" + (auxiliary ? "true" : "false") + ",\"generated\":" + (generated ? "true" : "false") + ",\"parent\":" + (parent == UInt32(-1) ? "-1" : std::to_string(parent)) + ",\"parentType\":" + std::to_string(parentType) + ",\"properties\":[";
    for (unsigned p = 0; p < properties.size(); ++p) { CPropVariant prop; const HRESULT status = archive->GetProperty(i, properties[p].id, &prop); if (status != S_OK) prop.Clear(); if (p) item += ','; item += value(prop, properties[p].id, status); }
    item += "],\"raw\":[";
    for (unsigned p = 0; p < rawProperties.size(); ++p) {
      const void *data = nullptr; UInt32 size = 0, type = 0; RINOK(arc.GetRawProps->GetRawProp(i, rawProperties[p].id, &data, &size, &type))
      if (size && !data) return E_FAIL;
      if (p) item += ',';
      item += "{\"id\":" + std::to_string(rawProperties[p].id) + ",\"type\":" + std::to_string(type) + ",\"size\":" + std::to_string(size) + ",\"sort\":" + quote(rawSortData(rawProperties[p].id, static_cast<const Byte *>(data), size)) + ",\"value\":" + quote(RawProperty(rawProperties[p].id, static_cast<const Byte *>(data), size, false)) + ",\"list\":" + quote(RawProperty(rawProperties[p].id, static_cast<const Byte *>(data), size, true)) + '}';
    }
    if (!emit(item + "]}")) return E_FAIL;
  }
  if (!emit("],\"folders\":[")) return E_FAIL;
  PortAgentContext context{archive, UString(codecs->GetFormatNamePtr(arc.FormatIndex))};
  if (arc.GetRawProps && arc.IsTree) {
    CProxyArc2 proxy; RINOK(proxy.Load(arc, nullptr))
    for (unsigned i = 0; i < proxy.Dirs.Size(); ++i) {
      if (NConsoleClose::TestBreakSignal()) return E_ABORT;
      const auto &dir = proxy.Dirs[i];
      const auto owner = dir.ArcIndex;
      const auto parentItem = owner < 0 ? -1 : proxy.Files[owner].Parent;
      const auto parentDir = i == 0 ? -1 : parentItem < 0 ? 0 : proxy.Files[parentItem].DirIndex;
      const auto alt = owner < 0 ? (i == 0 ? 1 : -1) : proxy.Files[owner].AltDirIndex;
      PortAgentFolder normal(context, nullptr, &proxy, i), flat(context, nullptr, &proxy, i, true);
      auto text = folder(proxy.GetDirPath_as_Prefix(i), owner, dir); text.pop_back();
      text += ",\"id\":" + std::to_string(i) + ",\"parentDir\":" + std::to_string(parentDir) +
        ",\"alt\":" + std::to_string(alt) + ",\"alternateStreams\":" + (proxy.IsAltDir(i) ? "true" : "false") +
        ",\"properties\":" + agentValues(normal, 0, true) + ",\"children\":[";
      for (unsigned j = 0; j < dir.Items.Size(); ++j) {
        const auto real = dir.Items[j]; const auto &file = proxy.Files[real];
        if (j) text += ',';
        text += agentRow(normal, flat, j, real, file.DirIndex, file.AltDirIndex);
      }
      if (!emit((i ? "," : "") + text + "]}")) return E_FAIL;
    }
  } else {
    CProxyArc proxy; RINOK(proxy.Load(arc, nullptr))
    for (unsigned i = 0; i < proxy.Dirs.Size(); ++i) {
      if (NConsoleClose::TestBreakSignal()) return E_ABORT;
      const auto &dir = proxy.Dirs[i];
      PortAgentFolder normal(context, &proxy, nullptr, i), flat(context, &proxy, nullptr, i, true);
      auto text = folder(proxy.GetDirPath_as_Prefix(i), i ? dir.ArcIndex : -1, dir); text.pop_back();
      text += ",\"id\":" + std::to_string(i) + ",\"parentDir\":" + std::to_string(dir.ParentDir) +
        ",\"alt\":-1,\"alternateStreams\":false,\"properties\":" + agentValues(normal, 0, true) + ",\"children\":[";
      const unsigned count = dir.SubDirs.Size() + dir.SubFiles.Size();
      for (unsigned j = 0; j < count; ++j) {
        if (j) text += ',';
        const int child = j < dir.SubDirs.Size() ? int(dir.SubDirs[j]) : -1;
        text += agentRow(normal, flat, j, proxy.GetRealIndex(i, j), child, -1);
      }
      if (!emit((i ? "," : "") + text + "]}")) return E_FAIL;
    }
  }
  if (!emit("],\"layers\":[")) return E_FAIL;
  const PROPID special[] = {kpidPath, kpidType, kpidErrorType, kpidError, kpidErrorFlags, kpidWarning, kpidWarningFlags, kpidOffset, kpidPhySize, kpidTailSize};
  for (unsigned layer = 0; layer < link.Arcs.Size(); ++layer) {
    const CArc &current = link.Arcs[layer]; std::vector<Property> archiveSchema, childSchema;
    RINOK(getSchema(current.Archive, true, archiveSchema))
    std::string text = layer ? ",{\"properties\":[" : "{\"properties\":[";
    bool first = true;
    auto add = [&](PROPID id, const UString &name, const CPropVariant &prop) { if (!first) text += ','; first = false; auto field = value(prop, id); field.pop_back(); text += field + ",\"name\":" + quote(name) + '}'; };
    for (PROPID id : special) {
      CPropVariant p;
      switch (id) {
        case kpidPath: p = current.Path; break;
        case kpidType: p = UString(codecs->GetFormatNamePtr(current.FormatIndex)); break;
        case kpidErrorType: if (current.ErrorInfo.ErrorFormatIndex >= 0) p = UString(codecs->GetFormatNamePtr(current.ErrorInfo.ErrorFormatIndex)); break;
        case kpidErrorFlags: if (current.ErrorInfo.GetErrorFlags()) p = current.ErrorInfo.GetErrorFlags(); break;
        case kpidWarningFlags: if (current.ErrorInfo.GetWarningFlags()) p = current.ErrorInfo.GetWarningFlags(); break;
        case kpidOffset: if (current.GetGlobalOffset()) p.Set_Int64(current.GetGlobalOffset()); break;
        case kpidTailSize: if (current.ErrorInfo.TailSize) p = current.ErrorInfo.TailSize; break;
        default: RINOK(current.Archive->GetArchiveProperty(id, &p))
      }
      add(id, PropertyName(id, nullptr), p);
    }
    for (const auto &s : archiveSchema) { CPropVariant p; RINOK(current.Archive->GetArchiveProperty(s.id, &p)) add(s.id, s.name, p); }
    text += "],\"childProperties\":["; first = true;
    if (layer + 1 < link.Arcs.Size()) {
      RINOK(getSchema(current.Archive, false, childSchema))
      for (const auto &s : childSchema) { CPropVariant p; RINOK(current.Archive->GetProperty(link.Arcs[layer + 1].SubfileIndex, s.id, &p)) add(s.id, s.name, p); }
    }
    if (!emit(text + "]}")) return E_FAIL;
  }
  if (!emit("],\"nonOpen\":[")) return E_FAIL;
  bool first = true;
  for (PROPID id : special) {
    CPropVariant p;
    switch (id) {
      case kpidPath: if (!link.NonOpen_ArcPath.IsEmpty()) p = link.NonOpen_ArcPath; break;
      case kpidErrorType: if (link.NonOpen_ErrorInfo.ErrorFormatIndex >= 0) p = UString(codecs->GetFormatNamePtr(link.NonOpen_ErrorInfo.ErrorFormatIndex)); break;
      case kpidErrorFlags: if (link.NonOpen_ErrorInfo.GetErrorFlags()) p = link.NonOpen_ErrorInfo.GetErrorFlags(); break;
      case kpidWarningFlags: if (link.NonOpen_ErrorInfo.GetWarningFlags()) p = link.NonOpen_ErrorInfo.GetWarningFlags(); break;
    }
    if (p.vt == VT_EMPTY) continue;
    auto field = value(p, id); field.pop_back();
    if (!emit((first ? "" : ",") + field + ",\"name\":" + quote(PropertyName(id, nullptr)) + '}')) return E_FAIL;
    first = false;
  }
  return emit("]}\n") && std::fflush(out) == 0 ? S_OK : E_FAIL;
}
