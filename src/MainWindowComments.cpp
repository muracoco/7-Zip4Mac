// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "PanelDisplay.h"
#include "UiLanguage.h"
#include <QDir>
#include "ComboDialog.h"
#include <QMessageBox>
#include <QtConcurrent>

namespace { constexpr int PathRole = Qt::UserRole, ParentRole = Qt::UserRole + 2; }
bool MainWindow::canComment() const {
    auto item = files->currentItem();
    if (!item || item->data(0, ParentRole).toBool()) return false;
    if (!archivePath.isEmpty()) {
        if (archiveType.compare("zip", Qt::CaseInsensitive) != 0 || !canUpdateArchive()) return false;
        if (canUseNativeArchiveUpdate()) {
            const auto index = item->data(0, Qt::UserRole + 110).toLongLong();
            return index >= 0 && index < archiveEntries.size();
        }
        auto path = item->data(0, PathRole).toString(); while (path.endsWith('/')) path.chop(1);
        for (const auto &entry : archiveEntries) if (entry.path == path) return true;
        return false;
    }
    return QFileInfo(item->data(0, PathRole).toString()).absolutePath() == fsPath;
}
void MainWindow::cancelComment() {
    if (commentControl) commentControl->cancel();
    if (commentDialog) commentDialog->reject();
}
void MainWindow::finishComment(QString error) {
    commentInProgress = false; commentControl.reset();
    updateState(); if (auto other = otherPanel()) other->updateState();
    if (!error.isEmpty()) {
        auto warning = new QMessageBox(QMessageBox::Warning, "7-Zip", "Comment: " + commentFocus + '\n' + error, QMessageBox::Ok, this);
        warning->setAttribute(Qt::WA_DeleteOnClose); warning->open();
    }
}
void MainWindow::setupComments() {
    connect(&commentReader, &QFutureWatcher<FileCommentDocument>::finished, this, [this] {
        const auto document = commentReader.result();
        if (document.cancelled) { finishComment(); return; }
        if (!document.error.isEmpty()) { finishComment(document.error); return; }
        auto dialog = new ComboDialog(commentName + " : " + UiLanguage::resource(6400), UiLanguage::resource(6401), FileComments::display(document.value(commentName)), this); commentDialog = dialog;
        dialog->setObjectName("commentDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(dialog, &QDialog::finished, this, [this, document, dialog](int answer) {
            commentDialog = nullptr;
            if (answer != QDialog::Accepted) { finishComment(); return; }
            const auto value = dialog->textValue(); const auto name = commentName; const auto stop = commentControl;
            if (progressDialog) progressDialog->close(); progressDialog = new ProgressDialog("Comment", commentFocus, this); progressDialog->setAttribute(Qt::WA_DeleteOnClose);
            connect(progressDialog, &ProgressDialog::cancelRequested, this, &MainWindow::cancelComment);
            connect(progressDialog, &ProgressDialog::pauseRequested, this, [this, stop](bool pause) { stop->setPaused(pause); if (progressDialog) progressDialog->setPaused(pause); });
            connect(progressDialog, &ProgressDialog::backgroundRequested, this, [stop](bool background) { stop->setBackground(background); });
            progressDialog->setPauseAvailable(true); progressDialog->show();
            commentWriter.setFuture(QtConcurrent::run([document, name, value, stop] { return FileComments::write(document, name, value, stop); }));
        });
        dialog->open();
    });
    connect(&commentWriter, &QFutureWatcher<QString>::finished, this, [this] {
        const auto error = commentWriter.result(); if (progressDialog) progressDialog->finishFile(error);
        finishComment();
        if (!error.isEmpty()) return;
        const auto selection = commentBrowseSelection;
        showFilesystem(fsPath, [this, selection] { restoreBrowseSelection(selection); });
    });
}
void MainWindow::comment() {
    if (operationBusy() || !canComment()) return;
    commentFocus = files->currentItem()->data(0, PathRole).toString(); commentName = QFileInfo(commentFocus).fileName(); commentSelection = markedPaths();
    commentBrowseSelection = saveBrowseSelection();
    commentNativeSelection.clear();
    if (!archivePath.isEmpty()) {
        auto path = commentFocus; while (path.endsWith('/')) path.chop(1);
        ArchiveRequest request; request.operation = ArchiveOperation::Comment; request.archive = archivePath; request.files = {path}; request.format = "zip"; request.password = archivePassword;
        QString value;
        if (canUseNativeArchiveUpdate()) {
            setArchiveSelection(request, {files->currentItem()});
            ArchiveRequest view; setArchiveSelection(view, files->markedItems()); commentNativeSelection = view.selection;
            value = archiveEntries[request.selection.first().archiveIndex].properties.value("Comment");
        } else for (const auto &entry : archiveEntries) if (entry.path == path) value = entry.properties.value("Comment");
        auto dialog = new ComboDialog(panelItemName(files->currentItem()) + " : " + UiLanguage::resource(6400), UiLanguage::resource(6401), value, this); commentDialog = dialog; commentInProgress = true;
        dialog->setObjectName("commentDialog"); dialog->setAttribute(Qt::WA_DeleteOnClose);

        connect(dialog, &QDialog::finished, this, [this, dialog, request](int answer) mutable {
            commentDialog = nullptr; commentInProgress = false;
            if (answer == QDialog::Accepted) { request.comment = dialog->textValue(); run(request); }
            else { updateState(); if (auto other = otherPanel()) other->updateState(); }
        });
        updateState(); if (auto other = otherPanel()) other->updateState(); dialog->open(); return;
    }
    commentInProgress = true; commentControl = std::make_shared<OperationControl>(); const auto folder = fsPath; const auto stop = commentControl;
    commentReader.setFuture(QtConcurrent::run([folder, stop] { return FileComments::read(folder, stop); }));
    updateState(); if (auto other = otherPanel()) other->updateState();
}
