// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ArchiveMetadata.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

namespace {
QString normalized(QString path) { path.replace('\\', '/'); while (path.endsWith('/')) path.chop(1); return path; }
ArchiveProperty readProperty(const QJsonObject &object, const ArchivePropertyDefinition &definition) {
    ArchiveProperty result{definition.name, object.value("value").toString()};
    result.id = definition.id; result.native = true; result.raw = definition.raw;
    result.type = quint16(object.value("type").toInt()); result.number = object.value("number").toString();
    result.listValue = object.value("list").toString(); result.fileTime = object.value("ticks").toString();
    result.rawSize = quint32(object.value("size").toInteger()); result.rawType = quint32(object.value("type").toInteger());
    result.sortData = QByteArray::fromHex(object.value("sort").toString().toLatin1());
    result.retrievalError = object.value("status").toInteger();
    for (const auto &precision : object.value("precision").toArray()) result.timePrecision << quint16(precision.toInt());
    return result;
}
bool readSchema(const QJsonValue &value, QList<ArchivePropertyDefinition> &schema, bool raw) {
    if (!value.isArray()) return false;
    for (const auto &item : value.toArray()) {
        const auto object = item.toObject();
        if (!object.value("id").isDouble() || !object.value("type").isDouble() || !object.value("name").isString()) return false;
        schema.append({quint32(object.value("id").toInteger()), quint16(object.value("type").toInt()), object.value("name").toString(), raw});
    }
    return true;
}
bool readValues(const QJsonValue &value, const QList<ArchivePropertyDefinition> &schema, ArchivePropertyList &values) {
    if (!value.isArray() || value.toArray().size() != schema.size()) return false;
    const auto array = value.toArray();
    for (qsizetype i = 0; i < schema.size(); ++i) {
        const auto object = array[i].toObject();
        if (object.value("id").toInteger(-1) != schema[i].id || !object.value("value").isString() || !object.value("type").isDouble()) return false;
        values.append(readProperty(object, schema[i]));
    }
    return true;
}
bool readLayer(const QJsonValue &value, ArchivePropertyList &properties) {
    if (!value.isArray()) return false;
    for (const auto &item : value.toArray()) {
        const auto object = item.toObject();
        if (!object.value("id").isDouble() || !object.value("name").isString() || !object.value("value").isString()) return false;
        properties.append(readProperty(object, {quint32(object.value("id").toInteger()), 0, object.value("name").toString(), false}));
    }
    return true;
}
}
QString parseNativeMetadata(const QByteArray &data, ArchiveResult &result) {
    QJsonParseError error; const auto document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return "Invalid native archive metadata: " + error.errorString();
    const auto root = document.object(); ArchiveResult parsed; parsed.metadata.native = true;
    if (root.value("version").toInt() != 1 || !readSchema(root.value("schema"), parsed.metadata.schema, false) || !readSchema(root.value("rawSchema"), parsed.metadata.rawSchema, true) || !root.value("entries").isArray() || !root.value("folders").isArray() || !root.value("layers").isArray()) return "Unsupported native archive metadata schema.";
    qint64 expectedIndex = 0;
    if (root.contains("sourceSnapshot")) {
        if (!root.value("sourceSnapshot").isString()) return "Invalid archive source snapshot.";
        parsed.metadata.sourceSnapshot = root.value("sourceSnapshot").toString();
    }
    for (const auto &item : root.value("entries").toArray()) {
        const auto object = item.toObject(); ArchiveEntry entry;
        if (object.value("index").toInteger(-1) != expectedIndex++ || !object.value("path").isString() || !object.value("directory").isBool() || !readValues(object.value("properties"), parsed.metadata.schema, entry.orderedProperties) || !readValues(object.value("raw"), parsed.metadata.rawSchema, entry.orderedProperties)) return "Native archive item schema or index mismatch.";
        entry.archiveIndex = object.value("index").toInteger(); entry.path = object.value("path").toString(); entry.directory = object.value("directory").toBool();
        if (object.contains("auxiliary") && !object.value("auxiliary").isBool()) return "Invalid auxiliary archive item property.";
        entry.auxiliary = object.value("auxiliary").toBool();
        if (object.contains("generated") && !object.value("generated").isBool()) return "Invalid generated archive item property.";
        entry.generated = object.value("generated").toBool();
        entry.parentIndex = object.value("parent").toInteger(-1); entry.parentType = quint32(object.value("parentType").toInteger());
        for (const auto &property : entry.orderedProperties) if (!property.raw && property.type != 0) entry.properties[property.name] = property.value;
        entry.properties["Path"] = entry.path;
        entry.size = entry.properties.value("Size").toULongLong(); entry.packed = entry.properties.value("Packed Size").toULongLong(); entry.modified = entry.properties.value("Modified");
        entry.attributes = entry.properties.value("Attributes"); entry.crc = entry.properties.value("CRC"); entry.method = entry.properties.value("Method"); entry.extension = entry.properties.value("Extension"); entry.encrypted = entry.properties.value("Encrypted") == "+";
        entry.link = !entry.properties.value("Symbolic Link").isEmpty() || !entry.properties.value("Hard Link").isEmpty() || !entry.properties.value("Link").isEmpty() || entry.attributes.section(' ', 0, 0).contains('L') || entry.attributes.contains(" lr") || entry.properties.value("Mode").startsWith('l');
        parsed.entries.append(entry);
    }
    if (root.contains("selectedIndices")) {
        if (!root.value("selectedIndices").isArray()) return "Invalid native archive selection.";
        qint64 previous = -1;
        for (const auto &value : root.value("selectedIndices").toArray()) {
            const auto index = value.toInteger(-1);
            if (!value.isDouble() || index <= previous || index >= parsed.entries.size()) return "Invalid native archive selection index.";
            previous = index; parsed.metadata.selectedIndices.append(quint32(index));
        }
        parsed.metadata.selectionResolved = true;
        if (!root.value("selectedPath").isString()) return "Invalid native archive selection path.";
        parsed.metadata.selectedPath = root.value("selectedPath").toString();
    }
    for (const auto &item : root.value("folders").toArray()) {
        const auto object = item.toObject(); ArchiveFolderTotals totals; bool ok = false;
        if (!object.value("path").isString() || !object.value("crcDefined").isBool()) return "Invalid native archive folder.";
        totals.size = object.value("size").toString().toULongLong(&ok); if (!ok) return "Invalid native folder size.";
        totals.packed = object.value("packed").toString().toULongLong(&ok); if (!ok) return "Invalid native folder packed size.";
        totals.folders = object.value("folders").toString().toULongLong(&ok); if (!ok) return "Invalid native folder count.";
        totals.files = object.value("files").toString().toULongLong(&ok); if (!ok) return "Invalid native file count.";
        totals.crc = object.value("crc").toString().toUInt(&ok); if (!ok) return "Invalid native folder CRC.";
        totals.crcDefined = object.value("crcDefined").toBool(); totals.archiveIndex = object.value("index").toInteger(-1);
        const auto path = normalized(object.value("path").toString());
        if (!object.value("alternateStreams").toBool() && !parsed.metadata.folders.contains(path)) parsed.metadata.folders.insert(path, totals);
        if (object.contains("id")) {
            ArchiveDirectory directory; directory.totals = totals;
            directory.id = object.value("id").toInt(-1); directory.parent = object.value("parentDir").toInt(-2);
            directory.alternateDirectory = object.value("alt").toInt(-2); directory.path = object.value("path").toString(); directory.path.replace('\\', '/');
            directory.alternateStreams = object.value("alternateStreams").toBool();
            if (directory.id != parsed.metadata.directories.size() || directory.parent < -1 || directory.alternateDirectory < -1 || !object.value("alternateStreams").isBool() || !readLayer(object.value("properties"), directory.properties) || !object.value("children").isArray()) return "Invalid native directory identity.";
            for (const auto &child : object.value("children").toArray()) {
                const auto rowObject = child.toObject(); ArchiveRow row;
                row.identity = {directory.id, int(directory.children.size())}; row.archiveIndex = rowObject.value("index").toInteger(-2);
                row.childDirectory = rowObject.value("dir").toInt(-2); row.alternateDirectory = rowObject.value("alt").toInt(-2);
                if (row.archiveIndex < -1 || row.archiveIndex >= parsed.entries.size() || row.childDirectory < -1 || row.alternateDirectory < -1 || !rowObject.value("name").isString() || !rowObject.value("path").isString() || !readLayer(rowObject.value("properties"), row.properties) || !readLayer(rowObject.value("flatProperties"), row.flatProperties)) return "Invalid native item reference.";
                row.name = rowObject.value("name").toString(); row.path = rowObject.value("path").toString(); row.path.replace('\\', '/');
                if (rowObject.contains("copyName") && !rowObject.value("copyName").isString()) return "Invalid native Copy output name.";
                row.copyName = rowObject.value("copyName").toString(row.name);
                directory.children.append(row);
            }
            parsed.metadata.directories.append(directory);
        } else if (!parsed.metadata.directories.isEmpty()) return "Incomplete native directory graph.";
    }
    for (const auto &directory : parsed.metadata.directories) {
        const int count = int(parsed.metadata.directories.size());
        if (directory.parent >= count || directory.parent == directory.id || directory.alternateDirectory >= count || (directory.id == 0 && directory.parent != -1)) return "Invalid native directory relationship.";
        for (const auto &row : directory.children) if (row.childDirectory >= count || row.alternateDirectory >= count || row.childDirectory == directory.id) return "Invalid native child directory relationship.";
    }
    for (const auto &item : root.value("layers").toArray()) {
        ArchiveLayer layer; const auto object = item.toObject();
        if (!readLayer(object.value("properties"), layer.properties) || !readLayer(object.value("childProperties"), layer.childProperties)) return "Invalid native archive layer.";
        for (const auto &property : layer.properties) if (property.type != 0 && !property.value.isEmpty()) parsed.properties[property.name] = property.value;
        parsed.layers.append(layer);
    }
    if (parsed.layers.isEmpty() || !readLayer(root.value("nonOpen"), parsed.metadata.failedProperties)) return "Native archive metadata has no open layer or failed-open properties.";
    parsed.archiveType = parsed.properties.value("Type"); parsed.properties["Open Layers"] = QString::number(parsed.layers.size());
    result.entries = std::move(parsed.entries); result.metadata = std::move(parsed.metadata); result.layers = std::move(parsed.layers); result.properties = std::move(parsed.properties); result.archiveType = parsed.archiveType;
    return {};
}
