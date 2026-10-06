// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveBackend.h"
#include <QHash>

// AgentProxy's cached descendant totals. Only archive listing data is read;
// opening Properties never scans or extracts filesystem/archive contents.
class ArchiveFolderIndex {
public:
    ArchiveFolderIndex() = default;
    explicit ArchiveFolderIndex(const QList<ArchiveEntry> &entries, const ArchiveMetadata &metadata = {});
    QList<ArchivePropertyDefinition> columns(bool flat) const;
    bool native() const { return metadata.native; }
    bool hasDirectories() const { return !metadata.directories.isEmpty(); }
    QString sourceSnapshot() const { return metadata.sourceSnapshot; }
    const ArchiveDirectory *directory(int id) const;
    int findDirectory(QString path) const;
    QList<ArchiveRow> directoryRows(int id, bool flat) const;
    ArchivePropertyList rowProperties(ArchiveRowIdentity identity, bool flat, int baseDirectory) const;
    ArchivePropertyList nativeSelectionProperties(const QList<ArchiveRowIdentity> &identities, bool flat, int baseDirectory) const;
    ArchivePropertyList directoryProperties(int id) const;
    bool uniquePaths(const QStringList &paths) const;
    ArchivePropertyList failedProperties() const;
    ArchiveFolderTotals totals(QString prefix = {}) const;
    const ArchiveEntry *entry(QString path) const;
    ArchivePropertyList itemProperties(QString path, bool flat, QString prefix = {}, qint64 archiveIndex = -1) const;
    ArchivePropertyList selectionProperties(const QStringList &paths, bool flat, QString prefix = {}) const;
    ArchivePropertyList folderProperties(QString prefix) const;
private:
    QList<ArchiveEntry> entries;
    QHash<QString, qsizetype> positions;
    QHash<QString, int> pathCounts, directoryCounts;
    QHash<QString, ArchiveFolderTotals> folders;
    QStringList schema;
    ArchiveMetadata metadata;
};

QString archivePropertyNumber(quint64 value);
ArchivePropertyList archivePropertyRows(const ArchivePropertyList &properties);
ArchivePropertyList archiveLayerProperties(const ArchiveLayer &layer, QString outerPath = {});

// Adapt the verified original Agent update to Qt's native graph identities.
// Implicit folders have no handler index; never match them as index -1 rows.
void bindArchiveMutationGraph(ArchiveResult &result, const ArchiveMetadata &before,
    const QString &renamedSource = {}, const QString &renamedDestination = {});
