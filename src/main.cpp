#include "MainWindow.h"
#include "OpenWith.h"
#include "PortStyle.h"
#include <QApplication>
#include <QFileOpenEvent>
#include <QStyleFactory>
#include <QTimer>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <cstdio>

class PortApplication : public QApplication {
public:
    using QApplication::QApplication;
    QStringList earlyOpens;
    bool event(QEvent *event) override {
        if (event->type() == QEvent::FileOpen) {
            auto open = static_cast<QFileOpenEvent *>(event);
            if (open->url().isLocalFile()) earlyOpens << open->url().toLocalFile();
            return true;
        }
        return QApplication::event(event);
    }
};
int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    PortApplication app(argc, argv);
    app.setApplicationName("7-Zip Mac Port"); app.setOrganizationName("SevenZipMacPort"); app.setQuitOnLastWindowClosed(false);
    const auto profile = qEnvironmentVariable("SEVENZIP_PORT_SETTINGS_DIR");
    if (!profile.isEmpty()) {
        if (!QDir::isAbsolutePath(profile) || profile.contains(QChar::Null) || !QDir().mkpath(profile) || !QFileInfo(profile).isWritable()) {
            std::fputs("Cannot initialize the explicitly requested settings directory.\n", stderr); return 2;
        }
        // Native CFPreferences does not reliably honor redirected HOME hints.
        // Integration runs can explicitly opt into a private INI profile.
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, profile);
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, profile);
    }
    applyPortAppearance(app);
    MainWindow window(QCoreApplication::applicationDirPath() + "/7zz"); OpenWithController launcher(&window, true);
    auto args = app.arguments().mid(1); const bool menuMode = args.removeAll("--open-with") > 0; args.removeAll("--file-manager");
    for (int n = args.size() - 1; n >= 0; --n) if (args[n].startsWith("-psn_")) args.removeAt(n);
    launcher.enqueue(app.earlyOpens);
    if (menuMode) launcher.enqueue(args);
    else if (!args.isEmpty()) { window.show(); QTimer::singleShot(0, &window, [&window, args] { window.openPath(args.first()); }); }
    QTimer::singleShot(250, &window, [&] { if (!launcher.receivedRequests() && !window.isVisible()) window.show(); });
    return app.exec();
}
