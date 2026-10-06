// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ArchiveProperties.h"
#include <QSet>
#include <QRegularExpression>

void bindArchiveMutationGraph(ArchiveResult &result, const ArchiveMetadata &before,
    const QString &renamedSource, const QString &renamedDestination) {
    const auto &after = result.metadata.directories;
    if (before.directories.isEmpty() || after.isEmpty()) return;
    auto key = [](const ArchiveDirectory &dir, QString path) {
        return QString(dir.alternateStreams ? "a:" : "d:") + path;
    };
    QHash<QString, QList<int>> paths, owners;
    QHash<QString, int> oldPathCounts;
    QHash<qint64, QList<ArchiveRowIdentity>> realRows;
    for (const auto &dir : before.directories) ++oldPathCounts[key(dir, dir.path)];
    for (const auto &dir : after) {
        paths[key(dir, dir.path)].append(dir.id);
        if (dir.totals.archiveIndex >= 0) owners[key(dir, QString::number(dir.totals.archiveIndex))].append(dir.id);
        for (const auto &row : dir.children) if (row.archiveIndex >= 0) realRows[row.archiveIndex].append(row.identity);
    }
    auto mappedIndex = [&](qint64 index) { return index >= 0 && index < result.itemIndexMap.size() ? result.itemIndexMap[index] : -1; };
    auto descendantIndices = [](const QList<ArchiveDirectory> &graph, int id) {
        QSet<qint64> indices; QSet<int> visited; QList<int> stack{id};
        while (!stack.isEmpty()) {
            const int next = stack.takeLast(); if (next < 0 || next >= graph.size() || visited.contains(next)) continue;
            visited.insert(next);
            for (const auto &row : graph[next].children) {
                if (row.archiveIndex >= 0) indices.insert(row.archiveIndex);
                if (row.childDirectory >= 0) stack.append(row.childDirectory);
            }
        }
        return indices;
    };
    result.directoryIndexMap.fill(-1, before.directories.size());
    for (const auto &dir : before.directories) {
        if (dir.id == 0) { result.directoryIndexMap[0] = 0; continue; }
        QString path = dir.path;
        if (!renamedSource.isEmpty() && (path == renamedSource || path.startsWith(renamedSource + '/') || path.startsWith(renamedSource + ':')))
            path = renamedDestination + path.mid(renamedSource.size());
        const auto owner = mappedIndex(dir.totals.archiveIndex);
        auto candidates = owner >= 0 ? owners.value(key(dir, QString::number(owner))) : paths.value(key(dir, path));
        // A deleted explicit directory must not attach to a different folder
        // which happens to have the same display path.
        if (dir.totals.archiveIndex >= 0 && owner < 0) continue;
        if (candidates.size() == 1 && (owner >= 0 || oldPathCounts.value(key(dir, dir.path)) == 1)) {
            result.directoryIndexMap[dir.id] = candidates.first(); continue;
        }
        QSet<qint64> surviving;
        for (const auto index : descendantIndices(before.directories, dir.id)) if (mappedIndex(index) >= 0) surviving.insert(mappedIndex(index));
        if (surviving.isEmpty()) continue;
        int match = -1;
        for (const int candidate : candidates) {
            const auto members = descendantIndices(after, candidate);
            bool contains = true; for (const auto index : surviving) if (!members.contains(index)) { contains = false; break; }
            if (contains) { if (match >= 0) { match = -1; break; } match = candidate; }
        }
        result.directoryIndexMap[dir.id] = match;
    }
    result.rowIdentityMap.resize(before.directories.size());
    for (const auto &dir : before.directories) {
        auto &mapping = result.rowIdentityMap[dir.id]; mapping.fill({}, dir.children.size());
        const auto parent = result.directoryIndexMap[dir.id];
        for (const auto &row : dir.children) {
            const auto real = mappedIndex(row.archiveIndex);
            if (row.archiveIndex >= 0) {
                const auto candidates = realRows.value(real); ArchiveRowIdentity match;
                for (const auto candidate : candidates) if (candidate.directory == parent) { if (match.directory >= 0) { match = {}; break; } match = candidate; }
                if (match.directory < 0 && candidates.size() == 1) match = candidates.first();
                mapping[row.identity.item] = match;
            } else if (row.childDirectory >= 0 && row.childDirectory < result.directoryIndexMap.size() && parent >= 0) {
                const auto child = result.directoryIndexMap[row.childDirectory];
                if (child < 0) continue;
                for (const auto &updated : after[parent].children) if (updated.childDirectory == child) { mapping[row.identity.item] = updated.identity; break; }
            }
        }
    }
}

