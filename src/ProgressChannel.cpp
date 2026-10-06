// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ProgressChannel.h"
#include <QProcess>
#include <QProcessEnvironment>
#include <QSocketNotifier>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <atomic>
ProgressChannel::ProgressChannel(QProcess *process, QObject *parent) : QObject(parent), process(process) {
    connect(process, &QProcess::started, this, [this] { if (childFd >= 0) { ::close(childFd); childFd = -1; } });
    connect(process, &QProcess::finished, this, [this] { drain(); close(); });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) close(); });
}
ProgressChannel::~ProgressChannel() { close(); }
void ProgressChannel::close() {
    if (notifier) { delete notifier; notifier = nullptr; }
    if (parentFd >= 0) ::close(parentFd); if (childFd >= 0) ::close(childFd); parentFd = childFd = -1;
    pendingOverwrite.clear();
    pendingFileState.clear();
    pendingExtraction.clear();
}
bool ProgressChannel::prepare(bool enabled, bool automatic) {
    close(); received = false; lastOverwriteId = 0; lastFileStateId = 0; lastExtractionId = 0; process->setChildProcessModifier({});
    static std::atomic<quint64> next{1}; generation = next.fetch_add(1);
    auto environment = process->processEnvironment(); if (environment.isEmpty()) environment = QProcessEnvironment::systemEnvironment(); environment.remove("SEVENZIP_PORT_PROGRESS_FD"); process->setProcessEnvironment(environment);
    if (!enabled) return false;
    int sockets[2]; if (::socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) != 0) return false; parentFd = sockets[0]; childFd = sockets[1];
    int noSignal = 1; ::setsockopt(parentFd, SOL_SOCKET, SO_NOSIGPIPE, &noSignal, sizeof(noSignal));
    for (const auto fd : sockets) if (::fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) { close(); return false; }
    if (::fcntl(parentFd, F_SETFL, O_NONBLOCK) != 0) { close(); return false; }
    int receiveSize = 262144, sendSize = 65536; ::setsockopt(parentFd, SOL_SOCKET, SO_RCVBUF, &receiveSize, sizeof(receiveSize)); ::setsockopt(childFd, SOL_SOCKET, SO_SNDBUF, &sendSize, sizeof(sendSize));
    environment.insert("SEVENZIP_PORT_PROGRESS_FD", "3"); process->setProcessEnvironment(environment);
    const auto reader = parentFd, writer = childFd; auto child = process;
    process->setChildProcessModifier([reader, writer, child] {
        ::close(reader); if (::dup2(writer, 3) < 0) child->failChildProcessModifier("progress channel");
        if (writer != 3) ::close(writer); else ::fcntl(3, F_SETFD, 0);
    });
    if (automatic) { notifier = new QSocketNotifier(parentFd, QSocketNotifier::Read, this); connect(notifier, &QSocketNotifier::activated, this, [this] { drain(); }); }
    return true;
}
bool ProgressChannel::replyOverwrite(const NativeOverwriteRequest &request, OverwriteAnswer answer) {
    if (parentFd < 0 || request.generation != generation || request.id.isEmpty() || request.id != pendingOverwrite) return false;
    QByteArray response;
    switch (answer) {
    case OverwriteAnswer::Yes: response = "yes"; break;
    case OverwriteAnswer::No: response = "no"; break;
    case OverwriteAnswer::YesToAll: response = "yesAll"; break;
    case OverwriteAnswer::NoToAll: response = "noAll"; break;
    case OverwriteAnswer::AutoRename: response = "rename"; break;
    case OverwriteAnswer::Cancel: response = "cancel"; break;
    }
    if (response.isEmpty()) return false;
    const auto packet = QByteArray("7ZOW001 ") + request.id.toLatin1() + ' ' + response + '\n';
    if (::send(parentFd, packet.constData(), size_t(packet.size()), MSG_DONTWAIT) != packet.size()) return false;
    pendingOverwrite.clear(); return true;
}
bool ProgressChannel::replyFileState(const NativeFileStateRequest &request, const FileSnapshot &file) {
    if (parentFd < 0 || request.generation != generation || request.id.isEmpty() || request.id != pendingFileState) return false;
    const int error = file.error.isEmpty() ? 0 : file.errorCode ? file.errorCode : EIO;
    if (error < 0 || error > 4095 || (!error && file.exists && (file.name.isEmpty() || file.name.contains('/') || file.name.contains('\0')))) return false;
    QByteArray encoded = "-", name = "-";
    if (!error && file.exists) {
        // A private same-host/architecture child protocol, not a persistent
        // format. Zero padding and copy kernel fields; native checks sizeof(stat).
        struct stat wire{};
        wire.st_dev=file.stamp.st_dev; wire.st_ino=file.stamp.st_ino; wire.st_mode=file.stamp.st_mode;
        wire.st_nlink=file.stamp.st_nlink; wire.st_uid=file.stamp.st_uid; wire.st_gid=file.stamp.st_gid; wire.st_rdev=file.stamp.st_rdev;
        wire.st_atimespec=file.stamp.st_atimespec; wire.st_mtimespec=file.stamp.st_mtimespec;
        wire.st_ctimespec=file.stamp.st_ctimespec; wire.st_birthtimespec=file.stamp.st_birthtimespec;
        wire.st_size=file.stamp.st_size; wire.st_blocks=file.stamp.st_blocks; wire.st_blksize=file.stamp.st_blksize;
        wire.st_flags=file.stamp.st_flags; wire.st_gen=file.stamp.st_gen;
        encoded = QByteArray(reinterpret_cast<const char *>(&wire), sizeof(wire)).toHex(); name = file.name.toHex();
    }
    const auto packet = QByteArray("7ZFS001 ") + request.id.toLatin1() + ' ' + QByteArray::number(error) + ' ' +
        (file.exists && !error ? "1 " : "0 ") + encoded + ' ' + name + '\n';
    if (::send(parentFd, packet.constData(), size_t(packet.size()), MSG_DONTWAIT) != packet.size()) return false;
    pendingFileState.clear(); return true;
}
bool ProgressChannel::replyExtraction(const NativeExtractionRequest &request, int error) {
    if (parentFd < 0 || request.generation != generation || request.id.isEmpty() || request.id != pendingExtraction || error < 0 || error > 4095) return false;
    const auto packet = QByteArray("7ZER001 ") + request.id.toLatin1() + ' ' + QByteArray::number(error) + '\n';
    if (::send(parentFd, packet.constData(), size_t(packet.size()), MSG_DONTWAIT) != packet.size()) return false;
    pendingExtraction.clear(); return true;
}
void ProgressChannel::drain() {
    if (parentFd < 0) return;
    char buffer[65536];
    for (;;) {
        const auto count = ::recv(parentFd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (count < 0) { if (errno == EINTR) continue; if (errno != EAGAIN && errno != EWOULDBLOCK && notifier) notifier->setEnabled(false); return; }
        if (!count) { if (notifier) notifier->setEnabled(false); return; }
        QJsonParseError error; const auto document = QJsonDocument::fromJson(QByteArray(buffer, int(count)), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject()) continue;
        const auto object = document.object(); if (object.value("version").toInt() != 1) continue;
        if (object.value("mode").toString() == "operationError") {
            const auto code = object.value("code");
            if (!code.isDouble() || code.toDouble() != code.toInt() || code.toInt() < -1 || !object.value("encrypted").isBool() ||
                !object.value("name").isString() || !object.value("archive").isString() || !object.value("message").isString()) continue;
            emit operationError({code.toInt(), object.value("encrypted").toBool(), object.value("name").toString(), object.value("archive").toString(), object.value("message").toString()});
            continue;
        }
        if (object.value("mode").toString() == "extraction") {
            NativeExtractionRequest request{generation, object.value("id").toString(), object.value("action").toString(), object.value("item").toObject()};
            static const QRegularExpression positive("^[1-9][0-9]{0,19}$"); bool valid = false;
            const auto id = request.id.toULongLong(&valid);
            valid &= positive.match(request.id).hasMatch() && object.value("item").isObject() &&
                QStringList{"begin", "mutation", "publish", "finish", "seed"}.contains(request.action);
            if (valid && pendingExtraction.isEmpty() && id > lastExtractionId) {
                lastExtractionId = id; pendingExtraction = request.id; emit extractionRequest(request);
            }
            continue;
        }
        if (object.value("mode").toString() == "fileState") {
            NativeFileStateRequest request{generation, object.value("id").toString(), object.value("path").toString()};
            static const QRegularExpression positive("^[1-9][0-9]{0,19}$"); bool valid = false;
            const auto id = request.id.toULongLong(&valid);
            request.probe = object.value("probe").toBool();
            valid &= positive.match(request.id).hasMatch() && object.value("path").isString() &&
                request.path.startsWith('/') && !request.path.contains(QChar::Null) && object.value("probe").isBool();
            if (valid && pendingFileState.isEmpty() && id > lastFileStateId) {
                lastFileStateId = id; pendingFileState = request.id; emit fileStateRequest(request);
            }
            continue;
        }
        if (object.value("mode").toString() == "overwrite") {
            NativeOverwriteRequest request; request.generation = generation; request.id = object.value("id").toString();
            static const QRegularExpression positive("^[1-9][0-9]{0,19}$"); bool valid = false;
            const auto nativeId = request.id.toULongLong(&valid); valid &= positive.match(request.id).hasMatch();
            auto info = [&](const char *prefix, OverwriteFileInfo &file) {
                const auto name = QString::fromLatin1(prefix);
                const auto path = object.value(name + "Path"); file.path = path.toString();
                valid &= path.isString() && (name != "existing" || !file.path.isEmpty()) && !file.path.contains(QChar::Null);
                const auto directory = object.value(name + "Directory"); valid &= directory.isBool(); file.directory = directory.toBool();
                for (const auto &suffix : {QString("Size"), QString("Time")}) {
                    const auto json = object.value(name + suffix); if (json.isNull()) continue;
                    static const QRegularExpression decimal("^[0-9]{1,20}$"); bool ok = false; const auto number = json.toString().toULongLong(&ok);
                    valid &= json.isString() && decimal.match(json.toString()).hasMatch() && ok;
                    if (suffix == "Size") { file.size = number; file.sizeDefined = ok; }
                    else { file.fileTime = number; file.modified = QDateTime::fromMSecsSinceEpoch(qint64(number / 10000) - 11644473600000LL, QTimeZone::UTC); }
                }
            };
            info("existing", request.conflict.existing); info("incoming", request.conflict.incoming);
            if (valid && pendingOverwrite.isEmpty() && nativeId > lastOverwriteId) {
                lastOverwriteId = nativeId; pendingOverwrite = request.id; emit overwriteRequest(request);
            }
            continue;
        }
        if (object.value("mode").toString() == "benchmark") { emit benchmarkSnapshot(object); continue; }
        ArchiveProgress progress; progress.mode = object.value("mode").toString(); progress.current = object.value("current").toString();
        progress.titleFileName = object.value("title").toString(); progress.status = object.value("status").toString(); progress.directory = object.value("directory").toBool();
        if (!QStringList{"unknown", "compress", "extract", "test", "hash"}.contains(progress.mode)) continue;
        bool valid = (!object.contains("title") || object.value("title").isString()) && (!object.contains("status") || object.value("status").isString()) && (!object.contains("directory") || object.value("directory").isBool());
        auto value = [&object, &valid](const char *key) -> std::optional<quint64> {
            const auto json = object.value(key); if (json.isNull()) return {};
            static const QRegularExpression decimal("^[0-9]{1,20}$"); const auto text = json.toString(); bool ok = false;
            const auto number = text.toULongLong(&ok); if (!json.isString() || !decimal.match(text).hasMatch() || !ok) { valid = false; return {}; } return number;
        };
        progress.total = value("total"); progress.completed = value("completed"); progress.input = value("input"); progress.output = value("output"); progress.files = value("files"); progress.doneFiles = value("doneFiles");
        if (valid) { received = true; emit snapshot(progress); }
    }
}
