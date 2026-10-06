// SPDX-License-Identifier: LGPL-3.0-or-later
// Guarded host publication; original 7-Zip owns decoding and archive policy.
#include "ExtractionInstaller.h"
#include "ArchiveBackend.h"
#include "ArchiveSourceStamp.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <copyfile.h>
#include <fcntl.h>
#include <sys/attr.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <algorithm>

namespace {
QString metadataError(const QString &operation, const QString &path) {
    return operation + ": " + path + '\n' + QString::fromLocal8Bit(std::strerror(errno));
}
bool sameIdentity(const struct stat &a, const struct stat &b) { return a.st_dev == b.st_dev && a.st_ino == b.st_ino; }
QString outputKey(const FileSnapshot &file) {
    const auto key = file.path.normalized(QString::NormalizationForm_C);
    return file.parent && ::fpathconf(file.parent->descriptor, _PC_CASE_SENSITIVE) == 0 ? key.toCaseFolded() : key;
}
QString setCreation(const FileSnapshot &file, const std::optional<timespec> &created) {
    if (!created) return {};
    if (!FileInstall::unchanged(file)) return "Output changed before setting creation time: " + file.path;
    const int fd = ::openat(file.parent->descriptor, file.name.constData(), O_EVTONLY | O_CLOEXEC | (S_ISLNK(file.stamp.st_mode) ? O_SYMLINK : O_NOFOLLOW));
    if (fd < 0) return metadataError("Cannot open creation-time output", file.path);
    const auto close = qScopeGuard([&] { ::close(fd); }); struct stat now{};
    if (::fstat(fd, &now) != 0 || !sameIdentity(file.stamp, now)) return "Output changed before setting creation time: " + file.path;
    struct attrlist attributes{}; attributes.bitmapcount = ATTR_BIT_MAP_COUNT; attributes.commonattr = ATTR_CMN_CRTIME;
    auto value = *created;
    if (::fsetattrlist(fd, &attributes, &value, sizeof(value), 0) != 0) return metadataError("Cannot restore creation time", file.path);
    return {};
}
QString preparedLink(const FileSnapshot &source, const QByteArray &target, const QString &path) {
    if (!FileInstall::unchanged(source)) return "Extracted symbolic link changed: " + source.path;
    const int input = ::openat(source.parent->descriptor, source.name.constData(), O_RDONLY | O_SYMLINK | O_CLOEXEC);
    if (input < 0) return metadataError("Cannot open extracted symbolic link", source.path);
    const auto closeInput = qScopeGuard([&] { ::close(input); });
    if (::symlink(target.constData(), QFile::encodeName(path).constData()) != 0) return metadataError("Cannot prepare relocated symbolic link", path);
    const int output = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_SYMLINK | O_CLOEXEC);
    if (output < 0) return metadataError("Cannot open relocated symbolic link", path);
    const auto closeOutput = qScopeGuard([&] { ::close(output); });
    if (::fcopyfile(input, output, nullptr, COPYFILE_METADATA) != 0) return metadataError("Cannot retain symbolic-link metadata", path);
    return FileInstall::unchanged(source) ? QString() : "Extracted symbolic link changed: " + source.path;
}
struct ExtractDirectory {
    std::shared_ptr<FileDirectory> source, target;
    ExtractMetadata metadata;
    bool created = false;
};
QString restoreDirectory(const ExtractDirectory &directory) {
    struct stat source{}, target{};
    const auto namedTarget = FileInstall::capture(directory.target->path);
    if (!namedTarget.error.isEmpty() || !namedTarget.exists || !sameIdentity(namedTarget.stamp, directory.target->stamp)) return "Extraction directory path changed during installation: " + directory.target->path;
    if (::fstat(directory.source->descriptor, &source) != 0 || ::fstat(directory.target->descriptor, &target) != 0 ||
        !sameIdentity(source, directory.source->stamp) || !sameIdentity(target, directory.target->stamp)) return "Extraction directory changed during installation.";
    const auto &stamp = directory.source->stamp;
    if (directory.created && ::fcopyfile(directory.source->descriptor, directory.target->descriptor, nullptr, COPYFILE_ACL | COPYFILE_XATTR) != 0) return metadataError("Cannot retain folder metadata", directory.target->path);
    const timespec times[2]{directory.created || directory.metadata.accessed ? stamp.st_atimespec : timespec{0, UTIME_OMIT},
                            directory.created || directory.metadata.modified ? stamp.st_mtimespec : timespec{0, UTIME_OMIT}};
    if (::futimens(directory.target->descriptor, times) != 0) return metadataError("Cannot restore folder times", directory.target->path);
    if ((directory.created || directory.metadata.attributes) && ::fchmod(directory.target->descriptor, stamp.st_mode & 07777) != 0) return metadataError("Cannot restore folder permissions", directory.target->path);
    if (directory.created || directory.metadata.created) {
        struct attrlist attributes{}; attributes.bitmapcount = ATTR_BIT_MAP_COUNT; attributes.commonattr = ATTR_CMN_CRTIME;
        auto birth = directory.metadata.created.value_or(stamp.st_birthtimespec);
        if (::fsetattrlist(directory.target->descriptor, &attributes, &birth, sizeof(birth), 0) != 0) return metadataError("Cannot restore folder creation time", directory.target->path);
    }
    return {};
}
}

