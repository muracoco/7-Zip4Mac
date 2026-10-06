// SPDX-License-Identifier: LGPL-3.0-or-later
// Disposable LaunchServices witness; never opens or terminates a user editor.
#include <QApplication>
#include <QFileOpenEvent>
#include <QFile>
#include <QTimer>
#include <unistd.h>

class Application : public QApplication {
public:
    using QApplication::QApplication;
    QString gate;
    bool event(QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            const auto path = static_cast<QFileOpenEvent *>(event)->file();
            QFile edited(path), record(gate + ".path"), pid(gate + ".pid");
            if (!edited.open(QIODevice::WriteOnly) || edited.write("edited 日本語 file bytes\n") < 0 ||
                !record.open(QIODevice::WriteOnly) || record.write(path.toUtf8()) < 0 ||
                !pid.open(QIODevice::WriteOnly) || pid.write(QByteArray::number(::getpid())) < 0) exit(2);
            return true;
        }
        return QApplication::event(event);
    }
};
int main(int argc, char **argv) {
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    Application app(argc, argv); app.setQuitOnLastWindowClosed(false);
    if (app.arguments().size() < 2) return 3; app.gate = app.arguments()[1];
    QTimer wait; wait.setInterval(50); QObject::connect(&wait, &QTimer::timeout, &app, [&] { if (QFile::exists(app.gate + ".release")) app.quit(); }); wait.start();
    QTimer::singleShot(20000, &app, [&] { app.exit(4); }); return app.exec();
}
