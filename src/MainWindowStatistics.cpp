// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "FileListItem.h"
#include <QApplication>
#include <QHeaderView>
#include <QSettings>

namespace {
constexpr int PathRole = Qt::UserRole, DirRole = Qt::UserRole + 1, SizeRole = Qt::UserRole + 3;
QString sizeText(quint64 value) { return officialPanelSize(value); }
}

QStringList MainWindow::operatedFolders() const {
    auto items = selectedItems();
    QStringList paths;
    for (auto item : items) if (item->data(0, DirRole).toBool()) paths << item->data(0, PathRole).toString();
    return paths;
}

void MainWindow::setupFolderStatistics() {
    connect(&folderStatistics, &FolderStatistics::progress, this, [this](quint64 size, QString path) { if (progressDialog) progressDialog->update(-1, size, 0, path, false); });
    connect(&folderStatistics, &FolderStatistics::pausedChanged, this, [this](bool paused) { if (progressDialog) progressDialog->setPaused(paused); });
    connect(&folderStatistics, &FolderStatistics::pauseAvailabilityChanged, this, [this](bool available) { if (progressDialog) progressDialog->setPauseAvailable(available); });
    connect(&folderStatistics, &FolderStatistics::finished, this, [this](const FolderStatisticsResult &result) {
        QStringList errors;
        const bool sorted = files->isSortingEnabled(); files->setSortingEnabled(false);
        for (const auto &stat : result.items) {
            if (!stat.error.isEmpty()) { errors << stat.error; continue; }
            if (!stat.complete) continue;
            for (int n = 0; n < files->topLevelItemCount(); ++n) {
                auto item = files->topLevelItem(n);
                if (!archivePath.isEmpty() || item->data(0, PathRole).toString() != stat.path || !item->data(0, DirRole).toBool()) continue;
                for (const auto pair : {qMakePair(OfficialSort::kpidSize,stat.size),qMakePair(OfficialSort::kpidNumSubDirs,stat.folders),qMakePair(OfficialSort::kpidNumSubFiles,stat.files)}) {
                    const int column=propertyColumn(files,pair.first); if(column<0) continue;
                    item->setText(column,sizeText(pair.second)); item->setData(column,SizeRole,pair.second);
                    ArchiveProperty property; property.id=pair.first; property.type=pair.first==OfficialSort::kpidSize?21:19; property.number=QString::number(pair.second);
                    if (auto file=dynamic_cast<FileItem *>(item)) file->updateSortProperty(property);
                    item->setTextAlignment(column,Qt::AlignRight|Qt::AlignVCenter);
                }
            }
        }
        files->setSortingEnabled(sorted);
        if (result.cancelled) errors << "Operation cancelled.";
        if (progressDialog) progressDialog->finishFile(errors.join("\n\n"));
        updateState(); if (auto other = otherPanel()) other->updateState();
    });
}

void MainWindow::viewFocusedItem() {
    if (operationBusy()) return;
    const auto folders = operatedFolders();
    // The Windows CalcItemFullSize interface is provided by FSFolder only.
    // Selected folders take precedence over the focused file for F3, not F4.
    if (archivePath.isEmpty() && !folders.isEmpty()) {
        if (progressDialog) progressDialog->close();
        progressDialog = new ProgressDialog("Calculating", folders.join('\n'), this); progressDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(progressDialog, &ProgressDialog::cancelRequested, &folderStatistics, &FolderStatistics::cancel);
        connect(progressDialog, &ProgressDialog::pauseRequested, &folderStatistics, &FolderStatistics::setPaused);
        progressDialog->show(); folderStatistics.start(folders); updateState();
        if (auto other = otherPanel()) other->updateState();
    } else openExternal(false, settings.viewer, true);
}
