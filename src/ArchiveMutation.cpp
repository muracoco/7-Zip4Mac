// SPDX-License-Identifier: LGPL-3.0-or-later
// Staged CreateFolder / UpdateOneFile / ZIP Comment retain unrelated entries.
// Archive writing uses official 7-Zip APIs; decoded verification uses 7zz.
#include "ArchiveBackend.h"
#include "ArchiveFormats.h"
#include "ArchiveSourceStamp.h"
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTimer>
#include <QRegularExpression>
#include <QtConcurrent>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <copyfile.h>

namespace {
QString ioError(const QString &action, const QString &path) {
    return action + ": " + path + '\n' + QString::fromLocal8Bit(std::strerror(errno));
}
bool sameFile(const struct stat &a, const struct stat &b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size && a.st_mode == b.st_mode && a.st_uid == b.st_uid && a.st_gid == b.st_gid &&
        a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec == b.st_mtimespec.tv_nsec &&
        a.st_ctimespec.tv_sec == b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec == b.st_ctimespec.tv_nsec;
}
QString entryPath(QString path) {
    path.replace('\\', '/'); while (path.endsWith('/')) path.chop(1); return path;
}
QString collisionKey(QString path) { return entryPath(path).normalized(QString::NormalizationForm_C).toCaseFolded(); }
bool sameItem(const ArchiveEntry &old, const ArchiveEntry &item) {
    if (item.path != old.path || item.directory != old.directory || item.link != old.link || item.size != old.size || item.crc != old.crc || item.encrypted != old.encrypted || item.modified != old.modified || item.attributes != old.attributes) return false;
    // Method, Packed Size, Block and physical Offset can change when solid data is
    // recompressed. These semantic properties must remain unchanged.
    for (const auto *name : {"Created", "Accessed", "Mode", "User", "Group", "User ID", "Group ID", "Symbolic Link", "Hard Link", "Comment", "Short Name", "Link", "SHA-1", "Alternate Streams", "NT Security"})
        if (item.properties.value(name) != old.properties.value(name)) return false;
    return true;
}
QByteArray semanticKey(const ArchiveEntry &item) {
    QByteArray key; QDataStream stream(&key, QIODevice::WriteOnly);
    auto modified = item.modified;
    static const QRegularExpression time("^(\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}:\\d{2})(?:\\.(\\d{1,9}))?$");
    const auto match = time.match(modified);
    if (match.hasMatch()) modified = match.captured(1) + '.' + match.captured(2).leftJustified(9, '0');
    // QString equality treats null and empty alike. Its stream encoding does
    // not, so normalize absent properties before building the multiset key.
    auto text = [](const QString &value) { return value.isNull() ? QStringLiteral("") : value; };
    stream << text(item.path) << item.directory << item.link << item.size << text(item.crc) << item.encrypted << text(modified) << text(item.attributes);
    for (const auto *name : {"Created", "Accessed", "Mode", "User", "Group", "User ID", "Group ID", "Symbolic Link", "Hard Link", "Comment", "Short Name", "Link", "SHA-1", "Alternate Streams", "NT Security"}) stream << text(item.properties.value(name));
    return key;
}
}

