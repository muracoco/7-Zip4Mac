// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "FileInstall.h"
#include <QMap>
#include <memory>

struct ArchiveEntry;
struct ExtractMetadata {
    std::optional<timespec> created;
    bool accessed = false, modified = false, attributes = false;
    QString hardLink, hardLinkSource;
};
ExtractMetadata extractionMetadataFor(const ArchiveEntry &entry);
QString extractionOutputTarget(const QString &root, const QString &relative, bool absolute, const QMap<QString, QString> &targets);

// One ordered extraction session. Call only from its worker sequence; the GUI
// and native callback can wait asynchronously without owning filesystem state.
class ExtractionInstaller {
public:
    ExtractionInstaller(QString stage, QString root, bool absolute, QMap<QString, QString> targets,
                        QString removedRoot, QString mode, OverwriteBroker &broker,
                        std::shared_ptr<OperationControl> control, QMap<QString, QString> references = {});
    ~ExtractionInstaller();
    QString prepareDirectory(QString relative, const FileSnapshot &source, ExtractMetadata metadata, bool createdHere = false);
    QString install(QString relative, const FileSnapshot &completed, ExtractMetadata metadata, QString group,
                    std::optional<FileSnapshot> expected = {}, QString approvedMode = {});
    QString finish(QString error = {});
    QString relocated(const QString &from, const QString &to);
    QString reference(const QString &path, const QString &fingerprint);
private:
    struct Data;
    std::unique_ptr<Data> data;
};
