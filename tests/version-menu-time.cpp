// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "VersionControl.h"
#include "MainWindow.h"
#include "PanelMenu.h"
#include "TimeText.h"
#include "OverwriteDialog.h"
#include "UiLanguage.h"
#include "PortStyle.h"
#include <QApplication>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QtTest>
#include <fcntl.h>
#include <unistd.h>
static QString executable;
static bool write(QString path, QByteArray bytes) { if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false; QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static mode_t mode(QString path) { struct stat s{}; return ::stat(QFile::encodeName(path).constData(), &s) == 0 ? s.st_mode & 07777 : 0; }
static bool permissions(QString path, mode_t value) { return ::chmod(QFile::encodeName(path).constData(), value) == 0; }
static bool setTime(QString path, time_t seconds, long ns) { timespec times[2]{{seconds, ns}, {seconds, ns}}; return ::utimensat(AT_FDCWD, QFile::encodeName(path).constData(), times, 0) == 0; }
static timespec modified(QString path) { struct stat s{}; ::stat(QFile::encodeName(path).constData(), &s); return s.st_mtimespec; }
static QString shadow(QString store, QString source) { return QDir::cleanPath(store + '/' + source); }
static FileList *files(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
static bool select(MainWindow &window, QString name) { const auto rows = files(window)->findItems(name, Qt::MatchExactly); if (rows.size() != 1) return false; files(window)->clearSelection(); files(window)->setCurrentItem(rows.first()); files(window)->setItemSelected(rows.first(), true); return true; }

class VersionMenuTimeTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    VersionResult execute(VersionCommand command, QString path, QString store, OverwriteAnswer answer = OverwriteAnswer::Yes) {
        VersionControl worker; QSignalSpy finished(&worker, &VersionControl::finished);
        connect(&worker, &VersionControl::overwriteRequested, &worker, [&worker, answer](OverwriteConflict conflict) { worker.resolveOverwrite(conflict.id, answer); });
        worker.start(command, path, store); if (finished.isEmpty() && !finished.wait(15000)) return {command, path, "Test timed out", {}};
        return qvariant_cast<VersionResult>(finished.last()[0]);
    }
private slots:
    void initTestCase() { QVERIFY(temporary.isValid()); qInfo().noquote() << "Production preferences:" << QSettings(QSettings::NativeFormat, QSettings::UserScope, "SevenZipMacPort", "7-Zip Mac Port").fileName(); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences")); }
    void init() { QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/AutoRefresh", false); }
    void editCommitHistoryAndRevert() {
        const auto root = temporary.filePath("cycle"), path = root + "/source/日本語 space.txt", store = root + "/store";
        QVERIFY(write(path, "original")); QVERIFY(setTime(path, 1791100000, 123456700)); QVERIFY(permissions(path, 0444));
        auto result = execute(VersionCommand::Edit, path, store); QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        const auto backup = shadow(store, path); QCOMPARE(read(backup), QByteArray("original")); QCOMPARE(mode(backup), mode_t(0444)); QCOMPARE(mode(path), mode_t(0644));
        QCOMPARE(modified(backup).tv_sec, time_t(1791100000)); QCOMPARE(modified(backup).tv_nsec, 123456700L);
        QVERIFY(write(path, "second")); QVERIFY(setTime(path, 1791110000, 345678900));
        result = execute(VersionCommand::Commit, path, store); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(mode(path), mode_t(0444)); QCOMPARE(read(backup), QByteArray("original"));
        QVERIFY(modified(path).tv_sec <= 1791110000 && modified(path).tv_sec > 1791100000); QCOMPARE(modified(path).tv_nsec, 0L);
        result = execute(VersionCommand::Edit, path, store); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(read(backup), QByteArray("second"));
        const auto history = QFileInfo(backup).absolutePath() + "/_7vc/日本語 space.txt";
        QCOMPARE(read(history + "/001"), QByteArray("original")); QVERIFY(write(history + "/not-number", "ignored")); QVERIFY(write(history + "/2147483648", "ignored large")); QVERIFY(write(history + "/004", "numbered history"));
        QVERIFY(write(path, "third")); QVERIFY(setTime(path, 1791120000, 765432100)); QVERIFY(execute(VersionCommand::Commit, path, store).error.isEmpty());
        result = execute(VersionCommand::Edit, path, store); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(read(history + "/005"), QByteArray("second")); QCOMPARE(read(backup), QByteArray("third"));
        const auto savedTime = modified(backup); QVERIFY(write(path, "uncommitted"));
        result = execute(VersionCommand::Revert, path, store, OverwriteAnswer::No); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(read(path), QByteArray("uncommitted"));
        result = execute(VersionCommand::Diff, path, store); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(result.diffPaths, QStringList({QFileInfo(backup).canonicalFilePath(), path}));
        result = execute(VersionCommand::Revert, path, store); QVERIFY2(result.error.isEmpty(), qPrintable(result.error)); QCOMPARE(read(path), QByteArray("third")); QCOMPARE(mode(path), mode_t(0444)); QCOMPARE(modified(path).tv_sec, savedTime.tv_sec); QCOMPARE(modified(path).tv_nsec, savedTime.tv_nsec);
    }
    void sameBytesDifferentTime() {
        const auto root = temporary.filePath("same"), path = root + "/file", store = root + "/store"; QVERIFY(write(path, "same")); QVERIFY(setTime(path, 1791100000, 0)); QVERIFY(permissions(path, 0444)); QVERIFY(execute(VersionCommand::Edit, path, store).error.isEmpty());
        QVERIFY(setTime(path, 1791100001, 0)); auto result = execute(VersionCommand::Commit, path, store); QVERIFY(result.error.contains("Same data, but different timestamps")); QCOMPARE(mode(path), mode_t(0644));
        bool prompted = false; VersionControl worker; QSignalSpy done(&worker, &VersionControl::finished); connect(&worker, &VersionControl::overwriteRequested, &worker, [&](OverwriteConflict c) { prompted = true; worker.resolveOverwrite(c.id, OverwriteAnswer::Yes); }); worker.start(VersionCommand::Revert, path, store); QVERIFY(done.wait(10000)); QVERIFY(!prompted); QCOMPARE(modified(path).tv_sec, time_t(1791100000)); QCOMPARE(mode(path), mode_t(0444));
    }
    void validationAndNoSnapshot() {
        const auto root = temporary.filePath("validation"), path = root + "/file", store = root + "/store"; QVERIFY(write(path, "data"));
        QVERIFY(execute(VersionCommand::Edit, path, store).error.contains("not read-only")); QVERIFY(execute(VersionCommand::Revert, path, store).error.contains("No file to revert")); QVERIFY(execute(VersionCommand::Diff, path, store).diffPaths.isEmpty());
        QVERIFY(permissions(path, 0444)); for (auto command : {VersionCommand::Commit, VersionCommand::Revert, VersionCommand::Diff}) QVERIFY(execute(command, path, store).error.contains("File is read-only"));
        QCOMPARE(read(path), QByteArray("data"));
        QFile big(root + "/large"); QVERIFY(big.open(QIODevice::WriteOnly)); QVERIFY(big.resize((qint64(1) << 28) + 1)); big.close(); QVERIFY(permissions(big.fileName(), 0444)); QVERIFY(!execute(VersionCommand::Edit, big.fileName(), store).error.isEmpty()); QVERIFY(!QFileInfo::exists(shadow(store, big.fileName())));
        const auto link = root + "/link"; QVERIFY(QFile::link(path, link)); QVERIFY(!execute(VersionCommand::Edit, link, store).error.isEmpty()); QCOMPARE(read(path), QByteArray("data"));
    }
    void asynchronousPauseCancel() {
        const auto root = temporary.filePath("cancel"), path = root + "/file", store = root + "/store"; QVERIFY(write(path, QByteArray(32 * 1024 * 1024, 'a'))); QVERIFY(permissions(path, 0444));
        VersionControl worker; QSignalSpy done(&worker, &VersionControl::finished); bool paused = false; int ticks = 0; QTimer heartbeat; heartbeat.setInterval(1); connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; }); heartbeat.start();
        connect(&worker, &VersionControl::byteProgress, &worker, [&](quint64, quint64, QString) { if (!paused) { paused = worker.setPaused(true); QTimer::singleShot(30, &worker, &VersionControl::cancel); } }); worker.start(VersionCommand::Edit, path, store); QVERIFY(done.wait(10000)); QVERIFY(paused); QVERIFY(ticks > 2); QVERIFY(qvariant_cast<VersionResult>(done.last()[0]).error.contains("cancelled")); QCOMPARE(mode(path), mode_t(0444)); QVERIFY(!QFileInfo::exists(shadow(store, path))); QVERIFY(execute(VersionCommand::Edit, path, store).error.isEmpty());
    }
    void changedOriginalDuringRevert() {
        const auto root = temporary.filePath("changed"), path = root + "/file", store = root + "/store"; QVERIFY(write(path, "before")); QVERIFY(permissions(path, 0444)); QVERIFY(execute(VersionCommand::Edit, path, store).error.isEmpty()); QVERIFY(write(path, "edited"));
        VersionControl worker; QSignalSpy done(&worker, &VersionControl::finished); connect(&worker, &VersionControl::overwriteRequested, &worker, [&](OverwriteConflict c) { write(path, "changed after prompt"); worker.resolveOverwrite(c.id, OverwriteAnswer::Yes); }); worker.start(VersionCommand::Revert, path, store); QVERIFY(done.wait(10000)); QVERIFY(!qvariant_cast<VersionResult>(done.last()[0]).error.isEmpty()); QCOMPARE(read(path), QByteArray("changed after prompt")); QCOMPARE(read(shadow(store, path)), QByteArray("before"));
    }
    void officialMenuConditions() {
        const auto path = temporary.filePath("menu/file"); QVERIFY(write(path, "menu")); FileMenuState state; state.count = 1; state.filePath = path; state.diff = "diff";
        QVERIFY(officialVersionMenuItems(state).isEmpty()); state.versionStore = "store";
        QCOMPARE(officialVersionMenuItems(state), QStringList({"verCommitAction", "verRevertAction", "verDiffAction"})); QVERIFY(permissions(path, 0444)); QCOMPARE(officialVersionMenuItems(state), QStringList({"verEditAction"}));
        state.filesystem = false; QVERIFY(officialVersionMenuItems(state).isEmpty()); state.filesystem = true; state.count = 2; QVERIFY(officialVersionMenuItems(state).isEmpty()); state.count = 1; state.allFiles = false; QVERIFY(officialVersionMenuItems(state).isEmpty()); state.allFiles = true; state.diff.clear(); QVERIFY(officialVersionMenuItems(state).isEmpty());
    }
    void guiCommandsAndOverwriteDefault() {
        const auto root = temporary.filePath("gui"), path = root + "/日本語 space.txt", store = root + "/store";
        QVERIFY(write(path, "GUI original")); QVERIFY(permissions(path, 0444)); QSettings().setValue("7vc", store); auto prefs = FileManagerSettings::load(); prefs.diff = "/usr/bin/true"; QVERIFY(prefs.save());
        MainWindow window(executable); window.show(); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(select(window, "日本語 space.txt"));
        const auto edit = window.findChild<QAction *>("verEditAction"); QVERIFY(edit && edit->isVisible()); auto worker = window.findChild<VersionControl *>(); QSignalSpy done(worker, &VersionControl::finished);
        edit->trigger(); QTRY_VERIFY_WITH_TIMEOUT(done.size() == 1 && !window.operationBusy(), 10000); QCOMPARE(read(shadow(store, path)), QByteArray("GUI original")); QCOMPARE(mode(path), mode_t(0644));
        QVERIFY(write(path, "GUI edits")); QVERIFY(select(window, "日本語 space.txt")); bool shown = false, defaultNo = false, extras = true; QTimer responder; responder.setInterval(10);
        const auto script = root + "/diff-witness.sh"; QVERIFY(write(script, "printf '%s\\n' \"$1\" \"$2\" > diff-arguments.txt\n")); prefs.diff = "/bin/sh \"" + script + "\""; QVERIFY(prefs.save());
        // Reuse the standard settings-Apply path without opening another modal.
        window.findChild<QAction *>("time1Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(select(window, "日本語 space.txt"));
        window.findChild<QAction *>("verDiffAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(done.size() == 2 && !window.operationBusy(), 10000); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(root + "/diff-arguments.txt"), 10000); QCOMPARE(read(root + "/diff-arguments.txt"), (QFileInfo(shadow(store, path)).canonicalFilePath() + '\n' + path + '\n').toUtf8());
        connect(&responder, &QTimer::timeout, &window, [&] { for (auto dialog : window.findChildren<OverwriteDialog *>()) if (dialog->isVisible()) { shown = true; defaultNo = dialog->findChild<QPushButton *>("overwriteNo")->isDefault(); extras = dialog->findChild<QPushButton *>("overwriteYesAll") != nullptr; QTest::mouseClick(dialog->findChild<QPushButton *>("overwriteYes"), Qt::LeftButton); } }); responder.start();
        window.findChild<QAction *>("verRevertAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(done.size() == 3 && !window.operationBusy(), 10000); QVERIFY(shown && defaultNo && !extras); QCOMPARE(read(path), QByteArray("GUI original")); QCOMPARE(mode(path), mode_t(0444));
        QMenu menu; FileMenuState state; state.count = 1; state.filePath = path; state.diff = prefs.diff; state.versionStore = store; appendOfficialFileMenu(&menu, window.menuBar()->actions().first()->menu(), state); QCOMPARE(menu.findChildren<QAction *>("verEditAction").size(), 1); QVERIFY(!menu.findChild<QAction *>("verCommitAction"));
        files(window)->clearSelection(); edit->trigger(); QTest::qWait(30); QCOMPARE(done.size(), 3); // focused but unmarked => original no-op
    }
    void sourceTimeRenderer() {
        const auto now = QDateTime(QDate(2026, 10, 5), QTime(1, 2, 3, 456), QTimeZone::UTC);
        QCOMPARE(officialTimeText(now, "123456789", 0, true), QString("2026-10-05Z")); QCOMPARE(officialTimeText(now, "123456789", 1, true), QString("2026-10-05 01:02Z")); QCOMPARE(officialTimeText(now, "123456789", 2, true), QString("2026-10-05 01:02:03Z"));
        QCOMPARE(officialTimeText(now, "123456789", 7, true), QString("2026-10-05 01:02:03.1234567Z")); QCOMPARE(officialTimeText(now, "123456789", 9, true), QString("2026-10-05 01:02:03.123456789Z")); QVERIFY(!officialTimeText(now, {}, 7, false).endsWith('Z'));
        QCOMPARE(officialTimeMenuSamples(now, true).size(), 5); QVERIFY(officialTimeText({}, {}, 2, false).isEmpty());
    }
    void dynamicTimeMenuAndDefault() {
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto view = window.menuBar()->actions()[2]->menu(); auto menu = view->findChild<QMenu *>("timeMenu"); QVERIFY(menu);
        QMetaObject::invokeMethod(view, "aboutToShow"); QVERIFY(window.findChild<QAction *>("time1Action")->isChecked()); QVERIFY(menu->title().startsWith(QDate::currentDate().toString("yyyy-MM-dd"))); QCOMPARE(menu->actions().size(), 6);
        window.findChild<QAction *>("timeUTCAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QMetaObject::invokeMethod(view, "aboutToShow"); for (int n = 0; n < 5; ++n) QVERIFY(menu->actions()[n]->text().endsWith('Z')); window.findChild<QAction *>("time7Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(QSettings().value("View/TimePrecision").toInt(), 7);
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 2; executable = argv[1]; const auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("VersionMenuTime"); app.setQuitOnLastWindowClosed(false);
    VersionMenuTimeTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "version-menu-time.moc"