ExtractMetadata extractionMetadataFor(const ArchiveEntry &entry) {
    ExtractMetadata result;
    result.accessed = !entry.properties.value("Accessed").isEmpty(); result.modified = !entry.modified.isEmpty();
    result.attributes = !entry.attributes.isEmpty() || !entry.properties.value("Mode").isEmpty();
    result.hardLink = entry.properties.value("Hard Link");
    for (const auto &property : entry.orderedProperties) if (property.name == "Created" && property.type == 64) {
        bool ok; const auto ticks = property.fileTime.toULongLong(&ok);
        const auto &precision = property.timePrecision;
        const long extra = precision.size() == 3 && precision[0] == 0 && precision[1] < 100 && precision[2] == 0 ? precision[1] : 0;
        // A zero ZIP NTFS timestamp is displayed as blank by the original
        // console. It must not become an out-of-range APFS creation time.
        if (ok && ticks != 0) result.created = timespec{time_t(ticks / 10000000ULL) - 11644473600LL, long(ticks % 10000000ULL) * 100 + extra};
    }
    return result;
}
QString extractionOutputTarget(const QString &root, const QString &relative, bool absolute, const QMap<QString, QString> &targets) {
    if (!absolute) return relative.isEmpty() ? root : QDir(root).filePath(relative);
    if (targets.contains(relative)) return targets.value(relative);
    QString parent = relative;
    for (;;) {
        const auto slash = parent.lastIndexOf('/'); if (slash < 0) return {};
        parent.truncate(slash);
        if (targets.contains(parent)) return QDir(targets.value(parent)).filePath(relative.mid(parent.size() + 1));
    }
}

