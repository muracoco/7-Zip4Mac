// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "PanelDisplay.h"
#include "PropertiesDialog.h"
#include <QApplication>

namespace {
constexpr int PathRole = Qt::UserRole;
void separator(ArchivePropertyList &rows, bool small = false) { rows.append(ArchiveProperty{small ? "----------------" : "------------------------", {}}); }
void addLayers(ArchivePropertyList &rows, const QList<ArchiveLayer> &layers, const QString &path) {
    for (qsizetype index = layers.size(); index-- > 0;) {
        separator(rows); rows.append(archiveLayerProperties(layers[index], index == 0 ? path : QString()));
        if (index > 0 && !layers[index - 1].childProperties.isEmpty()) { separator(rows, true); rows.append(archivePropertyRows(layers[index - 1].childProperties)); }
    }
}
void addFailed(ArchivePropertyList &rows, const ArchiveFolderIndex &index) {
    const auto failed = index.failedProperties(); if (failed.isEmpty()) return;
    separator(rows); separator(rows);
    rows.append(archiveLayerProperties({failed, {} }));
}
}
void MainWindow::setArchiveListing(const ArchiveResult &result) {
    const auto oldDirectory = archiveFolderIndex.directory(archiveDirectory);
    const bool alternate = oldDirectory && oldDirectory->alternateStreams && oldDirectory->path == archivePrefix;
    int mappedDirectory = -1;
    if (archiveFolderIndex.sourceSnapshot() == result.previousSnapshot) {
        // The original refresh returns to a surviving parent when the bound
        // folder is removed. Use graph parents, including native tree folders.
        int id = archiveDirectory;
        while (id >= 0 && id < result.directoryIndexMap.size()) {
            mappedDirectory = result.directoryIndexMap[id]; if (mappedDirectory >= 0) break;
            const auto dir = archiveFolderIndex.directory(id); if (!dir) break; id = dir->parent;
        }
    }
    archiveEntries = result.entries; archiveProperties = result.properties; archiveType = result.archiveType; archiveLayers = result.layers; archiveFolderIndex = ArchiveFolderIndex(archiveEntries, result.metadata);
    archiveDirectory = mappedDirectory >= 0 ? mappedDirectory : qMax(0, archiveFolderIndex.findDirectory(archivePrefix));
    if (mappedDirectory < 0 && alternate) for (int id = 0; const auto dir = archiveFolderIndex.directory(id); ++id) if (dir->alternateStreams && dir->path == archivePrefix) { archiveDirectory = id; break; }
    if (const auto dir = archiveFolderIndex.directory(archiveDirectory)) archivePrefix = dir->path;
}
void MainWindow::info() {
    if (operationBusy() || QApplication::activeModalWidget()) return;
    ArchivePropertyList rows;
    if (!archivePath.isEmpty()) {
        auto selected = archiveFolderIndex.selectionProperties(selectedPaths(), flatView, archivePrefix);
        const auto items = selectedItems();
        if (archiveFolderIndex.hasDirectories()) {
            QList<ArchiveRowIdentity> identities; for (const auto item : items) identities.append({item->data(0, Qt::UserRole + 112).toInt(), item->data(0, Qt::UserRole + 113).toInt()});
            selected = archiveFolderIndex.nativeSelectionProperties(identities, flatView, archiveDirectory);
        } else if (items.size() == 1 && items.first()->data(0, Qt::UserRole + 110).isValid()) selected = archiveFolderIndex.itemProperties(items.first()->data(0, PathRole).toString(), flatView, archivePrefix, items.first()->data(0, Qt::UserRole + 110).toLongLong());
        if (!selected.isEmpty()) { rows.append(selected); separator(rows); }
        rows.append(archiveFolderIndex.hasDirectories() ? archiveFolderIndex.directoryProperties(archiveDirectory) : archiveFolderIndex.folderProperties(archivePrefix)); addLayers(rows, archiveLayers, archiveVirtualPath); addFailed(rows, archiveFolderIndex);
        for (qsizetype index = parentArchives.size(); index-- > 0;) {
            const auto &parent = parentArchives[index]; separator(rows, true);
            if (const auto item = parent.folderIndex.entry(parent.child)) rows.append(archivePropertyRows(item->orderedProperties));
            addLayers(rows, parent.layers, parent.virtualPath);
            addFailed(rows, parent.folderIndex);
        }
    } else {
        // Native Windows filesystem Properties is a shell extension. This Mac
        // substitute uses the visible listing/F3 cache without an implicit scan.
        for (auto item : selectedItems()) {
            rows.append({"Name", panelItemName(item)});
            for (int column = 1; column < files->columnCount(); ++column) if (!item->text(column).isEmpty()) rows.append({files->headerItem()->text(column), item->text(column)});
            rows.append({"Path", item->data(0, PathRole).toString()}); separator(rows);
        }
        if (rows.isEmpty()) rows.append({"Path", pathBox->text()});
    }
    auto dialog = new PropertiesDialog(rows, this); dialog->setAttribute(Qt::WA_DeleteOnClose); dialog->show();
}
