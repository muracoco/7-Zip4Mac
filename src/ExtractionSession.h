// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveBackend.h"
#include "ExtractionInstaller.h"
#include "ProgressChannel.h"

// Sequential worker-owned state for the original callback's required messages.
// No QObject, process or GUI ownership; the caller sends asynchronous replies.
class ExtractionSession {
public:
    ExtractionSession(QString stage, QString root, QString records, QList<ArchiveEntry> entries, OverwriteBroker &broker,
                      QString removedRoot = {}, bool absolute = false, QMap<QString, QString> targets = {});
    ~ExtractionSession();
    FileSnapshot lookup(const NativeFileStateRequest &request);
    int apply(const NativeExtractionRequest &request);
    QString error() const;
    QString publicPath(const QString &nativePath) const;
    QString close();
    bool retainsStaging() const;
private:
    struct Data;
    std::unique_ptr<Data> data;
};
