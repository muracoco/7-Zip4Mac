// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveProgress.h"
#include "Overwrite.h"
#include "FileInstall.h"
#include <QObject>
#include <QJsonObject>
class QProcess;
class QSocketNotifier;
struct NativeOverwriteRequest {
    quint64 generation = 0;
    QString id;
    OverwriteConflict conflict;
};
Q_DECLARE_METATYPE(NativeOverwriteRequest)
struct NativeFileStateRequest {
    quint64 generation = 0;
    QString id, path;
    bool probe = false;
};
Q_DECLARE_METATYPE(NativeFileStateRequest)
struct NativeExtractionRequest {
    quint64 generation = 0;
    QString id, action;
    QJsonObject item;
};
Q_DECLARE_METATYPE(NativeExtractionRequest)
class ProgressChannel : public QObject {
    Q_OBJECT
public:
    explicit ProgressChannel(QProcess *process, QObject *parent = nullptr);
    ~ProgressChannel() override;
    bool prepare(bool enabled, bool automatic = true);
    void drain();
    bool replyOverwrite(const NativeOverwriteRequest &request, OverwriteAnswer answer);
    bool replyFileState(const NativeFileStateRequest &request, const FileSnapshot &file);
    bool replyExtraction(const NativeExtractionRequest &request, int error = 0);
    bool hasSnapshot() const { return received; }
signals:
    void snapshot(ArchiveProgress progress);
    void operationError(ArchiveOperationError error);
    void benchmarkSnapshot(QJsonObject snapshot);
    void overwriteRequest(NativeOverwriteRequest request);
    void fileStateRequest(NativeFileStateRequest request);
    void extractionRequest(NativeExtractionRequest request);
private:
    void close();
    QProcess *process;
    QSocketNotifier *notifier = nullptr;
    int parentFd = -1, childFd = -1;
    bool received = false;
    quint64 generation = 0;
    quint64 lastOverwriteId = 0;
    QString pendingOverwrite;
    quint64 lastFileStateId = 0;
    QString pendingFileState;
    quint64 lastExtractionId = 0;
    QString pendingExtraction;
};
