// SPDX-License-Identifier: LGPL-3.0-or-later
#include "WorkerProcess.h"
#include "ProgressChannel.h"
#include "MacProcessPriority.h"
#include <QElapsedTimer>
#include <QFileInfo>
#include <QProcess>
#include <QStringDecoder>
#include <signal.h>
#include <cerrno>
#include <cstring>
WorkerProcess::Result WorkerProcess::run(QString program, QStringList arguments,
    const QString &password, const std::shared_ptr<OperationControl> &control,
    const std::function<void(ArchiveProgress)> &progress, int timeoutMs) {
    Result result;
    if (control->checkpoint()) { result.cancelled = true; result.exitCode = 255; result.failure = "Operation cancelled."; return result; }
    QProcess child; ProgressChannel channel(&child);
    const QString helper = QFileInfo(program).absolutePath() + "/7zz-progress";
    const bool native = QFileInfo(program).fileName() == "7zz" && QFileInfo(helper).isExecutable() && channel.prepare(true, false);
    if (!native) channel.prepare(false);
    QObject::connect(&channel, &ProgressChannel::snapshot, &channel, [&](ArchiveProgress snapshot) {
        if (!password.isEmpty()) { snapshot.current.replace(password, "[redacted]"); snapshot.titleFileName.replace(password, "[redacted]"); snapshot.status.replace(password, "[redacted]"); }
        if (progress) progress(snapshot);
    }, Qt::DirectConnection);
    child.start(native ? helper : program, arguments);
    if (!child.waitForStarted(10000)) { result.failure = "Cannot start verification: " + child.errorString(); return result; }
    QByteArray secret = password.toUtf8() + '\n'; child.write(secret); secret.fill('\0'); child.closeWriteChannel();
    QStringDecoder output(QStringDecoder::Utf8), errors(QStringDecoder::Utf8);
    auto drain = [&] {
        channel.drain(); result.output += output(child.readAllStandardOutput()); result.errors += errors(child.readAllStandardError());
        constexpr int limit = 1024 * 1024; if (result.output.size() > limit) result.output = result.output.right(limit); if (result.errors.size() > limit) result.errors = result.errors.right(limit);
    };
    QElapsedTimer clock; clock.start(); qint64 pausedMs = 0;
    bool background = false;
    auto priority = [&] {
        const auto desired = control->isBackground(); if (desired == background || child.state() == QProcess::NotRunning) return;
        const auto error = MacProcessPriority::set(child.processId(), desired); if (!error.isEmpty()) result.errors += error + '\n';
        background = desired;
    };
    while (child.state() != QProcess::NotRunning) {
        priority();
        if (control->isPaused()) {
            const auto pid = child.processId();
            if (pid <= 0 || ::kill(pid_t(pid), SIGSTOP) != 0) {
                result.failure = "Cannot pause verification: " + QString::fromLocal8Bit(std::strerror(errno)); break;
            }
            QElapsedTimer pause; pause.start();
            while (control->isPaused() && !control->isCancelled()) { priority(); control->waitPausedChange(background); }
            const bool cancelled = control->isCancelled(); pausedMs += pause.elapsed();
            if (cancelled) { result.cancelled = true; result.failure = "Verification cancelled; sources retained."; break; }
            if (::kill(pid_t(pid), SIGCONT) != 0) { result.failure = "Cannot resume verification: " + QString::fromLocal8Bit(std::strerror(errno)); break; }
        }
        if (control->isCancelled()) { result.cancelled = true; result.failure = "Verification cancelled; sources retained."; break; }
        if (timeoutMs > 0 && clock.elapsed() - pausedMs > timeoutMs) { result.failure = "Verification timed out; sources retained."; break; }
        child.waitForFinished(50); drain();
    }
    if (child.state() != QProcess::NotRunning) { child.kill(); child.waitForFinished(3000); }
    drain(); result.exitCode = result.cancelled ? 255 : child.exitCode();
    result.success = result.failure.isEmpty() && child.exitStatus() == QProcess::NormalExit && result.exitCode == 0;
    if (!result.success && result.failure.isEmpty()) result.failure = "Verification failed (7-Zip exit code " + QString::number(result.exitCode) + ").";
    if (!password.isEmpty()) { result.output.replace(password, "[redacted]"); result.errors.replace(password, "[redacted]"); result.failure.replace(password, "[redacted]"); }
    return result;
}
