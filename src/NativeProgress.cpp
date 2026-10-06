// SPDX-License-Identifier: LGPL-3.0-or-later
// Supplemental, optional telemetry for the unchanged official console workflow.
#include "NativeProgress.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/7zip/UI/Common/Extract.h"
#include "CPP/7zip/UI/Common/HashCalc.h"
#include <fcntl.h>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {
using Clock = std::chrono::steady_clock;
struct Value { bool known; std::uint64_t value; Value(bool defined = false, std::uint64_t number = 0) : known(defined), value(number) {} };
std::string jsonString(const std::string &text) {
    std::string result = "\"";
    const char hex[] = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { result += '\\'; result += char(c); }
        else if (c < 32) { result += "\\u00"; result += hex[c >> 4]; result += hex[c & 15]; }
        else result += char(c);
    }
    result += '"'; return result;
}
std::string unicode(const wchar_t *text) {
    if (!text) return {}; AString utf8; ConvertUnicodeToUTF8(UString(text), utf8); return {utf8.Ptr(), utf8.Len()};
}
struct State {
    std::mutex mutex;
    bool enabled = false, directory = false;
    std::string mode = "unknown", current, title, status;
    Value total, completed, input, output, files;
    std::uint64_t doneFiles = 0;
    Clock::time_point sent{};
    State() {
        const char *fd = std::getenv("SEVENZIP_PORT_PROGRESS_FD");
        int type = 0; socklen_t length = sizeof(type);
        enabled = fd && std::strcmp(fd, "3") == 0 && getsockopt(3, SOL_SOCKET, SO_TYPE, &type, &length) == 0 && type == SOCK_DGRAM;
        if (enabled) { int yes = 1; setsockopt(3, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes)); }
    }
    void send(bool force) {
        const auto now = Clock::now(); if (!force && now - sent < std::chrono::milliseconds(200)) return;
        auto field = [](const Value &value) { return value.known ? jsonString(std::to_string(value.value)) : "null"; };
        const std::string prefix = "{\"version\":1,\"mode\":" + jsonString(mode) + ",\"total\":" + field(total) + ",\"completed\":" + field(completed) + ",\"input\":" + field(input) + ",\"output\":" + field(output) + ",\"files\":" + field(files) + ",\"doneFiles\":" + jsonString(std::to_string(doneFiles)) + ",\"current\":";
        const auto suffix = ",\"title\":" + jsonString(title) + ",\"status\":" + jsonString(status) + ",\"directory\":" + (directory ? "true" : "false") + "}";
        std::string record = prefix + jsonString(current) + suffix;
        if (record.size() > 60000) record = prefix + "\"\"}";
        // A slow/closed GUI must never block or interrupt the official operation.
        const auto result = ::send(3, record.data(), record.size(), MSG_DONTWAIT);
        if (result >= 0) sent = now;
    }
};
State &state() { static State value; return value; }
template<class Function> void update(Function change, bool force = false) noexcept {
    auto &value = state(); if (!value.enabled) return;
    try { std::lock_guard<std::mutex> lock(value.mutex); change(value); value.send(force); }
    catch (...) { /* Telemetry is optional, even when allocation fails. */ }
}
}
bool PortProgress::Enabled() noexcept { return state().enabled; }
void PortProgress::Begin(const char *mode, const wchar_t *name, bool preserveFileTotal) noexcept {
    update([=](State &state) { const auto files = state.files; state.mode = mode; state.title = unicode(name); state.current.clear(); state.status = mode; state.total = {}; state.completed = {}; state.input = {}; state.output = {}; state.files = preserveFileTotal ? files : Value{}; state.doneFiles = 0; state.directory = false; }, true);
}
void PortProgress::Current(const wchar_t *name, bool directory) noexcept { update([=](State &state) { state.current = unicode(name); state.directory = directory; }); }
void PortProgress::Status(const char *status, bool force) noexcept { update([=](State &state) { state.status = status; }, force); }
void PortProgress::Scanning(std::uint64_t files, std::uint64_t bytes, const wchar_t *path, bool directory) noexcept {
    update([=](State &state) { state.doneFiles = files; state.total = {true, bytes}; state.current = unicode(path); state.directory = directory; state.status = "scan"; });
}
void PortProgress::UpdateOperation(unsigned operation, const wchar_t *name, bool directory) noexcept {
    update([=](State &state) { state.status = "update:" + std::to_string(operation); state.current = unicode(name); state.directory = directory; });
}
void PortProgress::Error(int code, bool encrypted, const wchar_t *name, const wchar_t *message) noexcept {
    auto &value = state(); if (!value.enabled) return;
    try {
        std::lock_guard<std::mutex> lock(value.mutex);
        const auto record = "{\"version\":1,\"mode\":\"operationError\",\"code\":" + std::to_string(code) +
            ",\"encrypted\":" + (encrypted ? "true" : "false") + ",\"name\":" + jsonString(unicode(name)) +
            ",\"archive\":" + jsonString(value.title) + ",\"message\":" + jsonString(unicode(message)) + "}";
        if (record.size() <= 60000) ::send(3, record.data(), record.size(), MSG_DONTWAIT);
    } catch (...) { /* Optional display must never interrupt the operation. */ }
}
void PortProgress::Total(std::uint64_t size) noexcept { update([=](State &state) { state.total = {true, size}; }, true); }
void PortProgress::Completed(const std::uint64_t *size) noexcept { if (size) update([=](State &state) { state.completed = {true, *size}; }); }
void PortProgress::Ratio(const std::uint64_t *input, const std::uint64_t *output) noexcept { update([=](State &state) { if (input) state.input = {true, *input}; if (output) state.output = {true, *output}; }); }
void PortProgress::Files(std::uint64_t count) noexcept { update([=](State &state) { state.files = {true, count}; }, true); }
void PortProgress::FileDone() noexcept { update([](State &state) { if (state.mode == "compress" || !state.directory) ++state.doneFiles; }); }
void PortProgress::Finish() noexcept { update([](State &state) { state.status.clear(); }, true); }
void PortProgress::Record(const std::string &json) noexcept {
    auto &value = state(); if (!value.enabled || json.size() > 60000) return;
    try { std::lock_guard<std::mutex> lock(value.mutex); ::send(3, json.data(), json.size(), MSG_DONTWAIT); }
    catch (...) { /* A dropped benchmark display must not interrupt the engine. */ }
}

