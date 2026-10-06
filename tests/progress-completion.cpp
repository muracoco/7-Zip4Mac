// SPDX-License-Identifier: LGPL-3.0-or-later
#include "progress-dialog-driver.h"
#include "Dialogs.h"
#include "MainWindow.h"
#include "UiLanguage.h"
#include "PortStyle.h"
#include "ResourceDialogs.h"
#include <QAction>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

static QString executable;
class ProgressCompletionTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    static void write(const QString &path, const QByteArray &bytes) { QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly)); QCOMPARE(f.write(bytes), qint64(bytes.size())); }
    static void language(OfficialProgressState &state) { state.setLanguage("Testing", "Pause", "Continue", "Paused", "Background", "Foreground"); }
    ArchiveResult execute(SevenZipProcessBackend &backend, const ArchiveRequest &request) {
        QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(30000)) qFatal("Owned completion fixture timed out"); return qvariant_cast<ArchiveResult>(done.last()[0]);
    }
private slots:
    void initTestCase() { QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("prefs")); }
    void init() { QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); UiLanguage::set("en"); }
    void originalCompletionPolicy_data() {
        QTest::addColumn<QString>("error"); QTest::addColumn<QString>("ok"); QTest::addColumn<bool>("files"); QTest::addColumn<bool>("cancelled"); QTest::addColumn<bool>("ended"); QTest::addColumn<int>("notices");
        QTest::newRow("success-auto-close") << QString() << QString() << false << false << true << 0;
        QTest::newRow("test-ok-message") << QString() << QString("result") << false << false << true << 1;
        QTest::newRow("fatal-error-message") << QString("failure") << QString() << false << false << true << 1;
        QTest::newRow("file-errors-retain-close") << QString() << QString("ignored") << true << false << false << 0;
        QTest::newRow("fatal-and-file-errors") << QString("failure") << QString() << true << false << false << 1;
        QTest::newRow("cancelled-with-errors-close") << QString() << QString() << true << true << true << 0;
    }
    void originalCompletionPolicy() {
        QFETCH(QString, error); QFETCH(QString, ok); QFETCH(bool, files); QFETCH(bool, cancelled); QFETCH(bool, ended); QFETCH(int, notices);
        OfficialProgressState state; language(state); ArchiveProgress p; p.total = 100; p.completed = 70; p.current = "日本語.txt"; state.update(p, 1000);
        if (files) state.addError("CRC failure", 1000); state.finish(error, ok, "Testing", cancelled, 1200); const auto &view = state.view();
        QCOMPARE(view.ended, ended); QCOMPARE(view.waitForClose, !ended); QCOMPARE(view.finalNotices.size(), notices); QVERIFY(view.hidePause); QVERIFY(view.hideBackground); QVERIFY(view.cancelDefault); QCOMPARE(view.text.value(2), UiLanguage::resource(408)); QCOMPARE(view.barPosition, 70); QCOMPARE(view.text.value(102), QString("\n日本語.txt"));
        if (notices) { QCOMPARE(view.finalNotices.first().error, !error.isEmpty()); QCOMPARE(view.finalNotices.first().title, error.isEmpty() ? QString("Testing") : QString("7-Zip")); }
    }
    void originalTestAndHashText() {
        DecompressStatistics s; s.NumArchives = 2; s.PackSize = 1024; s.NumFolders = 3; s.NumFiles = 4; s.UnpackSize = 2048;
        QCOMPARE(officialTestResult(s), QString("Archives: 2\nPacked Size: 1024 bytes : 1 KiB\nFolders: 3\nFiles: 4\nSize: 2048 bytes : 2 KiB\n\nThere are no errors"));
        s.NumFolders = 0; s.NumFiles = 1; s.UnpackSize = 3;
        QCOMPARE(officialInsideTestResult(s, "日本語.txt"), QString("Name: 日本語.txt\nSize: 3 bytes\n\nThere are no errors\n"));
        HashStatistics h; h.NumFiles = 1; h.FirstFileName = "日本語.txt"; h.FilesSize = 3; h.hashers.append({"SHA256", {"AABBCC", "names", "streams"}});
        const auto view = officialHashResults(h); QCOMPARE(view.title, UiLanguage::resource(7501)); QVERIFY(view.deleteAllowed); QVERIFY(!view.selectFirst); QCOMPARE(view.columns, 2u); QCOMPARE(view.rows.last(), qMakePair(QString("SHA256"), QString("AABBCC")));
        h.NumDirs = 1; h.NumFiles = 2; h.NumAltStreams = 1; h.AltStreamsSize = 10; h.NumErrors = 1;
        const auto multiple = officialHashResults(h, s); QCOMPARE(multiple.rows.first().first, UiLanguage::resource(3907)); QVERIFY(multiple.rows.contains(qMakePair(UiLanguage::resource(7503).replace("CRC", "SHA256").remove(':'), QString("names")))); QVERIFY(multiple.rows.contains(qMakePair(UiLanguage::resource(7504).replace("CRC", "SHA256").remove(':'), QString("streams"))));
        UiLanguage::set("ja"); QVERIFY(officialTestResult(s).contains(UiLanguage::resource(3001))); QVERIFY(officialHashResults(h).rows.first().first == UiLanguage::resource(1070));
    }
    void checksumSelectionCopyDeleteAndDetail() {
        ChecksumPresentation view; view.title = UiLanguage::resource(7501); view.columns = 2; view.deleteAllowed = true; view.selectFirst = false;
        const auto raw = QString(1100, 'a') + "\nraw value"; view.rows = {{"SHA256", raw}, {"CRC", "1234"}};
        ChecksumResultsDialog dialog(view); dialog.show(); auto tree = dialog.findChild<QTreeWidget *>("checksumResults");
        QCOMPARE(tree->selectedItems().size(), 0); QVERIFY(dialog.copyText().isEmpty()); QCOMPARE(tree->topLevelItem(0)->text(1), raw.left(1024) + " ...");
        tree->setCurrentItem(tree->topLevelItem(0)); tree->topLevelItem(0)->setSelected(true); QCOMPARE(dialog.copyText(), QString("SHA256: ") + raw + '\n');
        bool seen = false; QTimer response; response.setInterval(10); connect(&response, &QTimer::timeout, &dialog, [&] {
            auto detail = dialog.findChild<QDialog *>("checksumItemInfo"); if (!detail || !detail->isVisible()) return;
            seen = true; auto text = detail->findChild<QPlainTextEdit *>("checksumItemText"); QVERIFY(text->isReadOnly()); QCOMPARE(text->toPlainText(), raw); QVERIFY(detail->findChild<ResourceDialogLayout *>()); detail->accept();
        }); response.start(); QKeyEvent enter(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier); QApplication::sendEvent(tree, &enter); response.stop(); QVERIFY(seen);
        auto remove = dialog.findChild<QAction *>("deleteChecksumRows"); QVERIFY(remove); remove->trigger(); QCOMPARE(tree->topLevelItemCount(), 1); QCOMPARE(tree->topLevelItem(0)->text(0), QString("CRC")); QCOMPARE(view.rows.size(), 2);
        const auto before = tree->size(); dialog.resize(dialog.width() + 180, dialog.height() + 100); QApplication::processEvents(); QCOMPARE(tree->size() - before, QSize(180, 100));
        UiLanguage::set("ja"); UiLanguage::apply(&dialog); QCOMPARE(dialog.windowTitle(), UiLanguage::resource(7501));
    }
    void actualEngineResults_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void actualEngineResults() {
        QFETCH(QString, format); const auto root = temporary.filePath(format); QVERIFY(QDir().mkpath(root + "/input/empty")); const QByteArray data(32 * 1024 * 1024 + 17, 'a');
        write(root + "/input/日本語 space.txt", data); write(root + "/input/ascii.txt", "abc"); SevenZipProcessBackend backend(executable);
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/fixture." + format; request.workingDirectory = root; request.files = {"input"}; request.format = format; request.method = format == "zip" ? "Deflate" : "LZMA2"; request.level = 1;
        auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        request.operation = ArchiveOperation::Test; request.files.clear(); result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.completion.decompression); const auto statistics = *result.completion.decompression;
        QCOMPARE(statistics.NumArchives, quint64(1)); QCOMPARE(statistics.NumFiles, quint64(2)); QCOMPARE(statistics.NumFolders, quint64(2)); QCOMPARE(statistics.UnpackSize, quint64(data.size() + 3)); QCOMPARE(statistics.PackSize, quint64(QFileInfo(request.archive).size()));
        ProgressDialogDriver driver; ProgressDialog test("Test", request.archive); test.show(); test.finish(result); QVERIFY(!test.isVisible()); QCOMPARE(driver.notices.size(), 1); QCOMPARE(driver.notices.first().text, officialTestResult(statistics)); QVERIFY(!driver.notices.first().error);
        request.operation = ArchiveOperation::Hash; request.files = {"input/ascii.txt"}; request.hashMethod = "SHA256"; result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.completion.hash); QCOMPARE(result.completion.hash->NumFiles, quint64(1)); QCOMPARE(result.completion.hash->FilesSize, quint64(3));
        const auto digest = QString::fromLatin1(QCryptographicHash::hash("abc", QCryptographicHash::Sha256).toHex()); QCOMPARE(result.completion.hash->hashers.first().groups[0], digest);
        ProgressDialog hash("CRC SHA", root); hash.show(); hash.finish(result); QVERIFY(!hash.isVisible()); QCOMPARE(driver.checksums.size(), 1); QVERIFY(driver.checksums.first().rows.contains(qMakePair(QString("SHA256"), digest)));
        request.operation = ArchiveOperation::HashArchive; request.files = {"input/ascii.txt"}; result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.completion.hash); QVERIFY(result.completion.decompression); QCOMPARE(result.completion.hash->hashers.first().groups[0], digest);
        request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/out"; request.overwriteMode = "overwrite"; result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); ProgressDialog extract("Extract", request.archive); extract.show(); driver.clear(); extract.finish(result); QVERIFY(!extract.isVisible()); QVERIFY(driver.notices.isEmpty()); QVERIFY(QFileInfo::exists(root + "/out/input/日本語 space.txt"));
    }
    void actualInsideTestSummary() {
        const auto root = temporary.filePath("inside"); QVERIFY(QDir().mkpath(root)); write(root + "/日本語 space.txt", "abc"); QVERIFY(QDir().mkpath(root + "/folder")); write(root + "/folder/nested 日本語.txt", "xyz");
        SevenZipProcessBackend builder(executable); ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/inside.7z"; request.workingDirectory = root; request.files = {"日本語 space.txt", "folder"}; QVERIFY(execute(builder, request).success);
        ProgressDialogDriver driver; MainWindow window(executable); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto tree = window.findChild<FileList *>("fileList"); const auto rows = tree->findItems("日本語 space.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 1); tree->setCurrentItem(rows.first()); rows.first()->setSelected(true);
        auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished); window.findChild<QAction *>("testAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000);
        const auto result = qvariant_cast<ArchiveResult>(done.first()[0]); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.testInside); QVERIFY(result.completion.hash); QVERIFY(result.completion.hash->hashers.isEmpty()); QCOMPARE(driver.notices.size(), 1); QCOMPARE(driver.notices.first().text, QString("Name: 日本語 space.txt\nSize: 3 bytes\n\nThere are no errors\n")); QVERIFY(driver.checksums.isEmpty());
        for (auto progress : window.findChildren<ProgressDialog *>()) QVERIFY(!progress->isVisible());
        const auto folder = tree->findItems("folder", Qt::MatchExactly); QCOMPARE(folder.size(), 1); tree->clearSelection(); tree->setCurrentItem(folder.first()); folder.first()->setSelected(true);
        done.clear(); window.findChild<QAction *>("hashSHA256Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000); QVERIFY(qvariant_cast<ArchiveResult>(done.first()[0]).success); QCOMPARE(driver.checksums.size(), 1); QVERIFY(driver.checksums.first().rows.contains(qMakePair(QString("Name"), QString("folder"))));
        window.findChild<QAction *>("openAction")->trigger(); const auto nested = tree->findItems("nested 日本語.txt", Qt::MatchExactly); QCOMPARE(nested.size(), 1); tree->clearSelection(); tree->setCurrentItem(nested.first()); nested.first()->setSelected(true);
        done.clear(); window.findChild<QAction *>("testAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000); QVERIFY(qvariant_cast<ArchiveResult>(done.first()[0]).success); QCOMPARE(driver.notices.size(), 2); QVERIFY(driver.notices.last().text.startsWith("Name: nested 日本語.txt\n"));
    }
    void fatalAndFileErrorsAndParentTitle() {
        ProgressDialogDriver driver; QWidget parent; parent.setWindowTitle("7-Zip File Manager"); parent.show();
        ProgressDialog failure("Extract", "fixture.7z", &parent); failure.show(); ArchiveProgress p; p.total = 100; p.completed = 50; failure.updateDetails(p); QVERIFY(parent.windowTitle().startsWith("50% "));
        failure.reportError({3, false, "file.txt", {}, {}}); ArchiveResult r; r.operation = ArchiveOperation::Extract; r.message = "failure"; r.exitCode = 2; failure.finish(r); QVERIFY(failure.isVisible()); QVERIFY(driver.notices.isEmpty()); QVERIFY(parent.windowTitle().startsWith("50% ")); failure.findChild<QPushButton *>("cancelOperation")->click(); QCOMPARE(parent.windowTitle(), QString("7-Zip File Manager"));
        ProgressDialog fatal("Extract", "missing.7z", &parent); fatal.show(); r.message = "Cannot open archive"; r.target = "missing.7z"; fatal.finish(r); QVERIFY(!fatal.isVisible()); QCOMPARE(driver.notices.size(), 1); QVERIFY(driver.notices.first().error); QVERIFY(driver.notices.first().text.contains("7-Zip exit code: 2")); QCOMPARE(parent.windowTitle(), QString("7-Zip File Manager"));
        ProgressDialog success("Add", "fixture.7z"); success.show(); success.finishFile({}); QVERIFY(!success.isVisible());
        ProgressDialog cancelled("Add", "fixture.7z"); cancelled.show(); cancelled.finishFile("Operation cancelled"); QVERIFY(!cancelled.isVisible()); QCOMPARE(driver.notices.size(), 1);
    }
    void invalidCompletionRecordIsRejected() {
        const auto path = temporary.filePath("bad-completion.json"); write(path, "{\"version\":1,\"decompression\":{\"NumArchives\":\"18446744073709551616\"},\"hash\":null}"); CompletionData data; QString error; QVERIFY(!readCompletionData(path, data, error)); QVERIFY(!error.isEmpty()); QVERIFY(!data.decompression);
    }
    void passwordAttemptEndsWithoutFinalErrorOrCancelQuestion() {
        ProgressDialogDriver driver; QWidget parent; parent.setWindowTitle("7-Zip File Manager"); parent.show();
        ProgressDialog attempt("Extract", "encrypted.zip", &parent); attempt.show();
        ArchiveProgress progress; progress.total = 100; progress.completed = 50; attempt.updateDetails(progress);
        attempt.append("Enter password:\n"); attempt.finishForRetry();
        QVERIFY(!attempt.isVisible()); QVERIFY(driver.notices.isEmpty()); QCOMPARE(parent.windowTitle(), QString("7-Zip File Manager"));
        ProgressDialog retry("Extract", "encrypted.zip", &parent); retry.show();
        ArchiveResult result; result.operation = ArchiveOperation::Extract; result.success = true; retry.finish(result);
        QVERIFY(!retry.isVisible()); QVERIFY(driver.notices.isEmpty());
    }
};
int main(int argc, char **argv) {
    if (argc < 2) return 2; executable = argv[1]; QLocale::setDefault(QLocale(QLocale::English)); const auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("ProgressCompletion"); app.setQuitOnLastWindowClosed(false);
    ProgressCompletionTests tests; QList<char *> args{argv[0]}; for (int i = 2; i < argc; ++i) args << argv[i]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "progress-completion.moc"
