// SPDX-License-Identifier: LGPL-3.0-or-later
// Disposable independent process used only by the editor integration tests.
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QThread>

static bool write(const QString &path, const QByteArray &bytes) {
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() < 3) return 2;
    const auto mode = args[1], gate = args[2];
    if (mode == "server") {
        if (!write(gate + ".ready", "ready")) return 3;
        while (!QFileInfo::exists(gate + ".release")) {
            if (QFileInfo::exists(gate + ".commit") && QFileInfo::exists(gate + ".request")) {
                QFile request(gate + ".request");
                if (!request.open(QIODevice::ReadOnly)) return 4;
                const auto path = QString::fromUtf8(request.readAll());
                if (!write(path, "handoff edited 日本語 bytes\n") || !write(gate + ".edited", "edited")) return 5;
            }
            QThread::msleep(10);
        }
        return 0;
    }
    if (args.size() != 4) return 2;
    if (mode == "long") QThread::msleep(2300);
    if (mode == "changed" && !write(args[3], "immediate client edit\n")) return 6;
    return write(gate + ".request", args[3].toUtf8()) ? 0 : 7;
}
