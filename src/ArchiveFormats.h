// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QStringList>
#include <QMap>
struct ArchiveFormat {
    QString name;
    QStringList extensions;
    QMap<QString, QString> additionalExtensions;
    bool canCreate = false;
    quint32 precisionMask = 0;
    int defaultPrecision = 0;
    bool modificationTime = false, creationTime = false, accessTime = false;
    bool defaultModificationTime = false, defaultCreationTime = false, defaultAccessTime = false;
    bool keepName = false, symbolicLinks = false, hardLinks = false;
};
class ArchiveFormats {
public:
    static QList<ArchiveFormat> all();
    static ArchiveFormat find(const QString &name);
    static QStringList extensions();
    static bool recognizes(const QString &path);
    static QString unnamedEntry(const QString &archive);
    static QStringList openTypes();
};
