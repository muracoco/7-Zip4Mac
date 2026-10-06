// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveProgress.h"
#include "OperationControl.h"
#include <QStringList>
#include <functional>
#include <memory>
namespace WorkerProcess {
struct Result {
    int exitCode = -1;
    bool success = false, cancelled = false;
    QString output, errors, failure;
};
// The worker exclusively owns, pauses and reaps its child. No PID crosses threads.
Result run(QString program, QStringList arguments, const QString &password,
    const std::shared_ptr<OperationControl> &control,
    const std::function<void(ArchiveProgress)> &progress = {}, int timeoutMs = 0);
}
