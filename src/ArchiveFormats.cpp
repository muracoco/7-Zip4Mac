// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ArchiveFormats.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
QStringList ArchiveFormats::openTypes() {
    // Official 26.03 Explorer/ContextMenu.cpp::kOpenTypes, including the
    // normal entry. #:a is commented out upstream and is not a menu choice.
    return {"", "*", "#", "#:e", "7z", "zip", "cab", "rar"};
}
static void initFormatResources() { Q_INIT_RESOURCE(formats); }
QList<ArchiveFormat> ArchiveFormats::all() {
    static const QList<ArchiveFormat> formats = [] {
        initFormatResources(); QFile file(":/formats.json");
        if (!file.open(QIODevice::ReadOnly)) qFatal("Archive format inventory is missing");
        auto array = QJsonDocument::fromJson(file.readAll()).object()["formats"].toArray();
        QList<ArchiveFormat> result;
        for (auto item : array) {
            auto value = item.toObject(); ArchiveFormat format; format.name = value["name"].toString(); format.canCreate = value["canCreate"].toBool();
            const auto caps = value["capabilities"].toObject();
            if (caps.isEmpty()) qFatal("Archive handler capabilities are missing");
            format.precisionMask = caps["precisionMask"].toInteger(); format.defaultPrecision = caps["defaultPrecision"].toInt();
            format.modificationTime = caps["modificationTime"].toBool(); format.creationTime = caps["creationTime"].toBool(); format.accessTime = caps["accessTime"].toBool();
            format.defaultModificationTime = caps["defaultModificationTime"].toBool(); format.defaultCreationTime = caps["defaultCreationTime"].toBool(); format.defaultAccessTime = caps["defaultAccessTime"].toBool();
            format.keepName = caps["keepName"].toBool(); format.symbolicLinks = caps["symbolicLinks"].toBool(); format.hardLinks = caps["hardLinks"].toBool();
            for (auto ext : value["extensions"].toArray()) format.extensions << ext.toString();
            const auto extras = value["additionalExtensions"].toObject();
            for (auto it = extras.begin(); it != extras.end(); ++it) format.additionalExtensions[it.key()] = it.value().toString();
            result << format;
        }
        if (result.isEmpty()) qFatal("Archive format inventory is empty");
        return result;
    }();
    return formats;
}
QStringList ArchiveFormats::extensions() {
    static const QStringList extensions = [] { QStringList list; for (const auto &format : all()) list << format.extensions; list.removeDuplicates(); list.sort(); return list; }();
    return extensions;
}
ArchiveFormat ArchiveFormats::find(const QString &name) { for (const auto &format : all()) if (format.name.compare(name, Qt::CaseInsensitive) == 0) return format; return {}; }
bool ArchiveFormats::recognizes(const QString &path) { return extensions().contains(QFileInfo(path).suffix().toLower()); }
QString ArchiveFormats::unnamedEntry(const QString &archive) {
    const QFileInfo info(archive); QString name = info.completeBaseName(), suffix;
    for (const auto &format : all()) {
        const auto extra = format.additionalExtensions.value(info.suffix().toLower());
        if (!extra.isEmpty() && extra != "*") { suffix = extra; break; }
    }
    if (info.suffix().isEmpty() && suffix.isEmpty()) suffix = "~";
    return (name + suffix).trimmed();
}
