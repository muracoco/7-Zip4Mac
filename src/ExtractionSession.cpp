// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ExtractionSession.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <cerrno>
#include <vector>

struct ExtractionSession::Data {
    QString stage, root, records, diagnostic, current, removedRoot;
    bool absolute;
    QMap<QString, QString> targets;
    FileSnapshot approved;
    QMap<QString, FileSnapshot> probes;
    QMap<qint64, ArchiveEntry> entries;
    struct Directory { QString nativePath; qint64 index; FileSnapshot observed; bool created = false; };
    QMap<QString, Directory> directories;
    struct OpenStream { FileSnapshot source, expected; };
    QMap<QString, OpenStream> openStreams;
    struct Retired { std::unique_ptr<QTemporaryDir> folder; QString original, saved; bool published = false, restored = false; };
    std::vector<Retired> retired;
    std::shared_ptr<OperationControl> publication = std::make_shared<OperationControl>();
    ExtractionInstaller installer;
    bool finished = false, retainStage = false;
    Data(QString stage, QString root, QString records, const QList<ArchiveEntry> &items, OverwriteBroker &broker,
         QString removedRoot, bool absolute, QMap<QString, QString> targets)
        : stage(std::move(stage)), root(std::move(root)), records(std::move(records)),
          removedRoot(std::move(removedRoot)), absolute(absolute), targets(std::move(targets)),
          installer(this->stage, this->root, this->absolute, this->targets, this->removedRoot, "overwrite", broker, publication) {
        for (qsizetype index = 0; index < items.size(); ++index) entries[items[index].archiveIndex >= 0 ? items[index].archiveIndex : index] = items[index];
    }
    QString relative(const QString &path) const {
        if (path == stage) return QStringLiteral("");
        if (!path.startsWith(stage + '/')) return {};
        auto name = path.mid(stage.size() + 1);
        if (!removedRoot.isEmpty()) {
            if (name == removedRoot) return QStringLiteral("");
            if (!name.startsWith(removedRoot + '/')) return {};
            name.remove(0, removedRoot.size() + 1);
        }
        return SevenZipProcessBackend::safeArchivePath(name) ? name : QString();
    }
    QString destination(const QString &name) const { return extractionOutputTarget(root, name, absolute, targets); }
    int fail(int code, QString message) {
        if (diagnostic.isEmpty()) diagnostic = std::move(message);
        else if (!message.isEmpty() && !diagnostic.contains(message)) diagnostic += '\n' + message;
        for (auto &folder : retired) {
            if (folder.published || folder.restored) continue;
            folder.folder->setAutoRemove(false);
            const auto location = "\nApproved previous output retained in: " + folder.folder->path();
            if (!diagnostic.contains(location)) diagnostic += location;
        }
        return code;
    }
    int move(const FileSnapshot &source, const FileSnapshot &target) {
        if (target.exists) return fail(EEXIST, "The original rename destination is occupied: " + target.path);
        const auto result = FileInstall::tryMove(source, target, publication);
        if (!result) return fail(EOPNOTSUPP, "This filesystem cannot perform the guarded original rename: " + source.path);
        if (!result->error.isEmpty()) return fail(EIO, result->error);
        const auto error = installer.relocated(source.path, result->installedFile.path);
        return error.isEmpty() ? 0 : fail(ESTALE, error);
    }
    int mutation(const QJsonObject &item) {
        const auto name = relative(item.value("path").toString());
        if (name.isEmpty() || name != current) return fail(EINVAL, "Mutation does not match the original pre-stream observation.");
        if (!FileInstall::unchanged(approved)) return fail(ESTALE, "Approved extraction output changed: " + approved.path);
        const auto operation = item.value("operation").toString();
        if (operation == "rename") {
            const auto other = relative(item.value("destination").toString());
            if (other.isEmpty() || !probes.contains(other)) return fail(EINVAL, "Original auto-name destination was not inspected.");
            const auto result = move(approved, probes.value(other)); if (result) return result;
        } else if (operation == "delete" || operation == "rmdir") {
            const auto check = FileInstall::validateRemoval(approved, operation == "rmdir");
            if (check) return fail(check, "Cannot remove extraction output: " + approved.path);
            auto folder = std::make_unique<QTemporaryDir>(QFileInfo(approved.path).absolutePath() + "/.7zip-displaced-XXXXXX");
            if (!folder->isValid()) return fail(EIO, "Cannot preserve approved extraction output.");
            const auto source = approved, target = FileInstall::capture(folder->filePath("previous"));
            const auto result = move(source, target);
            if (result) { folder->setAutoRemove(false); retired.push_back({std::move(folder), source.path, target.path}); return result; }
            if (operation == "rmdir" && FileInstall::validateRemoval(FileInstall::capture(target.path), true)) {
                const auto restore = FileInstall::tryMove(FileInstall::capture(target.path), FileInstall::capture(source.path), publication);
                if (!restore || !restore->error.isEmpty()) { folder->setAutoRemove(false); diagnostic = "Directory changed during removal; recovery retained in: " + folder->path(); }
                retired.push_back({std::move(folder), source.path, target.path}); return ENOTEMPTY;
            }
            retired.push_back({std::move(folder), source.path, target.path});
        } else return fail(EINVAL, "Unknown original extraction mutation.");
        approved = FileInstall::capture(destination(name)); return approved.error.isEmpty() && !approved.exists ? 0 : fail(ESTALE, "Removed extraction output changed before opening its stream.");
    }
    int begin(const QJsonObject &item) {
        const auto path = item.value("path").toString(), name = relative(path);
        const auto index = qint64(item.value("index").toDouble(-1));
        bool validDevice = false, validInode = false;
        const auto device = item.value("device").toString().toULongLong(&validDevice);
        const auto inode = item.value("inode").toString().toULongLong(&validInode);
        if (name.isNull() || name.isEmpty() || !entries.contains(index) || !validDevice || !validInode)
            return fail(EINVAL, "Invalid original open-stream identity.");
        const auto source = FileInstall::capture(path);
        if (!source.error.isEmpty() || !source.exists || !S_ISREG(source.stamp.st_mode) ||
            quint64(source.stamp.st_dev) != device || quint64(source.stamp.st_ino) != inode)
            return fail(ESTALE, "Original extraction stream changed before registration: " + path);
        // Split items reopen the same file. Keep the first public approval;
        // later pieces must never acquire a fresh permission to replace it.
        if (openStreams.contains(name)) {
            const auto &previous = openStreams[name].source;
            return previous.stamp.st_dev == source.stamp.st_dev && previous.stamp.st_ino == source.stamp.st_ino
                ? 0 : fail(ESTALE, "Split extraction stream changed: " + path);
        }
        const auto error = FileInstall::ensureDirectory(QFileInfo(destination(name)).absolutePath());
        if (!error.isEmpty()) return fail(EIO, error);
        auto expected = name == current ? approved : probes.contains(name) ? probes.value(name) : FileInstall::capture(destination(name));
        if (!expected.parent || expected.parent->descriptor < 0) {
            const auto now = FileInstall::capture(destination(name));
            if (expected.exists || now.exists) return fail(ESTALE, "Previously absent extraction output changed: " + now.path);
            expected = now;
        }
        if (!expected.error.isEmpty() || (name != current && expected.exists))
            return fail(EINVAL, "Open stream has no original pre-stream approval: " + expected.path);
        openStreams.insert(name, {source, expected});
        return 0;
    }
    int publish(const QJsonObject &item) {
        const auto name = relative(item.value("path").toString());
        const auto index = qint64(item.value("index").toDouble(-1));
        if (name.isNull() || destination(name).isEmpty() || !entries.contains(index)) return fail(EINVAL, "Invalid native completed-output identity.");
        const auto saved = item.value("snapshot").toString();
        if (saved.isEmpty()) {
            const auto source = FileInstall::capture(item.value("path").toString());
            if (!source.error.isEmpty() || !source.exists || !S_ISDIR(source.stamp.st_mode)) return fail(EINVAL, "Invalid original directory record.");
            const auto before = FileInstall::inspect(destination(name));
            if (!before.error.isEmpty()) return fail(before.errorCode ? before.errorCode : EIO, before.error);
            const bool created = !before.exists || (directories.contains(name) && directories.value(name).created);
            const auto error = FileInstall::ensureDirectory(destination(name));
            if (!error.isEmpty()) return fail(EIO, error);
            directories[name] = {item.value("path").toString(), index, FileInstall::capture(destination(name)), created}; return 0;
        }
        const auto base = records + "/payloads/item-";
        static const QRegularExpression number("^[0-9]+$"), group("^[0-9]+:[0-9]+$");
        if (!saved.startsWith(base) || !number.match(saved.mid(base.size())).hasMatch() || !group.match(item.value("group").toString()).hasMatch()) return fail(EINVAL, "Invalid frozen native output.");
        const auto error = FileInstall::ensureDirectory(QFileInfo(destination(name)).absolutePath()); if (!error.isEmpty()) return fail(EIO, error);
        FileSnapshot expected = name == current ? approved : probes.contains(name) ? probes.value(name) : FileInstall::capture(destination(name));
        if (!expected.parent || expected.parent->descriptor < 0) {
            const auto now = FileInstall::capture(destination(name));
            if (expected.exists || now.exists) return fail(ESTALE, "Previously absent extraction output changed: " + now.path);
            expected = now;
        }
        if (name != current && expected.exists) return fail(EINVAL, "Output has no original pre-stream approval: " + expected.path);
        auto metadata = extractionMetadataFor(entries.value(index));
        metadata.hardLink.clear();
        const auto link = item.value("hardLink").toString();
        if (!link.isEmpty()) {
            metadata.hardLink = relative(link); metadata.hardLinkSource = item.value("publicHardLink").toString();
            if (metadata.hardLink.isNull() || metadata.hardLink.isEmpty() || metadata.hardLinkSource.isEmpty() || metadata.hardLinkSource != destination(metadata.hardLink))
                return fail(EINVAL, "Invalid original public hard-reference target.");
        }
        const auto result = installer.install(name, FileInstall::capture(saved), metadata, item.value("group").toString(), expected, "overwrite");
        if (!result.isEmpty()) return fail(EIO, result);
        openStreams.remove(name);
        for (auto &retained : retired) if (QFile::encodeName(retained.original) == QFile::encodeName(destination(name))) retained.published = true;
        probes.clear(); current.clear(); approved = {}; return 0;
    }
    void recoverOpenStreams() {
        // The original callback leaves the opened stream behind on process
        // death. Recover only identities registered after its successful open,
        // never arbitrary files found in staging. Decoding/close metadata has
        // not completed, so use the actual stream metadata, not archive values.
        for (auto stream = openStreams.cbegin(); stream != openStreams.cend(); ++stream) {
            const auto now = FileInstall::capture(stream->source.path);
            QString error;
            if (!now.error.isEmpty() || !now.exists || !S_ISREG(now.stamp.st_mode) ||
                now.stamp.st_dev != stream->source.stamp.st_dev || now.stamp.st_ino != stream->source.stamp.st_ino)
                error = "Interrupted extraction stream changed: " + stream->source.path;
            else {
                const auto group = QString::number(quint64(now.stamp.st_dev)) + ':' + QString::number(quint64(now.stamp.st_ino));
                error = installer.install(stream.key(), now, {}, group, stream->expected, "overwrite");
            }
            if (!error.isEmpty()) {
                retainStage = true;
                if (!diagnostic.isEmpty()) diagnostic += '\n';
                diagnostic += error;
            } else for (auto &retained : retired)
                if (QFile::encodeName(retained.original) == QFile::encodeName(destination(stream.key()))) retained.published = true;
        }
        openStreams.clear();
        if (retainStage) diagnostic += "\nInterrupted extraction data retained in: " + stage;
    }
    int finish(bool originalCleanup = true) {
        QString error;
        for (auto directory = directories.cbegin(); originalCleanup && directory != directories.cend(); ++directory) {
            const auto now = FileInstall::capture(destination(directory.key()));
            if (!now.error.isEmpty() || !now.exists || now.stamp.st_dev != directory->observed.stamp.st_dev || now.stamp.st_ino != directory->observed.stamp.st_ino) {
                error += "Extraction directory changed before finalization: " + now.path + '\n'; continue;
            }
            const auto result = installer.prepareDirectory(directory.key(), FileInstall::capture(directory->nativePath), extractionMetadataFor(entries.value(directory->index)), directory->created);
            if (!result.isEmpty()) { if (!error.isEmpty()) error += '\n'; error += result; }
        }
        error = installer.finish(error); finished = true;
        for (auto &retained : retired) {
            if (retained.published) { retained.folder->setAutoRemove(true); continue; }
            const auto saved = FileInstall::capture(retained.saved), target = FileInstall::capture(retained.original);
            if (!saved.exists) continue;
            const auto restored = target.exists ? std::optional<FileInstallResult>{} : FileInstall::tryMove(saved, target, publication);
            if (!restored || !restored->error.isEmpty()) {
                retained.folder->setAutoRemove(false);
                error += "\nUnpublished previous output retained in: " + retained.folder->path();
            } else { retained.restored = true; retained.folder->setAutoRemove(true); }
        }
        if (!error.isEmpty()) return fail(EIO, error);
        retired.clear();
        return 0;
    }
};
ExtractionSession::ExtractionSession(QString stage, QString root, QString records, QList<ArchiveEntry> entries, OverwriteBroker &broker,
                                     QString removedRoot, bool absolute, QMap<QString, QString> targets)
    : data(std::make_unique<Data>(std::move(stage), std::move(root), std::move(records), entries, broker, std::move(removedRoot), absolute, std::move(targets))) {}
