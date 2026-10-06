// SPDX-License-Identifier: LGPL-3.0-or-later
#include "DirectoryScanner.h"
#include "FileComments.h"
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QtConcurrent>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace {
QString error(QString path, int code = errno) {
    return "Cannot read folder: " + path + '\n' + QString::fromLocal8Bit(std::strerror(code)) + " (" + QString::number(code) + ')';
}
bool enumerate(int descriptor, QString path, DirectorySnapshot &snapshot, const std::shared_ptr<std::atomic_bool> &cancelled, unsigned depth = 0) {
    DIR *directory = ::fdopendir(descriptor);
    if (!directory) { const int code = errno; ::close(descriptor); snapshot.error = error(path, code); return false; }
    const auto close = qScopeGuard([&] { ::closedir(directory); });
    for (;;) {
        if (cancelled->load()) { snapshot.cancelled = true; return false; }
        errno = 0; const auto native = ::readdir(directory);
        if (!native) { if (errno) snapshot.error = error(path); return snapshot.error.isEmpty(); }
        if (!std::strcmp(native->d_name, ".") || !std::strcmp(native->d_name, "..")) continue;
        const auto name = QFile::decodeName(native->d_name), child = QDir(path).filePath(name);
        struct stat stamp{};
        if (::fstatat(::dirfd(directory), native->d_name, &stamp, AT_SYMLINK_NOFOLLOW) != 0) { snapshot.error = error(child); return false; }
        DirectoryEntry entry; entry.path = child; entry.name = name; entry.iconInfo = QFileInfo(child);
        // Populate QFileInfo's cache before crossing the thread boundary. Native
        // icons remain on the GUI thread, while all displayed metadata is ready.
        entry.directory = entry.iconInfo.isDir(); entry.link = S_ISLNK(stamp.st_mode);
        entry.size = quint64(qMax<qint64>(0, stamp.st_size)); entry.suffix = entry.iconInfo.suffix().toUpper();
        auto time=[](timespec value) { return QDateTime::fromMSecsSinceEpoch(qint64(value.tv_sec)*1000+value.tv_nsec/1000000); };
        entry.modified = time(stamp.st_mtimespec); entry.created = time(stamp.st_birthtimespec);
        entry.accessed = time(stamp.st_atimespec); entry.changed = time(stamp.st_ctimespec);
        entry.modifiedFraction = QString::number(stamp.st_mtimespec.tv_nsec).rightJustified(9, '0');
        entry.createdFraction = QString::number(stamp.st_birthtimespec.tv_nsec).rightJustified(9, '0');
        entry.accessedFraction = QString::number(stamp.st_atimespec.tv_nsec).rightJustified(9, '0');
        entry.changedFraction = QString::number(stamp.st_ctimespec.tv_nsec).rightJustified(9, '0');
        entry.packed = quint64(qMax<qint64>(0, stamp.st_blocks))*512; entry.inode=quint64(stamp.st_ino); entry.links=quint64(stamp.st_nlink); entry.mode=stamp.st_mode;
        if (snapshot.flat) entry.prefix = QDir(snapshot.path).relativeFilePath(path);
        snapshot.entries.append(std::move(entry));
        if (snapshot.flat && S_ISDIR(stamp.st_mode)) {
            if (depth >= 256) { snapshot.error = "Folder nesting is too deep: " + child; return false; }
            const int nested = ::openat(::dirfd(directory), native->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (nested < 0) { snapshot.error = error(child); return false; }
            struct stat opened{};
            if (::fstat(nested, &opened) != 0) { const int code = errno; ::close(nested); snapshot.error = error(child, code); return false; }
            if (opened.st_dev != stamp.st_dev || opened.st_ino != stamp.st_ino) { ::close(nested); snapshot.error = "Folder changed while reading: " + child; return false; }
            if (!enumerate(nested, child, snapshot, cancelled, depth + 1)) return false;
        }
    }
}
}
DirectoryScanner::DirectoryScanner(QObject *parent) : QObject(parent) { qRegisterMetaType<DirectorySnapshot>(); }
DirectoryScanner::~DirectoryScanner() { cancel(); }
void DirectoryScanner::cancel() { ++generation; for (const auto &job : jobs) job.cancelled->store(true); }
quint64 DirectoryScanner::start(QString path, bool flat) {
    cancel(); const auto id = generation;
    const auto flag = std::make_shared<std::atomic_bool>(false); auto watcher = new QFutureWatcher<DirectorySnapshot>(this);
    jobs.insert(id, {watcher, flag});
    connect(watcher, &QFutureWatcher<DirectorySnapshot>::finished, this, [this, id, watcher] {
        auto snapshot = watcher->result(); jobs.remove(id); watcher->deleteLater();
        if (id == generation) emit finished(std::move(snapshot));
    });
    watcher->setFuture(QtConcurrent::run([path = QDir(path).absolutePath(), flat, id, flag] {
        DirectorySnapshot snapshot; snapshot.generation = id; snapshot.path = path; snapshot.flat = flat;
        // A root alias is a user-selected location; child aliases are never
        // followed by Flat View. This preserves normal symlink-folder browsing.
        const int descriptor = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (descriptor < 0) snapshot.error = error(path);
        else {
            const auto comments = FileComments::readAt(descriptor);
            enumerate(descriptor, path, snapshot, flag);
            if (comments.error.isEmpty()) for (auto &entry : snapshot.entries) {
                if (flag->load()) { snapshot.cancelled = true; break; }
                entry.comment = FileComments::display(comments.value(QDir(path).relativeFilePath(entry.path)));
            }
        }
        if (flag->load()) snapshot.cancelled = true;
        return snapshot;
    }));
    return id;
}
