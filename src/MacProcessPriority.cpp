// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MacProcessPriority.h"
#include <sys/resource.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <set>
namespace {
std::mutex mutex;
std::set<const void *> owners;
int original = 0;
}
QString MacProcessPriority::set(qint64 pid, bool background) {
    if (pid <= 0 || pid > 0x7fffffff) return "Cannot change priority: invalid owned process.";
    if (::setpriority(PRIO_DARWIN_PROCESS, id_t(pid), background ? PRIO_DARWIN_BG : 0) == 0) return {};
    return "Cannot change process priority: " + QString::fromLocal8Bit(std::strerror(errno));
}
QString MacProcessPriority::lease(const void *owner, bool background) {
    std::lock_guard<std::mutex> guard(mutex);
    if (background == (owners.count(owner) != 0)) return {};
    if (background && owners.empty()) {
        errno = 0; original = ::getpriority(PRIO_DARWIN_PROCESS, 0);
        if (errno) return "Cannot read process priority: " + QString::fromLocal8Bit(std::strerror(errno));
        const auto error = set(::getpid(), true); if (!error.isEmpty()) return error;
    }
    if (!background && owners.size() == 1) { const auto error = set(::getpid(), original != 0); if (!error.isEmpty()) return error; }
    if (background) owners.insert(owner); else owners.erase(owner);
    return {};
}
void MacProcessPriority::release(const void *owner) { (void)lease(owner, false); }
