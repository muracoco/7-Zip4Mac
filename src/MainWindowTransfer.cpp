// SPDX-License-Identifier: LGPL-3.0-or-later
// Qt workflow for official App.cpp::OnCopy / Agent::CopyTo / CopyFrom.
#include "MainWindow.h"
#include "CopyDialog.h"
#include "FileListItem.h"
#include "PanelDisplay.h"
#include "UiLanguage.h"
#include "OverwriteDialog.h"
#include <QDir>
#include <QFileInfo>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QScopedValueRollback>

namespace {
QString folderPrefix(QString path) { return path.endsWith('/') ? path : path + '/'; }
QString normalizedDestination(const QString &base, const QString &value) {
    // CorrectFsPath is identity on non-Windows platforms. Keep the final
    // slash: FSFolder uses it to distinguish a filename from a directory.
    auto path = QDir::cleanPath(QDir(base).absoluteFilePath(value));
    return value.endsWith('/') ? folderPrefix(path) : path;
}
QList<CopySummaryItem> summaryItems(const QList<QTreeWidgetItem *> &items, bool filesystem, const QString &base) {
    QList<CopySummaryItem> summary;
    for (auto item : items) {
        CopySummaryItem entry; entry.directory = item->data(0, Qt::UserRole + 1).toBool();
        const auto path = item->data(0, Qt::UserRole).toString();
        entry.name = filesystem ? QDir(base).relativeFilePath(path) : path;
        // The archive row's Prefix property plus raw Name is GetItemRelPath.
        if (!filesystem) { entry.name = path; if (entry.name.startsWith(base)) entry.name.remove(0, base.size()); while (entry.name.endsWith('/')) entry.name.chop(1); }
        if (auto file = dynamic_cast<FileItem *>(item)) {
            const auto value = file->sortInfo.values.constFind(quint64(OfficialSort::kpidSize) << 1);
            if (value != file->sortInfo.values.cend() && !value->retrievalError && (value->type == 21 || value->type == 19 || value->type == 18)) entry.size = value->unsignedValue;
        }
        summary.append(entry);
    }
    return summary;
}
}

