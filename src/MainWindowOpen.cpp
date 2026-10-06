// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "PanelOpen.h"
#include "UiLanguage.h"
#include "OpenProfile.h"
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>

namespace {
constexpr int PathRole = Qt::UserRole, DirRole = Qt::UserRole + 1, ParentRole = Qt::UserRole + 2;
}

void MainWindow::openSelectedItems(bool tryInternal) {
    if (operationBusy()) return;
    const auto plan = panelOpenPlan(files, tryInternal);
    if (plan.tooManyItems) { QMessageBox::warning(this, "7-Zip", UiLanguage::resource(3016)); return; }
    for (const auto &action : plan.actions) {
        if (!action.item) continue;
        OpenCommand command;
        command.kind = static_cast<OpenCommand::Kind>(action.kind);
        command.path = action.item->data(0, PathRole).toString();
        command.parent = action.item->data(0, ParentRole).toBool();
        command.tryInternal = action.tryInternal;
        command.directory = action.item->data(0, Qt::UserRole + 111).isValid() ? action.item->data(0, Qt::UserRole + 111).toInt() : -1;
        if (!archivePath.isEmpty() && !command.parent) {
            command.selection.archive = archivePath;
            auto path = command.path; if (path.endsWith('/')) path.chop(1);
            command.selection.files = {path};
            setArchiveSelection(command.selection, {action.item}, true);
        }
        openCommands.append(command);
    }
    continueOpenCommands();
}

void MainWindow::continueOpenCommands() {
    if (backend.busy() || externalPending || filesystemRead || files->sortingBusy() || closingRequested) return;
    openCommandActive = false;
    while (!openCommands.isEmpty()) {
        const auto command = openCommands.takeFirst();
        // The original policy stops at the first internally opened folder.
        // Such navigation is therefore always the last planned action.
        if (command.kind == OpenCommand::BrowseFolder) {
            openCommands.clear();
            if (command.parent) up();
            else if (archivePath.isEmpty()) openPath(command.path);
            else if (command.directory >= 0) bindArchiveDirectory(command.directory);
            else { archivePrefix = command.path; showArchive(); }
            updateState(); return;
        }
        if (command.parent) {
            // Finder cannot address virtual archive folders. Open their real
            // containing directory instead of passing a nonexistent path.
            launchExternal({archivePath.isEmpty() ? QFileInfo(fsPath).absolutePath() : QFileInfo(archivePath).absolutePath()});
            continue;
        }
        if (command.kind == OpenCommand::OpenFile && command.tryInternal) {
            openCommands.clear();
            for (int row = 0; row < files->topLevelItemCount(); ++row) {
                auto item = files->topLevelItem(row);
                bool match = item->data(0, PathRole).toString() == command.path;
                if (command.selection.selection.size() == 1) {
                    const auto id = command.selection.selection.first().identity;
                    match = item->data(0, Qt::UserRole + 112).toInt() == id.directory && item->data(0, Qt::UserRole + 113).toInt() == id.item;
                }
                if (match && !item->data(0, ParentRole).toBool()) { activateItem(item); return; }
            }
            updateState(); return;
        }
        if (command.kind == OpenCommand::OpenFile && externalNameBlocked(command.path)) continue;
        if (archivePath.isEmpty()) { launchExternal({command.path}); continue; }
        // Each archive item gets its own extraction directory and native row
        // identity. Same-name siblings must not overwrite one another before
        // their external applications start.
        openCommandActive = true;
        startExternalExtraction(command.selection, false, {});
        if (externalPending) return;
        openCommandActive = false;
    }
    updateState();
}

void MainWindow::openExternalItems(const QList<QTreeWidgetItem *> &items, bool drag, const QString &command) {
    QStringList paths;
    for (auto item : items) if (item && !item->data(0, ParentRole).toBool()) {
        const auto path = item->data(0, PathRole).toString();
        if (!archivePath.isEmpty() && !drag && !item->data(0, DirRole).toBool() && externalNameBlocked(path)) return;
        paths << path;
    }
    if (paths.isEmpty()) return;
    if (!drag && paths.size() > 20) { QMessageBox::warning(this, "7-Zip", UiLanguage::resource(3016)); return; }
    if (archivePath.isEmpty()) { launchExternal(paths, command); return; }
    ArchiveRequest request; request.archive = archivePath;
    for (auto path : paths) { if (path.endsWith('/')) path.chop(1); request.files << path; }
    setArchiveSelection(request, items, true);
    startExternalExtraction(request, drag, command);
}

void MainWindow::startExternalExtraction(ArchiveRequest request, bool drag, const QString &command) {
    externalTemp = std::make_unique<QTemporaryDir>(archiveWorkingFolder() + "/.7zip-open-XXXXXX");
    if (!externalTemp->isValid()) { externalTemp.reset(); QMessageBox::warning(this, "7-Zip", "Cannot create temporary extraction directory."); return; }
    request.operation = ArchiveOperation::Extract; request.password = archivePassword;
    request.outputDirectory = externalTemp->path(); request.overwriteMode = "overwrite";
    externalPending = true; dragPending = drag; externalCommand = command; run(request);
}

bool MainWindow::externalNameBlocked(const QString &path) {
    const auto relative = archivePath.isEmpty() ? QDir(fsPath).relativeFilePath(path) : path.startsWith(archivePrefix) ? path.mid(archivePrefix.size()) : path;
    const auto warning = openProfileFilenameWarning(relative);
    if (warning.isEmpty()) return false;
    QMessageBox message(QMessageBox::Warning, "7-Zip", warning, QMessageBox::Ok, this);
    message.setTextFormat(Qt::PlainText); message.exec();
    return true;
}
