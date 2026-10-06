// SPDX-License-Identifier: LGPL-3.0-or-later
#include "NativeAgentSelection.h"
#include "NativeAgentProperties.h"
#include "ArchiveSourceStamp.h"
#include "C/Sort.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/7zip/UI/Console/ConsoleClose.h"
#include "CPP/7zip/Archive/Common/ItemNameUtils.h"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <set>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

using CProxyItem = PortProxyItem;
#include "upstream/AgentSelection.inc"

namespace {
std::string utf8(const UString &s) { AString a; ConvertUnicodeToUTF8(s, a); return {a.Ptr(), a.Len()}; }
class Reader {
  std::vector<unsigned char> data;
  size_t position = 0;
public:
  bool open(const char *path) {
    const int fd = ::open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return false;
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat st{};
    if (::fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != ::geteuid() ||
        (st.st_mode & 077) || st.st_size < 8 || st.st_size > 64 * 1024 * 1024) return false;
    data.resize(size_t(st.st_size));
    while (position < data.size()) {
      const auto n = ::read(fd, data.data() + position, data.size() - position);
      if (n <= 0) return false;
      position += size_t(n);
    }
    if (std::memcmp(data.data(), "7ZSEL001", 8)) return false;
    position = 8; return true;
  }
  bool number(UInt32 &v) {
    if (data.size() - position < 4) return false;
    v = 0; for (unsigned i = 0; i < 4; ++i) v |= UInt32(data[position++]) << (8 * i);
    return true;
  }
  bool text(std::string &s) {
    UInt32 n; if (!number(n) || n > data.size() - position) return false;
    s.assign(reinterpret_cast<const char *>(data.data() + position), n); position += n;
    return s.find('\0') == std::string::npos;
  }
  bool finished() const { return position == data.size(); }
};
bool stamp(const UString &path, std::string &result) {
  struct stat st{}; const auto name = utf8(path);
  if (::stat(name.c_str(), &st) || !S_ISREG(st.st_mode)) return false;
  result += archiveSourceStamp(st);
  return true;
}
}
bool PortSelection::Requested() { const auto path = std::getenv("SEVENZIP_PORT_SELECTION_PATH"); return path && *path; }
std::string PortSelection::Snapshot(const CArchiveLink &link) {
  std::string result;
  if (link.Arcs.IsEmpty() || !stamp(link.Arcs[0].Path, result)) return {};
  for (unsigned i = 0; i < link.VolumePaths.Size(); ++i) if (!stamp(link.VolumePaths[i], result)) return {};
  return result;
}
HRESULT PortSelection::Resolve(const CArchiveLink &link, const CCodecs *codecs, Resolved &result) {
  const auto path = std::getenv("SEVENZIP_PORT_SELECTION_PATH");
  Reader reader; std::string snapshot, type;
  UInt32 expectedCount, base, flags, rowCount;
  if (!path || !reader.open(path) || !reader.text(snapshot) || snapshot.empty() ||
      snapshot != Snapshot(link) || !reader.text(type) || !reader.number(expectedCount) ||
      !reader.number(base) || !reader.number(flags) || flags > 31 || !reader.number(rowCount) ||
      rowCount > 10000000) return E_INVALIDARG;
  const CArc &arc = link.Arcs.Back(); UInt32 count;
  RINOK(arc.Archive->GetNumberOfItems(&count))
  if (count != expectedCount || utf8(UString(codecs->GetFormatNamePtr(arc.FormatIndex))) != type) return E_INVALIDARG;
  result.tree = arc.GetRawProps && arc.IsTree; result.preservePaths = (flags & 2) != 0;
  const bool flat = (flags & 1) != 0;
  auto &proxy = result.proxy; auto &proxy2 = result.proxy2;
  if (result.tree) { RINOK(proxy2.Load(arc, nullptr)) if (base >= proxy2.Dirs.Size()) return E_INVALIDARG; }
  else { RINOK(proxy.Load(arc, nullptr)) if (base >= proxy.Dirs.Size()) return E_INVALIDARG; }
  auto &context = result.context;
  context.archive = arc.Archive; context.ArchiveType = codecs->GetFormatNamePtr(arc.FormatIndex);
  result.folder.reset(new PortAgentFolder(context, result.tree ? nullptr : &proxy, result.tree ? &proxy2 : nullptr, base, flat));
  auto &folder = *result.folder;
  CRecordVector<PortProxyItem> rows; CUIntVector local;
  std::set<UInt64> identities;
  for (UInt32 i = 0; i < rowCount; ++i) {
    if (NConsoleClose::TestBreakSignal()) return E_ABORT;
    UInt32 dir, item, real; std::string name;
    if (!reader.number(dir) || !reader.number(item) || !reader.number(real) || !reader.text(name) ||
        (!flat && dir != base) || !identities.insert((UInt64(dir) << 32) | item).second) return E_INVALIDARG;
    if (result.tree) {
      if (dir >= proxy2.Dirs.Size() || item >= proxy2.Dirs[dir].Items.Size()) return E_INVALIDARG;
    } else if (dir >= proxy.Dirs.Size() || item >= proxy.Dirs[dir].SubDirs.Size() + proxy.Dirs[dir].SubFiles.Size()) return E_INVALIDARG;
    PortAgentFolder normal(context, result.tree ? nullptr : &proxy, result.tree ? &proxy2 : nullptr, dir);
    if (UInt32(normal.GetRealIndex(item)) != real || utf8(normal.GetName(item)) != name) return E_INVALIDARG;
    rows.Add({dir, item}); local.Add(flat ? i : item);
  }
  if (!reader.finished()) return E_INVALIDARG;
  folder.SetItems(rows);
  result.localIndices = local;
  if (local.Size() == 1) {
    UString name = folder.GetName(local[0]);
    NArchive::NItemName::NormalizeSlashes_in_FileName_for_OsPath(name);
    result.selectedPath = folder.GetFullPrefix(local[0]) + name;
  }
  if (flags & 8) {
    // CommentItem uses exactly GetRealIndex, including a real folder record.
    if (local.Size() != 1) return E_INVALIDARG;
    const int index = folder.GetRealIndex(local[0]);
    if (index < 0) return E_NOTIMPL;
    result.indices.Add(UInt32(index));
  } else {
    // Agent Extract/Test/Delete use false; Rename expands even in Flat mode.
    folder.GetRealIndices(local.ConstData(), local.Size(), (flags & 16) == 0, (flags & 4) != 0, result.indices);
  }
  Normalize(result.indices);
  if (result.tree) {
    proxy2.GetDirPathParts(base, result.pathParts, result.alternateFolder);
    result.baseParent = UInt32(proxy2.Dirs[base].ArcIndex);
  } else {
    bool changed = false; proxy.GetDirPathParts_isChanged(base, result.pathParts, changed);
    if (changed) return E_NOTIMPL;
  }
  if (result.preservePaths) { result.pathParts.Clear(); result.alternateFolder = false; result.baseParent = UInt32(-1); }
  return S_OK;
}
void PortSelection::Normalize(CUIntVector &indices) {
  // Boundary normalization removes only repeat references to the SAME item.
  // Distinct handler indices with identical names remain distinct.
  CUIntVector unique;
  for (unsigned i = 0; i < indices.Size(); ++i)
    if (i == 0 || indices[i] != indices[i - 1]) unique.Add(indices[i]);
  indices = unique;
}
