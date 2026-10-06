// SPDX-License-Identifier: LGPL-3.0-or-later
// Adapter for the official portable enumerator's logical-prefix argument.
#include "ArchiveTransferPort.h"
#include "CPP/Common/UTFConvert.h"
#include "CPP/Common/StringConvert.h"
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

bool PortArchiveTransfer::Enabled() noexcept {
    const auto value = std::getenv("SEVENZIP_PORT_COPY_FROM");
    return value && std::strcmp(value, "1") == 0;
}
UString PortArchiveTransfer::Prefix() {
    UString result;
    if (!Enabled()) return result;
    const auto value = std::getenv("SEVENZIP_PORT_ADD_PREFIX");
    if (!value || !*value) return result;
    if (!ConvertUTF8ToUnicode(AString(value), result)) throw "Invalid archive transfer prefix UTF-8";
    if (result[0] == L'/' || result.Back() != L'/') throw "Invalid archive transfer prefix";
    unsigned component = 0;
    for (unsigned i = 0; i < result.Len(); ++i) {
        const auto c = result[i];
        if (c < 32 || c == L'\\' || (i == 1 && c == L':')) throw "Invalid archive transfer prefix";
        if (c == L'/') {
            const auto length = i - component;
            if (!length || (length == 1 && result[component] == L'.') ||
                (length == 2 && result[component] == L'.' && result[component + 1] == L'.'))
                throw "Invalid archive transfer prefix";
            component = i + 1;
        }
    }
    return result;
}

HRESULT PortArchiveTransfer::Enumerate(CDirItems &items) {
    // OnCopy / SetFiles keeps the requested vector, including duplicate names.
    // A CLI wildcard censor would merge identical arguments before scanning.
    const char *path = std::getenv("SEVENZIP_PORT_COPY_INPUT");
    if (!Enabled() || !path || !*path) return E_INVALIDARG;
    const int fd = ::open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return E_INVALIDARG;
    struct Close { int fd; ~Close() { ::close(fd); } } close{fd};
    struct stat st{};
    if (::fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != ::geteuid() ||
        (st.st_mode & 077) || st.st_size < 16 || st.st_size > 64 * 1024 * 1024) return E_INVALIDARG;
    std::vector<unsigned char> data(size_t(st.st_size)); size_t pos = 0;
    while (pos < data.size()) {
        const auto n = ::read(fd, data.data() + pos, data.size() - pos);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return E_INVALIDARG;
        pos += size_t(n);
    }
    if (std::memcmp(data.data(), "7ZCPY001", 8)) return E_INVALIDARG;
    pos = 8;
    auto number = [&](UInt32 &n) {
        if (data.size() - pos < 4) return false;
        n = 0; for (unsigned i = 0; i < 4; ++i) n |= UInt32(data[pos++]) << (8 * i);
        return true;
    };
    auto text = [&](UString &value) {
        UInt32 n; if (!number(n) || n > data.size() - pos) return false;
        if (std::memchr(data.data() + pos, 0, n)) return false;
        AString bytes; bytes.SetFrom(reinterpret_cast<const char *>(data.data() + pos), n); pos += n;
        return ConvertUTF8ToUnicode(bytes, value);
    };
    UString base; UInt32 count;
    if (!text(base) || (!base.IsEmpty() && (base[0] != L'/' || base.Back() != L'/')) ||
        !number(count) || !count || count > 1000000) return E_INVALIDARG;
    FStringVector names;
    for (UInt32 i = 0; i < count; ++i) { UString value; if (!text(value) || value.IsEmpty()) return E_INVALIDARG; names.Add(us2fs(value)); }
    if (pos != data.size()) return E_INVALIDARG;
    // Original AgentOut.cpp::DoOperation scanner; no Port enumeration algorithm.
    return items.EnumerateItems2(us2fs(base), Prefix(), names, NULL);
}