namespace {
QString normalized(QString path) { path.replace('\\', '/'); while (path.endsWith('/')) path.chop(1); return path; }
QString parent(QString path) { const auto slash = path.lastIndexOf('/'); return slash < 0 ? QString() : path.left(slash); }
bool sizeProperty(const QString &name) {
    static const QSet<QString> names{"Size", "Packed Size", "Folders", "Files", "Offset", "Links", "Blocks", "Volumes", "Physical Size", "Headers Size", "Total Size", "Free Space", "Cluster Size", "Errors", "Streams", "Alternate Streams", "Alternate Streams Size", "Virtual Size", "Unpack Size", "Total Physical Size", "Tail Size", "Embedded Stub Size"};
    return names.contains(name);
}
void append(ArchivePropertyList &rows, QString name, QString value) {
    if (value.isEmpty()) return;
    bool number = false; const auto integer = value.toULongLong(&number);
    if (number && sizeProperty(name)) value = archivePropertyNumber(integer);
    rows.append({name, value});
}
bool nativeSize(quint32 id) {
    static const QSet<quint32> ids{7, 8, 31, 32, 36, 37, 38, 39, 44, 45, 56, 57, 58, 70, 74, 75, 76, 77, 78, 79, 87, 88};
    return ids.contains(id);
}
void append(ArchivePropertyList &rows, ArchiveProperty property) {
    if (property.value.isEmpty()) return;
    if (property.native) {
        if (!property.raw && nativeSize(property.id) && (property.type == 21 || property.type == 19 || property.type == 18) && !property.number.isEmpty()) property.value = archivePropertyNumber(property.number.toULongLong());
        rows.append(property);
    } else append(rows, property.name, property.value);
}
ArchivePropertyList ordered(const ArchiveEntry &entry) {
    if (!entry.orderedProperties.isEmpty()) return entry.orderedProperties;
    ArchivePropertyList result; for (auto it = entry.properties.cbegin(); it != entry.properties.cend(); ++it) result.append({it.key(), it.value()}); return result;
}
}
QString archivePropertyNumber(quint64 value) {
    auto text = QString::number(value); for (int n = text.size() - 3; n > 0; n -= 3) text.insert(n, ' '); return text;
}
ArchivePropertyList archivePropertyRows(const ArchivePropertyList &properties) { ArchivePropertyList rows; for (const auto &property : properties) append(rows, property); return rows; }
ArchiveFolderIndex::ArchiveFolderIndex(const QList<ArchiveEntry> &items, const ArchiveMetadata &nativeMetadata) : entries(items), metadata(nativeMetadata) {
    folders.insert({}, {});
    for (qsizetype index = 0; index < entries.size(); ++index) {
        const auto &item = entries[index]; const auto path = normalized(item.path);
        ++pathCounts[path];
        if (!positions.contains(path)) positions.insert(path, index);
        for (const auto &property : ordered(item)) if (!schema.contains(property.name)) schema << property.name;
        auto folder = item.directory ? path : parent(path);
        for (;;) { folders.insert(folder, {}); if (folder.isEmpty()) break; folder = parent(folder); }
    }
    if (metadata.native) { folders = QHash<QString, ArchiveFolderTotals>(metadata.folders.cbegin(), metadata.folders.cend()); for (const auto &dir : metadata.directories) ++directoryCounts[normalized(dir.path)]; return; }
    // Count every distinct explicit/implicit folder once under each ancestor.
    const auto paths = folders.keys();
    for (const auto &path : paths) if (!path.isEmpty()) {
        auto folder = parent(path); for (;;) { ++folders[folder].folders; if (folder.isEmpty()) break; folder = parent(folder); }
    }
    static const QRegularExpression crc32("^[0-9A-Fa-f]{8}$");
    for (const auto &item : entries) if (!item.directory) {
        auto folder = parent(normalized(item.path)); const bool hasCrc = crc32.match(item.crc).hasMatch();
        bool sizeDefined = false; item.properties.value("Size").toULongLong(&sizeDefined);
        for (;;) {
            auto &total = folders[folder]; total.size += item.size; total.packed += item.packed; ++total.files;
            if (hasCrc) total.crc += item.crc.toUInt(nullptr, 16);
            else if (item.size != 0 || !sizeDefined || !item.crc.isEmpty()) total.crcDefined = false;
            if (folder.isEmpty()) break; folder = parent(folder);
        }
    }
}
QList<ArchivePropertyDefinition> ArchiveFolderIndex::columns(bool flat) const {
    if (!metadata.native) return {};
    QList<ArchivePropertyDefinition> columns; bool hasName = false;
    for (auto definition : metadata.schema) {
        if (definition.id == 3) { definition.id = 4; definition.name = "Name"; hasName = true; }
        if (definition.id == 4) hasName = true;
        if (definition.id == 6) continue; // PanelItems only excludes IsDir.
        columns.append(definition);
    }
    if (!hasName) columns.prepend({4, 8, "Name", false});
    columns.append({31, 19, "Folders", false}); columns.append({32, 19, "Files", false});
    if (flat) columns.append({30, 8, "Path Prefix", false});
    columns.append(metadata.rawSchema);
    // Qt's icon/name model, like Win32 ListView, uses logical column zero.
    for (qsizetype i = 0; i < columns.size(); ++i) if (!columns[i].raw && columns[i].id == 4) { columns.move(i, 0); break; }
    return columns;
}
ArchiveFolderTotals ArchiveFolderIndex::totals(QString prefix) const { return folders.value(normalized(prefix)); }
ArchivePropertyList ArchiveFolderIndex::failedProperties() const { return archivePropertyRows(metadata.failedProperties); }
const ArchiveDirectory *ArchiveFolderIndex::directory(int id) const { return id >= 0 && id < metadata.directories.size() ? &metadata.directories[id] : nullptr; }
int ArchiveFolderIndex::findDirectory(QString path) const {
    path = normalized(path);
    for (const auto &dir : metadata.directories) if (normalized(dir.path) == path && !dir.alternateStreams) return dir.id;
    for (const auto &dir : metadata.directories) if (normalized(dir.path) == path && dir.alternateStreams) return dir.id;
    return -1;
}
bool ArchiveFolderIndex::uniquePaths(const QStringList &paths) const { for (const auto &path : paths) if (pathCounts.value(normalized(path)) > 1 || directoryCounts.value(normalized(path)) > 1) return false; return true; }
QList<ArchiveRow> ArchiveFolderIndex::directoryRows(int id, bool flat) const {
    const auto base = directory(id); if (!base) return {};
    if (!flat) return base->children;
    // Same directory-first traversal as Agent::LoadFolder. Alternate stream
    // folders are only traversed when explicitly bound, matching its default.
    QList<ArchiveRow> result; QList<ArchiveRowIdentity> stack; QSet<int> visited;
    auto push = [&](const ArchiveDirectory &dir) { for (qsizetype i = dir.children.size(); i-- > 0;) stack.append({dir.id, int(i)}); };
    visited.insert(id); push(*base);
    while (!stack.isEmpty()) {
        const auto identity = stack.takeLast(); const auto dir = directory(identity.directory);
        const auto &row = dir->children[identity.item]; result.append(row);
        if (const auto child = directory(row.childDirectory); child && !visited.contains(child->id)) { visited.insert(child->id); push(*child); }
    }
    return result;
}
ArchivePropertyList ArchiveFolderIndex::rowProperties(ArchiveRowIdentity identity, bool flat, int baseDirectory) const {
    const auto dir = directory(identity.directory); if (!dir || identity.item < 0 || identity.item >= dir->children.size()) return {};
    const auto &row = dir->children[identity.item];
    const auto source = row.archiveIndex >= 0 && row.archiveIndex < entries.size() ? &entries[row.archiveIndex] : nullptr;
    auto find = [](const ArchivePropertyList &properties, quint32 id) -> const ArchiveProperty * { for (const auto &p : properties) if (!p.raw && p.id == id) return &p; return nullptr; };
    auto definitions = metadata.schema; bool nameDefined = false;
    for (auto &definition : definitions) { if (definition.id == 3) { definition.id = 4; definition.name = "Name"; } if (definition.id == 4) nameDefined = true; }
    if (!nameDefined) definitions.prepend({4, 8, "Name", false});
    definitions.append({31, 19, "Folders", false}); definitions.append({32, 19, "Files", false});
    if (flat) definitions.append({30, 8, "Path Prefix", false}); definitions.append(metadata.rawSchema);
    ArchivePropertyList result;
    for (const auto &definition : definitions) {
        const auto override = flat ? find(row.flatProperties, definition.id) : nullptr;
        const auto synthesized = find(row.properties, definition.id);
        const ArchiveProperty *property = !definition.raw ? override ? override : synthesized : nullptr;
        if (!property && source) for (const auto &p : source->orderedProperties) if (p.raw == definition.raw && p.id == definition.id) { property = &p; break; }
        if (property) append(result, *property);
        else if (!definition.raw && definition.id == 30) {
            QString prefix = dir->path; const auto base = directory(baseDirectory);
            if (base && prefix.startsWith(base->path)) prefix.remove(0, base->path.size());
            ArchiveProperty p{"Path Prefix", prefix}; p.id = 30; p.type = 8; p.native = true; append(result, p);
        }
    }
    return result;
}
ArchivePropertyList ArchiveFolderIndex::nativeSelectionProperties(const QList<ArchiveRowIdentity> &identities, bool flat, int baseDirectory) const {
    if (identities.isEmpty()) return {};
    if (identities.size() == 1) return rowProperties(identities.first(), flat, baseDirectory);
    ArchiveFolderTotals selected;
    for (const auto identity : identities) {
        const auto dir = directory(identity.directory); if (!dir || identity.item < 0 || identity.item >= dir->children.size()) continue;
        const auto &row = dir->children[identity.item]; const auto child = directory(row.childDirectory);
        if (child) { selected.folders += 1 + child->totals.folders; selected.files += child->totals.files; }
        else ++selected.files;
        for (const auto &p : rowProperties(identity, flat, baseDirectory)) {
            if (!p.raw && p.id == 7) selected.size += p.number.toULongLong();
            if (!p.raw && p.id == 8) selected.packed += p.number.toULongLong();
        }
    }
    ArchivePropertyList result{{{}, QString::number(identities.size()) + " object(s) selected"}};
    if (selected.folders) append(result, "Folders", QString::number(selected.folders));
    if (selected.files) append(result, "Files", QString::number(selected.files));
    append(result, "Size", QString::number(selected.size)); append(result, "Packed Size", QString::number(selected.packed)); return result;
}
ArchivePropertyList ArchiveFolderIndex::directoryProperties(int id) const {
    const auto dir = directory(id); if (!dir) return {};
    // PanelMenu::Properties displays folder Path using the Name label, then
    // Agent's five kFolderProps. Folder Type/GetName are not displayed here.
    ArchivePropertyList result;
    for (auto property : dir->properties) if (property.id == 3) { property.id = 4; property.name = "Name"; append(result, property); }
    for (quint32 id : {7U, 8U, 31U, 32U, 19U}) for (const auto &property : dir->properties) if (property.id == id) { append(result, property); break; }
    return result;
}
const ArchiveEntry *ArchiveFolderIndex::entry(QString path) const {
    path = normalized(path);
    if (metadata.native && metadata.folders.contains(path)) { const auto id = metadata.folders.value(path).archiveIndex; if (id >= 0 && id < entries.size() && entries[id].archiveIndex == id) return &entries[id]; }
    const auto found = positions.constFind(path); return found == positions.cend() ? nullptr : &entries[*found];
}
ArchivePropertyList ArchiveFolderIndex::itemProperties(QString path, bool flat, QString prefix, qint64 archiveIndex) const {
    const bool folderPath = path.endsWith('/'); path = normalized(path); const auto source = archiveIndex >= 0 && archiveIndex < entries.size() && entries[archiveIndex].archiveIndex == archiveIndex ? &entries[archiveIndex] : entry(path); const bool directory = folderPath || (source ? source->directory : folders.contains(path));
    if (!source && !directory) return {};
    ArchivePropertyList result; auto properties = source ? ordered(*source) : ArchivePropertyList();
    if (!source) {
        if (metadata.native) for (const auto &definition : metadata.schema) { ArchiveProperty p{definition.name, {}}; p.id = definition.id; p.native = true; properties.append(p); }
        else for (const auto &name : schema) properties.append({name, {}});
    }
    const auto total = totals(path); bool hasName = false;
    for (auto property : properties) {
        if ((!property.native && property.name == "Path") || (property.native && !property.raw && property.id == 3)) { property.name = "Name"; property.id = 4; property.value = path.section('/', -1); property.type = 8; append(result, property); hasName = true; continue; }
        if (directory) {
            if (property.name == "Folder") property.value = "+";
            else if (!flat && property.name == "Size") { property.value = property.number = QString::number(total.size); property.type = 21; }
            else if (!flat && property.name == "Packed Size") { property.value = property.number = QString::number(total.packed); property.type = 21; }
            else if (property.name == "CRC" && property.value.isEmpty() && total.crcDefined) property.value = QString::number(total.crc, 16).rightJustified(8, '0').toUpper();
        }
        append(result, property);
    }
    if (!hasName) { ArchiveProperty p{"Name", path.section('/', -1)}; p.id = 4; p.type = 8; p.native = metadata.native; result.prepend(p); }
    auto extra = [&](quint32 id, QString name, QString value, quint16 type) { ArchiveProperty p{name, value}; p.id = id; p.type = type; p.native = metadata.native; if (type == 19) p.number = value; append(result, p); };
    // AgentFolder appends synthesized typed fields before the raw schema.
    ArchivePropertyList raw; result.removeIf([&](const ArchiveProperty &p) { if (!p.raw) return false; raw.append(p); return true; });
    if (directory) { extra(31, "Folders", QString::number(total.folders), 19); extra(32, "Files", QString::number(total.files), 19); }
    if (flat) { auto folder = parent(path); if (!folder.isEmpty()) folder += '/'; if (folder.startsWith(prefix)) folder.remove(0, prefix.size()); extra(30, "Path Prefix", folder, 8); }
    result.append(raw);
    return result;
}
ArchivePropertyList ArchiveFolderIndex::selectionProperties(const QStringList &paths, bool flat, QString prefix) const {
    if (paths.isEmpty()) return {};
    if (paths.size() == 1) return itemProperties(paths.first(), flat, prefix);
    ArchiveFolderTotals selected;
    for (const auto &path : paths) {
        const auto item = entry(path); const bool directory = path.endsWith('/') || (item ? item->directory : folders.contains(normalized(path)));
        if (directory) { const auto total = totals(path); selected.folders += 1 + total.folders; selected.files += total.files; selected.size += flat ? item ? item->size : 0 : total.size; selected.packed += flat ? item ? item->packed : 0 : total.packed; }
        else if (item) { ++selected.files; selected.size += item->size; selected.packed += item->packed; }
    }
    ArchivePropertyList result{{{}, QString::number(paths.size()) + " object(s) selected"}};
    if (selected.folders) append(result, "Folders", QString::number(selected.folders));
    if (selected.files) append(result, "Files", QString::number(selected.files));
    append(result, "Size", QString::number(selected.size)); append(result, "Packed Size", QString::number(selected.packed)); return result;
}
ArchivePropertyList ArchiveFolderIndex::folderProperties(QString prefix) const {
    const auto total = totals(prefix); ArchivePropertyList result;
    append(result, "Name", prefix); append(result, "Size", QString::number(total.size)); append(result, "Packed Size", QString::number(total.packed));
    append(result, "Folders", QString::number(total.folders)); append(result, "Files", QString::number(total.files));
    if (total.crcDefined) append(result, "CRC", QString::number(total.crc, 16).rightJustified(8, '0').toUpper()); return result;
}
ArchivePropertyList archiveLayerProperties(const ArchiveLayer &layer, QString outerPath) {
    if (!layer.properties.isEmpty() && layer.properties.first().native) {
        ArchivePropertyList result;
        // The native adapter already follows Agent special fields, then the
        // complete handler archive schema. Duplicate PROPIDs are intentional.
        for (auto property : layer.properties) {
            if (property.id == 69 && !property.value.isEmpty()) result.append(ArchiveProperty{"Open WARNING:", "Cannot open the file as expected archive type"});
            if (property.id == 3 && !outerPath.isEmpty()) property.value = outerPath;
            append(result, property);
        }
        return result;
    }
    static const QStringList special{"Path", "Type", "Error Type", "Error", "Error Flags", "Warning", "Warning Flags", "Offset", "Physical Size", "Tail Size"};
    ArchivePropertyList result;
    for (const auto &name : special) for (const auto &property : layer.properties) if (property.name == name) {
        if (name == "Error Type" && !property.value.isEmpty()) result.append(ArchiveProperty{"Open WARNING:", "Cannot open the file as expected archive type"});
        append(result, name, name == "Path" && !outerPath.isEmpty() ? outerPath : property.value); break;
    }
    for (const auto &property : layer.properties) if (!special.contains(property.name)) append(result, property.name, property.value); return result;
}
