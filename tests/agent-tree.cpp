// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "MainWindow.h"
#include "PortStyle.h"
#include "PropertiesDialog.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QFile>
#include <QSettings>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

static QString engine, fixtures;
static QString propertyValue(const ArchivePropertyList &rows, QString name) { for (const auto &p : rows) if (p.name == name) return p.value; return {}; }
static FileList *list(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
static QTreeWidgetItem *single(MainWindow &window, QString name) { const auto rows = list(window)->findItems(name, Qt::MatchExactly); return rows.size() == 1 ? rows.first() : nullptr; }
class AgentTreeTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ArchiveResult listing(QString name) {
        SevenZipProcessBackend backend(engine); QSignalSpy done(&backend, &ArchiveBackend::finished); ArchiveRequest request; request.archive = fixtures + '/' + name; backend.start(request);
        if (done.isEmpty() && !done.wait(10000)) qFatal("Native tree listing timed out"); return qvariant_cast<ArchiveResult>(done.last()[0]);
    }
private slots:
    void initTestCase() { QVERIFY(temporary.isValid()); QVERIFY(QFile::exists(fixtures + "/duplicate.zip")); QVERIFY(QFile::exists(fixtures + "/streams.ntfs")); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences")); }
    void init() { QSettings().clear(); QSettings().setValue("View/LastPath", fixtures); QSettings().setValue("View/AutoRefresh", false); FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save()); }
    void zipIdentityPropertiesAndTraversal() {
        const auto result = listing("duplicate.zip"); QVERIFY2(result.success, qPrintable(result.message + result.details)); ArchiveFolderIndex index(result.entries, result.metadata); QVERIFY(index.hasDirectories());
        QList<ArchiveRow> duplicates; for (const auto &row : index.directoryRows(0, false)) if (row.name == "same.txt") duplicates.append(row);
        QCOMPARE(duplicates.size(), 2); QVERIFY(duplicates[0].archiveIndex != duplicates[1].archiveIndex); QVERIFY(duplicates[0].identity.item != duplicates[1].identity.item);
        QCOMPARE(propertyValue(index.rowProperties(duplicates[0].identity, false, 0), "Size"), QString("3")); QCOMPARE(propertyValue(index.rowProperties(duplicates[1].identity, false, 0), "Size"), QString("26"));
        const auto selected = index.nativeSelectionProperties({duplicates[0].identity, duplicates[1].identity}, false, 0); QCOMPARE(propertyValue(selected, "Size"), QString("29")); QCOMPARE(propertyValue(selected, "Files"), QString("2"));
        const auto implicit = index.findDirectory("implicit/"); QVERIFY(implicit > 0); QCOMPARE(index.directory(implicit)->totals.archiveIndex, qint64(-1)); QCOMPARE(propertyValue(index.directoryProperties(implicit), "Name"), QString("implicit/")); QCOMPARE(propertyValue(index.directoryProperties(implicit), "Files"), QString("1"));
        const auto flat = index.directoryRows(0, true); QCOMPARE(flat.size(), 6); QCOMPARE(flat[0].name, QString("empty")); QCOMPARE(flat[1].name, QString("implicit")); QCOMPARE(flat[2].name, QString("sub")); QCOMPARE(flat[3].name, QString("deep.txt"));
        QCOMPARE(propertyValue(index.rowProperties(flat[3].identity, true, 0), "Path Prefix"), QString("implicit/sub/")); QVERIFY(propertyValue(index.rowProperties(flat[1].identity, true, 0), "Size").isEmpty());
        MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(fixtures + "/duplicate.zip"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QCOMPARE(list(window)->findItems("same.txt", Qt::MatchExactly).size(), 2); QVERIFY(single(window, "implicit")); list(window)->setCurrentItem(single(window, "implicit")); window.findChild<QAction *>("insideAction")->trigger(); QVERIFY(single(window, "sub"));
        window.findChild<QAction *>("upAction")->trigger(); QVERIFY(single(window, "implicit")); window.findChild<QAction *>("flatAction")->trigger(); QVERIFY(single(window, "implicit")); QVERIFY(single(window, "sub")); QVERIFY(single(window, "deep.txt"));
        auto rows = list(window)->findItems("same.txt", Qt::MatchExactly); list(window)->clearSelection(); rows[0]->setSelected(true); QVERIFY(window.findChild<QAction *>("renameAction")->isEnabled()); QVERIFY(window.findChild<QAction *>("deleteAction")->isEnabled()); rows[1]->setSelected(true); window.findChild<QAction *>("infoAction")->trigger(); auto dialog = window.findChild<PropertiesDialog *>("propertiesDialog"); QVERIFY(dialog); auto size = dialog->table()->findItems("Size", Qt::MatchExactly); QVERIFY(!size.isEmpty()); QCOMPARE(size.first()->data(1, Qt::UserRole).toString(), QString("29")); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
    }
    void nativeAlternateStreamBinding() {
        const auto result = listing("streams.ntfs"); QVERIFY2(result.success, qPrintable(result.message + result.details)); ArchiveFolderIndex index(result.entries, result.metadata);
        QVERIFY(index.hasDirectories()); const auto root = index.directory(0); QVERIFY(root); QCOMPARE(root->parent, -1);
        ArchiveRow payload; for (const auto &row : root->children) if (row.name == "payload.txt") payload = row; QVERIFY(payload.archiveIndex >= 0); QVERIFY(payload.alternateDirectory > 0);
        const auto alt = index.directory(payload.alternateDirectory); QVERIFY(alt); QVERIFY(alt->alternateStreams); QCOMPARE(alt->parent, 0); QCOMPARE(alt->path, QString("payload.txt:")); QCOMPARE(alt->children.size(), 1); QCOMPARE(alt->children.first().name, QString("details")); QCOMPARE(alt->children.first().path, QString("payload.txt:details"));
        QCOMPARE(propertyValue(index.rowProperties(alt->children.first().identity, false, alt->id), "Size"), QString("14"));
        for (const auto &row : index.directoryRows(0, true)) QVERIFY(row.name != "details");
        MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(fixtures + "/streams.ntfs"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto action = window.findChild<QAction *>("alternateStreamsAction"); QVERIFY(action); QVERIFY(!action->isEnabled()); auto file = single(window, "payload.txt"); QVERIFY(file); list(window)->setCurrentItem(file); QVERIFY(action->isEnabled());
        bool clicked = false; QTimer choose, limit; choose.setInterval(10); limit.setSingleShot(true);
        connect(&choose, &QTimer::timeout, &window, [&] { if (auto menu = window.findChild<QMenu *>("fileContextMenu"); menu && menu->isVisible()) { auto copy = menu->findChild<QAction *>(action->objectName(), Qt::FindDirectChildrenOnly); if (!copy) return; choose.stop(); clicked = true; QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(copy).center()); } });
        connect(&limit, &QTimer::timeout, &window, [&] { for (auto menu : window.findChildren<QMenu *>()) menu->close(); }); choose.start(); limit.start(5000);
        const auto point = list(window)->visualItemRect(file).center(); QContextMenuEvent event(QContextMenuEvent::Mouse, point, list(window)->viewport()->mapToGlobal(point)); QApplication::sendEvent(list(window)->viewport(), &event); choose.stop(); limit.stop(); QVERIFY(clicked);
        QVERIFY(single(window, "details")); QVERIFY(!single(window, "payload.txt")); QVERIFY(!action->isEnabled());
        list(window)->setCurrentItem(single(window, "details")); window.findChild<QAction *>("infoAction")->trigger(); auto dialog = window.findChild<PropertiesDialog *>("propertiesDialog"); QVERIFY(dialog); auto size = dialog->table()->findItems("Size", Qt::MatchExactly); QVERIFY(!size.isEmpty()); QCOMPARE(size.first()->data(1, Qt::UserRole).toString(), QString("14")); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
        window.findChild<QAction *>("upAction")->trigger(); QVERIFY(single(window, "payload.txt")); window.findChild<QAction *>("flatAction")->trigger(); QVERIFY(!single(window, "details"));
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 3) return 1; engine = QString::fromLocal8Bit(argv[1]); fixtures = QString::fromLocal8Bit(argv[2]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("AgentTree"); app.setQuitOnLastWindowClosed(false);
    AgentTreeTests tests; QList<char *> args{argv[0]}; for (int i = 3; i < argc; ++i) args << argv[i]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "agent-tree.moc"
