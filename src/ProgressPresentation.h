// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveProgress.h"
#include "CompletionData.h"
#include <QMap>
#include <QPair>
#include <QVector>
#include <memory>

struct ProgressFinalNotice {
    QString title, text;
    bool error = false;
};
struct ChecksumPresentation {
    QString title;
    QVector<QPair<QString, QString>> rows;
    bool deleteAllowed = false, selectFirst = true;
    unsigned columns = 1;
};
struct ProgressPresentation {
    QMap<unsigned, QString> text;
    QVector<QPair<QString, QString>> messages;
    QString title, parentTitlePrefix;
    int barMaximum = 100, barPosition = 0;
    bool errorsVisible = false;
    quint64 columnRevision = 0;
    QString clipboard;
    QVector<ProgressFinalNotice> finalNotices;
    bool ended = false, waitForClose = false, messagesDisplayed = false, cancelDefault = false;
    bool hidePause = false, hideBackground = false;
};
QString officialProgressStatus(QString token);
QString officialProgressError(const ArchiveOperationError &error);
QString officialTestResult(const DecompressStatistics &statistics);
QString officialInsideTestResult(const HashStatistics &statistics);
QString officialInsideTestResult(const DecompressStatistics &statistics, QString fileName);
ChecksumPresentation officialHashResults(const HashStatistics &statistics, const std::optional<DecompressStatistics> &archiveStatistics = {});
QString officialChecksumCopy(const QVector<QPair<QString, QString>> &rows, const QVector<unsigned> &selectedRows);

// All original Sync/UI state is consumed on the Qt GUI thread. Worker state
// crosses the existing queued snapshot boundary, never this adapter's memory.
class OfficialProgressState {
public:
    OfficialProgressState();
    ~OfficialProgressState();
    void update(const ArchiveProgress &snapshot, quint64 activeMilliseconds);
    void tick(quint64 activeMilliseconds, bool all = false);
    void setPaused(bool paused);
    void setBackground(bool background);
    void setLanguage(QString title, QString pause, QString resume, QString paused,
                     QString background, QString foreground);
    void setFileNameCapacity(unsigned capacity);
    void addError(QString message, quint64 activeMilliseconds);
    QString copyMessages(const QVector<unsigned> &selectedRows);
    void finish(QString error, QString ok, QString title, bool cancelled, quint64 activeMilliseconds);
    const ProgressPresentation &view() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
