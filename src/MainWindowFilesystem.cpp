// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "AddressCombo.h"
#include "FileListItem.h"
#include "UiLanguage.h"
#include <QDir>
#include <QApplication>
#include <QElapsedTimer>
#include <QHeaderView>
#include <QMessageBox>
#include <QSettings>
#include <QSignalBlocker>
#include <QScopeGuard>
#include <QStackedWidget>

namespace {
constexpr int PathRole = Qt::UserRole, DirRole = Qt::UserRole + 1, ParentRole = Qt::UserRole + 2, SizeRole = Qt::UserRole + 3;
bool sameContents(const DirectorySnapshot &a, const DirectorySnapshot &b) {
    if (a.path != b.path || a.flat != b.flat || a.entries.size() != b.entries.size()) return false;
    for (qsizetype n = 0; n < a.entries.size(); ++n) {
        const auto &x = a.entries[n], &y = b.entries[n];
        if (x.path != y.path || x.name != y.name || x.prefix != y.prefix || x.suffix != y.suffix ||
            x.comment != y.comment || x.directory != y.directory || x.link != y.link ||
            x.size != y.size || x.packed != y.packed || x.inode != y.inode || x.links != y.links || x.mode != y.mode ||
            x.modified != y.modified || x.created != y.created || x.accessed != y.accessed || x.changed != y.changed ||
            x.modifiedFraction != y.modifiedFraction || x.createdFraction != y.createdFraction ||
            x.accessedFraction != y.accessedFraction || x.changedFraction != y.changedFraction) return false;
    }
    return true;
}

}

