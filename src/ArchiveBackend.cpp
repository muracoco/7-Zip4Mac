#include "ArchiveBackend.h"
#include "MacProcessPriority.h"
#include "ArchiveMetadata.h"
#include "ArchiveProperties.h"
#include "ArchiveFormats.h"
#include "ArchiveSourceStamp.h"
#include "FileInstall.h"
#include "ExtractionSession.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcessEnvironment>
#include <QtConcurrent>
#include <QSet>
#include <sys/stat.h>
#include <fcntl.h>
#include <zlib.h>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QDataStream>
#include <signal.h>

QString operationName(ArchiveOperation op) {
    switch (op) {
    case ArchiveOperation::List: return "Open archive";
    case ArchiveOperation::Add: return "Add to Archive";
    case ArchiveOperation::Extract: return "Extract";
    case ArchiveOperation::Test: return "Test";
    case ArchiveOperation::Delete: return "Delete from archive";
    case ArchiveOperation::Rename: return "Rename in archive";
    case ArchiveOperation::Hash:
    case ArchiveOperation::HashArchive: return "CRC SHA";
    case ArchiveOperation::HashFile: return "SHA-256 -> file.sha256";
    case ArchiveOperation::CreateFolder: return "Create Folder";
    case ArchiveOperation::ReplaceFile: return "Update archive";
    case ArchiveOperation::Comment: return "Comment";
    }
    return {};
}

SevenZipProcessBackend::SevenZipProcessBackend(QString executable, QObject *parent)
    : ArchiveBackend(parent), program(std::move(executable)), progressChannel(&process, this) {
    connect(&extractionWatcher, &QFutureWatcher<ExtractionTaskReply>::finished, this, &SevenZipProcessBackend::extractionWorkerFinished);
    connect(&progressChannel, &ProgressChannel::fileStateRequest, this, [this](NativeFileStateRequest message) {
        if (!extractionSession || extractionWatcher.isRunning()) { FileSnapshot invalid; invalid.errorCode=EBUSY; invalid.error="Extraction session is not ready."; progressChannel.replyFileState(message,invalid); return; }
        const auto session=extractionSession;
        extractionWatcher.setFuture(QtConcurrent::run([session,message] { ExtractionTaskReply reply; reply.state=message; reply.file=session->lookup(message); return reply; }));
    });
    connect(&progressChannel, &ProgressChannel::extractionRequest, this, [this](NativeExtractionRequest message) {
        if (!extractionSession || extractionWatcher.isRunning()) { progressChannel.replyExtraction(message,EBUSY); return; }
        const auto session=extractionSession;
        emit extractionCheckpoint(message.action,session->publicPath(message.item.value("path").toString()));
        extractionWatcher.setFuture(QtConcurrent::run([session,message] { ExtractionTaskReply reply; reply.kind=ExtractionTaskReply::Publication; reply.publication=message; reply.code=session->apply(message); return reply; }));
    });
    connect(&progressChannel, &ProgressChannel::overwriteRequest, this, [this](NativeOverwriteRequest message) {
        if (!extractionSession || extractionWatcher.isRunning()) { progressChannel.replyOverwrite(message,OverwriteAnswer::Cancel); return; }
        const auto session=extractionSession;
        extractionWatcher.setFuture(QtConcurrent::run([this,session,message] {
            ExtractionTaskReply reply; reply.kind=ExtractionTaskReply::Overwrite; reply.overwrite=message;
            auto conflict=message.conflict; conflict.existing.path=session->publicPath(conflict.existing.path);
            reply.answer=conflict.existing.path.isEmpty()?OverwriteAnswer::Cancel:overwriteBroker.ask(conflict); return reply;
        }));
    });
    connect(&progressChannel, &ProgressChannel::snapshot, this, [this](ArchiveProgress snapshot) {
        if (!active || request.operation == ArchiveOperation::List || preflight || checkingSources) return;
        if (!request.password.isEmpty()) { snapshot.current.replace(request.password, "[redacted]"); snapshot.titleFileName.replace(request.password, "[redacted]"); snapshot.status.replace(request.password, "[redacted]"); }
        const auto done = snapshot.processed();
        emit progress(snapshot.percent(), done.value_or(0), snapshot.total.value_or(0), snapshot.current);
        emit progressDetails(snapshot);
    });
    connect(&progressChannel, &ProgressChannel::operationError, this, [this](ArchiveOperationError error) {
        if (!active || request.operation == ArchiveOperation::List || preflight || checkingSources) return;
        operationHadNativeError = true;
        if (!request.password.isEmpty()) { error.fileName.replace(request.password, "[redacted]"); error.archive.replace(request.password, "[redacted]"); error.message.replace(request.password, "[redacted]"); }
        emit operationError(error);
    });
    connect(&process, &QProcess::readyReadStandardOutput, this, &SevenZipProcessBackend::readOutput);
    connect(&process, &QProcess::readyReadStandardError, this, &SevenZipProcessBackend::readOutput);
    connect(&process, &QProcess::finished, this, &SevenZipProcessBackend::processFinished);
    connect(&process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (active && error == QProcess::FailedToStart) complete(false, -1, process.errorString());
    });
    connect(&process, &QProcess::started, this, [this] {
        if (backgroundMode) { const auto error = MacProcessPriority::set(process.processId(), true); if (!error.isEmpty()) emit operationError({-1, false, {}, request.archive, error}); }
        emit pauseAvailabilityChanged(canPause());
        // The official console reads redirected stdin. No secret in argv or environment.
        if (!commentHelperActive && !request.password.isEmpty()) {
            QByteArray secret = request.password.toUtf8() + '\n';
            process.write(secret);
            if ((request.operation == ArchiveOperation::Add && !checkingSources) || mutationPhase == MutationPhase::Update) process.write(secret);
            secret.fill('\0');
        }
        process.closeWriteChannel();
    });
    connect(&mergeWatcher, &QFutureWatcher<QString>::finished, this, [this] {
        if (mutationPhase != MutationPhase::None) { mutationWorkerFinished(); return; }
        cancelled |= stopFlag && stopFlag->isCancelled();
        if (request.operation == ArchiveOperation::Extract) { finishExtract(mergeWatcher.result()); return; }
        complete(mergeWatcher.result().isEmpty() && !cancelled, cancelled ? 255 : mergeWatcher.result().isEmpty() ? 0 : -1, mergeWatcher.result());
    });
    connect(&mergeWatcher, &QFutureWatcher<QString>::started, this, [this] { emit pauseAvailabilityChanged(canPause()); });
    connect(&overwriteBroker, &OverwriteBroker::requested, this, [this](OverwriteConflict conflict) {
        if (active && overwriteBroker.current(conflict.id)) emit overwriteRequested(conflict);
    });
    connect(&overwriteBroker, &OverwriteBroker::waitingChanged, this, [this] { emit pauseAvailabilityChanged(canPause()); });
}
SevenZipProcessBackend::~SevenZipProcessBackend() {
    disconnect(&progressChannel, nullptr, this, nullptr);
    disconnect(&process, nullptr, this, nullptr);
    if (stopFlag) stopFlag->cancel();
    overwriteBroker.cancel();
    process.kill(); process.waitForFinished(3000);
    extractionWatcher.waitForFinished();
    // A window can be destroyed before QProcess::finished is dispatched.
    // Close the worker-owned session after the engine stops so interrupted
    // streams and retired user outputs survive that shutdown path too.
    if (extractionSession) {
        extractionSession->close();
        if (extractionSession->retainsStaging() && staging) staging->setAutoRemove(false);
    }
    mergeWatcher.waitForFinished();
}

