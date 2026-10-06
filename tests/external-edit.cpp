// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "ArchiveBackend.h"
#include "ExternalProcess.h"
#include "MainWindow.h"
#include "PortStyle.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include "input-driver.h"
#include <QMessageBox>
#include <QSettings>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QUuid>
#include <QPlainTextEdit>
#include <QTest>
#include <signal.h>

static QString executable;
static bool write(QString path, QByteArray bytes = {}) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static QByteArray hash(QString path) { return QCryptographicHash::hash(read(path), QCryptographicHash::Sha256); }
static bool focus(MainWindow &window, QString name) {
    auto tree = window.findChild<FileList *>("fileList"); auto items = tree->findItems(name, Qt::MatchExactly);
    if (items.size() != 1) return false;
    tree->clearSelection(); tree->setCurrentItem(items[0]); items[0]->setSelected(true); return true;
}
static QString diagnostic(MainWindow &window, QString error) {
    for (auto output : window.findChildren<QPlainTextEdit *>()) error += output->toPlainText();
    return error;
}

class EditorTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QString script;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(30000)) return {};
        return qvariant_cast<ArchiveResult>(done.first()[0]);
    }
    QString command(QString gate, QString mode = "edit") const { return "/bin/sh \"" + script + "\" \"" + gate + "\" " + mode; }
    void configure(QString gate, QString mode = "edit") {
        FileManagerSettings settings; settings.workMode = 2; settings.workPath = temporary.path(); settings.removableOnly = false;
        settings.viewer = settings.editor = command(gate, mode); settings.save();
    }
    ArchiveRequest create(QString root, QString format = "7z", int encryption = 0) {
        write(root + "/input/日本語 space [*].txt", "original editor content\n"); write(root + "/input/unrelated.txt", "untouched sibling\n");
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/archive." + format;
        request.format = format; request.workingDirectory = root + "/input"; request.level = 1;
        request.method = format == "zip" ? "Deflate" : format == "tar" ? "posix" : "LZMA2";
        request.files = QStringList{"gzip", "bzip2", "xz"}.contains(format) ? QStringList{"日本語 space [*].txt"} : QStringList{"日本語 space [*].txt", "unrelated.txt"};
        if (encryption) request.password = "editor secret 日本語";
        request.encryptNames = format == "7z" && encryption == 2;
        // 7-Zip ZIP encryption uses byte-oriented passwords; use ASCII.
        if (format == "zip" && encryption) { request.password = "editor zip secret"; if (encryption == 2) request.encryptionMethod = "ZipCrypto"; }
        return request;
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        script = temporary.filePath("test editor 日本語.sh");
        QVERIFY(write(script, R"SH(#!/bin/sh
gate=$1
mode=$2
file=$3
printf '%s' "$file" > "$gate.path"
if [ "$mode" = edit ]; then printf 'edited 日本語 file bytes\n' > "$file"; fi
if [ "$mode" = fork ]; then
  (printf 'edited 日本語 file bytes\n' > "$file"; printf ready > "$gate.child"; while [ ! -f "$gate.release" ]; do /bin/sleep 0.05; done) &
  exit 0
fi
if [ "$mode" = short ]; then exit 0; fi
while [ ! -f "$gate.release" ]; do /bin/sleep 0.05; done
if [ "$mode" = failure ]; then exit 7; fi
)SH"));
        qInfo() << "Owned external-editor fixtures:" << temporary.path();
    }
    void init() { QSettings().remove("View"); }
    void processWaitsForDescendants() {
        const auto gate = temporary.filePath("process fork"), file = temporary.filePath("process edited.txt"); QVERIFY(write(file, "original"));
        const auto release = qScopeGuard([&] { write(gate + ".release"); });
        ExternalProcess process; QSignalSpy done(&process, &ExternalProcess::finished); QString error;
        QVERIFY2(process.start(command(gate, "fork"), file, temporary.path(), &error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".child"), 5000); QTest::qWait(350); QVERIFY(done.isEmpty()); QVERIFY(process.running());
        QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000); QCOMPARE(done.first()[0].toInt(), 0); QVERIFY(!process.running());
        QVERIFY(read(file).contains("edited")); QVERIFY(!process.start(command(gate), file, temporary.path(), &error));
        ExternalProcess missing; QVERIFY(!missing.start(temporary.filePath("missing application"), file, {}, &error)); QVERIFY(error.contains("Cannot start application"));
    }
    void independentSameExecutableHandoff_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("short-unchanged-discovers-server") << QString("client");
        QTest::newRow("changed-client-does-not-discover") << QString("changed");
        QTest::newRow("long-client-does-not-discover") << QString("long");
    }
    void independentSameExecutableHandoff() {
        QFETCH(QString, mode);
        const auto gate = temporary.filePath("independent handoff " + mode);
        const auto file = temporary.filePath("handoff 日本語 space " + mode + ".txt");
        QVERIFY(write(file, "original"));
        QProcess server;
        const auto release = qScopeGuard([&] { write(gate + ".release"); if (!server.waitForFinished(3000) && server.state() != QProcess::NotRunning) { server.kill(); server.waitForFinished(); } });
        server.start(QString::fromUtf8(PORT_HANDOFF_EXECUTABLE), {"server", gate});
        QVERIFY(server.waitForStarted()); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".ready"), 5000);
        ExternalProcess observer; QSignalSpy done(&observer, &ExternalProcess::finished); QString error;
        const auto command = QString('"') + QString::fromUtf8(PORT_HANDOFF_EXECUTABLE) + "\" " + mode + " \"" + gate + '"';
        QVERIFY2(observer.start(command, file, temporary.path(), &error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".request"), 5000);
        if (mode == "client") {
            QTest::qWait(350); QVERIFY(done.isEmpty()); QVERIFY(observer.running());
            // The server existed before the launcher, with a different parent
            // and process group. Only official executable-name discovery can
            // bridge this independent IPC hand-off.
            QVERIFY(write(gate + ".commit")); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".edited"), 5000);
            QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000);
            QCOMPARE(read(file), QByteArray("handoff edited 日本語 bytes\n"));
        } else {
            QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000);
            QCOMPARE(server.state(), QProcess::Running);
            QCOMPARE(read(file), mode == "changed" ? QByteArray("immediate client edit\n") : QByteArray("original"));
        }
        QCOMPARE(done.first()[0].toInt(), 0);
    }
    void independentHandoffArchiveWriteBack() {
        const auto root = temporary.filePath("archive independent handoff"), gate = root + "/gate";
        auto request = create(root, "zip"); QVERIFY(execute(request).success);
        auto settings = FileManagerSettings::load(); settings.workMode = 2; settings.workPath = temporary.path(); settings.removableOnly = false;
        settings.editor = QString('"') + QString::fromUtf8(PORT_HANDOFF_EXECUTABLE) + "\" client \"" + gate + '"'; QVERIFY(settings.save());
        QProcess server;
        const auto release = qScopeGuard([&] { write(gate + ".release"); if (!server.waitForFinished(3000) && server.state() != QProcess::NotRunning) { server.kill(); server.waitForFinished(); } });
        server.start(QString::fromUtf8(PORT_HANDOFF_EXECUTABLE), {"server", gate}); QVERIFY(server.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".ready"), 5000);
        MainWindow window(executable); int questions = 0, updates = 0; QString errors;
        connect(window.findChild<SevenZipProcessBackend *>(), &ArchiveBackend::finished, &window, [&](const ArchiveResult &result) { if (result.operation == ArchiveOperation::ReplaceFile) { if (result.success) ++updates; else errors += result.message + result.details; } });
        QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) {
                if (box->text().contains("was modified")) { ++questions; QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton); }
                else { errors += box->text(); box->accept(); }
                return;
            }
        }); dialogs.start();
        window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space [*].txt"));
        window.findChild<QAction *>("editAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".request"), 10000);
        QTest::qWait(350); QCOMPARE(questions, 0); QCOMPARE(updates, 0);
        QVERIFY(write(gate + ".commit")); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".edited"), 5000);
        QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(updates, 1, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(questions, 1); QVERIFY2(errors.isEmpty(), qPrintable(errors));
        request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite";
        const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(read(request.outputDirectory + "/日本語 space [*].txt"), QByteArray("handoff edited 日本語 bytes\n"));
        QCOMPARE(read(request.outputDirectory + "/unrelated.txt"), QByteArray("untouched sibling\n"));
    }
    void writeBack_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<int>("encryption"); QTest::addColumn<QString>("action");
        for (const auto &format : {"7z", "zip", "tar", "wim", "gzip", "bzip2", "xz"}) QTest::newRow(format) << QString(format) << 0 << QString("edit");
        QTest::newRow("7z-data-view") << QString("7z") << 1 << QString("view");
        QTest::newRow("7z-headers") << QString("7z") << 2 << QString("edit");
        QTest::newRow("zip-AES") << QString("zip") << 1 << QString("edit");
        QTest::newRow("zip-ZipCrypto") << QString("zip") << 2 << QString("edit");
    }
    void writeBack() {
        QFETCH(QString, format); QFETCH(int, encryption); QFETCH(QString, action);
        const auto root = temporary.filePath("writeback " + format + QString::number(encryption)), gate = root + "/gate"; configure(gate);
        const auto release = qScopeGuard([&] { write(gate + ".release"); });
        auto request = create(root, format, encryption); auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); const auto original = hash(request.archive);
        MainWindow window(executable); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy finished(backend, &ArchiveBackend::finished);
        int questions = 0, ticks = 0, replacements = 0; QString errors;
        QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue(request.password); input->accept(); return; }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) {
                if (box->text().contains("was modified")) { ++questions; QVERIFY(window.operationBusy()); QCOMPARE(box->defaultButton(), box->button(QMessageBox::Yes)); QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton); }
                else { errors += box->text(); box->accept(); } return;
            }
        }); dialogs.start();
        connect(backend, &ArchiveBackend::finished, &window, [&](ArchiveResult r) { if (r.operation == ArchiveOperation::ReplaceFile) { ++replacements; QVERIFY2(r.success, qPrintable(r.message + r.details)); } });
        QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, &window, [&] { ++ticks; }); heartbeat.start();
        window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QString entry;
        for (auto item : window.findChild<FileList *>("fileList")->findItems("*", Qt::MatchWildcard)) if (!item->data(0, Qt::UserRole + 1).toBool() && item->text(0) != "unrelated.txt") entry = item->data(0, Qt::UserRole).toString();
        QVERIFY(!entry.isEmpty()); QVERIFY(focus(window, entry)); window.findChild<QAction *>(action + "Action")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), 10000); const auto edited = QString::fromUtf8(read(gate + ".path"));
        QPointer<ExternalProcess> observer; for (auto process : qApp->findChildren<ExternalProcess *>()) if (process->running()) observer = process;
        QVERIFY(observer); QSignalSpy observed(observer, &ExternalProcess::finished);
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QTest::qWait(300); QCOMPARE(questions, 0); QCOMPARE(hash(request.archive), original); QVERIFY(ticks >= 10); QVERIFY(window.findChild<FileList *>("fileList")->isEnabled());
        QVERIFY(write(gate + ".release")); QTRY_VERIFY2_WITH_TIMEOUT(replacements == 1, qPrintable(QString("finished=%1, questions=%2, observer=%3, busy=%4, errors=%5").arg(observed.size()).arg(questions).arg(bool(observer)).arg(window.operationBusy()).arg(errors)), 10000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QCOMPARE(questions, 1); QVERIFY2(errors.isEmpty(), qPrintable(errors)); QVERIFY(hash(request.archive) != original); QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(edited), 5000);
        request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(read(request.outputDirectory + '/' + entry), QByteArray("edited 日本語 file bytes\n"));
        if (!QStringList{"gzip", "bzip2", "xz"}.contains(format)) QCOMPARE(hash(root + "/input/unrelated.txt"), hash(request.outputDirectory + "/unrelated.txt"));
    }
    void discardedOrRetained_data() {
        QTest::addColumn<QString>("mode"); QTest::addColumn<int>("answer"); QTest::addColumn<bool>("retained");
        QTest::newRow("no") << QString("normal") << int(QMessageBox::No) << false;
        QTest::newRow("cancel") << QString("normal") << int(QMessageBox::Cancel) << false;
        QTest::newRow("other-folder") << QString("navigate") << int(QMessageBox::Yes) << true;
        QTest::newRow("read-only") << QString("readonly") << int(QMessageBox::Yes) << true;
        QTest::newRow("removed-item") << QString("removed") << int(QMessageBox::Yes) << true;
    }
    void discardedOrRetained() {
        QFETCH(QString, mode); QFETCH(int, answer); QFETCH(bool, retained);
        const auto root = temporary.filePath("decision " + mode + QString::number(answer)), gate = root + "/gate"; configure(gate);
        const auto release = qScopeGuard([&] { write(gate + ".release"); }); auto request = create(root); QVERIFY(execute(request).success);
        if (mode == "readonly") QVERIFY(QFile::setPermissions(request.archive, QFileDevice::ReadOwner));
        QString edited, error; int questions = 0;
        {
            MainWindow window(executable); QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] {
                for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) { ++questions; QTest::mouseClick(box->button(QMessageBox::StandardButton(answer)), Qt::LeftButton); } else { error = box->text(); box->accept(); } return; }
            }); dialogs.start(); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space [*].txt"));
            // Read-only archives allow Edit; modified copies are retained, not silently lost.
            QVERIFY(window.findChild<QAction *>("editAction")->isEnabled()); window.findChild<QAction *>("editAction")->trigger();
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), 10000); edited = QString::fromUtf8(read(gate + ".path")); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
            if (mode == "navigate") window.openPath(root);
            if (mode == "removed") { auto remove = request; remove.operation = ArchiveOperation::Delete; remove.files = {"日本語 space [*].txt"}; QVERIFY(execute(remove).success); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); }
            const auto unchanged = hash(request.archive); QVERIFY(write(gate + ".release"));
            if (retained) QTRY_VERIFY_WITH_TIMEOUT(!error.isEmpty(), 5000);
            else QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(edited), 5000);
            QCOMPARE(hash(request.archive), unchanged); QCOMPARE(questions, mode == "readonly" ? 0 : 1); QVERIFY(!window.operationBusy());
            if (retained) QVERIFY(error.contains(edited));
        }
        QCOMPARE(QFileInfo::exists(edited), retained);
        if (retained) QCOMPARE(read(edited), QByteArray("edited 日本語 file bytes\n"));
        QVERIFY(QFile::setPermissions(request.archive, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    }
    void runningEditorSurvivesClose_data() { QTest::addColumn<QString>("mode"); QTest::newRow("modified") << QString("edit"); QTest::newRow("unchanged") << QString("wait"); }
    void runningEditorSurvivesClose() {
        QFETCH(QString, mode); const auto root = temporary.filePath("close " + mode), gate = root + "/gate"; configure(gate, mode);
        const auto release = qScopeGuard([&] { write(gate + ".release"); }); auto request = create(root); QVERIFY(execute(request).success); const auto original = hash(request.archive);
        QString edited; QPointer<ExternalProcess> process;
        {
            MainWindow window(executable); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space [*].txt")); window.findChild<QAction *>("editAction")->trigger();
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), 10000); edited = QString::fromUtf8(read(gate + ".path")); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
            for (auto observer : qApp->findChildren<ExternalProcess *>()) if (observer->running()) process = observer;
            QVERIFY(process); QVERIFY(window.close()); QVERIFY(process->running()); QCOMPARE(::kill(pid_t(process->processId()), 0), 0);
        }
        QVERIFY(process && process->running()); QVERIFY(QFileInfo::exists(edited)); QCOMPARE(::kill(pid_t(process->processId()), 0), 0);
        QVERIFY(write(gate + ".release")); QTRY_VERIFY_WITH_TIMEOUT(!process, 5000); QVERIFY(QFileInfo::exists(edited)); QCOMPARE(hash(request.archive), original);
    }
    void shortLauncherLifetime_data() { QTest::addColumn<bool>("changed"); QTest::newRow("unchanged-retained") << false; QTest::newRow("later-edit-retained") << true; }
    void shortLauncherLifetime() {
        QFETCH(bool, changed); const auto root = temporary.filePath("short " + QString::number(changed)), gate = root + "/gate"; configure(gate, "short");
        auto request = create(root); QVERIFY(execute(request).success); const auto original = hash(request.archive); QString extracted;
        {
            MainWindow window(executable); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space [*].txt")); window.findChild<QAction *>("viewAction")->trigger();
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), 10000); extracted = QString::fromUtf8(read(gate + ".path")); QTest::qWait(500); QVERIFY(QFileInfo::exists(extracted));
            if (changed) QVERIFY(write(extracted, "later edit after launcher quit"));
            QTest::qWait(250); QVERIFY(!window.findChild<QMessageBox *>()); QVERIFY(window.close());
        }
        QVERIFY(QFileInfo::exists(extracted)); QCOMPARE(hash(request.archive), original);
    }
    void focusedFileOnly() {
        const auto root = temporary.filePath("focused selection"), gate = root + "/gate"; configure(gate); const auto release = qScopeGuard([&] { write(gate + ".release"); });
        auto request = create(root); QVERIFY(execute(request).success); MainWindow window(executable); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto tree = window.findChild<FileList *>("fileList"); QVERIFY(focus(window, "日本語 space [*].txt")); const auto focused = tree->currentItem();
        tree->findItems("unrelated.txt", Qt::MatchExactly)[0]->setSelected(true); QCOMPARE(tree->selectedItems().size(), 2);
        int questions = 0; QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { ++questions; QTest::mouseClick(box->button(QMessageBox::No), Qt::LeftButton); } }); dialogs.start();
        window.findChild<QAction *>("editAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), 10000); const auto extracted = QString::fromUtf8(read(gate + ".path"));
        QCOMPARE(QFileInfo(extracted).fileName(), focused->text(0)); QVERIFY(!QFileInfo::exists(QFileInfo(extracted).absolutePath() + "/unrelated.txt")); QCOMPARE(tree->selectedItems().size(), 2);
        QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(questions, 1, 5000); QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(extracted), 5000);
    }
    void applicationBundleWaitAndArguments() {
        const auto root = temporary.filePath("application launch"), gate = root + "/日本語 gate with spaces", bundle = root + "/Editor witness.app";
        const auto release = qScopeGuard([&] { write(gate + ".release"); }); QDir().mkpath(bundle + "/Contents/MacOS");
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/editor_application", bundle + "/Contents/MacOS/witness"));
        const auto identifier = "org.sevenzipmac.tests.editor." + QUuid::createUuid().toString(QUuid::Id128);
        QVERIFY(write(bundle + "/Contents/Info.plist", ("<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>CFBundleIdentifier</key><string>" + identifier + "</string><key>CFBundleExecutable</key><string>witness</string><key>CFBundlePackageType</key><string>APPL</string><key>LSUIElement</key><true/><key>CFBundleDocumentTypes</key><array><dict><key>CFBundleTypeRole</key><string>Viewer</string><key>CFBundleTypeExtensions</key><array><string>txt</string></array></dict></array></dict></plist>").toUtf8()));
        auto request = create(root); QVERIFY(execute(request).success); configure(gate);
        auto settings = FileManagerSettings::load(); settings.editor = "\"" + bundle + "\" \"" + gate + "\""; QVERIFY(settings.save());
        MainWindow window(executable); QString error; int questions = 0;
        QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) { ++questions; QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton); } else { error = box->text(); box->accept(); } return; } }); dialogs.start();
        window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space [*].txt")); window.findChild<QAction *>("editAction")->trigger();
        QTRY_VERIFY2_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), qPrintable(error), 10000); const auto extracted = QString::fromUtf8(read(gate + ".path"));
        QTest::qWait(250); QCOMPARE(questions, 0); const auto pid = read(gate + ".pid").toLongLong(); QVERIFY(pid > 0); QCOMPARE(::kill(pid_t(pid), 0), 0);
        QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(questions, 1, 10000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(extracted), 5000);
        QVERIFY2(error.isEmpty(), qPrintable(error)); request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite";
        QVERIFY(execute(request).success); QCOMPARE(read(request.outputDirectory + "/日本語 space [*].txt"), QByteArray("edited 日本語 file bytes\n"));
        // Normal filesystem F4 must also pass app arguments after --args,
        // rather than accidentally presenting them as additional documents.
        const auto fsGate = root + "/filesystem argument 日本語", source = root + "/normal source 日本語.txt";
        const auto fsRelease = qScopeGuard([&] { write(fsGate + ".release"); }); QVERIFY(write(source, "normal filesystem content"));
        settings.editor = "\"" + bundle + "\" \"" + fsGate + "\""; QVERIFY(settings.save());
        MainWindow normal(executable); normal.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!normal.operationBusy(), 10000); QVERIFY(focus(normal, QFileInfo(source).fileName())); normal.findChild<QAction *>("editAction")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!read(fsGate + ".path").isEmpty(), 10000); QCOMPARE(QFileInfo(QString::fromUtf8(read(fsGate + ".path"))).canonicalFilePath(), QFileInfo(source).canonicalFilePath()); QVERIFY(!normal.operationBusy());
        const auto fsPid = read(fsGate + ".pid").toLongLong(); QVERIFY(fsPid > 0); QVERIFY(write(fsGate + ".release"));
        QTRY_VERIFY_WITH_TIMEOUT(::kill(pid_t(fsPid), 0) != 0, 5000); QCOMPARE(read(source), QByteArray("edited 日本語 file bytes\n"));
    }
    void observedNewSessionChild() {
        const auto root = temporary.filePath("new session child"), gate = root + "/gate", file = root + "/file.txt", launcher = root + "/launcher.py"; QVERIFY(write(file, "original"));
        const auto release = qScopeGuard([&] { write(gate + ".release"); });
        QVERIFY(write(launcher, R"PY(import os, pathlib, sys, time
gate=pathlib.Path(sys.argv[1]); file=pathlib.Path(sys.argv[2])
pid=os.fork()
if pid==0:
    os.setsid(); file.write_text('new session edit'); pathlib.Path(str(gate)+'.child').touch()
    while not pathlib.Path(str(gate)+'.release').exists(): time.sleep(.05)
    os._exit(0)
time.sleep(.5)
)PY"));
        ExternalProcess process; QString error; QSignalSpy done(&process, &ExternalProcess::finished);
        QVERIFY2(process.start("/usr/bin/python3 \"" + launcher + "\" \"" + gate + "\"", file, root, &error), qPrintable(error));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".child"), 5000); QTest::qWait(750); QVERIFY(process.running()); QVERIFY(done.isEmpty());
        QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 5000); QCOMPARE(done.first()[0].toInt(), 0);
    }
    void nestedEditorWriteBack() {
        const auto root = temporary.filePath("nested editor"), gate = root + "/gate", outer = root + "/outer.7z"; configure(gate);
        const auto release = qScopeGuard([&] { write(gate + ".release"); }); auto inner = create(root, "zip", 1); QVERIFY(execute(inner).success);
        ArchiveRequest parent; parent.operation = ArchiveOperation::Add; parent.archive = outer; parent.workingDirectory = root; parent.files = {"archive.zip"}; parent.password = "outer header secret"; parent.encryptNames = true; parent.level = 1; QVERIFY(execute(parent).success); const auto original = hash(outer);
        MainWindow window(executable); int questions = 0; QString error; QTimer dialogs; dialogs.setInterval(10);
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue(window.currentArchivePath().isEmpty() ? parent.password : inner.password); input->accept(); return; }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) { ++questions; QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton); } else { error += box->text(); box->accept(); } return; }
        }); dialogs.start(); window.openPath(outer + "/archive.zip/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(window.currentArchivePath() != outer); QVERIFY(focus(window, "日本語 space [*].txt"));
        QVERIFY(window.findChild<QAction *>("editAction")->isEnabled()); window.findChild<QAction *>("editAction")->trigger(); QTRY_VERIFY2_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), qPrintable(diagnostic(window, error)), 10000);
        QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(questions, 1, 10000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(hash(outer), original);
        window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(questions, 2); QCOMPARE(window.currentArchivePath(), outer); QVERIFY(hash(outer) != original); QVERIFY2(error.isEmpty(), qPrintable(error));
        parent.operation = ArchiveOperation::Extract; parent.files.clear(); parent.outputDirectory = root + "/outer restored"; parent.overwriteMode = "overwrite"; QVERIFY(execute(parent).success);
        inner.operation = ArchiveOperation::Extract; inner.archive = parent.outputDirectory + "/archive.zip"; inner.files.clear(); inner.outputDirectory = root + "/inner restored"; inner.overwriteMode = "overwrite"; QVERIFY(execute(inner).success);
        QCOMPARE(read(inner.outputDirectory + "/日本語 space [*].txt"), QByteArray("edited 日本語 file bytes\n"));
    }
    void updateCancellationRetainsEdit() {
        const auto root = temporary.filePath("editor update cancel"), gate = root + "/gate"; configure(gate); const auto release = qScopeGuard([&] { write(gate + ".release"); });
        auto request = create(root); QByteArray large(4 << 20, 'x'); QVERIFY(write(root + "/input/large.bin", large)); request.files << "large.bin"; request.level = 0; request.method = "Copy"; QVERIFY(execute(request).success); QVERIFY(QFileInfo(request.archive).size() >= (4 << 20)); const auto original = hash(request.archive); QString extracted, error;
        {
            MainWindow window(executable); auto backend = window.findChild<SevenZipProcessBackend *>(); bool cancelled = false; QTimer dialogs; dialogs.setInterval(10);
            connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton); else { error = box->text(); box->accept(); } return; } }); dialogs.start();
            window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space [*].txt")); window.findChild<QAction *>("editAction")->trigger();
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), 10000); extracted = QString::fromUtf8(read(gate + ".path")); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
            connect(backend, &ArchiveBackend::progress, backend, [&](int, quint64 bytes, quint64, QString) { if (bytes == (1 << 20)) QMetaObject::invokeMethod(backend, [&] { backend->cancel(); cancelled = true; }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
            QVERIFY(write(gate + ".release")); QTRY_VERIFY_WITH_TIMEOUT(!error.isEmpty(), 10000); QVERIFY2(cancelled, qPrintable(error)); QVERIFY(error.contains(extracted)); QCOMPARE(hash(request.archive), original); QVERIFY(!window.operationBusy());
        }
        QCOMPARE(read(extracted), QByteArray("edited 日本語 file bytes\n"));
    }
    void twoPanelsSerializeWrites() {
        const auto root = temporary.filePath("two panel editor"), firstGate = root + "/first", secondGate = root + "/second"; configure(firstGate);
        const auto release = qScopeGuard([&] { write(firstGate + ".release"); write(secondGate + ".release"); }); auto request = create(root);
        QVERIFY(write(root + "/input/large.bin", QByteArray(4 << 20, 'x'))); request.files << "large.bin"; request.level = 0; request.method = "Copy"; QVERIFY(execute(request).success); QVERIFY(QFileInfo(request.archive).size() >= (4 << 20));
        MainWindow window(executable); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY(focus(window, "日本語 space [*].txt")); QVERIFY(window.findChild<QAction *>("editAction")->isEnabled());
        window.findChild<QAction *>("editAction")->trigger(); QTRY_VERIFY2_WITH_TIMEOUT(QFileInfo::exists(firstGate + ".path"), qPrintable(diagnostic(window, {})), 10000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        configure(secondGate); window.findChild<QAction *>("twoPanelsAction")->trigger();
        auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second); second->openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY(focus(*second, "unrelated.txt"));
        second->findChild<QAction *>("editAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(secondGate + ".path"), 10000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto firstBackend = window.findChild<SevenZipProcessBackend *>(); auto secondBackend = second->findChild<SevenZipProcessBackend *>(); bool paused = false; int firstDone = 0, secondDone = 0, secondQuestions = 0; QString error;
        connect(firstBackend, &ArchiveBackend::progress, firstBackend, [&](int, quint64 bytes, quint64, QString) { if (!paused && bytes == (1 << 20)) QMetaObject::invokeMethod(firstBackend, [&] { QVERIFY(firstBackend->setPaused(true)); paused = true; }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
        connect(firstBackend, &ArchiveBackend::finished, &window, [&](ArchiveResult r) { if (r.operation == ArchiveOperation::ReplaceFile) { QVERIFY(r.success); ++firstDone; } });
        connect(secondBackend, &ArchiveBackend::finished, second, [&](ArchiveResult r) { if (r.operation == ArchiveOperation::ReplaceFile) { QVERIFY(r.success); ++secondDone; } });
        QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) { if (box->text().contains("unrelated.txt")) ++secondQuestions; QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton); } else { error += box->text(); box->accept(); } return; } }); dialogs.start();
        QVERIFY(write(firstGate + ".release")); QTRY_VERIFY_WITH_TIMEOUT(paused, 5000); QVERIFY(write(secondGate + ".release")); QTest::qWait(350);
        QVERIFY(window.operationBusy()); QVERIFY(second->operationBusy()); QVERIFY(!secondBackend->busy()); QCOMPARE(secondQuestions, 0); QVERIFY(firstBackend->setPaused(false));
        QTRY_COMPARE_WITH_TIMEOUT(firstDone, 1, 10000); QTRY_COMPARE_WITH_TIMEOUT(secondDone, 1, 10000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(secondQuestions, 1); QVERIFY2(error.isEmpty(), qPrintable(error));
        request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite"; QVERIFY(execute(request).success);
        QCOMPARE(read(request.outputDirectory + "/日本語 space [*].txt"), QByteArray("edited 日本語 file bytes\n")); QCOMPARE(read(request.outputDirectory + "/unrelated.txt"), QByteArray("edited 日本語 file bytes\n")); QCOMPARE(read(request.outputDirectory + "/large.bin"), QByteArray(4 << 20, 'x'));
    }
    void failedLaunchAndUnchangedCompletion_data() {
        QTest::addColumn<QString>("mode"); QTest::newRow("missing-program") << QString("missing"); QTest::newRow("nonzero-exit") << QString("failure"); QTest::newRow("long-unchanged") << QString("wait");
    }
    void failedLaunchAndUnchangedCompletion() {
        QFETCH(QString, mode); const auto root = temporary.filePath("editor failure " + mode), gate = root + "/gate"; configure(gate, mode);
        const auto release = qScopeGuard([&] { write(gate + ".release"); });
        if (mode == "missing") { auto settings = FileManagerSettings::load(); settings.editor = "\"" + root + "/missing executable\""; QVERIFY(settings.save()); }
        auto request = create(root); QVERIFY(execute(request).success); const auto original = hash(request.archive); QString error, extracted;
        MainWindow window(executable); QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { error = box->text(); box->accept(); return; } }); dialogs.start();
        const auto before = QDir(temporary.path()).entryList({".7zip-open-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
        window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space [*].txt")); window.findChild<QAction *>("editAction")->trigger();
        if (mode == "missing") { QTRY_VERIFY_WITH_TIMEOUT(!error.isEmpty(), 10000); QVERIFY(error.contains("Cannot start application")); }
        else {
            QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(gate + ".path"), 10000); extracted = QString::fromUtf8(read(gate + ".path"));
            if (mode == "wait") QTest::qWait(2100);
            QVERIFY(write(gate + ".release")); QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(extracted), 5000);
            if (mode == "failure") QVERIFY(error.contains("Exit code: 7")); else QVERIFY(error.isEmpty());
        }
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 5000); QCOMPARE(hash(request.archive), original);
        QTRY_COMPARE_WITH_TIMEOUT(QDir(temporary.path()).entryList({".7zip-open-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot), before, 5000);
    }
};

int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("ExternalEdit"); app.setQuitOnLastWindowClosed(false);
    EditorTests test; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr;
    return QTest::qExec(&test, args.size() - 1, args.data());
}
#include "external-edit.moc"
