// SPDX-License-Identifier: LGPL-3.0-or-later
#include "FolderStatistics.h"
#include <QElapsedTimer>
#include <QFile>
#include <QScopeGuard>
#include <QtConcurrent>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <functional>
#include <limits>

namespace {
QString error(QString path, int code = errno) {
    return "Cannot calculate folder size: " + path + '\n' + QString::fromLocal8Bit(std::strerror(code)) + " (" + QString::number(code) + ')';
}
bool addSize(FolderStatistic &stat, quint64 size, QString path) {
    if (stat.size > std::numeric_limits<quint64>::max() - size) {
        stat.error = "Folder size exceeds the supported range: " + path; return false;
    }
    stat.size += size; return true;
}
bool scan(int descriptor, QString path, FolderStatistic &stat, const std::shared_ptr<OperationControl> &control,
          QElapsedTimer &clock, const std::function<void(quint64, QString)> &progress, unsigned depth = 0) {
    DIR *directory = ::fdopendir(descriptor);
    if (!directory) { const int code = errno; ::close(descriptor); stat.error = error(path, code); return false; }
    const auto close = qScopeGuard([&] { ::closedir(directory); });
    for (;;) {
        if (control->checkpoint()) return false;
        errno = 0; const auto entry = ::readdir(directory);
        if (!entry) { if (errno) stat.error = error(path); return stat.error.isEmpty(); }
        if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
        const auto child = path + '/' + QFile::decodeName(entry->d_name);
        struct stat stamp{};
        if (::fstatat(::dirfd(directory), entry->d_name, &stamp, AT_SYMLINK_NOFOLLOW) != 0) { stat.error = error(child); return false; }
        if (S_ISDIR(stamp.st_mode)) {
            ++stat.folders;
            // Avoid stack exhaustion. A normal macOS path reaches PATH_MAX
            // long before this limit; excessive nesting is reported, not partial.
            if (depth >= 256) { stat.error = "Folder nesting is too deep: " + child; return false; }
            const int nested = ::openat(::dirfd(directory), entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (nested < 0) { stat.error = error(child); return false; }
            struct stat opened{};
            if (::fstat(nested, &opened) != 0) { const int code = errno; ::close(nested); stat.error = error(child, code); return false; }
            if (opened.st_dev != stamp.st_dev || opened.st_ino != stamp.st_ino) {
                ::close(nested); stat.error = "Folder changed during calculation: " + child; return false;
            }
            if (!scan(nested, child, stat, control, clock, progress, depth + 1)) return false;
        } else {
            ++stat.files;
            if (!addSize(stat, quint64(qMax<off_t>(0, stamp.st_size)), child)) return false;
        }
        if (clock.elapsed() >= 50) { progress(stat.size, child); clock.restart(); }
    }
}
}

FolderStatistics::FolderStatistics(QObject *parent) : QObject(parent) {
    qRegisterMetaType<FolderStatisticsResult>();
    connect(&watcher, &QFutureWatcher<FolderStatisticsResult>::started, this, [this] { emit pauseAvailabilityChanged(true); });
    connect(&watcher, &QFutureWatcher<FolderStatisticsResult>::finished, this, [this] {
        emit pausedChanged(false); emit pauseAvailabilityChanged(false); emit finished(watcher.result());
    });
}
FolderStatistics::~FolderStatistics() { cancel(); watcher.waitForFinished(); }
void FolderStatistics::cancel() { if (control) control->cancel(); emit pausedChanged(false); emit pauseAvailabilityChanged(false); }
bool FolderStatistics::setPaused(bool paused) {
    if (!busy() || !control) return false;
    control->setPaused(paused); emit pausedChanged(paused); return true;
}
void FolderStatistics::start(QStringList paths) {
    if (busy()) return;
    control = std::make_shared<OperationControl>(); const auto flag = control;
    watcher.setFuture(QtConcurrent::run([this, paths, flag] {
        FolderStatisticsResult result; QElapsedTimer clock; clock.start();
        for (const auto &path : paths) {
            if (flag->checkpoint()) { result.cancelled = true; break; }
            FolderStatistic stat; stat.path = path;
            const int descriptor = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (descriptor < 0) stat.error = error(path);
            else {
                emit progress(0, path);
                stat.complete = scan(descriptor, path, stat, flag, clock, [this](quint64 size, QString name) { emit progress(size, name); });
            }
            if (flag->checkpoint()) { result.cancelled = true; break; }
            result.items.append(stat);
        }
        return result;
    }));
}
