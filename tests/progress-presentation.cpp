#include "progress-dialog-driver.h"
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ProgressPresentation.h"
#include "Dialogs.h"
#include "UiLanguage.h"
#include "PortStyle.h"
#include "WorkerProcess.h"
#include <QApplication>
#include <QAction>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QtConcurrent>
#include <sys/resource.h>
#include <libproc.h>
#include <mach/task_policy.h>

static QString executable;
class ProgressPresentationTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
    static QString read(QString path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString(); }
    static void write(QString path, QByteArray bytes) { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(bytes), qint64(bytes.size())); }
    static void english(OfficialProgressState &state) { state.setLanguage("Extracting", "Pause", "Continue", "Paused", "Background", "Foreground"); }
    static int childBackground(quint32 pid) {
        proc_bsdinfo info{};
        if (::proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) != sizeof(info)) return -1;
        return (info.pbi_flags & (PROC_FLAG_DARWINBG | PROC_FLAG_EXT_DARWINBG)) ? 1 : 0;
    }
private slots:
    void initTestCase() { QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("prefs")); }
    void init() { resultDialogs.clear(); UiLanguage::set("en"); }
    void originalStatisticsAndPause() {
        OfficialProgressState state; english(state);
        ArchiveProgress p; p.mode = "extract"; p.total = 1000; p.completed = 750; p.input = 300; p.output = 750; p.files = 4; p.doneFiles = 3; p.titleFileName = "fixture.7z";
        state.update(p, 2000); const auto &view = state.view();
        QCOMPARE(view.text.value(120), QString("00:00:02")); QCOMPARE(view.text.value(121), QString("00:00:00")); QCOMPARE(view.text.value(123), QString("375 B/s"));
        QCOMPARE(view.text.value(124), QString("750")); QCOMPARE(view.text.value(110), QString("300")); QCOMPARE(view.text.value(125), QString("40%"));
        QCOMPARE(view.text.value(111), QString("3")); QCOMPARE(view.text.value(112), QString(" / 4")); QCOMPARE(view.title, QString("75% Extracting fixture.7z")); QCOMPARE(view.parentTitlePrefix, QString("75% "));
        state.setPaused(true); p.completed = 900; state.update(p, 2000); QCOMPARE(view.text.value(124), QString("750")); QVERIFY(view.title.startsWith("Paused 75% "));
        state.setBackground(true); QVERIFY(view.title.contains("Background")); QCOMPARE(view.text.value(444), QString("Foreground"));
        state.setPaused(false); state.update(p, 2100); QVERIFY(view.title.startsWith("90% "));
        p.filesProgressMode = true; p.files = 4; p.doneFiles = 2; state.update(p, 2200); QVERIFY(view.title.startsWith("50% "));
    }
    void originalFileNameAndStatusReduction() {
        OfficialProgressState state; english(state); state.setFileNameCapacity(12);
        ArchiveProgress p; p.mode = "extract"; p.current = "/folder/abcdefghijklmnopqrstuvwxyz0123456789.txt"; p.status = "0123456789abcdefghijklmnopqrst"; p.titleFileName = "abcdefghijklmnopqrstuvwxyz0123456789.txt";
        state.update(p, 0); QCOMPARE(state.view().text.value(102), QString("/folder/\nabcdef ... 89.txt")); QCOMPARE(state.view().text.value(103), QString("012345 ... opqrst"));
        QCOMPARE(state.view().title, QString("0% Extracting abcdefghijklmnopqr ... wxyz0123456789.txt"));
        p.directory = true; p.current = "/folder/deepDir"; state.update(p, 1); QCOMPARE(state.view().text.value(102), QString("/folde ... eepDir\n"));
        state.setFileNameCapacity(100); QCOMPARE(state.view().text.value(102), QString("/folder/deepDir\n"));
    }
    void originalMessagesAndLocalizedErrors() {
        OfficialProgressState state; english(state); state.addError("first\ncontinuation", 1000); state.addError("second", 1000);
        QVERIFY(state.view().errorsVisible); QCOMPARE(state.view().text.value(126), QString("2")); QCOMPARE(state.view().messages.size(), 3);
        QCOMPARE(state.view().messages[0], qMakePair(QString("1"), QString("first"))); QCOMPARE(state.view().messages[1], qMakePair(QString(), QString("continuation"))); QCOMPARE(state.view().messages[2].first, QString("2"));
        QCOMPARE(state.copyMessages({2, 0}), QString("first\nsecond\n")); QCOMPARE(state.copyMessages({}), QString("first\ncontinuation\nsecond\n"));
        QCOMPARE(officialProgressError({3, false, "日本語.txt", {}, {}}), UiLanguage::resource(3723) + " : 日本語.txt");
        QVERIFY(officialProgressError({3, true, "encrypted.txt", {}, {}}).contains(UiLanguage::resource(3710)));
        UiLanguage::set("ja"); QCOMPARE(officialProgressStatus("extract"), UiLanguage::resource(3300)); QCOMPARE(officialProgressStatus("update:0"), UiLanguage::resource(3320)); QCOMPARE(officialProgressStatus("update:7"), UiLanguage::resource(3327));
        QCOMPARE(officialProgressError({9, true, "暗号化.7z", {}, {}}), UiLanguage::resource(3729) + " : 暗号化.7z"); QVERIFY(officialProgressStatus("update:99").isEmpty());
    }
    void widgetPresentationAndLanguage() {
        ProgressDialog dialog("Extract", "日本語 archive.7z"); dialog.show();
        QVERIFY(!dialog.findChild<QTreeWidget *>("progressMessages")->isVisible());
        ArchiveProgress p; p.mode = "extract"; p.total = 100; p.completed = 75; p.current = "/tmp/日本語 space.txt"; p.status = "extract"; dialog.updateDetails(p);
        QCOMPARE(dialog.findChild<QLabel *>("progressFileName")->text(), QString("/tmp/\n日本語 space.txt")); QCOMPARE(dialog.findChild<QLabel *>("progressStatus")->text(), UiLanguage::resource(3300)); QVERIFY(dialog.windowTitle().startsWith("75% Extracting"));
        dialog.setPaused(true); UiLanguage::set("ja"); UiLanguage::apply(&dialog); QVERIFY(dialog.windowTitle().startsWith(UiLanguage::resource(447) + ' ')); dialog.setPaused(false);
        dialog.reportError({3, false, "日本語.txt", {}, {}}); auto messages = dialog.findChild<QTreeWidget *>("progressMessages"); QVERIFY(messages->isVisible()); QCOMPARE(messages->topLevelItem(0)->text(0), QString("1")); QVERIFY(messages->topLevelItem(0)->text(1).contains(UiLanguage::resource(3723)));
        QCOMPARE(messages->findChild<QAction *>("copyProgressMessages")->shortcut(), QKeySequence("Ctrl+C"));
        dialog.finishFile("controlled failure"); QVERIFY(!dialog.findChild<QPushButton *>("backgroundOperation")->isVisible()); QVERIFY(!dialog.findChild<QPushButton *>("pauseOperation")->isVisible()); QCOMPARE(dialog.findChild<QPushButton *>("cancelOperation")->text(), UiLanguage::resource(408));
    }
    void cancelConfirmation_data() {
        QTest::addColumn<int>("answer"); QTest::addColumn<bool>("alreadyPaused");
        for (const auto pair : {qMakePair("yes", int(QMessageBox::Yes)), qMakePair("no", int(QMessageBox::No)), qMakePair("cancel", int(QMessageBox::Cancel))}) for (bool pause : {false, true}) QTest::newRow(qPrintable(QString::fromLatin1(pair.first) + (pause ? "-paused" : "-running"))) << pair.second << pause;
    }
    void cancelConfirmation() {
        QFETCH(int, answer); QFETCH(bool, alreadyPaused); ProgressDialog dialog("Extract", "fixture.7z"); dialog.setPauseAvailable(true); dialog.setPaused(alreadyPaused); dialog.show();
        connect(&dialog, &ProgressDialog::pauseRequested, &dialog, &ProgressDialog::setPaused); QSignalSpy pause(&dialog, &ProgressDialog::pauseRequested), cancel(&dialog, &ProgressDialog::cancelRequested);
        bool seen = false; QTimer response; response.setInterval(1); connect(&response, &QTimer::timeout, &dialog, [&] {
            auto question = dialog.findChild<QMessageBox *>("progressCancelConfirmation"); if (!question || seen) return; seen = true;
            QTimer::singleShot(500, question, [question] { question->done(QMessageBox::Cancel); });
            QCOMPARE(question->text(), UiLanguage::resource(448)); question->button(QMessageBox::StandardButton(answer))->click();
        }); response.start(); dialog.findChild<QPushButton *>("cancelOperation")->click(); response.stop(); QVERIFY(seen);
        QCOMPARE(cancel.size(), answer == QMessageBox::Yes ? 1 : 0); QCOMPARE(pause.size(), alreadyPaused ? 0 : 2);
        if (!alreadyPaused) { QVERIFY(pause[0][0].toBool()); QVERIFY(!pause[1][0].toBool()); }
        QCOMPARE(dialog.findChild<QPushButton *>("pauseOperation")->text(), UiLanguage::resource(alreadyPaused ? 411 : 446));
    }
    void completionDuringConfirmation() {
        ProgressDialog dialog("Extract", "fixture.7z"); dialog.setPauseAvailable(true); connect(&dialog, &ProgressDialog::pauseRequested, &dialog, &ProgressDialog::setPaused); QSignalSpy cancelled(&dialog, &ProgressDialog::cancelRequested);
        QTimer response; response.setInterval(1); bool seen = false; connect(&response, &QTimer::timeout, &dialog, [&] {
            auto question = dialog.findChild<QMessageBox *>("progressCancelConfirmation"); if (!question || seen) return; seen = true; dialog.finishFile({}); question->button(QMessageBox::Yes)->click();
        }); response.start(); dialog.findChild<QPushButton *>("cancelOperation")->click(); response.stop(); QVERIFY(seen); QVERIFY(cancelled.isEmpty()); QCOMPARE(dialog.findChild<QPushButton *>("cancelOperation")->text(), UiLanguage::resource(408));
    }
    void backgroundChildAndCancel() {
        const auto root = temporary.filePath("priority"); QVERIFY(QDir().mkpath(root));
        const auto program = QCoreApplication::applicationDirPath() + "/progress_priority_engine"; QVERIFY(QFileInfo(program).isExecutable()); write(root + "/source.txt", "owned source");
        SevenZipProcessBackend backend(program); ProgressDialog dialog("Add", root + "/output.7z"); dialog.show();
        connect(&dialog, &ProgressDialog::backgroundRequested, &backend, &SevenZipProcessBackend::setBackground); connect(&dialog, &ProgressDialog::pauseRequested, &backend, &SevenZipProcessBackend::setPaused); connect(&dialog, &ProgressDialog::cancelRequested, &backend, &SevenZipProcessBackend::cancel);
        connect(&backend, &ArchiveBackend::pausedChanged, &dialog, &ProgressDialog::setPaused); connect(&backend, &ArchiveBackend::pauseAvailabilityChanged, &dialog, &ProgressDialog::setPauseAvailable); connect(&backend, &ArchiveBackend::finished, &dialog, &ProgressDialog::finish);
        QSignalSpy done(&backend, &ArchiveBackend::finished); ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/output.7z"; request.workingDirectory = root; request.files = {"source.txt"}; backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(root + "/pid"), 10000);
        const auto pid = read(root + "/pid").toUInt(); QVERIFY(pid > 0); QTRY_VERIFY(backend.canPause()); const int before = ::getpriority(PRIO_DARWIN_PROCESS, 0);
        dialog.findChild<QPushButton *>("backgroundOperation")->click();
        QTRY_COMPARE_WITH_TIMEOUT(read(root + "/priority"), QString("1"), 5000); QCOMPARE(::getpriority(PRIO_DARWIN_PROCESS, 0), 1); QCOMPARE(dialog.findChild<QPushButton *>("backgroundOperation")->text(), UiLanguage::resource(445));
        QVERIFY(backend.setPaused(true)); dialog.findChild<QPushButton *>("backgroundOperation")->click(); QCOMPARE(childBackground(pid), 0); QCOMPARE(::getpriority(PRIO_DARWIN_PROCESS, 0), before);
        QTimer response; response.setInterval(1); connect(&response, &QTimer::timeout, &dialog, [&] { if (auto question = dialog.findChild<QMessageBox *>("progressCancelConfirmation")) question->button(QMessageBox::Yes)->click(); }); response.start(); dialog.findChild<QPushButton *>("cancelOperation")->click(); response.stop(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000); QVERIFY(qvariant_cast<ArchiveResult>(done.last()[0]).cancelled); QVERIFY(QFileInfo::exists(root + "/source.txt"));
        // Verification helpers own their child on a worker thread. Priority
        // changes must also be applied while that child is SIGSTOP-paused.
        QVERIFY(QFile::remove(root + "/pid")); QVERIFY(QFile::remove(root + "/priority"));
        auto control = std::make_shared<OperationControl>();
        auto worker = QtConcurrent::run([=] { return WorkerProcess::run(program, {root}, {}, control, {}, 10000); });
        const auto reap = qScopeGuard([&] { control->cancel(); worker.waitForFinished(); });
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(root + "/pid"), 10000); const auto workerPid = read(root + "/pid").toUInt(); QVERIFY(workerPid > 0);
        control->setBackground(true); QTRY_COMPARE(childBackground(workerPid), 1);
        control->setPaused(true); QTest::qWait(150); control->setBackground(false); QTRY_COMPARE(childBackground(workerPid), 0);
        control->cancel(); QTRY_VERIFY_WITH_TIMEOUT(worker.isFinished(), 10000); QVERIFY(worker.result().cancelled); QCOMPARE(::getpriority(PRIO_DARWIN_PROCESS, 0), before);
    }
    void genuineCallbackErrors() {
        const auto file = temporary.filePath("crc 日本語.txt"); write(file, "CRC fixture contents"); const auto zip = temporary.filePath("crc.zip"); QProcess create; create.start(executable, {"a", "-tzip", "-mx0", zip, file}); QVERIFY(create.waitForFinished(10000)); QCOMPARE(create.exitCode(), 0);
        QFile archive(zip); QVERIFY(archive.open(QIODevice::ReadWrite)); auto bytes = archive.readAll(); const auto where = bytes.indexOf("CRC fixture contents"); QVERIFY(where >= 0); bytes[where] ^= 1; archive.seek(0); QCOMPARE(archive.write(bytes), qint64(bytes.size())); archive.close();
        SevenZipProcessBackend backend(executable); ProgressDialog dialog("Test", zip); connect(&backend, &ArchiveBackend::operationError, &dialog, &ProgressDialog::reportError); connect(&backend, &ArchiveBackend::progressDetails, &dialog, &ProgressDialog::updateDetails); connect(&backend, &ArchiveBackend::finished, &dialog, &ProgressDialog::finish);
        QSignalSpy errors(&backend, &ArchiveBackend::operationError), done(&backend, &ArchiveBackend::finished); ArchiveRequest request; request.operation = ArchiveOperation::Test; request.archive = zip; backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000); QVERIFY(!qvariant_cast<ArchiveResult>(done.last()[0]).success); QVERIFY(!errors.isEmpty());
        const auto error = qvariant_cast<ArchiveOperationError>(errors[0][0]); QCOMPARE(error.code, 3); QVERIFY(error.fileName.contains("日本語")); QVERIFY(error.archive.endsWith("crc.zip"));
        auto messages = dialog.findChild<QTreeWidget *>("progressMessages"); QVERIFY(messages->topLevelItemCount() >= 1); QVERIFY(messages->topLevelItem(0)->text(1).contains(UiLanguage::resource(3723))); QCOMPARE(dialog.findChild<QLabel *>("progressErrors")->text(), QString("1"));
    }
};
int main(int argc, char **argv) {
    if (argc < 2) return 2; executable = argv[1]; QLocale::setDefault(QLocale(QLocale::English)); const auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("ProgressPresentation"); app.setQuitOnLastWindowClosed(false);
    ProgressPresentationTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "progress-presentation.moc"
