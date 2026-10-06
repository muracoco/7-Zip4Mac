// SPDX-License-Identifier: LGPL-3.0-or-later
#include "FileInstall.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QScopeGuard>
#include <QUuid>
#include <copyfile.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <limits.h>
#include <sys/attr.h>

namespace {
QString ioError(QString action, QString path) { const int code = errno; return action + ": " + path + '\n' + QString::fromLocal8Bit(std::strerror(code)) + " (" + QString::number(code) + ')'; }
bool identity(const struct stat &a, const struct stat &b) { return a.st_dev == b.st_dev && a.st_ino == b.st_ino; }
bool sameAttributes(const struct stat &a, const struct stat &b) {
    return identity(a, b) && a.st_mode == b.st_mode && a.st_size == b.st_size && a.st_nlink == b.st_nlink && a.st_uid == b.st_uid && a.st_gid == b.st_gid && a.st_flags == b.st_flags;
}
bool same(const struct stat &a, const struct stat &b, bool renamed = false) {
    return sameAttributes(a, b) && a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec == b.st_mtimespec.tv_nsec &&
        (renamed || (a.st_ctimespec.tv_sec == b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec == b.st_ctimespec.tv_nsec));
}
int openDirectory(QString path, bool create = false) {
    if (!path.startsWith('/') || path.contains(QChar::Null)) { errno = EINVAL; return -1; }
    int current = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (current < 0) return -1;
    for (const auto &part : path.split('/', Qt::SkipEmptyParts)) {
        if (part == "." || part == "..") { ::close(current); errno = EINVAL; return -1; }
        const auto name = QFile::encodeName(part);
        int next = ::openat(current, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (next < 0 && errno == ENOENT && create) {
            if (::mkdirat(current, name.constData(), 0777) == 0 || errno == EEXIST) next = ::openat(current, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        const int code = errno; ::close(current);
        if (next < 0) { errno = code; return -1; } current = next;
    }
    return current;
}
bool parentUnchanged(const std::shared_ptr<FileDirectory> &parent) {
    if (!parent) return false;
    const int current = openDirectory(parent->path); if (current < 0) return false;
    struct stat now{}; const bool ok = ::fstat(current, &now) == 0 && identity(parent->stamp, now); ::close(current); return ok;
}
bool readLink(int parent, const QByteArray &name, QByteArray &target) {
    target.resize(PATH_MAX); const auto length = ::readlinkat(parent, name.constData(), target.data(), target.size());
    if (length < 0) return false; if (length == target.size()) { errno = ENAMETOOLONG; return false; } target.resize(length); return true;
}
bool matches(int parent, const QByteArray &name, const struct stat &stamp, const QByteArray &target, bool renamed = false) {
    struct stat now{}; if (::fstatat(parent, name.constData(), &now, AT_SYMLINK_NOFOLLOW) != 0 || !same(stamp, now, renamed)) return false;
    if (!S_ISLNK(stamp.st_mode)) return true;
    QByteArray bytes; return readLink(parent, name, bytes) && bytes == target;
}
QString actualPath(int descriptor, QString fallback) { char path[PATH_MAX]{}; return ::fcntl(descriptor, F_GETPATH, path) == 0 ? QFile::decodeName(path) : fallback; }
bool directoryEmpty(int parent, const QByteArray &name) {
    const int fd = ::openat(parent, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC); if (fd < 0) return false;
    DIR *dir = ::fdopendir(fd); if (!dir) { ::close(fd); return false; } const auto close = qScopeGuard([&] { ::closedir(dir); });
    for (;;) { errno = 0; const auto item = ::readdir(dir); if (!item) return errno == 0; if (std::strcmp(item->d_name, ".") && std::strcmp(item->d_name, "..")) { errno = ENOTEMPTY; return false; } }
}
QString nameInDirectory(const FileSnapshot &destination) {
    // Common/FilePathAutoRename.cpp: split the final extension, except dotfiles,
    // and preserve the upstream binary search across occupied suffixes.
    QString name = QFile::decodeName(destination.name), extension;
    const int dot = name.lastIndexOf('.'); if (dot > 0) { extension = name.mid(dot); name.truncate(dot); } name += '_';
    auto occupied = [&](quint32 number) { struct stat stamp{}; const auto bytes = QFile::encodeName(name + QString::number(number) + extension); return ::fstatat(destination.parent->descriptor, bytes.constData(), &stamp, AT_SYMLINK_NOFOLLOW) == 0 || errno != ENOENT; };
    quint32 left = 1, right = quint32(1) << 30;
    while (left != right) { const auto middle = (left + right) / 2; if (occupied(middle)) left = middle + 1; else right = middle; }
    return occupied(right) ? QString() : name + QString::number(right) + extension;
}
// Only known identities are cleaned through retained handles, never by
// recursive deletion of a staging pathname that another process can replace.
class Stage {
public:
    std::shared_ptr<FileDirectory> parent;
    QByteArray name;
    int descriptor = -1;
    struct stat stamp{}, disposable{};
    bool ownedEntry = false, preserve = false;
    explicit Stage(std::shared_ptr<FileDirectory> folder) : parent(std::move(folder)) {
        name = QFile::encodeName(".7zip-file-install-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        if (::mkdirat(parent->descriptor, name.constData(), 0700) != 0) return;
        struct stat created{}; if (::fstatat(parent->descriptor, name.constData(), &created, AT_SYMLINK_NOFOLLOW) != 0) return;
        descriptor = ::openat(parent->descriptor, name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (descriptor < 0) return;
        if (::fstat(descriptor, &stamp) != 0 || !identity(created, stamp)) { ::close(descriptor); descriptor = -1; }
    }
    QString path() const { return actualPath(descriptor, parent->path + '/' + QFile::decodeName(name)); }
    ~Stage() {
        if (descriptor < 0) return;
        if (!preserve) {
            struct stat entry{}, current{};
            if (ownedEntry && ::fstatat(descriptor, "replacement", &entry, AT_SYMLINK_NOFOLLOW) == 0 && identity(entry, disposable)) ::unlinkat(descriptor, "replacement", S_ISDIR(entry.st_mode) ? AT_REMOVEDIR : 0);
            if (::fstatat(parent->descriptor, name.constData(), &current, AT_SYMLINK_NOFOLLOW) == 0 && identity(stamp, current)) ::unlinkat(parent->descriptor, name.constData(), AT_REMOVEDIR);
        }
        ::close(descriptor);
    }
};
bool unsupportedRename() { return errno == ENOTSUP || errno == EINVAL; }
QString changedProperties(const FileSnapshot &file) {
    const auto now = FileInstall::captureAt(file.parent, file.name); QStringList properties;
    if (!now.exists || !now.error.isEmpty()) return "missing or inaccessible";
    if (!identity(file.stamp, now.stamp)) properties << "identity";
    if (file.stamp.st_size != now.stamp.st_size) properties << "size";
    if (file.stamp.st_mode != now.stamp.st_mode) properties << "permissions/type";
    if (file.stamp.st_nlink != now.stamp.st_nlink) properties << "link count";
    if (file.stamp.st_uid != now.stamp.st_uid || file.stamp.st_gid != now.stamp.st_gid) properties << "owner";
    if (file.stamp.st_flags != now.stamp.st_flags) properties << "flags";
    if (file.stamp.st_mtimespec.tv_sec != now.stamp.st_mtimespec.tv_sec || file.stamp.st_mtimespec.tv_nsec != now.stamp.st_mtimespec.tv_nsec) properties << "modification time";
    if (file.stamp.st_ctimespec.tv_sec != now.stamp.st_ctimespec.tv_sec || file.stamp.st_ctimespec.tv_nsec != now.stamp.st_ctimespec.tv_nsec) properties << "change time";
    return properties.isEmpty() ? QString("parent folder") : properties.join(", ");
}
// Ordinary rename is safe only into this freshly created, private staging
// directory. Public output names always use an exclusive creation primitive.
int renameIntoStage(int source, const QByteArray &name, Stage &stage, const char *target) {
    int result = ::renameatx_np(source, name.constData(), stage.descriptor, target, RENAME_EXCL);
    if (result == 0 || !unsupportedRename()) return result;
    struct stat occupied{};
    if (::fstatat(stage.descriptor, target, &occupied, AT_SYMLINK_NOFOLLOW) == 0) { errno = EEXIST; return -1; }
    if (errno != ENOENT) return -1;
    return ::renameat(source, name.constData(), stage.descriptor, target);
}
std::shared_ptr<FileDirectory> stageFolder(Stage &stage) {
    auto folder = std::make_shared<FileDirectory>(); folder->descriptor = ::dup(stage.descriptor); folder->path = stage.path();
    if (folder->descriptor >= 0) ::fstat(folder->descriptor, &folder->stamp); return folder;
}
struct ExclusiveCopy { QString error; FileSnapshot output; int errorCode = 0; bool created = false; };
ExclusiveCopy copyExclusive(const FileSnapshot &source, const FileSnapshot &destination, const std::shared_ptr<OperationControl> &control) {
    ExclusiveCopy result;
    if (!S_ISREG(source.stamp.st_mode)) { result.error = "This filesystem cannot install the requested link type: " + destination.path; return result; }
    const int input = ::openat(source.parent->descriptor, source.name.constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (input < 0) { result.error = ioError("Cannot read prepared data", source.path); return result; } const auto closeInput = qScopeGuard([&] { ::close(input); });
    int output = ::openat(destination.parent->descriptor, destination.name.constData(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (output < 0) { result.errorCode = errno; result.error = ioError("Cannot exclusively create output", destination.path); return result; }
    result.created = true; const auto closeOutput = qScopeGuard([&] { if (output >= 0) ::close(output); });
    result.output = destination; result.output.exists = true; result.output.error.clear(); result.output.linkTarget.clear();
    // Keep the identity of our open descriptor, never a recaptured public leaf.
    ::fstat(output, &result.output.stamp);
    auto fail = [&](QString error) {
        result.error = error;
        struct stat current{};
        if (output >= 0 && ::fstat(output, &current) == 0 && identity(result.output.stamp, current)) result.output.stamp = current;
        return result;
    };
    struct stat opened{}; if (::fstat(input, &opened) != 0 || !same(source.stamp, opened)) return fail("Prepared data changed: " + source.path);
    QByteArray buffer(1024 * 1024, Qt::Uninitialized);
    QCryptographicHash copiedData(QCryptographicHash::Sha256);
    for (;;) {
        if (control->checkpoint()) return fail("Operation cancelled.");
        ssize_t length; do { length = ::read(input, buffer.data(), size_t(buffer.size())); } while (length < 0 && errno == EINTR);
        if (length < 0) return fail(ioError("Cannot read prepared data", source.path)); if (!length) break;
        copiedData.addData(QByteArrayView(buffer.constData(), length));
        ssize_t offset = 0; while (offset < length) { ssize_t written; do { written = ::write(output, buffer.data() + offset, size_t(length - offset)); } while (written < 0 && errno == EINTR); if (written <= 0) return fail(ioError("Cannot write output", destination.path)); offset += written; }
    }
    const timespec times[2]{source.stamp.st_atimespec, source.stamp.st_mtimespec};
    if (::fcopyfile(input, output, nullptr, COPYFILE_ACL | COPYFILE_XATTR) != 0 || ::fchmod(output, source.stamp.st_mode & 07777) != 0 || ::futimens(output, times) != 0 || ::fsync(output) != 0)
        return fail(ioError("Cannot preserve output metadata", destination.path));
    struct stat actual{}; if (::fstat(output, &actual) != 0) return fail(ioError("Cannot inspect completed output", destination.path));
    if (!FileInstall::unchanged(source)) return fail("Prepared data changed during installation (" + changedProperties(source) + "): " + source.path);
    auto checkedOutput = destination; checkedOutput.exists = true; checkedOutput.stamp = actual;
    if (!matches(destination.parent->descriptor, destination.name, actual, {})) return fail("Output changed during installation (" + changedProperties(checkedOutput) + "): " + destination.path);
    ::fstat(output, &result.output.stamp);
    const int closed = ::close(output); output = -1; if (closed != 0) return fail(ioError("Cannot close completed output", destination.path));
    // Refresh attributes through a read handle after the writer has closed.
    // SMB may normalize timestamps at close; retain the known writer identity
    // and compare the actual bytes before accepting the resulting snapshot.
    output = ::openat(destination.parent->descriptor, destination.name.constData(), O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (output < 0) return fail(ioError("Cannot verify completed output", destination.path));
    struct stat committed{}, verified{};
    if (::fstat(output, &committed) != 0 || !sameAttributes(result.output.stamp, committed)) return fail("Completed output changed after closing.");
    QCryptographicHash verifiedData(QCryptographicHash::Sha256);
    for (;;) {
        if (control->checkpoint()) return fail("Operation cancelled.");
        ssize_t length; do { length = ::read(output, buffer.data(), size_t(buffer.size())); } while (length < 0 && errno == EINTR);
        if (length < 0) return fail(ioError("Cannot verify completed data", destination.path)); if (!length) break;
        verifiedData.addData(QByteArrayView(buffer.constData(), length));
    }
    if (verifiedData.result() != copiedData.result() || ::fstat(output, &verified) != 0 || !same(committed, verified) || !matches(destination.parent->descriptor, destination.name, verified, {})) return fail("Completed output changed during verification.");
    result.output.stamp = verified; return result;
}
FileInstallResult installExclusive(Stage &stage, const FileSnapshot &source, const FileSnapshot &destination,
    const std::shared_ptr<OperationControl> &control, bool renameExisting, const FileInstallObserver &observer) {
    FileInstallResult result; result.target = destination.path;
    auto fail = [&](QString error) {
        result.error = std::move(error); result.cancelled = control->isCancelled();
        if (stage.preserve) { result.recovery = stage.path(); result.error += "\nRecovery files retained in: " + result.recovery; }
        return result;
    };
    if (!FileInstall::unchanged(source) || !FileInstall::unchanged(destination)) return fail("Source or destination changed; previous data retained.");
    auto folder = stageFolder(stage); const auto prepared = FileInstall::captureAt(folder, "replacement");
    if (!prepared.error.isEmpty() || !prepared.exists || !S_ISREG(prepared.stamp.st_mode)) return fail("This filesystem does not support the requested atomic/link operation.");
    if (destination.exists) {
        if (renameIntoStage(destination.parent->descriptor, destination.name, stage, "previous") != 0) return fail(ioError("Cannot preserve previous output", destination.path));
        stage.preserve = true;
        if (!matches(stage.descriptor, "previous", destination.stamp, destination.linkTarget, true)) return fail("Destination changed while being preserved.");
    }
    auto absent = FileInstall::captureAt(destination.parent, destination.name);
    const auto installed = copyExclusive(prepared, absent, control);
    if (!installed.error.isEmpty()) {
        if (installed.created && renameIntoStage(destination.parent->descriptor, destination.name, stage, "incomplete") == 0) {
            struct stat actual{};
            if (::fstatat(stage.descriptor, "incomplete", &actual, AT_SYMLINK_NOFOLLOW) == 0 && identity(installed.output.stamp, actual)) ::unlinkat(stage.descriptor, "incomplete", 0);
            else stage.preserve = true;
        }
        return fail(installed.error);
    }
    if (observer) observer(FileInstallPhase::Installed, stage.path());
    if (!FileInstall::unchanged(installed.output)) { stage.preserve |= destination.exists; return fail("Installed output changed (" + changedProperties(installed.output) + "); sources retained: " + installed.output.path); }
    if (!FileInstall::unchanged(source)) { stage.preserve |= destination.exists; return fail("Source changed (" + changedProperties(source) + "); sources retained: " + source.path); }
    if (destination.exists) {
        if (renameExisting) {
            bool saved = false;
            const auto previous = FileInstall::captureAt(folder, "previous");
            for (unsigned attempt = 0; attempt < 64; ++attempt) {
                const auto name = FileInstall::autoName(destination); if (name.isEmpty()) break;
                const auto backup = FileInstall::capture(name); if (backup.exists) continue;
                const auto copied = copyExclusive(previous, backup, control);
                if (copied.error.isEmpty()) { result.preservedFile = copied.output; saved = true; break; }
                if (copied.created || copied.errorCode != EEXIST) return fail(copied.error);
            }
            if (!saved) return fail("Cannot preserve the previous output under an automatic name.");
        }
        if (!FileInstall::unchanged(installed.output)) return fail("New output changed (" + changedProperties(installed.output) + "); previous data retained.");
        if (!matches(stage.descriptor, "previous", destination.stamp, destination.linkTarget, true)) {
            auto previous = FileInstall::captureAt(folder, "previous"); previous.stamp = destination.stamp;
            return fail("Previous output changed (" + changedProperties(previous) + "); previous data retained.");
        }
        if (::unlinkat(stage.descriptor, "previous", 0) != 0) return fail(ioError("Cannot finish previous output cleanup", destination.path));
    }
    stage.preserve = false; result.installedFile = installed.output; result.installed = true; return result;
}
}
FileDirectory::~FileDirectory() { if (descriptor >= 0) ::close(descriptor); }
OverwriteFileInfo FileSnapshot::info(QString displayPath) const {
    OverwriteFileInfo result; result.path = displayPath.isEmpty() ? path : displayPath; result.directory = exists && S_ISDIR(stamp.st_mode); result.sizeDefined = exists && !result.directory;
    result.size = exists ? quint64(qMax<off_t>(0, stamp.st_size)) : 0;
    if (exists) result.modified = QDateTime::fromMSecsSinceEpoch(qint64(stamp.st_mtimespec.tv_sec) * 1000 + stamp.st_mtimespec.tv_nsec / 1000000); return result;
}
FileSnapshot FileInstall::capture(QString path, bool canonicalizeParent) {
    FileSnapshot result; const QFileInfo file(path); QString folder = file.absolutePath();
    if (path.isEmpty() || path.contains(QChar::Null) || file.fileName().isEmpty() || file.fileName() == "." || file.fileName() == "..") { result.errorCode = EINVAL; result.error = "Invalid file path."; return result; }
    if (canonicalizeParent) folder = QFileInfo(folder).canonicalFilePath();
    result.path = folder + '/' + file.fileName(); result.name = QFile::encodeName(file.fileName()); result.parent = std::make_shared<FileDirectory>(); result.parent->path = folder;
    result.parent->descriptor = openDirectory(folder);
    if (result.parent->descriptor < 0 || ::fstat(result.parent->descriptor, &result.parent->stamp) != 0) { result.errorCode = errno; result.error = ioError("Cannot open file folder", folder); return result; }
    return captureAt(result.parent, result.name);
}
FileSnapshot FileInstall::inspect(QString path) {
    const auto parts = path.split('/');
    if (!path.startsWith('/') || path.contains(QChar::Null) || parts.contains(".") || parts.contains("..")) {
        FileSnapshot invalid; invalid.path = path; invalid.errorCode = EINVAL; invalid.error = "Invalid metadata lookup path."; return invalid;
    }
    auto result = capture(std::move(path));
    if (result.errorCode == ENOENT) { result.error.clear(); result.errorCode = 0; }
    return result;
}
int FileInstall::validateRemoval(const FileSnapshot &file, bool directory) {
    if (!file.error.isEmpty()) return file.errorCode ? file.errorCode : EIO;
    if (!file.exists) return ENOENT;
    if (!unchanged(file)) return ESTALE;
    if (directory && !S_ISDIR(file.stamp.st_mode)) return ENOTDIR;
    if (!directory && S_ISDIR(file.stamp.st_mode)) return EISDIR;
    if (directory && !directoryEmpty(file.parent->descriptor, file.name)) return errno ? errno : ENOTEMPTY;
    return unchanged(file) ? 0 : ESTALE;
}
FileSnapshot FileInstall::captureAt(const std::shared_ptr<FileDirectory> &parent, QByteArray name) {
    FileSnapshot result; result.parent = parent; result.name = name;
    if (!parent || parent->descriptor < 0 || name.isEmpty() || name.contains('/') || name.contains('\0') || name == "." || name == "..") { result.errorCode = EINVAL; result.error = "Invalid file component."; return result; }
    result.path = parent->path + '/' + QFile::decodeName(name);
    if (::fstatat(result.parent->descriptor, result.name.constData(), &result.stamp, AT_SYMLINK_NOFOLLOW) == 0) {
        result.exists = true; if (S_ISLNK(result.stamp.st_mode) && !readLink(result.parent->descriptor, result.name, result.linkTarget)) { result.errorCode = errno; result.error = ioError("Cannot read symbolic link", result.path); }
    } else if (errno != ENOENT) { result.errorCode = errno; result.error = ioError("Cannot inspect file", result.path); }
    return result;
}
bool FileInstall::unchanged(const FileSnapshot &snapshot, bool renamed) {
    if (!snapshot.error.isEmpty() || !parentUnchanged(snapshot.parent)) return false;
    if (snapshot.exists) return matches(snapshot.parent->descriptor, snapshot.name, snapshot.stamp, snapshot.linkTarget, renamed);
    struct stat stamp{}; return ::fstatat(snapshot.parent->descriptor, snapshot.name.constData(), &stamp, AT_SYMLINK_NOFOLLOW) != 0 && errno == ENOENT;
}
std::shared_ptr<FileDirectory> FileInstall::openFolder(const FileSnapshot &folder, QString *error) {
    auto result = std::make_shared<FileDirectory>(); result->path = folder.path;
    if (!folder.exists || !S_ISDIR(folder.stamp.st_mode) || !unchanged(folder)) { *error = "Source folder changed or is not a directory: " + folder.path; return {}; }
    result->descriptor = ::openat(folder.parent->descriptor, folder.name.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (result->descriptor < 0 || ::fstat(result->descriptor, &result->stamp) != 0) { *error = ioError("Cannot open folder", folder.path); return {}; }
    if (!identity(result->stamp, folder.stamp)) { *error = "Folder changed while opening: " + folder.path; return {}; }
    return result;
}
QString FileInstall::ensureDirectory(QString path) { const int fd = openDirectory(path, true); if (fd < 0) return ioError("Cannot create output folder (links refused)", path); ::close(fd); return {}; }
QString FileInstall::autoName(const FileSnapshot &destination) { if (!destination.parent || !parentUnchanged(destination.parent)) return {}; const auto name = nameInDirectory(destination); return name.isEmpty() ? QString() : destination.parent->path + '/' + name; }
QString FileInstall::useRequestedName(FileInstallResult &result) {
    if (!result.installed) return {};
    const auto file = result.installedFile;
    if (!unchanged(file)) return "Output changed before restoring its name: " + result.target;
    if (::fpathconf(file.parent->descriptor, _PC_CASE_SENSITIVE) != 0) return {};
    struct attrlist attributes{}; attributes.bitmapcount = ATTR_BIT_MAP_COUNT; attributes.commonattr = ATTR_CMN_NAME;
    struct { uint32_t length; attrreference_t name; char bytes[PATH_MAX]; } buffer{};
    if (::getattrlistat(file.parent->descriptor, file.name.constData(), &attributes, &buffer, sizeof(buffer), FSOPT_NOFOLLOW) != 0) return ioError("Cannot inspect installed output name", result.target);
    const qint64 start = offsetof(decltype(buffer), name) + qint64(buffer.name.attr_dataoffset), end = start + buffer.name.attr_length;
    if (start < qint64(sizeof(uint32_t) + sizeof(attrreference_t)) || end > qint64(qMin<size_t>(buffer.length, sizeof(buffer))) || buffer.name.attr_length < 1) return "Invalid installed output name: " + result.target;
    const auto name = reinterpret_cast<const char *>(&buffer) + start;
    if (name[buffer.name.attr_length - 1] != 0) return "Invalid installed output name: " + result.target;
    const QByteArray actual(name, buffer.name.attr_length - 1);
    if (actual == file.name) return {};
    if (QFile::decodeName(actual).toCaseFolded() != QFile::decodeName(file.name).toCaseFolded()) return "Unexpected installed output name: " + result.target;
    if (!matches(file.parent->descriptor, actual, file.stamp, file.linkTarget, false) || !unchanged(file)) return "Output name changed during installation: " + result.target;
    // A macOS swap can retain the displaced leaf's spelling. On insensitive
    // volumes both names address the same guarded entry; no other entry is lost.
    if (::renameatx_np(file.parent->descriptor, actual.constData(), file.parent->descriptor, file.name.constData(), 0) != 0) return ioError("Cannot restore requested output name", result.target);
    result.installedFile = captureAt(file.parent, file.name);
    if (!result.installedFile.error.isEmpty()) return result.installedFile.error;
    return result.installedFile.exists && identity(file.stamp, result.installedFile.stamp) ? QString() : "Output changed while restoring its name: " + result.target;
}
FileInstallResult FileInstall::install(const FileSnapshot &original, const FileSnapshot &destination, const std::shared_ptr<OperationControl> &control, bool renameExisting, const FileInstallObserver &observer, bool hardLink) {
    auto source = original;
    FileInstallResult result; result.target = destination.path;
    auto fail = [&](QString error) { result.error = std::move(error); return result; };
    if (!source.error.isEmpty() || !destination.error.isEmpty()) return fail(source.error + destination.error);
    if (!source.exists || (!S_ISREG(source.stamp.st_mode) && !S_ISLNK(source.stamp.st_mode))) return fail("Unsupported source file: " + source.path);
    if (destination.exists && !S_ISREG(destination.stamp.st_mode) && !S_ISLNK(destination.stamp.st_mode) && !S_ISDIR(destination.stamp.st_mode)) return fail("Unsupported destination file: " + destination.path);
    if (destination.exists && identity(source.stamp, destination.stamp)) {
        if (hardLink && unchanged(source) && unchanged(destination)) { result.installed = true; result.installedFile = destination; return result; }
        return fail("Cannot copy or move a file onto itself: " + source.path);
    }
    // Replacing by rename is permitted by POSIX directory permissions even
    // when the existing file is read-only. Preserve the File Manager's guard.
    if (destination.exists && !renameExisting && S_ISREG(destination.stamp.st_mode) && !(destination.stamp.st_mode & 0222)) return fail("Cannot overwrite read-only output: " + destination.path);
    if (!unchanged(source) || !unchanged(destination)) return fail("File or folder changed before installation: " + destination.path);
    if (destination.exists && S_ISDIR(destination.stamp.st_mode) && !renameExisting && !directoryEmpty(destination.parent->descriptor, destination.name)) return fail("Cannot replace a nonempty directory: " + destination.path);
    if (control->checkpoint()) { result.cancelled = true; return fail("Operation cancelled."); }
    Stage stage(destination.parent); if (stage.descriptor < 0) return fail(ioError("Cannot prepare output staging", destination.path));
    const bool symbolic = S_ISLNK(source.stamp.st_mode);
    if (hardLink && !S_ISREG(source.stamp.st_mode)) return fail("Hard link target must be a regular file: " + source.path);
    const int input = ::openat(source.parent->descriptor, source.name.constData(), (hardLink ? O_EVTONLY : O_RDONLY) | O_CLOEXEC | (symbolic ? O_SYMLINK : O_NOFOLLOW));
    if (input < 0) return fail(ioError("Cannot read source", source.path)); const auto closeInput = qScopeGuard([&] { ::close(input); });
    struct stat opened{}; if (::fstat(input, &opened) != 0 || !same(source.stamp, opened)) return fail("Source changed before copying: " + source.path);
    int output;
    if (hardLink) {
        if (::linkat(source.parent->descriptor, source.name.constData(), stage.descriptor, "replacement", 0) != 0) return fail(ioError("Cannot prepare hard link", destination.path));
        if (::fstatat(stage.descriptor, "replacement", &stage.disposable, AT_SYMLINK_NOFOLLOW) != 0) return fail(ioError("Cannot inspect prepared hard link", destination.path));
        stage.ownedEntry = true;
        // Creating our new name changes only link count/ctime on the target.
        struct stat current{}, expected = source.stamp; ++expected.st_nlink;
        if (::fstat(input, &current) != 0 || !same(expected, current, true)) return fail("Hard link target changed during preparation: " + source.path);
        source.stamp = current;
        output = ::openat(stage.descriptor, "replacement", O_EVTONLY | O_NOFOLLOW | O_CLOEXEC);
    } else if (symbolic) {
        if (::symlinkat(source.linkTarget.constData(), stage.descriptor, "replacement") != 0) return fail(ioError("Cannot prepare symbolic link", destination.path));
        output = ::openat(stage.descriptor, "replacement", O_RDONLY | O_SYMLINK | O_CLOEXEC);
    } else output = ::openat(stage.descriptor, "replacement", O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (output < 0) return fail(ioError("Cannot create prepared output", destination.path)); const auto closeOutput = qScopeGuard([&] { if (output >= 0) ::close(output); });
    if (::fstat(output, &stage.disposable) != 0) return fail(ioError("Cannot inspect prepared output", destination.path)); stage.ownedEntry = true;
    if (!symbolic && !hardLink) {
        QByteArray buffer(1024 * 1024, Qt::Uninitialized);
        for (;;) {
            if (control->checkpoint()) { result.cancelled = true; return fail("Operation cancelled."); }
            ssize_t length; do { length = ::read(input, buffer.data(), buffer.size()); } while (length < 0 && errno == EINTR);
            if (length < 0) return fail(ioError("Cannot read source", source.path)); if (!length) break;
            ssize_t offset = 0; while (offset < length) { ssize_t written; do { written = ::write(output, buffer.data() + offset, size_t(length - offset)); } while (written < 0 && errno == EINTR); if (written <= 0) return fail(ioError("Cannot write prepared output", destination.path)); offset += written; }
        }
        if (::fcopyfile(input, output, nullptr, COPYFILE_ACL | COPYFILE_XATTR) != 0 || ::fchmod(output, source.stamp.st_mode & 07777) != 0) return fail(ioError("Cannot preserve output metadata", destination.path));
        const timespec times[2]{source.stamp.st_atimespec, source.stamp.st_mtimespec};
        if (::futimens(output, times) != 0 || ::fsync(output) != 0) return fail(ioError("Cannot finish prepared output", destination.path));
    } else if (symbolic && ::fcopyfile(input, output, nullptr, COPYFILE_METADATA) != 0) return fail(ioError("Cannot preserve symbolic-link metadata", destination.path));
    // SMB can finalize timestamps when our write handle closes. Complete that
    // operation before taking the snapshot used by fallback copying/publication.
    struct stat written{}; if (::fstat(output, &written) != 0) return fail(ioError("Cannot inspect prepared output", destination.path));
    const int closed = ::close(output); output = -1;
    if (closed != 0) return fail(ioError("Cannot close prepared output", destination.path));
    // Opening the completed private leaf refreshes SMB attributes; a path-only
    // stat immediately after close can still return the client's old cache.
    output = ::openat(stage.descriptor, "replacement", (hardLink ? O_EVTONLY : O_RDONLY) | O_CLOEXEC | (symbolic ? O_SYMLINK : O_NOFOLLOW));
    if (output < 0) return fail(ioError("Cannot open completed preparation", destination.path));
    struct stat prepared{};
    if (::fstat(output, &prepared) != 0) return fail(ioError("Cannot inspect completed preparation", destination.path));
    if (!sameAttributes(written, prepared))
        return fail("Prepared output changed while closing; previous data retained: " + destination.path);
    if (observer) observer(FileInstallPhase::Prepared, stage.path());
    if (!unchanged(source) || !unchanged(destination) || !matches(stage.descriptor, "replacement", prepared, source.linkTarget)) return fail("File or folder changed during copying; previous data retained: " + destination.path);
    if (control->checkpoint()) { result.cancelled = true; return fail("Operation cancelled."); }
    if (observer) observer(FileInstallPhase::Validated, stage.path());
    stage.preserve = true;
    if (::renameatx_np(stage.descriptor, "replacement", destination.parent->descriptor, destination.name.constData(), destination.exists ? RENAME_SWAP : RENAME_EXCL) != 0) {
        stage.preserve = false;
        if (unsupportedRename() && !hardLink) return installExclusive(stage, source, destination, control, renameExisting, observer);
        return fail(ioError("Cannot install output; previous data retained", destination.path));
    }
    if (observer) observer(FileInstallPhase::Installed, stage.path());
    auto candidateInstalled = [&] { return matches(destination.parent->descriptor, destination.name, prepared, source.linkTarget, true); };
    auto success = [&] {
        result.installedFile = captureAt(destination.parent, destination.name);
        if (!result.installedFile.exists || !result.installedFile.error.isEmpty() || !same(prepared, result.installedFile.stamp, true) || result.installedFile.linkTarget != source.linkTarget) return fail("Installed output changed; source retained: " + destination.path);
        result.installed = true; return result;
    };
    if (!destination.exists) {
        stage.ownedEntry = false;
        if (candidateInstalled() && parentUnchanged(destination.parent)) { stage.preserve = false; return success(); }
        result.recovery = actualPath(destination.parent->descriptor, destination.parent->path) + '/' + QFile::decodeName(destination.name);
        return fail("Output folder or installed file changed. Prepared output location: " + result.recovery);
    }
    struct stat displaced{}; const bool haveDisplaced = ::fstatat(stage.descriptor, "replacement", &displaced, AT_SYMLINK_NOFOLLOW) == 0;
    if (matches(stage.descriptor, "replacement", destination.stamp, destination.linkTarget, true) && candidateInstalled() && parentUnchanged(destination.parent)) {
        if (renameExisting) {
            for (unsigned attempt = 0; attempt < 64; ++attempt) {
                const auto renamed = nameInDirectory(destination); if (renamed.isEmpty()) break;
                const auto bytes = QFile::encodeName(renamed);
                if (::renameatx_np(stage.descriptor, "replacement", destination.parent->descriptor, bytes.constData(), RENAME_EXCL) == 0) {
                    stage.ownedEntry = false; stage.preserve = false;
                    if (candidateInstalled() && parentUnchanged(destination.parent) && matches(destination.parent->descriptor, bytes, destination.stamp, destination.linkTarget, true)) { result.preservedFile = captureAt(destination.parent, bytes); return success(); }
                    result.recovery = actualPath(destination.parent->descriptor, destination.parent->path) + '/' + renamed;
                    return fail("Output or backup changed after installation; source retained. Previous output location: " + result.recovery);
                }
                if (errno != EEXIST) break;
            }
        } else {
            if (::unlinkat(stage.descriptor, "replacement", S_ISDIR(displaced.st_mode) ? AT_REMOVEDIR : 0) == 0) { stage.ownedEntry = false; stage.preserve = false; return success(); }
            result.recovery = stage.path(); result.installed = true; return fail(ioError("Output installed, but previous data retained in " + result.recovery, destination.path));
        }
    }
    if (observer) observer(FileInstallPhase::Rollback, stage.path());
    struct stat current{};
    if (haveDisplaced && ::fstatat(stage.descriptor, "replacement", &current, AT_SYMLINK_NOFOLLOW) == 0 && identity(displaced, current) && candidateInstalled() &&
        ::renameatx_np(stage.descriptor, "replacement", destination.parent->descriptor, destination.name.constData(), RENAME_SWAP) == 0 && matches(stage.descriptor, "replacement", prepared, source.linkTarget, true)) {
        stage.disposable = prepared; stage.ownedEntry = true; stage.preserve = false; return fail("Destination changed or installation failed; previous entry restored: " + destination.path);
    }
    result.recovery = stage.path(); return fail("Destination changed during installation. Recovery files retained in: " + result.recovery + "\nInspect destination: " + destination.path);
}
FileInstallResult FileInstall::transferFile(QString source, QString destination, QString &mode, OverwriteBroker &broker, const std::shared_ptr<OperationControl> &control, OverwriteFileInfo incoming, bool allowDestinationLinks, const FileInstallObserver &observer) {
    return transfer(capture(source, true), capture(destination), mode, broker, control, incoming, allowDestinationLinks, observer);
}
FileInstallResult FileInstall::transfer(const FileSnapshot &original, FileSnapshot target, QString &mode, OverwriteBroker &broker, const std::shared_ptr<OperationControl> &control, OverwriteFileInfo incoming, bool allowDestinationLinks, const FileInstallObserver &observer, bool move, bool hardLink) {
    QString destination = target.path; FileInstallResult result; result.target = destination;
    if (!QStringList{"ask", "overwrite", "skip", "rename", "renameExisting"}.contains(mode)) { result.error = "Unsupported overwrite mode: " + mode; return result; }
    if (!original.error.isEmpty() || !target.error.isEmpty()) { result.error = original.error + target.error; return result; }
    if (!hardLink && original.exists && target.exists && identity(original.stamp, target.stamp)) { result.error = "Cannot copy or move a file onto itself: " + original.path; return result; }
    if (target.exists && S_ISLNK(target.stamp.st_mode) && !allowDestinationLinks) { result.error = "Symbolic-link output refused: " + destination; return result; }
    if (target.exists && mode == "ask") {
        if (incoming.path.isEmpty()) incoming = original.info();
        const auto answer = broker.ask({0, target.info(), incoming});
        if (answer == OverwriteAnswer::Cancel) { control->cancel(); result.cancelled = true; result.error = "Operation cancelled."; return result; }
        if (answer == OverwriteAnswer::No || answer == OverwriteAnswer::NoToAll) { if (answer == OverwriteAnswer::NoToAll) mode = "skip"; result.skipped = true; return result; }
        if (answer == OverwriteAnswer::YesToAll) mode = "overwrite";
        if (answer == OverwriteAnswer::AutoRename) mode = "rename";
        // The approval belongs to this exact destination snapshot.
        if (!unchanged(target)) { result.error = "Destination changed while awaiting confirmation; previous data retained: " + destination; return result; }
    }
    if (target.exists && mode == "skip") { result.skipped = true; return result; }
    if (target.exists && mode == "rename") {
        const auto originalTarget = target;
        for (unsigned attempt = 0; attempt < 64; ++attempt) {
            destination = autoName(originalTarget); if (destination.isEmpty()) { result.error = "Cannot create an automatic output name."; return result; }
            target = capture(destination); if (!target.error.isEmpty() || !target.exists) break;
        }
        if (target.exists) { result.error = "Automatic output name is occupied; existing data retained: " + destination; return result; }
    }
    if (move) { const auto moved = tryMove(original, target, control, mode == "renameExisting", observer); if (moved) return *moved; }
    return install(original, target, control, mode == "renameExisting", observer, hardLink);
}
static std::optional<FileInstallResult> moveSnapshot(const FileSnapshot &source, const FileSnapshot &requestedDestination,
    const std::shared_ptr<OperationControl> &control, bool renameExisting, const FileInstallObserver &observer, bool renameOnly) {
    auto destination = requestedDestination;
    FileInstallResult result; result.target = destination.path;
    auto fail = [&](QString error) { result.error = std::move(error); return result; };
    if (!source.error.isEmpty() || !destination.error.isEmpty()) return fail(source.error + destination.error);
    if (!source.exists || !source.parent || !destination.parent) return fail("Source does not exist: " + source.path);
    if (renameOnly) {
        if (!FileInstall::unchanged(source) || !FileInstall::unchanged(destination)) return fail("File or folder changed before renaming: " + source.path);
        const bool sameFolder = identity(source.parent->stamp, destination.parent->stamp);
        if (sameFolder && source.name == destination.name) { result.installed = true; result.installedFile = source; return result; }
        if (destination.exists) {
            if (!sameFolder || !identity(source.stamp, destination.stamp)) return fail("Destination already exists: " + destination.path);
            // On an insensitive filesystem the new spelling resolves to the
            // source. A distinct hard-link entry must still be refused.
            DIR *entries = ::fdopendir(::openat(source.parent->descriptor, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
            if (!entries) return fail(ioError("Cannot enumerate rename folder", source.path));
            bool exact = false; int readError = 0;
            for (;;) { errno = 0; auto entry = ::readdir(entries); if (!entry) { readError = errno; break; } if (QFile::decodeName(entry->d_name).normalized(QString::NormalizationForm_C) == QFile::decodeName(destination.name).normalized(QString::NormalizationForm_C)) { exact = true; break; } }
            ::closedir(entries); if (readError) return fail("Cannot enumerate rename folder: " + source.path);
            if (exact) return fail("Destination already exists: " + destination.path);
            destination.exists = false; // It becomes absent when source is staged.
        }
    }
    if (source.stamp.st_dev != destination.parent->stamp.st_dev || (S_ISDIR(source.stamp.st_mode) && destination.exists)) return {};
    if (!S_ISREG(source.stamp.st_mode) && !S_ISLNK(source.stamp.st_mode) && !S_ISDIR(source.stamp.st_mode)) return {};
    if (destination.exists && (identity(source.stamp, destination.stamp) || S_ISDIR(destination.stamp.st_mode))) return fail("Cannot move onto the same file or a folder: " + destination.path);
    if (destination.exists && !renameExisting && S_ISREG(destination.stamp.st_mode) && !(destination.stamp.st_mode & 0222)) return fail("Cannot overwrite read-only output: " + destination.path);
    if (!FileInstall::unchanged(source) || !FileInstall::unchanged(renameOnly ? requestedDestination : destination)) return fail("File or folder changed before moving: " + destination.path);
    if (control->checkpoint()) { result.cancelled = true; return fail("Operation cancelled."); }
    Stage stage(destination.parent); if (stage.descriptor < 0) return fail(ioError("Cannot prepare move", destination.path));
    if (::renameatx_np(source.parent->descriptor, source.name.constData(), stage.descriptor, "replacement", RENAME_EXCL) != 0) {
        if (errno == EXDEV || unsupportedRename()) return {}; return fail(ioError("Cannot move source", source.path));
    }
    stage.preserve = true; // A moved source must never be deleted by cleanup.
    auto restoreSource = [&](QString error) {
        struct stat actual{};
        if (::fstatat(stage.descriptor, "replacement", &actual, AT_SYMLINK_NOFOLLOW) == 0 && identity(source.stamp, actual) &&
            ::renameatx_np(stage.descriptor, "replacement", source.parent->descriptor, source.name.constData(), RENAME_EXCL) == 0) stage.preserve = false;
        else { result.recovery = stage.path(); error += "\nSource recovery location: " + result.recovery; }
        return fail(error);
    };
    if (observer) observer(FileInstallPhase::Prepared, stage.path());
    const auto sourceSlot = FileInstall::captureAt(source.parent, source.name);
    if (!sourceSlot.error.isEmpty() || sourceSlot.exists || !parentUnchanged(source.parent) || !FileInstall::unchanged(destination) || !matches(stage.descriptor, "replacement", source.stamp, source.linkTarget, true))
        return restoreSource("Source or destination changed during move preparation.");
    if (control->checkpoint()) { result.cancelled = true; return restoreSource("Operation cancelled."); }
    if (observer) observer(FileInstallPhase::Validated, stage.path());
    if (::renameatx_np(stage.descriptor, "replacement", destination.parent->descriptor, destination.name.constData(), destination.exists ? RENAME_SWAP : RENAME_EXCL) != 0)
        return restoreSource(ioError("Cannot install moved output", destination.path));
    if (observer) observer(FileInstallPhase::Installed, stage.path());
    auto candidateInstalled = [&] { return matches(destination.parent->descriptor, destination.name, source.stamp, source.linkTarget, true); };
    auto success = [&] {
        result.installedFile = FileInstall::captureAt(destination.parent, destination.name); result.sourceMoved = true; stage.preserve = false;
        if (!result.installedFile.error.isEmpty() || !result.installedFile.exists || !same(source.stamp, result.installedFile.stamp, true) || result.installedFile.linkTarget != source.linkTarget) {
            result.recovery = destination.path; return fail("Moved output changed; inspect: " + result.recovery);
        }
        result.installed = true; return result;
    };
    if (!destination.exists) {
        if (candidateInstalled() && parentUnchanged(destination.parent) && parentUnchanged(source.parent)) return success();
        result.sourceMoved = true; result.recovery = actualPath(destination.parent->descriptor, destination.parent->path) + '/' + QFile::decodeName(destination.name);
        stage.preserve = false; return fail("Moved output or its folder changed. Inspect: " + result.recovery);
    }
    if (candidateInstalled() && parentUnchanged(destination.parent) && parentUnchanged(source.parent) && matches(stage.descriptor, "replacement", destination.stamp, destination.linkTarget, true)) {
        if (renameExisting) {
            for (unsigned attempt = 0; attempt < 64; ++attempt) {
                const auto name = nameInDirectory(destination); if (name.isEmpty()) break; const auto bytes = QFile::encodeName(name);
                if (::renameatx_np(stage.descriptor, "replacement", destination.parent->descriptor, bytes.constData(), RENAME_EXCL) == 0) {
                    result.recovery = destination.parent->path + '/' + name;
                    if (candidateInstalled() && matches(destination.parent->descriptor, bytes, destination.stamp, destination.linkTarget, true) && parentUnchanged(destination.parent)) { result.preservedFile = FileInstall::captureAt(destination.parent, bytes); return success(); }
                    stage.preserve = false; result.sourceMoved = true; return fail("Moved output or previous output changed. Inspect: " + result.recovery);
                }
                if (errno != EEXIST) break;
            }
        } else if (::unlinkat(stage.descriptor, "replacement", 0) == 0) return success();
    }
    if (observer) observer(FileInstallPhase::Rollback, stage.path());
    // Preserve concurrent destination edits. Roll back only while the installed
    // candidate is still our exact source identity and content stamp.
    if (candidateInstalled() && ::renameatx_np(stage.descriptor, "replacement", destination.parent->descriptor, destination.name.constData(), RENAME_SWAP) == 0)
        return restoreSource("Destination changed or move installation failed; previous entry restored.");
    result.sourceMoved = true; result.recovery = stage.path();
    return fail("Destination changed during move. Recovery files retained in: " + result.recovery + "\nInspect moved output: " + destination.path);
}
std::optional<FileInstallResult> FileInstall::tryMove(const FileSnapshot &source, const FileSnapshot &destination,
    const std::shared_ptr<OperationControl> &control, bool renameExisting, const FileInstallObserver &observer) {
    return moveSnapshot(source, destination, control, renameExisting, observer, false);
}
FileInstallResult FileInstall::rename(const FileSnapshot &source, const FileSnapshot &destination, const std::shared_ptr<OperationControl> &control) {
    auto result = moveSnapshot(source, destination, control, false, {}, true);
    if (result) return *result;
    FileInstallResult failure; failure.target = destination.path; failure.error = "Filesystem does not support an exclusive label rename: " + source.path; return failure;
}
QString FileInstall::removeMovedSource(const FileSnapshot &source) {
    if (!source.exists || !unchanged(source)) return "Copied successfully, but source changed; source retained: " + source.path;
    Stage stage(source.parent); if (stage.descriptor < 0) return ioError("Copied successfully, but cannot prepare source removal", source.path);
    stage.preserve = true;
    if (renameIntoStage(source.parent->descriptor, source.name, stage, "replacement") != 0) { stage.preserve = false; return ioError("Copied successfully, but cannot remove source", source.path); }
    if (matches(stage.descriptor, "replacement", source.stamp, source.linkTarget, true) && parentUnchanged(source.parent)) {
        if (::unlinkat(stage.descriptor, "replacement", S_ISDIR(source.stamp.st_mode) ? AT_REMOVEDIR : 0) == 0) { stage.preserve = false; return {}; }
    }
    if (::renameatx_np(stage.descriptor, "replacement", source.parent->descriptor, source.name.constData(), RENAME_EXCL) == 0) { stage.preserve = false; return "Copied successfully, but source removal failed; source restored: " + source.path; }
    return "Copied successfully, but source removal failed. Source recovery retained in: " + stage.path();
}
