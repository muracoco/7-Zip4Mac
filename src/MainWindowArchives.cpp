// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "ArchiveFormats.h"
#include "UiLanguage.h"
#include "OpenProfile.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QInputDialog>
#include <QMessageBox>
#include <QUrl>

namespace {
constexpr int PathRole = Qt::UserRole, DirRole = Qt::UserRole + 1, ParentRole = Qt::UserRole + 2;
QString entryFolder(const QString &entry) {
    const int slash = entry.lastIndexOf('/');
    return slash < 0 ? QString() : entry.left(slash + 1);
}
}

QString MainWindow::archiveLocation() const { return archiveVirtualPath + '/' + archivePrefix; }
QString MainWindow::archiveWorkingFolder() const {
    // Each temporary directory must be independent. Keeping a child temp
    // alive cannot protect it from a parent temp's recursive removal.
    const auto outer = parentArchives.isEmpty() ? archivePath : parentArchives.first().path;
    return settings.workingFolder(QFileInfo(outer).absolutePath());
}

void MainWindow::openPath(QString path, QString readMode, bool reopen) {
    if (dataOperationBusy()) return;
    if (path != pendingDefaultOpen) pendingDefaultOpen.clear();
    pathBox->setText(archivePath.isEmpty() ? fsPath : archiveLocation());
    if (!reopen && !archivePath.isEmpty() && (path == archiveVirtualPath || path.startsWith(archiveVirtualPath + '/'))) {
        navigateArchive(path == archiveVirtualPath ? QString() : path.mid(archiveVirtualPath.size() + 1)); return;
    }
    if (!parentArchives.isEmpty() && path != archivePath) {
        QString keep;
        for (auto it = parentArchives.crbegin(); it != parentArchives.crend(); ++it) {
            if (path == it->virtualPath || path.startsWith(it->virtualPath + '/')) { keep = it->virtualPath; break; }
        }
        leaveNestedArchives(keep, [this, path, readMode, reopen] { openPath(path, readMode, reopen); }); return;
    }
    if (QFileInfo(path).isRelative()) path = QDir(fsPath).absoluteFilePath(path);
    if (QFileInfo(path).isDir()) { showFilesystem(path); return; }
    // Addresses and Favorites contain virtual nested paths. Find the real
    // containing archive, then resolve each internal folder/archive in order.
    QString physical = path;
    while (!QFileInfo::exists(physical)) {
        const auto parent = QFileInfo(physical).absolutePath();
        if (parent == physical) break;
        physical = parent;
    }
    if (!QFileInfo(physical).isFile()) { listFocusPending = false; QMessageBox::warning(this, "7-Zip", "Cannot open: " + path); return; }
    cancelFilesystemRead();
    pendingArchiveTail = physical == path ? QString() : path.mid(physical.size() + 1);
    pendingArchive = QFileInfo(physical).absoluteFilePath();
    pendingReopen = reopen;
    ArchiveRequest r; r.archive = pendingArchive; r.readMode = readMode;
    if (pendingArchive == archivePath) r.password = archivePassword;
    run(r);
}

void MainWindow::navigateArchive(QString relative) {
    while (relative.endsWith('/')) relative.chop(1);
    if (relative.isEmpty()) { archiveDirectory = 0; archivePrefix.clear(); showArchive(); return; }
    if (!SevenZipProcessBackend::safeArchivePath(relative)) { QMessageBox::warning(this, "7-Zip", "Invalid archive path: " + relative); return; }
    if (const auto id = archiveFolderIndex.findDirectory(relative); id >= 0) { bindArchiveDirectory(id); return; }
    for (const auto &entry : archiveEntries) {
        if ((entry.directory && entry.path == relative) || entry.path.startsWith(relative + '/')) {
            archivePrefix = relative + '/'; showArchive(); return;
        }
    }
    for (const auto &entry : archiveEntries) {
        if (!entry.directory && (entry.path == relative || relative.startsWith(entry.path + '/'))) {
            openNestedArchive(entry.path, false, relative.mid(entry.path.size() + (entry.path == relative ? 0 : 1))); return;
        }
    }
    listFocusPending = false; QMessageBox::warning(this, "7-Zip", "Cannot open archive folder: " + relative);
}

void MainWindow::bindArchiveDirectory(int id) {
    if (const auto dir = archiveFolderIndex.directory(id)) { archiveDirectory = id; archivePrefix = dir->path; showArchive(); }
}
int MainWindow::alternateArchiveDirectory() const {
    const auto dir = archiveFolderIndex.directory(archiveDirectory);
    if (!dir || dir->alternateStreams) return -1;
    const auto selected = selectedItems();
    int id = -1;
    if (selected.size() == 1) id = selected.first()->data(0, Qt::UserRole + 114).toInt();
    else if (selected.isEmpty()) id = dir->alternateDirectory;
    const auto alt = archiveFolderIndex.directory(id);
    return alt && (!(selected.isEmpty() && dir->id == 0) || !alt->children.isEmpty()) ? id : -1;
}
void MainWindow::openAlternateStreams() { if (!operationBusy()) bindArchiveDirectory(alternateArchiveDirectory()); }

