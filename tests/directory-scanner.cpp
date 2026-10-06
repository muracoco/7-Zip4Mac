// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "DirectoryScanner.h"
#include "MainWindow.h"
#include "OpenWith.h"
#include "PortStyle.h"
#include "PanelSort.h"
#include "progress-dialog-driver.h"
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMessageBox>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <sys/stat.h>
#include <unistd.h>

static QString executable;
static bool write(QString path, QByteArray content) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}
class DirectoryTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
    QString small, many, archive;
    FileList *tree(MainWindow &window) {
        for (auto list : window.findChildren<FileList *>("fileList")) {
            auto parent = list->parentWidget();
            while (parent && !qobject_cast<MainWindow *>(parent)) parent = parent->parentWidget();
            if (parent == &window) return list;
        }
        return nullptr;
    }
    DirectorySnapshot scan(QString path, bool flat) {
        DirectoryScanner scanner; QSignalSpy done(&scanner, &DirectoryScanner::finished); scanner.start(path, flat);
        if (done.isEmpty() && !done.wait(10000)) return {};
        return qvariant_cast<DirectorySnapshot>(done.first()[0]);
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        small = temporary.filePath("small 日本語"), many = temporary.filePath("many entries"), archive = temporary.filePath("sample.7z");
        QVERIFY(write(small + "/日本語 space.txt", "payload")); QVERIFY(QDir().mkpath(many));
        for (int n = 0; n < 30000; ++n) QVERIFY(::link(QFile::encodeName(small + "/日本語 space.txt").constData(), QFile::encodeName(many + '/' + QString::number(n)).constData()) == 0);
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished);
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = archive; request.workingDirectory = small; request.files = {"日本語 space.txt"}; request.level = 1;
        backend.start(request); if (done.isEmpty()) QVERIFY(done.wait(10000)); QVERIFY(qvariant_cast<ArchiveResult>(done.first()[0]).success);
        qInfo() << "Owned directory fixtures:" << temporary.path();
    }
    void init() {
        resultDialogs.clear();
        QSettings().clear(); QSettings().setValue("View/LastPath", small); QSettings().setValue("View/Panel2Path", small);
        FileManagerSettings settings; settings.realIcons = false; QVERIFY(settings.save());
    }
    void metadataAndFlatDoesNotFollowChildLinks() {
        const auto root = temporary.filePath("metadata"); QVERIFY(write(root + "/ASCII.txt", "123")); QVERIFY(write(root + "/日本語 folder/space name.bin", "1234567"));
        QVERIFY(write(root + "/.hidden", "hidden")); QVERIFY(QDir().mkpath(root + "/empty"));
        QVERIFY(::symlink("日本語 folder", QFile::encodeName(root + "/linked directory").constData()) == 0);
        QVERIFY(::symlink(".", QFile::encodeName(root + "/loop").constData()) == 0);
        QVERIFY(::symlink("missing", QFile::encodeName(root + "/dangling").constData()) == 0);
        QVERIFY(::mkfifo(QFile::encodeName(root + "/pipe").constData(), 0600) == 0);
        const auto normal = scan(root, false), flat = scan(root, true); QVERIFY(normal.error.isEmpty()); QVERIFY(flat.error.isEmpty());
        QCOMPARE(normal.path, root); QCOMPARE(normal.entries.size(), 8); QCOMPARE(flat.entries.size(), 9);
        bool nested = false, link = false, hidden = false;
        for (const auto &entry : flat.entries) {
            QVERIFY(!entry.path.contains("//")); if (!entry.link) QVERIFY(entry.modified.isValid()); QCOMPARE(entry.modifiedFraction.size(), 9);
            if (entry.name == "space name.bin") { QCOMPARE(entry.size, 7); QCOMPARE(entry.prefix, QString("日本語 folder")); QCOMPARE(entry.suffix, QString("BIN")); nested = true; }
            if (entry.name == "linked directory") { QVERIFY(entry.link); QVERIFY(entry.directory); link = true; }
            if (entry.name == ".hidden") hidden = true;
        }
        QVERIFY(nested && link && hidden);
        const auto alias = temporary.filePath("root alias"); QVERIFY(::symlink(QFile::encodeName(root).constData(), QFile::encodeName(alias).constData()) == 0);
        const auto aliasResult = scan(alias, true); QVERIFY(aliasResult.error.isEmpty()); QCOMPARE(aliasResult.path, alias); QCOMPARE(aliasResult.entries.size(), 9);
    }
    void scannerSupersedesAndCanBeDestroyed() {
        DirectoryScanner scanner; QSignalSpy done(&scanner, &DirectoryScanner::finished);
        for (int n = 0; n < 8; ++n) scanner.start(many, true);
        const auto latest = scanner.start(small, false); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000);
        const auto result = qvariant_cast<DirectorySnapshot>(done.first()[0]); QCOMPARE(result.generation, latest); QCOMPARE(result.path, small); QCOMPARE(result.entries.size(), 1);
        QTest::qWait(80); QCOMPARE(done.size(), 1);
        QElapsedTimer duration; duration.start(); { DirectoryScanner abandoned; abandoned.start(many, true); }
        QVERIFY(duration.elapsed() < 250);
    }
    void completeLargeViewKeepsHeartbeatAndHeader_data() { QTest::addColumn<bool>("flat"); QTest::newRow("normal") << false; QTest::newRow("flat") << true; }
    void completeLargeViewKeepsHeartbeatAndHeader() {
        QFETCH(bool, flat); QSettings().setValue("View/Flat", flat);
        MainWindow window(executable); QTRY_VERIFY(!window.operationBusy()); auto list = tree(window); QVERIFY(list);
        list->setColumnWidth(0, 301); list->header()->moveSection(list->header()->visualIndex(1), 2);
        QSignalSpy loaded(&window, &MainWindow::directoryLoaded); int ticks = 0; qint64 maximumGap = 0; QElapsedTimer clock; clock.start(); qint64 last = 0;
        QTimer heartbeat; heartbeat.setInterval(5); connect(&heartbeat, &QTimer::timeout, &window, [&] { const auto now = clock.elapsed(); maximumGap = qMax(maximumGap, now - last); last = now; ++ticks; }); heartbeat.start();
        window.openPath(many); QVERIFY(window.operationBusy()); QCOMPARE(window.currentDirectory(), small); QCOMPARE(list->topLevelItemCount(), 1);
        QVERIFY(window.findChild<QAction *>("refreshAction")->isEnabled()); QVERIFY(window.findChild<QLineEdit *>("currentPath")->isEnabled()); QVERIFY(!window.findChild<QAction *>("addAction")->isEnabled());
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); maximumGap = qMax(maximumGap, clock.elapsed() - last); heartbeat.stop();
        QCOMPARE(loaded.size(), 1); QCOMPARE(window.currentDirectory(), many); QCOMPARE(list->topLevelItemCount(), 30000); QVERIFY(ticks > 5); QVERIFY2(maximumGap < 1000, qPrintable(QString::number(maximumGap)));
        QCOMPARE(list->columnWidth(0), 301); QCOMPARE(list->header()->visualIndex(1), 2); QVERIFY(list->isEnabled()); QCOMPARE(QSettings().value("View/LastPath").toString(), many);
        QCOMPARE(propertyColumn(list,OfficialSort::kpidPrefix) >= 0, flat);
        if (flat) QVERIFY(!list->isColumnHidden(propertyColumn(list,OfficialSort::kpidPrefix)));
        qInfo() << "Flat" << flat << "30,000 rows: elapsed ms" << clock.elapsed() << "heartbeat ticks" << ticks << "maximum event-loop gap ms" << maximumGap;
    }
    void cancelDuringRowPreparationAndNavigate() {
        MainWindow window(executable); QTRY_VERIFY(!window.operationBusy()); auto list = tree(window); auto scanner = window.findChild<DirectoryScanner *>(); QVERIFY(scanner);
        bool ready = false, cancelled = false; connect(scanner, &DirectoryScanner::finished, &window, [&](DirectorySnapshot snapshot) { if (snapshot.path == many) ready = true; });
        QSignalSpy loaded(&window, &MainWindow::directoryLoaded); QTimer interrupt; interrupt.setInterval(5);
        connect(&interrupt, &QTimer::timeout, &window, [&] {
            if (!ready || !window.operationBusy()) return;
            cancelled = true; interrupt.stop(); QCOMPARE(window.currentDirectory(), small); QCOMPARE(list->topLevelItemCount(), 1);
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier); QApplication::sendEvent(window.findChild<QLineEdit *>("currentPath"), &escape);
        }); interrupt.start(); window.openPath(many);
        QTRY_VERIFY_WITH_TIMEOUT(cancelled, 10000); QVERIFY(!window.operationBusy()); QCOMPARE(loaded.size(), 0); QCOMPARE(window.currentDirectory(), small); QCOMPARE(QSettings().value("View/LastPath").toString(), small);
        QTest::qWait(80); QCOMPARE(list->topLevelItemCount(), 1); window.openPath(many); window.openPath(small); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(loaded.size(), 1); QCOMPARE(window.currentDirectory(), small); QCOMPARE(list->topLevelItemCount(), 1);
    }
    void refreshOptionsAndManagerRestoreSelection() {
        MainWindow window(executable); QTRY_VERIFY(!window.operationBusy()); auto list = tree(window); list->setCurrentItem(list->topLevelItem(0)); list->topLevelItem(0)->setSelected(true);
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(list->selectedItems().size(), 1);
        QSignalSpy loaded(&window, &MainWindow::directoryLoaded); bool applied = false;
        QTimer::singleShot(30, &window, [&] {
            auto dialog = window.findChild<OptionsDialog *>("optionsDialog"); QVERIFY(dialog);
            auto dots = dialog->findChild<QCheckBox *>("showDots"); auto buttons = dialog->findChild<QDialogButtonBox *>("optionsButtons");
            dots->setChecked(true); buttons->button(QDialogButtonBox::Apply)->click(); dots->setChecked(false); buttons->button(QDialogButtonBox::Apply)->click(); applied = true; dialog->reject();
        }); window.findChild<QAction *>("optionsAction")->trigger(); QVERIFY(applied); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(loaded.size(), 1); QCOMPARE(list->selectedItems().size(), 1); QCOMPARE(list->topLevelItemCount(), 1);
        list->clearSelection(); window.executeOpenWith("manager", {small + "/日本語 space.txt"}); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(list->selectedItems().size(), 1); QCOMPARE(list->selectedItems().first()->text(0), QString("日本語 space.txt")); window.close();
    }
    void failurePreservesPreviousViewAndRecovers() {
        const auto denied = temporary.filePath("permission error"); QVERIFY(QDir().mkpath(denied)); QVERIFY(::chmod(QFile::encodeName(denied).constData(), 0000) == 0);
        const auto permissions = qScopeGuard([&] { ::chmod(QFile::encodeName(denied).constData(), 0700); });
        MainWindow window(executable); QTRY_VERIFY(!window.operationBusy()); auto list = tree(window); list->topLevelItem(0)->setSelected(true);
        bool completionReturned = false; connect(window.findChild<DirectoryScanner *>(), &DirectoryScanner::finished, &window, [&](DirectorySnapshot snapshot) { if (!snapshot.error.isEmpty()) completionReturned = true; });
        QString error; QTimer dialogs; dialogs.setInterval(5); connect(&dialogs, &QTimer::timeout, &window, [&] { if (!completionReturned) return; for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { error = box->text(); box->accept(); } }); dialogs.start();
        QSignalSpy loaded(&window, &MainWindow::directoryLoaded); window.openPath(denied); QTRY_VERIFY(completionReturned); QTRY_VERIFY(!window.operationBusy()); QTRY_COMPARE(loaded.size(), 1);
        QVERIFY(error.contains(denied)); QVERIFY(error.contains("Permission denied")); QVERIFY(!loaded.first()[1].toBool()); QCOMPARE(window.currentDirectory(), small); QCOMPARE(QSettings().value("View/LastPath").toString(), small); QCOMPARE(list->selectedItems().size(), 1);
        QVERIFY(::chmod(QFile::encodeName(denied).constData(), 0700) == 0); window.openPath(denied); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(list->topLevelItemCount(), 0); QCOMPARE(window.currentDirectory(), denied);
        window.openPath(small); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(list->topLevelItemCount(), 1);
    }
    void archiveExitRefreshAndConsecutiveUp() {
        MainWindow window(executable); window.openPath(archive); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(window.currentArchivePath(), archive);
        window.openPath(many); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000);
        QVERIFY(window.currentArchivePath().isEmpty()); QCOMPARE(window.currentDirectory(), many); QTest::qWait(80); QCOMPARE(window.currentDirectory(), many);
        window.openPath(archive); QTRY_VERIFY(!window.operationBusy()); window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(tree(window)->selectedItems().size(), 1); QCOMPARE(tree(window)->selectedItems().first()->text(0), QString("sample.7z"));
        window.openPath(archive); QTRY_VERIFY(!window.operationBusy()); window.openPath(many); window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(window.currentDirectory(), temporary.path());
    }
    void failedStartupCommandRestoresFilesystem() {
        const auto broken = temporary.filePath("not an archive.7z"); QVERIFY(write(broken, "invalid archive"));
        MainWindow window(executable); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished);
        window.executeOpenWith("test", {broken}); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000); QVERIFY(!qvariant_cast<ArchiveResult>(done.first()[0]).success);
        QCOMPARE(resultDialogs.notices.size(),1); QVERIFY(resultDialogs.notices.first().error); QVERIFY(resultDialogs.notices.first().text.contains(broken));
        QTRY_VERIFY(!window.operationBusy()); QCOMPARE(window.currentDirectory(), small); QCOMPARE(tree(window)->topLevelItemCount(), 1); QVERIFY(tree(window)->isEnabled());
    }
    void openWithMenuSupersedesHiddenStartupScans() {
        QSettings().setValue("View/LastPath", many); QSettings().setValue("View/Panel2Path", many); QSettings().setValue("View/TwoPanels", true);
        MainWindow window(executable); OpenWithController controller(&window); QSignalSpy loaded(&window, &MainWindow::directoryLoaded);
        QVERIFY(window.operationBusy()); controller.enqueue({small + "/日本語 space.txt"});
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentMenu(), 1000); QVERIFY(!window.isVisible()); QVERIFY(!window.operationBusy()); QCOMPARE(loaded.size(), 0);
        auto manager = controller.currentMenu()->findChild<QPushButton *>("openWith_manager"); QVERIFY(manager); manager->click();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); QCOMPARE(window.currentDirectory(), small); QCOMPARE(tree(window)->selectedItems().size(), 1);
        auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second); QCOMPARE(second->currentDirectory(), many); QCOMPARE(tree(*second)->topLevelItemCount(), 30000); window.close();
    }
    void twoPanelRestoreRemovalAndClose() {
        QSettings().setValue("View/TwoPanels", true); MainWindow window(executable); window.show(); auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second);
        QTRY_VERIFY(!window.operationBusy()); window.openPath(many); second->openPath(small); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); QCOMPARE(window.currentDirectory(), many); QCOMPARE(second->currentDirectory(), small);
        second->openPath(many); QVERIFY(window.operationBusy()); QVERIFY(window.findChild<QAction *>("twoPanelsAction")->isEnabled()); QPointer<MainWindow> deleted(second); window.findChild<QAction *>("twoPanelsAction")->trigger(); QTRY_VERIFY(!deleted); QVERIFY(!window.operationBusy());
        // The parent is already visible: setWindowFlags in the embedded
        // constructor can deliver Show before its controls are initialized.
        window.findChild<QAction *>("twoPanelsAction")->trigger(); QVERIFY(window.findChild<MainWindow *>("secondPanel")); QTRY_VERIFY(!window.operationBusy());
        window.findChild<QAction *>("twoPanelsAction")->trigger(); QTRY_VERIFY(!window.operationBusy());
        window.openPath(many); QVERIFY(window.operationBusy()); QVERIFY(window.close()); QVERIFY(!window.operationBusy()); QTest::qWait(80); QVERIFY(!window.isVisible());
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("DirectoryScanner"); app.setQuitOnLastWindowClosed(false);
    DirectoryTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr;
    return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "directory-scanner.moc"