void MainWindow::transfer(bool move, bool copyToSame) {
    if (operationBusy()) return;
    auto items = selectedItems();
    if (copyToSame) { const auto focus = files->currentItem(); if (!focus || focus->data(0, Qt::UserRole + 2).toBool()) return; items = {focus}; }
    if (items.isEmpty()) return;
    if (move && !archivePath.isEmpty() && !canUpdateArchive()) { QMessageBox::warning(this, "7-Zip", UiLanguage::resource(6001) + "\n" + UiLanguage::text("The operation is not supported.")); return; }
    auto other = otherPanel();
    const auto sourceAddress = archivePath.isEmpty() ? folderPrefix(fsPath) : archiveLocation();
    const auto base = archivePath.isEmpty() ? fsPath : sourceAddress;
    QString initial;
    if (copyToSame) initial = panelItemName(items.first());
    else if (other) initial = other->archivePath.isEmpty() ? folderPrefix(other->fsPath) : other->archiveLocation();
    else initial = archivePath.isEmpty() ? folderPrefix(fsPath) : folderPrefix(QFileInfo(parentArchives.isEmpty() ? archivePath : parentArchives.first().path).absolutePath());
    CopyDialog dialog(UiLanguage::resource(move ? 6001 : 6000), UiLanguage::resource(move ? 6003 : 6002), initial,
        copyItemsInfo(summaryItems(items, archivePath.isEmpty(), archivePath.isEmpty() ? fsPath : archivePrefix), sourceAddress), QSettings().value("Copy/History").toStringList(), this, base);
    UiLanguage::bindTitle(&dialog, move ? 6001 : 6000); UiLanguage::bind(dialog.findChild<QLabel *>("copyLabel"), move ? 6003 : 6002);
    bool accepted;
    { QScopedValueRollback guard(copyDialogActive, true); updateState(); if (other) other->updateState(); accepted = dialog.exec() == QDialog::Accepted; }
    updateState(); if (other) other->updateState();
    if (!accepted) return;
    if (dialog.textValue().isEmpty() || dialog.textValue().contains(QChar::Null)) { QMessageBox::warning(this, "7-Zip", UiLanguage::text("The operation is not supported.")); return; }
    auto target = normalizedDestination(base, dialog.textValue());
    if (QDir::cleanPath(target) == QDir::cleanPath(sourceAddress)) { QMessageBox::warning(this, "7-Zip", "Cannot copy files onto itself"); return; }
    const bool destinationArchive = !copyToSame && other && !other->archivePath.isEmpty() && QDir::cleanPath(target) == QDir::cleanPath(other->archiveLocation());
    if (destinationArchive && !other->canUpdateArchive()) { QMessageBox::warning(this, "7-Zip", "This archive does not support the requested update."); return; }
    // ArchiveFolder.cpp::CAgentFolder::CopyTo returns E_NOTIMPL for Move.
    // Preserve the source-defined error; do not extract and delete its input.
    if (move && !archivePath.isEmpty()) { QMessageBox::warning(this, "7-Zip", "Move\n" + archiveLocation() + "\nThe operation is not supported.\n7-Zip Agent: E_NOTIMPL (0x80004001)"); return; }
    QStringList paths; for (auto item : items) paths << item->data(0, Qt::UserRole).toString();
    // Multiple inputs always address a directory, even without a final slash.
    if (paths.size() != 1) target = folderPrefix(target);
    if (destinationArchive || !archivePath.isEmpty()) CopyDialog::remember(target);
    auto state = std::make_shared<TransferOperation>(); state->source = this; state->destination = copyToSame ? nullptr : other;
    state->sourceSelection = saveBrowseSelection(); if (state->destination) state->destinationSelection = state->destination->saveBrowseSelection();
    state->move = move; state->copyToSame = copyToSame;
    for (auto item : items) {
        if (archivePath.isEmpty()) state->copyNames << QDir(fsPath).relativeFilePath(item->data(0, Qt::UserRole).toString());
        else state->copyNames << item->data(0, PanelCopyNameRole).toString();
    }
    if (destinationArchive) {
        state->addRequest.operation = ArchiveOperation::Add; state->addRequest.archive = other->archivePath;
        state->addRequest.format = other->archiveType.toLower(); state->addRequest.password = other->archivePassword;
        state->addRequest.useArchiveDefaults = true; state->addRequest.archivePrefix = other->archivePrefix;
        state->addRequest.workingDirectory = fsPath; state->addRequest.deleteAfter = move;
    }
    if (!archivePath.isEmpty() && destinationArchive) {
        state->temporary = std::make_shared<QTemporaryDir>(archiveWorkingFolder() + "/.7zip-copy-XXXXXX");
        if (!state->temporary->isValid()) { QMessageBox::warning(this, "7-Zip", "Cannot create temporary Copy directory."); return; }
    }
    transferOperation = state; if (state->destination) state->destination->transferOperation = state;
    if (!archivePath.isEmpty()) {
        state->phase = TransferOperation::Extract;
        ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = archivePath; request.password = archivePassword;
        request.outputDirectory = state->temporary ? state->temporary->path() : target;
        request.files = paths; for (auto &path : request.files) while (path.endsWith('/')) path.chop(1);
        request.pathMode = flatView ? "flat" : "full"; request.overwriteMode = "ask"; setArchiveSelection(request, items); run(request);
    } else if (destinationArchive) {
        state->phase = TransferOperation::Add; state->addRequest.files = state->copyNames; other->run(state->addRequest);
    } else {
        state->phase = TransferOperation::Filesystem; fileProgress(move ? "Move" : "Copy", target); fileOps.transfer(paths, target, move); updateState();
    }
}

