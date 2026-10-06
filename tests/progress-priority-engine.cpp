// SPDX-License-Identifier: LGPL-3.0-or-later
// Owned native process fixture. No interpreter launcher or archive operations.
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <sys/resource.h>
#include <libproc.h>
#include <mach/task_policy.h>
#include <unistd.h>

int main(int argc, char **argv) {
    namespace fs = std::filesystem;
    const auto root = argc == 2 && fs::is_directory(argv[1]) ? fs::path(argv[1]) : fs::current_path();
    auto write = [&](const char *name, int value) {
        const auto path = root / name;
        const auto temporary = root / (std::string(name) + ".tmp");
        { std::ofstream file(temporary); file << value; if (!file) return false; }
        std::error_code error; fs::rename(temporary, path, error); return !error;
    };
    if (!write("pid", int(::getpid()))) return 1;
    for (;;) {
        proc_bsdinfo info{};
        if (::proc_pidinfo(::getpid(), PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) return 1;
        const bool background = info.pbi_flags & (PROC_FLAG_DARWINBG | PROC_FLAG_EXT_DARWINBG);
        if (!write("priority", background ? 1 : 0)) return 1;
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}
