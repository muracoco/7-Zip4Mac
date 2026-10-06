// SPDX-License-Identifier: LGPL-3.0-or-later
#include "NativeFileState.h"
#include "NativeProgress.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/7zip/UI/Console/ConsoleClose.h"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
std::string quote(const FString &path) {
  AString bytes; ConvertUnicodeToUTF8(fs2us(path), bytes); std::string result = "\"";
  const char hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < bytes.Len(); ++i) {
    const auto c = static_cast<unsigned char>(bytes[i]);
    if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
    else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
    else result += char(c);
  }
  return result + '"';
}
bool unhex(const std::string &text, std::string &result) {
  if (text.size() % 2) return false;
  auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
  result.clear();
  for (size_t i = 0; i < text.size(); i += 2) {
    const auto a = digit(text[i]), b = digit(text[i + 1]); if (a < 0 || b < 0) return false;
    result += char(a * 16 + b);
  }
  return true;
}
bool parse(const std::string &packet, UInt64 expected, int &error, bool &found,
           NWindows::NFile::NFind::CFileInfo &info) {
  std::istringstream input(packet); std::string header, encoded, leaf, extra; UInt64 id; int exists;
  if (!(input >> header >> id >> error >> exists >> encoded >> leaf) || header != "7ZFS001" || id != expected ||
      error < 0 || error > 4095 || (exists != 0 && exists != 1) || (input >> extra)) return false;
  found = exists == 1;
  if (error || !found) return encoded == "-" && leaf == "-";
  std::string bytes, name;
  if (!unhex(encoded, bytes) || bytes.size() != sizeof(struct stat) || !unhex(leaf, name) ||
      name.empty() || name.find('/') != std::string::npos || name.find('\0') != std::string::npos) return false;
  struct stat stamp; std::memcpy(&stamp, bytes.data(), sizeof(stamp));
  info.SetFrom_stat(stamp); info.Name = name.c_str(); return true;
}
}
bool PortFileState::Enabled() {
  const char *setting = std::getenv("SEVENZIP_PORT_OUTPUT_STATE_RPC");
  return setting && std::strcmp(setting, "1") == 0 && PortProgress::Enabled();
}
HRESULT PortFileState::Find(const FString &path, NWindows::NFile::NFind::CFileInfo &info, bool &found, bool probe) {
  if (!Enabled()) { found = info.Find(path); return S_OK; }
  static UInt64 serial = 0; const UInt64 id = ++serial; const auto parent = ::getppid();
  const auto request = "{\"version\":1,\"mode\":\"fileState\",\"id\":\"" + std::to_string(id) + "\",\"path\":" + quote(path) + ",\"probe\":" + (probe ? "true" : "false") + "}";
  if (request.size() > 60000) return E_FAIL;
  bool sendRequest = true;
  for (;;) {
    if (NConsoleClose::TestBreakSignal() || ::getppid() != parent) return E_ABORT;
    if (sendRequest) {
      const auto count = ::send(3, request.data(), request.size(), MSG_DONTWAIT);
      if (count < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) return E_ABORT;
    }
    struct pollfd descriptor{3, POLLIN, 0}; const auto ready = ::poll(&descriptor, 1, 250);
    if (ready < 0) { if (errno == EINTR) continue; return E_ABORT; }
    if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) return E_ABORT;
    sendRequest = ready == 0; if (!(descriptor.revents & POLLIN)) continue;
    char packet[4096]; const auto count = ::recv(3, packet, sizeof(packet), MSG_DONTWAIT);
    if (count < 0) { if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; return E_ABORT; }
    if (!count) return E_ABORT;
    int error = 0;
    if (count == sizeof(packet) || !parse(std::string(packet, size_t(count)), id, error, found, info)) continue;
    if (error) { std::fprintf(stderr, "ERROR: Cannot inspect extraction destination: %s (%d)\n", std::strerror(error), error); return E_FAIL; }
    return S_OK;
  }
}
bool PortFileState::Exists(const FString &path) {
  NWindows::NFile::NFind::CFileInfo info; bool found = false;
  // The original bool-only auto-name routine must treat failed metadata queries
  // as occupied, never authorize a potentially existing output after an error.
  return Find(path, info, found, true) != S_OK || found;
}
