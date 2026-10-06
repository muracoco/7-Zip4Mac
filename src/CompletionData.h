// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QString>
#include <QVector>
#include <array>
#include <optional>

struct DecompressStatistics {
    quint64 NumArchives = 0, UnpackSize = 0, AltStreams_UnpackSize = 0, PackSize = 0;
    quint64 NumFolders = 0, NumFiles = 0, NumAltStreams = 0;
};
struct HashDigestResult {
    QString name;
    std::array<QString, 3> groups;
};
struct HashStatistics {
    quint64 NumDirs = 0, NumFiles = 0, NumAltStreams = 0, FilesSize = 0, AltStreamsSize = 0, NumErrors = 0;
    QString MainName, FirstFileName;
    QVector<HashDigestResult> hashers;
};
struct CompletionData {
    std::optional<DecompressStatistics> decompression;
    std::optional<HashStatistics> hash;
};
bool readCompletionData(const QString &path, CompletionData &data, QString &error);
