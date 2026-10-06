// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "AddressCombo.h"
#include "PanelDrag.h"
#include "ExternalProcess.h"
#include "UiLanguage.h"
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>

MainWindow::~MainWindow() {
    closingRequested = true;
    qApp->removeEventFilter(this); disconnect(qApp, nullptr, this, nullptr);
    for (auto child : findChildren<QObject *>()) disconnect(child, nullptr, this, nullptr);
    // QWidget's base destructor hides children after MainWindow's QString
    // members have been destroyed. QComboBox can call virtual hidePopup while
    // hiding, so its location callback must stop referencing those members.
    if (pathCombo) pathCombo->currentLocation = {};
    delete dragController; dragController = nullptr;
    // Member workers are destroyed after the UI state they reference. Their
    // cancellation signals must not enter callbacks on a partially torn-down
    // MainWindow (in particular its already-destroyed progress QPointer).
    disconnect(&backend, nullptr, this, nullptr);
    disconnect(&fileOps, nullptr, this, nullptr);
    disconnect(&versionControl, nullptr, this, nullptr);
    disconnect(&folderStatistics, nullptr, this, nullptr);
    disconnect(&directoryScanner, nullptr, this, nullptr);
    if (commentControl) commentControl->cancel();
    cancelFilesystemRead(); closeExternalSessions();
}

bool MainWindow::externalFileChanged(const ExternalEdit &edit) {
    struct stat current{};
    if (::lstat(QFile::encodeName(edit.path).constData(), &current) != 0) return false;
    return !S_ISREG(current.st_mode) || current.st_size != edit.stamp.st_size ||
        current.st_mtimespec.tv_sec != edit.stamp.st_mtimespec.tv_sec || current.st_mtimespec.tv_nsec != edit.stamp.st_mtimespec.tv_nsec;
}

bool MainWindow::rebindArchiveSelection(ArchiveRequest &selection, const ArchiveResult &result, bool requireAll) {
    if (selection.selectionDirectory < 0 || selection.selectionSnapshot != result.previousSnapshot || selection.selectionDirectory >= result.directoryIndexMap.size()) return false;
    auto updated = selection; updated.selection.clear(); updated.files.clear(); updated.selectionDirectory = result.directoryIndexMap[selection.selectionDirectory];
    if (updated.selectionDirectory < 0) return false;
    for (const auto &old : selection.selection) {
        ArchiveRowIdentity identity;
        if (old.identity.directory >= 0 && old.identity.directory < result.rowIdentityMap.size() && old.identity.item >= 0 && old.identity.item < result.rowIdentityMap[old.identity.directory].size())
            identity = result.rowIdentityMap[old.identity.directory][old.identity.item];
        if (identity.directory < 0 || identity.directory >= result.metadata.directories.size() || identity.item < 0 || identity.item >= result.metadata.directories[identity.directory].children.size()) { if (requireAll) return false; else continue; }
        const auto &row = result.metadata.directories[identity.directory].children[identity.item];
        updated.selection.append({identity, row.archiveIndex, row.name}); updated.files << row.path;
    }
    const auto &base = result.metadata.directories[updated.selectionDirectory];
    updated.selectionBasePath = base.path; updated.selectionAlternateStreams = base.alternateStreams;
    updated.selectionSnapshot = result.metadata.sourceSnapshot; updated.selectionType = result.archiveType; updated.selectionEntryCount = result.entries.size();
    selection = updated; return true;
}

void MainWindow::rebindArchiveSessions(const ArchiveResult &result, bool other) {
    if (!result.success || result.itemIndexMap.isEmpty()) return;
    const auto oldVirtualPath = archiveVirtualPath;
    for (const auto &edit : externalEdits) if (edit->archive == result.target) {
        const auto oldBase = edit->selection.selectionBasePath;
        if (rebindArchiveSelection(edit->selection, result) && edit->selection.files.size() == 1) {
            edit->entry = edit->selection.files[0];
            if (edit->location.endsWith(oldBase)) { edit->location.chop(oldBase.size()); edit->location += edit->selection.selectionBasePath; }
        }
    }
    for (auto &frame : parentArchives) if (frame.path == result.target && rebindArchiveSelection(frame.childSelection, result)) {
        if (frame.childSelection.files.size() == 1) frame.child = frame.childSelection.files[0];
        frame.entries = result.entries; frame.properties = result.properties; frame.type = result.archiveType; frame.layers = result.layers;
        frame.folderIndex = ArchiveFolderIndex(result.entries, result.metadata); frame.directory = frame.childSelection.selectionDirectory; frame.prefix = frame.childSelection.selectionBasePath;
    }
    for (qsizetype index = 1; index < parentArchives.size(); ++index)
        parentArchives[index].virtualPath = parentArchives[index - 1].virtualPath + '/' + parentArchives[index - 1].child;
    if (!parentArchives.isEmpty()) {
        archiveVirtualPath = parentArchives.last().virtualPath + '/' + parentArchives.last().child;
        if (archiveVirtualPath != oldVirtualPath) {
            pathBox->setText(archiveLocation());
            for (const auto &edit : externalEdits) if (edit->archive == archivePath && edit->location.startsWith(oldVirtualPath + '/'))
                edit->location = archiveVirtualPath + edit->location.mid(oldVirtualPath.size());
        }
    }
    if (other) if (auto panel = otherPanel()) {
        panel->rebindArchiveSessions(result, false);
        if (panel->archivePath == result.target && panel->archiveFolderIndex.sourceSnapshot() == result.previousSnapshot && !panel->panelBusy()) {
            panel->showUpdatedArchive(result);
            panel->updateState();
        }
    }
}

