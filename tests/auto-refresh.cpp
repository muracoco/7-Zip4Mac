// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "PortStyle.h"
#include "DirectoryScanner.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest>

static QString executable;
class AutoRefreshTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QString root;
    bool write(QString path, QByteArray data) {
        QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    }
    FileList *list(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
    QTreeWidgetItem *item(MainWindow &window, QString name) {
        const auto found=list(window)->findItems(name,Qt::MatchExactly); return found.isEmpty()?nullptr:found.first();
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.filePath("settings"));
    }
    void init() {
        root=temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
        QVERIFY(QDir().mkpath(root));
        for(int n=0;n<150;++n) QVERIFY(write(root+QString("/file%1.txt").arg(n,3,10,QChar('0')), "payload"));
        QSettings().clear(); QSettings().setValue("View/LastPath",root);
        QSettings().setValue("View/AutoRefresh",true);
        FileManagerSettings settings; settings.realIcons=true; settings.language="en"; QVERIFY(settings.save());
    }
    void unchangedNotificationKeepsTheVisibleModel() {
        MainWindow window(executable); window.show();
        QTRY_VERIFY(!window.operationBusy()); QTRY_VERIFY(!list(window)->nativeIconsBusy());
        auto files=list(window); auto selected=item(window,"file080.txt"); QVERIFY(selected);
        files->setCurrentItem(selected); files->setItemSelected(selected,true);
        files->verticalScrollBar()->setValue(40); const int scroll=files->verticalScrollBar()->value();
        const QPersistentModelIndex identity=files->indexFromItem(selected);
        QSignalSpy reset(files->model(),&QAbstractItemModel::modelReset);
        QSignalSpy loaded(&window,&MainWindow::directoryLoaded);
        QSignalSpy scan(window.findChild<DirectoryScanner *>(),&DirectoryScanner::finished);
        QSignalSpy actionChanges(window.findChild<QAction *>("optionsAction"),&QAction::changed);
        auto debounce=window.findChild<QTimer *>("archiveRefreshDebounce"); QVERIFY(debounce);
        debounce->start(1); QTRY_VERIFY(!scan.isEmpty()); QTest::qWait(100);
        QCOMPARE(reset.size(),0); QCOMPARE(loaded.size(),0); QCOMPARE(actionChanges.size(),0);
        QVERIFY(identity.isValid()); QCOMPARE(files->currentItem(),selected); QVERIFY(files->operatedItems().contains(selected));
        QCOMPARE(files->verticalScrollBar()->value(),scroll);
        for(int n=0;n<4;++n) { const auto before=scan.size(); debounce->start(1); QTRY_VERIFY(scan.size()>before); }
        QTest::qWait(100); QCOMPARE(reset.size(),0); QCOMPARE(loaded.size(),0);
        qInfo()<<"Five unchanged notifications scanned without model resets, busy-state changes or selection/scroll loss";
    }
    void nativeChangesAndAutoRefreshPreference() {
        MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy());
        auto files=list(window); auto selected=item(window,"file080.txt"); QVERIFY(selected);
        files->setCurrentItem(selected); files->setItemSelected(selected,true);
        const auto added=root+"/日本語 space.txt"; QVERIFY(write(added,"new"));
        QTRY_VERIFY_WITH_TIMEOUT(item(window,"日本語 space.txt"),10000); QTRY_VERIFY(!window.operationBusy());
        QCOMPARE(files->currentItem()->text(0),QString("file080.txt"));
        QVERIFY(write(added,"a larger replacement payload"));
        QTRY_COMPARE_WITH_TIMEOUT(item(window,"日本語 space.txt")->data(1,Qt::UserRole+3).toULongLong(),quint64(28),10000);
        QVERIFY(QFile::rename(added,root+"/renamed.txt"));
        QTRY_VERIFY_WITH_TIMEOUT(item(window,"renamed.txt")&&!item(window,"日本語 space.txt"),10000);
        QVERIFY(QFile::remove(root+"/renamed.txt")); QTRY_VERIFY_WITH_TIMEOUT(!item(window,"renamed.txt"),10000);
        window.findChild<QAction *>("autoRefreshAction")->trigger(); QVERIFY(!QSettings().value("View/AutoRefresh").toBool());
        QVERIFY(write(root+"/manual.txt","manual")); QTest::qWait(800); QVERIFY(!item(window,"manual.txt"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY(item(window,"manual.txt"));
        qInfo()<<"Native create/overwrite/rename/delete reflected; Auto Refresh off and manual Refresh honored";
    }
};
int main(int argc,char **argv) {
    if(argc<2) return 1; executable=QString::fromLocal8Bit(argv[1]);
    if(!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta);
    QApplication app(argc,argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests");
    app.setApplicationName("AutoRefresh"); app.setQuitOnLastWindowClosed(false);
    AutoRefreshTests test; QList<char *> args{argv[0]}; for(int n=2;n<argc;++n) args<<argv[n]; args<<nullptr;
    return QTest::qExec(&test,args.size()-1,args.data());
}
#include "auto-refresh.moc"
