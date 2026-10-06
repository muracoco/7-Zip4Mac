// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "OperationControl.h"
#include "Overwrite.h"
#include <QByteArray>
#include <memory>
#include <functional>
#include <optional>
#include <sys/stat.h>

struct FileDirectory {
    int descriptor = -1;
    QString path;
    struct stat stamp{};
    ~FileDirectory();
};
struct FileSnapshot {
    QString path, error;
    QByteArray name, linkTarget;
    std::shared_ptr<FileDirectory> parent;
    struct stat stamp{};
    bool exists = false;
    int errorCode = 0;
    OverwriteFileInfo info(QString displayPath = {}) const;
};
enum class FileInstallPhase { Prepared, Validated, Installed, Rollback };
using FileInstallObserver = std::function<void(FileInstallPhase, const QString &)>;
struct FileInstallResult {
    QString target, error, recovery;
    FileSnapshot installedFile, preservedFile;
    bool installed = false, skipped = false, cancelled = false, sourceMoved = false;
};

namespace FileInstall {
// Parents are walked through O_NOFOLLOW directory handles. Explicit source
// locations may first canonicalize their parent (e.g. macOS /var -> /private/var).
FileSnapshot capture(QString path, bool canonicalizeParent = false);
// Read-only metadata lookup. A missing ancestor means an absent output;
// symlink/permission/I/O failures remain errors. Not an installation token.
FileSnapshot inspect(QString path);
int validateRemoval(const FileSnapshot &file, bool directory);
FileSnapshot captureAt(const std::shared_ptr<FileDirectory> &parent, QByteArray name);
std::shared_ptr<FileDirectory> openFolder(const FileSnapshot &folder, QString *error);
bool unchanged(const FileSnapshot &snapshot, bool renamed = false);
QString ensureDirectory(QString path);
QString autoName(const FileSnapshot &destination);
QString useRequestedName(FileInstallResult &result);
FileInstallResult install(const FileSnapshot &source, const FileSnapshot &destination,
    const std::shared_ptr<OperationControl> &control, bool renameExisting = false,
    const FileInstallObserver &observer = {}, bool hardLink = false);
FileInstallResult transferFile(QString source, QString destination, QString &mode,
    OverwriteBroker &broker, const std::shared_ptr<OperationControl> &control,
    OverwriteFileInfo incoming = {}, bool allowDestinationLinks = false,
    const FileInstallObserver &observer = {});
FileInstallResult transfer(const FileSnapshot &source, FileSnapshot destination, QString &mode,
    OverwriteBroker &broker, const std::shared_ptr<OperationControl> &control,
    OverwriteFileInfo incoming = {}, bool allowDestinationLinks = false,
    const FileInstallObserver &observer = {}, bool move = false, bool hardLink = false);
// Same-volume identity-preserving rename. Empty result requests copy fallback.
std::optional<FileInstallResult> tryMove(const FileSnapshot &source,
    const FileSnapshot &destination, const std::shared_ptr<OperationControl> &control,
    bool renameExisting = false, const FileInstallObserver &observer = {});
FileInstallResult rename(const FileSnapshot &source, const FileSnapshot &destination,
    const std::shared_ptr<OperationControl> &control);
// Move cleanup first captures the leaf in an owned directory, validates what
// was actually moved, and deletes only that identity. It never follows links.
QString removeMovedSource(const FileSnapshot &source);
}