// Completion data uses an owned sidecar rather than lossy progress datagrams.
// Only UI/statistics values are serialized; engine operation results are intact.
void PortProgress::Completion(const CDecompressStat *statistics, const CHashBundle *hash) noexcept {
    const char *path = std::getenv("SEVENZIP_PORT_COMPLETION_PATH");
    if (!path || !*path) return;
    int fd = -1;
    try {
        auto number = [](std::uint64_t value) { return jsonString(std::to_string(value)); };
        std::string record = "{\"version\":1,\"decompression\":";
        if (statistics) {
            const auto &s = *statistics;
            record += "{\"NumArchives\":" + number(s.NumArchives) + ",\"UnpackSize\":" + number(s.UnpackSize) + ",\"AltStreams_UnpackSize\":" + number(s.AltStreams_UnpackSize) + ",\"PackSize\":" + number(s.PackSize) + ",\"NumFolders\":" + number(s.NumFolders) + ",\"NumFiles\":" + number(s.NumFiles) + ",\"NumAltStreams\":" + number(s.NumAltStreams) + "}";
        } else record += "null";
        record += ",\"hash\":";
        if (hash) {
            const auto &h = *hash;
            std::string first = unicode(h.FirstFileName);
            if (first.empty() && h.NumFiles == 1 && h.NumDirs == 0) { auto &s = state(); std::lock_guard<std::mutex> lock(s.mutex); first = s.current; }
            record += "{\"NumDirs\":" + number(h.NumDirs) + ",\"NumFiles\":" + number(h.NumFiles) + ",\"NumAltStreams\":" + number(h.NumAltStreams) + ",\"FilesSize\":" + number(h.FilesSize) + ",\"AltStreamsSize\":" + number(h.AltStreamsSize) + ",\"NumErrors\":" + number(h.NumErrors) + ",\"MainName\":" + jsonString(unicode(h.MainName)) + ",\"FirstFileName\":" + jsonString(first) + ",\"hashers\":[";
            for (unsigned i = 0; i < h.Hashers.Size(); ++i) {
                const auto &hasher = h.Hashers[i]; if (i) record += ',';
                record += "{\"name\":" + jsonString(hasher.Name.Ptr()) + ",\"groups\":[";
                for (unsigned group = 1; group < k_HashCalc_NumGroups; ++group) {
                    char text[k_HashCalc_DigestSize_Max * 2 + k_HashCalc_ExtraSize * 2 + 16]; hasher.WriteToString(group, text);
                    if (group != 1) record += ','; record += jsonString(text);
                }
                record += "]}";
            }
            record += "]}";
        } else record += "null";
        record += "}";
        if (record.size() > 1024 * 1024) return;
        fd = ::open(path, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (fd < 0) return;
        std::size_t cursor = 0;
        while (cursor < record.size()) {
            const auto count = ::write(fd, record.data() + cursor, record.size() - cursor);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) { ::close(fd); return; }
            cursor += std::size_t(count);
        }
        ::close(fd);
    } catch (...) { if (fd >= 0) ::close(fd); }
}