ExtractionSession::~ExtractionSession() = default;
FileSnapshot ExtractionSession::lookup(const NativeFileStateRequest &request) {
    const auto name = data->relative(request.path);
    if (data->finished || name.isEmpty()) { FileSnapshot invalid; invalid.errorCode = EINVAL; invalid.error = "Invalid original output query."; return invalid; }
    auto result = FileInstall::inspect(data->destination(name));
    if (!result.error.isEmpty()) data->diagnostic = "Extraction output refused: " + result.error;
    if (request.probe) data->probes[name] = result;
    else { data->current = name; data->approved = result; data->probes.clear(); }
    return result;
}
int ExtractionSession::apply(const NativeExtractionRequest &request) {
    if (data->finished) return EINVAL;
    if (request.action == "begin") return data->begin(request.item);
    if (request.action == "mutation") return data->mutation(request.item);
    if (request.action == "publish") return data->publish(request.item);
    if (request.action == "seed") {
        const auto name = data->relative(request.item.value("path").toString());
        const auto source = request.item.value("source").toString();
        if (name.isNull() || name.isEmpty() || source != data->destination(name)) return data->fail(EINVAL, "Invalid existing hard-reference mapping.");
        const auto error = data->installer.reference(source, request.item.value("sourceSnapshot").toString());
        return error.isEmpty() ? 0 : data->fail(ESTALE, error);
    }
    if (request.action == "finish") return data->finish();
    return EINVAL;
}
QString ExtractionSession::error() const { return data->diagnostic; }
QString ExtractionSession::publicPath(const QString &nativePath) const { const auto name = data->relative(nativePath); return name.isNull() ? QString() : data->destination(name); }
QString ExtractionSession::close() {
    if (!data->finished) { data->recoverOpenStreams(); data->finish(false); }
    return data->diagnostic;
}
bool ExtractionSession::retainsStaging() const { return data->retainStage; }
