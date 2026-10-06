// SPDX-License-Identifier: LGPL-3.0-or-later
// LinkDialog.cpp semantics with POSIX symlink snapshots and guarded exchange.
#include "FileOperations.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QStringDecoder>
#include <QUuid>
#include <limits.h>
#include <cerrno>
#include <cstring>
#include <copyfile.h>
#include <fcntl.h>
#include <unistd.h>

namespace {
QString ioError(QString operation, QString path) {
    const int code = errno;
    return operation + ": " + path + "\n" + QString::fromLocal8Bit(std::strerror(code)) + " (" + QString::number(code) + ')';
}
bool identity(const struct stat &a, const struct stat &b) { return a.st_dev == b.st_dev && a.st_ino == b.st_ino; }
bool same(const struct stat &a, const struct stat &b, bool renamed = false) {
    return identity(a, b) && S_ISLNK(b.st_mode) && a.st_size == b.st_size && a.st_mode == b.st_mode && a.st_nlink == b.st_nlink &&
        a.st_uid == b.st_uid && a.st_gid == b.st_gid && a.st_flags == b.st_flags &&
        a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec == b.st_mtimespec.tv_nsec &&
        (renamed || (a.st_ctimespec.tv_sec == b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec == b.st_ctimespec.tv_nsec));
}
bool rawTarget(int directory, const QByteArray &name, QByteArray &bytes) {
    QByteArray buffer(4096, Qt::Uninitialized); const auto count = ::readlinkat(directory, name.constData(), buffer.data(), buffer.size());
    if (count < 0) return false;
    if (count == buffer.size()) { errno = ENAMETOOLONG; return false; }
    buffer.resize(count); bytes = buffer; return true;
}
bool matches(int directory, const QByteArray &name, const SymbolicLinkSnapshot &original, bool renamed = false) {
    struct stat now{}; QByteArray target;
    return ::fstatat(directory, name.constData(), &now, AT_SYMLINK_NOFOLLOW) == 0 && same(original.link, now, renamed) && rawTarget(directory, name, target) && target == original.target;
}
QString targetError(QString folder, QString target, LinkType type) {
    if (target.isEmpty() || target.contains(QChar::Null)) return "Specify a nonempty link target without NUL characters.";
    if (type != LinkType::Hard && type != LinkType::SymbolicFile && type != LinkType::SymbolicDirectory) return "Incorrect link type.";
    QFileInfo source(QDir(folder).absoluteFilePath(target));
    if (source.exists() && source.isDir() != (type == LinkType::SymbolicDirectory)) return "Incorrect link type: " + target;
    if (type == LinkType::Hard && (!source.isFile() || source.isSymLink())) return "Hard link target must be a regular file: " + target;
    return {};
}
}
QString FileOperations::createLink(QString from, QString target, LinkType type) {
    if (from.isEmpty() || from.contains(QChar::Null)) return "Specify a link path without NUL characters.";
    QFileInfo destination(from);
    if (destination.exists() || destination.isSymLink()) return "Destination already exists: " + from;
    if (!QFileInfo(destination.absolutePath()).isDir()) return "Destination folder does not exist: " + destination.absolutePath();
    const auto error = targetError(destination.absolutePath(), target, type); if (!error.isEmpty()) return error;
    // A relative symbolic target remains relative to the destination parent.
    const auto nativeFrom = QFile::encodeName(from), nativeTarget = QFile::encodeName(type == LinkType::Hard ? QDir(destination.absolutePath()).absoluteFilePath(target) : target);
    const int result = type == LinkType::Hard ? ::link(nativeTarget.constData(), nativeFrom.constData()) : ::symlink(nativeTarget.constData(), nativeFrom.constData());
    return result == 0 ? QString() : ioError("Cannot create link", from);
}
SymbolicLinkSnapshot FileOperations::inspectSymbolicLink(QString path) {
    SymbolicLinkSnapshot original;
    if (path.isEmpty() || path.contains(QChar::Null)) { original.error = "Invalid symbolic link path."; return original; }
    const QFileInfo file(path); original.path = file.absoluteFilePath(); original.folder = file.absolutePath(); original.name = QFile::encodeName(file.fileName());
    const int directory = ::open(QFile::encodeName(original.folder).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) { original.error = ioError("Cannot open link folder", original.folder); return original; }
    const auto close = qScopeGuard([&] { ::close(directory); });
    if (::fstat(directory, &original.directory) != 0 || ::fstatat(directory, original.name.constData(), &original.link, AT_SYMLINK_NOFOLLOW) != 0) { original.error = ioError("Cannot inspect link", original.path); return original; }
    original.symbolic = S_ISLNK(original.link.st_mode); if (!original.symbolic) return original;
    if (!rawTarget(directory, original.name, original.target)) { original.error = ioError("Cannot read symbolic link", original.path); return original; }
    QStringDecoder decoder(QStringDecoder::Utf8); const QString text = decoder(original.target);
    if (decoder.hasError() || text.contains(QChar::Null) || QFile::encodeName(text) != original.target) original.error = "Symbolic link target is not representable as UTF-8; original retained: " + original.path;
    else if (!matches(directory, original.name, original)) original.error = "Symbolic link changed while reading; original retained: " + original.path;
    return original;
}
QString FileOperations::editSymbolicLink(const SymbolicLinkSnapshot &original, QString target, LinkType type, const LinkEditObserver &observer) {
    if (!original.error.isEmpty() || !original.symbolic) return "Cannot edit unreadable or non-symbolic link; original retained: " + original.path;
    if (type == LinkType::Hard) return "An existing symbolic link cannot be retargeted as a hard link. Choose a different Link from path to create a new link.";
    const auto error = targetError(original.folder, target, type); if (!error.isEmpty()) return error;
    if (original.link.st_nlink != 1 || (original.link.st_flags & (UF_IMMUTABLE | SF_IMMUTABLE | UF_APPEND | SF_APPEND))) return "Symbolic link is hard-linked or immutable; original retained: " + original.path;
    const int directory = ::open(QFile::encodeName(original.folder).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) return ioError("Cannot open link folder", original.folder);
    const auto close = qScopeGuard([&] { ::close(directory); }); struct stat parent{};
    if (::fstat(directory, &parent) != 0 || !identity(original.directory, parent) || !matches(directory, original.name, original)) return "Symbolic link or folder changed; original retained: " + original.path;
    if (QFile::encodeName(target) == original.target) return {};
    const auto stageName = QFile::encodeName(".7zip-link-edit-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (::mkdirat(directory, stageName.constData(), 0700) != 0) return ioError("Cannot create link staging folder", original.folder);
    struct stat stageIdentity{};
    if (::fstatat(directory, stageName.constData(), &stageIdentity, AT_SYMLINK_NOFOLLOW) != 0) return ioError("Cannot inspect link staging folder", original.folder);
    const int staging = ::openat(directory, stageName.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (staging < 0) return ioError("Cannot open link staging folder", original.folder);
    const auto closeStaging = qScopeGuard([&] { ::close(staging); }); struct stat openedStage{};
    if (::fstat(staging, &openedStage) != 0 || !identity(stageIdentity, openedStage)) return "Link staging folder changed; original retained: " + original.path;
    bool preserve = false, haveDisposable = false; struct stat disposable{};
    const auto cleanup = qScopeGuard([&] {
        if (preserve) return;
        struct stat entry{}, stage{};
        if (haveDisposable && ::fstatat(staging, "replacement", &entry, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(entry.st_mode) && identity(entry, disposable)) ::unlinkat(staging, "replacement", 0);
        // Never recursively remove a path. Both handles anchor the owned
        // inode even after its parent is moved; unknown contents are retained.
        if (::fstatat(directory, stageName.constData(), &stage, AT_SYMLINK_NOFOLLOW) == 0 && identity(stage, openedStage)) ::unlinkat(directory, stageName.constData(), AT_REMOVEDIR);
    });
    auto stagePath = [&] {
        char current[PATH_MAX]{};
        return ::fcntl(staging, F_GETPATH, current) == 0 ? QFile::decodeName(current) : original.folder + '/' + QFile::decodeName(stageName);
    };
    const auto targetBytes = QFile::encodeName(target);
    if (::symlinkat(targetBytes.constData(), staging, "replacement") != 0) return ioError("Cannot prepare symbolic link", original.path);
    if (::fstatat(staging, "replacement", &disposable, AT_SYMLINK_NOFOLLOW) != 0) return ioError("Cannot inspect prepared symbolic link", original.path);
    haveDisposable = true;
    const int input = ::openat(directory, original.name.constData(), O_RDONLY | O_SYMLINK | O_CLOEXEC);
    if (input < 0) return ioError("Cannot read symbolic link metadata", original.path);
    const auto closeInput = qScopeGuard([&] { ::close(input); }); struct stat inputInfo{};
    if (::fstat(input, &inputInfo) != 0 || !same(original.link, inputInfo)) return "Symbolic link changed; original retained: " + original.path;
    const int output = ::openat(staging, "replacement", O_RDONLY | O_SYMLINK | O_CLOEXEC);
    if (output < 0) return ioError("Cannot open prepared symbolic link", original.path);
    const auto closeOutput = qScopeGuard([&] { ::close(output); });
    // O_SYMLINK descriptors refer to the symlinks themselves. Neither path
    // nor target data is used by fcopyfile; only metadata is copied.
    if (::fcopyfile(input, output, nullptr, COPYFILE_METADATA) != 0) return ioError("Cannot preserve symbolic link metadata", original.path);
    struct stat prepared{};
    if (::fstat(output, &prepared) != 0 || !S_ISLNK(prepared.st_mode) || prepared.st_uid != original.link.st_uid || prepared.st_gid != original.link.st_gid || (prepared.st_mode & 07777) != (original.link.st_mode & 07777)) return "Cannot preserve symbolic link ownership or mode; original retained: " + original.path;
    if (observer) observer(LinkEditPhase::Prepared, stagePath());
    QByteArray candidateTarget; struct stat pathParent{};
    auto parentUnchanged = [&] { return ::stat(QFile::encodeName(original.folder).constData(), &pathParent) == 0 && identity(parent, pathParent); };
    if (!rawTarget(staging, "replacement", candidateTarget) || candidateTarget != targetBytes || !matches(directory, original.name, original) || !parentUnchanged()) return "Symbolic link or folder changed; original retained: " + original.path;
    if (observer) observer(LinkEditPhase::Validated, stagePath());
    // Retain whatever the kernel actually exchanges until its identity has
    // been checked. RENAME_SWAP is atomic; a final-check race cannot cause an
    // unexpected ordinary file to be silently deleted with temporary data.
    preserve = true;
    if (::renameatx_np(staging, "replacement", directory, original.name.constData(), RENAME_SWAP) != 0) { preserve = false; return ioError("Cannot install symbolic link", original.path); }
    if (observer) observer(LinkEditPhase::Exchanged, stagePath());
    struct stat displaced{}; const bool haveDisplaced = ::fstatat(staging, "replacement", &displaced, AT_SYMLINK_NOFOLLOW) == 0;
    auto candidateInstalled = [&] {
        struct stat installed{}; QByteArray bytes;
        return ::fstatat(directory, original.name.constData(), &installed, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(installed.st_mode) && identity(installed, prepared) && rawTarget(directory, original.name, bytes) && bytes == targetBytes;
    };
    if (matches(staging, "replacement", original, true) && candidateInstalled() && parentUnchanged()) { disposable = original.link; preserve = false; return {}; }
    if (observer) observer(LinkEditPhase::Rollback, stagePath());
    struct stat stillDisplaced{};
    if (haveDisplaced && ::fstatat(staging, "replacement", &stillDisplaced, AT_SYMLINK_NOFOLLOW) == 0 && identity(displaced, stillDisplaced) && candidateInstalled() &&
        ::renameatx_np(staging, "replacement", directory, original.name.constData(), RENAME_SWAP) == 0) {
        struct stat restoredCandidate{};
        if (::fstatat(staging, "replacement", &restoredCandidate, AT_SYMLINK_NOFOLLOW) == 0 && S_ISLNK(restoredCandidate.st_mode) && identity(restoredCandidate, prepared)) { disposable = prepared; preserve = false; return "Link or folder changed during installation; previous entry restored: " + original.path; }
    }
    return "Link changed during installation. Recovery files retained in: " + stagePath() + "\nInspect both paths before making another change: " + original.path;
}