void SevenZipProcessBackend::beginArchiveMutation() {
    const bool replacing = request.operation == ArchiveOperation::ReplaceFile;
    const bool commenting = request.operation == ArchiveOperation::Comment;
    const bool itemUpdate = request.operation == ArchiveOperation::Delete || request.operation == ArchiveOperation::Rename || ((commenting || replacing) && request.selectionDirectory >= 0);
    const bool renaming = request.operation == ArchiveOperation::Rename;
    const auto fail = [this](QString error) { QTimer::singleShot(0, this, [this, error] { complete(false, -1, error); }); };
    if (itemUpdate) {
        if (request.selectionDirectory < 0 || request.selection.isEmpty() ||
            (renaming && (request.selection.size() != 1 || request.files.size() != 2 || !safeArchivePath(request.files[1]) || request.files[1].endsWith('/') || request.files[1].contains('\\'))) ||
            ((commenting || replacing) && (request.selection.size() != 1 || request.files.size() != 1 || !safeArchivePath(request.files[0])))) { fail("Invalid archive update selection."); return; }
        request.format = request.selectionType.toLower();
        // The Windows Agent already owns an opened handler. The helper
        // reopens its byte-identical candidate with that known handler, which
        // also enables prefix search for files whose extension is misleading.
        forcedReadFormat = request.selectionType;
    } else {
        if (request.files.size() != 1 || !safeArchivePath(request.files[0]) || request.files[0].contains('\\')) { fail("Invalid archive item name."); return; }
        for (const auto &part : request.files[0].split('/')) if (part.isEmpty() || part == "." || part == "..") { fail("Invalid archive item name."); return; }
    }
    const QStringList formats = commenting ? QStringList{"zip"} : replacing || itemUpdate ? QStringList{"7z", "zip", "tar", "wim", "gzip", "bzip2", "xz"} : QStringList{"7z", "zip", "tar", "wim"};
    if (!formats.contains(request.format.toLower())) { fail("This archive format cannot perform the requested update."); return; }
    // The handler checks the final UTF-8/OEM encoded 16-bit ZIP field. The
    // private UTF-8 transport can be larger than that field for OEM entries.
    if (commenting && (request.comment.contains(QChar::Null) || request.comment.toUtf8().size() > 4 * 65535)) { fail("ZIP comment input is too large or contains NUL."); return; }
    const auto source = QFileInfo(request.archive);
    if (!source.isFile() || source.isSymLink() || !source.isWritable() || ::lstat(QFile::encodeName(request.archive).constData(), &mutationOriginal) != 0 ||
        !S_ISREG(mutationOriginal.st_mode) || !(mutationOriginal.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH))) { fail("Archive is not a writable regular file: " + request.archive); return; }
    if (itemUpdate && request.selectionSnapshot != QString::fromStdString(archiveSourceStamp(mutationOriginal))) { fail("Archive selection changed; refresh before updating. Original retained."); return; }
    const auto workFolder = request.temporaryDirectory.isEmpty() ? source.absolutePath() : request.temporaryDirectory;
    staging = std::make_unique<QTemporaryDir>(workFolder + (replacing ? "/.7zip-replace-XXXXXX" : "/.7zip-folder-XXXXXX"));
    if (!staging->isValid()) { fail("Cannot create archive update staging directory."); return; }
    QDir stage(staging->path());
    if (!stage.mkdir("archive") || !stage.mkdir("input")) { fail("Cannot create archive update staging folders."); return; }
    mutationCandidate = stage.filePath("archive/" + source.fileName()); mutationInput = stage.filePath("input");
    if (replacing && !QDir(mutationInput).mkpath(QFileInfo(request.files[0]).path())) { fail("Cannot create the temporary input folder."); return; }
    if (commenting) {
        QFile value(stage.filePath("comment.txt")); const auto bytes = request.comment.toUtf8();
        if (!value.open(QIODevice::WriteOnly | QIODevice::NewOnly) || !value.setPermissions(QFile::ReadOwner | QFile::WriteOwner) || value.write(bytes) != bytes.size()) { fail("Cannot prepare ZIP comment input."); return; }
    }
    mutationPhase = MutationPhase::Copy; const auto original = mutationOriginal; const auto stop = stopFlag;
    mergeWatcher.setFuture(QtConcurrent::run([this, original, stop]() -> QString {
        const int descriptor = ::open(QFile::encodeName(request.archive).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
        if (descriptor < 0) return ioError("Cannot read archive", request.archive);
        QFile source; if (!source.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) { ::close(descriptor); return source.errorString(); }
        struct stat opened{}; if (::fstat(descriptor, &opened) != 0 || !sameFile(original, opened)) return "Archive changed before update; original retained.";
        QFile candidate(mutationCandidate); if (!candidate.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return candidate.errorString();
        QCryptographicHash digest(QCryptographicHash::Sha256); quint64 done = 0;
        while (!source.atEnd()) {
            if (stop->checkpoint()) return "Operation cancelled; original archive retained.";
            const auto block = source.read(1024 * 1024);
            if (block.isEmpty() && source.error() != QFileDevice::NoError) return source.errorString();
            if (candidate.write(block) != block.size()) return candidate.errorString();
            digest.addData(block); done += quint64(block.size()); emit progress(original.st_size ? int(100.0 * done / original.st_size) : 0, done, quint64(original.st_size), request.archive);
        }
        if (!candidate.flush() || ::fsync(candidate.handle()) != 0) return ioError("Cannot flush staged archive", mutationCandidate);
        struct stat after{}; if (::fstat(descriptor, &after) != 0 || !sameFile(original, after)) return "Archive changed during update preparation; original retained.";
        mutationDigest = digest.result();
        if (request.operation == ArchiveOperation::ReplaceFile) {
            const auto inputPath = QFile::encodeName(request.replacementSource);
            const int inputDescriptor = ::open(inputPath.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
            if (inputDescriptor < 0) return ioError("Cannot read replacement file", request.replacementSource);
            QFile input; if (!input.open(inputDescriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) { ::close(inputDescriptor); return input.errorString(); }
            struct stat before{};
            if (::fstat(inputDescriptor, &before) != 0 || !S_ISREG(before.st_mode)) return "Replacement must be a regular file.";
            QFile output(QDir(mutationInput).filePath(request.files[0]));
            if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return output.errorString();
            QCryptographicHash content(QCryptographicHash::Sha256);
            while (!input.atEnd()) {
                if (stop->checkpoint()) return "Operation cancelled; original archive retained.";
                const auto bytes = input.read(1024 * 1024);
                if (bytes.isEmpty() && input.error() != QFileDevice::NoError) return input.errorString();
                if (output.write(bytes) != bytes.size()) return output.errorString();
                content.addData(bytes);
            }
            struct stat after{};
            struct stat current{};
            if (::fstat(inputDescriptor, &after) != 0 || ::lstat(inputPath.constData(), &current) != 0 || !sameFile(before, after) || !sameFile(before, current)) return "Replacement file changed while being copied; original archive retained.";
            const timespec times[2] = {before.st_atimespec, before.st_mtimespec};
            if (!output.flush() || ::fchmod(output.handle(), before.st_mode & 0777) != 0 || ::futimens(output.handle(), times) != 0 || ::fsync(output.handle()) != 0) return ioError("Cannot prepare replacement file", output.fileName());
            replacementDigest = content.result(); replacementSize = quint64(before.st_size); replacementOriginal = before;
        }
        return {};
    }));
}

void SevenZipProcessBackend::mutationWorkerFinished() {
    // The atomic rename is the commit point. A Cancel event delivered after
    // it must not report that the unchanged original was retained.
    if (mutationPhase == MutationPhase::Commit && mutationInstalled) {
        // The verified metadata belongs to the staged candidate. Atomic
        // installation changes inode/ctime (and can copy across volumes), so
        // bind its row identities to the installed physical archive token.
        struct stat installed{};
        if (archiveMetadata.sourceSnapshot.count(';') == 1 && ::stat(QFile::encodeName(request.archive).constData(), &installed) == 0 && S_ISREG(installed.st_mode))
            archiveMetadata.sourceSnapshot = QString::fromStdString(archiveSourceStamp(installed));
        else archiveMetadata.sourceSnapshot.clear();
        cancelled = false; complete(true, 0); return;
    }
    const auto error = mergeWatcher.result();
    if (!error.isEmpty() || cancelled) { complete(false, cancelled ? 255 : -1, error.isEmpty() ? "Operation cancelled; original archive retained." : error); return; }
    if (mutationPhase == MutationPhase::Copy) { mutationPhase = MutationPhase::BeforeList; launch({"l", "-slt", "--", mutationCandidate}); }
    else if (mutationPhase == MutationPhase::VerifyReplacementDigest) { ++mutationVerificationCursor; verifyNextReplacement(); }
    else complete(true, 0);
}

bool SevenZipProcessBackend::mutationListing(QList<ArchiveEntry> &items) {
    QMap<QString, QString> properties; int types = 0;
    QString header;
    if (metadataStaging) {
        if (!readNativeListing(items)) return false;
        properties = archiveProperties; types = archiveLayers.size();
    } else {
        const auto boundary = textOutput.indexOf("\n----------\n");
        if (boundary < 0) { complete(false, -1, "Archive metadata is missing; original retained."); return false; }
        header = textOutput.left(boundary);
        for (const auto &line : header.split('\n')) {
            const auto separator = line.indexOf(" = "); if (separator < 1) continue;
            const auto name = line.left(separator); if (name == "Type") ++types;
            properties[name] = line.mid(separator + 3);
        }
        QString error; items = parseListing(textOutput.mid(boundary + 12), &error);
        if (!error.isEmpty()) { complete(false, -1, error + " Original archive retained."); return false; }
    }
    QStringList unsupported;
    if (types != 1) unsupported << "open layers: " + QString::number(types);
    if (properties.value("Type").compare(request.format, Qt::CaseInsensitive) != 0) unsupported << "handler: " + properties.value("Type") + " (expected " + request.format + ')';
    if (properties.value("Read-only") == "+") unsupported << "read-only";
    if (properties.value("Multivolume") == "+" || properties.value("Volumes").toInt() > 1) unsupported << "multiple volumes";
    if (properties.value("Tail Size").toULongLong()) unsupported << "tail bytes: " + properties.value("Tail Size");
    if (properties.value("Offset").toLongLong() < 0) unsupported << "negative offset";
    // Original Agent and writable handlers accept nonfatal header warnings.
    // Error flags remain distinct from warning flags and offset notices.
    if (!properties.value("Errors").isEmpty() || !properties.value("Error Flags").isEmpty() || (!metadataStaging && header.contains("\nERRORS:\n"))) unsupported << "archive open errors";
    if (!unsupported.isEmpty()) { complete(false, -1, "This archive layout is not supported for the requested update (" + unsupported.join(", ") + "); original retained."); return false; }
    const auto prefixSize = properties.value("Offset").toULongLong();
    if (prefixSize > quint64(mutationOriginal.st_size)) { complete(false, -1, "Archive prefix exceeds its file size; original retained."); return false; }
    if (mutationPhase == MutationPhase::BeforeList) mutationPrefixSize = prefixSize;
    else if (prefixSize != mutationPrefixSize) { complete(false, -1, "Archive prefix position changed during update; original retained."); return false; }
    QSet<QString> seen;
    for (auto &entry : items) {
        if (entry.path.isEmpty() && items.size() == 1 && QStringList{"gzip", "bzip2", "xz"}.contains(request.format.toLower())) entry.path = ArchiveFormats::unnamedEntry(request.archive);
        entry.path = entryPath(entry.path); entry.properties["Path"] = entry.path;
        bool found = false; for (auto &property : entry.orderedProperties) if (property.name == "Path") { property.value = entry.path; found = true; } if (!found) entry.orderedProperties.prepend({"Path", entry.path});
        const auto key = collisionKey(entry.path);
        const bool itemUpdate = request.operation == ArchiveOperation::Delete || request.operation == ArchiveOperation::Rename || ((request.operation == ArchiveOperation::Comment || request.operation == ArchiveOperation::ReplaceFile) && archiveMetadata.native);
        if (!safeArchivePath(entry.path) || (!itemUpdate && seen.contains(key))) { complete(false, -1, "Unsafe or colliding archive entries; original retained."); return false; }
        seen.insert(key);
    }
    archiveType = properties.value("Type"); properties["Path"] = request.archive; archiveProperties = properties;
    if (!metadataStaging) archiveLayers = parseArchiveLayers(header);
    if (!archiveLayers.isEmpty()) for (auto &property : archiveLayers.first().properties) if (property.name == "Path") property.value = request.archive;
    return true;
}

void SevenZipProcessBackend::mutationProcessFinished() {
    const bool replacing = request.operation == ArchiveOperation::ReplaceFile;
    const bool commenting = request.operation == ArchiveOperation::Comment;
    bool itemUpdate = request.operation == ArchiveOperation::Delete || request.operation == ArchiveOperation::Rename || ((commenting || replacing) && request.selectionDirectory >= 0);
    if (mutationPhase == MutationPhase::BeforeList) {
        if (!mutationListing(mutationOriginalEntries)) return;
        mutationOriginalMetadata = archiveMetadata;
        if ((commenting || replacing) && !itemUpdate) {
            // Compatibility requests with only a path must first resolve one
            // real row. GUI requests already carry their focused row identity.
            int target = -1;
            for (qsizetype index = 0; index < mutationOriginalEntries.size(); ++index) if (mutationOriginalEntries[index].path == entryPath(request.files[0])) {
                if (target >= 0) { complete(false, -1, "Update target is ambiguous; select one native archive row. Original retained."); return; }
                target = int(index);
            }
            if (!archiveMetadata.native || target < 0) { complete(false, -1, "Update target must be one real archive entry; original retained."); return; }
            for (const auto &directory : archiveMetadata.directories) for (const auto &row : directory.children) if (row.archiveIndex == target) {
                request.selection = {{row.identity, row.archiveIndex, row.name}}; request.selectionDirectory = directory.id;
            }
            if (request.selection.isEmpty()) { complete(false, -1, "Cannot resolve native update row; original retained."); return; }
            request.selectionSnapshot = QString::fromStdString(archiveSourceStamp(mutationOriginal)); request.selectionType = archiveType; request.selectionEntryCount = mutationOriginalEntries.size();
            request.selectionFlat = false; request.selectionPreservePaths = false;
            for (const auto &directory : archiveMetadata.directories) if (directory.id == request.selectionDirectory) { request.selectionBasePath = directory.path; request.selectionAlternateStreams = directory.alternateStreams; }
            archiveMetadata.selectionResolved = true; archiveMetadata.selectedIndices = {quint32(target)}; archiveMetadata.selectedPath = mutationOriginalEntries[target].path;
            forcedReadFormat = archiveType; itemUpdate = true;
        }
        if (itemUpdate) {
            if (!archiveMetadata.selectionResolved) { complete(false, -1, "Native archive selection was not resolved; original retained."); return; }
            mutationSelectedIndices = archiveMetadata.selectedIndices; mutationSelectedPath = entryPath(archiveMetadata.selectedPath);
            if (replacing) {
                if (mutationSelectedIndices.size() != 1 || mutationSelectedIndices.first() >= quint32(mutationOriginalEntries.size()) || entryPath(request.files[0]) != mutationSelectedPath) { complete(false, -1, "Replacement target does not match the selected archive row; original retained."); return; }
                const auto &entry = mutationOriginalEntries[mutationSelectedIndices.first()];
                if (entry.directory || entry.link) { complete(false, -1, "Replacement target must be a regular archive file; original retained."); return; }
                if (entry.encrypted && request.password.isEmpty()) { passwordDiagnostic = true; complete(false, 2, "Enter the extraction password to retain updated-file encryption; original retained."); return; }
            }
            if (commenting) {
                if (mutationSelectedIndices.size() != 1 || mutationSelectedIndices.first() >= quint32(mutationOriginalEntries.size()) || entryPath(request.files[0]) != mutationSelectedPath) { complete(false, -1, "Comment target does not match the selected archive row; original retained."); return; }
            }
            if (request.operation == ArchiveOperation::Rename) {
                auto name = request.selection.first().name; name.replace('\\', '/');
                const auto prefix = mutationSelectedPath.left(mutationSelectedPath.size() - name.size());
                const auto newName = request.files[1].mid(prefix.size());
                const auto destination = prefix + newName;
                if (!mutationSelectedPath.endsWith(name) || !safeArchivePath(mutationSelectedPath) || entryPath(request.files[0]) != mutationSelectedPath || !request.files[1].startsWith(prefix) || newName.isEmpty() || newName.endsWith('/') || newName.split('/').contains(QStringLiteral(".")) || newName.split('/').contains(QString()) || newName.contains('\\') || newName.contains(':') || request.files[1] != destination) { complete(false, -1, "Rename target does not match the selected archive row; original retained."); return; }
                const QSet<quint32> selected(mutationSelectedIndices.begin(), mutationSelectedIndices.end());
                QSet<QString> newPaths;
                for (const auto index : mutationSelectedIndices) {
                    const auto path = mutationOriginalEntries[index].path;
                    if (!path.startsWith(mutationSelectedPath)) { complete(false, -1, "Invalid rename prefix; original retained."); return; }
                    newPaths.insert(collisionKey(destination + path.mid(mutationSelectedPath.size())));
                }
                for (qsizetype index = 0; index < mutationOriginalEntries.size(); ++index)
                    if (!selected.contains(quint32(index)) && newPaths.contains(collisionKey(mutationOriginalEntries[index].path))) { complete(false, -1, "Rename destination already exists; original retained."); return; }
            }
            if (mutationSelectedIndices.isEmpty()) {
                archiveMetadata.sourceSnapshot = request.selectionSnapshot; entries = mutationOriginalEntries; complete(true, 0); return;
            }
            mutationPhase = MutationPhase::Update; launch({"l", "-slt", "--", mutationCandidate}); return;
        }
        const auto target = collisionKey(request.files[0]);
        for (const auto &entry : mutationOriginalEntries) {
            const auto key = collisionKey(entry.path);
            if (key == target || key.startsWith(target + '/')) {
                complete(false, -1, "Archive folder or file already exists: " + request.files[0]); return;
            }
            if (target.startsWith(key + '/') && (!entry.directory || entry.link)) { complete(false, -1, "Archive folder parent is not a directory: " + entry.path); return; }
        }
        mutationPhase = MutationPhase::Update;
        // CreateFolder imports AgentOut's NoChange pairs and synthetic item.
        // List opens the same handler, then the private adapter updates it.
        launch({"l", "-slt", "--", mutationCandidate});
    } else if (mutationPhase == MutationPhase::Update) {
        if (!QFile::remove(mutationCandidate) || !QFile::rename(mutationCandidate + ".updated", mutationCandidate)) { complete(false, -1, "Cannot prepare updated archive; original retained."); return; }
        mutationPhase = MutationPhase::AfterList; launch({"l", "-slt", "--", mutationCandidate});
    } else if (mutationPhase == MutationPhase::AfterList) {
        if (!mutationListing(entries)) return;
        validateArchiveMutation();
    } else if (mutationPhase == MutationPhase::VerifyEmptyStream) {
        commitArchiveMutation();
    } else if (mutationPhase == MutationPhase::VerifyReplacement) {
        mutationPhase = MutationPhase::VerifyReplacementDigest; const auto stop = stopFlag;
        mergeWatcher.setFuture(QtConcurrent::run([this, stop]() -> QString {
            QFile input(staging->filePath("replacement-verify-data"));
            if (!input.open(QIODevice::ReadOnly) || quint64(input.size()) != replacementSize) return "Archived replacement size does not match the source; original retained.";
            QCryptographicHash hash(QCryptographicHash::Sha256);
            while (!input.atEnd()) {
                if (stop->checkpoint()) return "Operation cancelled; original retained.";
                const auto bytes = input.read(1024 * 1024);
                if (bytes.isEmpty() && input.error() != QFileDevice::NoError) return input.errorString();
                hash.addData(bytes);
            }
            if (hash.result() != replacementDigest) return "Archived replacement content does not match the source; original retained.";
            return {};
        }));
    }
}

void SevenZipProcessBackend::verifyNextReplacement() {
    if (mutationVerificationCursor >= mutationVerificationCandidates.size()) { commitArchiveMutation(); return; }
    const auto index = mutationVerificationCandidates[mutationVerificationCursor];
    mutationVerificationSelection = {}; mutationVerificationSelection.operation = ArchiveOperation::ReplaceFile;
    mutationVerificationSelection.selectionSnapshot = archiveMetadata.sourceSnapshot; mutationVerificationSelection.selectionType = archiveType; mutationVerificationSelection.selectionEntryCount = entries.size();
    for (const auto &directory : archiveMetadata.directories) for (const auto &row : directory.children) if (row.archiveIndex == index) {
        mutationVerificationSelection.selection = {{row.identity, row.archiveIndex, row.name}}; mutationVerificationSelection.selectionDirectory = directory.id;
    }
    if (mutationVerificationSelection.selection.isEmpty()) { complete(false, -1, "Cannot resolve updated native file for verification; original retained."); return; }
    // Stream plaintext only into private staging, never diagnostics/logs.
    QFile output(staging->filePath("replacement-verify-data"));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate) || !output.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) { complete(false, -1, "Cannot prepare private replacement verification; original retained."); return; }
    output.close(); mutationPhase = MutationPhase::VerifyReplacement;
    launch({"x", "-so", "-bso0", "-bsp0", "-bb0", "--", mutationCandidate});
}

void SevenZipProcessBackend::validateArchiveMutation() {
    const bool replacing = request.operation == ArchiveOperation::ReplaceFile, commenting = request.operation == ArchiveOperation::Comment;
    if (replacing) {
        if (entries.size() != mutationOriginalEntries.size() || mutationSelectedIndices.size() != 1) { complete(false, -1, "Archive entry count changed during replacement; original retained."); return; }
        QMap<QByteArray, QList<qint64>> available;
        auto key = [this](const ArchiveEntry &entry) { return entry.generated && request.format == "wim" ? QByteArray("generated:") + entry.path.toUtf8() : semanticKey(entry); };
        for (qsizetype index = 0; index < entries.size(); ++index) available[key(entries[index])].append(index);
        mutationItemIndexMap.fill(-1, mutationOriginalEntries.size());
        const auto selected = mutationSelectedIndices.first();
        for (qsizetype index = 0; index < mutationOriginalEntries.size(); ++index) {
            if (quint32(index) == selected) continue;
            auto found = available.find(key(mutationOriginalEntries[index]));
            if (found == available.end() || found->isEmpty()) { complete(false, -1, "Unrelated archive item changed during replacement: " + mutationOriginalEntries[index].path + ". Original retained."); return; }
            mutationItemIndexMap[index] = found->takeFirst(); if (found->isEmpty()) available.erase(found);
        }
        if (available.size() != 1 || available.first().size() != 1) { complete(false, -1, "Replacement produced unexpected entries; original retained."); return; }
        const auto updated = available.first().first(); const auto &item = entries[updated];
        // Some single-stream handlers (notably bzip2) do not expose kpidSize.
        // The decoded stream below still has to match the exact source size
        // and SHA-256 before installation.
        const bool knownSize = !item.properties.value("Size").isEmpty();
        if (item.path != mutationSelectedPath || item.directory || item.link || (knownSize && item.size != replacementSize) || (mutationOriginalEntries[selected].encrypted && !item.encrypted)) { complete(false, -1, "Replacement did not retain the original item name/type/encryption; original retained."); return; }
        mutationItemIndexMap[selected] = updated;
        // If the new entry has identical semantics to an existing sibling,
        // verify every indistinguishable candidate before permitting rebinding.
        const auto updatedKey = key(item);
        for (qsizetype index = 0; index < entries.size(); ++index) if (key(entries[index]) == updatedKey) mutationVerificationCandidates.append(quint32(index));
        archiveMetadata.selectionResolved = true; archiveMetadata.selectedIndices = {quint32(updated)}; archiveMetadata.selectedPath = mutationSelectedPath;
        verifyNextReplacement(); return;
    }
    if (commenting && request.selectionDirectory >= 0) {
        if (entries.size() != mutationOriginalEntries.size() || mutationSelectedIndices.size() != 1) { complete(false, -1, "ZIP entry count changed during comment update; original retained."); return; }
        // The official ZIP writer emits the update-pair order. Require that
        // order as well as every semantic property before restoring GUI IDs.
        for (qsizetype index = 0; index < entries.size(); ++index) {
            auto expected = mutationOriginalEntries[index];
            if (quint32(index) == mutationSelectedIndices.first()) expected.properties["Comment"] = request.comment;
            if (semanticKey(entries[index]) != semanticKey(expected)) { complete(false, -1, "ZIP item changed unexpectedly during comment update: " + expected.path + ". Original retained."); return; }
        }
        archiveMetadata.selectionResolved = true; archiveMetadata.selectedIndices = mutationSelectedIndices; archiveMetadata.selectedPath = mutationSelectedPath;
        for (qsizetype index = 0; index < entries.size(); ++index) mutationItemIndexMap.append(index);
        // Original packed data is NoChange, including encrypted payloads.
        // A whole-archive Test would impose an unrelated password requirement.
        commitArchiveMutation(); return;
    }
    if (request.operation == ArchiveOperation::Delete || request.operation == ArchiveOperation::Rename) {
        if (request.operation == ArchiveOperation::Delete && request.format == "xz" &&
            mutationOriginalEntries.size() == 1 && mutationSelectedIndices == QList<quint32>{0}) {
            // XzHandler::UpdateItems(0) calls the official Xz_EncodeEmpty.
            // Its reader still exposes one zero-size logical item.
            if (entries.size() != 1 || entries[0].properties.value("Size") != "0" || entries[0].directory || entries[0].encrypted) {
                complete(false, -1, "XZ deletion did not produce an empty stream; original retained."); return;
            }
            mutationItemIndexMap.fill(-1, 1); mutationPhase = MutationPhase::VerifyEmptyStream;
            launch({"t", "-bsp0", "--", mutationCandidate}); return;
        }
        // Handler order can change (7z, PAX TAR, WIM). Compare a multiset of
        // semantic entries, preserving the multiplicity of same-name items.
        QMap<QByteArray, QList<qint64>> actual;
        for (qsizetype index = 0; index < entries.size(); ++index) {
            const auto &entry = entries[index];
            const auto key = entry.generated && request.format == "wim" ? QByteArray("generated:") + entry.path.toUtf8() : semanticKey(entry);
            actual[key].append(index);
        }
        mutationItemIndexMap.fill(-1, mutationOriginalEntries.size());
        const QSet<quint32> selected(mutationSelectedIndices.begin(), mutationSelectedIndices.end());
        for (qsizetype index = 0; index < mutationOriginalEntries.size(); ++index) {
            auto expected = mutationOriginalEntries[index];
            if (selected.contains(quint32(index))) {
                if (request.operation == ArchiveOperation::Delete) continue;
                // bzip2/XZ accept NewProps but do not store a filename.
                // The official Agent succeeds and the displayed name stays.
                if (request.format != "bzip2" && request.format != "xz")
                    expected.path = request.files[1] + expected.path.mid(mutationSelectedPath.size());
                if (request.format == "tar") {
                    // The official default TAR writer rebuilds NewProps as
                    // GNU headers: second-resolution MTime, no PAX extra
                    // record text/CTime/ATime. NoChange entries stay intact.
                    expected.modified = expected.modified.section('.', 0, 0);
                    for (const auto *name : {"Created", "Accessed", "Comment"}) expected.properties.remove(name);
                }
            }
            const auto key = expected.generated && request.format == "wim" ? QByteArray("generated:") + expected.path.toUtf8() : semanticKey(expected);
            auto found = actual.find(key);
            if (found == actual.end() || found->isEmpty()) { complete(false, -1, "Existing archive item changed unexpectedly: " + expected.path + ". Original retained."); return; }
            const auto updated = found->takeFirst();
            if (request.operation == ArchiveOperation::Rename || !selected.contains(quint32(index))) mutationItemIndexMap[index] = updated;
            if (found->isEmpty()) actual.erase(found);
        }
        if (request.operation == ArchiveOperation::Rename && request.format == "wim") {
            // WimHandlerOut materializes missing parent directories for a
            // new relative path. Permit only those payload-free ancestors;
            // every original semantic entry and its mapping was matched above.
            QSet<QString> parents;
            const auto parts = request.files[1].split('/'); QString prefix;
            for (int part = 0; part + 1 < parts.size(); ++part) {
                prefix += (prefix.isEmpty() ? QString() : QStringLiteral("/")) + parts[part]; parents.insert(prefix);
            }
            for (auto it = actual.begin(); it != actual.end();) {
                bool allowed = true;
                for (const auto index : it.value()) {
                    const auto &entry = entries[index];
                    if (!parents.contains(entry.path) || !entry.directory || entry.size || entry.link || entry.encrypted) { allowed = false; break; }
                }
                if (allowed) it = actual.erase(it); else ++it;
            }
        }
        if (!actual.isEmpty()) { complete(false, -1, "Archive update produced unexpected entries; original retained."); return; }
        // The official writer copies old packed streams; it requests a data
        // password itself only when an encrypted solid block needs repacking.
        commitArchiveMutation(); return;
    }
    QMap<QString, ArchiveEntry> updated; for (const auto &entry : entries) updated.insert(entry.path, entry);
    const auto added = updated.value(request.files[0]);
    if (added.path != request.files[0] || !added.directory || added.size != 0) { complete(false, -1, "7-Zip did not perform the requested update; original retained."); return; }
    for (const auto &old : mutationOriginalEntries) {
        // WIM's synthetic [1].xml is regenerated when image metadata changes;
        // it is not an existing user payload and cannot be held byte-identical.
        if (old.generated && request.format == "wim") {
            if (!updated.value(old.path).generated) { complete(false, -1, "WIM generated metadata is missing; original retained."); return; }
            continue;
        }
        auto before = old, item = updated.value(old.path);
        if (!sameItem(before, item)) {
            complete(false, -1, "Existing archive item changed unexpectedly: " + old.path + ". Original archive retained."); return;
        }
    }
    // Windows CreateFolder copies existing packed streams unchanged. Testing
    // every old payload here would add an unrelated data-password requirement.
    commitArchiveMutation();
}

void SevenZipProcessBackend::commitArchiveMutation() {
    mutationPhase = MutationPhase::Commit; const auto original = mutationOriginal; const auto expected = mutationDigest; const auto stop = stopFlag;
    // Observe the publication boundary without adding unrelated archive tests.
    emit mutationCheckpoint("before-install", request.archive);
    mergeWatcher.setFuture(QtConcurrent::run([this, original, expected, stop]() -> QString {
        const auto sourcePath = QFile::encodeName(request.archive);
        auto candidatePath = QFile::encodeName(mutationCandidate);
        const int descriptor = ::open(sourcePath.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
        if (descriptor < 0) return ioError("Cannot verify original archive", request.archive);
        QFile source; if (!source.open(descriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) { ::close(descriptor); return source.errorString(); }
        struct stat before{}; if (::fstat(descriptor, &before) != 0 || !sameFile(original, before)) return "Original archive changed; update was not installed.";
        QCryptographicHash digest(QCryptographicHash::Sha256);
        while (!source.atEnd()) { if (stop->checkpoint()) return "Operation cancelled; original archive retained."; const auto bytes = source.read(1024 * 1024); if (bytes.isEmpty() && source.error() != QFileDevice::NoError) return source.errorString(); digest.addData(bytes); }
        struct stat current{};
        if (digest.result() != expected || ::lstat(sourcePath.constData(), &current) != 0 || !sameFile(original, current)) return "Original archive changed; update was not installed.";
        if (mutationPrefixSize) {
            QFile updated(mutationCandidate);
            if (!updated.open(QIODevice::ReadOnly) || !source.seek(0)) return "Cannot verify archive prefix; original retained.";
            quint64 remaining = mutationPrefixSize;
            while (remaining) {
                if (stop->checkpoint()) return "Operation cancelled; original archive retained.";
                const auto size = qint64(qMin(remaining, quint64(1024 * 1024)));
                const auto bytes = source.read(size);
                if (bytes.size() != size || updated.read(size) != bytes) return "Archive prefix changed during update; original retained.";
                remaining -= quint64(size);
            }
        }
        if (request.operation == ArchiveOperation::ReplaceFile) {
            const auto inputPath = QFile::encodeName(request.replacementSource);
            const int inputDescriptor = ::open(inputPath.constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
            if (inputDescriptor < 0) return ioError("Cannot verify replacement file", request.replacementSource);
            QFile input; if (!input.open(inputDescriptor, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) { ::close(inputDescriptor); return input.errorString(); }
            struct stat before{};
            if (::fstat(inputDescriptor, &before) != 0 || !sameFile(replacementOriginal, before)) return "Replacement file changed; original archive retained.";
            QCryptographicHash content(QCryptographicHash::Sha256);
            while (!input.atEnd()) { if (stop->checkpoint()) return "Operation cancelled; original archive retained."; const auto bytes = input.read(1024 * 1024); if (bytes.isEmpty() && input.error() != QFileDevice::NoError) return input.errorString(); content.addData(bytes); }
            if (content.result() != replacementDigest || ::lstat(inputPath.constData(), &current) != 0 || !sameFile(replacementOriginal, current)) return "Replacement file changed; original archive retained.";
        }
        if (stop->checkpoint()) return "Operation cancelled; original archive retained.";
        // Work Folder can be on another volume. Complete a cancellable copy
        // beside the original before the final atomic rename, as opposed to
        // attempting a cross-volume rename or ignoring the preference.
        std::unique_ptr<QTemporaryDir> installation;
        struct stat staged{};
        if (::lstat(candidatePath.constData(), &staged) != 0 || !S_ISREG(staged.st_mode)) return ioError("Cannot inspect completed archive", mutationCandidate);
        if (staged.st_dev != original.st_dev) {
            installation = std::make_unique<QTemporaryDir>(QFileInfo(request.archive).absolutePath() + "/.7zip-install-XXXXXX");
            if (!installation->isValid()) return "Cannot create final archive installation directory; original retained.";
            QFile input(mutationCandidate), output(installation->filePath("archive"));
            if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return "Cannot prepare final archive installation; original retained.";
            quint64 copied = 0;
            while (!input.atEnd()) {
                if (stop->checkpoint()) return "Operation cancelled; original archive retained.";
                const auto bytes = input.read(1024 * 1024);
                if ((bytes.isEmpty() && input.error() != QFileDevice::NoError) || output.write(bytes) != bytes.size()) return "Cannot copy completed archive for installation; original retained.";
                copied += quint64(bytes.size()); emit progress(staged.st_size ? int(100.0 * copied / staged.st_size) : 0, copied, quint64(staged.st_size), request.archive);
            }
            if (!output.flush()) return "Cannot flush final archive installation; original retained.";
            candidatePath = QFile::encodeName(output.fileName());
        }
        const int candidateDescriptor = ::open(candidatePath.constData(), O_RDWR | O_NOFOLLOW);
        if (candidateDescriptor < 0) return ioError("Cannot open completed archive", mutationCandidate);
        struct stat candidateInfo{};
        if (::fstat(candidateDescriptor, &candidateInfo) != 0 ||
            ((candidateInfo.st_uid != original.st_uid || candidateInfo.st_gid != original.st_gid) && ::fchown(candidateDescriptor, original.st_uid, original.st_gid) != 0) ||
            ::fcopyfile(descriptor, candidateDescriptor, nullptr, COPYFILE_XATTR | COPYFILE_ACL) != 0) {
            const auto error = ioError("Cannot preserve archive ownership, ACL or extended attributes", request.archive); ::close(candidateDescriptor); return error;
        }
        const int syncResult = ::fsync(candidateDescriptor), syncError = errno; ::close(candidateDescriptor);
        if (syncResult != 0) { errno = syncError; return ioError("Cannot flush completed archive", mutationCandidate); }
        if (::chmod(candidatePath.constData(), original.st_mode & 07777) != 0) return ioError("Cannot preserve archive permissions", mutationCandidate);
        if (stop->checkpoint()) return "Operation cancelled; original archive retained.";
        if (::lstat(sourcePath.constData(), &current) != 0 || !sameFile(original, current)) return "Original archive changed; update was not installed.";
        if (request.operation == ArchiveOperation::ReplaceFile && (::lstat(QFile::encodeName(request.replacementSource).constData(), &current) != 0 || !sameFile(replacementOriginal, current))) return "Replacement file changed; original archive retained.";
        // Candidate and destination share a volume; rename installs the
        // completed archive atomically. A final source-change check above
        // protects normal concurrent edits; this is not a global file lock.
        if (::rename(candidatePath.constData(), sourcePath.constData()) != 0) return ioError("Cannot install updated archive", request.archive);
        mutationInstalled = true;
        return {};
    }));
}