void MainWindow::showUpdatedArchive(const ArchiveResult &result) {
    ArchiveRequest selected, focused; setArchiveSelection(selected, files->markedItems()); setArchiveSelection(focused, {files->currentItem()});
    const bool focusedSelected = files->currentItem() && files->currentItem()->isSelected();
    const int oldFocus = files->indexOfTopLevelItem(files->currentItem());
    const bool reboundSelection = rebindArchiveSelection(selected, result, false), reboundFocus = rebindArchiveSelection(focused, result);
    setArchiveListing(result); showArchive(); files->clearSelection();
    bool restoredFocus = false;
    for (int index = 0; index < files->topLevelItemCount(); ++index) {
        auto item = files->topLevelItem(index); if (item->data(0, Qt::UserRole + 2).toBool()) continue;
        const auto matches = [&](const auto &row) { return row.identity.directory == item->data(0, Qt::UserRole + 112).toInt() && row.identity.item == item->data(0, Qt::UserRole + 113).toInt(); };
        if (reboundSelection) for (const auto &row : selected.selection) if (matches(row)) files->setItemSelected(item, true);
        if (reboundFocus && focused.selection.size() == 1 && matches(focused.selection.first())) { files->setCurrentItem(item, 0, settings.alternativeSelection && focusedSelected ? QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows : QItemSelectionModel::NoUpdate); restoredFocus = true; }
    }
    if (!restoredFocus && oldFocus >= 0 && files->topLevelItemCount() > 0)
        files->setCurrentItem(files->topLevelItem(qMin(oldFocus, files->topLevelItemCount() - 1)), 0, settings.alternativeSelection && focusedSelected ? QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows : QItemSelectionModel::NoUpdate);
}

void MainWindow::launchArchiveExternal(std::shared_ptr<QTemporaryDir> temporary, const QStringList &entries, const QString &command, const QString &password, const ArchiveRequest &selection) {
    auto group = std::make_shared<ExternalTempGroup>(); group->temporary = temporary;
    // Protect the files before launching. The last session decides cleanup;
    // one successful file must not erase a sibling's failed/recoverable edit.
    temporary->setAutoRemove(false);
    for (const auto &entry : entries) {
        const auto path = temporary->filePath(entry);
        if (QFileInfo(path).isDir() && !QFileInfo(path).isSymLink()) {
            retainedTemps.append(temporary); launchExternal({path}, command); continue;
        }
        auto edit = std::make_shared<ExternalEdit>(); edit->group = group; edit->path = path;
        edit->archive = archivePath; edit->location = archiveLocation(); edit->entry = entry; edit->format = archiveType.toLower();
        edit->password = password; edit->readOnly = !canUpdateArchive();
        if (selection.selectionDirectory >= 0) {
            edit->selection = selection; edit->selection.password.fill(QChar::Null); edit->selection.password.clear(); edit->selection.selection.clear();
            for (const auto &row : selection.selection) if (row.archiveIndex >= 0 && row.archiveIndex < archiveEntries.size() && archiveEntries[row.archiveIndex].path == entry) edit->selection.selection.append(row);
            if (edit->selection.selection.size() != 1) edit->readOnly = true;
        }
        if (::lstat(QFile::encodeName(path).constData(), &edit->stamp) != 0 || !S_ISREG(edit->stamp.st_mode)) {
            QMessageBox::warning(this, "7-Zip", "Cannot open a non-regular extracted file: " + path); continue;
        }
        // The observer outlives a closed panel and reaps its launcher. It owns
        // no right to stop the user's application, even on Manager destruction.
        auto observer = new ExternalProcess(qApp); edit->process = observer;
        connect(observer, &ExternalProcess::finished, observer, &QObject::deleteLater);
        connect(observer, &ExternalProcess::finished, this, [this, edit](int code) {
            if (edit->state == ExternalEdit::Closed) return;
            if (externalFileChanged(*edit)) edit->state = ExternalEdit::Ready;
            else if (code != 0) {
                edit->state = ExternalEdit::Done;
                QMessageBox::warning(this, "7-Zip", "The external application exited unsuccessfully.\nTarget: " + edit->path + "\nExit code: " + QString::number(code));
            } else edit->state = edit->process && edit->process->elapsed() < 2000 ? ExternalEdit::Complex : ExternalEdit::Done;
            externalDecisions.start();
        });
        QString error;
        if (!observer->start(command, path, temporary->path(), &error)) {
            observer->deleteLater(); QMessageBox::warning(this, "7-Zip", error); continue;
        }
        externalEdits.append(edit);
    }
}