void MainWindow::activateItem(QTreeWidgetItem *item, bool insideOnly, QString readMode) {
    if (operationBusy()) return;
    if (item->data(0, ParentRole).toBool()) { up(); return; }
    const auto path = item->data(0, PathRole).toString();
    if (!insideOnly && !item->data(0, DirRole).toBool() && externalNameBlocked(path)) return;
    if (!archivePath.isEmpty()) {
        if (item->data(0, DirRole).toBool()) { if (item->data(0, Qt::UserRole + 111).isValid()) bindArchiveDirectory(item->data(0, Qt::UserRole + 111).toInt()); else { archivePrefix = path; showArchive(); } }
        else if (insideOnly || !openProfileAlwaysExternal(path)) openNestedArchive(path, !insideOnly, {}, readMode, item);
        else openExternalItems({item}, false, {});
    } else if (item->data(0, DirRole).toBool() || insideOnly) openPath(path, readMode);
    else if (openProfileAlwaysExternal(QDir(fsPath).relativeFilePath(path))) launchExternal({path});
    else { pendingDefaultOpen = path; openPath(path, readMode); }
}

void MainWindow::openNestedArchive(QString entry, bool externalFallback, QString tail, QString readMode, QTreeWidgetItem *source) {
    if (operationBusy() || !SevenZipProcessBackend::safeArchivePath(entry)) return;
    auto pending = std::make_unique<NestedOpen>();
    pending->parent = {archivePath, archiveVirtualPath, entryFolder(entry), archivePassword, archiveType, entry, archiveEntries, archiveProperties, archiveTemporary, -1, {}, archiveReadMode, archiveLayers, archiveFolderIndex, 0, {}};
    pending->parent.directory = qMax(0, archiveFolderIndex.findDirectory(pending->parent.prefix));
    pending->temporary = std::make_shared<QTemporaryDir>(archiveWorkingFolder() + "/.7zip-nested-XXXXXX");
    if (!pending->temporary->isValid()) { QMessageBox::warning(this, "7-Zip", "Cannot create temporary archive directory."); return; }
    pending->entry = entry; pending->path = pending->temporary->filePath(entry);
    pending->virtualPath = archiveVirtualPath + '/' + entry; pending->tail = tail; pending->externalFallback = externalFallback; pending->readMode = readMode;
    ArchiveRequest r; r.operation = ArchiveOperation::Extract; r.archive = archivePath; r.password = archivePassword;
    r.outputDirectory = pending->temporary->path(); r.files = {entry}; r.overwriteMode = "overwrite";
    const auto focused = source ? source : files->currentItem();
    if (focused && focused->data(0, PathRole).toString() == entry) {
        setArchiveSelection(r, {focused}, true); pending->parent.prefix = archivePrefix; pending->parent.directory = archiveDirectory;
    }
    pending->parent.childSelection = r; pending->parent.childSelection.password.fill(QChar::Null); pending->parent.childSelection.password.clear();
    pending->parent.childSelection.readMode = archiveReadMode;
    nestedOpen = std::move(pending); run(r);
}

