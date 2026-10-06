// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "FolderStatistics.h"
#include "MainWindow.h"
#include "PanelSort.h"
#include "PortStyle.h"
#include "progress-dialog-driver.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QHeaderView>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTest>
#include <sys/stat.h>
#include <unistd.h>

static QString executable;
static bool write(QString path, QByteArray bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
static FileList *ownedTree(MainWindow &window) {
    for (auto list : window.findChildren<FileList *>("fileList")) {
        auto parent = list->parentWidget(); while (parent && !qobject_cast<MainWindow *>(parent)) parent = parent->parentWidget();
        if (parent == &window) return list;
    }
    return nullptr;
}
static QTreeWidgetItem *item(FileList *tree, QString name) {
    const auto found = tree->findItems(name, Qt::MatchExactly); return found.size() == 1 ? found.first() : nullptr;
}

class StatisticsTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
    QString many;
    FolderStatisticsResult calculate(QStringList paths) {
        FolderStatistics job; QSignalSpy done(&job, &FolderStatistics::finished); job.start(paths);
        if (done.isEmpty() && !done.wait(10000)) return {};
        return qvariant_cast<FolderStatisticsResult>(done.first()[0]);
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        many = temporary.filePath("many entries"); QVERIFY(QDir().mkpath(many));
        const auto sample = temporary.filePath("hard-link sample"); QVERIFY(write(sample, "12345"));
        for (int n = 0; n < 30000; ++n) QVERIFY(::link(QFile::encodeName(sample).constData(), QFile::encodeName(many + '/' + QString::number(n)).constData()) == 0);
        qInfo() << "Owned folder-statistics fixtures:" << temporary.path();
    }
    void init() {
        resultDialogs.clear();
        QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/Panel2Path", temporary.path());
        FileManagerSettings settings; QVERIFY(settings.save());
    }
    void countsUnicodeHiddenLinksAndSparseFile() {
        const auto root = temporary.filePath("counts 日本語"); QVERIFY(write(root + "/a.txt", "12345"));
        QVERIFY(write(root + "/日本語 space/inside.bin", "1234567")); QVERIFY(QDir().mkpath(root + "/日本語 space/empty"));
        QVERIFY(write(root + "/.hidden/hidden file", "123"));
        const QList<QByteArray> targets{"a.txt", QString("日本語 space").toUtf8(), "missing", "."};
        quint64 size = 15;
        for (int n = 0; n < targets.size(); ++n) { QVERIFY(::symlink(targets[n].constData(), QFile::encodeName(root + "/link" + QString::number(n)).constData()) == 0); size += targets[n].size(); }
        QVERIFY(::link(QFile::encodeName(root + "/a.txt").constData(), QFile::encodeName(root + "/hard link").constData()) == 0); size += 5;
        QVERIFY(::mkfifo(QFile::encodeName(root + "/pipe").constData(), 0600) == 0);
        QFile sparse(root + "/32 MiB.bin"); QVERIFY(sparse.open(QIODevice::WriteOnly)); QVERIFY(sparse.resize(32 << 20)); sparse.close(); size += 32 << 20;
        const auto result = calculate({root}); QVERIFY(!result.cancelled); QCOMPARE(result.items.size(), 1);
        const auto stat = result.items.first(); QVERIFY2(stat.complete, qPrintable(stat.error)); QCOMPARE(stat.path, root);
        QCOMPARE(stat.size, size); QCOMPARE(stat.folders, 3); QCOMPARE(stat.files, 10);
        QVERIFY(QFileInfo::exists(root + "/a.txt")); QCOMPARE(QFileInfo(root + "/32 MiB.bin").size(), 32 << 20);
    }
    void errorsAndCompletedSibling() {
        const auto root = temporary.filePath("errors"); QVERIFY(QDir().mkpath(root + "/empty"));
        QVERIFY(write(root + "/file", "not a folder")); QVERIFY(QDir().mkpath(root + "/private"));
        QVERIFY(::chmod(QFile::encodeName(root + "/private").constData(), 0000) == 0);
        const auto permissions = qScopeGuard([&] { ::chmod(QFile::encodeName(root + "/private").constData(), 0700); });
        QVERIFY(::symlink("empty", QFile::encodeName(root + "/symbolic folder").constData()) == 0);
        const auto result = calculate({root + "/empty", root + "/missing", root + "/file", root + "/private", root + "/symbolic folder"});
        QCOMPARE(result.items.size(), 5); QVERIFY(result.items[0].complete); QCOMPARE(result.items[0].size, 0); QCOMPARE(result.items[0].files, 0); QCOMPARE(result.items[0].folders, 0);
        for (int n = 1; n < result.items.size(); ++n) { QVERIFY(!result.items[n].complete); QVERIFY(result.items[n].error.contains(result.items[n].path)); QVERIFY(result.items[n].error.contains("Cannot calculate")); }
    }
    void pauseResumeCancelAndReuse_data() { QTest::addColumn<bool>("cancel"); QTest::newRow("resume") << false; QTest::newRow("cancel") << true; }
    void pauseResumeCancelAndReuse() {
        QFETCH(bool, cancel); FolderStatistics job; QSignalSpy done(&job, &FolderStatistics::finished); bool paused = false;
        connect(&job, &FolderStatistics::progress, this, [&](quint64, QString) { if (!paused && job.busy()) paused = job.setPaused(true); });
        int ticks = 0; QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, this, [&] { ++ticks; }); heartbeat.start();
        job.start({many}); QTRY_VERIFY_WITH_TIMEOUT(paused, 5000); QTest::qWait(120); QVERIFY(done.isEmpty()); QVERIFY(ticks >= 5);
        disconnect(&job, &FolderStatistics::progress, this, nullptr);
        if (cancel) job.cancel(); else QVERIFY(job.setPaused(false));
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000); auto result = qvariant_cast<FolderStatisticsResult>(done.first()[0]); QCOMPARE(result.cancelled, cancel);
        if (cancel) QVERIFY(result.items.isEmpty()); else { QCOMPARE(result.items.size(), 1); QVERIFY(result.items.first().complete); QCOMPARE(result.items.first().files, 30000); QCOMPARE(result.items.first().size, 150000); }
        const auto empty = temporary.filePath("reuse empty"); QVERIFY(QDir().mkpath(empty));
        QVERIFY(!job.busy()); done.clear(); job.start({empty}); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000); result = qvariant_cast<FolderStatisticsResult>(done.first()[0]); QVERIFY(result.items.first().complete);
    }
    void selectedFoldersTakePrecedence_data() {
        QTest::addColumn<bool>("flat"); QTest::addColumn<int>("mode");
        QTest::newRow("details") << false << 3; QTest::newRow("icons") << false << 0; QTest::newRow("flat") << true << 3;
    }
    void selectedFoldersTakePrecedence() {
        QFETCH(bool, flat); QFETCH(int, mode); const auto root = temporary.filePath("gui " + QString::number(mode) + QString::number(flat));
        QVERIFY(write(root + "/first folder/日本語.txt", "12345678901")); QVERIFY(QDir().mkpath(root + "/first folder/empty"));
        for (int n = 0; n < 9; ++n) QVERIFY(write(root + "/first folder/empty" + QString::number(n), {}));
        QVERIFY(write(root + "/second folder/a", "12345")); QVERIFY(write(root + "/second folder/b", "1234567")); QVERIFY(write(root + "/focused.txt", "focus"));
        const auto capture = root + "/viewer capture", helper = root + "/viewer.sh"; QVERIFY(write(helper, "#!/bin/sh\nprintf started > \"$1\"\n"));
        auto settings = FileManagerSettings::load(); settings.viewer = "/bin/sh \"" + helper + "\" \"" + capture + "\""; QVERIFY(settings.save());
        QSettings().setValue("View/Flat", flat); QSettings().setValue("View/Mode", mode); QSettings().setValue("View/LastPath", root);
        MainWindow window(executable); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto tree = ownedTree(window);
        auto first = item(tree, "first folder"), second = item(tree, "second folder"), focused = item(tree, "focused.txt"); QVERIFY(first && second && focused);
        tree->setCurrentItem(focused); first->setSelected(true); second->setSelected(true); focused->setSelected(true);
        auto view = window.findChild<QAction *>("viewAction"), edit = window.findChild<QAction *>("editAction"); QVERIFY(view->isEnabled()); QVERIFY(edit->isEnabled());
        auto job = window.findChild<FolderStatistics *>(); QVERIFY(job); QSignalSpy done(job, &FolderStatistics::finished); view->trigger();
        if (done.isEmpty()) QVERIFY(done.wait(10000)); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 5000);
        QCOMPARE(first->text(1), QString("11")); QCOMPARE(first->text(propertyColumn(tree,OfficialSort::kpidNumSubDirs)), QString("1")); QCOMPARE(first->text(propertyColumn(tree,OfficialSort::kpidNumSubFiles)), QString("10"));
        QCOMPARE(second->text(1), QString("12")); QCOMPARE(second->text(propertyColumn(tree,OfficialSort::kpidNumSubDirs)), QString("0")); QCOMPARE(second->text(propertyColumn(tree,OfficialSort::kpidNumSubFiles)), QString("2"));
        QVERIFY(!QFileInfo::exists(capture)); QCOMPARE(tree->selectedItems().size(), 3); QCOMPARE(tree->currentItem(), focused);
        tree->sortItems(1, Qt::AscendingOrder); QVERIFY(tree->indexOfTopLevelItem(first) < tree->indexOfTopLevelItem(second));
        tree->sortItems(propertyColumn(tree,OfficialSort::kpidNumSubFiles), Qt::AscendingOrder); QVERIFY(tree->indexOfTopLevelItem(second) < tree->indexOfTopLevelItem(first));
        tree->clearSelection(); tree->setCurrentItem(first); tree->clearSelection(); QVERIFY(!view->isEnabled()); QVERIFY(!edit->isEnabled()); done.clear();
        // Original EditItem calculates operated folders only. An unmarked,
        // unselected focused directory is not a fallback statistics target.
        QTest::keyClick(tree, Qt::Key_F3); QTest::qWait(30); QVERIFY(done.isEmpty()); QVERIFY(!window.operationBusy()); QCOMPARE(first->text(1), QString("11"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); first = item(tree, "first folder"); QVERIFY(first); QVERIFY(first->text(1).isEmpty()); QVERIFY(first->text(propertyColumn(tree,OfficialSort::kpidNumSubDirs)).isEmpty());
    }
    void guiErrorDoesNotDisplayPartialCounts() {
        const auto root = temporary.filePath("gui errors"); QVERIFY(write(root + "/folder/visible.txt", "1234567"));
        QVERIFY(QDir().mkpath(root + "/folder/private")); QVERIFY(::chmod(QFile::encodeName(root + "/folder/private").constData(), 0000) == 0);
        const auto permissions = qScopeGuard([&] { ::chmod(QFile::encodeName(root + "/folder/private").constData(), 0700); });
        QSettings().setValue("View/LastPath", root); MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto tree = ownedTree(window);
        auto folder = item(tree, "folder"); QVERIFY(folder); tree->setCurrentItem(folder); folder->setSelected(true);
        auto job = window.findChild<FolderStatistics *>(); QSignalSpy done(job, &FolderStatistics::finished); window.findChild<QAction *>("viewAction")->trigger();
        if (done.isEmpty()) QVERIFY(done.wait(10000)); QTRY_VERIFY(!window.operationBusy()); auto result = qvariant_cast<FolderStatisticsResult>(done.first()[0]); QVERIFY(!result.items.first().complete);
        QVERIFY(folder->text(1).isEmpty()); QVERIFY(folder->text(propertyColumn(tree,OfficialSort::kpidNumSubDirs)).isEmpty()); QVERIFY(folder->text(propertyColumn(tree,OfficialSort::kpidNumSubFiles)).isEmpty());
        QCOMPARE(resultDialogs.notices.size(),1); QVERIFY(resultDialogs.notices.first().error); QVERIFY(resultDialogs.notices.first().text.contains(root + "/folder/private"));
        QVERIFY(::chmod(QFile::encodeName(root + "/folder/private").constData(), 0700) == 0); done.clear(); window.findChild<QAction *>("viewAction")->trigger();
        if (done.isEmpty()) QVERIFY(done.wait(10000)); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(folder->text(1), QString("7")); QCOMPARE(folder->text(propertyColumn(tree,OfficialSort::kpidNumSubDirs)), QString("1")); QCOMPARE(folder->text(propertyColumn(tree,OfficialSort::kpidNumSubFiles)), QString("1"));
    }
    void guiCancelAndClose() {
        QSettings().setValue("View/TwoPanels", true);
        const auto parent = QFileInfo(many).absolutePath(); MainWindow window(executable); window.openPath(parent); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto tree = ownedTree(window);
        auto folder = item(tree, "many entries"); QVERIFY(folder); tree->setCurrentItem(folder); folder->setSelected(true);
        auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second); auto childTree = second->findChild<FileList *>("fileList");
        auto childFolder = item(childTree, "many entries"); QVERIFY(childFolder); childTree->setCurrentItem(childFolder); childFolder->setSelected(true);
        auto job = second->findChild<FolderStatistics *>(); QSignalSpy done(job, &FolderStatistics::finished); bool paused = false;
        connect(job, &FolderStatistics::progress, &window, [&](quint64, QString) { if (!paused && job->busy()) paused = job->setPaused(true); });
        second->findChild<QAction *>("viewAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(paused, 5000); QVERIFY(window.operationBusy());
        QVERIFY(second->operationBusy()); QVERIFY(!window.findChild<QAction *>("viewAction")->isEnabled());
        auto progress = second->findChild<ProgressDialog *>(); QVERIFY(progress); auto cancel = progress->findChild<QPushButton *>("cancelOperation"); QVERIFY(cancel);
        bool confirmed=false; QTimer response; response.setInterval(5); connect(&response,&QTimer::timeout,&window,[&] { if(auto question=progress->findChild<QMessageBox *>("progressCancelConfirmation")) { confirmed=true; question->button(QMessageBox::Yes)->click(); } }); response.start();
        QTest::mouseClick(cancel, Qt::LeftButton); response.stop(); QVERIFY(confirmed); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000); QTRY_VERIFY(!window.operationBusy());
        QVERIFY(qvariant_cast<FolderStatisticsResult>(done.first()[0]).cancelled); QVERIFY(childFolder->text(1).isEmpty()); QVERIFY(childFolder->text(propertyColumn(childTree,OfficialSort::kpidNumSubDirs)).isEmpty());
        QVERIFY(!second->operationBusy()); QVERIFY(second->findChild<QAction *>("viewAction")->isEnabled());
        QVERIFY(tree->isEnabled()); QVERIFY(QFileInfo::exists(many + "/29999"));
        disconnect(job, &FolderStatistics::progress, &window, nullptr); done.clear(); paused = false;
        connect(job, &FolderStatistics::progress, &window, [&](quint64, QString) { if (!paused && job->busy()) paused = job->setPaused(true); });
        second->findChild<QAction *>("viewAction")->trigger(); QTRY_VERIFY(paused); QVERIFY(!window.close()); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 10000); QTRY_VERIFY(!window.operationBusy()); QVERIFY(window.close());
    }
};

int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("FolderStatistics"); app.setQuitOnLastWindowClosed(false);
    StatisticsTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr;
    return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "folder-statistics.moc"
