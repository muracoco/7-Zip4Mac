#include "FileOperations.h"
#include "FileInstall.h"
#include <QScopeGuard>
#include <dirent.h>
#include <cstring>
#include <cerrno>
#include <copyfile.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QtConcurrent>
#include <unistd.h>
#include <fcntl.h>

FileOperations::FileOperations(QObject *parent) : QObject(parent) {
    connect(&watcher, &QFutureWatcher<QString>::started, this, [this] { emit pauseAvailabilityChanged(!overwriteBroker.waiting()); });
    connect(&watcher, &QFutureWatcher<QString>::finished, this, [this] { active = false; emit pausedChanged(false); emit pauseAvailabilityChanged(false); emit finished(watcher.result()); });
    connect(&overwriteBroker, &OverwriteBroker::requested, this, [this](OverwriteConflict conflict) {
        if (busy() && overwriteBroker.current(conflict.id)) emit overwriteRequested(conflict);
    });
    connect(&overwriteBroker, &OverwriteBroker::waitingChanged, this, [this] { emit pauseAvailabilityChanged(busy() && stop && !stop->isCancelled() && !overwriteBroker.waiting()); });
}
FileOperations::~FileOperations() { cancel(); watcher.waitForFinished(); }
void FileOperations::cancel() { if (stop) stop->cancel(); overwriteBroker.cancel(); emit pausedChanged(false); emit pauseAvailabilityChanged(false); }
bool FileOperations::setPaused(bool value) {
    if (!busy() || !stop || stop->isCancelled() || overwriteBroker.waiting()) return false;
    stop->setPaused(value); emit pausedChanged(value); return true;
}
bool FileOperations::resolveOverwrite(quint64 id, OverwriteAnswer answer) { return busy() && overwriteBroker.answer(id, answer); }
namespace {
QString explicitOutputPath(QString path) {
    // User-entered parents may traverse standard macOS aliases such as /var.
    // Canonicalize their existing ancestor once; later installation walks
    // actual directory handles and refuses output links.
    QString cursor = QDir::cleanPath(QFileInfo(path).absoluteFilePath()); QStringList tail;
    while (!QFileInfo::exists(cursor) && !QFileInfo(cursor).isSymLink()) {
        const QFileInfo item(cursor); if (item.fileName().isEmpty()) return {};
        tail.prepend(item.fileName()); cursor = item.absolutePath();
    }
    const auto root = QFileInfo(cursor).canonicalFilePath(); if (root.isEmpty()) return {};
    return tail.isEmpty() ? root : root + '/' + tail.join('/');
}
QString copyItem(const FileSnapshot &source, FileSnapshot destination, QString &mode,
    bool move, OverwriteBroker &broker, const std::shared_ptr<OperationControl> &stop,
    const std::function<void(QString)> &progress) {
    if (stop->checkpoint()) return "Operation cancelled. Completed files were retained.";
    if (!source.error.isEmpty() || !destination.error.isEmpty()) return source.error + destination.error;
    if (!source.exists) return "Source does not exist: " + source.path;
    progress(source.path);
    if (!S_ISDIR(source.stamp.st_mode)) {
        if (destination.exists && S_ISDIR(destination.stamp.st_mode)) return "Cannot replace a folder with a file: " + destination.path;
        const auto installed = FileInstall::transfer(source, destination, mode, broker, stop, {}, true, {}, move);
        if (!installed.error.isEmpty()) return installed.error;
        if (move && installed.installed && !installed.sourceMoved) {
            if (stop->checkpoint()) return "Operation cancelled. Copied files and sources were retained.";
            if (!FileInstall::unchanged(installed.installedFile)) return "Copied output changed; source retained: " + source.path;
            return FileInstall::removeMovedSource(source);
        }
        return {};
    }
    if (destination.exists && !S_ISDIR(destination.stamp.st_mode)) return "Cannot replace a file or link with a folder: " + destination.path;
    if (move) { const auto renamed = FileInstall::tryMove(source, destination, stop); if (renamed) return renamed->error; }
    QString error; const auto sourceFolder = FileInstall::openFolder(source, &error); if (!sourceFolder) return error;
    const bool created = !destination.exists;
    if (created) {
        error = FileInstall::ensureDirectory(destination.path); if (!error.isEmpty()) return error;
        destination = FileInstall::captureAt(destination.parent, destination.name);
    }
    const auto targetFolder = FileInstall::openFolder(destination, &error); if (!targetFolder) return error;
    DIR *directory = ::fdopendir(::dup(sourceFolder->descriptor)); if (!directory) return "Cannot enumerate source folder: " + source.path;
    const auto close = qScopeGuard([&] { ::closedir(directory); });
    QList<QByteArray> names;
    for (;;) {
        errno = 0; const auto entry = ::readdir(directory);
        if (!entry) { if (errno) return "Cannot enumerate source folder: " + source.path; break; }
        const QByteArray name(entry->d_name); if (name == "." || name == "..") continue;
        if (QFile::encodeName(QFile::decodeName(name)) != name) return "Unsupported filename encoding: " + source.path;
        names << name;
    }
    std::sort(names.begin(), names.end());
    for (const auto &name : names) {
        error = copyItem(FileInstall::captureAt(sourceFolder, name), FileInstall::captureAt(targetFolder, name), mode, move, broker, stop, progress);
        if (!error.isEmpty()) return error;
    }
    if (created) {
        const timespec times[2]{source.stamp.st_atimespec, source.stamp.st_mtimespec};
        if (::fcopyfile(sourceFolder->descriptor, targetFolder->descriptor, nullptr, COPYFILE_ACL | COPYFILE_XATTR) != 0 || ::fchmod(targetFolder->descriptor, source.stamp.st_mode & 07777) != 0 || ::futimens(targetFolder->descriptor, times) != 0) return "Cannot preserve output folder metadata: " + destination.path;
    }
    if (move) {
        if (stop->checkpoint()) return "Operation cancelled. Completed moves were retained.";
        const auto remaining = FileInstall::captureAt(source.parent, source.name);
        if (!remaining.exists || remaining.stamp.st_dev != source.stamp.st_dev || remaining.stamp.st_ino != source.stamp.st_ino) return "Source folder changed; retained: " + source.path;
        return FileInstall::removeMovedSource(remaining); // rmdir only; skipped children remain.
    }
    return {};
}
}
void FileOperations::transfer(QStringList sources, QString destination, bool move) {
    if (busy()) return;
    stop = std::make_shared<OperationControl>(); auto flag = stop; overwriteBroker.reset();
    active = true; watcher.setFuture(QtConcurrent::run([this, sources, destination, move, flag]() -> QString {
        if (sources.isEmpty() || destination.isEmpty()) return "Choose source files and a destination.";
        const bool folder = sources.size() != 1 || destination.endsWith('/') || (QFileInfo(destination).isDir() && !QFileInfo(destination).isSymLink());
        if (folder && QFileInfo(destination).isSymLink()) return "Symbolic-link destination folder refused: " + destination;
        QString base = explicitOutputPath(folder ? destination : QFileInfo(destination).absolutePath());
        if (base.isEmpty()) return "Cannot resolve destination folder: " + destination;
        const auto directoryError = FileInstall::ensureDirectory(base); if (!directoryError.isEmpty()) return directoryError;
        emit transferDestinationPrepared(destination);
        QString mode = "ask";
        for (const auto &src : sources) {
            if (flag->checkpoint()) return "Operation cancelled. Completed files were retained.";
            const auto source = FileInstall::capture(src, true);
            const QString dest = base + '/' + (folder ? QFileInfo(src).fileName() : QFileInfo(destination).fileName());
            if (source.exists && S_ISDIR(source.stamp.st_mode) && (dest == source.path || dest.startsWith(source.path + '/'))) return "Cannot copy or move a folder into itself: " + src;
            const auto error = copyItem(source, FileInstall::capture(dest), mode, move, overwriteBroker, flag, [this](QString path) { emit progress(path); });
            if (!error.isEmpty()) return error;
        }
        return {};
    }));
}
void FileOperations::trash(QStringList sources) {
    if (busy()) return;
    QList<FileSnapshot> snapshots;
    for (const auto &source : sources) snapshots.append(FileInstall::capture(source, true));
    trashSnapshots(std::move(snapshots));
}
void FileOperations::trashSnapshots(QList<FileSnapshot> sources) {
    if (busy()) return;
    stop = std::make_shared<OperationControl>(); auto flag = stop;
    active = true; watcher.setFuture(QtConcurrent::run([this, sources, flag]() -> QString {
        for (const auto &src : sources) {
            if (flag->checkpoint()) return "Operation cancelled.";
            if (!src.error.isEmpty()) return src.error;
            if (!src.exists || !FileInstall::unchanged(src)) return "Source changed before moving to Trash: " + src.path;
            QString trashPath;
            if (!QFile::moveToTrash(src.path, &trashPath)) return "Cannot move to Trash: " + src.path;
            emit trashed(src.path, trashPath);
        }
        return {};
    }));
}

