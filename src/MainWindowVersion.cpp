// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "OverwriteDialog.h"
#include <QSettings>

void MainWindow::setupVersionControl() {
    connect(&versionControl, &VersionControl::byteProgress, this, [this](quint64 done, quint64 total, QString path) { if (progressDialog) progressDialog->update(total ? int(100.0 * done / total) : -1, done, total, path, false); });
    connect(&versionControl, &VersionControl::pausedChanged, this, [this](bool value) { if (progressDialog) progressDialog->setPaused(value); });
    connect(&versionControl, &VersionControl::pauseAvailabilityChanged, this, [this](bool available) { if (progressDialog) progressDialog->setPauseAvailable(available); });
    connect(&versionControl, &VersionControl::overwriteRequested, this, [this](OverwriteConflict conflict) {
        if (overwriteDialog) overwriteDialog->close();
        auto dialog = new OverwriteDialog(conflict, this, false, true); dialog->setAttribute(Qt::WA_DeleteOnClose); overwriteDialog = dialog;
        connect(dialog, &QDialog::finished, this, [this, dialog, id = conflict.id] { if (overwriteDialog == dialog) overwriteDialog.clear(); versionControl.resolveOverwrite(id, dialog->answer()); }); dialog->open();
    });
    connect(&versionControl, &VersionControl::finished, this, [this](VersionResult result) {
        if (overwriteDialog) overwriteDialog->close();
        if (progressDialog) progressDialog->finishFile(result.error);
        updateState();
        if (!result.diffPaths.isEmpty()) launchExternal(result.diffPaths, settings.diff);
        const auto selection = saveBrowseSelection(); showFilesystem(fsPath, [this, selection] { restoreBrowseSelection(selection); });
    });
}
void MainWindow::versionCommand(VersionCommand command) {
    if (operationBusy() || !archivePath.isEmpty()) return;
    const auto paths = markedPaths(); // Original VerCtrl uses marks, not the unmarked focus fallback.
    if (paths.size() != 1) return;
    const auto store = QSettings().value("7vc").toString(); if (store.isEmpty()) return;
    const auto name = "Ver " + QStringList{"Edit", "Commit", "Revert", "Diff"}.at(int(command));
    if (progressDialog) progressDialog->close();
    progressDialog = new ProgressDialog(name, paths.first(), this); progressDialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(progressDialog, &ProgressDialog::cancelRequested, &versionControl, &VersionControl::cancel);
    connect(progressDialog, &ProgressDialog::pauseRequested, &versionControl, &VersionControl::setPaused);
    progressDialog->show(); versionControl.start(command, paths.first(), store); updateState();
}
