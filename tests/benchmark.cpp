// SPDX-License-Identifier: LGPL-3.0-or-later
#include "Benchmark.h"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QScopeGuard>
#include <sys/types.h>
#include <signal.h>
#include <cerrno>
#include <limits>
static QString executable;
class BenchmarkTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    QJsonObject resultValue(quint64 speed = 2, quint64 size = 42ULL << 20) {
        return {{"defined", true}, {"speed", QString::number(speed)}, {"usage", "100"}, {"rating", "2"}, {"rpu", "2"}, {"size", QString::number(size)},
            {"speedText", QString::number(speed) + " KB/s"}, {"usageText", "100%"}, {"ratingText", "0.002 GIPS"}, {"rpuText", "0.002 GIPS"}, {"sizeText", QString::number(size >> 20) + " MB"}};
    }
    QJsonObject event(int completed, bool finished) {
        const auto value = resultValue();
        return {{"version", 1}, {"mode", "benchmark"}, {"completed", QString::number(completed)}, {"finished", finished},
            {"encCurrent", value}, {"decCurrent", value}, {"encResult", value}, {"decResult", value}, {"total", value}, {"log", "1T Frequency (MHz):\n 3000\nCompr Decompr Total   CPU\n0.002  0.002  0.002 100%"}};
    }
    void write(const QString &path, const QByteArray &data, bool executable = false) {
        QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), qint64(data.size())); file.close();
        if (executable) QVERIFY(file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    }
    static QByteArray literal(const QString &text) { const auto json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact); return json.mid(1, json.size() - 2); }
    QString fake(const QString &name, QJsonArray events, int exitCode = 0, int delay = 30) {
        const auto root = temp.filePath(name); QDir().mkpath(root);
        const auto python = QStandardPaths::findExecutable("python3");
        const auto engine = root + "/7zz"; write(engine, "#!" + python.toUtf8() + "\n", true);
        const auto json = QJsonDocument(events).toJson(QJsonDocument::Compact);
        const auto script = "#!" + python.toUtf8() + "\nimport json,os,pathlib,socket,sys,time\n" +
            "pathlib.Path(" + literal(root + "/arguments.json") + ").write_text(json.dumps(dict(args=sys.argv[1:],dictionary=os.environ.get('SEVENZIP_PORT_BENCHMARK_DICT'))))\n" +
            "s=socket.socket(fileno=3)\nevents=json.loads(" + literal(QString::fromUtf8(json)) + ")\n" +
            "for e in events:\n time.sleep(" + QByteArray::number(delay) + "/1000)\n s.send(json.dumps(e).encode())\n" +
            "print('日本語 benchmark fixture diagnostic')\nsys.exit(" + QByteArray::number(exitCode) + ")\n";
        write(root + "/7zz-progress", script, true); return engine;
    }
