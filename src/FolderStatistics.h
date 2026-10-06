// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QFutureWatcher>
#include "OperationControl.h"
#include <memory>

struct FolderStatistic {
    QString path, error;
    quint64 size = 0, folders = 0, files = 0;
    bool complete = false;
};
struct FolderStatisticsResult {
    QList<FolderStatistic> items;
    bool cancelled = false;
};
Q_DECLARE_METATYPE(FolderStatisticsResult)

// FileManager/PanelItems.cpp::EditItem(false) and FSFolder.cpp::CalcItemFullSize.
// Keep the GUI responsive and never traverse a symbolic link while counting.
class FolderStatistics final : public QObject {
    Q_OBJECT
public:
    explicit FolderStatistics(QObject *parent = nullptr);
    ~FolderStatistics() override;
    void start(QStringList paths);
    void cancel();
    bool setPaused(bool paused);
    bool busy() const { return watcher.isRunning(); }
signals:
    void progress(quint64 size, QString path);
    void finished(FolderStatisticsResult result);
    void pausedChanged(bool paused);
    void pauseAvailabilityChanged(bool available);
private:
    QFutureWatcher<FolderStatisticsResult> watcher;
    std::shared_ptr<OperationControl> control;
};