bool MainWindow::finishNestedOpen(const ArchiveResult &result) {
    if (!nestedOpen) return false;
    ArchiveResult completion = result;
    bool progressCompleted = false;
    if (!result.success) {
        bool passwordDeclined = false;
        if (result.passwordRequired) {
            bool ok; const auto password = QInputDialog::getText(this, "Password", result.message + "\nEnter password:", QLineEdit::Password, {}, &ok);
            if (ok) { auto retry = lastRequest; retry.password = password; run(retry); return true; }
            passwordDeclined = true;
        }
        const bool notArchive = openProfileNotArchive(result);
        if (nestedOpen->listing && nestedOpen->externalFallback && !result.cancelled && notArchive) {
            const auto temporary = nestedOpen->temporary; const auto entry = nestedOpen->entry; const auto password = nestedOpen->parent.password;
            const auto selection = nestedOpen->parent.childSelection;
            nestedOpen.reset(); launchArchiveExternal(temporary, {entry}, {}, password, selection);
        } else {
            nestedOpen.reset();
            if (!result.cancelled && !passwordDeclined) QMessageBox::warning(this, "7-Zip", result.message + "\nTarget: " + result.target + "\nExit code: " + QString::number(result.exitCode) + '\n' + result.details);
        }
    } else if (!nestedOpen->listing) {
        if (!QFileInfo(nestedOpen->path).isFile() || QFileInfo(nestedOpen->path).isSymLink()) {
            completion.success = false; completion.exitCode = -1; completion.message = "The extracted archive is not a regular file.";
            nestedOpen.reset(); QMessageBox::warning(this, "7-Zip", completion.message);
        } else {
            nestedOpen->parent.password = lastRequest.password;
            ::lstat(QFile::encodeName(nestedOpen->path).constData(), &nestedOpen->parent.childStamp);
            nestedOpen->parent.childSize = nestedOpen->parent.childStamp.st_size;
            nestedOpen->listing = true; pendingArchive = nestedOpen->path;
            ArchiveRequest r; r.archive = pendingArchive; r.readMode = nestedOpen->readMode; run(r); return true;
        }
    } else {
        parentArchives.append(nestedOpen->parent); archiveTemporary = nestedOpen->temporary;
        archivePath = nestedOpen->path; archiveVirtualPath = nestedOpen->virtualPath; archivePrefix.clear(); archivePassword = lastRequest.password;
        setArchiveListing(result); archiveReadMode = lastRequest.readMode;
        const auto tail = nestedOpen->tail; nestedOpen.reset();
        // Resolving a bookmark tail can start another Extract immediately.
        // Complete this dialog before that new operation replaces it.
        if (progressDialog) progressDialog->finish(completion);
        progressCompleted = true; showArchive();
        if (!tail.isEmpty()) navigateArchive(tail);
    }
    if (!progressCompleted && progressDialog) progressDialog->finish(completion);
    if (!backend.busy()) { lastRequest.password.fill(QChar::Null); lastRequest.password.clear(); }
    updateState(); return true;
}

void MainWindow::selectArchiveItem(const QString &entry, qint64 archiveIndex) {
    for (int n = 0; n < files->topLevelItemCount(); ++n) {
        auto item = files->topLevelItem(n);
        if (!item->data(0, ParentRole).toBool() && (archiveIndex >= 0 ? item->data(0, Qt::UserRole + 110).toLongLong() == archiveIndex : item->data(0, PathRole).toString() == entry)) { files->setCurrentItem(item); item->setSelected(true); files->scrollToItem(item); break; }
    }
}

void MainWindow::up() {
    if (dataOperationBusy()) return;
    if (filesystemRead) { QDir directory(requestedDirectory()); directory.cdUp(); showFilesystem(directory.absolutePath()); return; }
    if (!archivePath.isEmpty()) {
        if (const auto dir = archiveFolderIndex.directory(archiveDirectory); dir && dir->parent >= 0) { bindArchiveDirectory(dir->parent); return; }
        if (!archivePrefix.isEmpty()) { QString p = archivePrefix; p.chop(1); archivePrefix = entryFolder(p); showArchive(); return; }
        if (!parentArchives.isEmpty()) {
            leaveNestedArchives(parentArchives.last().virtualPath, [] {}); return;
        }
        const auto archive = archivePath; showFilesystem(QFileInfo(archive).absolutePath(), [this, archive] { selectArchiveItem(archive); });
    } else { QDir d(requestedDirectory()); d.cdUp(); showFilesystem(d.absolutePath()); }
}

namespace {
bool writableArchive(const QString &path, const QString &type, const QMap<QString, QString> &properties) {
    const QFileInfo file(path);
    return QStringList{"7z", "zip", "tar", "wim", "gzip", "bzip2", "xz"}.contains(type.toLower()) && file.isFile() && !file.isSymLink() && file.isWritable() &&
        properties.value("Read-only") != "+" && properties.value("Open Layers").toInt() <= 1 && properties.value("Multivolume") != "+" && properties.value("Volumes").toInt() <= 1 &&
        !properties.value("Tail Size").toULongLong() && properties.value("Offset").toLongLong() >= 0 && properties.value("Errors").isEmpty() && properties.value("Error Flags").isEmpty();
}
}

bool MainWindow::canUpdateArchive() const {
    if (!writableArchive(archivePath, archiveType, archiveProperties)) return false;
    for (const auto &frame : parentArchives) if (!writableArchive(frame.path, frame.type, frame.properties)) return false;
    return true;
}

void MainWindow::copyIntoArchive(QStringList sources, bool move) {
    if (operationBusy() || archivePath.isEmpty() || !canUpdateArchive() || sources.isEmpty()) return;
    ArchiveRequest request; request.operation = ArchiveOperation::Add;
    request.archive = archivePath; request.format = archiveType.toLower(); request.password = archivePassword;
    request.workingDirectory = fsPath; request.archivePrefix = archivePrefix;
    request.useArchiveDefaults = true; request.deleteAfter = move;
    // Agent's EnumerateItems2 drops each selected item's physical parent,
    // while preserving descendants of selected folders. Absolute console
    // arguments reproduce this for Flat selections and Finder multi-root drops.
    for (const auto &path : sources) request.files << QFileInfo(path).absoluteFilePath();
    run(request);
}