void MainWindow::retainExternalEdit(const std::shared_ptr<ExternalEdit> &edit, QString error) {
    edit->group->preserve = true; edit->state = ExternalEdit::Done;
    QMessageBox::warning(this, "7-Zip", "Cannot update file\n'" + edit->path + "'\nThe modified file has been retained.\n" + error);
}

void MainWindow::considerExternalEdits() {
    if (closingRequested) return;
    if (operationBusy() || QApplication::activeModalWidget() || QApplication::activePopupWidget()) { externalDecisions.start(); return; }
    externalEdits.removeIf([](const auto &edit) { return edit->state == ExternalEdit::Done || edit->state == ExternalEdit::Closed; });
    for (const auto &edit : externalEdits) {
        if (edit->state != ExternalEdit::Ready) continue;
        externalDecision = true; updateState();
        if (!externalFileChanged(*edit)) edit->state = ExternalEdit::Done;
        else if (edit->readOnly) retainExternalEdit(edit, "The archive or a containing archive is read-only or cannot be updated.");
        else {
            const auto question = UiLanguage::text("File '{0}' was modified.\nDo you want to update it in the archive?").replace("{0}", QFileInfo(edit->entry).fileName());
            const auto answer = QMessageBox::question(this, "7-Zip", question, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
            if (answer != QMessageBox::Yes) edit->state = ExternalEdit::Done; // Upstream No and Cancel both discard.
            else if (archivePath != edit->archive || archiveLocation() != edit->location)
                retainExternalEdit(edit, "The current folder differs from the folder from which the file was opened.\nTarget: " + edit->archive);
            else if (!canUpdateArchive()) retainExternalEdit(edit, "The archive can no longer be updated.\nTarget: " + edit->archive);
            else {
                bool present = false;
                for (const auto &entry : archiveEntries) if (entry.path == edit->entry && !entry.directory && !entry.link) { present = true; break; }
                if (!present) retainExternalEdit(edit, "The original file no longer exists in the archive.\nTarget: " + edit->archive);
                else {
                    externalUpdating = edit; edit->state = ExternalEdit::Updating;
                    ArchiveRequest request = edit->selection; request.operation = ArchiveOperation::ReplaceFile; request.archive = edit->archive;
                    request.format = edit->format; request.files = {edit->entry}; request.replacementSource = edit->path; request.password = edit->password;
                    externalDecision = false; run(request); return;
                }
            }
        }
        externalDecision = false; updateState(); externalDecisions.start(); return;
    }
}

bool MainWindow::finishExternalUpdate(const ArchiveResult &incoming) {
    if (!externalUpdating) return false;
    auto result = incoming; const auto edit = externalUpdating;
    if (result.operation != ArchiveOperation::ReplaceFile || result.target != edit->archive) {
        result.success = false; result.passwordRequired = false; result.exitCode = -1;
        result.message = "Unexpected archive operation during editor write-back.";
    }
    if (result.passwordRequired) {
        bool ok; const auto password = QInputDialog::getText(this, "Password", "Enter password:", QLineEdit::Password, {}, &ok);
        if (ok) { edit->password = password; auto retry = lastRequest; retry.password = password; run(retry); return true; }
    }
    if (progressDialog) progressDialog->finish(result);
    if (result.success) {
        archivePassword = lastRequest.password;
        edit->state = ExternalEdit::Done; showUpdatedArchive(result);
    } else retainExternalEdit(edit, result.message + "\nTarget: " + result.target + "\nExit code: " + QString::number(result.exitCode) + '\n' + result.details);
    externalUpdating.reset(); lastRequest.password.fill(QChar::Null); lastRequest.password.clear();
    updateState(); externalDecisions.start(); return true;
}

void MainWindow::closeExternalSessions() {
    externalDecisions.stop();
    for (const auto &edit : externalEdits) {
        // Running applications and short-lived/unknown launchers can still be
        // using the extracted file. Closing the Manager never removes it.
        if (edit->state == ExternalEdit::Running || edit->state == ExternalEdit::Complex || edit->state == ExternalEdit::Updating ||
            (edit->state == ExternalEdit::Ready && externalFileChanged(*edit))) edit->group->preserve = true;
        edit->state = ExternalEdit::Closed; edit->password.fill(QChar::Null); edit->password.clear();
    }
    externalEdits.clear();
}
