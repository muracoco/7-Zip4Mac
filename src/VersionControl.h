// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "OperationControl.h"
#include "Overwrite.h"
#include <QFutureWatcher>
#include <QStringList>
#include <memory>

enum class VersionCommand { Edit, Commit, Revert, Diff };
struct VersionResult {
    VersionCommand operation = VersionCommand::Edit;
    QString target, error;
    QStringList diffPaths;
};
Q_DECLARE_METATYPE(VersionResult)

class VersionControl final : public QObject {
    Q_OBJECT
public:
    explicit VersionControl(QObject *parent = nullptr);
    ~VersionControl() override;
    void start(VersionCommand operation, QString target, QString store);
    bool busy() const { return watcher.isRunning(); }
    void cancel();
    bool setPaused(bool value);
    bool resolveOverwrite(quint64 id, OverwriteAnswer answer);
signals:
    void finished(VersionResult result);
    void overwriteRequested(OverwriteConflict conflict);
    void byteProgress(quint64 processed, quint64 total, QString path);
    void pausedChanged(bool paused);
    void pauseAvailabilityChanged(bool available);
private:
    QFutureWatcher<VersionResult> watcher;
    std::shared_ptr<OperationControl> control;
    OverwriteBroker broker;
};