void MainWindow::leaveNestedArchives(QString keepVirtual, std::function<void()> continuation) {
    if (afterNestedExit) return;
    exitKeepVirtual = keepVirtual; afterNestedExit = std::move(continuation); continueNestedExit();
}

void MainWindow::restoreParentArchive() {
    const auto parent = parentArchives.takeLast();
    archivePath = parent.path; archiveVirtualPath = parent.virtualPath; archivePrefix = parent.prefix; archivePassword = parent.password;
    archiveType = parent.type; archiveEntries = parent.entries; archiveProperties = parent.properties; archiveTemporary = parent.temporary; archiveReadMode = parent.readMode; archiveLayers = parent.layers; archiveFolderIndex = parent.folderIndex;
    archiveDirectory = parent.directory;
    showArchive(); selectArchiveItem(parent.child, parent.childSelection.selection.size() == 1 ? parent.childSelection.selection.first().archiveIndex : -1);
}

void MainWindow::continueNestedExit() {
    while (!parentArchives.isEmpty() && archiveVirtualPath != exitKeepVirtual) {
        const auto &parent = parentArchives.last(); struct stat child{};
        // Upstream Panel.h compares size / mtime when closing each level.
        if (::lstat(QFile::encodeName(archivePath).constData(), &child) == 0 && (child.st_size != parent.childSize || child.st_mtimespec.tv_sec != parent.childStamp.st_mtimespec.tv_sec || child.st_mtimespec.tv_nsec != parent.childStamp.st_mtimespec.tv_nsec)) {
            const auto question = UiLanguage::text("File '{0}' was modified.\nDo you want to update it in the archive?").replace("{0}", QFileInfo(parent.child).fileName());
            const auto answer = QMessageBox::question(this, "7-Zip", question, QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
            // 26.03 OpenParentArchiveFolder treats both No and Cancel as discard.
            if (answer == QMessageBox::Yes) {
                nestedUpdate = std::make_unique<NestedUpdate>(NestedUpdate{archivePath, archiveTemporary});
                ArchiveRequest request = parent.childSelection; request.operation = ArchiveOperation::ReplaceFile; request.archive = parent.path;
                request.format = parent.type.toLower(); request.files = {parent.child}; request.replacementSource = archivePath; request.password = parent.password;
                run(request); return;
            }
        }
        restoreParentArchive();
    }
    auto continuation = std::move(afterNestedExit); afterNestedExit = {}; exitKeepVirtual.clear();
    updateState();
    if (continuation) continuation();
}

bool MainWindow::finishNestedUpdate(const ArchiveResult &incoming) {
    if (!nestedUpdate) return false;
    auto result = incoming;
    if (result.operation != ArchiveOperation::ReplaceFile || parentArchives.isEmpty() || result.target != parentArchives.last().path) {
        result.success = false; result.passwordRequired = false; result.exitCode = -1;
        result.message = "Unexpected archive operation during parent write-back; the modified file has been retained.";
    }
    if (result.passwordRequired) {
        bool ok; const auto password = QInputDialog::getText(this, "Password", "Enter password:", QLineEdit::Password, {}, &ok);
        if (ok) { auto retry = lastRequest; retry.password = password; run(retry); return true; }
    }
    if (progressDialog) progressDialog->finish(result);
    if (result.success) {
        auto &parent = parentArchives.last(); parent.password = lastRequest.password;
        parent.entries = result.entries; parent.properties = result.properties; parent.type = result.archiveType;
        parent.layers = result.layers; parent.folderIndex = ArchiveFolderIndex(result.entries, result.metadata);
        parent.directory = qMax(0, parent.folderIndex.findDirectory(parent.prefix));
    } else {
        // Retain a recoverable edited copy even after panel/app destruction.
        if (nestedUpdate->temporary) nestedUpdate->temporary->setAutoRemove(false);
        QMessageBox::warning(this, "7-Zip", "Cannot update file\n'" + nestedUpdate->path + "'\nThe modified file has been retained.\n" + result.message + "\nTarget: " + result.target + "\nExit code: " + QString::number(result.exitCode) + '\n' + result.details);
    }
    nestedUpdate.reset();
    if (!parentArchives.isEmpty()) restoreParentArchive();
    lastRequest.password.fill(QChar::Null); lastRequest.password.clear();
    continueNestedExit(); updateState(); return true;
}