struct ExtractionInstaller::Data {
    QString stage, root, removedRoot, mode;
    bool absolute;
    QMap<QString, QString> targets, publicSnapshots;
    OverwriteBroker &broker;
    std::shared_ptr<OperationControl> stop;
    QList<ExtractDirectory> directories;
    struct GroupOutput { QString path; struct stat stamp; };
    QMap<QString, GroupOutput> groups;
    struct CreationOutput { FileSnapshot file; timespec created; };
    QMap<QString, CreationOutput> creationOutputs;
    QMap<QString, QStringList> installedPaths;
    QMap<QString, QString> pathOwners;
    bool finished = false;
    QString result;
    Data(QString stage, QString root, bool absolute, QMap<QString, QString> targets, QString removedRoot,
         QString mode, OverwriteBroker &broker, std::shared_ptr<OperationControl> stop, QMap<QString, QString> references)
        : stage(std::move(stage)), root(std::move(root)), removedRoot(std::move(removedRoot)), mode(std::move(mode)),
          absolute(absolute), targets(std::move(targets)), publicSnapshots(std::move(references)), broker(broker), stop(std::move(stop)) {}
    QString destination(const QString &relative) const { return extractionOutputTarget(root, relative, absolute, targets); }
    QString install(const QString &rel, const FileSnapshot &completed, const ExtractMetadata &info,
                    const QString &group, const std::optional<FileSnapshot> &expected, const QString &approvedMode) {
        if (finished) return "Extraction session is already finalized.";
        if (!SevenZipProcessBackend::safeArchivePath(rel) || destination(rel).isEmpty()) return "Unsafe extraction output: " + rel;
        if (stop->checkpoint()) return "Cancelled while installing extracted files. Completed files were retained.";
        if (!FileInstall::unchanged(completed)) return "Completed extraction output changed: " + rel;
        const auto dest = destination(rel);
        auto error = FileInstall::ensureDirectory(QFileInfo(dest).absolutePath()); if (!error.isEmpty()) return error;
        auto source = FileInstall::capture(completed.path, true); if (!source.error.isEmpty()) return source.error;
        const auto stageStamp = completed.stamp;
        bool hard = !info.hardLink.isEmpty();
        if (hard) {
            const auto target = info.hardLinkSource;
            if (target.isEmpty()) return "Hard link target is not available in this extraction: " + rel;
            source = FileInstall::capture(target);
            if (!source.error.isEmpty()) return source.error;
            if (!source.exists || !S_ISREG(source.stamp.st_mode)) return "Hard link target is not a regular file: " + target;
            if (publicSnapshots.contains(target) && QString::fromStdString(archiveSourceStamp(source.stamp)) != publicSnapshots.value(target)) return "Existing hard-link target changed during extraction: " + target;
        } else if (S_ISREG(stageStamp.st_mode) && groups.contains(group)) {
            const auto reference = groups.value(group); source = FileInstall::capture(reference.path); hard = true;
            if (!source.exists || !sameIdentity(source.stamp, reference.stamp) || source.stamp.st_size != reference.stamp.st_size ||
                source.stamp.st_mtimespec.tv_sec != reference.stamp.st_mtimespec.tv_sec || source.stamp.st_mtimespec.tv_nsec != reference.stamp.st_mtimespec.tv_nsec)
                return "Extracted hard-link target changed: " + reference.path;
        }
        std::unique_ptr<QTemporaryDir> rebased;
        if (S_ISLNK(source.stamp.st_mode)) {
            const auto raw = QFile::decodeName(source.linkTarget);
            if (raw.startsWith(stage + '/')) {
                auto targetRel = raw.mid(stage.size() + 1);
                if (!removedRoot.isEmpty() && targetRel.startsWith(removedRoot + '/')) targetRel.remove(0, removedRoot.size() + 1);
                auto target = destination(targetRel); if (target.isEmpty() && absolute) target = '/' + targetRel;
                if (target.isEmpty() || !SevenZipProcessBackend::safeArchivePath(targetRel)) return "Unsafe relocated symbolic-link target: " + rel;
                rebased = std::make_unique<QTemporaryDir>(stage + "/.7zip-link-XXXXXX");
                if (!rebased->isValid()) return "Cannot prepare relocated symbolic link.";
                error = preparedLink(source, QFile::encodeName(target), rebased->filePath("link")); if (!error.isEmpty()) return error;
                source = FileInstall::capture(rebased->filePath("link"), true);
            }
        }
        const auto previous = expected ? *expected : FileInstall::capture(dest);
        if (!previous.error.isEmpty()) return previous.error;
        // macOS QFile encodes names in decomposed form. Compare the actual
        // filesystem spelling, rather than rejecting equivalent Unicode text.
        if (expected && QFile::encodeName(previous.path) != QFile::encodeName(QFileInfo(dest).absoluteFilePath()))
            return "Approved extraction destination does not match: " + rel;
        auto policy = approvedMode.isEmpty() ? mode : approvedMode;
        auto installed = FileInstall::transfer(source, previous, policy, broker, stop, completed.info(rel), true, {}, !hard, hard);
        if (approvedMode.isEmpty()) mode = policy;
        if (!installed.error.isEmpty()) return installed.error;
        error = FileInstall::useRequestedName(installed); if (!error.isEmpty()) return error;
        if (installed.preservedFile.exists) {
            const auto oldIdentity = QString::number(quint64(previous.stamp.st_dev)) + ':' + QString::number(quint64(previous.stamp.st_ino));
            if (creationOutputs.contains(oldIdentity)) {
                const auto created = creationOutputs.value(oldIdentity).created;
                error = setCreation(installed.preservedFile, created); if (!error.isEmpty()) return error;
                const auto retained = FileInstall::capture(installed.preservedFile.path); if (!retained.error.isEmpty()) return retained.error;
                const auto identity = QString::number(quint64(retained.stamp.st_dev)) + ':' + QString::number(quint64(retained.stamp.st_ino));
                creationOutputs[identity] = {retained, created}; installedPaths[identity] << retained.path; pathOwners[outputKey(retained)] = identity;
            }
        }
        if (installed.installed && (hard || S_ISLNK(stageStamp.st_mode))) {
            const auto output = FileInstall::capture(installed.target);
            const int fd = ::openat(output.parent->descriptor, output.name.constData(), O_EVTONLY | O_CLOEXEC | (S_ISLNK(output.stamp.st_mode) ? O_SYMLINK : O_NOFOLLOW));
            if (fd < 0) return metadataError("Cannot open extracted link metadata", output.path);
            const auto close = qScopeGuard([&] { ::close(fd); });
            const timespec times[2]{info.accessed ? stageStamp.st_atimespec : timespec{0, UTIME_OMIT}, info.modified ? stageStamp.st_mtimespec : timespec{0, UTIME_OMIT}};
            if (::futimens(fd, times) != 0) return metadataError("Cannot restore extracted link times", output.path);
            if (hard && info.hardLink.isEmpty() && info.attributes && ::fchmod(fd, stageStamp.st_mode & 07777) != 0) return metadataError("Cannot restore extracted hard-link permissions", output.path);
        }
        if (installed.installed && hard && publicSnapshots.contains(source.path)) {
            const auto updated = FileInstall::capture(source.path);
            if (!updated.error.isEmpty() || !updated.exists || !sameIdentity(source.stamp, updated.stamp) || source.stamp.st_size != updated.stamp.st_size) return "Existing hard-link target changed during installation: " + source.path;
            // linkat and the original alias metadata legitimately change
            // this inode's ctime. Retain the new token for further aliases.
            publicSnapshots[source.path] = QString::fromStdString(archiveSourceStamp(updated.stamp));
        }
        if (installed.installed) {
            const auto output = FileInstall::capture(installed.target);
            if (!output.error.isEmpty()) return output.error;
            const auto identity = QString::number(quint64(output.stamp.st_dev)) + ':' + QString::number(quint64(output.stamp.st_ino));
            pathOwners[outputKey(output)] = identity; installedPaths[identity] << installed.target;
            if (info.created || creationOutputs.contains(identity)) {
                auto &pending = creationOutputs[identity]; pending.file = output;
                if (info.created) pending.created = *info.created;
            }
        }
        if (installed.installed && S_ISREG(stageStamp.st_mode))
            groups[group] = {installed.target, FileInstall::capture(installed.target).stamp};
        return {};
    }
    QString restoreCreation() {
        // macOS can lower birth time when a hard-link alias restores an older
        // modification time. Restore creation time only after every alias.
        for (auto iter = creationOutputs.cbegin(); iter != creationOutputs.cend(); ++iter) {
            const auto &pending = iter.value(); FileSnapshot survivor;
            for (auto path = installedPaths[iter.key()].crbegin(); path != installedPaths[iter.key()].crend(); ++path) {
                auto current = FileInstall::capture(*path); if (!current.error.isEmpty()) return current.error;
                if (pathOwners.value(outputKey(current)) != iter.key()) continue; // Our later replacement superseded this inode.
                if (!current.exists || !sameIdentity(current.stamp, pending.file.stamp)) return "Output changed before setting creation time: " + *path;
                survivor = current; break;
            }
            if (survivor.exists) { const auto error = setCreation(survivor, pending.created); if (!error.isEmpty()) return error; }
        }
        return {};
    }
};