bool MainWindow::finishFilesystemTransfer(const QString &error) {
    if (!transferOperation || transferOperation->source != this || transferOperation->phase != TransferOperation::Filesystem) return false;
    if (overwriteDialog) overwriteDialog->close(); if (progressDialog) progressDialog->finishFile(error); completeTransfer(); return true;
}
bool MainWindow::finishTransfer(const ArchiveResult &result) {
    const auto state = transferOperation;
    if (!state || (state->phase == TransferOperation::Extract ? state->source != this || result.operation != ArchiveOperation::Extract : state->phase != TransferOperation::Add || state->destination != this || result.operation != ArchiveOperation::Add)) return false;
    if (result.success && result.target == archivePath && !lastRequest.password.isEmpty()) archivePassword = lastRequest.password;
    if (result.passwordRequired && !result.cancelled) {
        bool ok; const auto password = QInputDialog::getText(this, "Password", "Enter password:", QLineEdit::Password, {}, &ok);
        if (ok) { auto retry = lastRequest; retry.password = password; run(retry); return true; }
    }
    if (progressDialog) progressDialog->finish(result);
    if (result.success && state->phase == TransferOperation::Extract && state->temporary) {
        auto destination = state->destination;
        if (!destination) { completeTransfer(); return true; }
        // OnCopy sends every original selected output name, in selected order,
        // after temporary extraction. Keep duplicates and omitted/renamed
        // outputs: CopyFrom's original enumerator must decide their result.
        state->phase = TransferOperation::Add; state->addRequest.files = state->copyNames;
        state->addRequest.workingDirectory = state->temporary->path();
        lastRequest.password.fill(QChar::Null); lastRequest.password.clear(); destination->run(state->addRequest); return true;
    }
    lastRequest.password.fill(QChar::Null); lastRequest.password.clear(); completeTransfer(); return true;
}
void MainWindow::reloadTransferPanel(BrowseSelection selection, std::function<void()> continuation) {
    if (archivePath.isEmpty()) {
        auto connection = std::make_shared<QMetaObject::Connection>(); const auto path = fsPath;
        *connection = connect(this, &MainWindow::directoryLoaded, this, [this, connection, path, selection, continuation](QString loaded, bool success) {
            if (QDir::cleanPath(loaded) != QDir::cleanPath(path)) return; disconnect(*connection); if (success) restoreBrowseSelection(selection); if (continuation) continuation();
        });
        showFilesystem(path);
    } else {
        if (continuation) {
            auto connection = std::make_shared<QMetaObject::Connection>();
            *connection = connect(&backend, &ArchiveBackend::finished, this, [this, connection, continuation](ArchiveResult result) {
                if (result.operation != ArchiveOperation::List || backend.busy()) return;
                disconnect(*connection); continuation();
            });
        }
        pendingArchive = archivePath; pendingArchiveTail.clear(); pendingReopen = false; refreshSelection = selection;
        ArchiveRequest request; request.archive = archivePath; request.password = archivePassword; request.readMode = archiveReadMode; run(request);
    }
}
void MainWindow::completeTransfer() {
    auto state = transferOperation; if (!state) return;
    auto source = state->source, destination = state->destination;
    // Original OnCopy restores both lists, kills source marks (also on Cancel)
    // and only then returns focus. Keep the asynchronous equivalent busy until
    // both reloads finish; there must be no idle gap between the two panels.
    state->phase = TransferOperation::Refresh;
    if (!state->copyToSame) state->sourceSelection.marked.clear();
    const auto finish = [state, source, destination] {
        if (source && source->transferOperation == state) source->transferOperation.reset();
        if (destination && destination->transferOperation == state) destination->transferOperation.reset();
        if (source) source->listFocusPending = true;
        if (destination) destination->updateState();
        if (source) source->updateState();
    };
    const auto reloadSource = [source, selection = state->sourceSelection, finish] {
        if (source) source->reloadTransferPanel(selection, finish); else finish();
    };
    if (destination && destination != source) destination->reloadTransferPanel(state->destinationSelection, reloadSource);
    else reloadSource();
}
