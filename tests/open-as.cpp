// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "ArchiveFormats.h"
#include "MainWindow.h"
#include "OpenWith.h"
#include "PortStyle.h"
#include "UiLanguage.h"
#include <QApplication>
#include <QContextMenuEvent>
#include <QDialogButtonBox>
#include <QFile>
#include "input-driver.h"
#include <QLayout>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>

static QString executable;
static QByteArray read(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static bool write(const QString &path, const QByteArray &bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static ArchiveResult result(const QSignalSpy &spy) { return spy.isEmpty() ? ArchiveResult() : qvariant_cast<ArchiveResult>(spy.last().first()); }
static QStringList modes(QMenu *menu) { QStringList result; if (menu) for (auto action : menu->actions()) result << action->data().toString(); return result; }
static bool focus(MainWindow &window, QString name) {
    auto list = window.findChild<FileList *>("fileList"); const auto rows = list->findItems(name, Qt::MatchExactly); if (rows.size() != 1) return false;
    list->clearSelection(); list->setCurrentItem(rows.first()); return true;
}
class OpenAsTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QString zip, split, overlap;
    QByteArray zipBytes, overlapBytes;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(30000)) return {}; return result(done);
    }
    ArchiveResult create(QString archive, QStringList files, QString password = {}) {
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = archive; request.workingDirectory = temporary.path(); request.files = files;
        request.format = archive.endsWith(".zip") ? "zip" : "7z"; request.method = request.format == "zip" ? "Deflate" : "LZMA2"; request.level = 1; request.password = password; request.encryptNames = !password.isEmpty(); return execute(request);
    }
    // QMenu::exec and QPushButton::showMenu run nested event loops. Select by
    // real Qt menu hit-testing while their timers run; no native focus claim.
    bool clickPopup(QMenu *menu, const QString &mode) {
        for (auto action : menu->actions()) if (action->data().toString() == mode) {
            QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center()); return true;
        }
        return false;
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        QVERIFY(write(temporary.filePath("日本語 space.txt"), "independent Open As payload 日本語\n"));
        QVERIFY(write(temporary.filePath("folder/inside.txt"), "folder payload\n"));
        zip = temporary.filePath("plain.zip"); QVERIFY(create(zip, {"日本語 space.txt", "folder"}).success); zipBytes = read(zip);
        QVERIFY(create(temporary.filePath("plain.7z"), {"日本語 space.txt"}).success);
        split = temporary.filePath("container.bin.001");
        for (qsizetype position = 0, index = 1; position < zipBytes.size(); position += 75, ++index) QVERIFY(write(temporary.filePath("container.bin." + QString::number(index).rightJustified(3, '0')), zipBytes.mid(position, 75)));
        QProcess fixtures;
        fixtures.start("python3", {"-c", R"PY(
import binascii, io, pathlib, sys, zipfile
source, root = map(pathlib.Path, sys.argv[1:])
for name, output in [('test_read_format_cab_1.cab.uu', 'sample.cab'), ('test_read_format_rar.rar.uu', 'legacy.rar'), ('test_read_format_rar5_stored.rar.uu', 'modern.rar')]:
    lines=(source/name).read_bytes().splitlines(); start=next(n for n,line in enumerate(lines) if line.startswith(b'begin ')); data=b''
    for line in lines[start+1:]:
        if line == b'end': break
        data += binascii.a2b_uu(line)
    (root/output).write_bytes(data)
inner=io.BytesIO()
with zipfile.ZipFile(inner,'w',compression=zipfile.ZIP_STORED) as z: z.writestr('payload.txt',b'payload')
outer=io.BytesIO()
with zipfile.ZipFile(outer,'w',compression=zipfile.ZIP_STORED) as z: z.writestr('inner.zip',inner.getvalue())
(root/'overlap.bin').write_bytes(b'P'*60+outer.getvalue()+b'S'*80)
)PY", OPEN_AS_FIXTURES, temporary.path()}); QVERIFY(fixtures.waitForFinished(10000)); QCOMPARE(fixtures.exitCode(), 0);
        overlap = temporary.filePath("overlap.bin"); overlapBytes = read(overlap);
        qInfo() << "Owned Open As fixtures:" << temporary.path();
    }
    void init() {
        QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/AutoRefresh", false);
        FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save()); UiLanguage::set("en");
    }
    void menuOrderAndIndependentSettings() {
        const QStringList types{"", "*", "#", "#:e", "7z", "zip", "cab", "rar"}; QCOMPARE(ArchiveFormats::openTypes(), types);
        QCOMPARE(OpenWithSettings::items().size(), 10); QVERIFY(OpenWithSettings::load().enabled.contains("openAs"));
        OpenWithMenu defaults({zip}); auto button = defaults.findChild<QPushButton *>("openWith_openAs"); QVERIFY(button); QCOMPARE(modes(button->menu()), types.mid(1));
        QCOMPARE(defaults.layout()->itemAt(defaults.layout()->count() - 1)->widget()->objectName(), QString("openWith_manager"));
        QMenu context; populateOpenWithContextMenu(&context, {zip}, [](QString) {}); const auto actions = context.actions(); QCOMPARE(actions[0]->objectName(), QString("context_open")); QCOMPARE(actions[1]->objectName(), QString("context_openAs")); QCOMPARE(modes(actions[1]->menu()), types.mid(1));
        auto settings = OpenWithSettings::load(); settings.enabled.removeAll("open"); settings.icons = false; QVERIFY(settings.save());
        OpenWithMenu onlyAs({zip}); QVERIFY(!onlyAs.findChild<QPushButton *>("openWith_open")); button = onlyAs.findChild<QPushButton *>("openWith_openAs"); QVERIFY(button); QCOMPARE(modes(button->menu()), types); QVERIFY(button->icon().isNull());
        settings.enabled.removeAll("openAs"); settings.enabled << "open"; QVERIFY(settings.save()); OpenWithMenu onlyOpen({zip}); QVERIFY(!onlyOpen.findChild<QPushButton *>("openWith_openAs")); QVERIFY(onlyOpen.findChild<QPushButton *>("openWith_open"));
    }
    void visibility_data() {
        QTest::addColumn<QStringList>("names"); QTest::addColumn<bool>("visible");
        QTest::newRow("archive") << QStringList{"plain.zip"} << true; QTest::newRow("unknown-signature-candidate") << QStringList{"overlap.bin"} << true;
        QTest::newRow("multiple") << QStringList{"plain.zip", "plain.7z"} << false; QTest::newRow("folder") << QStringList{"folder"} << false; QTest::newRow("excluded-text") << QStringList{"日本語 space.txt"} << false;
    }
    void visibility() {
        QFETCH(QStringList, names); QFETCH(bool, visible); QStringList paths; for (auto name : names) paths << temporary.filePath(name);
        OpenWithMenu dialog(paths); QCOMPARE(bool(dialog.findChild<QPushButton *>("openWith_openAs")), visible);
        QMenu context; populateOpenWithContextMenu(&context, paths, [](QString) {}); QCOMPARE(bool(context.findChild<QAction *>("context_openAs")), visible);
    }
    void savedChoiceApplyCancel() {
        // A preexisting explicit list is authoritative: do not enable a new
        // command behind the user's back when they previously saved choices.
        auto settings = OpenWithSettings::load(); settings.enabled.removeAll("openAs"); QVERIFY(settings.save()); QVERIFY(!OpenWithSettings::load().enabled.contains("openAs"));
        OptionsDialog options; auto rows = options.findChild<QTreeWidget *>("openWithItems"); QCOMPARE(rows->topLevelItemCount(), 10); QTreeWidgetItem *row = nullptr;
        for (int n = 0; n < rows->topLevelItemCount(); ++n) if (rows->topLevelItem(n)->data(0, Qt::UserRole) == "openAs") row = rows->topLevelItem(n);
        QVERIFY(row); QCOMPARE(row->checkState(0), Qt::Unchecked); row->setCheckState(0, Qt::Checked); options.show();
        QTest::mouseClick(options.findChild<QDialogButtonBox *>("optionsButtons")->button(QDialogButtonBox::Apply), Qt::LeftButton); QVERIFY(OpenWithSettings::load().enabled.contains("openAs"));
        row->setCheckState(0, Qt::Unchecked); options.reject(); QVERIFY(OpenWithSettings::load().enabled.contains("openAs"));
    }
    void forcedHandlers_data() {
        QTest::addColumn<QString>("name"); QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("member"); QTest::addColumn<QByteArray>("expected");
        QTest::newRow("7z") << QString("plain.7z") << QString("7z") << QString("日本語 space.txt") << QByteArray("independent Open As payload 日本語\n");
        QTest::newRow("zip") << QString("plain.zip") << QString("zip") << QString("日本語 space.txt") << QByteArray("independent Open As payload 日本語\n");
        QTest::newRow("cab") << QString("sample.cab") << QString("cab") << QString("dir1/file1") << QByteArray("                          file 1 contents\nhello\nhello\nhello\n");
        QTest::newRow("rar") << QString("legacy.rar") << QString("rar") << QString("test.txt") << QByteArray("test text document\r\n");
    }
    void forcedHandlers() {
        QFETCH(QString, name); QFETCH(QString, mode); QFETCH(QString, member); QFETCH(QByteArray, expected);
        ArchiveRequest request; request.archive = temporary.filePath(name); request.readMode = mode; const auto before = read(request.archive);
        auto listing = execute(request); QVERIFY2(listing.success, qPrintable(listing.message + listing.details)); QVERIFY(std::any_of(listing.entries.begin(), listing.entries.end(), [&](const auto &entry) { return entry.path == member; }));
        request.operation = ArchiveOperation::Test; QVERIFY(execute(request).success); request.operation = ArchiveOperation::Extract; request.files = {member}; request.outputDirectory = temporary.filePath("forced extract/" + mode); request.overwriteMode = "overwrite";
        auto restored = execute(request); QVERIFY2(restored.success, qPrintable(restored.message + restored.details)); QCOMPARE(read(request.outputDirectory + '/' + member), expected); QCOMPARE(read(request.archive), before);
    }
    void parserEachPosition() {
        ArchiveRequest request; request.archive = overlap; request.readMode = "#"; auto ordinary = execute(request); QVERIFY(ordinary.success); QCOMPARE(ordinary.entries.size(), 3);
        request.readMode = "#:e"; auto every = execute(request); QVERIFY2(every.success, qPrintable(every.details)); QCOMPARE(every.entries.size(), 4);
        QCOMPARE(every.entries[1].properties.value("Offset").toULongLong(), 60ULL); QCOMPARE(every.entries[2].properties.value("Offset").toULongLong(), 99ULL);
        QCOMPARE(every.entries[2].properties.value("Type"), QString("zip")); QVERIFY(every.entries[2].size + 99 <= every.entries[1].size + 60);
        request.operation = ArchiveOperation::Test; QVERIFY(execute(request).success); request.operation = ArchiveOperation::Extract; request.outputDirectory = temporary.filePath("each-position extract"); request.overwriteMode = "overwrite"; QVERIFY(execute(request).success);
        for (const auto &entry : every.entries) QCOMPARE(read(request.outputDirectory + '/' + entry.path), overlapBytes.mid(entry.properties.value("Offset").toLongLong(), entry.size));
        QCOMPARE(read(overlap), overlapBytes);
    }
    void forcedHandlerFailuresAndRar5() {
        ArchiveRequest request; request.archive = zip; request.readMode = "cab"; auto failure = execute(request); QVERIFY(!failure.success); QCOMPARE(failure.exitCode, 2); QCOMPARE(failure.target, zip); QCOMPARE(read(zip), zipBytes);
        request.archive = temporary.filePath("modern.rar"); const auto before = read(request.archive); request.readMode = "rar"; failure = execute(request); QVERIFY(!failure.success); QCOMPARE(failure.exitCode, 2);
        request.readMode.clear(); auto normal = execute(request); QVERIFY(normal.success); QCOMPARE(normal.archiveType, QString("Rar5"));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = temporary.filePath("normal Rar5"); request.overwriteMode = "overwrite"; QVERIFY(execute(request).success); QCOMPARE(read(request.outputDirectory + "/helloworld.txt"), QByteArray("hello libarchive test suite!\n")); QCOMPARE(read(request.archive), before);
    }
    void guiOpenWithClickAndModeReset() {
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished);
        OpenWithMenu menu({split}, &window); connect(&menu, &OpenWithMenu::commandChosen, &window, &MainWindow::executeOpenWith); QSignalSpy chosen(&menu, &OpenWithMenu::commandChosen); menu.show();
        auto button = menu.findChild<QPushButton *>("openWith_openAs"); QVERIFY(button); bool clicked = false;
        QTimer select; select.setInterval(10); connect(&select, &QTimer::timeout, &menu, [&] { if (button->menu()->isVisible()) { select.stop(); clicked = clickPopup(button->menu(), "*"); } }); select.start();
        QTest::mouseClick(button, Qt::LeftButton); QTRY_VERIFY_WITH_TIMEOUT(clicked, 5000); select.stop(); QCOMPARE(chosen.size(), 1); QCOMPARE(chosen.first()[0].toString(), QString("openAs:*")); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("Split")); QVERIFY(focus(window, "container.bin"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("Split"));
        window.executeOpenWith("open", {split}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("zip")); QVERIFY(focus(window, "日本語 space.txt"));
        window.openPath(zip); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "folder")); window.findChild<QAction *>("insideAction")->trigger(); QVERIFY(focus(window, "inside.txt"));
        window.executeOpenWith("openAs:zip", {zip}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "folder")); QVERIFY(focus(window, "日本語 space.txt"));
    }
    void guiContextClickAndFailureRecovery() {
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "overlap.bin"));
        auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished); auto list = window.findChild<FileList *>("fileList"); bool clicked = false;
        QTimer select, limit; select.setInterval(20); limit.setSingleShot(true); connect(&select, &QTimer::timeout, &window, [&] {
            auto root = window.findChild<QMenu *>("fileContextMenu"); auto shell = window.findChild<QMenu *>("sevenZipContextMenu"); if (!root || !root->isVisible() || !shell) return;
            auto action = shell->findChild<QAction *>("context_openAs"); if (!action || !action->menu()) return; shell->popup(root->mapToGlobal(QPoint(root->width(), 0))); auto types = action->menu(); types->popup(shell->mapToGlobal(QPoint(shell->width(), 0))); select.stop(); clicked = clickPopup(types, "#:e");
        }); connect(&limit, &QTimer::timeout, &window, [&] { select.stop(); for (auto menu : window.findChildren<QMenu *>()) menu->close(); }); select.start(); limit.start(5000);
        const auto point = list->visualItemRect(list->currentItem()).center(); QContextMenuEvent event(QContextMenuEvent::Mouse, point, list->viewport()->mapToGlobal(point)); QApplication::sendEvent(list->viewport(), &event); select.stop(); limit.stop(); QVERIFY(clicked);
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).entries.size(), 4); QCOMPARE(window.currentArchivePath(), overlap); QVERIFY(focus(window, "3.zip"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).entries.size(), 4);
        QString warning; QTimer dismiss; dismiss.setInterval(5); connect(&dismiss, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { warning = box->text(); box->accept(); } }); dismiss.start();
        window.executeOpenWith("openAs:7z", {overlap}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!result(done).success); QVERIFY(warning.contains("Exit code: 2")); QCOMPARE(window.currentArchivePath(), overlap); QVERIFY(focus(window, "3.zip"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(result(done).success); QCOMPARE(result(done).entries.size(), 4); dismiss.stop();
        window.executeOpenWith("open", {overlap}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("zip")); QVERIFY(focus(window, "inner.zip"));
    }
    void guiFailedReopenInFolderAndPasswordCancel() {
        const auto encrypted = temporary.filePath("cancelled-password.7z"); QVERIFY(create(encrypted, {"日本語 space.txt"}, "cancelled secret").success); const auto before = read(encrypted);
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished);
        int passwords = 0, warnings = 0; QTimer responder; responder.setInterval(5); connect(&responder, &QTimer::timeout, &window, [&] {
            if (auto input = testInput(&window); input && input->isVisible()) { ++passwords; input->reject(); }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { ++warnings; box->accept(); }
        }); responder.start();
        window.executeOpenWith("openAs:zip", {zip}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "folder")); window.findChild<QAction *>("insideAction")->trigger(); QVERIFY(focus(window, "inside.txt"));
        window.executeOpenWith("openAs:7z", {zip}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(warnings, 1); QVERIFY(!result(done).success); QCOMPARE(window.currentArchivePath(), zip); QVERIFY(focus(window, "inside.txt"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(result(done).success); QVERIFY(focus(window, "inside.txt"));
        window.executeOpenWith("openAs:#:e", {overlap}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).entries.size(), 4);
        window.executeOpenWith("openAs:7z", {encrypted}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(passwords, 1); QVERIFY(!result(done).success); QCOMPARE(window.currentArchivePath(), overlap); QVERIFY(focus(window, "3.zip"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(result(done).success); QCOMPARE(result(done).entries.size(), 4); QCOMPARE(passwords, 1); QCOMPARE(warnings, 1); QCOMPARE(read(encrypted), before); responder.stop();
    }
    void guiPasswordAndInvalidCommand() {
        const auto archive = temporary.filePath("encrypted Open As.7z"); QVERIFY(create(archive, {"日本語 space.txt"}, "Open As secret").success); const auto before = read(archive);
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished); int passwords = 0, warnings = 0;
        QTimer responder; responder.setInterval(5); connect(&responder, &QTimer::timeout, &window, [&] {
            if (auto input = testInput(&window); input && input->isVisible()) { ++passwords; input->setTextValue("Open As secret"); input->accept(); }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { ++warnings; box->accept(); }
        }); responder.start(); window.executeOpenWith("openAs:7z", {archive}); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(passwords, 1); QCOMPARE(warnings, 0); QVERIFY(result(done).success); QVERIFY(focus(window, "日本語 space.txt"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(passwords, 1); QVERIFY(result(done).success);
        const auto completions = done.size(); window.executeOpenWith("openAs:zip:invalid", {archive}); window.executeOpenWith("openAs:zip", {zip, archive}); QCOMPARE(warnings, 2); QCOMPARE(done.size(), completions); QCOMPARE(window.currentArchivePath(), archive); QCOMPARE(read(archive), before); responder.stop();
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("OpenAs"); app.setQuitOnLastWindowClosed(false);
    OpenAsTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "open-as.moc"
