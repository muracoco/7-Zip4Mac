// SPDX-License-Identifier: LGPL-3.0-or-later
#include "NativeTempFiles.h"
#include "CPP/Common/MyVector.h"
#include "CPP/Common/IntToString.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/Windows/FileFind.h"
#include "CPP/Windows/PropVariantConv.h"
#include "CPP/7zip/UI/Common/PropIDUtils.h"
#include "CPP/7zip/UI/FileManager/PropertyNameRes.h"
#include "CPP/7zip/UI/FileManager/OverwriteDialogRes.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>

namespace {
using NWindows::NFile::NFind::CFileInfo;
int readFailure = 0;
std::string failedPath;
// Keep the Windows enumerator's interface around the official POSIX API.
class CEnumerator {
  NWindows::NFile::NFind::CEnumerator native;
  std::string path;
public:
  void SetDirPrefix(const FString &prefix) { path.assign(prefix.Ptr(), prefix.Len()); native.SetDirPrefix(prefix); }
  bool Next(CFileInfo &file, bool &found) {
    NWindows::NFile::NFind::CDirEntry entry;
    const bool ok = native.Next(entry, found) && (!found || native.Fill_FileInfo(entry, file, false));
    if (!ok && !readFailure) { readFailure = errno ? errno : EIO; failedPath = path; }
    return ok;
  }
  bool Next(CFileInfo &file) { bool found = false; return Next(file, found) && found; }
};
#include "upstream/TempBrowseEnumerator.inc"

struct TempPropertyPrinter {
  UString DirPrefix;
  int error = 0;
  void PrintFileProps(UString &s, const CFileInfo &file);
  void Reload_WithErrorMessage() {} // Qt reports the error and refreshes.
};
void MessageBox_LastError_path(TempPropertyPrinter &printer, const FString &) { printer.error = errno ? errno : EIO; }
UString tempLanguage(UInt32 id) {
  const char *name = nullptr, *fallback = "";
  switch (id) {
    case IDS_PROP_SIZE: name = "SIZE"; fallback = "Size"; break;
    case IDS_PROP_MTIME: name = "MTIME"; fallback = "Modified"; break;
    case IDS_PROP_ATTRIBUTES: name = "ATTRIBUTES"; fallback = "Attributes"; break;
    case IDS_PROP_FOLDERS: name = "FOLDERS"; fallback = "Folders"; break;
    case IDS_PROP_FILES: name = "FILES"; fallback = "Files"; break;
    case IDS_FILE_SIZE: name = "FILE_SIZE"; fallback = "{0} bytes"; break;
  }
  const char *value = name ? std::getenv((std::string("SEVENZIP_PORT_TEMP_LABEL_") + name).c_str()) : nullptr;
  UString result; ConvertUTF8ToUnicode(AString(value ? value : fallback), result); return result;
}
void AddLangString(UString &text, UInt32 id) { text += tempLanguage(id); }
UString MyFormatNew(UInt32 id, const wchar_t *value) { auto result = tempLanguage(id); result.Replace(L"{0}", value); return result; }
void PortTempTime(const CFiTime &time, char *text) { FILETIME value; FiTime_To_FILETIME(time, value); ConvertUtcFileTimeToString(value, text); }
#include "upstream/TempBrowseProperties.inc"

std::string quote(const std::string &s) {
  std::string result = "\"";
  for (unsigned char c : s) {
    if (c == '\\' || c == '"') { result += '\\'; result += char(c); }
    else if (c < 32) { char b[7]; std::snprintf(b, sizeof(b), "\\u%04x", c); result += b; }
    else result += char(c);
  }
  return result + '"';
}
bool tempName(const std::string &name) {
  // BrowseDialog2::Reload: 7z[E/O/S] followed by exactly eight hex digits.
  if (name.size() == 11 && name[0] == '7' && (name[1] == 'z' || name[1] == 'Z') &&
      std::strchr("eEoOsS", name[2])) {
    bool valid = true;
    for (size_t i = 3; i < name.size(); ++i)
      if (!std::strchr("0123456789abcdefABCDEF", name[i])) valid = false;
    if (valid) return true;
  }
  // Qt's private temporary names substitute the Windows temporary names.
  const char *prefixes[] = {".7zip-open-", ".7zip-nested-", ".7zip-replace-", ".7zip-folder-",
    ".7zip-add-", ".7zip-metadata-", ".7zip-selection-", ".7zip-extraction-records-",
    ".7zip-extract-", ".7zip-hash-", ".7zip-split-", ".7zip-combine-", ".7zip-install-", "7zip-verify-"};
  for (const char *prefix : prefixes) {
    const size_t length = std::strlen(prefix);
    if (name.compare(0, length, prefix) || name.size() != length + 6) continue;
    bool valid = true;
    for (size_t i = length; i < name.size(); ++i)
      if (!std::strchr("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", name[i])) valid = false;
    if (valid) return true;
  }
  for (const char *prefix : {".7zip-file-install-", ".7zip-comment-", ".7zip-link-edit-"}) {
    const size_t length = std::strlen(prefix);
    if (name.compare(0, length, prefix) || name.size() != length + 36) continue;
    bool valid = true;
    for (size_t i = 0; i < 36; ++i) {
      if (i == 8 || i == 13 || i == 18 || i == 23) { if (name[length + i] != '-') valid = false; }
      else if (!std::strchr("0123456789abcdefABCDEF", name[length + i])) valid = false;
    }
    if (valid) return true;
  }
  return false;
}
int fail(const std::string &path) {
  std::fprintf(stderr, "Cannot read temporary folder: %s\n%s (%d)\n", path.c_str(), std::strerror(errno), errno);
  return 2;
}
}
bool PortTempFiles::Requested() { const char *path = std::getenv("SEVENZIP_PORT_TEMP_FOLDER"); return path && *path; }
int PortTempFiles::Run() {
  const char *input = std::getenv("SEVENZIP_PORT_TEMP_FOLDER");
  const char *root = std::getenv("SEVENZIP_PORT_TEMP_ROOT");
  if (!input || !root || !*root) { errno = EINVAL; return fail(input ? input : ""); }
  char *resolved = ::realpath(input, nullptr);
  if (!resolved) return fail(input);
  const std::string path(resolved), boundary(root); std::free(resolved);
  if (path != input || (path != boundary && path.compare(0, boundary.size() + 1, boundary + '/'))) { errno = EPERM; return fail(input); }
  struct stat folder{};
  if (::lstat(input, &folder)) return fail(input);
  if (!S_ISDIR(folder.st_mode)) { errno = ENOTDIR; return fail(input); }
  const char *propertyName = std::getenv("SEVENZIP_PORT_TEMP_PROPERTIES");
  if (propertyName && *propertyName) {
    const std::string name(propertyName), full = path + '/' + name;
    if (name == "." || name == ".." || name.find('/') != std::string::npos || (path == boundary && !tempName(name))) { errno = EINVAL; return fail(full); }
    CFileInfo file;
    if (!file.Find(full.c_str())) return fail(full);
    if (file.IsOsSymLink()) { errno = EPERM; return fail(full); }
    TempPropertyPrinter printer; ConvertUTF8ToUnicode(AString((path + '/').c_str()), printer.DirPrefix); UString text; printer.PrintFileProps(text, file);
    if (printer.error) { errno = printer.error; return fail(full); }
    struct stat after{}, item{};
    if (::lstat(input, &after) || folder.st_dev != after.st_dev || folder.st_ino != after.st_ino || ::lstat(full.c_str(), &item) || file.dev != item.st_dev || file.ino != item.st_ino || S_ISLNK(item.st_mode)) { errno = ESTALE; return fail(full); }
    AString encoded; ConvertUnicodeToUTF8(text, encoded);
    const auto error = readFailure ? std::string("Cannot read temporary folder: ") + failedPath + '\n' + std::strerror(readFailure) : std::string();
    const auto output = "{\"text\":" + quote(std::string(encoded.Ptr(), encoded.Len())) + ",\"error\":" + quote(error) + "}\n";
    return std::fwrite(output.data(), 1, output.size(), stdout) == output.size() ? 0 : 2;
  }
  CEnumerator enumerator; enumerator.SetDirPrefix(FString((path + '/').c_str()));
  std::string output = "[";
  for (;;) {
    CFileInfo file; bool found = false;
    if (!enumerator.Next(file, found)) return fail(path);
    if (!found) break;
    const std::string name(file.Name.Ptr(), file.Name.Len());
    if (path == boundary && !tempName(name)) continue;
    const std::string full = path + '/' + name;
    CBrowseEnumerator summary; summary.bi.Size = file.IsDir() ? 0 : file.Size;
    if (file.IsDir() && !file.IsOsSymLink()) { summary.Path = full.c_str(); summary.Enumerate(0); }
    std::string rowError;
    if (readFailure) { rowError = "Cannot read temporary folder: " + failedPath + '\n' + std::strerror(readFailure) + " (" + std::to_string(readFailure) + ')'; summary.bi.WasInterrupted = true; readFailure = 0; }
    wchar_t formatted[64]; Browse_ConvertSizeToString(summary.bi.Size, formatted);
    AString sizeText; ConvertUnicodeToUTF8(UString(formatted), sizeText);
    struct stat stamp{}; if (::lstat(full.c_str(), &stamp)) { if (errno == ENOENT) continue; return fail(full); }
    if (output.size() > 1) output += ',';
    output += "{\"name\":" + quote(name) + ",\"error\":" + quote(rowError) + ",\"sizeText\":" + quote(std::string(sizeText.Ptr(), sizeText.Len())) + ",\"size\":" + quote(std::to_string(summary.bi.Size)) +
      ",\"files\":" + std::to_string(summary.bi.NumFiles) + ",\"folders\":" + std::to_string(summary.bi.NumDirs) +
      ",\"interrupted\":" + (summary.bi.WasInterrupted ? "true" : "false") +
      ",\"directory\":" + (file.IsDir() ? "true" : "false") + ",\"link\":" + (file.IsOsSymLink() ? "true" : "false") +
      ",\"modified\":" + quote(std::to_string(stamp.st_mtimespec.tv_sec)) +
      ",\"subName\":" + quote(summary.bi.NumRootItems == 1 ? std::string(summary.fi_SubFile.Name.Ptr(), summary.fi_SubFile.Name.Len()) : "") + '}';
  }
  struct stat after{};
  if (::lstat(input, &after) || folder.st_dev != after.st_dev || folder.st_ino != after.st_ino) { errno = ESTALE; return fail(input); }
  output += "]\n";
  return std::fwrite(output.data(), 1, output.size(), stdout) == output.size() ? 0 : 2;
}
