// SPDX-License-Identifier: LGPL-3.0-or-later
// Report original callback output names; no streams/passwords are recorded.
#include "NativeExtraction.h"
#include "NativeFileState.h"
#include "NativeProgress.h"
#include "CPP/Windows/FileDir.h"
#include "CPP/7zip/UI/Console/ConsoleClose.h"
#include "ArchiveSourceStamp.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/Windows/FileName.h"
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <unistd.h>
#include <cerrno>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#include <copyfile.h>
#include <sys/clonefile.h>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>

namespace {
std::string utf8(const FString &text) { AString bytes; ConvertUnicodeToUTF8(fs2us(text), bytes); return std::string(bytes.Ptr(), bytes.Len()); }
FString native(const std::string &text) { UString value; ConvertUTF8ToUnicode(AString(text.c_str()), value); return us2fs(value); }
struct Descriptor {
  int value = -1;
  explicit Descriptor(int value = -1) : value(value) {}
  ~Descriptor() { if (value >= 0) ::close(value); }
  void reset(int next) { if (value >= 0) ::close(value); value = next; }
};
// These are private staging/canonical output paths supplied by the host.
// Walk every parent without following links, including the public reference.
bool parent(const std::string &path, Descriptor &fd, std::string &leaf, bool create) {
  if (path.empty() || path[0] != '/') return false;
  const auto slash = path.rfind('/'); leaf = path.substr(slash + 1);
  if (leaf.empty() || leaf == "." || leaf == "..") return false;
  fd.reset(::open("/", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  for (size_t begin = 1; begin < slash;) {
    const auto end = path.find('/', begin); const auto part = path.substr(begin, end - begin);
    if (part.empty() || part == "." || part == ".." || fd.value < 0) return false;
    int next = ::openat(fd.value, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next < 0 && errno == ENOENT && create && ::mkdirat(fd.value, part.c_str(), 0700) == 0)
      next = ::openat(fd.value, part.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (next < 0) return false; fd.reset(next); begin = end + 1;
  }
  return fd.value >= 0;
}
bool unhex(const std::string &text, std::string &result) {
  result.clear(); if (text.size() % 2) return false;
  auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  for (size_t i = 0; i < text.size(); i += 2) {
    const int a = digit(text[i]), b = digit(text[i + 1]); if (a < 0 || b < 0 || (!a && !b)) return false;
    result += char(a * 16 + b);
  }
  return true;
}
struct Targets {
  bool enabled = false, valid = false, absolute = false;
  std::string stage, root;
  std::map<std::string, std::string> rules;
  Targets() {
    const char *name = std::getenv("SEVENZIP_PORT_HARDLINK_MAP"); if (!name || !*name) return;
    enabled = true; Descriptor fd(::open(name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC)); if (fd.value < 0) return;
    std::string bytes; char buffer[8192]; ssize_t count;
    while ((count = ::read(fd.value, buffer, sizeof(buffer))) > 0) bytes.append(buffer, size_t(count));
    if (count < 0) return;
    std::istringstream input(bytes); std::string line;
    if (!std::getline(input, line) || line != "7ZHARD01" || !std::getline(input, line) || !unhex(line, stage) ||
        !std::getline(input, line) || !unhex(line, root) || !std::getline(input, line) || (line != "0" && line != "1")) return;
    absolute = line == "1";
    while (std::getline(input, line)) {
      const auto split = line.find(' '); std::string rel, target;
      if (split == std::string::npos || !unhex(line.substr(0, split), rel) || !unhex(line.substr(split + 1), target) || !rules.emplace(rel, target).second) return;
    }
    valid = !stage.empty() && stage[0] == '/' && !root.empty() && root[0] == '/';
  }
  bool resolve(const std::string &path, std::string &staged, std::string &target) const {
    if (!valid) return false;
    std::string rel;
    if (path.compare(0, stage.size() + 1, stage + '/') == 0) rel = path.substr(stage.size() + 1);
    else if (absolute) { for (const auto &rule : rules) if (rule.second == path) { rel = rule.first; break; } }
    if (rel.empty() || rel[0] == '/' || rel.find("//") != std::string::npos) return false;
    std::istringstream parts(rel); std::string part;
    while (std::getline(parts, part, '/')) if (part.empty() || part == "." || part == "..") return false;
    staged = stage + '/' + rel;
    const auto explicitTarget = rules.find(rel);
    if (explicitTarget != rules.end()) { target = explicitTarget->second; return true; }
    if (!absolute) { target = root + (root.back() == '/' ? "" : "/") + rel; return true; }
    const auto rule = rules.find(rel); if (rule == rules.end()) return false; target = rule->second; return true;
  }
};
Targets &targets() { static Targets value; return value; }
std::string quote(const FString &text) {
  AString bytes; ConvertUnicodeToUTF8(fs2us(text), bytes); std::string result = "\"";
  const char hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < bytes.Len(); ++i) {
    const auto c = static_cast<unsigned char>(bytes[i]);
    if (c == '\\' || c == '"') { result += '\\'; result += char(c); }
    else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
    else result += char(c);
  }
  return result + '"';
}
HRESULT ioFailure(const char *action, const FString &path) {
  const int code = errno;
  std::fprintf(stderr, "ERROR: %s: %s: %s (%d)\n", action, quote(path).c_str(), std::strerror(code), code);
  return E_FAIL;
}
int output = -2;
HRESULT write(const std::string &bytes) {
  size_t position = 0;
  while (position < bytes.size()) {
    const auto count = ::write(output, bytes.data() + position, bytes.size() - position);
    if (count < 0 && errno == EINTR) continue;
    if (count <= 0) return E_FAIL;
    position += size_t(count);
  }
  return S_OK;
}
HRESULT prepareOutput() {
  if (output != -2) return output < 0 ? E_FAIL : S_OK;
  const char *destination = std::getenv("SEVENZIP_PORT_EXTRACTION_PATH");
  if (!destination || !*destination) return E_FAIL;
  output = ::open(destination, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
  return output < 0 ? E_FAIL : write("7ZOUT002\n");
}
HRESULT request(const char *action, const std::string &item, bool cleanup = false) {
  if (!PortExtraction::Interactive()) return S_OK;
  static UInt64 serial = 0; const auto id = ++serial; const auto parentPid = ::getppid();
  const auto packet = "{\"version\":1,\"mode\":\"extraction\",\"id\":\"" + std::to_string(id) +
    "\",\"action\":\"" + action + "\",\"item\":" + item + "}";
  if (packet.size() > 60000) return E_FAIL;
  bool retry = true;
  for (;;) {
    if ((!cleanup && NConsoleClose::TestBreakSignal()) || ::getppid() != parentPid) return E_ABORT;
    if (retry) {
      const auto count = ::send(3, packet.data(), packet.size(), MSG_DONTWAIT);
      if (count < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) return E_ABORT;
    }
    struct pollfd socket{3, POLLIN, 0}; const auto ready = ::poll(&socket, 1, 250);
    if (ready < 0) { if (errno == EINTR) continue; return E_ABORT; }
    if (socket.revents & (POLLERR | POLLHUP | POLLNVAL)) return E_ABORT;
    retry = ready == 0; if (!(socket.revents & POLLIN)) continue;
    char buffer[256]; const auto count = ::recv(3, buffer, sizeof(buffer), MSG_DONTWAIT);
    if (count < 0) { if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; return E_ABORT; }
    if (!count) return E_ABORT;
    if (count == sizeof(buffer)) continue;
    std::istringstream input(std::string(buffer, size_t(count))); std::string header, extra; UInt64 response; int error;
    if (!(input >> header >> response >> error) || header != "7ZER001" || response != id || error < 0 || error > 4095 || (input >> extra)) continue;
    if (!error) return S_OK;
    SetLastError(DWORD(error));
    return error == ECANCELED ? E_ABORT : E_FAIL;
  }
}
bool mutate(const char *operation, const FString &path, const FString &destination = FString()) {
  const auto result = request("mutation", "{\"operation\":\"" + std::string(operation) + "\",\"path\":" + quote(path) + ",\"destination\":" + quote(destination) + "}");
  if (result == E_ABORT) SetLastError(ECANCELED);
  return result == S_OK;
}
unsigned long long serial = 0;
std::map<std::string, UInt32> splitOutputs;
std::set<std::pair<UInt32, std::string>> deferredLinks;
// Freeze each completed stream before the original callback can delete or
// replace its logical name. APFS clones retain bytes/metadata without changing
// the callback's hard-link graph; other filesystems use a private copy.
HRESULT snapshot(const std::string &path, const struct stat &before, std::string &saved) {
  Descriptor sourceParent, outputParent; std::string sourceLeaf, outputLeaf;
  if (!parent(path, sourceParent, sourceLeaf, false)) return E_FAIL;
  const char *records = std::getenv("SEVENZIP_PORT_EXTRACTION_PATH");
  if (!records || !parent(records, outputParent, outputLeaf, false)) return E_FAIL;
  if (::mkdirat(outputParent.value, "payloads", 0700) != 0 && errno != EEXIST) return E_FAIL;
  Descriptor payloads(::openat(outputParent.value, "payloads", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
  if (payloads.value < 0) return E_FAIL;
  const auto name = std::string("item-") + std::to_string(++serial);
  const std::string recordPath(records);
  saved = recordPath.substr(0, recordPath.rfind('/')) + "/payloads/" + name;
  if (S_ISREG(before.st_mode) && ::clonefileat(sourceParent.value, sourceLeaf.c_str(), payloads.value, name.c_str(), CLONE_NOFOLLOW) == 0) {
    struct stat after{};
    if (::fstatat(sourceParent.value, sourceLeaf.c_str(), &after, AT_SYMLINK_NOFOLLOW) == 0 && archiveSourceStamp(before) == archiveSourceStamp(after)) return S_OK;
    errno = ESTALE; return E_FAIL;
  }
  const bool symbolic = S_ISLNK(before.st_mode);
  Descriptor source(::openat(sourceParent.value, sourceLeaf.c_str(), O_RDONLY | O_CLOEXEC | (symbolic ? O_SYMLINK : O_NOFOLLOW)));
  if (source.value < 0) return E_FAIL;
  struct stat opened{};
  if (::fstat(source.value, &opened) != 0) return E_FAIL;
  if (archiveSourceStamp(before) != archiveSourceStamp(opened)) { errno = ESTALE; return E_FAIL; }
  Descriptor copy;
  if (symbolic) {
    char target[65536]; const auto size = ::readlinkat(sourceParent.value, sourceLeaf.c_str(), target, sizeof(target) - 1);
    if (size < 0 || size == sizeof(target) - 1) return E_FAIL;
    target[size] = 0;
    if (::symlinkat(target, payloads.value, name.c_str()) != 0) return E_FAIL;
    copy.reset(::openat(payloads.value, name.c_str(), O_RDONLY | O_SYMLINK | O_CLOEXEC));
  } else copy.reset(::openat(payloads.value, name.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (copy.value < 0 || ::fcopyfile(source.value, copy.value, nullptr, symbolic ? COPYFILE_METADATA : COPYFILE_ALL) != 0) return E_FAIL;
  struct stat after{};
  if (::fstat(source.value, &after) != 0) return E_FAIL;
  if (archiveSourceStamp(before) != archiveSourceStamp(after)) { errno = ESTALE; return E_FAIL; }
  if (!symbolic) {
    const timespec times[2]{before.st_atimespec, before.st_mtimespec};
    if (::futimens(source.value, times) != 0) return E_FAIL;
  }
  return S_OK;
}
}
HRESULT PortExtraction::PrepareHardLinkTarget(FString &path) {
  const auto &context = targets(); if (!context.enabled) return S_OK;
  std::string staged, publicPath; if (!context.resolve(utf8(path), staged, publicPath)) return E_FAIL;
  path = native(staged);
  Descriptor stageParent; std::string stageLeaf;
  if (!parent(staged, stageParent, stageLeaf, true)) return E_FAIL;
  struct stat existing{};
  if (::fstatat(stageParent.value, stageLeaf.c_str(), &existing, AT_SYMLINK_NOFOLLOW) == 0) return S_OK;
  if (errno != ENOENT) return E_FAIL;
  Descriptor sourceParent; std::string sourceLeaf;
  if (!parent(publicPath, sourceParent, sourceLeaf, false)) return S_OK; // Original callback reports missing target.
  Descriptor source(::openat(sourceParent.value, sourceLeaf.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
  struct stat before{};
  if (source.value < 0 || ::fstat(source.value, &before) != 0 || !S_ISREG(before.st_mode)) return S_OK;
  Descriptor seed(::openat(stageParent.value, stageLeaf.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600));
  if (seed.value < 0 || ::fcopyfile(source.value, seed.value, nullptr, COPYFILE_METADATA) != 0) return E_FAIL;
  struct stat after{}, prepared{};
  if (::fstat(source.value, &after) != 0 || archiveSourceStamp(before) != archiveSourceStamp(after) || ::fstat(seed.value, &prepared) != 0) return E_FAIL;
  RINOK(prepareOutput())
  // The placeholder has no payload and cannot mutate the public inode. Only
  // the original callback links it; the host later links the guarded source.
  const auto row = "{\"kind\":\"seed\",\"path\":" + quote(native(staged)) + ",\"source\":" + quote(native(publicPath)) +
    ",\"sourceSnapshot\":" + quote(native(archiveSourceStamp(after))) + ",\"device\":\"" + std::to_string(prepared.st_dev) +
    "\",\"inode\":\"" + std::to_string(prepared.st_ino) + "\"}";
  RINOK(write(row + '\n'))
  return request("seed", row);
}
bool PortExtraction::Interactive() {
  const char *setting = std::getenv("SEVENZIP_PORT_EXTRACTION_RPC");
  const char *records = std::getenv("SEVENZIP_PORT_EXTRACTION_PATH");
  return setting && std::strcmp(setting, "1") == 0 && records && *records && PortFileState::Enabled();
}
bool PortExtraction::Move(const FString &from, const FString &to) {
  if (!Interactive()) return NWindows::NFile::NDir::MyMoveFile(from, to);
  if (!mutate("rename", from, to)) return false;
  Descriptor source, target; std::string sourceLeaf, targetLeaf;
  if (!parent(utf8(from), source, sourceLeaf, false) || !parent(utf8(to), target, targetLeaf, false)) return false;
  struct stat found{};
  if (::fstatat(source.value, sourceLeaf.c_str(), &found, AT_SYMLINK_NOFOLLOW) != 0) return errno == ENOENT;
  return ::renameatx_np(source.value, sourceLeaf.c_str(), target.value, targetLeaf.c_str(), RENAME_EXCL) == 0;
}
bool PortExtraction::RemoveDirectory(const FString &path) {
  if (!Interactive()) return NWindows::NFile::NDir::RemoveDir(path);
  if (!mutate("rmdir", path)) return false;
  Descriptor folder; std::string leaf;
  if (!parent(utf8(path), folder, leaf, false)) return false;
  return ::unlinkat(folder.value, leaf.c_str(), AT_REMOVEDIR) == 0 || errno == ENOENT;
}
bool PortExtraction::DeleteFile(const FString &path) {
  if (!Interactive()) return NWindows::NFile::NDir::DeleteFileAlways(path);
  if (!mutate("delete", path)) return false;
  Descriptor folder; std::string leaf;
  if (!parent(utf8(path), folder, leaf, false)) return false;
  return ::unlinkat(folder.value, leaf.c_str(), 0) == 0 || errno == ENOENT;
}
bool PortExtraction::ExistsRaw(const FString &path) {
  return Interactive() ? PortFileState::Exists(path) : NWindows::NFile::NFind::DoesFileExist_Raw(path);
}
HRESULT PortExtraction::Begin(UInt32 index, const FString &path) {
  if (!Interactive()) return S_OK;
  Descriptor folder; std::string leaf;
  if (!parent(utf8(path), folder, leaf, false)) return E_FAIL;
  struct stat opened{};
  if (::fstatat(folder.value, leaf.c_str(), &opened, AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(opened.st_mode)) return E_FAIL;
  const auto row = "{\"index\":" + std::to_string(index) + ",\"path\":" + quote(path) +
    ",\"device\":\"" + std::to_string(opened.st_dev) + "\",\"inode\":\"" + std::to_string(opened.st_ino) + "\"}";
  return request("begin", row);
}
HRESULT PortExtraction::DeferLink(UInt32 index, const FString &path) {
  const char *destination = std::getenv("SEVENZIP_PORT_EXTRACTION_PATH");
  if (destination && *destination) deferredLinks.emplace(index, utf8(path));
  return S_OK;
}
HRESULT PortExtraction::Record(UInt32 index, const FString &path, const FString &hardLink, bool split, bool linkComplete) {
  const char *destination = std::getenv("SEVENZIP_PORT_EXTRACTION_PATH");
  if (!destination || !*destination) return S_OK;
  // SetLink creates an empty private placeholder. Only the original post-link
  // callback can authorize recording the resulting link, including on failure.
  const auto linkKey = std::make_pair(index, utf8(path));
  if (deferredLinks.count(linkKey) && !linkComplete) return S_OK;
  if (linkComplete) deferredLinks.erase(linkKey);
  if (split) { splitOutputs[utf8(path)] = index; return S_OK; }
  RINOK(prepareOutput())
  Descriptor folder; std::string leaf; const auto actual = utf8(path);
  if (!parent(actual, folder, leaf, false)) return E_FAIL;
  struct stat before{};
  if (::fstatat(folder.value, leaf.c_str(), &before, AT_SYMLINK_NOFOLLOW) != 0) return errno == ENOENT ? S_OK : E_FAIL; // Deferred links are recorded after creation.
  if (!S_ISDIR(before.st_mode) && !S_ISREG(before.st_mode) && !S_ISLNK(before.st_mode)) return E_FAIL;
  std::string saved;
  if (!S_ISDIR(before.st_mode) && snapshot(actual, before, saved) != S_OK) return ioFailure("Cannot retain completed extraction output", path);
  FString recordedLink = hardLink, publicLink;
  if (!hardLink.IsEmpty() && targets().enabled) {
    std::string staged, target; if (!targets().resolve(utf8(hardLink), staged, target)) return E_FAIL;
    recordedLink = native(staged); publicLink = native(target);
  }
  const auto row = "{\"index\":" + std::to_string(index) + ",\"path\":" + quote(path) + ",\"hardLink\":" + quote(recordedLink) +
    ",\"publicHardLink\":" + quote(publicLink) + ",\"snapshot\":" + quote(native(saved)) +
    ",\"group\":\"" + std::to_string(before.st_dev) + ':' + std::to_string(before.st_ino) + "\"}";
  RINOK(write(row + '\n'))
  // Completed streams must become visible to the next original GetStream.
  // CloseArc cleanup after Cancel still needs an acknowledgement from the host.
  return request("publish", row, true);
}
HRESULT PortExtraction::Finish() {
  const auto pending = splitOutputs; splitOutputs.clear();
  for (const auto &item : pending) { RINOK(Record(item.second, native(item.first), FString())) }
  return request("finish", "{}", true);
}
