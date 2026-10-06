// SPDX-License-Identifier: LGPL-3.0-or-later
#include "VersionControl.h"
#include "FileInstall.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QTimeZone>
#include <QUuid>
#include <QtConcurrent>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/attr.h>
#include <unistd.h>
#include <vector>

namespace {
using UInt64 = quint64; using UInt32 = quint32; using Int32 = qint32;
using DWORD = quint32; using INT_PTR = int; using FChar = char;
constexpr DWORD FILE_ATTRIBUTE_READONLY = 1;
constexpr int IDYES = 6;
#define Z7_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
struct FString {
    QString value;
    mutable QByteArray bytes;
    FString() = default;
    FString(QString text) : value(std::move(text)) {}
    FString(const char *text) : value(QString::fromUtf8(text)) {}
    bool IsEmpty() const { return value.isEmpty(); }
    void Replace(char from, char to) { value.replace(QChar(from), QChar(to)); }
    void Add_PathSepar() { if (!value.endsWith('/')) value += '/'; }
    FString &operator+=(const FString &other) { value += other.value; return *this; }
    operator const char *() const { bytes = value.toUtf8(); return bytes.constData(); }
};
FString operator+(const FString &a, const FString &b) { return a.value + b.value; }
using UString = FString;
FString us2fs(const UString &s) { return s; }
UString fs2us(const FString &s) { return s; }
struct AString {
    QByteArray value;
    void Add_UInt32(UInt32 n) { value += QByteArray::number(n); }
    unsigned Len() const { return unsigned(value.size()); }
    void InsertAtFront(char c) { value.prepend(c); }
    operator FString() const { return QString::fromUtf8(value); }
};
struct CByteBuffer {
    std::vector<char> bytes;
    void Alloc(size_t size) { bytes.resize(size); }
    size_t Size() const { return bytes.size(); }
    operator char *() { return bytes.data(); }
    operator const char *() const { return bytes.data(); }
    bool operator==(const CByteBuffer &other) const { return bytes == other.bytes; }
};
template<class T> struct CRecordVector : std::vector<T> { unsigned Size() const { return unsigned(this->size()); } };
struct FILETIME { DWORD dwLowDateTime = 0, dwHighDateTime = 0; };
struct BY_HANDLE_FILE_INFORMATION {
    DWORD nFileSizeHigh = 0, nFileSizeLow = 0, dwFileAttributes = 0;
    FILETIME ftCreationTime, ftLastAccessTime, ftLastWriteTime;
};
UInt64 ticks(const FILETIME &t) { return t.dwLowDateTime | (UInt64(t.dwHighDateTime) << 32); }
FILETIME fileTime(timespec time) {
    const UInt64 t = UInt64(time.tv_sec + 11644473600LL) * 10000000ULL + UInt64(time.tv_nsec / 100);
    return {DWORD(t), DWORD(t >> 32)};
}
timespec posixTime(const FILETIME &time) {
    const auto t = ticks(time); return {time_t(t / 10000000ULL) - 11644473600LL, long(t % 10000000ULL) * 100};
}
int CompareFileTime(const FILETIME *a, const FILETIME *b) { return ticks(*a) < ticks(*b) ? -1 : ticks(*a) > ticks(*b) ? 1 : 0; }
void SetLastError(int) { errno = EIO; }
UInt64 ConvertStringToUInt64(const char *text, const char **end) {
    UInt64 value = 0; const auto first = text;
    while (*text >= '0' && *text <= '9') {
        const auto digit = unsigned(*text - '0');
        if (value > (std::numeric_limits<UInt64>::max() - digit) / 10) { *end = first; return 0; }
        value = value * 10 + digit; ++text;
    }
    *end = text; return value;
}
struct Context {
    VersionResult result;
    QString store;
    std::shared_ptr<OperationControl> control;
    OverwriteBroker *broker;
    QHash<QString, FileSnapshot> snapshots;
    QHash<QString, std::function<bool(DWORD)>> pendingWrites;
    std::function<void(quint64, quint64, QString)> progress;
    void checkpoint() const { if (control->checkpoint()) throw QString("Operation cancelled. Completed version files were retained."); }
};
thread_local Context *context = nullptr;
QString key(const FString &path) { return QDir::cleanPath(QFileInfo(path.value).absoluteFilePath()); }
[[noreturn]] void lastError() { throw QString::fromLocal8Bit(std::strerror(errno)); }
FileSnapshot capture(const FString &path) {
    auto snapshot = FileInstall::capture(path.value, true);
    if (!snapshot.error.isEmpty()) throw snapshot.error;
    return snapshot;
}
int openCaptured(const FileSnapshot &snapshot, int flags, mode_t mode = 0600) {
    if (!snapshot.parent || !FileInstall::unchanged(snapshot)) { errno = ESTALE; return -1; }
    return ::openat(snapshot.parent->descriptor, snapshot.name.constData(), flags | O_NOFOLLOW | O_CLOEXEC, mode);
}
bool sameIdentity(int fd, const FileSnapshot &snapshot) {
    struct stat now{};
    return ::fstat(fd, &now) == 0 && S_ISREG(now.st_mode) && now.st_dev == snapshot.stamp.st_dev && now.st_ino == snapshot.stamp.st_ino &&
        now.st_size == snapshot.stamp.st_size && now.st_mtimespec.tv_sec == snapshot.stamp.st_mtimespec.tv_sec && now.st_mtimespec.tv_nsec == snapshot.stamp.st_mtimespec.tv_nsec;
}
DWORD attributes(mode_t mode) { return (DWORD(mode & 07777) << 16) | ((mode & 0222) ? 0 : FILE_ATTRIBUTE_READONLY); }
namespace NAttributes { bool IsReadOnly(DWORD value) { return value & FILE_ATTRIBUTE_READONLY; } }
bool SetFileAttrib(const FString &path, DWORD value) {
    context->checkpoint();
    const auto pending = context->pendingWrites.value(key(path));
    if (pending) return pending(value);
    const auto snapshot = context->snapshots.value(key(path), capture(path));
    const int fd = openCaptured(snapshot, O_RDONLY); if (fd < 0) return false;
    mode_t mode = (value >> 16) & 07777;
    mode = value & FILE_ATTRIBUTE_READONLY ? mode & ~mode_t(0222) : mode | S_IWUSR;
    const bool ok = sameIdentity(fd, snapshot) && ::fchmod(fd, mode) == 0; ::close(fd); return ok;
}
namespace NName { void NormalizeDirPathPrefix(UString &path) { path.Add_PathSepar(); } }
bool GetFullPathAndSplit(const FString &path, FString &directory, FString &name) {
    const QFileInfo info(path.value); directory = info.absolutePath() + '/'; name = info.fileName();
    if (name.IsEmpty()) { errno = EINVAL; return false; } return true;
}
bool CreateComplexDir(const FString &path) {
    context->checkpoint(); const auto error = FileInstall::ensureDirectory(path.value);
    if (!error.isEmpty()) throw error; return true;
}
namespace NDir {
bool MyMoveFile(const FString &from, const FString &to) {
    context->checkpoint(); auto source = capture(from), destination = capture(to);
    if (destination.exists) { errno = EEXIST; return false; }
    const auto saved = context->snapshots.value(key(from));
    if (saved.exists && !FileInstall::unchanged(saved)) { errno = ESTALE; return false; }
    const auto result = FileInstall::tryMove(source, destination, context->control);
    if (!result) { errno = EXDEV; return false; }
    if (!result->error.isEmpty()) throw result->error;
    context->snapshots.remove(key(from)); return result->sourceMoved;
}
}
namespace NFind {
struct CDirEntry { FString Name; };
struct CEnumerator {
    QStringList names; qsizetype row = 0;
    void SetDirPrefix(const FString &path) { names = QDir(path.value).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System); }
    bool Next(CDirEntry &entry) { context->checkpoint(); if (row >= names.size()) return false; entry.Name = names[row++]; return true; }
};
}
namespace NIO {
struct CFile {
    int fd = -1;
    QString path;
    FileSnapshot snapshot;
    ~CFile() { if (fd >= 0) ::close(fd); }
    bool Open(const FString &name) {
        context->checkpoint(); path = key(name);
        if (!QFileInfo::exists(name.value) && !QFileInfo(name.value).isSymLink()) { errno = ENOENT; return false; }
        snapshot = capture(name);
        if (!snapshot.exists) { errno = ENOENT; return false; }
        if (!S_ISREG(snapshot.stamp.st_mode)) { errno = EINVAL; return false; }
        fd = openCaptured(snapshot, O_RDONLY); if (fd < 0) return false;
        if (!sameIdentity(fd, snapshot)) { errno = ESTALE; return false; }
        context->snapshots.insert(path, snapshot); return true;
    }
    bool SetTime(const FILETIME *creation, const FILETIME *access, const FILETIME *modified) {
        struct stat stamp{}; if (::fstat(fd, &stamp) != 0) return false;
        timespec times[2] = {access ? posixTime(*access) : stamp.st_atimespec, modified ? posixTime(*modified) : stamp.st_mtimespec};
        if (::futimens(fd, times) != 0) return false;
        if (creation) { attrlist attrs{}; attrs.bitmapcount = ATTR_BIT_MAP_COUNT; attrs.commonattr = ATTR_CMN_CRTIME; auto time = posixTime(*creation); if (::fsetattrlist(fd, &attrs, &time, sizeof(time), 0) != 0 && errno != ENOTSUP) return false; }
        if (!context->pendingWrites.contains(path)) { struct stat now{}; if (::fstat(fd, &now) != 0) return false; snapshot.stamp = now; context->snapshots.insert(path, snapshot); } return true;
    }
    bool SetMTime(const FILETIME *time) { return SetTime(nullptr, nullptr, time); }
};
struct CInFile : CFile {
    bool GetFileInformation(BY_HANDLE_FILE_INFORMATION *info) {
        struct stat stamp{}; if (::fstat(fd, &stamp) != 0) return false;
        info->nFileSizeHigh = DWORD(UInt64(stamp.st_size) >> 32); info->nFileSizeLow = DWORD(stamp.st_size);
        info->dwFileAttributes = attributes(stamp.st_mode); info->ftCreationTime = fileTime(stamp.st_birthtimespec);
        info->ftLastAccessTime = fileTime(stamp.st_atimespec); info->ftLastWriteTime = fileTime(stamp.st_mtimespec); return true;
    }
    bool ReadFull(char *data, size_t size, size_t &processed) {
        processed = 0;
        while (processed < size) { context->checkpoint(); const auto count = ::read(fd, data + processed, qMin<size_t>(size - processed, 1024 * 1024)); if (count < 0) { if (errno == EINTR) continue; return false; } if (!count) break; processed += size_t(count); context->progress(processed, size, path); }
        if (!sameIdentity(fd, snapshot)) { errno = ESTALE; return false; } return true;
    }
};
struct COutFile : CFile {
    QByteArray temporary;
    ~COutFile() {
        if (!temporary.isEmpty()) {
            context->pendingWrites.remove(path);
            struct stat own{}, current{};
            if (fd >= 0 && ::fstat(fd, &own) == 0 && ::fstatat(snapshot.parent->descriptor, temporary.constData(), &current, AT_SYMLINK_NOFOLLOW) == 0 && own.st_dev == current.st_dev && own.st_ino == current.st_ino)
                ::unlinkat(snapshot.parent->descriptor, temporary.constData(), 0);
        }
    }
    bool Create_ALWAYS_or_NEW(const FString &name, bool always) {
        context->checkpoint(); path = key(name); snapshot = capture(name);
        if (always) {
            const auto original = context->snapshots.value(path);
            if (!original.exists || !FileInstall::unchanged(original)) { errno = ESTALE; return false; }
            snapshot = original;
        } else {
            if (snapshot.exists) { errno = EEXIST; return false; }
        }
        temporary = ".7vc-" + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
        auto staged = FileInstall::captureAt(snapshot.parent, temporary);
        fd = openCaptured(staged, O_WRONLY | O_CREAT | O_EXCL); if (fd < 0) return false;
        context->pendingWrites.insert(path, [this](DWORD value) {
            mode_t mode = (value >> 16) & 07777; mode = value & FILE_ATTRIBUTE_READONLY ? mode & ~mode_t(0222) : mode | S_IWUSR;
            if (::fchmod(fd, mode) != 0 || ::fsync(fd) != 0) return false;
            const auto stagedFile = FileInstall::captureAt(snapshot.parent, temporary);
            const auto installed = FileInstall::install(stagedFile, snapshot, context->control);
            if (!installed.error.isEmpty()) throw installed.error;
            if (!installed.installed) { errno = EIO; return false; }
            context->snapshots.insert(path, installed.installedFile); return true;
        });
        return true;
    }
    bool Open_EXISTING(const FString &name) {
        path = key(name); snapshot = context->snapshots.value(path);
        fd = openCaptured(snapshot, O_WRONLY); return fd >= 0 && sameIdentity(fd, snapshot);
    }
    bool Write(const char *data, UInt32 size, UInt32 &processed) {
        processed = 0;
        while (processed < size) { context->checkpoint(); const auto count = ::write(fd, data + processed, qMin<UInt32>(size - processed, 1024 * 1024)); if (count < 0) { if (errno == EINTR) continue; return false; } if (!count) { errno = EIO; return false; } processed += UInt32(count); context->progress(processed, size, path); }
        return true;
    }
};
}
struct CPanel {
    bool Is_IO_FS_Folder() const { return true; }
    void Get_ItemIndices_Selected(CRecordVector<UInt32> &indices) const { indices.push_back(0); }
    UString GetItemFullPath(UInt32) const { return context->result.target; }
    [[noreturn]] void MessageBox_LastError() const { lastError(); }
    [[noreturn]] void MessageBox_Error(const wchar_t *text) const { throw QString::fromWCharArray(text); }
    [[noreturn]] void MessageBox_Error_UnsupportOperation() const { throw QString("Operation is not supported."); }
    int GetParent() const { return 0; }
};
struct COverwriteFileInfo : OverwriteFileInfo {
    UString Path;
    void SetTime(const FILETIME &time) { fileTime = ticks(time); modified = QDateTime::fromMSecsSinceEpoch(qint64(ticks(time) / 10000ULL) - 11644473600000LL, QTimeZone::UTC); }
    void SetSize(UInt64 value) { size = value; sizeDefined = true; }
    OverwriteFileInfo info() const { auto result = static_cast<const OverwriteFileInfo &>(*this); result.path = Path.value; return result; }
};
struct COverwriteDialog {
    COverwriteFileInfo OldFileInfo, NewFileInfo;
    bool ShowExtraButtons = true, DefaultButton_is_NO = false;
    int Create(int) { context->checkpoint(); OverwriteConflict conflict; conflict.existing = OldFileInfo.info(); conflict.incoming = NewFileInfo.info(); return context->broker->ask(conflict) == OverwriteAnswer::Yes ? IDYES : 0; }
};
struct CApp {
    CPanel panel;
    const CPanel &GetFocusedPanel() const { return panel; }
    void ReadReg_VerCtrlPath(UString &path) const { path = context->store; }
    void DiffFiles(const UString &old, const UString &current) { context->result.diffPaths = {QDir::cleanPath(old.value), QDir::cleanPath(current.value)}; }
    void VerCtrl(unsigned id);
};
#include "upstream/VerCtrl.inc"
#undef Z7_ARRAY_SIZE
QString canonicalStore(QString path) {
    QString cursor = QFileInfo(path).absoluteFilePath(); QStringList tail;
    while (!QFileInfo::exists(cursor)) { const QFileInfo info(cursor); if (info.isSymLink() || info.fileName().isEmpty()) throw QString("Invalid version-store path: ") + path; tail.prepend(info.fileName()); cursor = info.absolutePath(); }
    const auto prefix = QFileInfo(cursor).canonicalFilePath();
    if (prefix.isEmpty() || !QFileInfo(prefix).isDir()) throw QString("Invalid version-store folder: ") + path;
    return tail.isEmpty() ? prefix : prefix + '/' + tail.join('/');
}
}

