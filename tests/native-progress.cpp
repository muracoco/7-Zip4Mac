#include "progress-dialog-driver.h"
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ArchiveBackend.h"
#include "Dialogs.h"
#include "UiLanguage.h"
#include "ProgressText.h"
#include "WorkerProcess.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QtConcurrent>
#include <sys/socket.h>
#include <unistd.h>
#include <limits>
static QString executable;
class NativeProgressTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    ProgressDialogDriver resultDialogs;
    ArchiveResult execute(SevenZipProcessBackend &backend, const ArchiveRequest &request) {
        QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request); if (done.isEmpty() && !done.wait(30000)) qFatal("7-Zip timed out"); return qvariant_cast<ArchiveResult>(done.last().first());
    }
    static ArchiveProgress last(const QSignalSpy &spy, const QString &mode) {
        for (int n = spy.size() - 1; n >= 0; --n) { const auto snapshot = qvariant_cast<ArchiveProgress>(spy[n].first()); if (snapshot.mode == mode) return snapshot; } return {};
    }
    void write(const QString &path, const QByteArray &data) { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(data), qint64(data.size())); }
private slots:
    void init() { resultDialogs.clear(); }
    void initTestCase() { QVERIFY(temp.isValid()); }
    void callbacksAndRoundTrip_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void callbacksAndRoundTrip() {
        QFETCH(QString, format); const auto root = temp.filePath(format); QVERIFY(QDir().mkpath(root + "/empty"));
        const QByteArray payload(32 * 1024 * 1024 + 37, 'a'); write(root + "/日本語 space.txt", payload); write(root + "/second.txt", "second");
        SevenZipProcessBackend backend(executable); QSignalSpy updates(&backend, &ArchiveBackend::progressDetails);
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/sample." + format; request.workingDirectory = root; request.files = {"日本語 space.txt", "second.txt", "empty"}; request.format = format; request.level = 1; request.method = format == "7z" ? "LZMA2" : "Deflate";
        auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); auto compressed = last(updates, "compress");
        // ZIP's official complexity includes headers; ratio input remains data bytes.
        QVERIFY(compressed.total.value_or(0) >= quint64(payload.size() + 6)); if (format == "7z") QCOMPARE(compressed.total.value_or(0), quint64(payload.size() + 6));
        QCOMPARE(compressed.processed().value_or(0), quint64(payload.size() + 6)); QCOMPARE(compressed.files.value_or(99), quint64(2)); QCOMPARE(compressed.doneFiles.value_or(99), quint64(2)); QVERIFY(compressed.packed().value_or(0) > 0); QVERIFY(compressed.packed().value() < compressed.processed().value());
        request.operation = ArchiveOperation::Test; request.files.clear(); updates.clear(); QVERIFY(execute(backend, request).success); auto tested = last(updates, "test"); QCOMPARE(tested.files.value_or(99), quint64(2)); QCOMPARE(tested.doneFiles.value_or(99), quint64(2)); QCOMPARE(tested.processed().value_or(0), quint64(payload.size() + 6));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = root + "/out"; request.overwriteMode = "overwrite"; updates.clear(); QVERIFY(execute(backend, request).success); auto extracted = last(updates, "extract"); QCOMPARE(extracted.processed().value_or(0), quint64(payload.size() + 6)); QCOMPARE(extracted.packed(), tested.packed()); QVERIFY(QFileInfo(request.outputDirectory + "/empty").isDir()); QFile restored(request.outputDirectory + "/日本語 space.txt"); QVERIFY(restored.open(QIODevice::ReadOnly)); QCOMPARE(QCryptographicHash::hash(restored.readAll(), QCryptographicHash::Sha256), QCryptographicHash::hash(payload, QCryptographicHash::Sha256));
        request.files = {"second.txt"}; request.outputDirectory = root + "/selected"; updates.clear(); QVERIFY(execute(backend, request).success); const auto selected = last(updates, "extract"); QCOMPARE(selected.files.value_or(99), quint64(1)); QCOMPARE(selected.doneFiles.value_or(99), quint64(1)); QCOMPARE(QFileInfo(request.outputDirectory + "/second.txt").size(), qint64(6)); QVERIFY(!QFileInfo::exists(request.outputDirectory + "/日本語 space.txt"));
        request.operation = ArchiveOperation::Hash; request.files = {"日本語 space.txt", "second.txt"}; updates.clear(); QVERIFY(execute(backend, request).success); const auto hashed = last(updates, "hash"); QCOMPARE(hashed.processed().value_or(0), quint64(payload.size() + 6)); QCOMPARE(hashed.doneFiles.value_or(99), quint64(2)); QVERIFY(!hashed.packed());
    }
    void encryptedAndFailureRecovery() {
        const auto root = temp.filePath("encrypted"); QVERIFY(QDir().mkpath(root)); write(root + "/暗号 file.txt", "protected data"); SevenZipProcessBackend backend(executable); QSignalSpy updates(&backend, &ArchiveBackend::progressDetails);
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/encrypted.7z"; request.workingDirectory = root; request.files = {"暗号 file.txt"}; request.password = "private fixture secret"; request.encryptNames = true;
        QVERIFY(execute(backend, request).success); const auto compressed = last(updates, "compress"); QVERIFY(!compressed.current.contains(request.password)); QCOMPARE(compressed.processed().value_or(0), quint64(14));
        request.operation = ArchiveOperation::Test; request.files.clear(); request.password = "wrong fixture"; auto result = execute(backend, request); QVERIFY(!result.success); QVERIFY(result.passwordRequired); QVERIFY(!result.details.contains(request.password));
        request.password = "private fixture secret"; QVERIFY(execute(backend, request).success); request.operation = ArchiveOperation::Extract; request.outputDirectory = root + "/out"; request.overwriteMode = "overwrite"; QVERIFY(execute(backend, request).success); QFile file(request.outputDirectory + "/暗号 file.txt"); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("protected data"));
    }
    void extractionFailureDialogAndReuse() {
        const auto root = temp.filePath("failed-extraction"); QVERIFY(QDir().mkpath(root)); const QByteArray payload("owned CRC failure payload");
        write(root + "/日本語 space.txt", payload); const auto good = root + "/good.7z", damaged = root + "/damaged.7z";
        QProcess creator; creator.setWorkingDirectory(root); creator.start(executable, {"a", "-mx0", "--", good, "日本語 space.txt"});
        QVERIFY(creator.waitForStarted()); QVERIFY(creator.waitForFinished(10000)); QCOMPARE(creator.exitCode(), 0);
        QFile file(good); QVERIFY(file.open(QIODevice::ReadOnly)); auto bytes = file.readAll(); const auto offset = bytes.indexOf(payload); QVERIFY(offset >= 0); bytes[offset] ^= 1; write(damaged, bytes);
        SevenZipProcessBackend backend(executable); ProgressDialog dialog("Extract", damaged); dialog.show();
        connect(&backend, &ArchiveBackend::output, &dialog, &ProgressDialog::append);
        connect(&backend, &ArchiveBackend::operationError, &dialog, &ProgressDialog::reportError);
        connect(&backend, &ArchiveBackend::progressDetails, &dialog, &ProgressDialog::updateDetails);
        connect(&backend, &ArchiveBackend::finished, &dialog, &ProgressDialog::finish);
        QSignalSpy cancel(&dialog, &ProgressDialog::cancelRequested);
        ArchiveRequest request; request.operation = ArchiveOperation::Extract; request.archive = damaged; request.outputDirectory = root + "/out"; request.overwriteMode = "overwrite";
        const auto result = execute(backend, request); QVERIFY(!result.success); QCOMPARE(result.exitCode, 2); QVERIFY(!backend.busy());
        auto close = dialog.findChild<QPushButton *>("cancelOperation"); QVERIFY(close); QVERIFY(close->isEnabled()); QCOMPARE(close->text(), UiLanguage::resource(408));
        auto messages = dialog.findChild<QPlainTextEdit *>(); QVERIFY(messages); QVERIFY(messages->toPlainText().contains("CRC Failed"));
        QVERIFY(messages->toPlainText().contains("7-Zip exit code: 2")); QVERIFY(dialog.findChild<QTreeWidget *>("progressMessages")->topLevelItemCount() > 0);
        QFile extracted(request.outputDirectory + "/日本語 space.txt"); QVERIFY(extracted.open(QIODevice::ReadOnly)); auto expected = payload; expected[0] ^= 1; QCOMPARE(extracted.readAll(), expected);
        QTest::mouseClick(close, Qt::LeftButton); QVERIFY(!dialog.isVisible()); QCOMPARE(cancel.size(), 0);
        disconnect(&backend, nullptr, &dialog, nullptr); request.operation = ArchiveOperation::Test; request.archive = good;
        const auto reused = execute(backend, request); QVERIFY2(reused.success, qPrintable(reused.message + reused.details));
    }
    void pauseCancelAndReuse() {
        const auto root = temp.filePath("cancel"); QVERIFY(QDir().mkpath(root)); QFile random("/dev/urandom"); QVERIFY(random.open(QIODevice::ReadOnly)); const auto payload = random.read(32 * 1024 * 1024); QCOMPARE(payload.size(), qsizetype(32 * 1024 * 1024)); write(root + "/large.bin", payload);
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished), snapshots(&backend, &ArchiveBackend::progressDetails); ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/cancelled.7z"; request.workingDirectory = root; request.files = {"large.bin"}; request.level = 9; request.threads = 1;
        backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(backend.canPause(), 10000); QVERIFY(backend.setPaused(true)); const auto before = snapshots.size(); int heartbeats = 0; QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, [&] { ++heartbeats; }); heartbeat.start(); QTest::qWait(250); QVERIFY(heartbeats >= 10); const auto drained = snapshots.size(); QTest::qWait(250); QCOMPARE(snapshots.size(), drained); QVERIFY(drained >= before);
        QVERIFY(backend.setPaused(false)); QTRY_VERIFY_WITH_TIMEOUT(snapshots.size() > drained, 10000); QVERIFY(backend.setPaused(true)); QTest::qWait(250); backend.cancel(); if (done.isEmpty()) QVERIFY(done.wait(10000)); auto result = qvariant_cast<ArchiveResult>(done.last().first()); QVERIFY(result.cancelled); QVERIFY(!result.success); QVERIFY(!QFileInfo::exists(request.archive));
        request.archive = root + "/retry.7z"; request.level = 1; QVERIFY(execute(backend, request).success); request.operation = ArchiveOperation::Test; request.files.clear(); QVERIFY(execute(backend, request).success);
    }
    void progressLabelsAndLargeValues() {
        ProgressDialog dialog("Extract", "fixture"); ArchiveProgress snapshot; snapshot.mode = "extract"; snapshot.total = 1000; snapshot.completed = 750; snapshot.input = 300; snapshot.output = 750; snapshot.files = 4; snapshot.doneFiles = 3; dialog.updateDetails(snapshot);
        QCOMPARE(dialog.findChild<QLabel *>("progressProcessed")->text(), QString("750")); QCOMPARE(dialog.findChild<QLabel *>("progressPacked")->text(), QString("300")); QCOMPARE(dialog.findChild<QLabel *>("progressFiles")->text(), QString("3")); QCOMPARE(dialog.findChild<QLabel *>("progressRatio")->text(), QString("40%")); QCOMPARE(dialog.findChild<QLabel *>("progressFilesTotal")->text(), QString(" / 4"));
        QTest::qWait(30); snapshot.completed = 1024 * 1024; snapshot.total = 2 * 1024 * 1024; snapshot.output = 0; dialog.updateDetails(snapshot); QCOMPARE(dialog.findChild<QLabel *>("progressProcessed")->text(), QString("0")); const auto speedText = dialog.findChild<QLabel *>("progressSpeed")->text(); const double units = speedText.endsWith(" MB/s") ? 1024 * 1024 : speedText.endsWith(" KB/s") ? 1024 : 1; QVERIFY(speedText.section(' ', 0, 0).toDouble() * units > 1000 * 1024);
        snapshot.mode = "compress"; snapshot.input = 0; snapshot.output.reset(); snapshot.total = 0; snapshot.completed = 0; snapshot.files = 0; snapshot.doneFiles = 0; dialog.updateDetails(snapshot); QCOMPARE(dialog.findChild<QLabel *>("progressProcessed")->text(), QString("0")); QCOMPARE(dialog.findChild<QLabel *>("progressPacked")->text(), QString()); QCOMPARE(dialog.findChild<QLabel *>("progressFiles")->text(), QString("0"));
        snapshot.input = 1; snapshot.output = std::numeric_limits<quint64>::max(); dialog.updateDetails(snapshot); QVERIFY(dialog.findChild<QLabel *>("progressRatio")->text().endsWith('%')); dialog.updateDetails({}); QCOMPARE(dialog.findChild<QLabel *>("progressProcessed")->text(), QString("0")); QCOMPARE(dialog.findChild<QLabel *>("progressFiles")->text(), QString("0"));
    }
    void workerPauseCancelAndTimeout() {
        const auto python = QStandardPaths::findExecutable("python3"); QVERIFY(!python.isEmpty()); const auto heartbeat = temp.filePath("worker-heartbeat.txt");
        const QString script = "import pathlib,sys,time\np=pathlib.Path(sys.argv[1])\nfor n in range(200):\n p.write_text(str(n));time.sleep(.02)\n";
        auto control = std::make_shared<OperationControl>(); auto future = QtConcurrent::run([=] { return WorkerProcess::run(python, {"-c", script, heartbeat}, {}, control); });
        auto cleanup = qScopeGuard([&] { control->cancel(); future.waitForFinished(); });
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(heartbeat), 10000); control->setPaused(true); QTest::qWait(200); QFile first(heartbeat); QVERIFY(first.open(QIODevice::ReadOnly)); const auto frozen = first.readAll(); first.close(); QTest::qWait(250); QVERIFY(first.open(QIODevice::ReadOnly)); QCOMPARE(first.readAll(), frozen); first.close(); QVERIFY(!future.isFinished());
        control->setPaused(false); QTRY_VERIFY_WITH_TIMEOUT([&] { QFile file(heartbeat); return file.open(QIODevice::ReadOnly) && file.readAll() != frozen; }(), 5000); control->setPaused(true); QTest::qWait(100); control->cancel(); QTRY_VERIFY_WITH_TIMEOUT(future.isFinished(), 5000); QVERIFY(future.result().cancelled); QCOMPARE(future.result().exitCode, 255);
        const auto timeoutHeartbeat = temp.filePath("timeout-heartbeat.txt"); auto timeoutControl = std::make_shared<OperationControl>(); auto timed = QtConcurrent::run([=] { return WorkerProcess::run(python, {"-c", script, timeoutHeartbeat}, {}, timeoutControl, {}, 200); }); auto timeoutCleanup = qScopeGuard([&] { timeoutControl->cancel(); timed.waitForFinished(); });
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(timeoutHeartbeat), 5000); timeoutControl->setPaused(true); QTest::qWait(400); QVERIFY(!timed.isFinished()); timeoutControl->setPaused(false); QTRY_VERIFY_WITH_TIMEOUT(timed.isFinished(), 5000); QVERIFY(timed.result().failure.contains("timed out")); QVERIFY(!timed.result().cancelled);
    }
    void workerUnicodeErrorAndSecret() {
        const auto python = QStandardPaths::findExecutable("python3"); QVERIFY(!python.isEmpty()); auto control = std::make_shared<OperationControl>();
        const auto result = WorkerProcess::run(python, {"-c", "import sys\np=sys.stdin.readline().strip()\nprint('日本語 '+p);print('failure '+p,file=sys.stderr);sys.exit(2)"}, "owned fixture secret", control);
        QVERIFY(!result.success); QCOMPARE(result.exitCode, 2); QVERIFY(result.output.contains("日本語 [redacted]")); QVERIFY(result.errors.contains("failure [redacted]")); QVERIFY(!result.output.contains("owned fixture secret")); QVERIFY(!result.errors.contains("owned fixture secret"));
    }
    void sourceVerificationPauseCancel() {
        const auto root = temp.filePath("verification-cancel"); QVERIFY(QDir().mkpath(root)); const auto source = root + "/source.txt"; write(source, QByteArray(4 * 1024 * 1024, 'v'));
        const auto python = QStandardPaths::findExecutable("python3"); QVERIFY(!python.isEmpty()); const auto wrapper = root + "/verification-runner";
        auto encoded = QJsonDocument(QJsonArray{executable}).toJson(QJsonDocument::Compact); encoded = encoded.mid(1, encoded.size() - 2);
        write(wrapper, "#!" + python.toUtf8() + "\nimport os,sys,time\nif sys.argv[1] == 't': time.sleep(1)\nos.execv(" + encoded + ",[" + encoded + "]+sys.argv[1:])\n"); QVERIFY(QFile::setPermissions(wrapper, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        SevenZipProcessBackend backend(wrapper); QSignalSpy done(&backend, &ArchiveBackend::finished); bool paused = false;
        connect(&backend, &ArchiveBackend::progressDetails, &backend, [&](ArchiveProgress snapshot) {
            if (paused || !snapshot.current.startsWith("Verifying archive:")) return;
            QVERIFY(backend.canPause()); paused = backend.setPaused(true); QVERIFY(paused); QTimer::singleShot(250, &backend, &ArchiveBackend::cancel);
        });
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/created.7z"; request.workingDirectory = root; request.files = {"source.txt"}; request.level = 1; request.deleteAfter = true; backend.start(request); if (done.isEmpty()) QVERIFY(done.wait(30000));
        QVERIFY(paused); const auto result = qvariant_cast<ArchiveResult>(done.last().first()); QVERIFY(result.cancelled); QVERIFY(!result.success); QVERIFY(QFileInfo::exists(source)); QVERIFY(QFileInfo::exists(request.archive)); request.operation = ArchiveOperation::Test; request.files.clear(); request.deleteAfter = false; QVERIFY(execute(backend, request).success);
    }
    void sourceVerificationCallbacks() {
        const auto root = temp.filePath("verification-callbacks"); QVERIFY(QDir().mkpath(root)); const auto source = root + "/日本語 source.txt"; const QByteArray payload(4 * 1024 * 1024 + 37, 'v'); write(source, payload);
        SevenZipProcessBackend backend(executable); QSignalSpy snapshots(&backend, &ArchiveBackend::progressDetails);
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/created.7z"; request.workingDirectory = root; request.files = {"日本語 source.txt"}; request.level = 1; request.deleteAfter = true; request.password = "worker fixture password"; request.encryptNames = true; request.preserveAccessTime = true; request.memoryLimitGB = 1;
        const auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!QFileInfo::exists(source)); const auto tested = last(snapshots, "test"); QCOMPARE(tested.completed.value_or(0), quint64(payload.size())); QCOMPARE(tested.files.value_or(0), quint64(1));
        const auto verified = last(snapshots, "hash"); QCOMPARE(verified.completed.value_or(0), quint64(payload.size())); QCOMPARE(verified.total.value_or(0), quint64(payload.size())); QCOMPARE(verified.doneFiles.value_or(0), quint64(1));
        request.operation = ArchiveOperation::Extract; request.files.clear(); request.deleteAfter = false; request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite"; QVERIFY(execute(backend, request).success); QFile file(request.outputDirectory + "/日本語 source.txt"); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), payload);
    }
    void droppedConsumerDoesNotInterruptEngine() {
        int sockets[2]; QVERIFY(::socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets) == 0); const int reader = sockets[0], writer = sockets[1]; const auto root = temp.filePath("closed-reader"); QVERIFY(QDir().mkpath(root)); write(root + "/one.txt", "still works");
        QProcess process; auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_PROGRESS_FD", "3"); process.setProcessEnvironment(environment); process.setChildProcessModifier([reader, writer] { ::close(reader); ::dup2(writer, 3); if (writer != 3) ::close(writer); });
        process.start(QFileInfo(executable).absolutePath() + "/7zz-progress", {"a", "-bso0", "--", root + "/one.7z", root + "/one.txt"}); QVERIFY(process.waitForStarted()); ::close(reader); ::close(writer); QVERIFY(process.waitForFinished(10000)); QCOMPARE(process.exitStatus(), QProcess::NormalExit); QCOMPARE(process.exitCode(), 0); QVERIFY(QFileInfo(root + "/one.7z").size() > 0);
    }
    void protocolKeepsUInt64AndRejectsInvalidValues() {
        QProcess process; ProgressChannel channel(&process); QSignalSpy snapshots(&channel, &ProgressChannel::snapshot); QVERIFY(channel.prepare(true));
        const auto python = QStandardPaths::findExecutable("python3"); QVERIFY(!python.isEmpty());
        const QString script = "import socket,json\ns=socket.socket(fileno=3)\nr=dict(version=1,mode='extract',current='日本語 space',total='18446744073709551615',completed='9007199254740993',input=None,output='9007199254740993',files='1',doneFiles='0')\ns.send(json.dumps(r).encode())\nr['completed']=9007199254740993\ns.send(json.dumps(r).encode())\nr['completed']='18446744073709551616'\ns.send(json.dumps(r).encode())\n";
        process.start(python, {"-c", script}); QVERIFY(process.waitForStarted()); QVERIFY(process.waitForFinished(10000)); QCOMPARE(process.exitCode(), 0); QCOMPARE(snapshots.size(), 1); const auto value = qvariant_cast<ArchiveProgress>(snapshots.first().first()); QCOMPARE(value.total.value(), std::numeric_limits<quint64>::max()); QCOMPARE(value.completed.value(), quint64(9007199254740993ULL)); QCOMPARE(value.current, QString("日本語 space")); QVERIFY(!value.input);
    }
    void overwriteProtocolFullWidth() {
        QProcess process; ProgressChannel channel(&process); QSignalSpy requests(&channel, &ProgressChannel::overwriteRequest); QVERIFY(channel.prepare(true));
        const auto python = QStandardPaths::findExecutable("python3"); QVERIFY(!python.isEmpty());
        const QString script = R"PY(
import socket,json
s=socket.socket(fileno=3)
r=dict(version=1,mode='overwrite',id='1',existingPath='/owned/日本語 space',incomingPath='new',existingDirectory=False,incomingDirectory=False,
       existingSize='18446744073709551615',incomingSize='9007199254740993',existingTime='18446744073709551615',incomingTime=None)
s.send(json.dumps(r).encode())
r['id']='2';r['existingSize']=9007199254740993;s.send(json.dumps(r).encode())
r['id']='3';r['existingSize']='18446744073709551616';s.send(json.dumps(r).encode())
r['id']='4';r['existingSize']='1';r['existingTime']='invalid';s.send(json.dumps(r).encode())
)PY";
        process.start(python, {"-c", script}); QVERIFY(process.waitForStarted()); QVERIFY(process.waitForFinished(10000)); QCOMPARE(process.exitCode(), 0); QCOMPARE(requests.size(), 1);
        const auto request = qvariant_cast<NativeOverwriteRequest>(requests.first().first()); QCOMPARE(request.conflict.existing.size, std::numeric_limits<quint64>::max());
        QCOMPARE(request.conflict.incoming.size, quint64(9007199254740993ULL)); QCOMPARE(request.conflict.existing.fileTime.value(), std::numeric_limits<quint64>::max());
        QVERIFY(!request.conflict.incoming.fileTime); QVERIFY(!channel.replyOverwrite(request, OverwriteAnswer::Yes));
    }
};
int main(int argc, char **argv) { QApplication::setDesktopSettingsAware(false); const auto plugins = qgetenv("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({QString::fromUtf8(plugins)}); QApplication app(argc, argv); if (argc < 2) return 2; executable = QString::fromLocal8Bit(argv[1]); --argc; for (int n = 1; n < argc; ++n) argv[n] = argv[n + 1]; NativeProgressTests tests; return QTest::qExec(&tests, argc, argv); }
#include "native-progress.moc"
