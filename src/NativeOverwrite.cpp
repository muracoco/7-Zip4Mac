// SPDX-License-Identifier: LGPL-3.0-or-later
#include "NativeOverwrite.h"
#include "NativeProgress.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/7zip/UI/Common/IFileExtractCallback.h"
#include "CPP/7zip/UI/Console/ConsoleClose.h"
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
std::string quote(const UString &value) {
  AString utf8; ConvertUnicodeToUTF8(value, utf8); std::string result = "\"";
  const char hex[] = "0123456789abcdef";
  for (unsigned i = 0; i < utf8.Len(); ++i) {
    const auto c = static_cast<unsigned char>(utf8[i]);
    if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
    else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
    else result += char(c);
  }
  return result + '"';
}
std::string size(const UInt64 *value) { return value ? '"' + std::to_string(*value) + '"' : "null"; }
std::string time(const FILETIME *value) {
  if (!value) return "null";
  const UInt64 ticks = (UInt64(value->dwHighDateTime) << 32) | value->dwLowDateTime;
  return '"' + std::to_string(ticks) + '"';
}
bool parse(const std::string &packet, UInt64 expected, Int32 &answer) {
  std::istringstream input(packet); std::string header, response, extra; UInt64 id;
  if (!(input >> header >> id >> response) || header != "7ZOW001" || id != expected || (input >> extra)) return false;
  if (response == "yes") answer = NOverwriteAnswer::kYes;
  else if (response == "no") answer = NOverwriteAnswer::kNo;
  else if (response == "yesAll") answer = NOverwriteAnswer::kYesToAll;
  else if (response == "noAll") answer = NOverwriteAnswer::kNoToAll;
  else if (response == "rename") answer = NOverwriteAnswer::kAutoRename;
  else if (response == "cancel") answer = NOverwriteAnswer::kCancel;
  else return false;
  return true;
}
}
bool PortOverwrite::Enabled() {
  const char *setting = std::getenv("SEVENZIP_PORT_OVERWRITE_RPC");
  return setting && std::strcmp(setting, "1") == 0 && PortProgress::Enabled();
}
HRESULT PortOverwrite::Ask(const UString &existing, const FILETIME *existingTime, const UInt64 *existingSize,
                         const UString &incoming, const FILETIME *incomingTime, const UInt64 *incomingSize,
                         bool existingDirectory, bool incomingDirectory, Int32 *answer) {
  if (!answer || !Enabled()) return E_FAIL;
  static UInt64 serial = 0; const UInt64 id = ++serial;
  const auto parent = ::getppid();
  const std::string request = "{\"version\":1,\"mode\":\"overwrite\",\"id\":\"" + std::to_string(id) +
    "\",\"existingPath\":" + quote(existing) + ",\"incomingPath\":" + quote(incoming) +
    ",\"existingSize\":" + size(existingSize) + ",\"incomingSize\":" + size(incomingSize) +
    ",\"existingTime\":" + time(existingTime) + ",\"incomingTime\":" + time(incomingTime) +
    ",\"existingDirectory\":" + (existingDirectory ? "true" : "false") +
    ",\"incomingDirectory\":" + (incomingDirectory ? "true" : "false") + "}";
  if (request.size() > 60000) return E_FAIL;
  // Requests require a reply; telemetry remains optional. Retry a dropped
  // request without duplicating the host prompt, and observe Cancel/peer loss.
  bool sendRequest = true;
  for (;;) {
    if (NConsoleClose::TestBreakSignal() || ::getppid() != parent) return E_ABORT;
    if (sendRequest) {
      const auto sent = ::send(3, request.data(), request.size(), MSG_DONTWAIT);
      if (sent < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) return E_ABORT;
    }
    struct pollfd descriptor{3, POLLIN, 0};
    const auto ready = ::poll(&descriptor, 1, 250);
    if (ready < 0) { if (errno == EINTR) continue; return E_ABORT; }
    if (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) return E_ABORT;
    sendRequest = ready == 0;
    if (!(descriptor.revents & POLLIN)) continue;
    char packet[256]; const auto count = ::recv(3, packet, sizeof(packet), MSG_DONTWAIT);
    if (count < 0) { if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) continue; return E_ABORT; }
    if (!count) return E_ABORT;
    if (count == sizeof(packet)) continue;
    if (parse(std::string(packet, size_t(count)), id, *answer)) return S_OK;
  }
}