VersionControl::VersionControl(QObject *parent) : QObject(parent) {
    connect(&watcher, &QFutureWatcher<VersionResult>::started, this, [this] { emit pauseAvailabilityChanged(true); });
    connect(&watcher, &QFutureWatcher<VersionResult>::finished, this, [this] { emit pausedChanged(false); emit pauseAvailabilityChanged(false); emit finished(watcher.result()); });
    connect(&broker, &OverwriteBroker::requested, this, [this](OverwriteConflict conflict) { if (busy() && broker.current(conflict.id)) emit overwriteRequested(conflict); });
    connect(&broker, &OverwriteBroker::waitingChanged, this, [this] { emit pauseAvailabilityChanged(busy() && control && !control->isCancelled() && !broker.waiting()); });
}
VersionControl::~VersionControl() { cancel(); watcher.waitForFinished(); }
void VersionControl::cancel() { if (control) control->cancel(); broker.cancel(); emit pausedChanged(false); emit pauseAvailabilityChanged(false); }
bool VersionControl::setPaused(bool value) { if (!busy() || !control || control->isCancelled() || broker.waiting()) return false; control->setPaused(value); emit pausedChanged(value); return true; }
bool VersionControl::resolveOverwrite(quint64 id, OverwriteAnswer answer) { return busy() && broker.answer(id, answer); }
void VersionControl::start(VersionCommand operation, QString target, QString store) {
    if (busy()) return;
    control = std::make_shared<OperationControl>(); broker.reset();
    const auto stop = control;
    watcher.setFuture(QtConcurrent::run([this, operation, target, store, stop] {
        Context state{{operation, target, {}, {}}, store, stop, &broker, {}, {}, [this](quint64 done, quint64 total, QString path) { emit byteProgress(done, total, path); }};
        context = &state;
        try {
            if (!store.isEmpty()) { state.store = canonicalStore(store); CApp app; const unsigned ids[] = {IDM_VER_EDIT, IDM_VER_COMMIT, IDM_VER_REVERT, IDM_VER_DIFF}; app.VerCtrl(ids[unsigned(operation)]); }
        } catch (const QString &error) { state.result.error = error + "\nOperation: Ver " + QStringList{"Edit", "Commit", "Revert", "Diff"}.value(int(operation)) + "\nTarget: " + target; }
          catch (const std::exception &error) { state.result.error = QString::fromLocal8Bit(error.what()) + "\nTarget: " + target; }
        context = nullptr; return state.result;
    }));
}
