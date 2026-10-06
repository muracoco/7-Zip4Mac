// SPDX-License-Identifier: LGPL-3.0-or-later
// Official 7-Zip prepares streams/links/metadata; this adapter installs them.
#include "ArchiveBackend.h"
#include "ExtractionInstaller.h"
#include "ExtractionSession.h"
#include "FileInstall.h"
#include "ArchiveSourceStamp.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QtConcurrent>
#include <algorithm>
#include <copyfile.h>
#include <fcntl.h>
#include <sys/attr.h>
#include <unistd.h>
#include <optional>
#include <cerrno>
#include <climits>
#include <cstring>


void SevenZipProcessBackend::beginExtract() {
    const bool nativeSelection = request.selectionDirectory >= 0;
    const QSet<quint32> indices(archiveMetadata.selectedIndices.begin(), archiveMetadata.selectedIndices.end());
    auto selectedEntry = [&](const ArchiveEntry &entry) {
        if (nativeSelection) return entry.archiveIndex >= 0 && indices.contains(quint32(entry.archiveIndex));
        bool selected = request.files.isEmpty(); for (const auto &path : request.files) selected |= entry.path == path || entry.path.startsWith(path + '/'); return selected;
    };
    bool anySelected = !nativeSelection && request.files.isEmpty();
    if (nativeSelection) totalSize = 0;
    for (const auto &e : entries) {
        const bool selected = selectedEntry(e);
        if (nativeSelection && !selected) continue;
        anySelected |= selected;
        const QString stagedPath = e.path.startsWith('/') && request.pathMode == "absolute" ? e.path.mid(1) : e.path;
        if (!safeArchivePath(stagedPath) || stagedPath.contains("//")) {
            complete(false, -1, "Unsafe archive path refused: " + e.path); return;
        }
        if (selected) totalSize += e.size;
    }
    if (!anySelected) { complete(nativeSelection, nativeSelection ? 0 : -1, nativeSelection ? "There are no files to process." : "No selected archive items matched."); return; }
    if (request.outputDirectory.isEmpty() || !QDir().mkpath(request.outputDirectory)) {
        complete(false, -1, "Cannot create extraction output directory."); return;
    }
    if (QFileInfo(request.outputDirectory).isSymLink()) { complete(false, -1, "Symbolic-link output directory refused."); return; }
    request.outputDirectory = QFileInfo(request.outputDirectory).canonicalFilePath();
    if (request.pathMode == "absolute") for (const auto &entry : entries) {
        QString link = entry.properties.value("Hard Link"); if (link.isEmpty()) continue;
        link.replace('\\', '/'); link = QDir::cleanPath(link);
        const auto relative = link.startsWith('/') ? link.mid(1) : link;
        if (!safeArchivePath(relative) || relative.contains("//")) continue;
        const auto target = link.startsWith('/') ? link : request.outputDirectory + '/' + relative;
        if (extractHardTargets.contains(relative) && extractHardTargets.value(relative) != target) { complete(false, -1, "Conflicting absolute hard-link targets refused."); return; }
        extractHardTargets.insert(relative, target);
    }
    if (request.pathMode == "absolute") for (const auto &entry : entries) {
        if (!selectedEntry(entry)) continue;
        QString rel = entry.path.startsWith('/') ? entry.path.mid(1) : entry.path;
        QString target = entry.path.startsWith('/') ? entry.path : request.outputDirectory + '/' + rel;
        while (!rel.isEmpty() && rel != ".") {
            if (extractTargets.contains(rel) && extractTargets.value(rel) != target) { complete(false, -1, "Conflicting absolute extraction paths refused."); return; }
            extractTargets.insert(rel, target); int slash = rel.lastIndexOf('/'); if (slash < 0) break; rel = rel.left(slash); target = QFileInfo(target).absolutePath();
        }
    }
    QString basePrefix;
    if (request.eliminateRoot && request.pathMode == "full" && !entries.isEmpty()) {
        const QString name = QFileInfo(request.outputDirectory).fileName(); bool matches = !name.isEmpty();
        // The ordinary Agent proxy removes the current folder prefix. Tree
        // handlers retain root-relative paths in Full pathnames mode.
        if (nativeSelection && !request.selectionPreservePaths && !archiveMetadata.directories.isEmpty() &&
            archiveMetadata.directories[0].alternateDirectory < 0 && request.selectionDirectory < archiveMetadata.directories.size())
            basePrefix = archiveMetadata.directories[request.selectionDirectory].path;
        for (const auto &entry : entries) {
            if (nativeSelection && !selectedEntry(entry)) continue;
            QString path = entry.path; if (!basePrefix.isEmpty() && path.startsWith(basePrefix)) path.remove(0, basePrefix.size());
            matches &= (path == name && entry.directory) || path.startsWith(name + '/');
        }
        if (matches) extractRootPrefix = name;
    }
    if (!extractRootPrefix.isEmpty()) for (const auto &entry : entries) {
        QString link = entry.properties.value("Hard Link"); if (link.isEmpty()) continue;
        link.replace('\\', '/'); link = QDir::cleanPath(link);
        if (!basePrefix.isEmpty() && link.startsWith(basePrefix)) link.remove(0, basePrefix.size());
        if (!safeArchivePath(link) || !link.startsWith(extractRootPrefix + '/')) continue;
        // The official callback still prepares the uneliminated private path;
        // its existing public reference lives under the reduced output root.
        extractHardTargets.insert(link, QDir(request.outputDirectory).filePath(link.mid(extractRootPrefix.size() + 1)));
    }
    staging = std::make_unique<QTemporaryDir>(request.outputDirectory + "/.7zip-extract-XXXXXX");
    if (!staging->isValid()) { complete(false, -1, "Cannot create extraction staging directory."); return; }
    const QMap<QString,QString> overwriteArguments{{"ask",""},{"overwrite","-aoa"},{"skip","-aos"},{"rename","-aou"},{"renameExisting","-aot"}};
    if (!overwriteArguments.contains(request.overwriteMode)) { complete(false,-1,"Unsupported extraction overwrite mode."); return; }
    QStringList args{request.pathMode == "flat" ? "e" : "x", "-o" + staging->path()};
    if (!overwriteArguments.value(request.overwriteMode).isEmpty()) args << overwriteArguments.value(request.overwriteMode);
    args << "-bsp1" << "-bb1" << "--" << request.archive;
    if (!nativeSelection) args << request.files; launch(args);
}
void SevenZipProcessBackend::extractionWorkerFinished() {
    const auto reply=extractionWatcher.result();
    if (reply.kind==ExtractionTaskReply::Close) {
        readOutput(); paused=false; if(stopFlag) stopFlag->setPaused(false);
        emit pausedChanged(false); emit pauseAvailabilityChanged(false);
        const auto code=extractionExit?extractionExit->first:-1;
        const auto status=extractionExit?extractionExit->second:QProcess::CrashExit;
        extractExitCode=cancelled?255:status==QProcess::NormalExit?code:-1;
        const bool dangerousLink=textErrors.contains("Dangerous link")||textErrors.contains("Hard link to symbolic link");
        extractResultMessage=cancelled?"Operation cancelled.":status==QProcess::CrashExit?"7-Zip terminated unexpectedly.":dangerousLink?"Unsafe archive link refused by 7-Zip.":code?"7-Zip reported an error or warning. Extracted files may be incomplete.":QString();
        if (reply.retainStaging && staging) staging->setAutoRemove(false);
        extractionSession.reset(); extractionExit.reset(); finishExtract(reply.error); return;
    }
    if (!active) return;
    if (reply.kind==ExtractionTaskReply::State) progressChannel.replyFileState(reply.state,reply.file);
    else if(reply.kind==ExtractionTaskReply::Publication) progressChannel.replyExtraction(reply.publication,reply.code);
    else { cancelled |= reply.answer==OverwriteAnswer::Cancel; progressChannel.replyOverwrite(reply.overwrite,reply.answer); }
    if(extractionExit) closeExtractionSession();
}
void SevenZipProcessBackend::closeExtractionSession() {
    const auto session=extractionSession;
    extractionWatcher.setFuture(QtConcurrent::run([session] {
        ExtractionTaskReply reply;
        reply.kind = ExtractionTaskReply::Close;
        reply.error = session->close();
        reply.retainStaging = session->retainsStaging();
        return reply;
    }));
}
static bool safeDestination(const QString &root, const QString &relative) {
    QString path = root;
    for (const auto &part : relative.split('/')) {
        path += '/' + part; QFileInfo f(path);
        if (f.isSymLink()) return false;
        if (f.exists() && root != "/" && !f.canonicalFilePath().startsWith(root + '/') && f.canonicalFilePath() != root) return false;
    }
    return true;
}
void SevenZipProcessBackend::finishExtract(QString error) {
    auto message = extractResultMessage;
    if (!error.isEmpty()) { if (!message.isEmpty()) message += '\n'; message += error; }
    const int code = cancelled ? 255 : extractExitCode != 0 ? extractExitCode : error.isEmpty() ? 0 : -1;
    complete(!cancelled && extractExitCode == 0 && error.isEmpty(), code, message);
}
void SevenZipProcessBackend::commitExtract() {
    const auto removedRoot = extractRootPrefix;
    if (!removedRoot.isEmpty()) {
        const QString root = staging->filePath(removedRoot);
        for (const auto &entry : QDir(root).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
            if (!QFile::rename(entry.filePath(), staging->filePath(entry.fileName()))) { finishExtract("Cannot eliminate duplicate root folder."); return; }
        }
        if (QFileInfo::exists(root) && !QDir().rmdir(root)) { finishExtract("Cannot eliminate duplicate root folder."); return; }
        extractRootPrefix.clear();
    }
    QMap<QString, ExtractMetadata> metadata, fileMetadata; QMap<QString, int> order;
    QMap<QString, QString> payloads, logicalNames, groupIds, lastRecord;
    int serial = 0;
    struct Seed { QString source, sourceSnapshot; quint64 device = 0, inode = 0; };
    QMap<QString, Seed> seeds; QMap<QString, QString> publicSnapshots;
    if (!extractMetadataStaging) { finishExtract("Official extraction output records are missing."); return; }
    QFile outputs(extractMetadataStaging->filePath("outputs.jsonl"));
    if (outputs.exists()) {
        if (!outputs.open(QIODevice::ReadOnly) || outputs.readLine() != "7ZOUT002\n") { finishExtract("Invalid extraction output records."); return; }
        auto relative = [&](const QString &path, bool eliminate = true) {
            if (path == staging->path()) return QStringLiteral("");
            if (!path.startsWith(staging->path() + '/')) return QString();
            auto name = path.mid(staging->path().size() + 1);
            while (name.endsWith('/')) name.chop(1);
            if (eliminate) { if (name == removedRoot) name.clear(); else if (!removedRoot.isEmpty() && name.startsWith(removedRoot + '/')) name.remove(0, removedRoot.size() + 1); }
            return name;
        };
        auto publicTarget = [&](const QString &path) {
            const auto rel = relative(path, false);
            if (rel.isNull() || rel.isEmpty() || !safeArchivePath(rel)) return QString();
            return QDir::cleanPath(request.pathMode == "absolute" ? extractHardTargets.value(rel) : request.outputDirectory + '/' + rel);
        };
        while (!outputs.atEnd()) {
            QJsonParseError error; const auto document = QJsonDocument::fromJson(outputs.readLine(65537), &error);
            const auto row = document.object(); const auto index = row.value("index").toInteger(-1); const auto path = row.value("path").toString();
            if (row.value("kind").toString() == "seed") {
                const auto name = relative(path); Seed seed;
                seed.source = row.value("source").toString(); seed.sourceSnapshot = row.value("sourceSnapshot").toString();
                bool dev = false, inode = false; seed.device = row.value("device").toString().toULongLong(&dev); seed.inode = row.value("inode").toString().toULongLong(&inode);
                if (error.error != QJsonParseError::NoError || name.isNull() || name.isEmpty() || !safeArchivePath(name) || !dev || !inode || seed.sourceSnapshot.isEmpty() || seed.source.isEmpty() || seed.source != publicTarget(path)) { finishExtract("Invalid existing hard-link source record."); return; }
                if (publicSnapshots.contains(seed.source) && publicSnapshots.value(seed.source) != seed.sourceSnapshot) { finishExtract("Existing hard-link source changed during extraction."); return; }
                seeds.insert(name.normalized(QString::NormalizationForm_C), seed); publicSnapshots.insert(seed.source, seed.sourceSnapshot); continue;
            }
            if (error.error != QJsonParseError::NoError || !document.isObject() || index < 0 || index >= entries.size() || !row.value("path").isString() || !row.value("hardLink").isString()) { finishExtract("Invalid native extraction item record."); return; }
            auto name = relative(path);
            if (name.isNull() || (!name.isEmpty() && !safeArchivePath(name))) { finishExtract("Unexpected extraction path outside private staging."); return; }
            auto info = extractionMetadataFor(entries[index]);
            const auto link = row.value("hardLink").toString(); info.hardLink.clear();
            if (!link.isEmpty()) {
                info.hardLink = relative(link);
                if (info.hardLink.isNull() || info.hardLink.isEmpty() || !safeArchivePath(info.hardLink)) { finishExtract("Unexpected hard link target outside private staging."); return; }
                info.hardLinkSource = row.value("publicHardLink").toString();
                if (info.hardLinkSource.isEmpty() || info.hardLinkSource != publicTarget(link)) { finishExtract("Unexpected public hard-link target."); return; }
            }
            metadata[name.normalized(QString::NormalizationForm_C)] = info;
            if (!row.value("snapshot").isString() || !row.value("group").isString()) { finishExtract("Missing completed extraction snapshot."); return; }
            const auto saved = row.value("snapshot").toString();
            if (!saved.isEmpty()) {
                const auto base = extractMetadataStaging->filePath("payloads/item-");
                static const QRegularExpression number("^[0-9]+$"), group("^[0-9]+:[0-9]+$");
                if (!saved.startsWith(base) || !number.match(saved.mid(base.size())).hasMatch() || name.isEmpty() || !group.match(row.value("group").toString()).hasMatch()) { finishExtract("Invalid completed extraction snapshot."); return; }
                const auto owner = QString::number(index) + ':' + name;
                if (lastRecord.contains(owner)) {
                    const auto previous = lastRecord.value(owner);
                    payloads.remove(previous); logicalNames.remove(previous); groupIds.remove(previous); fileMetadata.remove(previous); order.remove(previous);
                }
                const auto key = QString::number(++serial); lastRecord[owner] = key;
                payloads[key] = saved; logicalNames[key] = name; groupIds[key] = row.value("group").toString(); fileMetadata[key] = info; order[key] = serial;
            }
        }
    }
    const QString stage = staging->path(), root = request.outputDirectory;
    const bool partialOutput = extractExitCode != 0;
    auto mode = request.overwriteMode; auto stop = stopFlag; const auto targets = extractTargets; const bool absolute = request.pathMode == "absolute";
    mergeWatcher.setFuture(QtConcurrent::run([this, stage, root, mode, stop, targets, absolute, removedRoot, partialOutput, metadata, fileMetadata, payloads, logicalNames, groupIds, order, seeds, publicSnapshots]() mutable -> QString {
        auto destination = [&](const QString &rel) { return absolute ? targets.value(rel) : root + '/' + rel; };
        QDirIterator iter(stage, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);
        QStringList dirs, files; QMap<QString, FileSnapshot> original, fileSources;
        while (iter.hasNext()) {
            if (stop->checkpoint()) return "Cancelled while installing extracted files. Completed files were retained.";
            iter.next(); const auto rel = QDir(stage).relativeFilePath(iter.filePath()); const auto target = destination(rel);
            auto source = FileInstall::capture(iter.filePath(), true);
            if (!source.error.isEmpty()) return source.error;
            const auto normalized = rel.normalized(QString::NormalizationForm_C);
            if (seeds.contains(normalized) && !metadata.contains(normalized)) {
                const auto seed = seeds.value(normalized);
                if (!source.exists || !S_ISREG(source.stamp.st_mode) || quint64(source.stamp.st_dev) != seed.device || quint64(source.stamp.st_ino) != seed.inode || source.stamp.st_size != 0) return "Private hard-link reference changed: " + rel;
                continue;
            }
            if (target.isEmpty() || (!S_ISDIR(source.stamp.st_mode) && !S_ISREG(source.stamp.st_mode) && !S_ISLNK(source.stamp.st_mode)) ||
                !safeArchivePath(rel) || !safeDestination("/", QFileInfo(target).absolutePath().mid(1))) return "Unsafe output path or link refused: " + rel;
            if (!S_ISDIR(source.stamp.st_mode) && !metadata.contains(normalized)) {
                // Failed/deferred link placeholders have no completed native
                // record. Retain only callback-authorized outputs on failure.
                if (partialOutput) continue;
                return "Unreported extracted file refused: " + rel;
            }
            if (S_ISDIR(source.stamp.st_mode)) { original[rel] = source; dirs << rel; }
        }
        std::sort(dirs.begin(), dirs.end(), [](const QString &a, const QString &b) { return a.size() == b.size() ? a < b : a.size() < b.size(); });
        for (auto iter = payloads.cbegin(); iter != payloads.cend(); ++iter) {
            auto source = FileInstall::capture(iter.value(), true);
            if (!source.error.isEmpty()) return source.error;
            const auto rel = logicalNames.value(iter.key());
            if (!source.exists || (!S_ISREG(source.stamp.st_mode) && !S_ISLNK(source.stamp.st_mode)) ||
                destination(rel).isEmpty() || !safeArchivePath(rel) || !safeDestination("/", QFileInfo(destination(rel)).absolutePath().mid(1))) return "Invalid completed extraction output: " + rel;
            fileSources[iter.key()] = source; files << iter.key();
        }
        // This is the original callback's completion order, including deferred
        // links; listing indices and a files-before-links sort are not equivalent.
        std::sort(files.begin(), files.end(), [&](const QString &a, const QString &b) { return order.value(a) < order.value(b); });
        ExtractionInstaller installer(stage, root, absolute, targets, removedRoot, mode, overwriteBroker, stop, publicSnapshots);
        const auto install = [&]() -> QString {
            for (const auto &rel : dirs) {
                const auto error = installer.prepareDirectory(rel, original[rel], metadata.value(rel.normalized(QString::NormalizationForm_C)));
                if (!error.isEmpty()) return error;
            }
            for (const auto &key : files) {
                const auto rel = logicalNames.value(key);
                const auto error = installer.install(rel, fileSources[key], fileMetadata.value(key), groupIds.value(key));
                if (!error.isEmpty()) return error;
                emit progress(100, totalSize, totalSize, rel);
            }
            return {};
        };
        return installer.finish(install());
    }));
}