bool SevenZipProcessBackend::safeArchivePath(const QString &path) {
    if (path.isEmpty() || path.contains(QChar::Null)) return false;
    QString p = path; p.replace('\\', '/');
    if (p.startsWith('/') || QRegularExpression("^[A-Za-z]:").match(p).hasMatch()) return false;
    for (const auto &part : p.split('/')) if (part == "..") return false;
    return true;
}
QList<ArchiveEntry> SevenZipProcessBackend::parseListing(const QString &text, QString *error) {
    QList<ArchiveEntry> list;
    QMap<QString, QString> fields;
    ArchivePropertyList ordered;
    auto finish = [&] {
        // Single-stream handlers can omit Path, Method and even unpacked Size.
        if (!fields.contains("Path") && !fields.contains("Size") && !fields.contains("Packed Size")) { fields.clear(); ordered.clear(); return; }
        ArchiveEntry e;
        e.path = fields.value("Path"); e.size = fields.value("Size").toULongLong();
        e.packed = fields.value("Packed Size").toULongLong(); e.modified = fields.value("Modified");
        e.attributes = fields.value("Attributes"); e.crc = fields.value("CRC"); e.method = fields.value("Method");
        e.extension = fields.value("Extension");
        e.properties = fields;
        e.orderedProperties = ordered;
        e.directory = fields.value("Folder") == "+" || e.attributes.startsWith('D') || fields.value("Mode").startsWith('d');
        e.encrypted = fields.value("Encrypted") == "+";
        e.link = !fields.value("Symbolic Link").isEmpty() || !fields.value("Hard Link").isEmpty() || !fields.value("Link").isEmpty() || e.attributes.section(' ', 0, 0).contains('L') || e.attributes.contains(" lr") || fields.value("Mode").startsWith('l');
        list.append(e); fields.clear(); ordered.clear();
    };
    for (QString line : text.split('\n')) {
        if (line.endsWith('\r')) line.chop(1);
        if (line.isEmpty()) { finish(); continue; }
        if (line.startsWith("Enter password:")) continue;
        int pos = line.indexOf(" = ");
        if (pos < 1) { if (error) *error = "Unsupported or ambiguous archive listing."; continue; }
        QString key = line.left(pos);
        if (fields.contains(key) && error) *error = "Duplicate fields in archive listing.";
        fields[key] = line.mid(pos + 3);
        ordered.append({key, fields[key]});
    }
    finish();
    return list;
}
QList<ArchiveLayer> SevenZipProcessBackend::parseArchiveLayers(const QString &header) {
    QList<ArchiveLayer> layers; ArchivePropertyList *properties = nullptr;
    // Console/List.cpp::Print_OpenArchive_Props uses -- for each archive and
    // ---- for its main subfile. Neither a repeated key nor a blank line
    // starts a new layer. Keep each provider's property sequence intact.
    const auto lines = header.split('\n');
    for (qsizetype index = 0; index < lines.size(); ++index) {
        QString line = lines[index]; if (line.endsWith('\r')) line.chop(1);
        if (line == "--") { layers.append(ArchiveLayer{}); properties = &layers.last().properties; continue; }
        if (!properties) continue;
        if (line == "----") { properties = &layers.last().childProperties; continue; }
        if (line == "ERRORS:" || line == "WARNINGS:") {
            QStringList messages;
            while (index + 1 < lines.size()) {
                const auto next = lines[index + 1].trimmed();
                if (next.isEmpty() || next.startsWith('-') || next.contains(" = ") || next.endsWith(':') || next.startsWith("Open WARNING:") || next.startsWith("Warning: ")) break;
                messages << lines[++index].trimmed();
            }
            if (!messages.isEmpty()) properties->append({line == "ERRORS:" ? QString("Error Flags") : QString("Warning Flags"), messages.join('\n')});
            continue;
        }
        const QString expectedTypePrefix = "Open WARNING: Cannot open the file as [";
        if (line.startsWith(expectedTypePrefix)) {
            const auto end = line.indexOf(']', expectedTypePrefix.size()); if (end > expectedTypePrefix.size()) properties->append({"Error Type", line.mid(expectedTypePrefix.size(), end - expectedTypePrefix.size())}); continue;
        }
        const auto split = line.indexOf(" = ");
        if (split > 0) {
            ArchiveProperty property{line.left(split), line.mid(split + 3)};
            if (property.name == "ERROR") property.name = "Error";
            if (property.name == "WARNING") property.name = "Warning";
            if (property.value.isEmpty() && index + 1 < lines.size() && lines[index + 1].trimmed() == "{") {
                ++index; QStringList value;
                while (++index < lines.size() && lines[index].trimmed() != "}") value << lines[index];
                property.value = value.join('\n');
            }
            properties->append(property);
        } else if (line == "Warning: The archive is open with offset") properties->append(ArchiveProperty{"Error Type", {}});
        else if (line.startsWith("Warning: ")) properties->append({"Warning", line.mid(9)});
    }
    for (auto &layer : layers) { QString type; for (const auto &property : layer.properties) if (property.name == "Type") type = property.value; for (auto &property : layer.properties) if (property.name == "Error Type" && property.value.isEmpty()) property.value = type; }
    return layers;
}
bool SevenZipProcessBackend::readNativeListing(QList<ArchiveEntry> &items) {
    if (!metadataStaging) return false;
    QFile file(metadataStaging->filePath("metadata.json")); ArchiveResult result;
    QString error;
    if (!file.open(QIODevice::ReadOnly)) error = "Native archive metadata was not produced.";
    else error = parseNativeMetadata(file.readAll(), result);
    if (!error.isEmpty()) { complete(false, -1, error); return false; }
    items = std::move(result.entries); archiveProperties = std::move(result.properties); archiveLayers = std::move(result.layers); archiveMetadata = std::move(result.metadata); archiveType = result.archiveType;
    return true;
}
bool SevenZipProcessBackend::prepareNativeSelection(QProcessEnvironment &environment) {
    const bool verifying = mutationPhase == MutationPhase::VerifyReplacement;
    const auto &selection = verifying ? mutationVerificationSelection : request;
    const bool updating = mutationPhase == MutationPhase::BeforeList || mutationPhase == MutationPhase::Update || verifying;
    if (updating) selectionStaging.reset();
    if (selectionStaging) {
        environment.insert("SEVENZIP_PORT_SELECTION_PATH", selectionStaging->filePath("selection.bin"));
        return true;
    }
    if (selection.selectionSnapshot.isEmpty() || selection.selectionType.isEmpty() || selection.selectionEntryCount < 0 || selection.selectionDirectory < 0) {
        complete(false, -1, "Archive selection has no native snapshot. Refresh the archive."); return false;
    }
    selectionStaging = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/.7zip-selection-XXXXXX");
    QFile file(selectionStaging->filePath("selection.bin"));
    if (!selectionStaging->isValid() || !file.open(QIODevice::WriteOnly | QIODevice::NewOnly) ||
        !file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) {
        complete(false, -1, "Cannot create private archive selection request."); return false;
    }
    QDataStream stream(&file); stream.setByteOrder(QDataStream::LittleEndian);
    stream.writeRawData("7ZSEL001", 8);
    auto text = [&](const QString &value) { const auto bytes = value.toUtf8(); stream << quint32(bytes.size()); stream.writeRawData(bytes.constData(), bytes.size()); };
    QString snapshot = selection.selectionSnapshot;
    if (updating) {
        struct stat staged{};
        if (::stat(QFile::encodeName(mutationCandidate).constData(), &staged) != 0 || !S_ISREG(staged.st_mode)) {
            complete(false, -1, "Cannot bind selection to the staged archive; original retained."); return false;
        }
        snapshot = QString::fromStdString(archiveSourceStamp(staged));
    }
    text(snapshot); text(selection.selectionType);
    stream << quint32(selection.selectionEntryCount) << quint32(selection.selectionDirectory)
           << quint32((selection.selectionFlat ? 1 : 0) | (selection.selectionPreservePaths ? 2 : 0) | (selection.operation == ArchiveOperation::Rename ? 4 : 0) | (selection.operation == ArchiveOperation::Comment ? 8 : 0) | (selection.operation == ArchiveOperation::ReplaceFile ? 16 : 0)) << quint32(selection.selection.size());
    for (const auto &item : selection.selection) {
        if (item.identity.directory < 0 || item.identity.item < 0 || item.archiveIndex < -1 || item.archiveIndex >= selection.selectionEntryCount || item.name.contains(QChar::Null)) {
            complete(false, -1, "Invalid archive item identity. Refresh the archive."); return false;
        }
        stream << quint32(item.identity.directory) << quint32(item.identity.item) << quint32(item.archiveIndex);
        text(item.name);
    }
    if (stream.status() != QDataStream::Ok || !file.flush()) { complete(false, -1, "Cannot write private archive selection request."); return false; }
    file.close(); environment.insert("SEVENZIP_PORT_SELECTION_PATH", file.fileName()); return true;
}
QStringList SevenZipProcessBackend::temporaryPaths() const {
    QStringList paths;
    for (const auto temp : {staging.get(), selectionStaging.get(), metadataStaging.get(), extractMetadataStaging.get()}) if (temp) paths << temp->path();
    return paths;
}
void SevenZipProcessBackend::launch(QStringList args, QString working) {
    commentHelperActive = false;
    lastProcessArguments = args; lastProcessWorking = working;
    stdoutDecoder = QStringDecoder(QStringDecoder::Utf8); stderrDecoder = QStringDecoder(QStringDecoder::Utf8); textOutput.clear(); textErrors.clear();
    passwordOutputTail.clear(); passwordErrorTail.clear(); passwordDiagnostic = false;
    QStringList common{"-sccUTF-8", "-scsUTF-8", "-spd", "-bse2"};
    const bool reading = !args.isEmpty() && QStringList{"l", "t", "x", "e"}.contains(args.first());
    if (reading && !request.readMode.isEmpty()) common << "-t" + request.readMode;
    else if (!forcedReadFormat.isEmpty()) common << "-t" + forcedReadFormat;
    if (request.memoryLimitGB > 0 && (request.operation == ArchiveOperation::Extract || request.operation == ArchiveOperation::Test || request.operation == ArchiveOperation::HashArchive || mutationPhase == MutationPhase::VerifyReplacement || mutationPhase == MutationPhase::VerifyEmptyStream))
        common << "-smemx" + QString::number(request.memoryLimitGB) + "g";
    // Read/test/extract must omit -p: on those commands bare -p means an empty
    // password. Add/update uses bare -p to ask via redirected stdin.
    if (!request.password.isEmpty() && request.operation == ArchiveOperation::Add && !checkingSources) common << "-p";
    int separator = args.indexOf("--");
    if (separator < 0) separator = args.size();
    for (const auto &s : common) args.insert(separator++, s);
    process.setWorkingDirectory(working);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.setStandardOutputFile(mutationPhase == MutationPhase::VerifyReplacement ? staging->filePath("replacement-verify-data") : QString());
    const QString helper = QFileInfo(program).absolutePath() + "/7zz-progress";
    const bool enhanced = QFileInfo(program).fileName() == "7zz" && QFileInfo(helper).isExecutable();
    const bool transfer = request.operation == ArchiveOperation::Add && request.useArchiveDefaults && !checkingSources;
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove("SEVENZIP_PORT_COMPLETION_PATH"); completionStaging.reset();
    environment.remove("SEVENZIP_PORT_OVERWRITE_RPC");
    environment.remove("SEVENZIP_PORT_OUTPUT_STATE_RPC");
    environment.remove("SEVENZIP_PORT_EXTRACTION_RPC");
    environment.remove("SEVENZIP_PORT_COPY_FROM"); environment.remove("SEVENZIP_PORT_ADD_PREFIX");
    environment.remove("SEVENZIP_PORT_COPY_INPUT"); transferInputStaging.reset();
    environment.remove("SEVENZIP_PORT_CREATE_FOLDER"); environment.remove("SEVENZIP_PORT_FOLDER_OUTPUT");
    environment.remove("SEVENZIP_PORT_ITEM_UPDATE"); environment.remove("SEVENZIP_PORT_RENAME_ITEM");
    environment.remove("SEVENZIP_PORT_COMMENT_PATH");
    environment.remove("SEVENZIP_PORT_REPLACEMENT_PATH"); environment.remove("SEVENZIP_PORT_UPDATE_PASSWORD");
    environment.remove("SEVENZIP_PORT_METADATA_PATH"); metadataStaging.reset();
    environment.remove("SEVENZIP_PORT_SELECTION_PATH");
    environment.remove("SEVENZIP_PORT_EXTRACTION_PATH");
    environment.remove("SEVENZIP_PORT_HARDLINK_MAP");
    const bool selecting = mutationPhase == MutationPhase::VerifyReplacement || (request.selectionDirectory >= 0 &&
        (mutationPhase == MutationPhase::None || mutationPhase == MutationPhase::BeforeList || mutationPhase == MutationPhase::Update));
    if (selecting) {
        if (!enhanced) { complete(false, -1, "Bundled native selection adapter is missing."); return; }
        if (!prepareNativeSelection(environment)) return;
    }
    const bool folder = request.operation == ArchiveOperation::CreateFolder && mutationPhase == MutationPhase::Update;
    const bool itemUpdate = selecting && mutationPhase == MutationPhase::Update &&
        (request.operation == ArchiveOperation::Delete || request.operation == ArchiveOperation::Rename || request.operation == ArchiveOperation::Comment || request.operation == ArchiveOperation::ReplaceFile);
    if (enhanced && !preflight && (request.operation == ArchiveOperation::Test || request.operation == ArchiveOperation::Hash || request.operation == ArchiveOperation::HashArchive)) {
        completionStaging = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/.7zip-completion-XXXXXX");
        if (!completionStaging->isValid()) { complete(false, -1, "Cannot prepare native operation results."); return; }
        environment.insert("SEVENZIP_PORT_COMPLETION_PATH", completionStaging->filePath("result.json"));
    }
    const bool metadata = enhanced && !folder && !itemUpdate && !args.isEmpty() && args.first() == "l";
    if (metadata) {
        metadataStaging = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/.7zip-metadata-XXXXXX");
        if (!metadataStaging->isValid()) { complete(false, -1, "Cannot create private metadata directory."); return; }
        environment.insert("SEVENZIP_PORT_METADATA_PATH", metadataStaging->filePath("metadata.json"));
    }
    if (request.operation == ArchiveOperation::Extract && !preflight) {
        if (!enhanced) { complete(false, -1, "Bundled native extraction adapter is missing."); return; }
        extractMetadataStaging = std::make_unique<QTemporaryDir>(QFileInfo(QDir::tempPath()).canonicalFilePath() + "/.7zip-extraction-records-XXXXXX");
        if (!extractMetadataStaging->isValid()) { complete(false, -1, "Cannot prepare extraction output records."); return; }
        environment.insert("SEVENZIP_PORT_EXTRACTION_PATH", extractMetadataStaging->filePath("outputs.jsonl"));
        QFile map(extractMetadataStaging->filePath("hard-links.txt"));
        QByteArray rules = "7ZHARD01\n" + staging->path().toUtf8().toHex() + '\n' + request.outputDirectory.toUtf8().toHex() + '\n' + (request.pathMode == "absolute" ? "1\n" : "0\n");
        for (auto iter = extractHardTargets.cbegin(); iter != extractHardTargets.cend(); ++iter) rules += iter.key().toUtf8().toHex() + ' ' + iter.value().toUtf8().toHex() + '\n';
        if (!map.open(QIODevice::WriteOnly | QIODevice::NewOnly) || map.write(rules) != rules.size() || !map.flush()) { complete(false, -1, "Cannot prepare official hard-link target mapping."); return; }
        map.close(); environment.insert("SEVENZIP_PORT_HARDLINK_MAP", map.fileName());
        extractionSession = std::make_shared<ExtractionSession>(staging->path(), request.outputDirectory, extractMetadataStaging->path(), entries,
            overwriteBroker, extractRootPrefix, request.pathMode == "absolute", extractTargets);
        environment.insert("SEVENZIP_PORT_OUTPUT_STATE_RPC", "1");
        environment.insert("SEVENZIP_PORT_OVERWRITE_RPC", "1");
        environment.insert("SEVENZIP_PORT_EXTRACTION_RPC", "1");
    }
    if (transfer) {
        if (!enhanced) { complete(false, -1, "Bundled archive transfer adapter is missing."); return; }
        transferInputStaging = std::make_unique<QTemporaryDir>(QDir::tempPath() + "/.7zip-copy-input-XXXXXX");
        if (!transferInputStaging->isValid()) { complete(false, -1, "Cannot prepare original CopyFrom inputs."); return; }
        QFile input(transferInputStaging->filePath("inputs.bin"));
        if (!input.open(QIODevice::WriteOnly | QIODevice::NewOnly) || !input.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            complete(false, -1, "Cannot create private CopyFrom input vector."); return;
        }
        QDataStream stream(&input); stream.setByteOrder(QDataStream::LittleEndian); stream.writeRawData("7ZCPY001", 8);
        auto writeText = [&](const QString &value) { const auto bytes = value.toUtf8(); stream << quint32(bytes.size()); stream.writeRawData(bytes.constData(), bytes.size()); };
        // EnumerateItems2 concatenates physical base + requested path. Bind
        // normalized absolute inputs to an empty base, as original Agent drop
        // callers do. Its logical paths still discard each selected prefix.
        writeText(QString());
        stream << quint32(request.files.size()); for (const auto &name : request.files) writeText(name);
        if (stream.status() != QDataStream::Ok || !input.flush()) { complete(false, -1, "Cannot write private CopyFrom inputs."); return; }
        input.close(); environment.insert("SEVENZIP_PORT_COPY_INPUT", input.fileName());
        environment.insert("SEVENZIP_PORT_COPY_FROM", "1"); environment.insert("SEVENZIP_PORT_ADD_PREFIX", request.archivePrefix);
    }
    if (folder) {
        if (!enhanced) { complete(false, -1, "Bundled native folder update adapter is missing."); return; }
        environment.insert("SEVENZIP_PORT_CREATE_FOLDER", request.files.first());
        environment.insert("SEVENZIP_PORT_FOLDER_OUTPUT", mutationCandidate + ".updated");
    }
    if (itemUpdate) {
        environment.insert("SEVENZIP_PORT_ITEM_UPDATE", request.operation == ArchiveOperation::Delete ? "delete" : request.operation == ArchiveOperation::Comment ? "comment" : request.operation == ArchiveOperation::ReplaceFile ? "replace" : "rename");
        if (request.operation == ArchiveOperation::Rename) environment.insert("SEVENZIP_PORT_RENAME_ITEM", request.files[1].mid(mutationSelectedPath.size() - request.selection.first().name.size()));
        if (request.operation == ArchiveOperation::Comment) environment.insert("SEVENZIP_PORT_COMMENT_PATH", staging->filePath("comment.txt"));
        if (request.operation == ArchiveOperation::ReplaceFile) {
            environment.insert("SEVENZIP_PORT_REPLACEMENT_PATH", QDir(mutationInput).filePath(request.files[0]));
            if (!request.password.isEmpty()) environment.insert("SEVENZIP_PORT_UPDATE_PASSWORD", "1");
        }
        environment.insert("SEVENZIP_PORT_FOLDER_OUTPUT", mutationCandidate + ".updated");
    }
    process.setProcessEnvironment(environment);
    const bool native = enhanced && progressChannel.prepare(true);
    if (!native) progressChannel.prepare(false);
    if (native && request.operation != ArchiveOperation::List && !preflight) emit progressDetails(ArchiveProgress{});
    process.start(native || transfer || metadata || folder || itemUpdate || selecting ? helper : program, args);
}
void SevenZipProcessBackend::launchCommentHelper(QStringList args) {
    progressChannel.prepare(false);
    commentHelperActive = true;
    stdoutDecoder = QStringDecoder(QStringDecoder::Utf8); stderrDecoder = QStringDecoder(QStringDecoder::Utf8); textOutput.clear(); textErrors.clear();
    process.setWorkingDirectory({}); process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start(QFileInfo(program).absolutePath() + "/7zip-comment", args);
}
bool SevenZipProcessBackend::applyZipComments(QList<ArchiveEntry> &items) {
    QJsonParseError error; const auto document = QJsonDocument::fromJson(textOutput.toUtf8(), &error); textOutput.clear();
    if (error.error != QJsonParseError::NoError || !document.isArray() || stdoutDecoder.hasError() || document.array().size() != items.size()) { complete(false, -1, "Cannot read ZIP comments unambiguously."); return false; }
    const auto values = document.array();
    for (qsizetype index = 0; index < items.size(); ++index) {
        auto &entry = items[index]; const auto value = values[index].toObject(); const auto path = value.value("path");
        if (!path.isString() || (path.toString() != entry.path && (!entry.directory || path.toString() != entry.path + '/')) || !value.value("comment").isString()) { complete(false, -1, "ZIP entry names changed while reading comments."); return false; }
        entry.properties["Comment"] = value.value("comment").toString();
        bool found = false; for (auto &property : entry.orderedProperties) if (property.name == "Comment") { property.value = entry.properties["Comment"]; found = true; }
        if (!found) entry.orderedProperties.append({"Comment", entry.properties["Comment"]});
    }
    return true;
}
void SevenZipProcessBackend::start(const ArchiveRequest &r) {
    if (active) return;
    request = r; active = true; cancelled = false; preflight = false; overwriteBroker.reset();
    extractExitCode = 0; extractResultMessage.clear();
    extractionSession.reset(); extractionExit.reset();
    creating = false; entries.clear(); extractRootPrefix.clear(); extractTargets.clear(); extractHardTargets.clear(); staging.reset(); selectionStaging.reset(); extractMetadataStaging.reset(); totalSize = request.totalSizeHint; currentFile.clear();
    checkingSources = false; sourceAccessTimes.clear();
    mutationPhase = MutationPhase::None; mutationInstalled = false; mutationCandidate.clear(); mutationInput.clear(); mutationDigest.clear(); replacementDigest.clear(); replacementSize = mutationPrefixSize = 0; mutationOriginalEntries.clear();
    mutationSelectedIndices.clear(); mutationSelectedPath.clear();
    mutationItemIndexMap.clear(); mutationOriginalMetadata = {}; mutationVerificationCandidates.clear(); mutationVerificationCursor = 0; mutationVerificationSelection = {};
    forcedReadFormat.clear();
    resolvingNames = false; archiveType.clear(); archiveProperties.clear(); archiveLayers.clear(); archiveMetadata = {}; metadataStaging.reset();
    commentHelperActive = readingZipComments = zipCommentsRead = false;
    stopFlag = std::make_shared<OperationControl>(); operationHadNativeError = false;
    if (!ArchiveFormats::openTypes().contains(request.readMode)) {
        QTimer::singleShot(0, this, [this] { complete(false, -1, "Unsupported archive open mode."); }); return;
    }
    if (request.password.contains('\n') || request.password.contains('\r') || request.password.contains(QChar::Null)) {
        QTimer::singleShot(0, this, [this] { complete(false, -1, "Password contains an unsupported control character."); }); return;
    }
    if (!QFileInfo(program).isExecutable()) {
        QTimer::singleShot(0, this, [this] { complete(false, -1, "Bundled 7zz executable is missing or not executable."); }); return;
    }
    request.archive = QFileInfo(request.archive).absoluteFilePath();
    if (request.selectionDirectory >= 0 && request.operation != ArchiveOperation::List && request.operation != ArchiveOperation::Extract && request.operation != ArchiveOperation::Test && request.operation != ArchiveOperation::HashArchive && request.operation != ArchiveOperation::Delete && request.operation != ArchiveOperation::Rename && request.operation != ArchiveOperation::Comment && request.operation != ArchiveOperation::ReplaceFile) {
        QTimer::singleShot(0, this, [this] { complete(false, -1, "Native selection is not connected to this operation yet."); }); return;
    }
    QStringList args;
    switch (request.operation) {
    case ArchiveOperation::CreateFolder:
    case ArchiveOperation::Comment:
    case ArchiveOperation::ReplaceFile: beginArchiveMutation(); return;
    case ArchiveOperation::List:
    case ArchiveOperation::Extract:
        preflight = request.operation == ArchiveOperation::Extract;
        launch({"l", "-slt", "--", request.archive}); return;
    case ArchiveOperation::Test:
        if (request.selectionDirectory >= 0) { preflight = true; launch({"l", "-slt", "--", request.archive}); return; }
        args << "t"; if (request.hashTest) args << "-tHash"; break;
    case ArchiveOperation::Hash:
    case ArchiveOperation::HashArchive:
        if ((request.files.isEmpty() && request.selectionDirectory < 0) || !QStringList{"CRC32", "CRC64", "XXH64", "MD5", "SHA1", "SHA256", "SHA384", "SHA512", "SHA3-256", "BLAKE2sp", "*"}.contains(request.hashMethod)) {
            QTimer::singleShot(0, this, [this] { complete(false, -1, "Choose files and a supported checksum method."); }); return;
        }
        if (request.operation == ArchiveOperation::HashArchive && request.selectionDirectory >= 0) { preflight = true; launch({"l", "-slt", "--", request.archive}); }
        else if (request.operation == ArchiveOperation::HashArchive) launch(QStringList{"t", "-scrc" + request.hashMethod, "-bsp1", "-bb1", "--", request.archive} + request.files);
        else launch(QStringList{"h", "-scrc" + request.hashMethod, "-bsp1", "--"} + request.files, request.workingDirectory);
        return;
    case ArchiveOperation::HashFile: {
        if (request.files.isEmpty() || QFileInfo::exists(request.archive) || QFileInfo(request.archive).isSymLink()) { QTimer::singleShot(0, this, [this] { complete(false, -1, "Checksum output already exists or no files were selected: " + request.archive); }); return; }
        creating = true; staging = std::make_unique<QTemporaryDir>(QFileInfo(request.archive).absolutePath() + "/.7zip-hash-XXXXXX");
        if (!staging->isValid()) { QTimer::singleShot(0, this, [this] { complete(false, -1, "Cannot create checksum output staging directory."); }); return; }
        // Official Hash archive handler implements generation (h itself does not).
        launch(QStringList{"a", "-tHash", "-mm=SHA256", "-bsp1", "--", staging->filePath(QFileInfo(request.archive).fileName())} + request.files, request.workingDirectory); return;
    }
    case ArchiveOperation::Delete:
        if (request.selectionDirectory >= 0) { beginArchiveMutation(); return; }
        if (request.files.isEmpty()) { QTimer::singleShot(0, this, [this] { complete(false, -1, "No archive items selected."); }); return; }
        args << "d"; break;
    case ArchiveOperation::Rename:
        if (request.selectionDirectory >= 0) { beginArchiveMutation(); return; }
        if (request.files.size() != 2 || !safeArchivePath(request.files[1])) { QTimer::singleShot(0, this, [this] { complete(false, -1, "Invalid archive rename."); }); return; }
        args << "rn"; break;
    case ArchiveOperation::Add: {
        if ((!request.archivePrefix.isEmpty() && (!request.useArchiveDefaults || !safeArchivePath(request.archivePrefix) ||
            !request.archivePrefix.endsWith('/') || request.archivePrefix.contains("//") || request.archivePrefix.split('/').contains("."))) ||
            (request.useArchiveDefaults && (request.pathMode != "relative" || !request.volume.isEmpty()))) {
            QTimer::singleShot(0, this, [this] { complete(false, -1, "Invalid archive transfer folder or path mode."); }); return;
        }
        if (request.useArchiveDefaults) for (auto &file : request.files)
            file = QDir::cleanPath(QDir(request.workingDirectory).absoluteFilePath(file));
        if (request.format == "zip") {
            for (auto c : request.password) if (c.unicode() > 127) {
                QTimer::singleShot(0, this, [this] { complete(false, -1, "ZIP passwords must use English letters, numbers and ASCII special characters (7-Zip limitation)."); }); return;
            }
        }
        if (request.files.isEmpty() || !QStringList{"7z", "zip", "tar", "wim", "xz", "gzip", "bzip2", "Hash"}.contains(request.format)) {
            QTimer::singleShot(0, this, [this] { complete(false, -1, "Choose files and a supported writable archive format."); }); return;
        }
        if (QStringList{"xz", "gzip", "bzip2"}.contains(request.format) && (request.files.size() != 1 || !QFileInfo(QDir(request.workingDirectory).absoluteFilePath(request.files.first())).isFile())) { QTimer::singleShot(0, this, [this] { complete(false, -1, "This stream format accepts one regular file. Use 7z, ZIP or TAR for folders and multiple files."); }); return; }
        if (QFileInfo(request.archive).isSymLink()) {
            QTimer::singleShot(0, this, [this] { complete(false, -1, "Refusing to update a symbolic link."); }); return;
        }
        creating = !QFileInfo::exists(request.archive);
        if (creating) {
            const QString root = request.temporaryDirectory.isEmpty() ? QFileInfo(request.archive).absolutePath() : request.temporaryDirectory;
            staging = std::make_unique<QTemporaryDir>(root + "/.7zip-add-XXXXXX");
            if (!staging->isValid()) { QTimer::singleShot(0, this, [this] { complete(false, -1, "Cannot create archive output staging directory."); }); return; }
        } else if (!request.volume.isEmpty()) {
            QTimer::singleShot(0, this, [this] { complete(false, -1, "Updating split archives is not supported."); }); return;
        }
        if (request.preserveAccessTime) {
            auto remember = [this](QString path) { struct stat info; const auto bytes = QFile::encodeName(path); if (::lstat(bytes.constData(), &info) == 0) sourceAccessTimes.append({path, info.st_atimespec.tv_sec, info.st_atimespec.tv_nsec}); };
            for (const auto &name : request.files) { const QString path = QDir(request.workingDirectory).absoluteFilePath(name); remember(path); if (QFileInfo(path).isDir() && !QFileInfo(path).isSymLink()) { QDirIterator it(path, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories); while (it.hasNext()) remember(it.next()); } }
        }
        if (request.format == "Hash" && (request.deleteAfter || !QStringList{"SHA256", "SHA1"}.contains(request.method))) { QTimer::singleShot(0, this, [this] { complete(false, -1, "Hash format supports SHA256 / SHA1 and retains source files."); }); return; }
        if (request.useArchiveDefaults) {
            // ArchiveFolderOut::CopyFrom resets ISetProperties and uses Add.
            // Do not override method/level/header encryption/ZIP AES defaults.
            args << "a" << "-t" + request.format;
            if (!request.temporaryDirectory.isEmpty()) args << "-w" + request.temporaryDirectory;
            break;
        }
        args << (request.updateMode == "add" ? "a" : "u") << (request.storeSymbolicLinks ? "-snl" : "-snl-") << "-t" + request.format;
        if (request.format != "Hash") args << "-mx=" + QString::number(request.level);
        if (request.format != "Hash" && request.threads > 0) args << "-mmt=" + QString::number(request.threads);
        if (!request.temporaryDirectory.isEmpty()) args << "-w" + request.temporaryDirectory;
        if (request.pathMode == "full") args << "-spf2"; else if (request.pathMode == "absolute") args << "-spf";
        if (request.storeHardLinks) args << "-snh";
        if (request.timestampPrecision >= 0 && request.format != "Hash") args << "-mtp=" + QString::number(request.timestampPrecision);
        for (auto value : {qMakePair(QString("tm"), request.modificationTime), qMakePair(QString("tc"), request.creationTime), qMakePair(QString("ta"), request.accessTime)}) if (value.second >= 0 && request.format != "Hash") args << "-m" + value.first + '=' + (value.second ? "on" : "off");
        if (request.latestArchiveTime) args << "-stl";
        // The official console accepts -stl only; explicit false uses its
        // default (switch omitted), while the GUI retains the specified pair.
        if (!request.compressionMemory.isEmpty()) {
            if (!QRegularExpression("^(?:[1-9][0-9]?|100)%$|^[1-9][0-9]*[kmgt]$", QRegularExpression::CaseInsensitiveOption).match(request.compressionMemory).hasMatch()) { QTimer::singleShot(0, this, [this] { complete(false, -1, "Invalid compression memory limit."); }); return; }
            args << "-mmemuse=" + request.compressionMemory;
        }
        if (request.updateMode == "fresh") args << "-up1q1r0x1y1z1w1";
        if (request.updateMode == "sync") args << "-up1q0r2x1y2z1w2";
        auto property = [&args](const QString &key, const QString &value) { if (!value.isEmpty()) args << "-m" + key + '=' + value; };
        if (request.format == "7z") {
            if (!request.methodAutomatic) property("0", request.method);
            if (request.level != 0 && (request.method == "LZMA2" || request.method == "LZMA")) { property("d", request.dictionary); property("fb", request.wordSize); }
            if (request.level != 0 && request.method == "PPMd") { property("0mem", request.dictionary); property("0o", request.wordSize); }
            if (request.level != 0 && request.method == "BZip2") property("0d", request.dictionary);
            if (request.level != 0 && request.method.startsWith("Deflate")) property("0fb", request.wordSize);
            property("s", request.solid);
            if (request.encryptNames && !request.password.isEmpty()) args << "-mhe=on";
        } else if (request.format == "zip") {
            if (!request.methodAutomatic) property("m", request.method);
            if (request.level != 0 && request.method == "LZMA") { property("d", request.dictionary); property("fb", request.wordSize); }
            if (request.level != 0 && request.method == "PPMd") { property("mem", request.dictionary); property("o", request.wordSize); }
            if (request.level != 0 && request.method == "BZip2") property("d", request.dictionary);
            if (request.level != 0 && request.method.startsWith("Deflate")) property("fb", request.wordSize);
            if (!request.password.isEmpty()) args << "-mem=" + request.encryptionMethod;
        } else if (request.format == "xz") {
            if (!request.methodAutomatic) property("0", "LZMA2"); property("d", request.dictionary); property("fb", request.wordSize); property("s", request.solid);
        } else if (request.format == "gzip") {
            property("fb", request.wordSize);
        } else if (request.format == "bzip2") {
            property("d", request.dictionary);
        } else if (request.format == "tar") {
            args << "-mm=" + request.method;
        } else if (request.format == "Hash") {
            args << "-mm=" + request.method;
        }
        for (const auto &parameter : QProcess::splitCommand(request.parameters)) {
            if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.]*(?:=[^\\x00\\r\\n]*)?$").match(parameter).hasMatch() || parameter.startsWith("password", Qt::CaseInsensitive) || parameter.startsWith("p=", Qt::CaseInsensitive)) { QTimer::singleShot(0, this, [this] { complete(false, -1, "Parameters must contain compression properties, not command-line switches or passwords."); }); return; }
            args << "-m" + parameter;
        }
        if (!request.volume.isEmpty()) {
            if (!QRegularExpression("^[1-9][0-9]*[bkmg]?$", QRegularExpression::CaseInsensitiveOption).match(request.volume).hasMatch()) {
                QTimer::singleShot(0, this, [this] { complete(false, -1, "Invalid volume size. Example: 100m."); }); return;
            }
            args << "-v" + request.volume;
        }
        break;
    }
    }
    args << "-bsp1" << "-bb1" << "--";
    args << (creating ? staging->filePath(QFileInfo(request.archive).fileName()) : request.archive);
    for (const auto &file : request.files) {
        QString argument = file;
        if (request.operation == ArchiveOperation::Add && request.pathMode != "relative") { QFileInfo source(QDir(request.workingDirectory).absoluteFilePath(file)); argument = QFileInfo(source.absolutePath()).canonicalFilePath() + '/' + source.fileName(); }
        args << argument;
    }
    launch(args, request.workingDirectory);
}
void SevenZipProcessBackend::readOutput() {
    QString out = stdoutDecoder(process.readAllStandardOutput());
    QString err = stderrDecoder(process.readAllStandardError());
    if (commentHelperActive) {
        // Structured properties are data, never progress logs. Preserve values
        // containing a password string without leaking it into diagnostics.
        if (!request.password.isEmpty()) err.replace(request.password, "[redacted]");
        textOutput += out; textErrors += err; return;
    }
    // Only classify actual console diagnostics. Filenames containing the word
    // password are not authentication errors. Tails handle fragmented UTF-8
    // process output and are never forwarded to logs, results or preferences.
    passwordOutputTail += out; passwordErrorTail += err;
    static const QRegularExpression prompt("(?:^|[\\r\\n])Enter password:");
    static const QRegularExpression wrong("Data Error in encrypted file\\. Wrong password|(?:Cannot|Can not) open encrypted archive\\. Wrong password|(?:^|[\\r\\n])ERROR:[ \\t]*Wrong password(?:[ :.]|$)");
    passwordDiagnostic |= (request.password.isEmpty() && prompt.match(passwordOutputTail).hasMatch()) || wrong.match(passwordErrorTail).hasMatch();
    passwordOutputTail = passwordOutputTail.right(512); passwordErrorTail = passwordErrorTail.right(512);
    // Even if an upstream diagnostic echoes input, do not keep passwords in logs/results.
    if (!request.password.isEmpty()) { out.replace(request.password, "[redacted]"); err.replace(request.password, "[redacted]"); }
    textOutput += out; textErrors += err;
    if (request.operation != ArchiveOperation::List && !preflight && !checkingSources) {
        emit output(out + err);
        if (totalSize == 0 && (request.operation == ArchiveOperation::Add || request.operation == ArchiveOperation::Hash || request.operation == ArchiveOperation::HashFile)) {
            auto sizes = QRegularExpression("(\\d+) bytes").globalMatch(textOutput.left(8192));
            while (sizes.hasNext()) totalSize = sizes.next().captured(1).toULongLong();
        }
        static const QRegularExpression percent("(\\d{1,3})%[^\\r\\n]*");
        auto it = percent.globalMatch(textOutput.right(8192)); int value = -1;
        while (it.hasNext()) { auto m = it.next(); value = qMin(100, m.captured(1).toInt()); currentFile = m.captured(0); }
        if (value >= 0 && !progressChannel.hasSnapshot()) emit progress(value, totalSize * quint64(value) / 100, totalSize, currentFile);
    }
    const bool listing = request.operation == ArchiveOperation::List || preflight || mutationPhase == MutationPhase::BeforeList || mutationPhase == MutationPhase::AfterList;
    if (textOutput.size() > 2 * 1024 * 1024 && !listing) textOutput = textOutput.right(1024 * 1024);
    if (textErrors.size() > 1024 * 1024) textErrors = textErrors.right(1024 * 1024);
}
void SevenZipProcessBackend::processFinished(int code, QProcess::ExitStatus status) {
    if (extractionSession && active) {
        readOutput();
        extractionExit=std::make_pair(code,status);
        if (!extractionWatcher.isRunning()) closeExtractionSession();
        return;
    }
    paused = false; if (stopFlag) stopFlag->setPaused(false);
    emit pausedChanged(false); emit pauseAvailabilityChanged(false);
    if (!active) return;
    readOutput();
    if (cancelled || status == QProcess::CrashExit || code != 0) {
        // Pre-archive readers (notably Mach-O) can be skipped by console's
        // automatic search. Retry the one registered extension handler after
        // auto-detection fails; retain signature-based detection as the first try.
        if (!commentHelperActive && !operationHadNativeError && request.readMode.isEmpty() && !cancelled && status == QProcess::NormalExit && code == 2 && forcedReadFormat.isEmpty() &&
            (request.operation == ArchiveOperation::List || request.operation == ArchiveOperation::Test || request.operation == ArchiveOperation::HashArchive || preflight)) {
            QStringList candidates; const auto extension = QFileInfo(request.archive).suffix().toLower();
            for (const auto &format : ArchiveFormats::all()) if (format.extensions.contains(extension)) candidates << format.name;
            if (candidates.size() == 1) { forcedReadFormat = candidates.first(); launch(lastProcessArguments, lastProcessWorking); return; }
        }
        const bool dangerousLink = request.operation == ArchiveOperation::Extract && (textErrors.contains("Dangerous link") || textErrors.contains("Hard link to symbolic link"));
        if (request.operation == ArchiveOperation::Extract && !preflight && staging && !cancelled && status == QProcess::NormalExit) {
            extractExitCode = code;
            extractResultMessage = dangerousLink ? "Unsafe archive link refused by 7-Zip." : "7-Zip reported an error or warning. Extracted files may be incomplete.";
            commitExtract(); return;
        }
        complete(false, code, cancelled ? "Operation cancelled." : dangerousLink ? "Unsafe archive link refused by 7-Zip." : "7-Zip reported an error or warning."); return;
    }
    if (readingZipComments) {
        readingZipComments = false; commentHelperActive = false;
        if (!applyZipComments(entries)) return;
        zipCommentsRead = true; finishListing(); return;
    }
    if (mutationPhase != MutationPhase::None) { mutationProcessFinished(); return; }
    if (checkingSources) {
        QString error; QList<ArchiveEntry> archiveItems;
        if (metadataStaging) { if (!readNativeListing(archiveItems)) return; }
        else archiveItems = parseListing(textOutput, &error);
        if (archiveItems.size() == 1 && archiveItems.first().path.isEmpty() && QStringList{"xz", "gzip", "bzip2"}.contains(request.format)) archiveItems.first().path = QFileInfo(request.archive).completeBaseName(); if (!error.isEmpty()) { complete(false, -1, error); return; }
        for (const auto &stamp : sourceAccessTimes) { const timespec times[2] = {{time_t(stamp.seconds), stamp.nanos}, {0, UTIME_OMIT}}; const auto bytes = QFile::encodeName(stamp.path); if (::utimensat(AT_FDCWD, bytes.constData(), times, AT_SYMLINK_NOFOLLOW) != 0) { complete(false, -1, "Cannot preserve source access time before moving to Trash: " + stamp.path); return; } }
        verifySources(archiveItems); return;
    }
    if (request.operation == ArchiveOperation::List || preflight) {
        if (metadataStaging) {
            if (!readNativeListing(entries)) return;
            zipCommentsRead = true; // Native handler values include complete ZIP comments.
            finishListing(); return;
        }
        if (resolvingNames) {
            // List.cpp's standard fields are 19 / 5 / 12 / 12 characters,
            // with 1 / 1 / 1 / 2 separating spaces. The console calculates
            // default names through GetItem_Path2, including hidden Extension
            // properties (FLV). Technical listings omit those generated paths.
            static const QRegularExpression row("^.{19} .{5} ([ 0-9]{12}) ([ 0-9]{12})  (.+)$");
            QStringList names;
            for (const auto &line : textOutput.split('\n')) {
                if (line.isEmpty() || line.startsWith("Enter password:")) continue;
                auto match = row.match(line);
                if (!match.hasMatch()) { complete(false, -1, "Cannot resolve generated archive filenames unambiguously."); return; }
                const int index = names.size();
                if (index >= entries.size() || (!match.captured(1).trimmed().isEmpty() && match.captured(1).trimmed().toULongLong() != entries[index].size)) { complete(false, -1, "Archive listing changed while resolving filenames."); return; }
                names << match.captured(3);
            }
            if (names.size() != entries.size()) { complete(false, -1, "Archive filename count does not match the technical listing."); return; }
            for (int n = 0; n < entries.size(); ++n) { if (!entries[n].path.isEmpty() && entries[n].path != names[n]) { complete(false, -1, "Archive paths changed while resolving filenames."); return; } entries[n].path = names[n]; entries[n].properties["Path"] = names[n];
                bool found = false; for (auto &property : entries[n].orderedProperties) if (property.name == "Path") { property.value = names[n]; found = true; } if (!found) entries[n].orderedProperties.prepend({"Path", names[n]}); }
            resolvingNames = false;
        } else {
            const auto boundary = textOutput.indexOf("\n----------\n");
            if (boundary < 0) { complete(false, -1, "Archive metadata boundary is missing from the technical listing."); return; }
            int layers = 0;
            archiveLayers = parseArchiveLayers(textOutput.left(boundary));
            for (const auto &line : textOutput.left(boundary).split('\n')) { const int split = line.indexOf(" = "); if (split > 0) { const auto key = line.left(split); if (key == "Type") ++layers; archiveProperties[key] = line.mid(split + 3); } }
            archiveProperties["Open Layers"] = QString::number(layers);
            archiveType = archiveProperties.value("Type");
            QString listing = textOutput.mid(boundary + 12);
            // Header-enabled listings finish with a warning count outside the
            // item properties. Preserve it separately instead of treating it as
            // part of a filename or ignoring arbitrary malformed item lines.
            static const QRegularExpression warnings("\\n\\nWarnings: ([0-9]+)\\n*$");
            auto warning = warnings.match(listing);
            if (warning.hasMatch()) { archiveProperties["Warnings"] = warning.captured(1); listing.truncate(warning.capturedStart()); }
            QString error; entries = parseListing(listing, &error);
            if (!error.isEmpty()) { complete(false, -1, error); return; }
            for (const auto &entry : entries) if (entry.path.isEmpty()) { resolvingNames = true; launch({"l", "-ba", "--", request.archive}); return; }
        }
        finishListing(); return;
    } else if (request.operation == ArchiveOperation::Extract) {
        commitExtract(); return;
    } else if ((request.operation == ArchiveOperation::Add || request.operation == ArchiveOperation::HashFile) && creating) {
        auto outputs = QDir(staging->path()).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        if (outputs.isEmpty()) { complete(false, -1, "7-Zip created no output archive."); return; }
        for (const auto &f : outputs) {
            QString dest = QFileInfo(request.archive).absolutePath() + '/' + f.fileName();
            if (QFileInfo::exists(dest) || QFileInfo(dest).isSymLink()) { complete(false, -1, "Archive output already exists: " + dest); return; }
        }
        QStringList installed;
        for (const auto &f : outputs) {
            QString dest = QFileInfo(request.archive).absolutePath() + '/' + f.fileName();
            if (QFileInfo::exists(dest) || !QFile::rename(f.filePath(), dest)) {
                for (const auto &path : installed) QFile::rename(path, staging->filePath(QFileInfo(path).fileName()));
                complete(false, -1, "Cannot install archive output: " + dest); return;
            }
            installed << dest;
        }
    }
    if (request.operation == ArchiveOperation::Add && request.deleteAfter) { checkingSources = true; launch({"l", "-slt", "-ba", "--", request.volume.isEmpty() ? request.archive : request.archive + ".001"}); return; }
    complete(true, code);
}
void SevenZipProcessBackend::finishListing() {
    if (!zipCommentsRead && archiveType.compare("zip", Qt::CaseInsensitive) == 0 && archiveProperties.value("Open Layers").toInt() <= 1 && archiveProperties.value("Volumes").toUInt() <= 1 && archiveProperties.value("Multivolume") != "+") {
        readingZipComments = true; launchCommentHelper({"read", request.archive}); return;
    }
    if (preflight) {
        preflight = false;
        if (request.selectionDirectory >= 0 && !archiveMetadata.selectionResolved) { complete(false, -1, "Native archive selection was not resolved."); return; }
        if (request.operation == ArchiveOperation::Test || request.operation == ArchiveOperation::HashArchive) {
            totalSize = 0; for (auto index : archiveMetadata.selectedIndices) if (!entries[index].directory) totalSize += entries[index].size;
            QStringList args{"t", "-bsp1", "-bb1"};
            if (request.operation == ArchiveOperation::HashArchive) args << "-scrc" + request.hashMethod;
            if (request.hashTest) args << "-tHash";
            launch(args + QStringList{"--", request.archive}); return;
        }
        beginExtract(); return;
    }
    complete(true, 0);
}
bool SevenZipProcessBackend::resolveOverwrite(quint64 id, OverwriteAnswer answer) {
    return active && overwriteBroker.answer(id, answer);
}
void SevenZipProcessBackend::complete(bool success, int code, QString message) {
    if (!active) return;
    if (commentHelperActive) textOutput.clear(); // Property JSON is never a diagnostic log.
    QStringList restoreErrors;
    for (const auto &stamp : sourceAccessTimes) if (QFileInfo::exists(stamp.path)) {
        const timespec times[2] = {{time_t(stamp.seconds), stamp.nanos}, {0, UTIME_OMIT}}; const auto bytes = QFile::encodeName(stamp.path);
        if (::utimensat(AT_FDCWD, bytes.constData(), times, AT_SYMLINK_NOFOLLOW) != 0) restoreErrors << stamp.path;
    }
    if (!restoreErrors.isEmpty()) { success = false; code = -1; message += "\nCannot preserve source access time: " + restoreErrors.mid(0, 5).join('\n'); }
    ArchiveResult result;
    if (completionStaging && !cancelled) {
        QString error;
        if (!readCompletionData(completionStaging->filePath("result.json"), result.completion, error) && success) { success = false; code = -1; message = error; }
        if (request.selectionDirectory >= 0 && result.completion.hash) {
            // PanelCopy.cpp seeds both names from GetItemRelPath only for one
            // operated row. The request carries that validated native row's
            // display path relative to the current panel, including Flat mode.
            const auto name = request.selection.size() == 1 ? request.selection.first().name : QString();
            result.completion.hash->FirstFileName = name; result.completion.hash->MainName = name;
        }
        if (!request.password.isEmpty() && result.completion.hash) {
            result.completion.hash->MainName.replace(request.password, "[redacted]"); result.completion.hash->FirstFileName.replace(request.password, "[redacted]");
        }
    }
    result.testInside = request.selectionDirectory >= 0;
    result.operation = request.operation; result.target = request.archive; result.exitCode = code;
    result.success = success; result.cancelled = cancelled; result.entries = entries;
    result.passwordRequired = !success && !cancelled && code > 0 && passwordDiagnostic;
    result.archiveType = archiveType; result.properties = archiveProperties; result.layers = archiveLayers; result.metadata = archiveMetadata;
    if (success && !mutationItemIndexMap.isEmpty()) {
        result.previousSnapshot = QString::fromStdString(archiveSourceStamp(mutationOriginal)); result.itemIndexMap = mutationItemIndexMap;
        bindArchiveMutationGraph(result, mutationOriginalMetadata,
            request.operation == ArchiveOperation::Rename ? mutationSelectedPath : QString(),
            request.operation == ArchiveOperation::Rename ? request.files.value(1) : QString());
    }
    result.message = message.isEmpty() ? "Everything is Ok" : message;
    result.details = textErrors + (request.operation == ArchiveOperation::List && success ? QString() : textOutput);
    request.password.fill(QChar::Null); request.password.clear();
    passwordOutputTail.fill(QChar::Null); passwordOutputTail.clear(); passwordErrorTail.fill(QChar::Null); passwordErrorTail.clear();
    active = false; overwriteBroker.cancel(); staging.reset(); completionStaging.reset(); metadataStaging.reset(); selectionStaging.reset(); extractMetadataStaging.reset(); mutationPhase = MutationPhase::None;
    paused = false; emit pausedChanged(false); emit pauseAvailabilityChanged(false);
    emit finished(result);
}
bool SevenZipProcessBackend::canPause() const {
    return active && !cancelled && !overwriteBroker.waiting() && (process.state() == QProcess::Running || mergeWatcher.isRunning());
}
bool SevenZipProcessBackend::setBackground(bool value) {
    if (process.state() == QProcess::Running) {
        const auto error = MacProcessPriority::set(process.processId(), value);
        if (!error.isEmpty()) { emit operationError({-1, false, {}, request.archive, error}); return false; }
    }
    backgroundMode = value; if (stopFlag) stopFlag->setBackground(value); return true;
}
bool SevenZipProcessBackend::setPaused(bool value) {
    if (!canPause()) return false;
    if (process.state() == QProcess::Running && ::kill(pid_t(process.processId()), value ? SIGSTOP : SIGCONT) != 0) return false;
    paused = value; stopFlag->setPaused(value); emit pausedChanged(value); return true;
}
void SevenZipProcessBackend::cancel() {
    if (!active) return;
    cancelled = true; stopFlag->cancel(); overwriteBroker.cancel();
    if (paused && process.state() == QProcess::Running) ::kill(pid_t(process.processId()), SIGCONT);
    paused = false; emit pausedChanged(false); emit pauseAvailabilityChanged(false);
    if (process.state() != QProcess::NotRunning) {
        process.terminate();
        QTimer::singleShot(1500, &process, [this] { if (active && cancelled && process.state() != QProcess::NotRunning) process.kill(); });
    }
}
