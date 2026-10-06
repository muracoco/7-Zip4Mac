// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <string>
#include <sys/stat.h>

// Shared by the native reader and Qt's atomic single-file installation. This
// is an identity/change token, not an archive checksum or a locking scheme.
inline std::string archiveSourceStamp(const struct stat &st) {
    return std::to_string(st.st_dev) + ',' + std::to_string(st.st_ino) + ',' +
        std::to_string(st.st_size) + ',' + std::to_string(st.st_mtimespec.tv_sec) + ',' +
        std::to_string(st.st_mtimespec.tv_nsec) + ',' + std::to_string(st.st_ctimespec.tv_sec) + ',' +
        std::to_string(st.st_ctimespec.tv_nsec) + ';';
}
