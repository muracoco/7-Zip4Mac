// SPDX-License-Identifier: LGPL-3.0-or-later
// Volume naming and size rules follow 7-Zip 26.03 FileManager/SplitUtils.cpp
// and PanelSplitFile.cpp. I/O uses macOS APIs without replacing existing data.
#include "FileOperations.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <limits>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <unistd.h>

namespace {
QString systemError(QString operation, QString path) {
    const int code = errno;
    return operation + ": " + path + "\n" + QString::fromLocal8Bit(std::strerror(code)) + " (" + QString::number(code) + ')';
}
QString publish(QString source, QString destination) {
    // RENAME_EXCL makes the final collision check atomic, including dangling
    // symlinks. The stage is on the destination volume, so no cross-device move.
    if (::renamex_np(QFile::encodeName(source).constData(), QFile::encodeName(destination).constData(), RENAME_EXCL) != 0)
        return systemError("Cannot create output", destination);
    return {};
}
QString destinationFolder(QString path) {
    if (QFileInfo(path).isSymLink()) return "Destination folder is a symbolic link: " + path;
    if (!QDir().mkpath(path) || !QFileInfo(path).isDir()) return "Cannot create destination folder: " + path;
    return {};
}
QString copyBytes(QFile &input, QFile &output, quint64 bytes, quint64 &processed,
                  quint64 total, const std::shared_ptr<OperationControl> &stop, FileOperations *jobs) {
    while (bytes) {
        if (stop->checkpoint()) return "Operation cancelled.";
        auto data = input.read(qMin<quint64>(bytes, 1024 * 1024));
        if (data.isEmpty()) return "Cannot read: " + input.fileName() + "\n" + (input.error() == QFile::NoError ? "Unexpected end of file." : input.errorString());
        if (output.write(data) != data.size()) return "Cannot write: " + output.fileName() + "\n" + output.errorString();
        bytes -= quint64(data.size()); processed += quint64(data.size());
        emit jobs->byteProgress(processed, total, input.fileName());
    }
    return {};
}
}
QList<quint64> FileOperations::parseVolumeSizes(QString text, QString *error) {
    if (error) error->clear();
    QList<quint64> sizes;
    // The upstream suffix is binary (K=1024), '-' starts a media description;
    // allow whitespace between the number and its suffix as the original does.
    text = text.section('-', 0, 0).trimmed();
    qsizetype pos = 0;
    bool previousNumber = false;
    while (pos < text.size()) {
        if (text[pos].isSpace()) { ++pos; continue; }
        if (previousNumber) {
            previousNumber = false; const auto suffix = text[pos].toLower();
            if (suffix == 'b') { ++pos; continue; }
            const int shift = suffix == 'k' ? 10 : suffix == 'm' ? 20 : suffix == 'g' ? 30 : suffix == 't' ? 40 : 0;
            if (shift) {
                if (sizes.last() > (std::numeric_limits<quint64>::max() >> shift)) { if (error) *error = "Volume size is too large."; return {}; }
                sizes.last() <<= shift;
                // Upstream permits KB/MB and ignores the rest of the suffix
                // token after K/M/G/T, until the next separating space.
                while (pos < text.size() && !text[pos].isSpace()) ++pos;
                continue;
            }
        }
        const auto start = pos; while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos;
        bool ok; const auto value = text.mid(start, pos - start).toULongLong(&ok);
        if (!ok || value == 0) { if (error) *error = "Incorrect volume size. Use positive byte values with B, K, M, G or T suffixes."; return {}; }
        sizes << value; previousNumber = true;
    }
    if (sizes.isEmpty() && error) *error = "Specify a volume size.";
    return sizes;
}
quint64 FileOperations::volumeCount(quint64 size, const QList<quint64> &sizes) {
    if (!size || sizes.isEmpty()) return 1;
    for (qsizetype i = 0; i < sizes.size(); ++i) {
        if (!sizes[i]) return 0;
        if (sizes[i] >= size) return quint64(i) + 1;
        size -= sizes[i];
    }
    return quint64(sizes.size()) + (size - 1) / sizes.last() + 1;
}
CombinePlan FileOperations::inspectVolumes(QString firstVolume) {
    CombinePlan plan; QFileInfo first(firstVolume);
    auto match = QRegularExpression("^(.*?)(0+1)$").match(first.fileName());
    if (!match.hasMatch() || !first.isFile() || first.isSymLink()) { plan.error = "Select the first volume (for example, filename.001)."; return plan; }
    const QString prefix = match.captured(1); const int digits = match.captured(2).size();
    QDir dir(first.absolutePath());
    // Inspect all numbered siblings first, so a missing middle part cannot
    // silently produce a truncated output. Upstream stops at the first gap.
    const QRegularExpression names("^" + QRegularExpression::escape(prefix) + "([0-9]{" + QString::number(digits) + ",})$");
    QMap<quint64, QFileInfo> numbered;
    for (const auto &entry : dir.entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot)) {
        auto m = names.match(entry.fileName()); if (!m.hasMatch()) continue;
        bool valid; const auto n = m.captured(1).toULongLong(&valid);
        if (valid && n && entry.fileName() == prefix + QString::number(n).rightJustified(digits, '0')) numbered[n] = entry;
    }
    quint64 next = 1;
    for (auto it = numbered.cbegin(); it != numbered.cend(); ++it, ++next) {
        if (it.key() != next) { plan.error = "Missing volume: " + prefix + QString::number(next).rightJustified(digits, '0'); return plan; }
        const auto f = it.value();
        if (!f.isFile() || f.isSymLink()) { plan.error = "Volume is not a regular file: " + f.filePath(); return plan; }
        if (quint64(f.size()) > std::numeric_limits<quint64>::max() - plan.totalSize) { plan.error = "Combined size is too large."; return plan; }
        plan.totalSize += quint64(f.size()); plan.volumes << f.filePath();
    }
    if (plan.volumes.size() < 2 || !plan.totalSize) { plan.error = "Two or more non-empty volumes are required."; return plan; }
    plan.outputName = prefix; while (plan.outputName.endsWith('.')) plan.outputName.chop(1);
    if (plan.outputName.isEmpty()) plan.outputName = "file";
    return plan;
}
void FileOperations::split(QString source, QString destination, QList<quint64> sizes) {
    if (busy()) return;
    stop = std::make_shared<OperationControl>(); auto flag = stop;
    active = true; watcher.setFuture(QtConcurrent::run([this, source, destination, sizes, flag]() -> QString {
        QFileInfo info(source); const quint64 total = quint64(info.size());
        if (!info.isFile() || info.isSymLink()) return "Select one regular file to split.";
        if (sizes.isEmpty() || sizes.contains(0) || total <= sizes.first()) return "File must be larger than the first volume size.";
        const auto count = volumeCount(total, sizes); const int digits = qMax(3, int(QString::number(count).size()));
        auto error = destinationFolder(destination); if (!error.isEmpty()) return error;
        const auto root = QFileInfo(destination).canonicalFilePath();
        // Check all output collisions before reading the source.
        for (quint64 n = 1; n <= count; ++n) {
            if (flag->checkpoint()) return "Operation cancelled.";
            const auto path = root + '/' + info.fileName() + '.' + QString::number(n).rightJustified(digits, '0');
            if (QFileInfo::exists(path) || QFileInfo(path).isSymLink()) return "Destination already exists: " + path;
        }
        QTemporaryDir stage(root + "/.7zip-split-XXXXXX"); if (!stage.isValid()) return "Cannot create staging directory: " + root;
        QFile input(source); if (!input.open(QIODevice::ReadOnly)) return "Cannot read: " + source + '\n' + input.errorString();
        quint64 processed = 0;
        for (quint64 n = 1; n <= count; ++n) {
            const auto name = info.fileName() + '.' + QString::number(n).rightJustified(digits, '0');
            QFile output(stage.filePath(name)); if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return "Cannot create volume: " + output.fileName() + '\n' + output.errorString();
            error = copyBytes(input, output, qMin(total - processed, sizes[qMin<quint64>(n - 1, sizes.size() - 1)]), processed, total, flag, this);
            if (!error.isEmpty()) return error;
            if (!output.flush()) return "Cannot flush volume: " + output.fileName() + '\n' + output.errorString();
            output.close();
        }
        if (!input.atEnd()) return "Source size changed during split: " + source;
        QStringList installed;
        for (quint64 n = 1; n <= count; ++n) {
            const auto name = info.fileName() + '.' + QString::number(n).rightJustified(digits, '0'); const auto path = root + '/' + name;
            error = flag->checkpoint() ? "Operation cancelled." : publish(stage.filePath(name), path);
            if (!error.isEmpty()) { for (const auto &owned : installed) QFile::remove(owned); return error; }
            installed << path;
        }
        return {};
    }));
}
void FileOperations::combine(QString firstVolume, QString destination) {
    if (busy()) return;
    stop = std::make_shared<OperationControl>(); auto flag = stop;
    active = true; watcher.setFuture(QtConcurrent::run([this, firstVolume, destination, flag]() -> QString {
        const auto plan = inspectVolumes(firstVolume); if (!plan.error.isEmpty()) return plan.error;
        auto error = destinationFolder(destination); if (!error.isEmpty()) return error;
        const auto root = QFileInfo(destination).canonicalFilePath(), target = root + '/' + plan.outputName;
        if (QFileInfo::exists(target) || QFileInfo(target).isSymLink()) return "Destination already exists: " + target;
        QTemporaryDir stage(root + "/.7zip-combine-XXXXXX"); if (!stage.isValid()) return "Cannot create staging directory: " + root;
        QFile output(stage.filePath("combined")); if (!output.open(QIODevice::WriteOnly | QIODevice::NewOnly)) return "Cannot create output: " + output.fileName() + '\n' + output.errorString();
        quint64 processed = 0;
        for (const auto &path : plan.volumes) {
            QFile input(path); if (!input.open(QIODevice::ReadOnly)) return "Cannot read volume: " + path + '\n' + input.errorString();
            error = copyBytes(input, output, quint64(input.size()), processed, plan.totalSize, flag, this);
            if (!error.isEmpty()) return error;
        }
        if (processed != plan.totalSize) return "Volume size changed during combine.";
        if (!output.flush()) return "Cannot flush output: " + target + '\n' + output.errorString();
        output.close(); if (flag->checkpoint()) return "Operation cancelled.";
        return publish(output.fileName(), target);
    }));
}
