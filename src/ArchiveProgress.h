// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
#include <QMetaType>
#include <optional>
struct ArchiveProgress {
    QString mode, current, titleFileName, status;
    bool directory = false, filesProgressMode = false;
    std::optional<quint64> total, completed, input, output, files, doneFiles;
    std::optional<quint64> processed() const { return input || output ? mode == "compress" ? input : output : completed; }
    std::optional<quint64> packed() const { return mode == "compress" ? output : input; }
    int percent() const { return completed && total && *total ? int(qMin(100.0L, 100.0L * *completed / *total)) : -1; }
};
Q_DECLARE_METATYPE(ArchiveProgress)
struct ArchiveOperationError {
    int code = -1;
    bool encrypted = false;
    QString fileName, archive, message;
};
Q_DECLARE_METATYPE(ArchiveOperationError)
