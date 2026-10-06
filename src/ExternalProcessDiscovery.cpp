// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ExternalProcessDiscovery.h"
#include <QFile>
#include <QFileInfo>
#include <QVector>
#include <algorithm>
#include <cerrno>
#include <libproc.h>
#include <sys/proc.h>
#include <unistd.h>

namespace {
using DWORD = pid_t;
constexpr int SYNCHRONIZE = 0;
struct UString {
    QString value;
    int ReverseFind_PathSepar() const { return value.lastIndexOf('/'); }
    UString Ptr(unsigned offset) const { return {value.mid(offset)}; }
    bool IsEmpty() const { return value.isEmpty(); }
    bool IsEqualTo_NoCase(const UString &other) const { return value.compare(other.value, Qt::CaseInsensitive) == 0; }
};
template<class T> struct CObjectVector : QVector<T> {
    void Add(const T &item) { this->append(item); }
};
struct Ids : QVector<pid_t> {
    int FindInSorted(pid_t id) const { const auto it = std::lower_bound(begin(), end(), id); return it != end() && *it == id ? int(it - begin()) : -1; }
    void AddToUniqueSorted(pid_t id) { const auto it = std::lower_bound(begin(), end(), id); if (it == end() || *it != id) insert(it, id); }
};
struct CSnapshotProcess {
    DWORD Id, ParentId;
    UString Name;
    ExternalProcessIdentity identity;
};
using HANDLE = const CSnapshotProcess *;
struct CChildProcesses {
    Ids _ids;
    UString Path;
    CObjectVector<HANDLE> Handles;
    CObjectVector<bool> NeedWait;
    CObjectVector<CSnapshotProcess> snapshot;
    void GetSnapshot(CObjectVector<CSnapshotProcess> &items) const { items = snapshot; }
    HANDLE OpenProcess(int, bool, pid_t id) const {
        for (const auto &item : snapshot) if (item.Id == id) {
            struct proc_bsdinfo current{};
            if (::proc_pidinfo(id, PROC_PIDTBSDINFO, 0, &current, sizeof(current)) != sizeof(current) || current.pbi_status == SZOMB) return nullptr;
            const ExternalProcessIdentity identity{current.pbi_start_tvsec, current.pbi_start_tvusec};
            return identity == item.identity ? &item : nullptr;
        }
        return nullptr;
    }
    pid_t GetCurrentProcessId() const { return ::getpid(); }
#define FOR_VECTOR(i, vector) for (int i = 0; i < (vector).size(); ++i)
#define DEBUG_PRINT(value) do {} while (false)
#define DEBUG_PRINT_W(value) do {} while (false)
#include "upstream/ProcessDiscovery.inc"
#undef DEBUG_PRINT_W
#undef DEBUG_PRINT
#undef FOR_VECTOR
};
}

QMap<pid_t, ExternalProcessIdentity> discoverExternalProcesses(
    const QMap<pid_t, ExternalProcessIdentity> &known, pid_t mainProcess,
    const QString &mainPath, bool findByName, bool *unknown) {
    CChildProcesses processes;
    processes.Path.value = mainPath;
    errno = 0;
    const auto required = ::proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (required <= 0) { *unknown = true; return {}; }
    QVector<pid_t> pids(required / int(sizeof(pid_t)) + 128);
    errno = 0;
    const auto size = ::proc_listpids(PROC_ALL_PIDS, 0, pids.data(), int(pids.size() * sizeof(pid_t)));
    if (size <= 0 || size == int(pids.size() * sizeof(pid_t))) { *unknown = true; return {}; }
    pids.resize(size / int(sizeof(pid_t)));
    for (auto pid : pids) {
        if (pid <= 0) continue;
        struct proc_bsdinfo info{};
        if (::proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) continue;
        char path[PROC_PIDPATHINFO_MAXSIZE]{};
        const auto pathSize = ::proc_pidpath(pid, path, sizeof(path));
        const QString name = pathSize > 0 ? QFileInfo(QFile::decodeName(path)).fileName() : QString::fromUtf8(info.pbi_comm);
        processes.snapshot.Add({pid, pid_t(info.pbi_ppid), {name}, {info.pbi_start_tvsec, info.pbi_start_tvusec}});
    }
    // A terminated original parent remains an anchor, as in the official
    // vector. A PID now belonging to another process must not become one.
    for (auto it = known.cbegin(); it != known.cend(); ++it) {
        auto current = std::find_if(processes.snapshot.cbegin(), processes.snapshot.cend(), [&](const auto &item) { return item.Id == it.key(); });
        if (current == processes.snapshot.cend() || current->identity == it.value()) processes._ids.AddToUniqueSorted(it.key());
    }
    if (!known.contains(mainProcess)) processes._ids.AddToUniqueSorted(mainProcess);
    processes.Update(findByName);
    QMap<pid_t, ExternalProcessIdentity> result;
    for (auto handle : processes.Handles) result.insert(handle->Id, handle->identity);
    return result;
}