private slots:
    void initTestCase() {
        QVERIFY(temp.isValid()); QVERIFY(!QStandardPaths::findExecutable("python3").isEmpty());
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.filePath("preferences"));
    }
    void nativeDictionaryCurrentAndAccumulation() {
        BenchmarkRunner runner(executable); QSignalSpy done(&runner, &BenchmarkRunner::finished), passes(&runner, &BenchmarkRunner::result), snapshots(&runner, &BenchmarkRunner::snapshot);
        bool inPass = false; int ticks = 0;
        connect(&runner, &BenchmarkRunner::snapshot, &runner, [&](const BenchmarkSnapshot &value) { if (!value.completed && value.encCurrent.defined && !value.encResult.defined) inPass = true; });
        QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; }); heartbeat.start();
        runner.startDictionary(384 * 1024, 1, 2); QVERIFY(runner.busy());
        QVERIFY2(done.wait(75000), "Official benchmark did not finish");
        QVERIFY2(done.last()[0].toString().isEmpty(), qPrintable(done.last()[0].toString()));
        QVERIFY(inPass); QVERIFY(ticks > 10); QCOMPARE(passes.size(), 2); QVERIFY(!runner.busy());
        QVERIFY(!snapshots.isEmpty()); const auto final = qvariant_cast<BenchmarkSnapshot>(snapshots.last()[0]);
        QCOMPARE(final.completed, 2); QVERIFY(final.finished); QVERIFY(final.encResult.defined); QVERIFY(final.decResult.defined); QVERIFY(final.total.defined);
        QVERIFY(final.encCurrent.speed > 0); QVERIFY(final.decCurrent.speed > 0); QVERIFY(final.encResult.size > final.encCurrent.size);
        QCOMPARE(final.encCurrent.size % (448 * 1024), quint64(0)); // actual 384 KiB dictionary + official 64 KiB input padding
        QVERIFY(final.encCurrent.size % (320 * 1024)); // cannot be the former 256 KiB console sweep
        QVERIFY(final.log.contains("Frequency (MHz):")); QVERIFY(final.log.contains("Compr Decompr Total   CPU")); QVERIFY(final.total.display[4].endsWith(" GIPS"));
        QVERIFY(final.system.value("version").toString().startsWith("7-Zip 26.03")); QVERIFY(!final.system.value("features").toString().isEmpty()); QVERIFY(!final.system.value("hardware").toString().isEmpty());
        BenchmarkSnapshot firstPass;
        for (const auto &row : snapshots) { const auto value = qvariant_cast<BenchmarkSnapshot>(row[0]); if (value.completed == 1) { firstPass = value; break; } }
        QVERIFY(firstPass.encResult.defined); QCOMPARE(final.encResult.size, firstPass.encResult.size + final.encCurrent.size);
    }
    void officialMemoryArithmetic() {
        const auto tool = QFileInfo(executable).absolutePath() + "/7zip-benchmark-tests";
        for (quint64 dictionary : {quint64(1) << 18, quint64(384) << 10, quint64(3) << 20, quint64(32) << 20, quint64(1) << 32}) {
            for (int threads : {1, 2, 4, 12}) {
                QProcess process; process.start(tool, {"--memory", QString::number(dictionary), QString::number(threads)});
                QVERIFY(process.waitForStarted()); QVERIFY(process.waitForFinished(5000)); QCOMPARE(process.exitCode(), 0);
                bool valid; const auto native = process.readAllStandardOutput().trimmed().toULongLong(&valid); QVERIFY(valid);
                QCOMPARE(BenchmarkRunner::memoryUsage(dictionary, threads), native);
            }
        }
    }
    void protocolPrecisionAndInvalidRecords() {
        auto valid = event(1, true); valid["encCurrent"] = resultValue(9007199254740993ULL, std::numeric_limits<quint64>::max());
        auto floating = valid; auto number = floating["encCurrent"].toObject(); number["speed"] = double(9007199254740993ULL); floating["encCurrent"] = number;
        auto overflow = valid; number = overflow["encCurrent"].toObject(); number["size"] = "18446744073709551616"; overflow["encCurrent"] = number;
        auto incomplete = valid; incomplete["completed"] = "0";
        BenchmarkRunner runner(fake("protocol", {floating, overflow, incomplete, valid}));
        QSignalSpy done(&runner, &BenchmarkRunner::finished), snapshots(&runner, &BenchmarkRunner::snapshot);
        runner.startDictionary(384 * 1024, 1, 1); QVERIFY(done.wait(5000)); QVERIFY2(done.last()[0].toString().isEmpty(), qPrintable(done.last()[0].toString()));
        QCOMPARE(snapshots.size(), 1); const auto result = qvariant_cast<BenchmarkSnapshot>(snapshots.last()[0]);
        QCOMPARE(result.encCurrent.speed, quint64(9007199254740993ULL)); QCOMPARE(result.encCurrent.size, std::numeric_limits<quint64>::max());
        QFile arguments(temp.filePath("protocol/arguments.json")); QVERIFY(arguments.open(QIODevice::ReadOnly));
        const auto params = QJsonDocument::fromJson(arguments.readAll()).object(); QCOMPARE(params["dictionary"].toString(), QString::number(384 * 1024));
        QVERIFY(params["args"].toArray().contains("-md384k")); QVERIFY(params["args"].toArray().contains("-mmt=1"));
    }
    void failureAndRecovery() {
        const auto program = fake("failure", {}, 2); BenchmarkRunner runner(program); QSignalSpy done(&runner, &BenchmarkRunner::finished);
        runner.startDictionary(384 * 1024, 1, 1); QVERIFY(done.wait(5000)); QVERIFY(!runner.busy());
        QVERIFY(done.last()[0].toString().contains("exit code: 2")); QVERIFY(done.last()[0].toString().contains("日本語"));
        fake("failure", {event(1, true)}); done.clear(); runner.startDictionary(384 * 1024, 1, 1); QVERIFY(done.wait(5000)); QVERIFY(done.last()[0].toString().isEmpty());
        BenchmarkRunner empty(fake("empty", {})); QSignalSpy missing(&empty, &BenchmarkRunner::finished);
        empty.startDictionary(384 * 1024, 1, 1); QVERIFY(missing.wait(5000)); QVERIFY(missing.last()[0].toString().contains("Incomplete benchmark"));
        BenchmarkRunner noExecutable(temp.filePath("missing")); QSignalSpy noStart(&noExecutable, &BenchmarkRunner::finished);
        noExecutable.start(18, 1, 1); QVERIFY(noStart.wait(5000)); QVERIFY(noStart.last()[0].toString().contains("Cannot start"));
    }
    void parametersAndGui() {
        QSettings settings; settings.clear(); settings.setValue("Benchmark/DictionaryBytes", 384 * 1024); settings.setValue("Benchmark/Threads", 1); settings.setValue("Benchmark/Passes", 3);
        auto complete = event(3, true); complete["encResult"] = resultValue(7, 126ULL << 20);
        BenchmarkDialog dialog(fake("gui", {complete})); dialog.show();
        auto dictionary = dialog.findChild<QComboBox *>("benchmarkDictionary"), passes = dialog.findChild<QComboBox *>("benchmarkPasses");
        QCOMPARE(dictionary->currentData().toULongLong(), quint64(384 * 1024)); QVERIFY(dictionary->findData(QVariant::fromValue(quint64(768 * 1024))) >= 0);
        QVERIFY(dictionary->findData(QVariant::fromValue(quint64(3) << 20)) >= 0); QCOMPARE(dictionary->itemData(dictionary->count() - 1).toULongLong(), quint64(1) << 32);
        QCOMPARE(passes->currentData().toInt(), 3); auto runner = dialog.findChild<BenchmarkRunner *>(); QVERIFY(runner);
        QSignalSpy done(runner, &BenchmarkRunner::finished); QVERIFY(done.wait(5000)); QVERIFY(done.last()[0].toString().isEmpty());
        QCOMPARE(dialog.findChild<QLabel *>("benchmarkCurrentSize0")->text(), QString("42 MB"));
        QCOMPARE(dialog.findChild<QLabel *>("benchmarkResultSize0")->text(), QString("126 MB"));
        QCOMPARE(dialog.findChild<QLabel *>("benchmarkCurrent0")->text(), QString("2 KB/s"));
        QCOMPARE(dialog.findChild<QLabel *>("benchmarkResult0")->text(), QString("7 KB/s"));
        QCOMPARE(dialog.findChild<QLabel *>("benchmarkTotalRating")->text(), QString("0.002 GIPS"));
        QCOMPARE(dialog.findChild<QLabel *>("benchmarkTotalUsage")->text(), QString("100%"));
        QVERIFY(dialog.findChild<QLabel *>("benchmarkElapsed")->text().contains('.'));
        QVERIFY(dialog.findChild<QPlainTextEdit *>("benchmarkLog")->toPlainText().contains("Frequency (MHz)"));
        auto restart = dialog.findChild<QPushButton *>("benchmarkRestart"); done.clear(); QTest::mouseClick(restart, Qt::LeftButton); QVERIFY(done.wait(5000)); QVERIFY(done.last()[0].toString().isEmpty());
        QTest::mouseClick(dialog.findChild<QPushButton *>("benchmarkCancel"), Qt::LeftButton); QVERIFY(!dialog.isVisible());
        BenchmarkRunner invalid(executable); QSignalSpy bad(&invalid, &BenchmarkRunner::finished); invalid.startDictionary(384 * 1024 + 1, 1, 1); QCOMPARE(bad.size(), 1); QVERIFY(!invalid.busy());
    }
    void stopWhileRunningAndClose() {
        QSettings settings; settings.clear(); settings.setValue("Benchmark/DictionaryBytes", 384 * 1024); settings.setValue("Benchmark/Threads", 1);
        const auto program = fake("close", {event(1, true)}, 0, 10000);
        auto dialog = new BenchmarkDialog(program); dialog->show(); auto runner = dialog->findChild<BenchmarkRunner *>();
        QTRY_VERIFY_WITH_TIMEOUT(runner->busy(), 3000);
        const auto marker = temp.filePath("close/arguments.json"); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(marker), 3000);
        QSignalSpy done(runner, &BenchmarkRunner::finished);
        QTest::mouseClick(dialog->findChild<QPushButton *>("benchmarkStop"), Qt::LeftButton);
        QVERIFY(done.wait(5000)); QVERIFY(runner->stopped()); QVERIFY(!runner->busy());
        auto restart = dialog->findChild<QPushButton *>("benchmarkRestart"); QTest::mouseClick(restart, Qt::LeftButton); QVERIFY(runner->busy());
        // Closing a live dialog must disconnect callbacks before member teardown.
        delete dialog;
    }
    void upstreamDefaults() {
        QSettings().clear(); BenchmarkDialog dialog(fake("defaults", {}));
        QCOMPARE(dialog.findChild<QComboBox *>("benchmarkPasses")->currentData().toInt(), 10);
        const auto dictionary = dialog.findChild<QComboBox *>("benchmarkDictionary");
        QCOMPARE(dictionary->itemText(dictionary->findData(QVariant::fromValue(quint64(1) << 20))), QString("1024 KB"));
        const auto threads = dialog.findChild<QComboBox *>("benchmarkThreads")->currentData().toInt();
        QVERIFY(threads == 1 || !(threads & 1));
    }
};
int main(int argc, char **argv) {
    QApplication::setDesktopSettingsAware(false); const auto plugins = qgetenv("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({QString::fromUtf8(plugins)});
    QApplication app(argc, argv); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("BenchmarkTests");
    if (argc < 2) return 2; executable = QString::fromLocal8Bit(argv[1]); --argc; for (int n = 1; n < argc; ++n) argv[n] = argv[n + 1];
    BenchmarkTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "benchmark.moc"