QString MainWindow::requestedDirectory() const { return filesystemRead ? filesystemRead->path : fsPath; }
void MainWindow::setupFilesystem() {
    filesystemChunks.setSingleShot(true);
    connect(&filesystemChunks, &QTimer::timeout, this, &MainWindow::prepareFilesystemRows);
    connect(&directoryScanner, &DirectoryScanner::finished, this, [this](DirectorySnapshot snapshot) {
        if (backgroundFilesystemRead && snapshot.generation == backgroundFilesystemRead->generation) {
            auto probe = std::move(backgroundFilesystemRead);
            if (snapshot.cancelled || !autoRefresh || closingRequested || !archivePath.isEmpty() ||
                snapshot.path != fsPath || snapshot.flat != flatView) return;
            if (operationBusy() || QApplication::activeModalWidget() || QApplication::activePopupWidget() ||
                files->property("renameEditing").toBool()) { watchDebounce.start(); return; }
            if (snapshot.error.isEmpty() && filesystemSnapshot && sameContents(*filesystemSnapshot, snapshot)) return;
            // Capture the current selection, not the selection from before the
            // worker scan: the unchanged view remained interactive meanwhile.
            const auto selection = saveBrowseSelection();
            probe->continuation = [this, selection] { restoreBrowseSelection(selection); };
            filesystemRead = std::move(probe);
            updateState(); if (auto other = otherPanel()) other->updateState();
        }
        if (!filesystemRead || snapshot.generation != filesystemRead->generation) return;
        if (snapshot.cancelled || !snapshot.error.isEmpty()) {
            listFocusPending = false;
            const auto path = snapshot.path, error = snapshot.error;
            cancelFilesystemRead(); updateState(); if (auto other = otherPanel()) other->updateState();
            if (!error.isEmpty()) {
                // A future completes inside posted-event dispatch. Entering
                // exec() here can strand Cocoa's nested timer dispatch. Return
                // to the event loop before the user acknowledges the warning.
                auto warning = new QMessageBox(QMessageBox::Warning, "7-Zip", error, QMessageBox::Ok, this);
                warning->setAttribute(Qt::WA_DeleteOnClose);
                connect(warning, &QDialog::finished, this, [this, path] { emit directoryLoaded(path, false); });
                warning->open();
            } else emit directoryLoaded(path, false);
            return;
        }
        filesystemRead->snapshot = std::move(snapshot);
        filesystemChunks.start(0);
    });
}
void MainWindow::cancelFilesystemRead() {
    filesystemChunks.stop(); directoryScanner.cancel(); filesystemRead.reset(); backgroundFilesystemRead.reset();
    if (files) files->cancelSorting();
    if (pathBox) pathBox->setText(archivePath.isEmpty() ? fsPath : archiveLocation());
}
void MainWindow::refreshFilesystemInBackground() {
    if (backgroundFilesystemRead) { watchDebounce.start(); return; }
    backgroundFilesystemRead = std::make_unique<FilesystemRead>();
    QSettings preferences;
    backgroundFilesystemRead->timePrecision = preferences.value("View/TimePrecision", 1).toInt();
    backgroundFilesystemRead->utc = preferences.value("View/UTC", false).toBool();
    backgroundFilesystemRead->path = fsPath;
    backgroundFilesystemRead->generation = directoryScanner.start(fsPath, flatView);
    // Watch notifications are hints. Reading a candidate must not toggle the
    // toolbar/status or replace an unchanged list and its native icons.
}
void MainWindow::showFilesystem(QString path, std::function<void()> continuation) {
    if (!parentArchives.isEmpty()) {
        leaveNestedArchives({}, [this, path, continuation = std::move(continuation)] { showFilesystem(path, continuation); }); return;
    }
    cancelFilesystemRead();
    filesystemRead = std::make_unique<FilesystemRead>();
    QSettings preferences; filesystemRead->timePrecision = preferences.value("View/TimePrecision", 1).toInt(); filesystemRead->utc = preferences.value("View/UTC", false).toBool();
    filesystemRead->path = QDir(path).absolutePath(); filesystemRead->continuation = std::move(continuation);
    filesystemRead->generation = directoryScanner.start(filesystemRead->path, flatView);
    // Keep the last complete view and its selection/header until the candidate
    // is ready. A failed or superseded read cannot commit navigation settings.
    pathBox->setText(filesystemRead->path); updateState(); if (auto other = otherPanel()) other->updateState();
}
void MainWindow::prepareFilesystemRows() {
    if (!filesystemRead) return;
    auto &read = *filesystemRead; QElapsedTimer slice; slice.start();
    if (!read.columns) read.columns=std::make_shared<const QList<ArchivePropertyDefinition>>(filesystemColumns(read.snapshot.flat));
    const auto &columns=*read.columns;
    const auto folderIcon=style()->standardIcon(QStyle::SP_DirIcon), fileIcon=style()->standardIcon(QStyle::SP_FileIcon);
    const auto end = qMin(read.next + 128, read.snapshot.entries.size());
    while (read.next < end) {
        const auto &entry = read.snapshot.entries[read.next++];
        auto item = new FileItem;
        ArchivePropertyList values;
        auto number=[&](quint32 id,quint64 value,quint16 type=21) { ArchiveProperty p; p.id=id; p.type=type; p.number=QString::number(value); p.value=p.number; values.append(p); };
        auto string=[&](quint32 id,QString value) { ArchiveProperty p; p.id=id; p.type=8; p.value=value; values.append(p); };
        auto time=[&](quint32 id,const QDateTime &date,const QString &fraction) { ArchiveProperty p; p.id=id; p.type=64; p.timeFraction=fraction; p.fileTime=QString::number(quint64(date.toSecsSinceEpoch()+11644473600LL)*10000000+fraction.leftJustified(9,'0').toULongLong()/100); values.append(p); };
        using namespace OfficialSort;
        string(kpidName,entry.name); string(kpidComment,entry.comment); string(kpidPrefix,entry.prefix);
        string(kpidExtension,entry.directory?UiLanguage::text("Folder"):entry.link?UiLanguage::text("Link"):entry.suffix);
        if (!entry.directory) { number(kpidSize,entry.size); number(kpidPackSize,entry.packed); }
        number(kpidINode,entry.inode); number(kpidLinks,entry.links,19);
        const auto attributes=officialPosixAttributes(entry.mode); number(kpidAttrib,attributes,19); values.last().value=officialAttributeText(attributes);
        time(kpidMTime,entry.modified,entry.modifiedFraction); time(kpidCTime,entry.created,entry.createdFraction);
        time(kpidATime,entry.accessed,entry.accessedFraction); time(kpidChangeTime,entry.changed,entry.changedFraction);
        for (int column=0;column<columns.size();++column) {
            for (const auto &value : values) if (value.id==columns[column].id) { if(!value.number.isEmpty()) item->setData(column,SizeRole,value.number.toULongLong()); break; }
            item->setTextAlignment(column,officialColumnAlignment(columns[column].id,columns[column].type));
        }
        item->setData(0,PanelNameRole,entry.name); item->setText(0,officialPanelName(entry.name));
        item->setDeferredProperties(values,read.columns,read.timePrecision,read.utc);
        item->setData(0,PanelIconIdentityRole,int(read.next-1));
        item->setData(0, PathRole, entry.path); item->setData(0, DirRole, entry.directory); item->setData(1, SizeRole, entry.directory ? 0 : entry.size);
        item->setSortProperties(values,read.next-1,entry.prefix);
        item->setIcon(0, entry.directory ? folderIcon : fileIcon);
        read.rows.append(item);
        if (slice.elapsed() >= 8) break;
    }
    if (read.next < read.snapshot.entries.size()) filesystemChunks.start(0); else installFilesystemRows();
}
void MainWindow::installFilesystemRows() {
    if (!filesystemRead) return;
    auto read = std::move(filesystemRead);
    const bool updates = viewStack->updatesEnabled(); viewStack->setUpdatesEnabled(false);
    const auto resumeUpdates = qScopeGuard([this, updates] { viewStack->setUpdatesEnabled(updates); });
    QSignalBlocker headerChanges(files->header()), rowChanges(files);
    fsPath = read->snapshot.path; archivePath.clear(); archivePrefix.clear(); archivePassword.fill(QChar::Null); archivePassword.clear();
    archiveEntries.clear(); archiveType.clear(); archiveReadMode.clear(); archiveProperties.clear(); archiveLayers.clear(); archiveFolderIndex = {}; archiveVirtualPath.clear(); pendingArchiveTail.clear();
    parentArchives.clear(); archiveTemporary.reset(); nestedOpen.reset();
    loadingView = true; files->archiveView = false; files->setProperty("archiveView", false); files->setSortingEnabled(false); files->clear();
    const auto columns=filesystemColumns(read->snapshot.flat); files->setColumnCount(columns.size()); QStringList labels;
    for (const auto &column : columns) labels << (column.name=="Type"?column.name:UiLanguage::propertyName(column.id,column.name,false));
    files->setHeaderLabels(labels);
    for (int n=0;n<columns.size();++n) { files->headerItem()->setData(n,ColumnPropertyRole,columns[n].id); files->headerItem()->setData(n,ColumnRawRole,false); files->headerItem()->setData(n,ColumnWidthRole,officialColumnWidth(columns[n].id,columns[n].type)); }
    if (settings.showDots) { auto parent = new FileItem(files, {".."}); parent->setData(0, ParentRole, true); parent->setData(0, DirRole, true); parent->setIcon(0, style()->standardIcon(QStyle::SP_ArrowUp)); }
    files->addTopLevelItems(read->rows); read->rows.clear();
    for (int n=0;n<columns.size();++n) { files->setColumnWidth(n,officialColumnWidth(columns[n].id,columns[n].type)); files->setColumnHidden(n,!officialColumnVisible(columns[n].id,true)); }
    const auto focused = QApplication::focusWidget();
    const bool editedAddress = pathBox->isModified() && focused &&
        (focused == pathCombo || pathCombo->isAncestorOf(focused));
    // A filesystem scan keeps the address editable. Preserve a newer user
    // draft when its candidate completes; Enter/Escape still commit/reset it.
    if (!editedAddress) pathBox->setText(fsPath);
    finishBrowse();
    if (settings.realIcons) {
        QList<PanelIconRequest> icons; icons.reserve(read->snapshot.entries.size());
        for (int n=0;n<read->snapshot.entries.size();++n) { const auto &entry=read->snapshot.entries[n]; icons.append({n,entry.path,entry.directory,false}); }
        files->loadNativeIcons(std::move(icons));
    }
    filesystemSnapshot = std::move(read->snapshot);
    if (read->continuation) read->continuation();
    updateState(); if (auto other = otherPanel()) other->updateState();
    emit directoryLoaded(fsPath, true);
}
