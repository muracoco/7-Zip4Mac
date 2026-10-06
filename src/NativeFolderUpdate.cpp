// SPDX-License-Identifier: LGPL-3.0-or-later
// Official Agent update operations retain packed streams with NoChange pairs.
#include "NativeFolderUpdate.h"
#include "NativeAgentSelection.h"
#include "NativeProgress.h"
#include "NativeZipMetadata.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/Windows/FileName.h"
#include "CPP/Windows/PropVariant.h"
#include "CPP/7zip/Common/FileStreams.h"
#include "CPP/7zip/Common/LimitedStreams.h"
#include "CPP/7zip/Compress/CopyCoder.h"
#include "CPP/7zip/UI/Common/UpdateCallback.h"
#include "CPP/7zip/Archive/Common/ItemNameUtils.h"
#include "CPP/7zip/UI/Console/UpdateCallbackConsole.h"
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
extern CStdOutStream *g_ErrStream;

namespace {
using namespace NWindows;
struct FolderUpdater {
  const CArc &arc;
  CMyComPtr<IOutArchive> writer;
  const CArchiveLink &link;
  PortAgentFolder *_agentFolder = nullptr;
  FolderUpdater(const CArc &opened, const CArchiveLink &archiveLink, PortAgentFolder *folder)
      : arc(opened), link(archiveLink), _agentFolder(folder) {}
  bool CanUpdate() const { return writer != nullptr; }
  IInArchive *GetArchive() const { return arc.Archive; }
  const CArc &GetArc() const { return arc; }
  HRESULT CommonUpdate(ISequentialOutStream *stream, UInt32 count, IArchiveUpdateCallback *callback) {
    return writer->UpdateItems(stream, count, callback);
  }
  HRESULT CreateFolder(ISequentialOutStream *, const wchar_t *, IUpdateCallbackUI *);
  HRESULT DeleteItems(ISequentialOutStream *, const UInt32 *, UInt32, IUpdateCallbackUI *);
  HRESULT RenameItem(ISequentialOutStream *, const UInt32 *, UInt32, const wchar_t *, IUpdateCallbackUI *);
  HRESULT CommentItem(ISequentialOutStream *, const UInt32 *, UInt32, const wchar_t *, IUpdateCallbackUI *);
  HRESULT UpdateOneFile(ISequentialOutStream *, const UInt32 *, UInt32, const wchar_t *, IUpdateCallbackUI *);
};
void SetInArchiveInterfaces(FolderUpdater *agent, CArchiveUpdateCallback *update) {
  update->Arc = &agent->arc; update->Archive = agent->arc.Archive;
  update->ArcFileName = ExtractFileNameFromPath(agent->arc.Path);
}
#include "upstream/AgentCreateFolder.inc"
#include "upstream/AgentItemUpdate.inc"
#include "upstream/AgentComment.inc"
#include "upstream/AgentUpdateOneFile.inc"
HRESULT readComment(UString &comment) {
  const char *path = std::getenv("SEVENZIP_PORT_COMMENT_PATH");
  if (!path || !*path) return E_INVALIDARG;
  const int fd = ::open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0) return HRESULT_FROM_WIN32(errno);
  struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
  struct stat info{};
  if (::fstat(fd, &info) || !S_ISREG(info.st_mode) || info.st_uid != ::geteuid() ||
      (info.st_mode & 077) || info.st_size < 0 || info.st_size > 4 * 65535) return E_INVALIDARG;
  std::string bytes(size_t(info.st_size), '\0');
  size_t read = 0;
  while (read < bytes.size()) {
    const auto count = ::read(fd, &bytes[read], bytes.size() - read);
    if (count <= 0) return E_FAIL;
    read += size_t(count);
  }
  if (bytes.find('\0') != std::string::npos || !ConvertUTF8ToUnicode(AString(bytes.c_str()), comment)) return E_INVALIDARG;
  return S_OK;
}
bool safeName(const UString &name) {
  if (name.IsEmpty() || name[0] == L'/') return false;
  unsigned start = 0;
  for (unsigned i = 0; i <= name.Len(); ++i) {
    if (i < name.Len() && (name[i] < 32 || name[i] == L'\\' || name[i] == L':')) return false;
    if (i == name.Len() || name[i] == L'/') {
      const unsigned size = i - start;
      if (!size || (size == 1 && name[start] == L'.') || (size == 2 && name[start] == L'.' && name[start + 1] == L'.')) return false;
      start = i + 1;
    }
  }
  return true;
}
}
bool PortFolderUpdate::Requested() {
  const char *name = std::getenv("SEVENZIP_PORT_CREATE_FOLDER");
  const char *operation = std::getenv("SEVENZIP_PORT_ITEM_UPDATE");
  return (name && *name) || (operation && *operation);
}
HRESULT PortFolderUpdate::Run(const CArchiveLink &link, const CCodecs *codecs, const COpenCallbackConsole &openCallback) {
  const char *name = std::getenv("SEVENZIP_PORT_CREATE_FOLDER");
  const char *operation = std::getenv("SEVENZIP_PORT_ITEM_UPDATE");
  const bool deleting = operation && std::strcmp(operation, "delete") == 0;
  const bool renaming = operation && std::strcmp(operation, "rename") == 0;
  const bool commenting = operation && std::strcmp(operation, "comment") == 0;
  const bool replacing = operation && std::strcmp(operation, "replace") == 0;
  if (operation && !deleting && !renaming && !commenting && !replacing) return E_INVALIDARG;
  if (renaming) name = std::getenv("SEVENZIP_PORT_RENAME_ITEM");
  const char *destination = std::getenv("SEVENZIP_PORT_FOLDER_OUTPUT");
  UString logical;
  if (!destination || !*destination || (!deleting && !commenting && !replacing && (!name || !ConvertUTF8ToUnicode(AString(name), logical) || !safeName(logical)))) return E_INVALIDARG;
  if (link.Arcs.Size() != 1 || !link.VolumePaths.IsEmpty()) return E_NOTIMPL;
  const CArc &arc = link.Arcs.Back();
  // Agent::CanUpdate does not reject warnings. The handler's own read-only
  // property/output interface decides whether a warning-bearing archive can
  // be updated; actual open errors and the transaction boundaries still fail.
  if (arc.GetGlobalOffset() < 0 || arc.Offset < 0 || arc.ErrorInfo.AreThereErrors() || arc.ErrorInfo.ThereIsTail) return E_NOTIMPL;
  NCOM::CPropVariant readOnly;
  RINOK(arc.Archive->GetArchiveProperty(kpidReadOnly, &readOnly))
  if (readOnly.vt == VT_BOOL && readOnly.boolVal != VARIANT_FALSE) return E_NOTIMPL;
  PortSelection::Resolved selected;
  if (deleting || renaming || commenting || replacing) {
    if (!PortSelection::Requested()) return E_INVALIDARG;
    RINOK(PortSelection::Resolve(link, codecs, selected))
    if (selected.localIndices.IsEmpty() || ((renaming || commenting) && selected.localIndices.Size() != 1)) return E_INVALIDARG;
  }
  UString diskPath;
  if (replacing) {
    const auto path = std::getenv("SEVENZIP_PORT_REPLACEMENT_PATH"); struct stat input{};
    if (!path || !*path || !ConvertUTF8ToUnicode(AString(path), diskPath) ||
        ::lstat(path, &input) || !S_ISREG(input.st_mode) || input.st_uid != ::geteuid() || selected.indices.Size() != 1) return E_INVALIDARG;
    logical = selected.selectedPath;
  }
  UString comment;
  if (commenting) {
    if (std::wcscmp(codecs->GetFormatNamePtr(arc.FormatIndex), L"zip") != 0) return E_NOTIMPL;
    RINOK(readComment(comment))
    logical = selected.selectedPath;
  }
  FolderUpdater updater(arc, link, selected.folder.get());
  RINOK(arc.Archive->QueryInterface(IID_IOutArchive, reinterpret_cast<void **>(&updater.writer)))
  if ((commenting || renaming) && std::wcscmp(codecs->GetFormatNamePtr(arc.FormatIndex), L"zip") == 0)
    RINOK(retainZipMetadataTimes(arc.Archive))
  CMyComPtr2_Create<IOutStream, COutFileStream> output;
  if (!output->Create_NEW(destination)) return HRESULT_FROM_WIN32(GetLastError());
  // Agent preserves only the prefix outside the handler's stream. The
  // unmodified 7z/ZIP handler preserves its own embedded stub independently.
  #include "upstream/AgentUpdateStream.inc"
  CUpdateCallbackConsole callback;
  callback.Init(&g_StdOut, g_ErrStream, nullptr, true);
  callback.PasswordIsDefined = openCallback.PasswordIsDefined;
  callback.Password = openCallback.Password;
  // Password2 remains undefined when none was needed to open the archive.
  // A data-password prompt is issued only if the handler actually requires it.
  // The GUI retains the extraction password for edit/nested write-back.
  // Ask only when such a password is supplied; its value remains on stdin.
  const auto updatePassword = std::getenv("SEVENZIP_PORT_UPDATE_PASSWORD");
  callback.AskPassword = replacing && updatePassword && std::strcmp(updatePassword, "1") == 0;
  PortProgress::Begin("compress", logical);
  const HRESULT result = deleting ? updater.DeleteItems(tailStream, selected.localIndices.ConstData(), selected.localIndices.Size(), &callback) :
      renaming ? updater.RenameItem(tailStream, selected.localIndices.ConstData(), selected.localIndices.Size(), logical, &callback) :
      commenting ? updater.CommentItem(tailStream, selected.localIndices.ConstData(), selected.localIndices.Size(), comment, &callback) :
      replacing ? updater.UpdateOneFile(tailStream, selected.localIndices.ConstData(), selected.localIndices.Size(), diskPath, &callback) :
      updater.CreateFolder(tailStream, logical, &callback);
  const HRESULT closed = output->Close();
  PortProgress::Finish();
  return result == S_OK ? closed : result;
}
