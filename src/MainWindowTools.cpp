// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "PanelDisplay.h"
#include "PanelKey.h"
#include "AddressCombo.h"
#include "FileListItem.h"
#include "PanelSelection.h"
#include "FileToolDialogs.h"
#include "TemporaryFilesDialog.h"
#include "UiLanguage.h"
#include <QApplication>
#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QClipboard>

QStringList MainWindow::activeTemporaryPaths() const {
    QStringList paths = backend.temporaryPaths();
    if (operationBusy()) paths << QFileInfo(QDir::tempPath()).canonicalFilePath();
    if (externalTemp) paths << externalTemp->path();
    if (archiveTemporary) paths << archiveTemporary->path();
    for (const auto &frame : parentArchives) if (frame.temporary) paths << frame.temporary->path();
    if (nestedOpen && nestedOpen->temporary) paths << nestedOpen->temporary->path();
    if (nestedUpdate && nestedUpdate->temporary) paths << nestedUpdate->temporary->path();
    for (const auto &temp : retainedTemps) paths << temp->path();
    for (const auto &edit : externalEdits) if (edit->group && edit->group->temporary) paths << edit->group->temporary->path();
    return paths;
}
void MainWindow::deleteTemporaryFiles() {
    if (operationBusy()) return;
    TemporaryFilesDialog dialog(QFileInfo(backendExecutable).absolutePath() + "/7zz-progress", this, {}, [] {
        QStringList paths;
        for (auto widget : QApplication::allWidgets()) if (auto window = qobject_cast<MainWindow *>(widget)) paths << window->activeTemporaryPaths();
        paths.removeDuplicates(); return paths;
    });
    connect(&dialog, &TemporaryFilesDialog::openRequested, this, [this](QString path, bool manager) {
        if (manager) {
            QProcess application; application.setProgram(QCoreApplication::applicationFilePath()); application.setArguments({"--file-manager", path});
            if (!application.startDetached()) QMessageBox::warning(this, "7-Zip", "Open Outside : 7-Zip\n" + path + '\n' + application.errorString());
        }
        else QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    dialog.exec();
}

void MainWindow::copyNamesToClipboard() {
    if (operationBusy()) return;
    QStringList names;
    for (auto item : files->markedItems()) names << panelItemName(item);
    // PanelMenu.cpp::EditCopy copies names as text, not filesystem URLs.
    QApplication::clipboard()->setText(names.join("\r\n"));
}
void MainWindow::calculateHash(const QString &method) {
    if (operationBusy() || selectedPaths().isEmpty()) return;
    if (archivePath.isEmpty()) { executeOpenWith("hash:" + method, selectedPaths()); return; }
    ArchiveRequest request; request.operation = ArchiveOperation::HashArchive;
    request.archive = archivePath; request.password = archivePassword; request.hashMethod = method;
    for (auto path : selectedPaths()) { while (path.endsWith('/')) path.chop(1); request.files << path; }
    setArchiveSelection(request, selectedItems());
    for (const auto &entry : archiveEntries) if (!entry.directory) {
        for (const auto &path : request.files) if (entry.path == path || entry.path.startsWith(path + '/')) { request.totalSizeHint += entry.size; break; }
    }
    run(request);
}

void MainWindow::fileProgress(QString operation, QString target) {
    if (progressDialog) progressDialog->close();
    progressDialog = new ProgressDialog(operation, target, this); progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(progressDialog, &ProgressDialog::cancelRequested, &fileOps, &FileOperations::cancel); progressDialog->show();
    connect(progressDialog, &ProgressDialog::pauseRequested, &fileOps, &FileOperations::setPaused);
}
void MainWindow::splitFile() {
    auto paths = selectedPaths(); if (operationBusy() || !archivePath.isEmpty() || paths.size() != 1) return;
    auto other = otherPanel(); SplitDialog dialog(paths.first(), other && other->archivePath.isEmpty() ? other->fsPath : fsPath, this);
    if (dialog.exec() != QDialog::Accepted) return;
    fileProgress("Splitting", paths.first()); fileOps.split(paths.first(), QDir(fsPath).absoluteFilePath(dialog.destination()), dialog.volumeSizes()); updateState();
}
void MainWindow::combineFiles() {
    auto paths = selectedPaths(); if (operationBusy() || !archivePath.isEmpty() || paths.size() != 1) return;
    const auto plan = FileOperations::inspectVolumes(paths.first());
    if (!plan.error.isEmpty()) { QMessageBox::warning(this, "7-Zip", plan.error); return; }
    auto other = otherPanel(); CombineDialog dialog(paths.first(), other && other->archivePath.isEmpty() ? other->fsPath : fsPath, this);
    if (dialog.exec() != QDialog::Accepted) return;
    fileProgress("Combining", paths.first()); fileOps.combine(paths.first(), QDir(fsPath).absoluteFilePath(dialog.destination())); updateState();
}
void MainWindow::link() {
    auto paths = selectedPaths();
    if (operationBusy() || !archivePath.isEmpty() || paths.size() != 1) return;
    const auto selection = saveBrowseSelection();
    auto other = otherPanel(); LinkDialog dialog(paths.first(), other && other->archivePath.isEmpty() ? other->fsPath : QFileInfo(paths.first()).absolutePath(), this, fsPath);
    if (dialog.exec() == QDialog::Accepted) {
        showFilesystem(fsPath, [this, selection] { restoreBrowseSelection(selection); });
        if (other) other->refresh();
    }
}
void MainWindow::selectByType(bool select) {
    auto focused = files->currentItem(); if (!focused || focused->data(0, Qt::UserRole + 2).toBool()) return;
    const bool directory = focused->data(0, Qt::UserRole + 1).toBool();
    const auto name = panelItemName(focused); const int dot = name.lastIndexOf('.');
    for (int n = 0; n < files->topLevelItemCount(); ++n) {
        auto item = files->topLevelItem(n); if (item->data(0, Qt::UserRole + 2).toBool()) continue;
        const bool dir = item->data(0, Qt::UserRole + 1).toBool();
        const auto candidate = panelItemName(item); const int otherDot = candidate.lastIndexOf('.');
        const bool matches = directory ? dir : !dir && (dot < 0 ? otherDot < 0 : otherDot >= 0 && candidate.mid(otherDot).compare(name.mid(dot), Qt::CaseInsensitive) == 0);
        if (matches) files->setItemSelected(item, select);
    }
}
void MainWindow::saveBookmark(int index) {
    if (operationBusy()) return;
    QSettings().setValue("bookmark" + QString::number(index), archivePath.isEmpty() ? fsPath : archiveLocation()); updateBookmarks();
}
void MainWindow::openBookmark(int index) {
    if (dataOperationBusy()) return;
    const auto path = QSettings().value("bookmark" + QString::number(index)).toString(); if (path.isEmpty()) return;
    openPath(path);
}
void MainWindow::updateBookmarks() {
    for (int n = 0; n < 10; ++n) {
        const auto suffix = QString::number(n); auto entry = actions.value("bookmark" + suffix); if (!entry) continue;
        const auto path = QSettings().value("bookmark" + suffix).toString();
        entry->setText((path.isEmpty() ? UiLanguage::text("Bookmark") + ' ' + suffix : path) + "\tAlt+" + suffix);
    }
}
bool MainWindow::eventFilter(QObject *object, QEvent *event) {
    if (uiReady && object == files && (event->type() == QEvent::FontChange || event->type() == QEvent::ApplicationFontChange)) syncAddressFont();
    if (listFocusPending && (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress)) {
        if (auto widget = qobject_cast<QWidget *>(object); widget && isAncestorOf(widget)) listFocusPending = false;
    }
    if (browseFocus && (event->type() == QEvent::KeyPress || event->type() == QEvent::MouseButtonPress)) {
        auto widget = qobject_cast<QWidget *>(object);
        // A scan must not take focus back after the user types in the address
        // field, clicks another control or switches to the opposite panel.
        if (widget && isAncestorOf(widget) && widget != browseFocus && !browseFocus->isAncestorOf(widget)) {
            browseFocus.clear(); browseFocusDisplaced.clear();
        }
    }
    if (object->objectName() == "renameEditor" && event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
    if (event->type() == QEvent::ShortcutOverride || event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent *>(event); const auto widget = qobject_cast<QWidget *>(object);
        const bool inPanel = widget && (widget == files || widget == icons || files->isAncestorOf(widget) || icons->isAncestorOf(widget));
        const bool inAddress = widget && (widget == pathCombo || pathCombo->isAncestorOf(widget));
        if (inAddress && !key->modifiers().testFlag(Qt::MetaModifier)) {
            const auto plan = officialPanelKey(key->key(), key->modifiers());
            const bool enter = (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && (widget == pathBox || widget == pathCombo);
            const bool escape = key->key() == Qt::Key_Escape;
            const bool tab = key->key() == Qt::Key_Tab || key->key() == Qt::Key_Backtab;
            if (enter || escape || tab || plan.command == PanelKeyCommand::FocusPath || plan.command == PanelKeyCommand::Close || plan.command == PanelKeyCommand::TogglePanels) {
                if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
                if (enter) {
                    const int selected = pathCombo->currentIndex();
                    const auto text = pathCombo->view()->isVisible() && selected >= 0 ? pathCombo->itemData(selected).toString() : pathBox->text();
                    submitAddress(text);
                } else if (escape || tab) {
                    if (escape && filesystemRead) { cancelFilesystemRead(); updateState(); if (auto other = otherPanel()) other->updateState(); }
                    pathCombo->hidePopup(); if (escape) pathBox->setText(archivePath.isEmpty() ? fsPath : archiveLocation()); focusList();
                } else if (plan.command == PanelKeyCommand::FocusPath) focusAddress(plan.panel);
                else if (plan.command == PanelKeyCommand::TogglePanels) { auto root = embedded ? otherPanel() : this; if (root) { pathCombo->hidePopup(); root->setTwoPanels(!root->secondPanel); } }
                else { auto root = embedded ? otherPanel() : this; if (root) QTimer::singleShot(0, root, [root] { root->close(); }); }
                return true;
            }
        }
        if (inPanel && object->objectName() != "renameEditor") {
            const auto plan = officialPanelKey(key->key(), key->modifiers(), rightControlDown);
            if (plan.command != PanelKeyCommand::None || plan.consumed) {
                if (event->type() == QEvent::ShortcutOverride) { event->accept(); return true; }
                if (operationBusy() && plan.command != PanelKeyCommand::Close && plan.command != PanelKeyCommand::FocusPath) return true;
                switch (plan.command) {
                case PanelKeyCommand::Rename: rename(); break;
                case PanelKeyCommand::View: viewFocusedItem(); break;
                case PanelKeyCommand::Edit: openExternal(false, settings.editor, true); break;
                case PanelKeyCommand::NewFile: newFile(); break;
                case PanelKeyCommand::NewFolder: newFolder(); break;
                case PanelKeyCommand::Copy: transfer(false, plan.focusedOnly); break;
                case PanelKeyCommand::Move: transfer(true, plan.focusedOnly); break;
                case PanelKeyCommand::FocusPath: focusAddress(plan.panel); break;
                case PanelKeyCommand::OtherSameFolder: setOtherPanelFolder(true); break;
                case PanelKeyCommand::OtherSubFolder: setOtherPanelFolder(false); break;
                case PanelKeyCommand::SwitchPanel: if (auto other = otherPanel()) { other->focusList(); return true; } break;
                case PanelKeyCommand::TogglePanels: { auto root = embedded ? otherPanel() : this; if (root) root->setTwoPanels(!root->secondPanel); return true; }
                case PanelKeyCommand::Sort: sortByProperty(plan.value); return true;
                case PanelKeyCommand::Delete: remove(); break; // Both variants use confirmed Trash on macOS.
                case PanelKeyCommand::ClipboardCopy: copyNamesToClipboard(); break;
                case PanelKeyCommand::SelectAll: files->selection->selectAll(plan.focusedOnly); break;
                case PanelKeyCommand::SelectMask: actions[plan.focusedOnly ? "selectpattern" : "deselectpattern"]->trigger(); break;
                case PanelKeyCommand::SelectType: selectByType(plan.focusedOnly); break;
                case PanelKeyCommand::Invert: files->selection->invert(); break;
                case PanelKeyCommand::Parent: up(); break;
                case PanelKeyCommand::Refresh: refresh(); break;
                case PanelKeyCommand::Close: { auto root = embedded ? otherPanel() : this; if (root) QTimer::singleShot(0, root, [root] { root->close(); }); break; }
                case PanelKeyCommand::Comment: comment(); break;
                case PanelKeyCommand::ViewMode: setViewMode(plan.value); break;
                case PanelKeyCommand::History: actions["history"]->trigger(); break;
                case PanelKeyCommand::StoreBookmark: saveBookmark(plan.value); break;
                case PanelKeyCommand::Bookmark: openBookmark(plan.value); break;
                case PanelKeyCommand::None: break;
                }
                // Upstream Alt+arrows return false so the list can move focus.
                if (plan.consumed) return true;
            }
        }
    }
    // macOS virtual keys 59/62 distinguish left/right Control. Keep the
    // upstream RightCtrl+digit binding without reserving LeftCtrl+digit.
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
        auto key = static_cast<QKeyEvent *>(event);
        if (object->objectName() == "renameEditor") return QMainWindow::eventFilter(object, event);
        auto widget = qobject_cast<QWidget *>(object);
        if (event->type() == QEvent::KeyPress && key->key() == Qt::Key_Escape && filesystemRead && !QApplication::activeModalWidget() && widget &&
            (widget == this || isAncestorOf(widget)) && (!secondPanel || !secondPanel->isAncestorOf(widget))) {
            cancelFilesystemRead(); updateState(); if (auto other = otherPanel()) other->updateState(); return true;
        }
        if (key->key() == Qt::Key_Control && key->nativeVirtualKey() == 62) rightControlDown = event->type() == QEvent::KeyPress;
        if (event->type() == QEvent::KeyPress && rightControlDown && key->modifiers().testFlag(Qt::ControlModifier) && key->key() >= Qt::Key_0 && key->key() <= Qt::Key_9 &&
            (object == files || object == icons || files->isAncestorOf(qobject_cast<QWidget *>(object)) || icons->isAncestorOf(qobject_cast<QWidget *>(object)))) {
            const int index = key->key() - Qt::Key_0; if (key->modifiers().testFlag(Qt::ShiftModifier)) saveBookmark(index); else openBookmark(index); return true;
        }
    } else if (event->type() == QEvent::ApplicationDeactivate) rightControlDown = false;
    return QMainWindow::eventFilter(object, event);
}