void FileOperations::rename(QString source, QString destination) {
    if (busy()) return;
    stop = std::make_shared<OperationControl>(); auto control = stop;
    active = true; watcher.setFuture(QtConcurrent::run([this, source, destination, control] {
        emit progress(source);
        auto original = FileInstall::capture(source, true);
        if (!original.error.isEmpty()) return original.error;
        const auto target = FileInstall::capture(destination, true);
        return FileInstall::rename(original, target, control).error;
    }));
}

void FileOperations::create(QString folder, QString name, bool directory) {
    if (busy()) return;
    stop = std::make_shared<OperationControl>(); auto control = stop;
    active = true; watcher.setFuture(QtConcurrent::run([this, folder, name, directory, control]() -> QString {
        const auto path = creationPath(folder, name);
        if (path.isEmpty() || (!directory && name.endsWith('/'))) return "Invalid creation path: " + name;
        emit progress(path);
        if (control->checkpoint()) return "Operation cancelled.";
        const auto inspected = FileInstall::inspect(path);
        if (!inspected.error.isEmpty()) return inspected.error;
        if (inspected.exists) return "Destination already exists: " + path;
        // CFSFolder::CreateFolder falls back to CreateComplexDir. CreateFile
        // uses Create_NEW and does not implicitly create missing parents.
        if (directory) { const auto error = FileInstall::ensureDirectory(QFileInfo(path).absolutePath()); if (!error.isEmpty()) return error; }
        const auto target = FileInstall::capture(path);
        if (!target.error.isEmpty()) return target.error;
        if (!FileInstall::unchanged(target) || target.exists) return "Destination changed before creation: " + path;
        if (control->checkpoint()) return "Operation cancelled. Created parent folders were retained.";
        const int result = directory ? ::mkdirat(target.parent->descriptor, target.name.constData(), 0777) : ::openat(target.parent->descriptor, target.name.constData(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0666);
        if (result < 0) return "Cannot create " + path + ": " + QString::fromLocal8Bit(std::strerror(errno));
        if (!directory) ::close(result);
        return {};
    }));
}
