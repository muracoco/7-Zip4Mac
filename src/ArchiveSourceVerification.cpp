// SPDX-License-Identifier: LGPL-3.0-or-later
// Verify source payloads before the macOS Trash substitute for DeleteAfter.
#include "ArchiveBackend.h"
#include "WorkerProcess.h"
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QtConcurrent>
#include <fcntl.h>
#include <unistd.h>
#include <zlib.h>

void SevenZipProcessBackend::verifySources(QList<ArchiveEntry> archiveItems) {
        const auto r = request; auto stop = stopFlag; const auto verificationProgram = program; const auto stamps = sourceAccessTimes;
        mergeWatcher.setFuture(QtConcurrent::run([this, r, archiveItems, stop, verificationProgram, stamps]() -> QString {
            auto progress = [this, stop, password = r.password](ArchiveProgress snapshot) {
                if (!password.isEmpty()) snapshot.current.replace(password, "[redacted]");
                QMetaObject::invokeMethod(this, [this, stop, snapshot] {
                    if (!active || stopFlag != stop || !checkingSources || cancelled) return;
                    emit this->progress(snapshot.percent(), snapshot.processed().value_or(0), snapshot.total.value_or(0), snapshot.current);
                    emit progressDetails(snapshot);
                }, Qt::QueuedConnection);
            };
            auto arguments = [&r](QString command, QStringList extra = {}) {
                QStringList args{command, "-sccUTF-8", "-scsUTF-8", "-spd", "-bso0", "-bse2"};
                if (r.memoryLimitGB > 0) args << "-smemx" + QString::number(r.memoryLimitGB) + "g";
                args << extra << "--" << (r.volume.isEmpty() ? r.archive : r.archive + ".001"); return args;
            };
            ArchiveProgress phase; phase.mode = "test"; phase.current = "Verifying archive: " + r.archive; progress(phase);
            const auto tested = WorkerProcess::run(verificationProgram, arguments("t"), r.password, stop, progress);
            if (!tested.success) return "Archive verification: " + r.archive + '\n' + tested.failure + '\n' + tested.errors;
            QMap<QString, ArchiveEntry> archived; for (const auto &entry : archiveItems) archived.insert(entry.path, entry);
            auto archiveName = [&r](QString path) {
                if (r.useArchiveDefaults) {
                    // Absolute CopyFrom inputs discard physical parent prefixes.
                    // Match descendants below each selected root to its basename.
                    for (const auto &name : r.files) {
                        const auto root = QDir::cleanPath(QDir(r.workingDirectory).absoluteFilePath(name));
                        if (path == root || path.startsWith(root + '/'))
                            return r.archivePrefix + (QFileInfo(name).isAbsolute() ? QFileInfo(name).fileName() : QDir::cleanPath(name)) + path.mid(root.size());
                    }
                    return QString();
                }
                if (r.pathMode != "relative") { QFileInfo info(path); path = QFileInfo(info.absolutePath()).canonicalFilePath() + '/' + info.fileName(); }
                if (r.pathMode == "absolute") return path; if (r.pathMode == "full") return path.startsWith('/') ? path.mid(1) : path;
                return QDir(r.workingDirectory).relativeFilePath(path);
            };
            QStringList sources;
            QList<QStringList> sourceTrees;
            QMap<QString, struct stat> links;
            for (const auto &name : r.files) {
                const QString path = QDir::cleanPath(QDir(r.workingDirectory).absoluteFilePath(name));
                sources << path;
                QStringList candidates{path};
                if (QFileInfo(path).isDir() && (!QFileInfo(path).isSymLink() || r.useArchiveDefaults)) {
                    QDirIterator::IteratorFlags flags = QDirIterator::Subdirectories;
                    if (r.useArchiveDefaults) flags |= QDirIterator::FollowSymlinks;
                    QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, flags);
                    while (it.hasNext()) {
                        if (stop->checkpoint()) return "Verification cancelled; sources retained.";
                        candidates << it.next();
                    }
                }
                for (const auto &candidate : candidates) if (QFileInfo(candidate).isSymLink()) {
                    struct stat identity{};
                    if (::lstat(QFile::encodeName(candidate).constData(), &identity) != 0) return "Cannot inspect source link; sources retained: " + candidate;
                    links.insert(candidate, identity);
                }
                sourceTrees << candidates;
            }
            phase = {}; phase.mode = "hash"; phase.doneFiles = 0; phase.completed = 0;
            quint64 verifiedBytes = 0, verifiedFiles = 0;
            QElapsedTimer notification; notification.start();
            auto updateBytes = [&](qint64 size) { verifiedBytes += quint64(size); phase.completed = verifiedBytes; if (notification.elapsed() >= 200) { progress(phase); notification.restart(); } };
            phase.total = 0; phase.files = 0;
            for (const auto &candidates : sourceTrees) {
                for (const auto &candidate : candidates) { const QFileInfo info(candidate); if (info.isFile() && (!info.isSymLink() || r.useArchiveDefaults)) { *phase.total += quint64(info.size()); ++*phase.files; } }
            }
            for (const auto &candidates : sourceTrees) {
                for (const auto &source : candidates) {
                    if (stop->checkpoint()) return "Cancelled before moving source files to Trash. The archive was created.";
                    auto entry = archived.value(archiveName(source)); if (QStringList{"xz", "gzip", "bzip2"}.contains(r.format) && archiveItems.size() == 1) entry = archiveItems.first(); QFileInfo info(source);
                    if ((info.isSymLink() && !(r.useArchiveDefaults && (info.isFile() || info.isDir()))) || entry.path.isEmpty() || entry.link || (info.isDir() != entry.directory)) return "The archive was created, but source data could not be verified; sources were retained: " + source;
                    if (info.isDir()) continue;
                    phase.current = "Verifying source: " + source; phase.completed = verifiedBytes; phase.doneFiles = verifiedFiles; progress(phase);
                    // BZIP2 has no stored unpacked size. An absent technical
                    // Size is unknown, not a zero-byte file; extract/hash below
                    // verifies its actual bytes before moving the source.
                    bool sizeDefined = false; entry.properties.value("Size").toULongLong(&sizeDefined);
                    QFile file(source); if ((sizeDefined && quint64(info.size()) != entry.size) || !file.open(QIODevice::ReadOnly)) return "The archive was created, but source data could not be verified; sources were retained: " + source;
                    if (entry.crc.isEmpty()) {
                        for (const auto &item : archiveItems) if (!SevenZipProcessBackend::safeArchivePath(item.path) || item.link) return "The archive was created, but verification refused an unsafe path or link; sources were retained.";
                        QTemporaryDir verify(QDir::tempPath() + "/7zip-verify-XXXXXX"); if (!verify.isValid()) return "Cannot create source verification directory; sources were retained.";
                        const auto extracted = WorkerProcess::run(verificationProgram, arguments("x", {"-o" + verify.path(), "-aoa"}) + QStringList{entry.path}, r.password, stop, progress);
                        if (!extracted.success) return "Source verification: " + source + '\n' + extracted.failure + '\n' + extracted.errors;
                        QFile restored(verify.filePath(entry.path)); if (!restored.open(QIODevice::ReadOnly)) return "Cannot read archived data for verification; sources were retained.";
                        auto hash = [&](QFile &input, bool source) { QCryptographicHash h(QCryptographicHash::Sha256); while (!input.atEnd()) { if (stop->checkpoint()) return QByteArray(); auto block = input.read(1024 * 1024); if (block.isEmpty() && input.error() != QFileDevice::NoError) return QByteArray(); h.addData(block); if (source) updateBytes(block.size()); } return h.result(); };
                        const auto originalHash = hash(file, true), storedHash = hash(restored, false); file.close(); if (originalHash.isEmpty() || storedHash.isEmpty()) return "Source verification was cancelled or failed; sources were retained.";
                        if (originalHash != storedHash) return "Source content changed; sources were retained.";
                        phase.doneFiles = ++verifiedFiles; progress(phase);
                        continue;
                    }
                    struct stat stamp; const auto bytes = QFile::encodeName(source); const bool restore = r.preserveAccessTime && ::stat(bytes.constData(), &stamp) == 0;
                    uLong crc = ::crc32(0, nullptr, 0); while (!file.atEnd()) { if (stop->checkpoint()) return "Cancelled before source deletion. The archive was created."; auto data = file.read(1024 * 1024); if (data.isEmpty() && file.error() != QFileDevice::NoError) return "Cannot verify source file: " + source; crc = ::crc32(crc, reinterpret_cast<const Bytef *>(data.constData()), uInt(data.size())); updateBytes(data.size()); } file.close();
                    if (restore) { const timespec times[2] = {stamp.st_atimespec, {0, UTIME_OMIT}}; if (::utimensat(AT_FDCWD, bytes.constData(), times, AT_SYMLINK_NOFOLLOW) != 0) return "Cannot preserve source access time: " + source; }
                    if (QString::number(crc, 16).rightJustified(8, '0').compare(entry.crc, Qt::CaseInsensitive) != 0) return "The archive was created, but source content changed; sources were retained: " + source;
                    phase.doneFiles = ++verifiedFiles; progress(phase);
                }
            }
            for (const auto &stamp : stamps) { const timespec times[2] = {{time_t(stamp.seconds), stamp.nanos}, {0, UTIME_OMIT}}; const auto bytes = QFile::encodeName(stamp.path); if (::utimensat(AT_FDCWD, bytes.constData(), times, AT_SYMLINK_NOFOLLOW) != 0) return "Cannot preserve source access time; sources were retained: " + stamp.path; }
            // CopyFrom follows directory and file links; Trash receives only
            // selected roots. Recheck link identity before source deletion so
            // a replaced/repointed link is never treated as verified input.
            for (auto link = links.cbegin(); link != links.cend(); ++link) {
                struct stat current{}; const auto &previous = link.value();
                if (::lstat(QFile::encodeName(link.key()).constData(), &current) != 0 || current.st_dev != previous.st_dev || current.st_ino != previous.st_ino ||
                    current.st_mode != previous.st_mode || current.st_size != previous.st_size || current.st_mtimespec.tv_sec != previous.st_mtimespec.tv_sec || current.st_mtimespec.tv_nsec != previous.st_mtimespec.tv_nsec)
                    return "Source link changed; sources were retained: " + link.key();
            }
            for (const auto &source : sources) { if (stop->checkpoint()) return "Cancelled while moving source files to Trash. Some files may already be in Trash; the archive was created."; if (!QFile::moveToTrash(source)) return "The archive was created, but moving source data to Trash failed: " + source; }
            return {};
        }));
}
