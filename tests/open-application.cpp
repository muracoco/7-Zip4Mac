// SPDX-License-Identifier: LGPL-3.0-or-later
// Owned handler for a fresh random extension. It never handles user formats.
#include <QApplication>
#include <QFileOpenEvent>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <unistd.h>

static QString resource(const QString &name) {
    QFile file(QCoreApplication::applicationDirPath() + "/../Resources/" + name);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()).trimmed() : QString();
}
class Application : public QApplication {
public:
    using QApplication::QApplication;
    QString gate;
    bool event(QEvent *event) override {
        if (event->type() != QEvent::FileOpen) return QApplication::event(event);
        const auto path = static_cast<QFileOpenEvent *>(event)->file();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) exit(2);
        const auto bytes = file.readAll(); file.close();
        if (QFile::exists(gate + ".edit")) {
            if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write("edited:" + bytes) != bytes.size() + 7) exit(3);
            file.close();
        }
        QFile records(gate + ".requests");
        const QJsonObject record{{"path", path}, {"data", QString::fromLatin1(bytes.toBase64())}, {"pid", qint64(::getpid())}};
        const auto text = QJsonDocument(record).toJson(QJsonDocument::Compact) + '\n';
        if (!records.open(QIODevice::WriteOnly | QIODevice::Append) || records.write(text) != text.size()) exit(4);
        return true;
    }
};
int main(int argc, char **argv) {
    // Read the plugin path before QApplication loads the Cocoa plugin.
    const auto executable = QString::fromLocal8Bit(argv[0]);
    QFile plugin(QFileInfo(executable).absolutePath() + "/../Resources/plugins");
    if (plugin.open(QIODevice::ReadOnly)) QCoreApplication::setLibraryPaths({QString::fromUtf8(plugin.readAll()).trimmed()});
    Application app(argc, argv); app.setQuitOnLastWindowClosed(false); app.gate = resource("gate");
    if (app.gate.isEmpty()) return 5;
    QTimer wait; wait.setInterval(25); QObject::connect(&wait, &QTimer::timeout, &app, [&] { if (QFile::exists(app.gate + ".release")) app.quit(); }); wait.start();
    QTimer::singleShot(25000, &app, [&] { app.exit(6); }); return app.exec();
}