ExtractionInstaller::ExtractionInstaller(QString stage, QString root, bool absolute, QMap<QString, QString> targets,
                                         QString removedRoot, QString mode, OverwriteBroker &broker,
                                         std::shared_ptr<OperationControl> control, QMap<QString, QString> references)
    : data(std::make_unique<Data>(std::move(stage), std::move(root), absolute, std::move(targets), std::move(removedRoot),
                                  std::move(mode), broker, std::move(control), std::move(references))) {}
ExtractionInstaller::~ExtractionInstaller() = default;
QString ExtractionInstaller::prepareDirectory(QString relative, const FileSnapshot &snapshot, ExtractMetadata metadata, bool createdHere) {
    auto &state = *data;
    if (state.finished) return "Extraction session is already finalized.";
    if ((!relative.isEmpty() && !SevenZipProcessBackend::safeArchivePath(relative)) || state.destination(relative).isEmpty()) return "Unsafe extraction directory: " + relative;
    if (state.stop->checkpoint()) return "Cancelled while installing extracted files. Completed files were retained.";
    QString error; auto source = FileInstall::openFolder(snapshot, &error); if (!source) return error;
    source->stamp.st_atimespec = snapshot.stamp.st_atimespec;
    auto target = FileInstall::capture(state.destination(relative)); if (!target.error.isEmpty()) return target.error;
    const bool created = createdHere || !target.exists;
    error = FileInstall::ensureDirectory(target.path); if (!error.isEmpty()) return error;
    auto folder = FileInstall::openFolder(FileInstall::capture(target.path), &error); if (!folder) return error;
    state.directories.append({source, folder, std::move(metadata), created});
    return {};
}
QString ExtractionInstaller::install(QString relative, const FileSnapshot &completed, ExtractMetadata metadata, QString group,
                                    std::optional<FileSnapshot> expected, QString approvedMode) {
    return data->install(relative, completed, metadata, group, expected, approvedMode);
}
QString ExtractionInstaller::finish(QString error) {
    auto &state = *data;
    if (state.finished) return state.result;
    auto append = [&](const QString &value) { if (!value.isEmpty()) { if (!error.isEmpty()) error += '\n'; error += value; } };
    append(state.restoreCreation());
    std::stable_sort(state.directories.begin(), state.directories.end(), [](const ExtractDirectory &a, const ExtractDirectory &b) { return a.target->path.size() < b.target->path.size(); });
    for (auto directory = state.directories.crbegin(); directory != state.directories.crend(); ++directory) append(restoreDirectory(*directory));
    state.finished = true; state.result = error; return error;
}
QString ExtractionInstaller::relocated(const QString &from, const QString &to) {
    auto &state = *data;
    if (state.finished) return "Extraction session is already finalized.";
    const auto moved = FileInstall::capture(to);
    if (!moved.error.isEmpty() || !moved.exists) return "Moved extraction output is unavailable: " + to;
    const bool insensitive = ::fpathconf(moved.parent->descriptor, _PC_CASE_SENSITIVE) == 0;
    auto key = [&](const QString &path) { const auto normalized = path.normalized(QString::NormalizationForm_C); return insensitive ? normalized.toCaseFolded() : normalized; };
    auto mapped = [&](const QString &path) {
        if (key(path) == key(from)) return to;
        return path.startsWith(from + '/') ? to + path.mid(from.size()) : path;
    };
    for (auto group = state.groups.begin(); group != state.groups.end(); ++group) {
        const auto path = mapped(group->path); if (path == group->path) continue;
        const auto current = FileInstall::capture(path);
        if (!current.error.isEmpty() || !current.exists || !sameIdentity(current.stamp, group->stamp)) return "Moved extraction output changed: " + path;
        group->path = path; group->stamp = current.stamp;
    }
    for (auto owner = state.installedPaths.begin(); owner != state.installedPaths.end(); ++owner) {
        for (auto &path : owner.value()) {
            const auto next = mapped(path); if (next == path) continue;
            const auto current = FileInstall::capture(next); if (!current.error.isEmpty()) return current.error;
            const auto identity = QString::number(quint64(current.stamp.st_dev)) + ':' + QString::number(quint64(current.stamp.st_ino));
            if (!current.exists || identity != owner.key()) continue;
            path = next; state.pathOwners[outputKey(current)] = owner.key();
        }
    }
    for (auto &directory : state.directories) directory.target->path = mapped(directory.target->path);
    return {};
}
QString ExtractionInstaller::reference(const QString &path, const QString &fingerprint) {
    auto &state = *data;
    if (state.finished) return "Extraction session is already finalized.";
    const auto file = FileInstall::capture(path);
    if (!file.error.isEmpty() || !file.exists || !S_ISREG(file.stamp.st_mode) || QString::fromStdString(archiveSourceStamp(file.stamp)) != fingerprint)
        return "Existing hard-reference source changed: " + path;
    if (state.publicSnapshots.contains(path) && state.publicSnapshots.value(path) != fingerprint) return "Existing hard-reference source changed during extraction: " + path;
    state.publicSnapshots[path] = fingerprint; return {};
}
