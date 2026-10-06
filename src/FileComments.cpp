// SPDX-License-Identifier: LGPL-3.0-or-later
// Compatible with official 7-Zip 26.03 UI/FileManager/TextPairs.cpp and
// FSFolder.cpp. The original implementation is Copyright (C) Igor Pavlov;
// its LGPL terms are included in licenses. No invented escaping is used.
#include "FileComments.h"
#include <QFile>
#include <QStringDecoder>
#include <QScopeGuard>
#include <QUuid>
#include <algorithm>
#include <copyfile.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <vector>

namespace {
constexpr qint64 limit = 1 << 28;
bool stopped(const std::shared_ptr<OperationControl> &control) { return control && control->checkpoint(); }
QString ioError(QString action) { return action + ": " + QString::fromLocal8Bit(std::strerror(errno)); }
bool same(const struct stat &a, const struct stat &b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size && a.st_mode == b.st_mode && a.st_uid == b.st_uid && a.st_gid == b.st_gid && a.st_nlink == b.st_nlink &&
        a.st_mtimespec.tv_sec == b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec == b.st_mtimespec.tv_nsec && a.st_ctimespec.tv_sec == b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec == b.st_ctimespec.tv_nsec;
}
int compare(const QString &a, const QString &b) {
    // Windows MyStringCompareNoCase compares uppercased UTF-16 code units.
    const auto count = qMin(a.size(), b.size());
    for (qsizetype i = 0; i < count; ++i) { const auto x = a[i].toUpper().unicode(), y = b[i].toUpper().unicode(); if (x != y) return x < y ? -1 : 1; }
    return a.size() == b.size() ? 0 : a.size() < b.size() ? -1 : 1;
}
QString trimmed(QString s) {
    // UString::Trim uses ASCII space, tab and LF, not all Unicode spaces.
    const auto space = [](QChar c) { return c == ' ' || c == '\t' || c == '\n'; };
    qsizetype a = 0, b = s.size(); while (a < b && space(s[a])) ++a; while (b > a && space(s[b - 1])) --b;
    return s.mid(a, b - a);
}
using CTextPair = FileCommentPair;
int MyStringCompareNoCase(const QString &a, const QString &b) { return compare(a, b); }
class PairPointerSort {
    using T = void *;
    unsigned _size;
    T *_items;
public:
    explicit PairPointerSort(std::vector<void *> &pointers) : _size(unsigned(pointers.size() - 1)), _items(pointers.data() + 1) {}
    // Sentinel storage makes the original one-based heap pointer valid.
    #include "upstream/TextPairsSort.inc"
};
template<class T> struct CObjectVector {
    QList<T> data;
    void Clear() { data.clear(); }
    unsigned Size() const { return unsigned(data.size()); }
    void Add(const T &value) { data.append(value); }
    void Insert(unsigned at, const T &value) { data.insert(at, value); }
    void Delete(unsigned at) { data.removeAt(at); }
    T &operator[](unsigned at) { return data[at]; }
    const T &operator[](unsigned at) const { return data[at]; }
    void Sort(int (*comparison)(void *const *, void *const *, void *), void *parameter) {
        std::vector<void *> pointers; pointers.reserve(data.size() + 1); pointers.push_back(nullptr);
        for (auto &value : data) pointers.push_back(&value);
        PairPointerSort(pointers).Sort(comparison, parameter);
        QList<T> sorted; sorted.reserve(data.size());
        for (size_t i = 1; i < pointers.size(); ++i) sorted.append(*static_cast<T *>(pointers[i]));
        data = std::move(sorted);
    }
};
#define FOR_VECTOR(i, v) for (unsigned i = 0; i < (v).Size(); ++i)
#include "upstream/TextPairsDeclarations.inc"
#include "upstream/TextPairs.inc"
#undef FOR_VECTOR
QByteArray serialize(const QList<FileCommentPair> &pairs) {
    CPairsStorage storage; storage.Pairs.data = pairs; QString text; storage.SaveToString(text);
    auto bytes = text.toUtf8(); if (std::any_of(bytes.cbegin(), bytes.cend(), [](char c) { return static_cast<unsigned char>(c) >= 128; })) bytes.prepend("\xef\xbb\xbf\r\n");
    return bytes;
}
}
QString FileCommentDocument::value(const QString &name) const {
    CPairsStorage storage; storage.Pairs.data = pairs; return storage.GetValue(name);
}
QString FileComments::display(QString value) { const auto end = value.indexOf(QChar(4)); return end < 0 ? value : value.left(end); }
FileCommentDocument FileComments::parse(QByteArray bytes, std::shared_ptr<OperationControl> control) {
    FileCommentDocument document; document.bytes = std::move(bytes);
    if (document.bytes.size() >= limit) { document.error = "descript.ion is too large (limit: 256 MiB)."; return document; }
    QStringDecoder decoder(QStringDecoder::Utf8); const QString text = decoder(document.bytes);
    if (decoder.hasError() || text.contains(QChar::Null)) { document.error = "descript.ion is not valid UTF-8 text."; return document; }
    CPairsStorage storage; storage.control = control;
    if (!storage.ReadFromString(text) || stopped(control)) { document.cancelled = true; return document; }
    document.pairs = std::move(storage.Pairs.data);
    return document;
}
FileCommentDocument FileComments::readAt(int directory, std::shared_ptr<OperationControl> control) {
    FileCommentDocument document;
    if (stopped(control)) { document.cancelled = true; return document; }
    if (::fstat(directory, &document.directory) != 0) { document.error = ioError("Cannot inspect comment folder"); return document; }
    const int descriptor = ::openat(directory, "descript.ion", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (descriptor < 0) { if (errno != ENOENT) document.error = ioError("Cannot read descript.ion"); return document; }
    const auto close = qScopeGuard([&] { ::close(descriptor); }); document.exists = true;
    if (::fstat(descriptor, &document.file) != 0 || !S_ISREG(document.file.st_mode)) { document.error = "descript.ion must be a regular file."; return document; }
    if (document.file.st_size >= limit) { document.error = "descript.ion is too large (limit: 256 MiB)."; return document; }
    QFile input; if (!input.open(descriptor, QIODevice::ReadOnly, QFileDevice::DontCloseHandle)) { document.error = input.errorString(); return document; }
    while (!input.atEnd()) {
        if (stopped(control)) { document.cancelled = true; return document; }
        const auto block = input.read(1024 * 1024);
        if ((block.isEmpty() && input.error() != QFileDevice::NoError) || document.bytes.size() + block.size() >= limit) { document.error = "Cannot read descript.ion completely."; return document; }
        document.bytes += block;
    }
    struct stat after{}, current{};
    if (::fstat(descriptor, &after) != 0 || !same(document.file, after) || ::fstatat(directory, "descript.ion", &current, AT_SYMLINK_NOFOLLOW) != 0 || !same(document.file, current)) { document.error = "descript.ion changed while reading."; return document; }
    auto parsed = parse(document.bytes, control); document.pairs = std::move(parsed.pairs); document.error = parsed.error; document.cancelled = parsed.cancelled;
    return document;
}
FileCommentDocument FileComments::read(QString folder, std::shared_ptr<OperationControl> control) {
    const int descriptor = ::open(QFile::encodeName(folder).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    FileCommentDocument document;
    if (descriptor < 0) document.error = ioError("Cannot read comment folder");
    else { document = readAt(descriptor, control); ::close(descriptor); }
    document.folder = folder; return document;
}
QString FileComments::write(const FileCommentDocument &original, QString name, QString value, std::shared_ptr<OperationControl> control) {
    if (!original.error.isEmpty() || original.cancelled) return "Comment data could not be read; original retained.";
    if (stopped(control)) return "Operation cancelled; original retained.";
    name = trimmed(name); value = trimmed(value);
    if (name.isEmpty() || name.contains('/') || name.contains('"') || name.contains('\t') || name.contains('\r') || name.contains('\n') || name.contains(QChar::Null) || value.contains('\r') || value.contains('\n') || value.contains(QChar::Null)) return "This name or comment cannot be represented in descript.ion.";
    auto pairs = original.pairs;
    for (qsizetype n = 1; n < pairs.size(); ++n) if (compare(pairs[n - 1].name, pairs[n].name) == 0) return "Duplicate comment names are ambiguous; original retained.";
    CPairsStorage storage; storage.Pairs.data = std::move(pairs);
    if (value.isEmpty()) storage.DeletePair(name); else storage.AddPair({name, value});
    pairs = std::move(storage.Pairs.data);
    const auto bytes = serialize(pairs); const auto parsed = parse(bytes, control);
    if (parsed.cancelled) return "Operation cancelled; original retained.";
    if (!parsed.error.isEmpty() || parsed.pairs.size() != pairs.size()) return "Comment data cannot be saved compatibly; original retained.";
    for (qsizetype i = 0; i < pairs.size(); ++i) if (parsed.pairs[i].name != pairs[i].name || parsed.pairs[i].value != pairs[i].value) return "Existing comment data cannot be saved compatibly; original retained.";
    const int directory = ::open(QFile::encodeName(original.folder).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0) return ioError("Cannot open comment folder");
    const auto closeDirectory = qScopeGuard([&] { ::close(directory); }); struct stat folder{};
    if (::fstat(directory, &folder) != 0 || folder.st_dev != original.directory.st_dev || folder.st_ino != original.directory.st_ino) return "Comment folder changed; original retained.";
    const int scan = ::openat(directory, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC); DIR *names = scan < 0 ? nullptr : ::fdopendir(scan);
    if (!names) { if (scan >= 0) ::close(scan); return ioError("Cannot inspect comment item names"); }
    int matches = 0; errno = 0;
    while (const auto item = ::readdir(names)) { const auto candidate = QFile::decodeName(item->d_name); if (candidate != "." && candidate != ".." && compare(trimmed(candidate), name) == 0) ++matches; }
    const int scanError = errno; ::closedir(names);
    if (scanError) { errno = scanError; return ioError("Cannot inspect comment item names"); }
    if (matches != 1) return "Comment item is missing or has an ambiguous case-insensitive name; original retained.";
    const auto current = readAt(directory, control);
    if (current.cancelled) return "Operation cancelled; original retained.";
    if (!current.error.isEmpty() || original.exists != current.exists || original.bytes != current.bytes || (original.exists && !same(original.file, current.file))) return "descript.ion changed; original retained.";
    if (original.exists && (!(original.file.st_mode & 0222) || original.file.st_nlink != 1)) return "descript.ion is read-only or hard-linked; original retained.";
    const auto temporary = QFile::encodeName(".7zip-comment-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
    const int outputDescriptor = ::openat(directory, temporary.constData(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0666);
    if (outputDescriptor < 0) return ioError("Cannot create temporary comment file");
    const auto cleanup = qScopeGuard([&] { ::close(outputDescriptor); ::unlinkat(directory, temporary.constData(), 0); });
    QFile output; if (!output.open(outputDescriptor, QIODevice::WriteOnly, QFileDevice::DontCloseHandle)) return output.errorString();
    for (qsizetype position = 0; position < bytes.size(); position += 1024 * 1024) {
        if (stopped(control)) return "Operation cancelled; original retained.";
        const auto amount = qMin<qsizetype>(1024 * 1024, bytes.size() - position);
        if (output.write(bytes.constData() + position, amount) != amount) return output.errorString();
    }
    if (!output.flush()) return output.errorString();
    if (original.exists) {
        const int input = ::openat(directory, "descript.ion", O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
        if (input < 0) return ioError("Cannot preserve comment metadata"); const auto closeInput = qScopeGuard([&] { ::close(input); }); struct stat info{};
        if (::fstat(input, &info) != 0 || !same(original.file, info)) return "descript.ion changed; original retained.";
        struct stat outputInfo{};
        if (::fstat(outputDescriptor, &outputInfo) != 0 ||
            ((original.file.st_uid != outputInfo.st_uid || original.file.st_gid != outputInfo.st_gid) && ::fchown(outputDescriptor, original.file.st_uid, original.file.st_gid) != 0) ||
            ::fcopyfile(input, outputDescriptor, nullptr, COPYFILE_XATTR | COPYFILE_ACL) != 0 || ::fchmod(outputDescriptor, original.file.st_mode & 07777) != 0) return ioError("Cannot preserve comment permissions or attributes");
    }
    if (::fsync(outputDescriptor) != 0) return ioError("Cannot flush comment file");
    const auto final = readAt(directory, control);
    if (final.cancelled || stopped(control)) return "Operation cancelled; original retained.";
    struct stat pathFolder{};
    if (!final.error.isEmpty() || final.exists != original.exists || final.bytes != original.bytes || (original.exists && !same(final.file, original.file)) ||
        ::stat(QFile::encodeName(original.folder).constData(), &pathFolder) != 0 || pathFolder.st_dev != folder.st_dev || pathFolder.st_ino != folder.st_ino) return "Comment file or folder changed; original retained.";
    // The final check protects normal concurrent edits, not an external writer
    // racing inside rename. Creation uses exclusive rename to prevent clobber.
    if (::renameatx_np(directory, temporary.constData(), directory, "descript.ion", original.exists ? 0 : RENAME_EXCL) != 0) return ioError("Cannot install comment file");
    return {};
}
