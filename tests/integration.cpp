#include "progress-dialog-driver.h"
#include <QLocale>
#include "test-activation.h"
#include "ArchiveBackend.h"
#include "MainWindow.h"
#include "FileOperations.h"
#include "FileToolDialogs.h"
#include "Benchmark.h"
#include <QApplication>
#include "PortStyle.h"
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFile>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QDialogButtonBox>
#include <QToolButton>
#include <QStyleFactory>
#include "input-driver.h"
#include <QMessageBox>
#include "Help.h"
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMenuBar>
#include <QMenu>
#include <QWindow>
#include <QTabWidget>
#include <QRadioButton>
#include <QSpinBox>
#include <QHeaderView>
#include <QStackedWidget>
#include "UiLanguage.h"
#include "OpenWith.h"
#include <QFileOpenEvent>
#include <QLayout>
#include <QScopeGuard>
#include <QClipboard>
#include <QRegularExpression>
#include <QPlainTextEdit>
#include <QLabel>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <fcntl.h>
#include <unistd.h>

static QString executable;
template<class T> static T *panelWidget(MainWindow &window, const QString &name = {}) {
    for (auto widget : window.findChildren<T *>(name)) {
        QWidget *owner = widget->parentWidget();
        while (owner && !qobject_cast<MainWindow *>(owner)) owner = owner->parentWidget();
        if (owner == &window) return widget;
    }
    return nullptr;
}
static void confirmProgressCancellation(QWidget *owner) {
    QTimer::singleShot(0, owner, [owner] {
        if (auto question = owner->findChild<QMessageBox *>("progressCancelConfirmation"))
            question->button(QMessageBox::Yes)->click();
    });
}
static void writeFile(QString path, QByteArray bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path); if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size()) qFatal("Cannot create test file"); }
static QByteArray hash(QString path) { QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {}; QCryptographicHash h(QCryptographicHash::Sha256); h.addData(&f); return h.result(); }

class Integration : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    ProgressDialogDriver resultDialogs;
    QString input;
    ArchiveResult execute(SevenZipProcessBackend &backend, ArchiveRequest r) {
        QSignalSpy spy(&backend, &ArchiveBackend::finished); backend.start(r);
        if (spy.isEmpty() && !spy.wait(90000)) {
            QTest::qFail("Archive operation timed out", __FILE__, __LINE__);
            backend.cancel(); if (spy.isEmpty()) spy.wait(10000);
            ArchiveResult timeout; timeout.message = "Archive operation timed out"; return timeout;
        }
        return qvariant_cast<ArchiveResult>(spy.takeFirst().at(0));
    }
    ArchiveRequest addRequest(QString archive, QString format = "7z") {
        ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive; r.format = format; r.method = format == "zip" ? "Deflate" : "LZMA2"; r.workingDirectory = temp.path(); r.files = {"input"}; r.level = 1; r.dictionary = "4m"; r.threads = 2; return r;
    }
    ArchiveResult menuExecute(MainWindow &window, QString command, QStringList paths) {
        auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy spy(backend, &ArchiveBackend::finished);
        window.executeOpenWith(command, paths); if (spy.isEmpty() && !spy.wait(30000)) qFatal("Open With command timed out");
        return qvariant_cast<ArchiveResult>(spy.last().at(0));
    }
private slots:
    void init() { resultDialogs.clear(); }
    void initTestCase() {
        QVERIFY(temp.isValid()); input = temp.filePath("input");
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.filePath("preferences")); QSettings().setValue("View/LastPath", temp.path());
        writeFile(input + "/ASCII.txt", "Hello 7-Zip\n"); writeFile(input + "/日本語ファイル.txt", QString("日本語 UTF-8\n").toUtf8());
        writeFile(input + "/space in name.txt", "spaces\n"); writeFile(input + "/hierarchy/deeper/data.txt", "nested\n"); QDir().mkpath(input + "/empty directory");
        QByteArray data(32 * 1024 * 1024, '\0'); QRandomGenerator rng(20261003); rng.fillRange(reinterpret_cast<quint32 *>(data.data()), data.size() / 4); writeFile(input + "/large.bin", data);
        qInfo() << "Dedicated fixture directory:" << temp.path();
    }
    void roundtrip_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void roundtrip() {
        QFETCH(QString, format); SevenZipProcessBackend backend(executable);
        QString archive = temp.filePath("roundtrip." + format);
        auto result = execute(backend, addRequest(archive, format)); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(QFileInfo::exists(archive));
        ArchiveRequest r; r.archive = archive; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details)); QVERIFY(result.entries.size() >= 8); bool japanese = false;
        for (const auto &e : result.entries) japanese |= e.path.contains("日本語"); QVERIFY(japanese);
        r.operation = ArchiveOperation::Test; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.operation = ArchiveOperation::Extract; r.outputDirectory = temp.filePath("roundtrip-out-" + format); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QDirIterator it(input, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); QString dest = r.outputDirectory + "/input/" + QDir(input).relativeFilePath(it.filePath()); QVERIFY2(QFileInfo::exists(dest), qPrintable(dest)); if (it.fileInfo().isFile()) QCOMPARE(hash(it.filePath()), hash(dest)); }
    }
    void encryption_data() { QTest::addColumn<QString>("format"); QTest::addColumn<bool>("headers"); QTest::newRow("7z-data") << QString("7z") << false; QTest::newRow("7z-headers") << QString("7z") << true; QTest::newRow("zip-AES") << QString("zip") << false; QTest::newRow("zip-ZipCrypto") << QString("zip") << true; }
    void encryption() {
        QFETCH(QString, format); QFETCH(bool, headers); SevenZipProcessBackend backend(executable);
        const QString password = format == "zip" ? "zip-test password 123" : "テスト用 password 123";
        auto r = addRequest(temp.filePath("encrypted-" + format + QString::number(headers) + '.' + format), format); r.files = {"input/日本語ファイル.txt", "input/ASCII.txt"}; r.password = password; r.encryptNames = format == "7z" && headers; if (format == "zip" && headers) r.encryptionMethod = "ZipCrypto";
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!result.details.contains(r.password));
        if (format == "7z" && headers) {
            auto noPassword = r; noPassword.operation = ArchiveOperation::List; noPassword.password.clear(); noPassword.files.clear();
            auto missing = execute(backend, noPassword); QVERIFY(!missing.success); QVERIFY(missing.details.contains("password", Qt::CaseInsensitive));
        }
        r.operation = ArchiveOperation::List; r.files.clear(); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(result.entries.size(), 2);
        r.operation = ArchiveOperation::Test; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details)); QVERIFY(!result.details.contains(r.password));
        r.password = "wrong-password"; result = execute(backend, r); QVERIFY(!result.success); QVERIFY(result.exitCode != 0);
        r.password = password; r.operation = ArchiveOperation::Extract; r.outputDirectory = temp.filePath("encrypted-out-" + format + QString::number(headers)); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(hash(input + "/日本語ファイル.txt"), hash(r.outputDirectory + "/input/日本語ファイル.txt"));
    }
    void updateRenameDelete() {
        SevenZipProcessBackend backend(executable); auto r = addRequest(temp.filePath("editing.7z")); r.files = {"input/ASCII.txt"}; auto result = execute(backend, r); QVERIFY(result.success);
        writeFile(temp.filePath("new 日本語.txt"), "added"); r.files = {"new 日本語.txt"}; r.updateMode = "update"; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.operation = ArchiveOperation::Rename; r.files = {"new 日本語.txt", "renamed file.txt"}; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.operation = ArchiveOperation::Delete; r.files = {"input/ASCII.txt"}; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.operation = ArchiveOperation::List; r.files.clear(); result = execute(backend, r); QVERIFY(result.success); QCOMPARE(result.entries.size(), 1); QCOMPARE(result.entries[0].path, QString("renamed file.txt"));
    }
    void encryptedUpdate() {
        SevenZipProcessBackend backend(executable); auto r = addRequest(temp.filePath("encrypted-update.7z")); r.files = {"input/ASCII.txt"}; r.password = "update-secret"; r.encryptNames = true;
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.files = {"input/日本語ファイル.txt"}; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.operation = ArchiveOperation::Test; r.files.clear(); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
    }
    void selectedExtractOverwrite() {
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.archive = temp.filePath("roundtrip.7z"); r.operation = ArchiveOperation::Extract; r.files = {"input/ASCII.txt"}; r.outputDirectory = temp.filePath("selected");
        auto result = execute(backend, r); QVERIFY(result.success); QVERIFY(QFileInfo::exists(r.outputDirectory + "/input/ASCII.txt")); QVERIFY(!QFileInfo::exists(r.outputDirectory + "/input/large.bin"));
        writeFile(r.outputDirectory + "/input/ASCII.txt", "keep me"); r.overwriteMode = "skip"; result = execute(backend, r); QVERIFY(result.success); QCOMPARE(hash(r.outputDirectory + "/input/ASCII.txt"), QCryptographicHash::hash("keep me", QCryptographicHash::Sha256));
        r.overwriteMode = "rename"; result = execute(backend, r); QVERIFY(result.success); QVERIFY(QFileInfo::exists(r.outputDirectory + "/input/ASCII_1.txt"));
        r.overwriteMode = "ask"; QSignalSpy spy(&backend, &ArchiveBackend::finished), overwrite(&backend, &ArchiveBackend::overwriteRequested); backend.start(r); QVERIFY(overwrite.wait(10000)); QVERIFY(backend.busy()); QVERIFY(backend.resolveOverwrite(qvariant_cast<OverwriteConflict>(overwrite.last().first()).id, OverwriteAnswer::Cancel)); if (spy.isEmpty()) QVERIFY(spy.wait(10000)); result = qvariant_cast<ArchiveResult>(spy[0][0]); QVERIFY(result.cancelled);
        r.overwriteMode = "overwrite"; result = execute(backend, r); QVERIFY(result.success); QCOMPARE(hash(input + "/ASCII.txt"), hash(r.outputDirectory + "/input/ASCII.txt"));
    }
    void safetyPaths() {
        for (const auto &p : {"../escape", "a/../../escape", "/absolute", "C:\\Windows\\file", "\\server\\share"}) QVERIFY(!SevenZipProcessBackend::safeArchivePath(p));
        QVERIFY(SevenZipProcessBackend::safeArchivePath("a\nPath = fake")); QVERIFY(SevenZipProcessBackend::safeArchivePath("a\rfile")); QVERIFY(!SevenZipProcessBackend::safeArchivePath(QString("a") + QChar::Null));
        QVERIFY(SevenZipProcessBackend::safeArchivePath("日本語/a space/file.txt"));
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Extract; QString fixtures = qEnvironmentVariable("PORT_FIXTURES"); QVERIFY(!fixtures.isEmpty());
        for (const auto &name : {"traversal", "absolute", "symlink"}) {
            r.archive = fixtures + '/' + name + ".zip"; r.outputDirectory = temp.filePath(QString("unsafe-") + name); auto result = execute(backend, r); QVERIFY2(!result.success, name); QVERIFY2(result.message.contains("refused"), qPrintable(result.message + result.details));
        }
        // Repeated legitimate names use original overwrite policy, rather
        // than the early port's blanket unsafe-path refusal. A standalone
        // backend needs an explicit policy (there is no GUI Ask responder).
        r.overwriteMode = "overwrite"; r.archive = fixtures + "/duplicate.zip";
        r.outputDirectory = temp.filePath("duplicates"); auto result = execute(backend, r); QVERIFY(result.success);
        QFile repeated(r.outputDirectory + "/same.txt"); QVERIFY(repeated.open(QIODevice::ReadOnly)); QCOMPARE(repeated.readAll(), QByteArray("unsafe test fixture"));
        r.archive = fixtures + "/flat-duplicate.zip"; r.pathMode = "flat"; r.outputDirectory = temp.filePath("flat-duplicates"); result = execute(backend, r); QVERIFY(result.success);
        QFile flattened(r.outputDirectory + "/same.txt"); QVERIFY(flattened.open(QIODevice::ReadOnly)); QCOMPARE(flattened.readAll(), QByteArray("unsafe test fixture"));
        r.pathMode = "full"; r.archive = temp.filePath("roundtrip.7z"); r.outputDirectory = temp.filePath("symlink-output"); QDir().mkpath(r.outputDirectory); QDir().mkpath(temp.filePath("link-target")); QVERIFY(QFile::link(temp.filePath("link-target"), r.outputDirectory + "/input")); result = execute(backend, r); QVERIFY(!result.success); QVERIFY(!QFileInfo::exists(temp.filePath("link-target/ASCII.txt")));
    }
    void errorsAndRecovery() {
        SevenZipProcessBackend backend(executable);
        // This failure/recovery case must also run independently of roundTrip.
        auto healthy = addRequest(temp.filePath("error-recovery.7z")); healthy.files = {"input/ASCII.txt"};
        QVERIFY(execute(backend, healthy).success);
        writeFile(temp.filePath("corrupt.7z"), "bad archive"); ArchiveRequest r; r.archive = temp.filePath("corrupt.7z"); r.operation = ArchiveOperation::Test;
        auto result = execute(backend, r); QVERIFY(!result.success); QVERIFY(result.exitCode != 0); QVERIFY(!result.details.isEmpty()); QVERIFY(!backend.busy());
        r.archive = healthy.archive; result = execute(backend, r); QVERIFY(result.success);
        r.operation = ArchiveOperation::Extract; r.outputDirectory = "/dev/null/not-a-directory"; result = execute(backend, r); QVERIFY(!result.success); QVERIFY(!backend.busy());
        SevenZipProcessBackend missing(temp.filePath("missing-7zz")); result = execute(missing, r); QVERIFY(!result.success); QCOMPARE(result.exitCode, -1);
    }
    void rarSelectedExtract() {
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.archive = qEnvironmentVariable("PORT_FIXTURES") + "/sample.rar";
        auto result = execute(backend, r); QVERIFY(result.success); QCOMPARE(result.entries.size(), 5);
        r.operation = ArchiveOperation::Test; result = execute(backend, r); QVERIFY(result.success);
        r.operation = ArchiveOperation::Extract; r.files = {"test.txt", "testdir/test.txt", "testemptydir"}; r.outputDirectory = temp.filePath("rar-out"); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QFile file(r.outputDirectory + "/test.txt"); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), QByteArray("test text document\r\n")); QVERIFY(QFileInfo(r.outputDirectory + "/testemptydir").isDir());
        QCOMPARE(hash(r.outputDirectory + "/test.txt"), hash(r.outputDirectory + "/testdir/test.txt"));
    }
    void splitVolumesAndReadOnly() {
        SevenZipProcessBackend backend(executable); auto r = addRequest(temp.filePath("volumes.7z")); r.volume = "4m";
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details)); QVERIFY(QFileInfo::exists(r.archive + ".001")); QVERIFY(QFileInfo::exists(r.archive + ".002"));
        r.archive += ".001"; r.operation = ArchiveOperation::Test; r.files.clear(); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.operation = ArchiveOperation::Extract; r.files = {"input/ASCII.txt"}; r.outputDirectory = temp.filePath("readonly-out"); r.overwriteMode = "overwrite"; writeFile(r.outputDirectory + "/input/ASCII.txt", "read-only-original");
        // Original DeleteFileAlways clears read-only before authorized overwrite;
        // the official macOS console also replaces this leaf on a writable parent.
        QVERIFY(QFile::setPermissions(r.outputDirectory + "/input/ASCII.txt", QFileDevice::ReadOwner)); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(hash(r.outputDirectory + "/input/ASCII.txt"), hash(input + "/ASCII.txt"));
        QFile::setPermissions(r.outputDirectory + "/input/ASCII.txt", QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    void cancellationAndResponsiveness() {
        SevenZipProcessBackend backend(executable); auto r = addRequest(temp.filePath("cancelled.7z")); r.level = 9; r.dictionary = "64m"; r.threads = 1;
        QSignalSpy spy(&backend, &ArchiveBackend::finished); int heartbeats = 0; QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, this, [&] { ++heartbeats; }); heartbeat.start();
        backend.start(r); QTimer::singleShot(350, &backend, &SevenZipProcessBackend::cancel); QVERIFY(spy.wait(15000)); auto result = qvariant_cast<ArchiveResult>(spy[0][0]); QVERIFY(result.cancelled); QVERIFY(heartbeats >= 5); QVERIFY(!backend.busy()); QVERIFY(!QFileInfo::exists(r.archive));
        r.operation = ArchiveOperation::Test; r.archive = temp.filePath("roundtrip.7z"); QVERIFY(execute(backend, r).success);
    }
    void nestedArchiveNavigation() {
        const auto oldSettings = FileManagerSettings::load(); auto working = oldSettings; working.workMode = 1; working.removableOnly = false; working.viewer = "/usr/bin/true"; QVERIFY(working.save()); const auto restore = qScopeGuard([&] { oldSettings.save(); });
        const auto root = temp.filePath("nested navigation"); const auto outer = root + "/outer.7z", middle = root + "/container/middle.zip", inner = root + "/middle-input/日本語/inside.data";
        writeFile(root + "/inner-input/data/日本語 space.txt", "nested payload\n"); writeFile(root + "/middle-input/日本語/bad.zip", "not an archive");
        SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = inner; r.workingDirectory = root + "/inner-input"; r.files = {"data"}; r.level = 1;
        QDir().mkpath(QFileInfo(inner).absolutePath()); QVERIFY(execute(builder, r).success);
        r.archive = middle; r.format = "zip"; r.method = "Deflate"; r.workingDirectory = root + "/middle-input"; r.files = {"日本語"}; QDir().mkpath(QFileInfo(middle).absolutePath()); QVERIFY(execute(builder, r).success);
        r.archive = outer; r.format = "7z"; r.method = "LZMA2"; r.workingDirectory = root; r.files = {"container"}; QVERIFY(execute(builder, r).success); const auto original = hash(outer);
        MainWindow window(executable); auto tree = panelWidget<FileList>(window, "fileList"); auto address = panelWidget<QLineEdit>(window, "currentPath"); auto backend = window.findChild<SevenZipProcessBackend *>();
        auto focus = [&](QString name) { const auto items = tree->findItems(name, Qt::MatchExactly); if (items.size() != 1) return false; tree->clearSelection(); tree->setCurrentItem(items[0]); items[0]->setSelected(true); return true; };
        window.openPath(outer); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY(focus("container")); window.findChild<QAction *>("openAction")->trigger(); QVERIFY(focus("middle.zip"));
        // Deliver the real list activation signal; no desktop focus is needed.
        emit tree->itemDoubleClicked(tree->currentItem(), 0); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(window.currentArchivePath() != outer); const auto middleTemporary = window.currentArchivePath(); QVERIFY(QFileInfo::exists(middleTemporary));
        QCOMPARE(address->text(), outer + "/container/middle.zip/"); QVERIFY(focus("日本語")); window.findChild<QAction *>("openAction")->trigger(); QVERIFY(focus("inside.data")); window.findChild<QAction *>("insideAction")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); const auto innerTemporary = window.currentArchivePath(); QVERIFY(innerTemporary != middleTemporary); QVERIFY(QFileInfo::exists(innerTemporary)); QCOMPARE(address->text(), outer + "/container/middle.zip/日本語/inside.data/");
        QVERIFY(window.findChild<QAction *>("folderAction")->isEnabled());
        QVERIFY(!window.findChild<QAction *>("editAction")->isEnabled()); // Focus is a folder, not a file.
        QVERIFY(focus("data")); window.findChild<QAction *>("openAction")->trigger(); QVERIFY(focus("日本語 space.txt")); window.findChild<QAction *>("storeBookmark7Action")->trigger(); const auto bookmark = address->text();
        QVERIFY(!bookmark.contains(".7zip-nested-")); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(address->text(), bookmark); QVERIFY(focus("日本語 space.txt"));
        window.findChild<QAction *>("viewAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY(window.findChild<QAction *>("editAction")->isEnabled());
        const auto externalFolders = QDir(root).entryList({".7zip-open-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot); QCOMPARE(externalFolders.size(), 1); const auto externalFile = root + '/' + externalFolders[0] + "/data/日本語 space.txt"; QCOMPARE(hash(externalFile), hash(root + "/inner-input/data/日本語 space.txt"));
        r = {}; r.archive = innerTemporary; r.operation = ArchiveOperation::Extract; r.outputDirectory = root + "/extracted"; r.files = {"data/日本語 space.txt"}; r.overwriteMode = "overwrite"; QVERIFY(execute(builder, r).success); QCOMPARE(hash(root + "/extracted/data/日本語 space.txt"), hash(root + "/inner-input/data/日本語 space.txt"));
        window.findChild<QAction *>("upAction")->trigger(); window.findChild<QAction *>("upAction")->trigger(); QCOMPARE(window.currentArchivePath(), middleTemporary); QCOMPARE(address->text(), outer + "/container/middle.zip/日本語/"); QCOMPARE(tree->currentItem()->text(0), QString("inside.data")); QVERIFY(!QFileInfo::exists(innerTemporary));
        QString error; QTimer dismiss; dismiss.setInterval(10); connect(&dismiss, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { error = box->text(); box->accept(); } }); dismiss.start(); QVERIFY(focus("bad.zip")); window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); dismiss.stop(); QVERIFY(error.contains("Exit code: 2")); QCOMPARE(window.currentArchivePath(), middleTemporary);
        QTimer::singleShot(50, &window, [&] { auto box = window.findChild<QMessageBox *>(); QVERIFY(box); box->accept(); }); const auto previous = address->text(); window.openPath(previous + "missing"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(address->text(), previous);
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.findChild<QAction *>("upAction")->trigger(); QCOMPARE(window.currentArchivePath(), outer); QCOMPARE(address->text(), outer + "/container/"); QCOMPARE(tree->currentItem()->text(0), QString("middle.zip")); QVERIFY(!QFileInfo::exists(middleTemporary));
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); int continuingStages = 0;
        const auto inspectStages = connect(backend, &ArchiveBackend::finished, &window, [&](ArchiveResult result) { if (result.operation == ArchiveOperation::List && result.success && window.operationBusy()) { QList<ProgressDialog *> visible; for (auto dialog : window.findChildren<ProgressDialog *>()) if (dialog->isVisible()) visible << dialog; QCOMPARE(visible.size(), 1); QCOMPARE(visible[0]->findChild<QPushButton *>("cancelOperation")->text(), QString("Cancel")); ++continuingStages; } });
        window.findChild<QAction *>("bookmark7Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); disconnect(inspectStages); QCOMPARE(continuingStages, 2); QCOMPARE(address->text(), bookmark); QVERIFY(focus("日本語 space.txt"));
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(window.currentArchivePath().isEmpty()); QCOMPARE(hash(outer), original); QVERIFY(!backend->busy()); QCOMPARE(hash(externalFile), hash(root + "/inner-input/data/日本語 space.txt"));
    }
    void nestedArchivePasswordsAndCancel() {
        const auto root = temp.filePath("nested passwords"), outer = root + "/outer.7z", inner = root + "/日本語 inner.7z"; QDir().mkpath(root);
        writeFile(root + "/payload.txt", "encrypted nested\n"); SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = inner; r.workingDirectory = root; r.files = {"payload.txt"}; r.password = "inner secret"; r.encryptNames = true; r.level = 1; QVERIFY(execute(builder, r).success);
        r.archive = outer; r.files = {"日本語 inner.7z"}; r.password = "outer secret"; QVERIFY(execute(builder, r).success); const auto original = hash(outer);
        MainWindow window(executable); auto backend = window.findChild<SevenZipProcessBackend *>(); auto tree = panelWidget<FileList>(window, "fileList"); auto address = panelWidget<QLineEdit>(window, "currentPath");
        QStringList passwords{"outer secret", "wrong inner", "inner secret"}; QTimer answer; answer.setInterval(10); connect(&answer, &QTimer::timeout, &window, [&] { for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { QVERIFY(!passwords.isEmpty()); dialog->setTextValue(passwords.takeFirst()); dialog->accept(); break; } }); answer.start();
        window.openPath(outer); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto items = tree->findItems("日本語 inner.7z", Qt::MatchExactly); QCOMPARE(items.size(), 1); tree->setCurrentItem(items[0]); items[0]->setSelected(true); window.findChild<QAction *>("insideAction")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(passwords.isEmpty()); QVERIFY(tree->findItems("payload.txt", Qt::MatchExactly).size() == 1); QVERIFY(!address->text().contains(".7zip-nested-")); answer.stop(); window.findChild<QAction *>("upAction")->trigger(); QCOMPARE(window.currentArchivePath(), outer);
        QSignalSpy finished(backend, &ArchiveBackend::finished); bool cancelledFromDialog = false;
        bool cancellationArmed = false;
        const auto cancelListing = [&] {
            if (!cancellationArmed || !backend->canPause()) return;
            cancellationArmed = false;
            // Pause the owned listing before answering the question. A tiny
            // archive can otherwise finish while Cancel confirmation is open;
            // the original completion policy correctly suppresses late Cancel.
            QVERIFY(backend->setPaused(true));
            auto progress = window.findChild<ProgressDialog *>("progressDialog"); QVERIFY(progress);
            auto button = progress->findChild<QPushButton *>("cancelOperation"); QCOMPARE(button->text(), QString("Cancel"));
            QSignalSpy cancelSignal(progress, &ProgressDialog::cancelRequested);
            confirmProgressCancellation(progress); button->click(); cancelledFromDialog = true;
            QCOMPARE(cancelSignal.size(), 1);
        };
        const auto listingReady = connect(backend, &ArchiveBackend::pauseAvailabilityChanged, &window, [&](bool available) { if (available) cancelListing(); });
        const auto cancelAtListing = connect(backend, &ArchiveBackend::finished, &window, [&](ArchiveResult result) { if (result.operation == ArchiveOperation::Extract && result.success) { cancellationArmed = true; cancelListing(); } });
        window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); disconnect(cancelAtListing); disconnect(listingReady); QVERIFY(cancelledFromDialog); QVERIFY(!finished.isEmpty());
        // Closing the completed modal dialog allows Auto Refresh to re-list
        // the unchanged parent. Identify the cancelled operation by result,
        // rather than mistaking that later refresh for the cancelled job.
        bool sawCancelledListing = false; for (const auto &args : finished) { const auto value = qvariant_cast<ArchiveResult>(args[0]); sawCancelledListing |= value.operation == ArchiveOperation::List && value.cancelled; } QVERIFY(sawCancelledListing); QCOMPARE(window.currentArchivePath(), outer);
        QTimer reject; reject.setInterval(10); connect(&reject, &QTimer::timeout, &window, [&] { for (auto dialog : testInputs(&window)) if (dialog->isVisible()) dialog->reject(); }); reject.start(); window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); reject.stop(); QCOMPARE(window.currentArchivePath(), outer); QVERIFY(tree->isEnabled());
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(hash(outer), original); QVERIFY(execute(builder, ArchiveRequest{ArchiveOperation::Test, outer, {}, {}, "outer secret"}).success);
    }
    void archiveRefreshPreservesFolder_data() { QTest::addColumn<bool>("encrypted"); QTest::newRow("plain") << false; QTest::newRow("encrypted-headers") << true; }
    void archiveRefreshPreservesFolder() {
        QFETCH(bool, encrypted); const auto root = temp.filePath("archive refresh " + QString::number(encrypted)), archive = root + "/outer.7z";
        writeFile(root + "/folder/payload.txt", "original content"); SevenZipProcessBackend builder(executable); ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = archive; request.workingDirectory = root; request.files = {"folder"}; request.level = 1;
        if (encrypted) { request.password = "refresh header secret"; request.encryptNames = true; } QVERIFY(execute(builder, request).success);
        MainWindow window(executable); auto backend = window.findChild<SevenZipProcessBackend *>(); auto tree = panelWidget<FileList>(window, "fileList"); auto address = panelWidget<QLineEdit>(window, "currentPath");
        int prompts = 0; QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto input : testInputs(&window)) if (input->isVisible()) { ++prompts; input->setTextValue(request.password); input->accept(); return; } }); dialogs.start();
        window.openPath(archive + "/folder/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(address->text(), archive + "/folder/"); QVERIFY(tree->findItems("payload.txt", Qt::MatchExactly).size() == 1);
        const auto added = QString("folder/追加 space.txt"); writeFile(root + '/' + added, "new external archive content"); request.files = {added}; QVERIFY(execute(builder, request).success);
        QSignalSpy reloaded(backend, &ArchiveBackend::finished); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!reloaded.isEmpty());
        const auto result = qvariant_cast<ArchiveResult>(reloaded.first()[0]); QCOMPARE(result.operation, ArchiveOperation::List); QVERIFY(result.success); QCOMPARE(address->text(), archive + "/folder/");
        const auto rows = tree->findItems("追加 space.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 1); QCOMPARE(rows[0]->data(1, Qt::UserRole + 3).toULongLong(), quint64(28)); QCOMPARE(prompts, encrypted ? 1 : 0);
    }
    void archivePauseResumeAndCancel() {
        const auto root = temp.filePath("pause gui"), source = root + "/pause.bin"; QDir().mkpath(root); QVERIFY(QFile::copy(input + "/large.bin", source));
        MainWindow window(executable); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished);
        int ticks = 0; QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, &window, [&] { ++ticks; }); heartbeat.start();
        window.executeOpenWith("7z", {source}); auto progress = window.findChild<ProgressDialog *>("progressDialog"); QVERIFY(progress); auto pause = progress->findChild<QPushButton *>("pauseOperation"); QVERIFY(pause); QTRY_VERIFY_WITH_TIMEOUT(pause->isEnabled(), 5000);
        QTest::mouseClick(pause, Qt::LeftButton); QCOMPARE(pause->text(), UiLanguage::resource(411)); QVERIFY(progress->windowTitle().startsWith("Paused")); const auto before = ticks; QTest::qWait(220); QVERIFY(done.isEmpty()); QVERIFY(ticks >= before + 10); QVERIFY(window.operationBusy());
        QTest::mouseClick(pause, Qt::LeftButton); QCOMPARE(pause->text(), UiLanguage::resource(446)); QVERIFY(done.wait(30000)); QVERIFY(qvariant_cast<ArchiveResult>(done.last()[0]).success); QVERIFY(!pause->isEnabled());
        SevenZipProcessBackend verifier(executable); ArchiveRequest r; r.operation = ArchiveOperation::Extract; r.archive = root + "/pause.7z"; r.outputDirectory = root + "/restored"; r.overwriteMode = "overwrite"; QVERIFY(execute(verifier, r).success); QCOMPARE(hash(source), hash(root + "/restored/pause.bin"));
        const auto cancelSource = root + "/cancel.bin"; QVERIFY(QFile::copy(source, cancelSource)); done.clear(); window.executeOpenWith("7z", {cancelSource});
        // The old dialog has deferred deletion; locate the one still running.
        progress = nullptr; for (auto dialog : window.findChildren<ProgressDialog *>()) if (dialog->findChild<QPushButton *>("cancelOperation")->text() == "Cancel") progress = dialog;
        QVERIFY(progress); pause = progress->findChild<QPushButton *>("pauseOperation"); QTRY_VERIFY_WITH_TIMEOUT(pause->isEnabled(), 5000); QTest::mouseClick(pause, Qt::LeftButton); QCOMPARE(pause->text(), UiLanguage::resource(411)); confirmProgressCancellation(progress); QTest::mouseClick(progress->findChild<QPushButton *>("cancelOperation"), Qt::LeftButton);
        if (done.isEmpty()) QVERIFY(done.wait(10000)); QVERIFY(qvariant_cast<ArchiveResult>(done.last()[0]).cancelled); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!QFileInfo::exists(root + "/cancel.7z")); QCOMPARE(hash(cancelSource), hash(source));
        r = {}; r.operation = ArchiveOperation::Test; r.archive = root + "/pause.7z"; QVERIFY(execute(verifier, r).success);
    }
    void filePauseResumeAndCancel() {
        const auto root = temp.filePath("paused file jobs"); FileOperations jobs; QSignalSpy done(&jobs, &FileOperations::finished); int pauses = 0;
        const auto pauseOnChunk = connect(&jobs, &FileOperations::byteProgress, &jobs, [&](quint64 bytes, quint64, QString) { if (bytes == (1 << 20)) QMetaObject::invokeMethod(&jobs, [&] { QVERIFY(jobs.setPaused(true)); ++pauses; }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
        jobs.split(input + "/large.bin", root + "/parts", {4 << 20}); QTRY_COMPARE_WITH_TIMEOUT(pauses, 1, 5000); QTest::qWait(150); QVERIFY(done.isEmpty()); QVERIFY(jobs.busy()); QVERIFY(jobs.setPaused(false)); QVERIFY(done.wait(10000)); QVERIFY(done.last()[0].toString().isEmpty()); disconnect(pauseOnChunk);
        pauses = 0; done.clear(); const auto pauseCombine = connect(&jobs, &FileOperations::byteProgress, &jobs, [&](quint64 bytes, quint64, QString) { if (bytes == (1 << 20)) QMetaObject::invokeMethod(&jobs, [&] { QVERIFY(jobs.setPaused(true)); ++pauses; }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
        jobs.combine(root + "/parts/large.bin.001", root + "/out"); QTRY_COMPARE_WITH_TIMEOUT(pauses, 1, 5000); QTest::qWait(150); QVERIFY(done.isEmpty()); jobs.cancel(); QVERIFY(done.wait(10000)); QVERIFY(done.last()[0].toString().contains("cancelled")); QVERIFY(!QFileInfo::exists(root + "/out/large.bin")); disconnect(pauseCombine);
        done.clear(); jobs.combine(root + "/parts/large.bin.001", root + "/out"); QVERIFY(done.wait(10000)); QVERIFY(done.last()[0].toString().isEmpty()); QCOMPARE(hash(input + "/large.bin"), hash(root + "/out/large.bin"));
        pauses = 0; done.clear(); QDir().mkpath(root + "/copy");
        const auto pauseTransfer = connect(&jobs, &FileOperations::progress, &jobs, [&](QString) { QMetaObject::invokeMethod(&jobs, [&] { QVERIFY(jobs.setPaused(true)); ++pauses; }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
        jobs.transfer({input + "/large.bin"}, root + "/copy", false); QTRY_COMPARE_WITH_TIMEOUT(pauses, 1, 5000); QTest::qWait(100); QVERIFY(done.isEmpty()); QVERIFY(!QFileInfo::exists(root + "/copy/large.bin")); QVERIFY(jobs.setPaused(false)); QVERIFY(done.wait(10000)); QVERIFY(done.last()[0].toString().isEmpty()); QCOMPARE(hash(input + "/large.bin"), hash(root + "/copy/large.bin"));
        pauses = 0; done.clear(); QDir().mkpath(root + "/move"); jobs.transfer({root + "/copy/large.bin"}, root + "/move", true); QTRY_COMPARE_WITH_TIMEOUT(pauses, 1, 5000); QTest::qWait(100); QVERIFY(done.isEmpty()); QVERIFY(QFileInfo::exists(root + "/copy/large.bin")); QVERIFY(!QFileInfo::exists(root + "/move/large.bin")); jobs.cancel(); QVERIFY(done.wait(10000)); QVERIFY(done.last()[0].toString().contains("cancelled")); QVERIFY(QFileInfo::exists(root + "/copy/large.bin")); disconnect(pauseTransfer);
    }
    void clipboardNamesAndUpstreamNoOps() {
        const auto twoPanels = QSettings().value("View/TwoPanels", false); QSettings().setValue("View/TwoPanels", false);
        const auto restorePanels = qScopeGuard([&] { QSettings().setValue("View/TwoPanels", twoPanels); });
        auto clipboard = QApplication::clipboard(); auto original = std::make_shared<QMimeData>();
        for (const auto &format : clipboard->mimeData()->formats()) original->setData(format, clipboard->mimeData()->data(format));
        QString owned; const auto restore = qScopeGuard([&] { if (clipboard->text() == owned) { auto restored = new QMimeData; for (const auto &format : original->formats()) restored->setData(format, original->data(format)); clipboard->setMimeData(restored); } });
        const auto root = temp.filePath("clipboard names"); writeFile(root + "/ASCII.txt", "clipboard ASCII"); writeFile(root + "/日本語 space.txt", "clipboard UTF-8"); QDir().mkpath(root + "/empty folder");
        MainWindow window(executable); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto tree = panelWidget<FileList>(window, "fileList"); auto icons = panelWidget<IconFileList>(window, "iconFileList");
        auto select = [&](QStringList names) { tree->clearSelection(); for (int n = 0; n < tree->topLevelItemCount(); ++n) { auto item = tree->topLevelItem(n); if (names.contains(item->text(0))) item->setSelected(true); } };
        const auto filesystemNames = QDir(root).entryList(QDir::Files | QDir::NoDotAndDotDot, QDir::Unsorted); QCOMPARE(filesystemNames.size(), 2);
        select({"ASCII.txt", "日本語 space.txt", ".."}); const auto expectedFilesystemNames = filesystemNames.join("\r\n"); QTest::keyClick(tree, Qt::Key_C, Qt::ControlModifier); owned = clipboard->text(); QCOMPARE(owned, expectedFilesystemNames); QVERIFY(!clipboard->mimeData()->hasUrls());
        QTest::keyClick(tree, Qt::Key_X, Qt::ControlModifier); QTest::keyClick(tree, Qt::Key_V, Qt::ControlModifier); QCOMPARE(clipboard->text(), owned); QVERIFY(QFileInfo::exists(root + "/ASCII.txt")); QCOMPARE(QDir(root).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size(), 3);
        select({"empty folder"}); window.findChild<QAction *>("mode0Action")->trigger(); owned = "empty folder"; QTest::keyClick(icons, Qt::Key_C, Qt::ControlModifier); QCOMPARE(clipboard->text(), owned);
        window.findChild<QAction *>("twoPanelsAction")->trigger(); auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second); second->openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto otherTree = second->findChild<FileList *>("fileList"); auto otherRows = otherTree->findItems("space in name.txt", Qt::MatchExactly); QCOMPARE(otherRows.size(), 1); otherTree->clearSelection(); otherTree->setCurrentItem(otherRows[0]); otherRows[0]->setSelected(true); owned = "space in name.txt"; QTest::keyClick(otherTree, Qt::Key_C, Qt::ControlModifier); QCOMPARE(clipboard->text(), owned);
        SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = temp.filePath("clipboard.7z"); r.workingDirectory = root; r.files = {"ASCII.txt", "日本語 space.txt", "empty folder"}; QVERIFY(execute(builder, r).success);
        // PanelMenu::EditCopy uses Get_ItemIndices_Selected: original folder
        // indices, independently of the panel's current display sort.
        r.operation = ArchiveOperation::List; r.files.clear(); const auto listed = execute(builder, r); QVERIFY(listed.success); QVERIFY(listed.metadata.native); QVERIFY(!listed.metadata.directories.isEmpty());
        QStringList nativeNames; for (const auto &row : listed.metadata.directories.first().children) if (row.name == "ASCII.txt" || row.name == "日本語 space.txt") nativeNames.append(row.name); QCOMPARE(nativeNames.size(), 2);
        window.openPath(r.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); select({"ASCII.txt", "日本語 space.txt"}); const auto expectedNames = nativeNames.join("\r\n"); QTest::keyClick(icons, Qt::Key_C, Qt::ControlModifier); owned = clipboard->text();
        QCOMPARE(owned, expectedNames); QVERIFY(!clipboard->mimeData()->hasUrls());
        select({}); owned.clear(); QTest::keyClick(tree, Qt::Key_C, Qt::ControlModifier); QCOMPARE(clipboard->text(), owned);
    }
    void archiveContentsHashes_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("encrypted"); QTest::addColumn<bool>("headers");
        QTest::newRow("7z") << QString("7z") << false << false; QTest::newRow("zip") << QString("zip") << false << false;
        QTest::newRow("7z-password") << QString("7z") << true << false; QTest::newRow("7z-headers") << QString("7z") << true << true; QTest::newRow("zip-AES") << QString("zip") << true << false;
    }
    void archiveContentsHashes() {
        QFETCH(QString, format); QFETCH(bool, encrypted); QFETCH(bool, headers);
        const auto twoPanels = QSettings().value("View/TwoPanels", false), flat = QSettings().value("View/Flat", false);
        QSettings().setValue("View/TwoPanels", false); QSettings().setValue("View/Flat", false);
        const auto restoreView = qScopeGuard([&] { QSettings().setValue("View/TwoPanels", twoPanels); QSettings().setValue("View/Flat", flat); });
        const auto root = temp.filePath("archive crc " + format + QString::number(encrypted) + QString::number(headers)); const auto name = QString("日本語 [*] space.txt"), source = root + "/folder/" + name, archive = root + "/data." + format;
        writeFile(source, "selected content hash\n"); writeFile(root + "/folder/other.txt", "other payload\n"); QDir().mkpath(root + "/folder/empty"); SevenZipProcessBackend builder(executable);
        ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive; r.format = format; r.method = format == "zip" ? "Deflate" : "LZMA2"; r.workingDirectory = root; r.files = {"folder"}; r.level = 1; r.encryptNames = headers; if (encrypted) r.password = "hash password"; QVERIFY(execute(builder, r).success); const auto archiveHash = hash(archive);
        MainWindow window(executable); auto tree = panelWidget<FileList>(window, "fileList"); auto backend = window.findChild<SevenZipProcessBackend *>();
        QStringList answers; if (encrypted) answers = {"wrong hash password", "hash password"}; int prompts = 0; QTimer password; password.setInterval(10); connect(&password, &QTimer::timeout, &window, [&] { for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { QVERIFY(!answers.isEmpty()); ++prompts; dialog->setTextValue(answers.takeFirst()); dialog->accept(); break; } }); password.start();
        window.openPath(archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); const auto folders = tree->findItems("folder", Qt::MatchExactly); QCOMPARE(folders.size(), 1); tree->setCurrentItem(folders[0]); folders[0]->setSelected(true); window.findChild<QAction *>("openAction")->trigger();
        const auto rows = tree->findItems(name, Qt::MatchExactly); QCOMPARE(rows.size(), 1); tree->clearSelection(); tree->setCurrentItem(rows[0]); rows[0]->setSelected(true);
        auto sums = [](QString text) { QMap<QString, QString> result; auto matches = QRegularExpression("(?:^|\\n)([A-Za-z0-9_-]+)[ \\t]+for data:[ \\t]+([0-9A-Fa-f]+)").globalMatch(text); while (matches.hasNext()) { auto m = matches.next(); result[m.captured(1)] = m.captured(2).toLower(); } return result; };
        for (const auto &method : checksumMethods()) {
            ArchiveRequest reference; reference.operation = ArchiveOperation::Hash; reference.archive = source; reference.workingDirectory = root; reference.files = {"folder/" + name}; reference.hashMethod = method.second; auto expected = execute(builder, reference); QVERIFY(expected.success); const auto expectedSums = sums(expected.details); QVERIFY(!expectedSums.isEmpty());
            auto action = window.findChild<QAction *>("hash" + method.second + "Action"); QVERIFY(action->isEnabled()); QSignalSpy done(backend, &ArchiveBackend::finished); action->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!done.isEmpty()); auto result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY2(result.success, qPrintable(method.first + result.message + result.details)); QCOMPARE(result.operation, ArchiveOperation::HashArchive); QCOMPARE(sums(result.details), expectedSums);
            if (method.second == "SHA256") QVERIFY(result.details.contains(QString::fromLatin1(hash(source).toHex()), Qt::CaseInsensitive));
        }
        QCOMPARE(prompts, encrypted ? 2 : 0); QVERIFY(answers.isEmpty()); password.stop(); QCOMPARE(hash(archive), archiveHash);
        window.findChild<QAction *>("upAction")->trigger(); auto parent = tree->findItems("folder", Qt::MatchExactly); QCOMPARE(parent.size(), 1); tree->clearSelection(); tree->setCurrentItem(parent[0]); parent[0]->setSelected(true);
        ArchiveRequest reference; reference.operation = ArchiveOperation::Hash; reference.archive = root + "/folder"; reference.workingDirectory = root; reference.files = {"folder"}; reference.hashMethod = "SHA256"; const auto expected = execute(builder, reference); QVERIFY(expected.success);
        QSignalSpy directoryDone(backend, &ArchiveBackend::finished); window.findChild<QAction *>("hashSHA256Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!directoryDone.isEmpty()); auto directoryResult = qvariant_cast<ArchiveResult>(directoryDone.last()[0]); QVERIFY2(directoryResult.success, qPrintable(directoryResult.details)); QCOMPARE(sums(directoryResult.details), sums(expected.details)); QCOMPARE(hash(archive), archiveHash);
    }
    void archiveFileReplacement_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<int>("encryption");
        for (const auto &format : {"7z", "zip", "tar", "wim", "gzip", "bzip2", "xz"}) QTest::newRow(format) << QString(format) << 0;
        QTest::newRow("7z-data") << QString("7z") << 1; QTest::newRow("7z-headers") << QString("7z") << 2;
        QTest::newRow("zip-AES") << QString("zip") << 1; QTest::newRow("zip-ZipCrypto") << QString("zip") << 2;
    }
    void archiveFileReplacement() {
        QFETCH(QString, format); QFETCH(int, encryption);
        const auto root = temp.filePath("file replacement " + format + QString::number(encryption)), archive = root + "/payload." + format;
        const bool stream = QStringList{"gzip", "bzip2", "xz"}.contains(format);
        const auto name = stream ? QString("payload") : QString("prefix/日本語 [*] space.txt");
        writeFile(root + "/input/" + name, "old archived bytes\n"); writeFile(root + "/input/unrelated.txt", "unchanged bytes\n");
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive; r.workingDirectory = root + "/input"; r.files = stream ? QStringList{name} : QStringList{"prefix", "unrelated.txt"};
        r.format = format; r.method = format == "zip" ? "Deflate" : format == "tar" ? "posix" : "LZMA2"; r.level = 1;
        if (encryption) r.password = "parent replacement secret"; r.encryptNames = format == "7z" && encryption == 2; if (format == "zip" && encryption == 2) r.encryptionMethod = "ZipCrypto";
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        r.operation = ArchiveOperation::List; r.files.clear(); result = execute(backend, r); QVERIFY(result.success); QString target;
        for (const auto &entry : result.entries) if (!entry.directory && (stream || entry.path == name)) target = entry.path;
        QVERIFY(!target.isEmpty());
        const auto replacement = root + "/external editor file"; writeFile(replacement, "new archived replacement bytes 日本語\n"); const auto old = hash(archive);
        r.operation = ArchiveOperation::ReplaceFile; r.files = {target}; r.replacementSource = replacement;
        result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(hash(archive) != old);
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/restored"; r.overwriteMode = "overwrite"; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(hash(root + "/restored/" + target), hash(replacement)); if (!stream) QCOMPARE(hash(root + "/restored/unrelated.txt"), hash(root + "/input/unrelated.txt"));
        // Replace even when the size matches and the new timestamp is older.
        const auto length = QFileInfo(replacement).size(); writeFile(replacement, QByteArray(length, 'x'));
        QFile older(replacement); QVERIFY(older.open(QIODevice::ReadWrite)); QVERIFY(older.setFileTime(QDateTime::currentDateTimeUtc().addYears(-3), QFileDevice::FileModificationTime)); older.close();
        r.operation = ArchiveOperation::ReplaceFile; r.files = {target}; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/same size restored"; result = execute(backend, r); QVERIFY(result.success); QCOMPARE(hash(r.outputDirectory + '/' + target), hash(replacement));
        const auto updated = hash(archive); r.operation = ArchiveOperation::ReplaceFile; r.files = {"missing"}; result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(archive), updated);
        r.files = {target}; r.replacementSource = root + "/linked-input"; QVERIFY(::symlink(QFile::encodeName(replacement).constData(), QFile::encodeName(r.replacementSource).constData()) == 0); result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(archive), updated);
        if (encryption) {
            r.replacementSource = replacement; r.password = "different replacement secret"; result = execute(backend, r);
            // Original Agent UpdateOneFile supplies a write password. Headers
            // need the old read password; otherwise the replaced item's own
            // new block can use a different password without decoding unrelated
            // NoChange blocks. Do not require an unrelated whole-archive Test.
            if (format == "7z" && encryption == 2) { QVERIFY(!result.success); QCOMPARE(hash(archive), updated); }
            else {
                QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(hash(archive) != updated);
                auto selected = r; selected.operation = ArchiveOperation::Extract; selected.files = {target};
                selected.outputDirectory = root + "/different password restored";
                auto restored = execute(backend, selected); QVERIFY2(restored.success, qPrintable(restored.message + restored.details));
                QCOMPARE(hash(selected.outputDirectory + '/' + target), hash(replacement));
                selected.files = {"unrelated.txt"}; selected.password = "parent replacement secret";
                selected.outputDirectory = root + "/original password sibling";
                restored = execute(backend, selected); QVERIFY2(restored.success, qPrintable(restored.message + restored.details));
                QCOMPARE(hash(selected.outputDirectory + "/unrelated.txt"), hash(root + "/input/unrelated.txt"));
            }
            QVERIFY(!result.details.contains(r.password));
        }
        QVERIFY(QDir(root).entryList({".7zip-replace-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }
    void archiveReplacementConcurrentEdit() {
        const auto root = temp.filePath("replacement concurrency"), archive = root + "/data.7z", replacement = root + "/modified file.txt";
        writeFile(root + "/file.txt", "original payload"); writeFile(replacement, "updated payload with more bytes");
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive; r.workingDirectory = root; r.files = {"file.txt"}; QVERIFY(execute(backend, r).success); const auto original = hash(archive);
        r.operation = ArchiveOperation::ReplaceFile; r.replacementSource = replacement; int checks = 0;
        const auto editBeforeInstall = connect(&backend, &ArchiveBackend::mutationCheckpoint, &backend, [&](QString phase, QString target) { if (phase == "before-install" && target == archive) { ++checks; writeFile(replacement, "editor changed file again while verifying"); } });
        auto result = execute(backend, r); disconnect(editBeforeInstall); QCOMPARE(checks, 1); QVERIFY(!result.success); QVERIFY(result.message.contains("Replacement file changed")); QCOMPARE(hash(archive), original);
        const auto fifo = root + "/fifo"; QVERIFY(::mkfifo(QFile::encodeName(fifo).constData(), 0600) == 0); r.replacementSource = fifo; result = execute(backend, r); QVERIFY(!result.success); QVERIFY(result.message.contains("regular file")); QCOMPARE(hash(archive), original);
    }
    void nestedArchiveCancelledWriteBack() {
        const auto oldSettings = FileManagerSettings::load(); auto settings = oldSettings; settings.workMode = 1; settings.removableOnly = false; QVERIFY(settings.save()); const auto restore = qScopeGuard([&] { oldSettings.save(); });
        const auto root = temp.filePath("nested cancel recovery"), outer = root + "/outer.7z", inner = root + "/inner.7z";
        writeFile(root + "/payload.txt", "recoverable original content"); QVERIFY(QFile::copy(input + "/large.bin", root + "/large.bin"));
        SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = inner; r.workingDirectory = root; r.files = {"payload.txt"}; r.level = 0; QVERIFY(execute(builder, r).success);
        r.archive = outer; r.files = {"inner.7z", "large.bin"}; QVERIFY(execute(builder, r).success); const auto original = hash(outer); QString recovery, message;
        {
            MainWindow window(executable); auto backend = window.findChild<SevenZipProcessBackend *>(); QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] {
                for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { dialog->setTextValue("recovered empty"); dialog->accept(); return; }
                for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) box->button(QMessageBox::Yes)->click(); else { message = box->text(); box->accept(); } return; }
            }); dialogs.start(); window.openPath(outer + "/inner.7z/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); window.findChild<QAction *>("folderAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); recovery = window.currentArchivePath(); const auto edited = hash(recovery); QVERIFY(!edited.isEmpty());
            // Read-only status of any containing archive blocks inner mutations.
            QVERIFY(QFile::setPermissions(outer, QFileDevice::ReadOwner)); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!window.findChild<QAction *>("folderAction")->isEnabled()); QVERIFY(QFile::setPermissions(outer, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
            int cancellations = 0; const auto cancelCopy = connect(backend, &ArchiveBackend::progress, backend, [&](int, quint64 bytes, quint64, QString target) { if (bytes == (1 << 20) && target == outer) QMetaObject::invokeMethod(backend, [&] { ++cancellations; backend->cancel(); }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
            window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); disconnect(cancelCopy); dialogs.stop(); QCOMPARE(cancellations, 1); QVERIFY(message.contains("retained")); QVERIFY(message.contains("Exit code: 255")); QVERIFY(message.contains(recovery)); QCOMPARE(window.currentArchivePath(), outer); QCOMPARE(hash(outer), original); QCOMPARE(hash(recovery), edited);
        }
        QVERIFY(QFileInfo::exists(recovery)); QVERIFY(recovery.startsWith(root + "/.7zip-nested-"));
        r = {}; r.operation = ArchiveOperation::Extract; r.archive = recovery; r.outputDirectory = root + "/recovered files"; r.overwriteMode = "overwrite"; QVERIFY(execute(builder, r).success); QVERIFY(QFileInfo(r.outputDirectory + "/recovered empty").isDir()); QCOMPARE(hash(r.outputDirectory + "/payload.txt"), hash(root + "/payload.txt"));
        QVERIFY(QDir(QFileInfo(recovery).absolutePath()).removeRecursively());
        QVERIFY(QDir(root).entryList({".7zip-replace-*", ".7zip-nested-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }
    void nestedStreamWriteBack_data() {
        QTest::addColumn<QString>("format"); for (const auto &format : {"gzip", "bzip2", "xz"}) QTest::newRow(format) << QString(format);
    }
    void nestedStreamWriteBack() {
        QFETCH(QString, format); const auto root = temp.filePath("nested tar stream " + format), tar = root + "/archive.tar", stream = root + "/archive.tar." + format;
        writeFile(root + "/payload.txt", "stream nested payload"); SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = tar; r.format = "tar"; r.method = "posix"; r.workingDirectory = root; r.files = {"payload.txt"}; QVERIFY(execute(builder, r).success);
        r.archive = stream; r.format = format; r.method = "LZMA2"; r.files = {"archive.tar"}; QVERIFY(execute(builder, r).success);
        MainWindow window(executable); QTimer dialogs; dialogs.setInterval(10); QString error; int updates = 0;
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { dialog->setTextValue("stream empty"); dialog->accept(); return; }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) { ++updates; box->button(QMessageBox::Yes)->click(); } else { error = box->text(); box->accept(); } return; }
        }); dialogs.start(); window.openPath(stream + "/archive.tar/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QVERIFY(window.findChild<QAction *>("folderAction")->isEnabled()); window.findChild<QAction *>("folderAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); dialogs.stop(); QCOMPARE(updates, 1); QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(window.currentArchivePath(), stream);
        r = {}; r.operation = ArchiveOperation::Extract; r.archive = stream; r.outputDirectory = root + "/restored stream"; r.overwriteMode = "overwrite"; QVERIFY(execute(builder, r).success); r.archive = r.outputDirectory + "/archive.tar"; r.outputDirectory = root + "/restored tar"; QVERIFY(execute(builder, r).success); QVERIFY(QFileInfo(r.outputDirectory + "/stream empty").isDir()); QCOMPARE(hash(r.outputDirectory + "/payload.txt"), hash(root + "/payload.txt"));
    }
    void nestedArchiveWriteBack_data() {
        QTest::addColumn<QString>("navigation"); QTest::addColumn<int>("answer"); QTest::addColumn<bool>("encrypted");
        QTest::newRow("up-yes") << QString("up") << int(QMessageBox::Yes) << false;
        QTest::newRow("address-yes") << QString("address") << int(QMessageBox::Yes) << false;
        QTest::newRow("ancestor-yes") << QString("ancestor") << int(QMessageBox::Yes) << false;
        QTest::newRow("close-yes-encrypted") << QString("close") << int(QMessageBox::Yes) << true;
        QTest::newRow("up-no") << QString("up") << int(QMessageBox::No) << false;
        QTest::newRow("up-cancel") << QString("up") << int(QMessageBox::Cancel) << false;
        QTest::newRow("close-no") << QString("close") << int(QMessageBox::No) << false;
        QTest::newRow("close-cancel") << QString("close") << int(QMessageBox::Cancel) << false;
    }
    void nestedArchiveWriteBack() {
        QFETCH(QString, navigation); QFETCH(int, answer); QFETCH(bool, encrypted);
        const auto view = QSettings().value("View/TwoPanels", false); QSettings().setValue("View/TwoPanels", false); const auto restore = qScopeGuard([&] { QSettings().setValue("View/TwoPanels", view); });
        const auto root = temp.filePath("nested writeback " + navigation + QString::number(answer)), outer = root + "/outer.7z", middle = root + "/middle.zip", inner = root + "/日本語 inner.7z";
        writeFile(root + "/payload.txt", "nested preserved content\n"); writeFile(root + "/remove.txt", "removed from child\n"); SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = inner; r.workingDirectory = root; r.files = {"payload.txt", "remove.txt"}; r.level = 1;
        if (encrypted) { r.password = "inner secret"; r.encryptNames = true; } QVERIFY(execute(builder, r).success);
        r.archive = middle; r.format = "zip"; r.method = "Deflate"; r.files = {QFileInfo(inner).fileName()}; if (encrypted) r.password = "middle secret"; QVERIFY(execute(builder, r).success);
        r.archive = outer; r.format = "7z"; r.method = "LZMA2"; r.files = {"middle.zip"}; if (encrypted) r.password = "outer secret"; QVERIFY(execute(builder, r).success); const auto original = hash(outer);
        MainWindow window(executable); window.show(); auto tree = panelWidget<FileList>(window, "fileList"); auto backend = window.findChild<SevenZipProcessBackend *>(); auto address = panelWidget<QLineEdit>(window, "currentPath");
        QStringList edits{"日本語 new empty", "renamed payload.txt"}; QStringList passwords; if (encrypted) passwords = {"outer secret", "middle secret", "inner secret"}; int updates = 0; QString error;
        QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { if (dialog->textEchoMode() == QLineEdit::Password) { QVERIFY(!passwords.isEmpty()); dialog->setTextValue(passwords.takeFirst()); } else { QVERIFY(!edits.isEmpty()); dialog->setTextValue(edits.takeFirst()); } dialog->accept(); return; }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) {
                    if (box->property("answerScheduled").toBool()) return;
                    box->setProperty("answerScheduled", true); ++updates; QCOMPARE(box->defaultButton(), box->button(QMessageBox::Yes)); QVERIFY(window.operationBusy());
                    // Expire the actual auto-refresh debounce inside exec(). It
                    // must not List the child while an exit decision is pending.
                    auto debounce = window.findChild<QTimer *>("archiveRefreshDebounce"); QVERIFY(debounce); debounce->start(1);
                    QPointer<QMessageBox> guarded(box); QTimer::singleShot(80, &window, [&, guarded, debounce] { QVERIFY(guarded); QVERIFY(!backend->busy()); QVERIFY(window.operationBusy()); debounce->setInterval(250); guarded->button(QMessageBox::StandardButton(answer))->click(); });
                } else if (box->text().contains("Delete selected items permanently")) box->button(QMessageBox::Yes)->click(); else { error = box->text(); box->accept(); } return; }
        }); dialogs.start();
        window.openPath(outer + "/middle.zip/日本語 inner.7z/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); QVERIFY(passwords.isEmpty()); QCOMPARE(address->text(), outer + "/middle.zip/日本語 inner.7z/"); const auto innerTemporary = window.currentArchivePath();
        const auto rows = tree->findItems("payload.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 1); tree->setCurrentItem(rows[0]); rows[0]->setSelected(true); QVERIFY(window.findChild<QAction *>("renameAction")->isEnabled()); QVERIFY(window.findChild<QAction *>("deleteAction")->isEnabled());
        window.findChild<QAction *>("folderAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); QVERIFY(tree->findItems("日本語 new empty", Qt::MatchExactly).size() == 1); QCOMPARE(hash(outer), original);
        const auto renameRows = tree->findItems("payload.txt", Qt::MatchExactly); QCOMPARE(renameRows.size(), 1); tree->clearSelection(); tree->setCurrentItem(renameRows[0]); renameRows[0]->setSelected(true); window.findChild<QAction *>("renameAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(edits.isEmpty());
        const auto removed = tree->findItems("remove.txt", Qt::MatchExactly); QCOMPARE(removed.size(), 1); tree->clearSelection(); tree->setCurrentItem(removed[0]); removed[0]->setSelected(true); window.findChild<QAction *>("deleteAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY2(tree->findItems("remove.txt", Qt::MatchExactly).isEmpty(), qPrintable(error));
        QSignalSpy completed(backend, &ArchiveBackend::finished);
        const auto replacementCount = [&] { int count = 0; for (const auto &record : completed) count += qvariant_cast<ArchiveResult>(record[0]).operation == ArchiveOperation::ReplaceFile; return count; };
        if (navigation == "up") {
            window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); QCOMPARE(updates, 1); QCOMPARE(hash(outer), original); QVERIFY(window.currentArchivePath() != innerTemporary);
            window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); QCOMPARE(window.currentArchivePath(), outer); QVERIFY(window.findChild<QAction *>("folderAction")->isEnabled());
        } else if (navigation == "ancestor") {
            window.openPath(outer + "/middle.zip/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); QCOMPARE(updates, 1); QCOMPARE(replacementCount(), 1); QCOMPARE(address->text(), outer + "/middle.zip/"); QCOMPARE(hash(outer), original); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        } else if (navigation == "address") { window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); }
        else window.close();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); if (navigation == "close") QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 5000); dialogs.stop(); QVERIFY2(error.isEmpty(), qPrintable(error)); QVERIFY(!QFileInfo::exists(innerTemporary));
        for (const auto &record : completed) { const auto result = qvariant_cast<ArchiveResult>(record[0]); QVERIFY(result.operation == ArchiveOperation::ReplaceFile || result.operation == ArchiveOperation::List); QVERIFY(result.success); }
        if (answer == QMessageBox::Yes) { QCOMPARE(updates, 2); QCOMPARE(replacementCount(), 2); QVERIFY(hash(outer) != original); }
        else { QCOMPARE(updates, 1); QCOMPARE(replacementCount(), 0); QCOMPARE(hash(outer), original); }
        r = {}; r.operation = ArchiveOperation::Extract; r.archive = outer; r.outputDirectory = root + "/verify outer"; r.overwriteMode = "overwrite"; if (encrypted) r.password = "outer secret"; QVERIFY(execute(builder, r).success);
        r.archive = r.outputDirectory + "/middle.zip"; r.outputDirectory = root + "/verify middle"; if (encrypted) r.password = "middle secret"; QVERIFY(execute(builder, r).success);
        r.archive = r.outputDirectory + "/日本語 inner.7z"; r.outputDirectory = root + "/verify inner"; if (encrypted) r.password = "inner secret"; QVERIFY(execute(builder, r).success);
        QCOMPARE(hash(r.outputDirectory + (answer == QMessageBox::Yes ? "/renamed payload.txt" : "/payload.txt")), hash(root + "/payload.txt")); QCOMPARE(QFileInfo(r.outputDirectory + "/日本語 new empty").isDir(), answer == QMessageBox::Yes); QCOMPARE(QFileInfo::exists(r.outputDirectory + "/remove.txt"), answer != QMessageBox::Yes);
    }
    void nestedUnchangedCloseAndPanelRemoval() {
        const auto two = QSettings().value("View/TwoPanels", false); QSettings().setValue("View/TwoPanels", false); const auto restore = qScopeGuard([&] { QSettings().setValue("View/TwoPanels", two); });
        const auto root = temp.filePath("nested unchanged close"), outer = root + "/outer.7z", inner = root + "/inner.zip"; writeFile(root + "/payload.txt", "unmodified content");
        SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = inner; r.workingDirectory = root; r.files = {"payload.txt"}; r.format = "zip"; r.method = "Deflate"; QVERIFY(execute(builder, r).success); r.archive = outer; r.files = {"inner.zip"}; r.format = "7z"; r.method = "LZMA2"; QVERIFY(execute(builder, r).success); const auto original = hash(outer);
        MainWindow window(executable); window.show(); window.openPath(outer + "/inner.zip/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); const auto firstTemp = window.currentArchivePath(); window.close(); QTRY_VERIFY_WITH_TIMEOUT(!window.isVisible(), 5000); QVERIFY(!QFileInfo::exists(firstTemp)); QCOMPARE(hash(outer), original);
        window.show(); window.findChild<QAction *>("twoPanelsAction")->trigger(); QPointer<MainWindow> panel(window.findChild<MainWindow *>("secondPanel")); QVERIFY(panel); panel->openPath(outer + "/inner.zip/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); const auto secondTemp = panel->currentArchivePath(); window.findChild<QAction *>("twoPanelsAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(panel.isNull(), 5000); QVERIFY(!QFileInfo::exists(secondTemp)); QCOMPARE(hash(outer), original); window.close();
    }
    void archiveFolderCreation_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<int>("encryption");
        for (const auto &format : {"7z", "zip", "tar", "wim"}) QTest::newRow(format) << QString(format) << 0;
        QTest::newRow("7z-data") << QString("7z") << 1; QTest::newRow("7z-headers") << QString("7z") << 2;
        QTest::newRow("zip-AES") << QString("zip") << 1; QTest::newRow("zip-ZipCrypto") << QString("zip") << 2;
    }
    void archiveFolderCreation() {
        QFETCH(QString, format); QFETCH(int, encryption);
        const auto root = temp.filePath("folder creation " + format + QString::number(encryption)); const auto archive = root + "/data." + format;
        writeFile(root + "/prefix/日本語 existing.txt", "preserved folder update payload\n"); QDir().mkpath(root + "/prefix/existing empty");
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive; r.workingDirectory = root; r.files = {"prefix"}; r.format = format; r.method = format == "zip" ? "Deflate" : format == "tar" ? "posix" : "LZMA2"; r.level = 1;
        if (encryption) r.password = "folder secret"; r.encryptNames = format == "7z" && encryption == 2; if (format == "zip" && encryption == 2) r.encryptionMethod = "ZipCrypto";
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); const auto permissions = QFile::permissions(archive);
        const QByteArray metadata("folder update metadata"); const auto archiveBytes = QFile::encodeName(archive); QVERIFY(::setxattr(archiveBytes.constData(), "org.7zip-mac-port.folder-test", metadata.constData(), metadata.size(), 0, 0) == 0);
        r.operation = ArchiveOperation::CreateFolder; r.files = {"prefix/日本語 empty [*]"}; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(result.operation, ArchiveOperation::CreateFolder); QCOMPARE(result.target, archive); QCOMPARE(QFile::permissions(archive), permissions);
        bool found = false; for (const auto &entry : result.entries) if (entry.path == r.files[0]) { found = entry.directory && entry.size == 0; } QVERIFY(found);
        r.files = {"root empty"}; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QByteArray restoredMetadata(metadata.size(), '\0'); QCOMPARE(::getxattr(archiveBytes.constData(), "org.7zip-mac-port.folder-test", restoredMetadata.data(), restoredMetadata.size(), 0, 0), ssize_t(metadata.size())); QCOMPARE(restoredMetadata, metadata);
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/restored"; r.overwriteMode = "overwrite"; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(hash(root + "/restored/prefix/日本語 existing.txt"), hash(root + "/prefix/日本語 existing.txt")); QVERIFY(QFileInfo(root + "/restored/prefix/日本語 empty [*]").isDir()); QVERIFY(QFileInfo(root + "/restored/prefix/existing empty").isDir()); QVERIFY(QFileInfo(root + "/restored/root empty").isDir());
        const auto unchanged = hash(archive); r.operation = ArchiveOperation::CreateFolder; r.files = {"PREFIX/日本語 empty [*]"}; result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(archive), unchanged);
        r.files = {"prefix/日本語 existing.txt/child"}; result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(archive), unchanged);
        if (encryption) {
            r.files = {"wrong password folder"}; r.password = "wrong password"; result = execute(backend, r);
            if (format == "7z" && encryption == 2) { QVERIFY(!result.success); QVERIFY(result.exitCode != 0); QCOMPARE(hash(archive), unchanged); }
            else { QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(hash(archive) != unchanged); }
            QVERIFY(!result.details.contains("wrong password"));
            r.operation = ArchiveOperation::List; r.password = "folder secret"; r.files.clear(); result = execute(backend, r); QVERIFY(result.success); for (const auto &entry : result.entries) if (!entry.directory) QVERIFY(entry.encrypted);
            if (format == "7z" && encryption == 2) { r.password.clear(); result = execute(backend, r); QVERIFY(!result.success); }
            else { r.operation = ArchiveOperation::Test; QVERIFY(execute(backend, r).success); }
        }
        QVERIFY(QDir(root).entryList({".7zip-folder-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }
    void archiveFolderCancelAndProtection() {
        const auto root = temp.filePath("folder update protection"); QDir().mkpath(root); SevenZipProcessBackend backend(executable);
        auto r = addRequest(root + "/large.7z"); r.level = 0; r.files = {"input/large.bin"}; QVERIFY(execute(backend, r).success); const auto original = hash(r.archive);
        r.operation = ArchiveOperation::CreateFolder; r.files = {"cancelled empty"}; QSignalSpy done(&backend, &ArchiveBackend::finished); int pauses = 0;
        const auto pauseCopy = connect(&backend, &ArchiveBackend::progress, &backend, [&](int, quint64 bytes, quint64, QString) { if (bytes == (1 << 20)) QMetaObject::invokeMethod(&backend, [&] { QVERIFY(backend.setPaused(true)); ++pauses; }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
        backend.start(r); QTRY_COMPARE_WITH_TIMEOUT(pauses, 1, 5000); QTest::qWait(150); QVERIFY(done.isEmpty()); QVERIFY(backend.busy()); backend.cancel(); QVERIFY(done.wait(10000)); QVERIFY(qvariant_cast<ArchiveResult>(done.last()[0]).cancelled); QCOMPARE(hash(r.archive), original); disconnect(pauseCopy);
        // Modify the destination after native CreateFolder has completed and
        // the resulting folder is being listed, just before installation.
        r.archive = root + "/late-change.7z"; auto small = addRequest(r.archive); small.files = {"input/ASCII.txt"}; QVERIFY(execute(backend, small).success);
        QByteArray lateChangeHash; const auto lateChange = connect(&backend, &ArchiveBackend::output, &backend, [&](QString text) { if (lateChangeHash.isEmpty() && text.contains("Path = cancelled empty")) { QFile external(r.archive); QVERIFY(external.open(QIODevice::Append)); QCOMPARE(external.write("late external edit"), qint64(18)); external.close(); lateChangeHash = hash(r.archive); } });
        auto lateResult = execute(backend, r); disconnect(lateChange); QVERIFY(!lateChangeHash.isEmpty()); QVERIFY(!lateResult.success); QVERIFY(lateResult.message.contains("changed")); QCOMPARE(hash(r.archive), lateChangeHash);
        r.archive = root + "/large.7z";
        done.clear(); int edits = 0; const auto changeOriginal = connect(&backend, &ArchiveBackend::progress, &backend, [&](int, quint64 bytes, quint64, QString) { if (bytes == (1 << 20)) QMetaObject::invokeMethod(&backend, [&] { QFile external(r.archive); QVERIFY(external.open(QIODevice::Append)); QCOMPARE(external.write("concurrent modification"), qint64(23)); ++edits; }, Qt::BlockingQueuedConnection); }, Qt::DirectConnection);
        backend.start(r); QVERIFY(done.wait(10000)); QCOMPARE(edits, 1); auto result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY(!result.success); QVERIFY(result.message.contains("changed")); disconnect(changeOriginal); QVERIFY(hash(r.archive) != original); const auto externallyModified = hash(r.archive);
        for (const auto &invalid : {"../outside", "/absolute", "bad//folder", "./dot", "C:/drive"}) { r.files = {invalid}; result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(r.archive), externallyModified); }
        r.files = {"empty"}; r.format = "gzip"; result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(r.archive), externallyModified);
        r.format = "7z"; QVERIFY(QFile::setPermissions(r.archive, QFileDevice::ReadOwner)); result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(r.archive), externallyModified); QVERIFY(QFile::setPermissions(r.archive, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        const auto link = root + "/linked.7z"; QVERIFY(::symlink(QFile::encodeName(r.archive).constData(), QFile::encodeName(link).constData()) == 0); r.archive = link; result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(link), externallyModified);
        writeFile(root + "/broken.7z", "broken archive"); r.archive = root + "/broken.7z"; result = execute(backend, r); QVERIFY(!result.success); QCOMPARE(hash(r.archive), QCryptographicHash::hash("broken archive", QCryptographicHash::Sha256));
        QVERIFY(QDir(root).entryList({".7zip-folder-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
    }
    void archiveFolderNativeWimImagesAndTimes() {
        const auto root = temp.filePath("native multiple WIM images");
        writeFile(root + "/1/dest/first.txt", "image one payload"); writeFile(root + "/2/dest/日本語 second.txt", "image two payload");
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = root + "/images.wim"; r.format = "wim"; r.parameters = "is=on"; r.workingDirectory = root; r.files = {"1", "2"};
        auto added = execute(backend, r); QVERIFY2(added.success, qPrintable(added.message + added.details));
        r.operation = ArchiveOperation::List; r.files.clear(); const auto original = execute(backend, r); QVERIFY(original.success); QCOMPARE(original.properties.value("Images"), QString("2"));
        r.operation = ArchiveOperation::CreateFolder; r.files = {"2/dest/日本語 empty"}; auto updated = execute(backend, r); QVERIFY2(updated.success, qPrintable(updated.message + updated.details)); QCOMPARE(updated.properties.value("Images"), QString("2"));
        bool found = false, generated = false;
        for (const auto &entry : updated.entries) {
            if (entry.generated) { generated = true; QCOMPARE(entry.parentIndex, qint64(-1)); QVERIFY(!entry.directory); }
            if (entry.path != r.files.first()) continue;
            found = true; QVERIFY(entry.directory); QCOMPARE(entry.size, quint64(0));
            // Native Agent supplies all three equal FILETIMEs. The official
            // WIM handler's default writes MTime; CTime/ATime are optional.
            QVERIFY(!entry.modified.isEmpty());
            for (const auto &name : {"Created", "Accessed"}) if (!entry.properties.value(name).isEmpty()) QCOMPARE(entry.properties.value(name), entry.modified);
            for (const auto &property : entry.orderedProperties) if ((property.name == "Created" || property.name == "Accessed" || property.name == "Modified") && !property.value.isEmpty()) { QVERIFY(property.native); QCOMPARE(property.type, quint16(64)); QVERIFY(property.timePrecision.size() == 3); QCOMPARE(property.timePrecision[0], quint16(16 + 7)); } // C/7zTypes.h: Base + 7 = 100 ns
        }
        QVERIFY(found); QVERIFY(generated);
        r.operation = ArchiveOperation::Test; r.files.clear(); QVERIFY(execute(backend, r).success);
        r.operation = ArchiveOperation::Extract; r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; const auto extracted = execute(backend, r); QVERIFY2(extracted.success, qPrintable(extracted.message + extracted.details));
        QCOMPARE(hash(root + "/out/1/dest/first.txt"), hash(root + "/1/dest/first.txt")); QCOMPARE(hash(root + "/out/2/dest/日本語 second.txt"), hash(root + "/2/dest/日本語 second.txt")); QVERIFY(QFileInfo(root + "/out/2/dest/日本語 empty").isDir());
        writeFile(root + "/sources/copied.txt", "copied WIM image payload"); r.operation = ArchiveOperation::Add; r.useArchiveDefaults = true; r.archivePrefix = "2/dest/"; r.files = {root + "/sources/copied.txt"}; auto copied = execute(backend, r); QVERIFY2(copied.success, qPrintable(copied.message + copied.details));
        writeFile(root + "/replacement.txt", "replacement WIM image payload"); r.operation = ArchiveOperation::ReplaceFile; r.files = {"2/dest/copied.txt"}; r.replacementSource = root + "/replacement.txt"; auto replaced = execute(backend, r); QVERIFY2(replaced.success, qPrintable(replaced.message + replaced.details));
        r.operation = ArchiveOperation::Rename; r.files = {"2/dest/copied.txt", "2/dest/renamed.txt"}; auto renamed = execute(backend, r); QVERIFY2(renamed.success, qPrintable(renamed.message + renamed.details));
        r.operation = ArchiveOperation::Extract; r.files = {"2/dest/renamed.txt"}; r.outputDirectory = root + "/renamed-out"; QVERIFY(execute(backend, r).success); QCOMPARE(hash(root + "/renamed-out/2/dest/renamed.txt"), hash(root + "/replacement.txt"));
        r.operation = ArchiveOperation::Delete; r.files = {"2/dest/renamed.txt"}; QVERIFY(execute(backend, r).success); r.operation = ArchiveOperation::Test; r.files.clear(); QVERIFY(execute(backend, r).success);
        const auto panels = QSettings().value("View/TwoPanels", false); QSettings().setValue("View/TwoPanels", false); const auto restore = qScopeGuard([&] { QSettings().setValue("View/TwoPanels", panels); });
        MainWindow window(executable); window.openPath(r.archive + "/2/dest"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto folderAction = window.findChild<QAction *>("folderAction"); QVERIFY(folderAction); QVERIFY(folderAction->isEnabled());
        QTimer answer; answer.setInterval(5); connect(&answer, &QTimer::timeout, &window, [&] { for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue("from GUI"); input->accept(); } }); answer.start(); folderAction->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); answer.stop(); QCOMPARE(panelWidget<FileList>(window, "fileList")->findItems("from GUI", Qt::MatchExactly).size(), 1); window.close();
        const auto unchanged = hash(r.archive); r.operation = ArchiveOperation::CreateFolder; r.files = {"invalid image folder"}; const auto invalid = execute(backend, r); QVERIFY(!invalid.success); QCOMPARE(hash(r.archive), unchanged);
    }
    void archiveFolderMixedDataPasswords_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void archiveFolderMixedDataPasswords() {
        QFETCH(QString, format); const auto root = temp.filePath("mixed folder passwords " + format); writeFile(root + "/first.txt", "first independently encrypted payload"); writeFile(root + "/second.txt", "second independently encrypted payload");
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = root + "/mixed." + format; r.workingDirectory = root; r.format = format; r.method = format == "zip" ? "Deflate" : "LZMA2"; r.solid = "off"; r.password = "first secret"; r.files = {"first.txt"}; QVERIFY(execute(backend, r).success);
        r.password = "second secret"; r.files = {"second.txt"}; auto added = execute(backend, r); QVERIFY2(added.success, qPrintable(added.message + added.details));
        r.operation = ArchiveOperation::CreateFolder; r.password.clear(); r.files = {"empty without data passwords"}; const auto updated = execute(backend, r); QVERIFY2(updated.success, qPrintable(updated.message + updated.details));
        for (const auto &name : {"first", "second"}) {
            r.operation = ArchiveOperation::Test; r.files = {QString(name) + ".txt"}; r.password = QString(name) + " secret"; const auto tested = execute(backend, r); QVERIFY2(tested.success, qPrintable(tested.message + tested.details));
            r.operation = ArchiveOperation::Extract; r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; const auto extracted = execute(backend, r); QVERIFY2(extracted.success, qPrintable(extracted.message + extracted.details)); QCOMPARE(hash(root + "/out/" + name + ".txt"), hash(root + '/' + name + ".txt"));
        }
    }
    void archiveFolderWorkingDirectory() {
        const auto workBase = qEnvironmentVariable("PORT_ARCHIVE_WORK_ROOT", temp.path()); QTemporaryDir work(workBase + "/.7zip-work-test-XXXXXX"); QVERIFY(work.isValid());
        const auto root = temp.filePath("working folder preference"); writeFile(root + "/data.txt", QByteArray(2 * 1024 * 1024, 'w'));
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = root + "/data.7z"; r.format = "7z"; r.level = 0; r.workingDirectory = root; r.files = {"data.txt"}; QVERIFY(execute(backend, r).success);
        if (qEnvironmentVariableIsSet("PORT_ARCHIVE_WORK_ROOT")) { struct stat source{}, other{}; QCOMPARE(::stat(QFile::encodeName(r.archive).constData(), &source), 0); QCOMPARE(::stat(QFile::encodeName(work.path()).constData(), &other), 0); QVERIFY(source.st_dev != other.st_dev); }
        bool stagedInPreference = false; const auto watch = connect(&backend, &ArchiveBackend::progress, &backend, [&](int, quint64, quint64, QString) { if (!QDir(work.path()).entryList({".7zip-folder-*"}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) stagedInPreference = true; }, Qt::DirectConnection);
        r.operation = ArchiveOperation::CreateFolder; r.temporaryDirectory = work.path(); r.files = {"empty in working folder"}; const auto updated = execute(backend, r); disconnect(watch); QVERIFY2(updated.success, qPrintable(updated.message + updated.details)); QVERIFY(stagedInPreference);
        QVERIFY(QDir(work.path()).entryList({".7zip-folder-*"}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()); QVERIFY(QDir(root).entryList({".7zip-install-*"}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(backend, r).success); QCOMPARE(hash(root + "/out/data.txt"), hash(root + "/data.txt")); QVERIFY(QFileInfo(root + "/out/empty in working folder").isDir());
        writeFile(root + "/replacement.txt", "replacement in working folder"); r.operation = ArchiveOperation::ReplaceFile; r.files = {"data.txt"}; r.replacementSource = root + "/replacement.txt"; const auto replaced = execute(backend, r); QVERIFY2(replaced.success, qPrintable(replaced.message + replaced.details));
        r.operation = ArchiveOperation::Extract; r.files.clear(); QVERIFY(execute(backend, r).success); QCOMPARE(hash(root + "/out/data.txt"), hash(root + "/replacement.txt"));
        const auto archive7z = r.archive; r.operation = ArchiveOperation::Add; r.format = "zip"; r.method = "Deflate"; r.archive = root + "/comment.zip"; r.temporaryDirectory.clear(); r.files = {"data.txt"}; QVERIFY(execute(backend, r).success);
        r.operation = ArchiveOperation::Comment; r.temporaryDirectory = work.path(); r.files = {"data.txt"}; r.comment = "working-folder 日本語 comment"; const auto commented = execute(backend, r); QVERIFY2(commented.success, qPrintable(commented.message + commented.details)); bool foundComment = false; for (const auto &entry : commented.entries) if (entry.path == "data.txt") foundComment = entry.properties.value("Comment") == r.comment; QVERIFY(foundComment);
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/zip-out"; QVERIFY(execute(backend, r).success); QCOMPARE(hash(root + "/zip-out/data.txt"), hash(root + "/data.txt"));
        const auto previousSettings = FileManagerSettings::load(); auto preferences = previousSettings; preferences.workMode = 2; preferences.workPath = work.path(); preferences.removableOnly = false; QVERIFY(preferences.save()); const auto restorePreferences = qScopeGuard([&] { previousSettings.save(); });
        const auto panels = QSettings().value("View/TwoPanels", false); QSettings().setValue("View/TwoPanels", false); const auto restorePanels = qScopeGuard([&] { QSettings().setValue("View/TwoPanels", panels); });
        MainWindow window(executable); window.openPath(archive7z); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto guiBackend = window.findChild<SevenZipProcessBackend *>(); QVERIFY(guiBackend); bool guiUsedPreference = false;
        connect(guiBackend, &ArchiveBackend::progress, guiBackend, [&](int, quint64, quint64, QString) { if (!QDir(work.path()).entryList({".7zip-folder-*"}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) guiUsedPreference = true; }, Qt::DirectConnection);
        QTimer answer; answer.setInterval(5); connect(&answer, &QTimer::timeout, &window, [&] { for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue("GUI working folder"); input->accept(); } }); answer.start(); window.findChild<QAction *>("folderAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); answer.stop(); QVERIFY(guiUsedPreference); QCOMPARE(panelWidget<FileList>(window, "fileList")->findItems("GUI working folder", Qt::MatchExactly).size(), 1); window.close();
        QVERIFY(QDir(work.path()).entryList({".7zip-folder-*", ".7zip-replace-*"}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()); QVERIFY(QDir(root).entryList({".7zip-install-*"}, QDir::Hidden | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
        const auto unchanged = hash(r.archive); r.operation = ArchiveOperation::CreateFolder; r.files = {"failed folder"}; r.temporaryDirectory = work.path() + "/missing"; const auto failed = execute(backend, r); QVERIFY(!failed.success); QCOMPARE(hash(r.archive), unchanged);
    }
    void archiveFolderLargeListing() {
        const auto archive = temp.filePath("large listing.tar"); QVERIFY(QFile::copy(qEnvironmentVariable("PORT_FIXTURES") + "/many-entry.tar", archive));
        QProcess listing; listing.start(executable, {"l", "-slt", "-sccUTF-8", "--", archive}); QVERIFY(listing.waitForFinished(10000)); QCOMPARE(listing.exitCode(), 0); QVERIFY(listing.readAllStandardOutput().size() > 2 * 1024 * 1024);
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::CreateFolder; r.archive = archive; r.format = "tar"; r.files = {"日本語 new empty"}; const auto result = execute(backend, r);
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(result.entries.size(), 12001); int files = 0, folders = 0;
        for (const auto &entry : result.entries) if (entry.directory) { ++folders; QCOMPARE(entry.path, r.files[0]); } else { ++files; QCOMPARE(entry.size, quint64(0)); }
        QCOMPARE(files, 12000); QCOMPARE(folders, 1); r.operation = ArchiveOperation::Test; r.files.clear(); QVERIFY(execute(backend, r).success);
    }
    void archiveFolderDiagnosticsAndLimits() {
        const auto root = temp.filePath("folder diagnostics limits"), archive = root + "/data.7z"; writeFile(root + "/passwords.txt", "filename is not a password error");
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive; r.workingDirectory = root; r.files = {"passwords.txt"}; QVERIFY(execute(backend, r).success); const auto original = hash(archive);
        r.operation = ArchiveOperation::CreateFolder; r.files = {"passwords.txt"}; auto result = execute(backend, r); QVERIFY(!result.success); QVERIFY(result.details.contains("passwords.txt")); QVERIFY(!result.passwordRequired); QCOMPARE(hash(archive), original);
        auto quoted = [](QString value) { value.replace('\'', "'\\''"); return '\'' + value + '\''; }; const auto argumentLog = root + "/test-arguments", forwardingEngine = root + "/forward-engine";
        writeFile(forwardingEngine, ("#!/bin/sh\nif [ \"$1\" = t ]; then printf '%s\\n' \"$@\" > " + quoted(argumentLog) + "; fi\nexec " + quoted(executable) + " \"$@\"\n").toUtf8()); QVERIFY(QFile::setPermissions(forwardingEngine, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        SevenZipProcessBackend forwarding(forwardingEngine); r.files = {"empty"}; r.memoryLimitGB = 1; result = execute(forwarding, r); QVERIFY(!result.success); QVERIFY(result.message.contains("adapter is missing")); QCOMPARE(hash(archive), original);
        r.operation = ArchiveOperation::Test; r.files.clear(); result = execute(forwarding, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QFile arguments(argumentLog); QVERIFY(arguments.open(QIODevice::ReadOnly)); QVERIFY(arguments.readAll().split('\n').contains("-smemx1g"));
        const auto links = SevenZipProcessBackend::parseListing("Path = junction\nFolder = +\nAttributes = DL\nLink = C:\\target\n\nPath = raw-link\nFolder = +\nAttributes = DL\nLink = \n\nPath = directory\nFolder = +\nAttributes = D\nLink = \n\n"); QCOMPARE(links.size(), 3); QVERIFY(links[0].directory && links[0].link); QVERIFY(links[1].directory && links[1].link); QVERIFY(links[2].directory && !links[2].link);
        // Replay official WIM technical fields to exercise the rejection path;
        // this is a parser/control regression, not a genuine WIM format fixture.
        const auto replay = root + "/replay-wim-list", fakeArchive = root + "/replay.wim";
        writeFile(replay, "#!/bin/sh\n[ \"$1\" = l ] || exit 99\nprintf 'Type = wim\\nImages = 1\\n\\n----------\\nPath = junction\\nFolder = +\\nAttributes = DL\\nLink = C:/target\\n\\n'\n"); QVERIFY(QFile::setPermissions(replay, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)); writeFile(fakeArchive, "owned technical-list replay fixture"); const auto replayHash = hash(fakeArchive);
        SevenZipProcessBackend replayBackend(replay); r.operation = ArchiveOperation::CreateFolder; r.archive = fakeArchive; r.format = "wim"; r.files = {"junction/child"}; result = execute(replayBackend, r); QVERIFY(!result.success); QVERIFY(result.message.contains("not a directory")); QVERIFY(!result.passwordRequired); QCOMPARE(hash(fakeArchive), replayHash);
    }
    void guiArchiveFolderCreation() {
        const auto panels = QSettings().value("View/TwoPanels", false), flat = QSettings().value("View/Flat", false); QSettings().setValue("View/TwoPanels", false); QSettings().setValue("View/Flat", false);
        const auto restoreView = qScopeGuard([&] { QSettings().setValue("View/TwoPanels", panels); QSettings().setValue("View/Flat", flat); });
        const auto root = temp.filePath("GUI archive folder"), archive = root + "/secret.7z"; writeFile(root + "/prefix/file.txt", "GUI folder payload"); writeFile(root + "/prefix/passwords.txt", "not a password error");
        SevenZipProcessBackend builder(executable); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = archive; r.workingDirectory = root; r.files = {"prefix"}; r.password = "GUI folder secret"; QVERIFY(execute(builder, r).success);
        MainWindow window(executable); auto backend = window.findChild<SevenZipProcessBackend *>(); auto tree = panelWidget<FileList>(window, "fileList"); auto folder = window.findChild<QAction *>("folderAction");
        window.openPath(archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto rows = tree->findItems("prefix", Qt::MatchExactly); QCOMPARE(rows.size(), 1); tree->setCurrentItem(rows[0]); rows[0]->setSelected(true); window.findChild<QAction *>("openAction")->trigger(); QVERIFY(folder->isEnabled()); QVERIFY(!window.findChild<QAction *>("newfileAction")->isEnabled());
        QStringList answers{"日本語 new empty"}; QTimer answer; answer.setInterval(10); connect(&answer, &QTimer::timeout, &window, [&] { for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { QVERIFY(!answers.isEmpty()); dialog->setTextValue(answers.takeFirst()); dialog->accept(); break; } }); answer.start();
        folder->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); answer.stop(); QVERIFY(answers.isEmpty()); rows = tree->findItems("日本語 new empty", Qt::MatchExactly); QCOMPARE(rows.size(), 1); QCOMPARE(tree->currentItem(), rows[0]); QVERIFY(rows[0]->isSelected()); QVERIFY(folder->isEnabled());
        for (auto progress : window.findChildren<ProgressDialog *>()) QVERIFY(!progress->isVisible());
        const auto unchanged = hash(archive); bool unexpectedPassword = false; int folderPrompts = 0; QTimer duplicate; duplicate.setInterval(10); connect(&duplicate, &QTimer::timeout, &window, [&] { for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { if (dialog->textEchoMode() == QLineEdit::Password) { unexpectedPassword = true; dialog->reject(); } else { ++folderPrompts; dialog->setTextValue("passwords.txt"); dialog->accept(); } break; } }); duplicate.start(); folder->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); duplicate.stop(); QCOMPARE(folderPrompts, 1); QVERIFY(!unexpectedPassword); QCOMPARE(hash(archive), unchanged);
        auto permissions = QFile::permissions(archive); QVERIFY(QFile::setPermissions(archive, QFileDevice::ReadOwner)); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!folder->isEnabled()); QVERIFY(QFile::setPermissions(archive, permissions));
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); r.archive = root + "/stream.gz"; r.format = "gzip"; r.method = "Deflate"; r.password.clear(); r.files = {"prefix/file.txt"}; QVERIFY(execute(builder, r).success); window.openPath(r.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!folder->isEnabled()); QVERIFY(!backend->busy());
    }
    void fileCopyMoveRenameFolder() {
        FileOperations jobs; QDir().mkpath(temp.filePath("copy-target")); QDir().mkpath(temp.filePath("move-target"));
        QSignalSpy spy(&jobs, &FileOperations::finished); jobs.transfer({input}, temp.filePath("copy-target"), false); QVERIFY(spy.wait(20000)); QVERIFY2(spy[0][0].toString().isEmpty(), qPrintable(spy[0][0].toString()));
        QCOMPARE(hash(input + "/large.bin"), hash(temp.filePath("copy-target/input/large.bin"))); QVERIFY(QFileInfo::exists(temp.filePath("copy-target/input/empty directory")));
        spy.clear(); jobs.transfer({temp.filePath("copy-target/input")}, temp.filePath("move-target"), true); QVERIFY(spy.wait(20000)); QVERIFY(spy[0][0].toString().isEmpty()); QVERIFY(!QFileInfo::exists(temp.filePath("copy-target/input"))); QVERIFY(QFileInfo::exists(temp.filePath("move-target/input")));
        spy.clear(); jobs.transfer({input}, input + "/hierarchy", false); QVERIFY(spy.wait(10000)); QVERIFY(!spy[0][0].toString().isEmpty());
    }
    void splitCombineAndFailures() {
        QString error; const auto sizes = FileOperations::parseVolumeSizes("1 M 3M - test", &error); QVERIFY(error.isEmpty()); QCOMPARE(sizes, QList<quint64>({1 << 20, 3 << 20}));
        QCOMPARE(FileOperations::parseVolumeSizes("1KB 2MB"), QList<quint64>({1024, 2 << 20}));
        QCOMPARE(FileOperations::volumeCount(32 << 20, sizes), quint64(12));
        for (auto invalid : {"0", "- CD", "1P", "18446744073709551616", "16777216T"}) { QVERIFY(FileOperations::parseVolumeSizes(invalid, &error).isEmpty()); QVERIFY(!error.isEmpty()); }
        const auto source = temp.filePath("分割 スペース.bin"); QVERIFY(QFile::copy(input + "/large.bin", source));
        FileOperations jobs; QSignalSpy done(&jobs, &FileOperations::finished), progress(&jobs, &FileOperations::byteProgress);
        const auto parts = temp.filePath("split-parts"), out = temp.filePath("combined-out");
        jobs.split(source, parts, sizes); QVERIFY(done.wait(15000)); QVERIFY2(done[0][0].toString().isEmpty(), qPrintable(done[0][0].toString())); QVERIFY(!progress.isEmpty()); QCOMPARE(progress.last()[0].toULongLong(), quint64(32 << 20));
        const auto first = parts + "/分割 スペース.bin.001"; auto plan = FileOperations::inspectVolumes(first); QVERIFY(plan.error.isEmpty()); QCOMPARE(plan.volumes.size(), 12); QCOMPARE(plan.totalSize, quint64(32 << 20)); QCOMPARE(QFileInfo(first).size(), qint64(1 << 20)); QCOMPARE(QFileInfo(plan.volumes[1]).size(), qint64(3 << 20));
        done.clear(); jobs.combine(first, out); QVERIFY(done.wait(15000)); QVERIFY2(done[0][0].toString().isEmpty(), qPrintable(done[0][0].toString())); QCOMPARE(hash(source), hash(out + "/分割 スペース.bin")); QCOMPARE(hash(source), hash(input + "/large.bin"));
        done.clear(); jobs.split(source, parts, sizes); QVERIFY(done.wait(10000)); QVERIFY(done[0][0].toString().contains("exists")); QCOMPARE(hash(source), hash(out + "/分割 スペース.bin"));
        QVERIFY(QFile::rename(plan.volumes[2], plan.volumes[2] + ".saved")); done.clear(); jobs.combine(first, temp.filePath("gap-output")); QVERIFY(done.wait(10000)); QVERIFY(done[0][0].toString().contains("Missing volume")); QVERIFY(!QFileInfo::exists(temp.filePath("gap-output/分割 スペース.bin"))); QVERIFY(QFile::rename(plan.volumes[2] + ".saved", plan.volumes[2]));
        done.clear(); jobs.combine(first, out); QVERIFY(done.wait(10000)); QVERIFY(done[0][0].toString().contains("exists")); QCOMPARE(hash(source), hash(out + "/分割 スペース.bin"));
        const auto cancelPath = temp.filePath("split-cancel");
        auto cancellation = connect(&jobs, &FileOperations::byteProgress, &jobs, [&jobs](quint64 bytes, quint64, QString) { if (bytes >= (1 << 20)) jobs.cancel(); }, Qt::DirectConnection);
        done.clear(); jobs.split(source, cancelPath, {1 << 20}); QVERIFY(done.wait(10000)); QVERIFY(done[0][0].toString().contains("cancelled")); disconnect(cancellation); QVERIFY(QDir(cancelPath).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden).isEmpty());
        done.clear(); jobs.split(source, temp.filePath("split-recovery"), {8 << 20}); QVERIFY(done.wait(10000)); QVERIFY(done[0][0].toString().isEmpty());
        const auto collision = temp.filePath("split-symlink"); QDir().mkpath(collision); const auto link = collision + "/分割 スペース.bin.002"; QVERIFY(::symlink("missing", QFile::encodeName(link).constData()) == 0);
        done.clear(); jobs.split(source, collision, {8 << 20}); QVERIFY(done.wait(10000)); QVERIFY(!done[0][0].toString().isEmpty()); QVERIFY(QFileInfo(link).isSymLink()); QVERIFY(!QFileInfo::exists(collision + "/分割 スペース.bin.001"));
    }
    void volumeNumberingAndLinks() {
        FileOperations jobs; QSignalSpy done(&jobs, &FileOperations::finished); const auto small = temp.filePath("numbered.bin"); writeFile(small, QByteArray(10001, 'v'));
        const auto parts = temp.filePath("1001-volumes"); jobs.split(small, parts, {10}); QVERIFY(done.wait(20000)); QVERIFY2(done[0][0].toString().isEmpty(), qPrintable(done[0][0].toString())); QVERIFY(QFileInfo::exists(parts + "/numbered.bin.0001")); QVERIFY(QFileInfo::exists(parts + "/numbered.bin.1001")); QVERIFY(!QFileInfo::exists(parts + "/numbered.bin.001"));
        done.clear(); jobs.combine(parts + "/numbered.bin.0001", temp.filePath("numbered-combined")); QVERIFY(done.wait(20000)); QVERIFY(done[0][0].toString().isEmpty()); QCOMPARE(hash(small), hash(temp.filePath("numbered-combined/numbered.bin")));
        const auto hard = temp.filePath("hard 日本語.bin"); QVERIFY(FileOperations::createLink(hard, small, LinkType::Hard).isEmpty()); struct stat original{}, linked{}; QVERIFY(::stat(QFile::encodeName(small).constData(), &original) == 0); QVERIFY(::stat(QFile::encodeName(hard).constData(), &linked) == 0); QCOMPARE(original.st_ino, linked.st_ino); QCOMPARE(original.st_dev, linked.st_dev);
        const auto sym = temp.filePath("relative symbolic.bin"); QVERIFY(FileOperations::createLink(sym, "numbered.bin", LinkType::SymbolicFile).isEmpty()); QCOMPARE(hash(sym), hash(small)); char target[256]{}; const auto length = ::readlink(QFile::encodeName(sym).constData(), target, sizeof(target)); QCOMPARE(QByteArray(target, length), QByteArray("numbered.bin"));
        const auto dirLink = temp.filePath("directory symbolic"); QVERIFY(FileOperations::createLink(dirLink, "input", LinkType::SymbolicDirectory).isEmpty()); QVERIFY(QFileInfo(dirLink).isSymLink()); QCOMPARE(hash(dirLink + "/ASCII.txt"), hash(input + "/ASCII.txt"));
        QVERIFY(!FileOperations::createLink(small, "input/ASCII.txt", LinkType::SymbolicFile).isEmpty()); QCOMPARE(QFileInfo(small).size(), qint64(10001)); QVERIFY(!FileOperations::createLink(temp.filePath("wrong-type"), "input", LinkType::Hard).isEmpty()); QVERIFY(!QFileInfo::exists(temp.filePath("wrong-type")));
    }
    void guiTypeBookmarksAndFileTools() {
        const auto root = temp.filePath("gui-tools"); writeFile(root + "/a.TXT", "alpha"); writeFile(root + "/b.txt", "beta"); writeFile(root + "/c.bin", QByteArray(1024 * 1024 + 7, 'c')); writeFile(root + "/extensionless", "no extension"); writeFile(root + "/.profile", "dot"); QDir().mkpath(root + "/folder");
        MainWindow window(executable); window.show(); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto files = panelWidget<FileList>(window, "fileList"); QVERIFY(files);
        auto focus = [&](QString name) { files->clearSelection(); auto found = files->findItems(name, Qt::MatchExactly); if (found.isEmpty()) return static_cast<QTreeWidgetItem *>(nullptr); files->setCurrentItem(found.first()); found.first()->setSelected(true); files->setFocus(); return found.first(); };
        QVERIFY(focus("a.TXT")); window.findChild<QAction *>("selecttypeAction")->trigger(); QCOMPARE(files->selectedItems().size(), 2); window.findChild<QAction *>("deselecttypeAction")->trigger(); QCOMPARE(files->selectedItems().size(), 0);
        QVERIFY(focus("extensionless")); window.findChild<QAction *>("selecttypeAction")->trigger(); QCOMPARE(files->selectedItems().size(), 1); QVERIFY(focus("folder")); window.findChild<QAction *>("selecttypeAction")->trigger(); QCOMPARE(files->selectedItems().size(), 1);
        window.findChild<QAction *>("storeBookmark3Action")->trigger(); QCOMPARE(QSettings().value("bookmark3").toString(), root); window.openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.findChild<QAction *>("bookmark3Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(window.currentDirectory(), root);
        // Synthetic native right-Control state, with Qt's unchanged Ctrl key.
        QKeyEvent control(QEvent::KeyPress, Qt::Key_Control, Qt::ControlModifier, 0, 62, 0); QApplication::sendEvent(files, &control); QKeyEvent digit(QEvent::KeyPress, Qt::Key_4, Qt::ControlModifier | Qt::ShiftModifier, 0, 21, 0); QApplication::sendEvent(files, &digit); QCOMPARE(QSettings().value("bookmark4").toString(), root); QKeyEvent release(QEvent::KeyRelease, Qt::Key_Control, Qt::NoModifier, 0, 62, 0); QApplication::sendEvent(files, &release);
        QVERIFY(focus("c.bin")); auto splitAction = window.findChild<QAction *>("splitAction"); QVERIFY(splitAction->isEnabled()); auto jobs = window.findChild<FileOperations *>(); QSignalSpy completed(jobs, &FileOperations::finished);
        QTimer::singleShot(50, &window, [&] { auto dialog = window.findChild<SplitDialog *>("splitDialog"); QVERIFY(dialog); dialog->findChild<QComboBox *>("splitDestination")->setEditText(root + "/parts"); dialog->findChild<QComboBox *>("splitSizes")->setEditText("512K"); dialog->accept(); }); splitAction->trigger(); if (completed.isEmpty()) QVERIFY(completed.wait(10000)); QVERIFY(completed.last()[0].toString().isEmpty()); QVERIFY(QFileInfo::exists(root + "/parts/c.bin.003"));
        window.openPath(root + "/parts"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus("c.bin.001")); completed.clear(); QTimer::singleShot(50, &window, [&] { auto dialog = window.findChild<CombineDialog *>("combineDialog"); QVERIFY(dialog); dialog->findChild<QComboBox *>("combineDestination")->setEditText(root + "/joined"); dialog->accept(); }); window.findChild<QAction *>("combineAction")->trigger(); if (completed.isEmpty()) QVERIFY(completed.wait(10000)); QVERIFY(completed.last()[0].toString().isEmpty()); QCOMPARE(hash(root + "/c.bin"), hash(root + "/joined/c.bin"));
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus("a.TXT")); QTimer::singleShot(50, &window, [&] { auto dialog = window.findChild<LinkDialog *>("linkDialog"); QVERIFY(dialog); dialog->findChild<QLineEdit *>("linkFrom")->setText(root + "/gui link.txt"); dialog->accept(); }); window.findChild<QAction *>("linkAction")->trigger(); QCOMPARE(hash(root + "/a.TXT"), hash(root + "/gui link.txt"));
        QVERIFY(!window.windowIcon().isNull());
        window.openPath(temp.filePath("roundtrip.7z")); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); panelWidget<QLineEdit>(window, "currentPath")->setText(temp.filePath("roundtrip.7z/input/hierarchy")); QTest::keyClick(panelWidget<QLineEdit>(window, "currentPath"), Qt::Key_Return); window.findChild<QAction *>("storeBookmark5Action")->trigger(); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.findChild<QAction *>("bookmark5Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(files->findItems("deeper", Qt::MatchExactly).size() == 1); QVERIFY(!window.findChild<QAction *>("splitAction")->isEnabled());
    }
    void officialBenchmarkAndStop() {
        BenchmarkRunner runner(executable); QSignalSpy done(&runner, &BenchmarkRunner::finished), results(&runner, &BenchmarkRunner::result); int ticks = 0; QTimer heartbeat; heartbeat.setInterval(5); connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; }); heartbeat.start();
        runner.start(18, 1, 1); QVERIFY(runner.busy()); QVERIFY(done.wait(45000)); QVERIFY2(done[0][0].toString().isEmpty(), qPrintable(done[0][0].toString())); QCOMPARE(results.size(), 1); const auto result = qvariant_cast<BenchmarkResult>(results[0][0]); QVERIFY(result.compressSpeed > 0); QVERIFY(result.decompressSpeed > 0); QVERIFY(result.compressRating > 0); QVERIFY(result.decompressRating > 0); QVERIFY(ticks > 10); QVERIFY(!runner.busy());
        done.clear(); runner.start(25, 2, 100); QTimer::singleShot(100, &runner, &BenchmarkRunner::stop); QVERIFY(done.wait(5000)); QVERIFY(!runner.busy()); QVERIFY(runner.stopped());
        BenchmarkRunner missing(temp.filePath("missing-7zz")); QSignalSpy failed(&missing, &BenchmarkRunner::finished); missing.start(18, 1, 1); QVERIFY(failed.wait(5000)); QVERIFY(failed[0][0].toString().contains("Cannot start")); QVERIFY(!missing.busy());
        QSettings().setValue("Benchmark/DictionaryLog", 18); QSettings().setValue("Benchmark/Threads", 1); BenchmarkDialog dialog(executable); dialog.show(); auto backend = dialog.findChild<BenchmarkRunner *>(); QTRY_VERIFY_WITH_TIMEOUT(backend->busy(), 3000); auto restart = dialog.findChild<QPushButton *>("benchmarkRestart"); QTest::mouseClick(restart, Qt::LeftButton); QTRY_VERIFY_WITH_TIMEOUT(backend->busy(), 3000); auto stop = dialog.findChild<QPushButton *>("benchmarkStop"); QTest::mouseClick(stop, Qt::LeftButton); QTRY_VERIFY_WITH_TIMEOUT(!backend->busy(), 5000); dialog.close();
    }
    void guiMenuMouseNavigation() {
        MainWindow window(executable); window.show(); window.raise(); window.activateWindow(); QVERIFY(activateTestWindow(&window)); window.openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto bar = window.menuBar(); QVERIFY(!bar->isNativeMenuBar());
        QVERIFY2(bar->palette().color(QPalette::Disabled, QPalette::Text) != bar->palette().color(QPalette::Active, QPalette::Text), "Disabled menu commands must not look enabled");
        QCOMPARE(qApp->styleHints()->colorScheme(), Qt::ColorScheme::Light);
        const auto menus = bar->actions(); QCOMPARE(menus.size(), 6);
        for (auto top : menus) {
            auto menu = top->menu(); QVERIFY(menu);
            QSignalSpy shown(menu, &QMenu::aboutToShow);
            auto position = bar->mapTo(&window, bar->actionGeometry(top).center());
            QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, position);
            QTRY_VERIFY_WITH_TIMEOUT(menu->isVisible(), 2000);
            QVERIFY(!shown.isEmpty()); QCOMPARE(QApplication::activePopupWidget(), menu);
            QTest::keyClick(menu, Qt::Key_Escape); QTRY_VERIFY(!menu->isVisible());
        }
        // Click an actual command in the dropdown, with no QAction::trigger().
        auto help = menus.last()->menu(); auto about = window.findChild<QAction *>("aboutAction"); QVERIFY(about);
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, bar->mapTo(&window, bar->actionGeometry(menus.last()).center()));
        QTRY_VERIFY(help->isVisible());
        bool sawAbout = false; QTimer dismiss; dismiss.setInterval(20);
        connect(&dismiss, &QTimer::timeout, [&] { if (auto dialog = window.findChild<AboutDialog *>()) { sawAbout = true; dialog->accept(); } }); dismiss.start();
        QTest::mouseClick(help->windowHandle(), Qt::LeftButton, Qt::NoModifier, help->actionGeometry(about).center());
        QTRY_VERIFY_WITH_TIMEOUT(sawAbout, 2000); dismiss.stop();
        auto rename = window.findChild<QAction *>("renameAction"); QVERIFY(rename); QVERIFY(!rename->isEnabled());
        auto tree = panelWidget<FileList>(window, "fileList"); QVERIFY(tree);
        for (int n = 0; n < tree->topLevelItemCount(); ++n) if (tree->topLevelItem(n)->text(0) == "ASCII.txt") tree->setCurrentItem(tree->topLevelItem(n));
        QVERIFY(rename->isEnabled());
        auto file = menus.first()->menu();
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, bar->mapTo(&window, bar->actionGeometry(menus.first()).center())); QTRY_VERIFY(file->isVisible());
        QTimer::singleShot(100, [&] { if (auto d = testInput(&window)) { d->setTextValue("menu-renamed.txt"); d->accept(); } });
        QTest::mouseClick(file->windowHandle(), Qt::LeftButton, Qt::NoModifier, file->actionGeometry(rename).center());
        QTRY_VERIFY(QFileInfo::exists(input + "/menu-renamed.txt")); QVERIFY(!QFileInfo::exists(input + "/ASCII.txt"));
        QVERIFY(QFile::rename(input + "/menu-renamed.txt", input + "/ASCII.txt")); window.close();
    }
    void guiNavigationAndActions() {
        MainWindow window(executable); window.show(); window.raise(); window.activateWindow(); QVERIFY(activateTestWindow(&window)); window.openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto tree = panelWidget<FileList>(window, "fileList"); QVERIFY(tree);
        panelWidget<QLineEdit>(window, "currentPath")->setFocus();
        auto details = window.findChild<QAction *>("mode3Action", Qt::FindDirectChildrenOnly); QVERIFY(details); details->trigger();
        QTRY_VERIFY(tree->isVisible() && tree->isEnabled()); auto find = [tree](QString name) -> QTreeWidgetItem * { for (int n = 0; n < tree->topLevelItemCount(); ++n) if (tree->topLevelItem(n)->text(0) == name) return tree->topLevelItem(n); return nullptr; };
        QVERIFY(find("日本語ファイル.txt")); auto dir = find("hierarchy"); QVERIFY(dir); tree->setCurrentItem(dir); tree->setFocus(); QTest::keyClick(tree, Qt::Key_Return); QTRY_COMPARE(window.currentDirectory(), input + "/hierarchy"); QTRY_VERIFY(!window.operationBusy()); QTest::keyClick(tree, Qt::Key_Backspace); QTRY_COMPARE(window.currentDirectory(), input); QTRY_VERIFY(!window.operationBusy());
        auto select = find("ASCII.txt"); tree->setCurrentItem(select); tree->setItemSelected(select, true);
        auto restoreInput = qScopeGuard([&] { if (QFileInfo::exists(input + "/renamed-ui.txt")) QFile::rename(input + "/renamed-ui.txt", input + "/ASCII.txt"); QDir(input + "/created-ui").removeRecursively(); });
        QTimer::singleShot(100, [&] { auto d = testInput(&window); if (d) { d->setTextValue("renamed-ui.txt"); d->accept(); } }); QTest::keyClick(tree, Qt::Key_F2); QTRY_VERIFY(QFileInfo::exists(input + "/renamed-ui.txt")); QTRY_VERIFY(!window.operationBusy());
        QTimer::singleShot(100, [&] { auto d = testInput(&window); if (d) { d->setTextValue("created-ui"); d->accept(); } }); QTest::keyClick(tree, Qt::Key_F7); QTRY_VERIFY(QFileInfo(input + "/created-ui").isDir()); QTRY_VERIFY(!window.operationBusy());
        for (const QString format : {QString("7z"), QString("zip")}) {
            for (int row = 0; row < tree->topLevelItemCount(); ++row) tree->setItemSelected(tree->topLevelItem(row), false);
            select = find("日本語ファイル.txt"); tree->setCurrentItem(select); tree->setItemSelected(select, true);
            QString archive = temp.filePath("gui-created." + format);
            QTimer::singleShot(100, [&] { auto d = window.findChild<AddDialog *>("addDialog"); if (d) { d->findChild<QComboBox *>("archiveFormat")->setCurrentText(format); d->findChild<QLineEdit *>("archivePath")->setText(archive); d->accept(); } });
            QTest::mouseClick(window.findChild<QToolButton *>("addButton"), Qt::LeftButton); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); QVERIFY(QFileInfo::exists(archive));
            window.openPath(archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(window.currentArchivePath(), archive); QVERIFY(find("日本語ファイル.txt"));
            QTest::mouseClick(window.findChild<QToolButton *>("testButton"), Qt::LeftButton); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
            QString output = temp.filePath("gui-output-" + format);
            QTimer::singleShot(100, [&] { auto d = window.findChild<ExtractDialog *>("extractDialog"); if (d) { d->findChild<QCheckBox *>("extractNameEnabled")->setChecked(false); d->findChild<QLineEdit *>("extractDestination")->setText(output); d->accept(); } });
            QTest::mouseClick(window.findChild<QToolButton *>("extractButton"), Qt::LeftButton); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(hash(input + "/日本語ファイル.txt"), hash(output + "/日本語ファイル.txt"));
            window.openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        }
        window.close();
    }
    void guiEncryptedOpenAndDrop() {
        MainWindow window(executable); window.show(); window.raise(); window.activateWindow(); QVERIFY(activateTestWindow(&window));
        QTimer passwordDialog; passwordDialog.setInterval(20);
        connect(&passwordDialog, &QTimer::timeout, &window, [&] { auto d = testInput(&window); if (d && d->isVisible()) { passwordDialog.stop(); d->setTextValue("テスト用 password 123"); d->accept(); } }); passwordDialog.start();
        window.openPath(temp.filePath("encrypted-7z1.7z")); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy() && !window.currentArchivePath().isEmpty(), 10000); QCOMPARE(window.currentArchivePath(), temp.filePath("encrypted-7z1.7z"));
        auto tree = panelWidget<FileList>(window, "fileList"); QVERIFY(tree);
        panelWidget<QLineEdit>(window, "currentPath")->setFocus();
        auto details = window.findChild<QAction *>("mode3Action", Qt::FindDirectChildrenOnly); QVERIFY(details); details->trigger();
        QTRY_VERIFY(tree->isVisible() && tree->isEnabled());
        QTreeWidgetItem *selectedFolder = nullptr;
        for (int n = 0; n < tree->topLevelItemCount(); ++n)
            if (!tree->topLevelItem(n)->data(0, Qt::UserRole + 2).toBool() && tree->topLevelItem(n)->data(0, Qt::UserRole + 1).toBool()) { selectedFolder = tree->topLevelItem(n); break; }
        QVERIFY(selectedFolder); tree->setCurrentItem(selectedFolder); tree->setItemSelected(selectedFolder, true);
        QString previousArchive = window.currentArchivePath();
        QString corrupt = temp.filePath("gui-corrupt.7z"); writeFile(corrupt, "not an archive");
        bool sawWarning = false; QTimer dismissError; dismissError.setInterval(20);
        connect(&dismissError, &QTimer::timeout, [&] { if (auto warning = window.findChild<QMessageBox *>()) { sawWarning = true; if (auto ok = warning->button(QMessageBox::Ok)) ok->click(); } }); dismissError.start();
        auto address = panelWidget<QLineEdit>(window, "currentPath"); QVERIFY(address); address->setText(corrupt + 'x'); address->setFocus(); address->setCursorPosition(address->text().size());
        QTest::keyClick(address, Qt::Key_Backspace); QCOMPARE(address->text(), corrupt); QCOMPARE(window.currentArchivePath(), previousArchive);
        QTest::keyClick(address, Qt::Key_Return);
        QTRY_VERIFY_WITH_TIMEOUT(sawWarning && !window.operationBusy(), 10000); dismissError.stop();
        QCOMPARE(window.currentArchivePath(), previousArchive); QCOMPARE(address->text(), previousArchive + '/'); QVERIFY(tree->isEnabled());
        window.openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QMimeData mime; mime.setUrls({QUrl::fromLocalFile(temp.filePath("roundtrip.zip"))});
        const QPoint emptyDropPoint(30, tree->viewport()->height() - 12);
        QDragEnterEvent enter(emptyDropPoint, Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(tree->viewport(), &enter); QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(emptyDropPoint), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(tree->viewport(), &drop); QVERIFY(drop.isAccepted());
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(window.currentArchivePath(), temp.filePath("roundtrip.zip")); window.close();
    }
    void guiCancel() {
        MainWindow window(executable); window.show(); window.raise(); window.activateWindow(); QVERIFY(activateTestWindow(&window)); window.openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto tree = panelWidget<FileList>(window, "fileList"); QVERIFY(tree);
        panelWidget<QLineEdit>(window, "currentPath")->setFocus();
        auto details = window.findChild<QAction *>("mode3Action", Qt::FindDirectChildrenOnly); QVERIFY(details); details->trigger();
        QTRY_VERIFY(tree->isVisible() && tree->isEnabled());
        for (int n = 0; n < tree->topLevelItemCount(); ++n) if (tree->topLevelItem(n)->text(0) == "large.bin") { tree->setCurrentItem(tree->topLevelItem(n)); tree->topLevelItem(n)->setSelected(true); }
        QString archive = temp.filePath("gui-cancel.7z");
        QTimer::singleShot(100, [&] { auto d = window.findChild<AddDialog *>("addDialog"); if (d) { d->findChild<QLineEdit *>("archivePath")->setText(archive); d->accept(); } });
        QTest::mouseClick(window.findChild<QToolButton *>("addButton"), Qt::LeftButton); QVERIFY(window.operationBusy());
        auto cancel = window.findChild<QPushButton *>("cancelOperation"); QVERIFY(cancel); QTest::qWait(150); confirmProgressCancellation(cancel->parentWidget()); QTest::mouseClick(cancel, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!QFileInfo::exists(archive)); QVERIFY(tree->isEnabled()); window.close();
    }
    void guiOptionsPersistenceAndBehavior() {
        const auto initial = FileManagerSettings::load();
        struct Restore { FileManagerSettings value; ~Restore() { value.save(); } } restore{initial};
        const QString root = temp.filePath("options files"); QDir().mkpath(root + "/child");
        writeFile(root + "/first 日本語.txt", "first"); writeFile(root + "/second file.txt", "second");
        MainWindow window(executable); window.show(); window.raise(); window.activateWindow(); QVERIFY(activateTestWindow(&window)); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto tree = panelWidget<FileList>(window, "fileList"); QVERIFY(tree);
        panelWidget<QLineEdit>(window, "currentPath")->setFocus();
        auto details = window.findChild<QAction *>("mode3Action", Qt::FindDirectChildrenOnly); QVERIFY(details); details->trigger();
        QTRY_VERIFY(tree->isVisible() && tree->isEnabled());
        auto find = [tree](QString name) -> QTreeWidgetItem * { for (int n = 0; n < tree->topLevelItemCount(); ++n) if (tree->topLevelItem(n)->text(0) == name) return tree->topLevelItem(n); return nullptr; };
        auto options = window.findChild<QAction *>("optionsAction"); QVERIFY(options); QVERIFY(options->isEnabled());
        bool sawOptions = false;
        QTimer::singleShot(100, [&] { if (auto d = window.findChild<OptionsDialog *>()) { sawOptions = true; d->reject(); } });
        auto bar = window.menuBar(); auto tools = bar->actions()[4]->menu();
        QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, bar->mapTo(&window, bar->actionGeometry(bar->actions()[4]).center())); QTRY_VERIFY(tools->isVisible());
        QTest::mouseClick(tools->windowHandle(), Qt::LeftButton, Qt::NoModifier, tools->actionGeometry(options).center()); QVERIFY(sawOptions);
        OptionsDialog dialog(&window);
        auto tabs = dialog.findChild<QTabWidget *>("optionsTabs"); QCOMPARE(tabs->count(), 6);
        QStringList names; for (int n = 0; n < tabs->count(); ++n) names << tabs->tabText(n); QCOMPARE(names, QStringList({"System", "7-Zip", "Folders", "Editor", "Settings", "Language"}));
        auto buttons = dialog.findChild<QDialogButtonBox *>("optionsButtons"); QVERIFY(!buttons->button(QDialogButtonBox::Apply)->isEnabled());
        // Cancel must leave the stored value unchanged.
        dialog.findChild<QCheckBox *>("showDots")->setChecked(!initial.showDots); dialog.reject(); QCOMPARE(FileManagerSettings::load().showDots, initial.showDots);
        bool applied = false; QTimer::singleShot(100, [&] {
            auto d = window.findChild<OptionsDialog *>("optionsDialog");
            // The earlier direct instance is hidden; find the actual menu dialog.
            for (auto candidate : window.findChildren<OptionsDialog *>()) if (candidate->isVisible()) d = candidate;
            if (!d) return;
            d->findChild<QTabWidget *>()->setCurrentIndex(4);
            d->findChild<QCheckBox *>("showDots")->setChecked(true); d->findChild<QCheckBox *>("showGrid")->setChecked(true); d->findChild<QCheckBox *>("fullRow")->setChecked(true); d->findChild<QCheckBox *>("singleClick")->setChecked(true);
            d->findChild<QCheckBox *>("memoryLimitEnabled")->setChecked(true); d->findChild<QSpinBox *>("memoryLimitGB")->setValue(1);
            QTest::mouseClick(d->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply), Qt::LeftButton);
            applied = FileManagerSettings::load().showDots && tree->property("showGrid").toBool();
            d->findChild<QCheckBox *>("showDots")->setChecked(false); d->reject();
        }); options->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(applied); QVERIFY(FileManagerSettings::load().showDots); QVERIFY(find("..")); QCOMPARE(tree->selectionBehavior(), QAbstractItemView::SelectRows);
        auto child = find("child"); QVERIFY(child); QSignalSpy clicked(tree, &QTreeWidget::itemClicked);
        QTest::mouseClick(tree->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(tree->columnViewportPosition(0) + 24, tree->visualItemRect(child).center().y()));
        QVERIFY2(!clicked.isEmpty(), "The owned folder row must receive the single click.");
        QTRY_COMPARE(window.currentDirectory(), root + "/child");
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        {
            MainWindow reopened(executable); reopened.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 10000); QVERIFY(panelWidget<FileList>(reopened)->property("showGrid").toBool());
            OptionsDialog reread; QVERIFY(reread.findChild<QCheckBox *>("showDots")->isChecked()); QVERIFY(reread.findChild<QCheckBox *>("singleClick")->isChecked()); QCOMPARE(reread.findChild<QSpinBox *>("memoryLimitGB")->value(), 1);
        }
        // Viewer/editor/diff receive literal UTF-8 paths and arguments, without a shell.
        auto value = FileManagerSettings::load(); value.singleClick = false; value.alternativeSelection = true;
        const QString capture = temp.filePath("captured args.txt"), helper = temp.filePath("capture helper.sh");
        writeFile(helper, "#!/bin/sh\noutput=$1\nshift\nprintf '%s\\n' \"$@\" > \"$output\"\n"); QVERIFY(QFile::setPermissions(helper, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        value.viewer = '"' + helper + "\" \"" + capture + "\" viewer"; value.editor = '"' + helper + "\" \"" + capture + "\" editor"; value.diff = '"' + helper + "\" \"" + capture + "\" diff";
        auto previous = value; previous.viewer.clear(); previous.editor.clear(); previous.diff.clear(); QVERIFY(previous.save());
        OptionsDialog programs;
        programs.findChild<QLineEdit *>("viewerCommand")->setText(value.viewer); programs.findChild<QLineEdit *>("editorCommand")->setText(value.editor); programs.findChild<QLineEdit *>("diffCommand")->setText(value.diff);
        const QString customWork = temp.filePath("UI working folder 日本語"); QDir().mkpath(customWork);
        programs.findChild<QRadioButton *>("workSpecified")->setChecked(true); programs.findChild<QLineEdit *>("workPath")->setText(customWork); programs.findChild<QCheckBox *>("removableOnly")->setChecked(false);
        QTest::mouseClick(programs.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
        QCOMPARE(FileManagerSettings::load().viewer, value.viewer); QCOMPARE(FileManagerSettings::load().editor, value.editor); QCOMPARE(FileManagerSettings::load().diff, value.diff); QCOMPARE(FileManagerSettings::load().workingFolder(root), customWork);
        // Reopening reloads preferences, as an application restart would.
        MainWindow configured(executable); configured.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!configured.operationBusy(), 10000); configured.show(); configured.raise(); configured.activateWindow(); QVERIFY(activateTestWindow(&configured));
        auto list = panelWidget<FileList>(configured); QCOMPARE(list->selectionMode(), QAbstractItemView::SingleSelection);
        auto select = [list](QString name) { for (int n = 0; n < list->topLevelItemCount(); ++n) if (list->topLevelItem(n)->text(0) == name) { list->setCurrentItem(list->topLevelItem(n)); list->setItemSelected(list->topLevelItem(n), true); } };
        auto captured = [&] { QFile file(capture); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); };
        select("first 日本語.txt"); list->setFocus(); QTest::keyClick(list, Qt::Key_F3); QTRY_COMPARE(captured(), ("viewer\n" + root + "/first 日本語.txt\n").toUtf8());
        QVERIFY(QFile::remove(capture)); QTest::keyClick(list, Qt::Key_F4); QTRY_COMPARE(captured(), ("editor\n" + root + "/first 日本語.txt\n").toUtf8());
        QVERIFY(QFile::remove(capture)); select("second file.txt"); auto diff = configured.findChild<QAction *>("diffAction"); QTRY_VERIFY(diff->isEnabled()); diff->trigger();
        QTRY_VERIFY(captured().startsWith("diff\n")); QVERIFY(captured().contains((root + "/first 日本語.txt\n").toUtf8())); QVERIFY(captured().contains((root + "/second file.txt\n").toUtf8()));
        configured.close(); window.close();
    }
    void configuredWorkingFolderAndMemory() {
        FileManagerSettings value; value.workMode = 2; value.workPath = temp.filePath("custom work 日本語"); value.removableOnly = false; QDir().mkpath(value.workPath);
        QCOMPARE(value.workingFolder(input), value.workPath); value.workMode = 1; QCOMPARE(value.workingFolder(input), input); value.workMode = 0; QCOMPARE(value.workingFolder(input), QDir::tempPath());
        value.removableOnly = true; QCOMPARE(value.workingFolder(input), input);
        SevenZipProcessBackend backend(executable); auto r = addRequest(temp.filePath("options-staging.7z")); r.temporaryDirectory = value.workPath;
        QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(r);
        QVERIFY(!QDir(value.workPath).entryList({".7zip-add-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
        if (done.isEmpty()) QVERIFY(done.wait(30000)); QVERIFY(qvariant_cast<ArchiveResult>(done[0][0]).success);
        QVERIFY(QDir(value.workPath).entryList({".7zip-add-*"}, QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot).isEmpty());
        r.updateMode = "update"; r.files = {"input/日本語ファイル.txt"}; auto updated = execute(backend, r); QVERIFY2(updated.success, qPrintable(updated.details));
        r.operation = ArchiveOperation::Test; r.files.clear(); r.memoryLimitGB = 1; auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details));
        r.operation = ArchiveOperation::Extract; r.outputDirectory = temp.filePath("options-memory-extract"); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details)); QCOMPARE(hash(input + "/large.bin"), hash(r.outputDirectory + "/input/large.bin"));
    }
    void officialLanguagesAndRootOptions() {
        const auto initial = FileManagerSettings::load(); struct Restore { FileManagerSettings v; ~Restore() { v.save(); UiLanguage::set(v.language); } } restore{initial};
        QCOMPARE(UiLanguage::available().size(), 93); UiLanguage::set("ja"); QVERIFY(UiLanguage::text("&File").contains("ファイル")); QVERIFY(UiLanguage::text("Cancel").contains("キャンセル"));
        auto preferences = initial; preferences.language = "ja"; QVERIFY(preferences.save());
        MainWindow window(executable); window.show(); QVERIFY(window.menuBar()->actions().first()->text().contains("ファイル"));
        OptionsDialog dialog(&window); auto language = dialog.findChild<QComboBox *>("uiLanguage"); QCOMPARE(language->currentData().toString(), QString("ja"));
        language->setCurrentIndex(language->findData("en")); dialog.findChild<QCheckBox *>("eliminateRoot")->setChecked(false); dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        QCOMPARE(FileManagerSettings::load().language, QString("en")); QVERIFY(!FileManagerSettings::load().eliminateRoot); QCOMPARE(UiLanguage::text("Cancel"), QString("Cancel")); dialog.reject(); window.close();
        ExtractDialog extract(temp.filePath("extract-root-option"), false); QVERIFY(!extract.findChild<QCheckBox *>("extractEliminateRoot")->isChecked()); extract.findChild<QCheckBox *>("extractEliminateRoot")->setChecked(true); QVERIFY(extract.options().eliminateRoot);
    }
    void eliminateRootAndRenameExisting() {
        SevenZipProcessBackend backend(executable); ArchiveRequest r; r.operation = ArchiveOperation::Extract; r.archive = temp.filePath("roundtrip.7z"); r.outputDirectory = temp.filePath("root-elimination/input"); r.pathMode = "full"; r.eliminateRoot = true;
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details + result.message)); QVERIFY(QFileInfo::exists(r.outputDirectory + "/large.bin")); QVERIFY(!QFileInfo::exists(r.outputDirectory + "/input")); QCOMPARE(hash(input + "/large.bin"), hash(r.outputDirectory + "/large.bin"));
        r.outputDirectory = temp.filePath("root-name-diff"); result = execute(backend, r); QVERIFY(result.success); QVERIFY(QFileInfo::exists(r.outputDirectory + "/input/large.bin"));
        r.files = {"input/日本語ファイル.txt"}; r.overwriteMode = "renameExisting"; writeFile(r.outputDirectory + "/input/日本語ファイル.txt", "old content"); result = execute(backend, r); QVERIFY(result.success);
        QCOMPARE(hash(input + "/日本語ファイル.txt"), hash(r.outputDirectory + "/input/日本語ファイル.txt")); QFile old(r.outputDirectory + "/input/日本語ファイル_1.txt"); QVERIFY(old.open(QIODevice::ReadOnly)); QCOMPARE(old.readAll(), QByteArray("old content"));
    }
    void advancedCompressionSettings() {
        SevenZipProcessBackend backend(executable); const QString file = temp.filePath("advanced-source 日本語.txt"); writeFile(file, "advanced options\n");
        const auto native = QFile::encodeName(file); const timespec times[2] = {{1600000000, 123456789}, {1700000000, 0}}; QVERIFY(::utimensat(AT_FDCWD, native.constData(), times, 0) == 0);
        ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = temp.filePath("advanced-options.7z"); r.workingDirectory = temp.path(); r.files = {QFileInfo(file).fileName()}; r.dictionary = "1m"; r.parameters = "d=1m fb=32"; r.compressionMemory = "50%"; r.modificationTime = 1; r.creationTime = 1; r.accessTime = 1; r.timestampPrecision = 0; r.latestArchiveTime = true; r.preserveAccessTime = true;
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); struct stat after; QVERIFY(::stat(native.constData(), &after) == 0); QCOMPARE(after.st_atimespec.tv_sec, time_t(1600000000)); QCOMPARE(after.st_atimespec.tv_nsec, long(123456789)); QCOMPARE(QFileInfo(r.archive).lastModified().toSecsSinceEpoch(), qint64(1700000000));
        r.operation = ArchiveOperation::List; r.files.clear(); result = execute(backend, r); QVERIFY(result.success); QCOMPARE(result.entries.size(), 1); QVERIFY(result.entries.first().modified.startsWith("2023-"));
        r.operation = ArchiveOperation::Add; r.archive = temp.filePath("omit-time.7z"); r.files = {QFileInfo(file).fileName()}; r.modificationTime = 0; r.creationTime = 0; r.accessTime = 0; r.latestArchiveTime = false; result = execute(backend, r); QVERIFY(result.success);
        r.operation = ArchiveOperation::List; result = execute(backend, r); QVERIFY(result.success); QVERIFY(result.entries.first().modified.isEmpty());
        r.operation = ArchiveOperation::Add; r.archive = temp.filePath("invalid-properties.7z"); r.parameters = "-psecret"; result = execute(backend, r); QVERIFY(!result.success); QVERIFY(!QFileInfo::exists(r.archive));
        r.parameters.clear(); r.compressionMemory.clear(); r.archive = temp.filePath("full-path.7z"); r.pathMode = "full"; result = execute(backend, r); QVERIFY(result.success); r.operation = ArchiveOperation::List; result = execute(backend, r); QVERIFY(result.success); QVERIFY(result.entries.first().path.endsWith("advanced-source 日本語.txt")); QVERIFY(result.entries.first().path.contains("folders/")); QVERIFY(!result.entries.first().path.startsWith('/'));
        AddDialog dialog(temp.filePath("settings-ui.7z")); dialog.show(); dialog.raise(); dialog.activateWindow(); QVERIFY(activateTestWindow(&dialog));
        QTimer::singleShot(100, [&] { if (auto d = dialog.findChild<QDialog *>("compressOptionsDialog")) { d->findChild<QCheckBox *>("storeModificationTimeSet")->setChecked(true); d->findChild<QCheckBox *>("storeModificationTime")->setChecked(false); d->findChild<QCheckBox *>("preserveAccessTime")->setChecked(true); d->accept(); } });
        QTest::mouseClick(dialog.findChild<QPushButton *>("compressionOptions"), Qt::LeftButton); QCOMPARE(dialog.options().modificationTime, 0); QVERIFY(dialog.options().preserveAccessTime); dialog.findChild<QLineEdit *>("compressionParameters")->setText("d=1m"); dialog.accept();
        AddDialog reopened(temp.filePath("settings-ui.7z")); QCOMPARE(reopened.options().modificationTime, 0); QVERIFY(reopened.options().preserveAccessTime); QCOMPARE(reopened.options().parameters, QString("d=1m")); dialog.close();
    }
    void trashAfterVerifiedCompression() {
        SevenZipProcessBackend backend(executable); const QString source = temp.filePath("trash-after-test.txt"); writeFile(source, "source preserved in archive\n");
        ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = temp.filePath("trash-after.7z"); r.workingDirectory = temp.path(); r.files = {QFileInfo(source).fileName()}; r.deleteAfter = true;
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(QFileInfo::exists(r.archive)); QVERIFY(!QFileInfo::exists(source));
        r.operation = ArchiveOperation::Extract; r.outputDirectory = temp.filePath("trash-after-extract"); r.files.clear(); result = execute(backend, r); QVERIFY(result.success); QFile restored(r.outputDirectory + "/trash-after-test.txt"); QVERIFY(restored.open(QIODevice::ReadOnly)); QCOMPARE(restored.readAll(), QByteArray("source preserved in archive\n"));
        writeFile(source, "must remain after failure"); r.operation = ArchiveOperation::Add; r.archive = temp.filePath("missing-parent/fail.7z"); r.files = {QFileInfo(source).fileName()}; result = execute(backend, r); QVERIFY(!result.success); QVERIFY(QFileInfo::exists(source));
    }
    void compressionMethods() {
        SevenZipProcessBackend backend(executable);
        writeFile(temp.filePath("method-input.txt"), "codec integration test\n");
        for (const auto &format : {QString("7z"), QString("zip")}) {
            const QStringList methods = format == "7z" ? QStringList{"Copy", "LZMA2", "LZMA", "PPMd", "BZip2", "Deflate", "Deflate64"} : QStringList{"Copy", "Deflate", "Deflate64", "BZip2", "LZMA", "PPMd"};
            for (const auto &method : methods) {
                auto r = addRequest(temp.filePath("method-" + format + '-' + method + '.' + format), format); r.files = {"method-input.txt"}; r.method = method;
                if (method == "PPMd") r.wordSize = "8";
                if (method == "BZip2") r.dictionary = "900k";
                auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(format + ' ' + method + result.details));
                r.operation = ArchiveOperation::Test; r.files.clear(); QVERIFY(execute(backend, r).success);
            }
        }
    }
    void absoluteExtractionAndDialogSettings() {
        const QString source = QFileInfo(temp.path()).canonicalFilePath() + "/absolute-target.txt"; writeFile(source, "absolute path roundtrip\n"); SevenZipProcessBackend backend(executable);
        ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = temp.filePath("absolute-created.7z"); r.workingDirectory = QFileInfo(source).absolutePath(); r.files = {QFileInfo(source).fileName()}; r.pathMode = "absolute";
        auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); r.operation = ArchiveOperation::List; r.files.clear(); result = execute(backend, r); QVERIFY(result.success); QCOMPARE(result.entries.first().path, source);
        QVERIFY(QFile::rename(source, source + ".original")); r.operation = ArchiveOperation::Extract; r.outputDirectory = temp.filePath("absolute-staging"); r.pathMode = "full"; result = execute(backend, r); QVERIFY(!result.success); QVERIFY(!QFileInfo::exists(source));
        r.pathMode = "absolute"; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(hash(source), hash(source + ".original"));
        ExtractDialog dialog(temp.filePath("named-output"), false); auto named = dialog.findChild<QCheckBox *>("extractNameEnabled"); named->setChecked(true); QCOMPARE(dialog.options().outputDirectory, temp.filePath("named-output")); dialog.findChild<QComboBox *>("extractOverwriteMode")->setCurrentIndex(4); dialog.accept();
        ExtractDialog reopened(temp.filePath("second-output"), false); QCOMPARE(reopened.options().overwriteMode, QString("renameExisting")); QVERIFY(reopened.findChild<QCheckBox *>("extractNameEnabled")->isChecked()); QVERIFY(QSettings().value("Extract/History").toStringList().contains(temp.path())); QSettings().remove("Extract");
    }
    void additionalFormatsAndLinks() {
        writeFile(temp.filePath("formats/plain.txt"), "additional archive formats\n"); QDir().mkpath(temp.filePath("formats/empty"));
        SevenZipProcessBackend backend(executable);
        for (const auto &fmt : {QString("tar"), QString("wim"), QString("xz"), QString("gzip"), QString("bzip2")}) {
            AddDialog dialog(temp.filePath("format-result.7z")); dialog.findChild<QComboBox *>("archiveFormat")->setCurrentText(fmt); auto r = dialog.options(); r.archive = temp.filePath("format-result." + fmt); r.workingDirectory = temp.path(); r.files = {fmt == "tar" || fmt == "wim" ? "formats" : "formats/plain.txt"}; r.parameters.clear();
            auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(fmt + result.message + result.details)); r.operation = ArchiveOperation::Test; r.files.clear(); result = execute(backend, r); QVERIFY2(result.success, qPrintable(fmt + result.message + result.details)); r.operation = ArchiveOperation::Extract; r.outputDirectory = temp.filePath("format-out-" + fmt); r.overwriteMode = "overwrite"; result = execute(backend, r); QVERIFY2(result.success, qPrintable(fmt + result.message + result.details));
            QDirIterator it(r.outputDirectory, QDir::Files, QDirIterator::Subdirectories); QVERIFY(it.hasNext()); QCOMPARE(hash(it.next()), hash(temp.filePath("formats/plain.txt")));
        }
        const auto linkPath = QFile::encodeName(temp.filePath("formats/link.txt")); QVERIFY(::symlink("plain.txt", linkPath.constData()) == 0);
        AddDialog tar(temp.filePath("links.tar")); tar.findChild<QComboBox *>("archiveFormat")->setCurrentText("tar");
        QTimer::singleShot(100, [&] { auto d = tar.findChild<QDialog *>("compressOptionsDialog"); QVERIFY(d); auto check = d->findChild<QCheckBox *>("storeHardLinks"); QVERIFY(check->isEnabled()); check->setChecked(true); d->accept(); }); QTest::mouseClick(tar.findChild<QPushButton *>("compressionOptions"), Qt::LeftButton); auto r = tar.options(); r.workingDirectory = temp.path(); r.files = {"formats"}; r.storeSymbolicLinks = true; r.archive = temp.filePath("links.tar"); auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); r.operation = ArchiveOperation::List; r.files.clear(); result = execute(backend, r); QVERIFY(result.success); bool link = false; for (const auto &entry : result.entries) link |= entry.link; QVERIFY(link);
    }
    void viewSettingsAndTwoPanels() {
        QSettings().remove("View"); FileManagerSettings state = FileManagerSettings::load(); state.singleClick = false; state.save();
        MainWindow window(executable); window.show(); window.raise(); window.activateWindow(); QVERIFY(activateTestWindow(&window)); window.openPath(input); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto tree = panelWidget<FileList>(window, "fileList"); auto icons = panelWidget<IconFileList>(window, "iconFileList");
        tree->setFocus(); QTest::keyClick(tree, Qt::Key_1, Qt::ControlModifier); QVERIFY(icons->isVisible()); QTest::keyClick(icons, Qt::Key_4, Qt::ControlModifier); QVERIFY(tree->isVisible()); window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); bool nested = false; for (int n = 0; n < tree->topLevelItemCount(); ++n) nested |= tree->topLevelItem(n)->text(0) == "data.txt"; QVERIFY(nested); window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        window.findChild<QAction *>("time9Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(tree->topLevelItem(0)->text(2).size(), 29); tree->setColumnWidth(0, 310); tree->header()->moveSection(tree->header()->visualIndex(1), 2); window.findChild<QAction *>("toolbarTextAction")->trigger(); QCOMPARE(window.findChild<QToolButton *>("addButton")->toolButtonStyle(), Qt::ToolButtonIconOnly);
        window.findChild<QAction *>("autoRefreshAction")->trigger(); writeFile(input + "/watch-off.txt", "off"); QTest::qWait(450); bool appeared = false; for (int n = 0; n < tree->topLevelItemCount(); ++n) appeared |= tree->topLevelItem(n)->text(0) == "watch-off.txt"; QVERIFY(!appeared);
        window.findChild<QAction *>("autoRefreshAction")->trigger(); writeFile(input + "/watch-on.txt", "on"); QTRY_VERIFY_WITH_TIMEOUT(tree->topLevelItemCount() >= QDir(input).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).size(), 3000);
        tree->setFocus(); QTest::keyClick(tree, Qt::Key_F9); auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second); QVERIFY(!second->isWindow()); second->openPath(temp.path()); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto secondTree = second->findChild<FileList *>("fileList"); window.raise(); window.activateWindow(); QVERIFY(activateTestWindow(&window)); secondTree->setFocus(); QTRY_VERIFY(secondTree->hasFocus());
        QTimer::singleShot(100, [&] { if (auto d = testInput(second)) { d->setTextValue("second-panel-folder"); d->accept(); } }); QTest::keyClick(secondTree, Qt::Key_F7); QTRY_VERIFY(QFileInfo(temp.filePath("second-panel-folder")).isDir());
        window.close(); MainWindow reopened(executable); reopened.show(); QTRY_VERIFY_WITH_TIMEOUT(!reopened.operationBusy(), 10000); QVERIFY(reopened.findChild<MainWindow *>("secondPanel")); FileList *restored = nullptr; auto child = reopened.findChild<MainWindow *>("secondPanel"); for (auto list : reopened.findChildren<FileList *>("fileList")) if (!child->isAncestorOf(list)) restored = list; QVERIFY(restored); QCOMPARE(restored->columnWidth(0), 310); QCOMPARE(restored->header()->visualIndex(1), 2); QVERIFY(reopened.findChild<QAction *>("time9Action")->isChecked()); reopened.findChild<QAction *>("twoPanelsAction")->trigger(); QVERIFY(!reopened.findChild<MainWindow *>("secondPanel")->isVisible()); reopened.close(); QSettings().remove("View");
    }
    void verifiedTrashOtherFormatsAndUpdateModes() {
        SevenZipProcessBackend backend(executable);
        for (const auto &fmt : {QString("tar"), QString("xz"), QString("7z")}) {
            writeFile(temp.filePath("verified-source.txt"), "verified before Trash\n"); ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = temp.filePath("verified-trash." + fmt); r.format = fmt; r.method = fmt == "tar" ? "POSIX" : "LZMA2"; r.level = fmt == "tar" ? 0 : 1; r.workingDirectory = temp.path(); r.files = {"verified-source.txt"}; r.deleteAfter = true; r.preserveAccessTime = true; if (fmt == "7z") { r.password = "verification password"; r.encryptNames = true; }
            auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(fmt + result.message + result.details)); QVERIFY(!QFileInfo::exists(temp.filePath("verified-source.txt"))); QVERIFY(QFileInfo::exists(r.archive));
        }
        writeFile(temp.filePath("update-modes/a.txt"), "old"); auto r = addRequest(temp.filePath("update-modes.zip"), "zip"); r.files = {"update-modes"}; r.compressionMemory = "50%"; auto result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details));
        writeFile(temp.filePath("update-modes/a.txt"), "new content"); writeFile(temp.filePath("update-modes/b.txt"), "new file"); r.updateMode = "fresh"; result = execute(backend, r); QVERIFY(result.success); r.operation = ArchiveOperation::List; result = execute(backend, r); QVERIFY(result.success); bool hasNew = false; for (const auto &entry : result.entries) hasNew |= entry.path.endsWith("b.txt"); QVERIFY(!hasNew);
        QVERIFY(QFile::remove(temp.filePath("update-modes/a.txt"))); r.operation = ArchiveOperation::Add; r.updateMode = "sync"; result = execute(backend, r); QVERIFY(result.success); r.operation = ArchiveOperation::List; result = execute(backend, r); QVERIFY(result.success); hasNew = false; bool hasOld = false; for (const auto &entry : result.entries) { hasNew |= entry.path.endsWith("b.txt"); hasOld |= entry.path.endsWith("a.txt"); } QVERIFY(hasNew); QVERIFY(!hasOld);
    }
    void openWithSettingsAndDelivery() {
        QSettings().remove("OpenWith"); QSettings().remove("View"); UiLanguage::set("en");
        const auto a = temp.filePath("open delivery/ASCII.txt"), b = temp.filePath("open delivery/日本語 file.txt"); writeFile(a, "A"); writeFile(b, "B");
        MainWindow window(executable); OpenWithController controller(&window);
        QFileOpenEvent first(a), second(b); QVERIFY(QApplication::sendEvent(qApp, &first)); QVERIFY(QApplication::sendEvent(qApp, &second));
        QTRY_VERIFY(controller.currentMenu()); auto menu = controller.currentMenu(); QCOMPARE(menu->paths(), QStringList({a, b})); QVERIFY(!window.isVisible()); QVERIFY(window.currentArchivePath().isEmpty());
        QVERIFY(menu->isModal()); QCOMPARE(QApplication::activeModalWidget(), menu);
        QVERIFY(!menu->findChild<QPushButton *>("openWith_extract")); QVERIFY(menu->findChild<QPushButton *>("openWith_7z"));
        QCOMPARE(menu->layout()->itemAt(menu->layout()->count() - 1)->widget()->objectName(), QString("openWith_manager"));
        menu->reject(); QTRY_VERIFY(!controller.currentMenu());
        OptionsDialog options; options.show(); auto rows = options.findChild<QTreeWidget *>("openWithItems"); QCOMPARE(rows->topLevelItemCount(), 10);
        options.findChild<QTabWidget *>("optionsTabs")->setCurrentIndex(1);
        auto firstRow = rows->topLevelItem(0); QTest::mouseClick(rows->viewport(), Qt::LeftButton, Qt::NoModifier, rows->visualItemRect(firstRow).center());
        QCOMPARE(rows->currentItem(), firstRow); QTest::keyClick(rows, Qt::Key_Space); QCOMPARE(firstRow->checkState(0), Qt::Unchecked);
        for (int n = 0; n < rows->topLevelItemCount(); ++n) rows->topLevelItem(n)->setCheckState(0, rows->topLevelItem(n)->data(0, Qt::UserRole) == "zip" ? Qt::Checked : Qt::Unchecked);
        options.findChild<QCheckBox *>("openWithIcons")->setChecked(false); auto buttons = options.findChild<QDialogButtonBox *>("optionsButtons"); QTest::mouseClick(buttons->button(QDialogButtonBox::Apply), Qt::LeftButton);
        QCOMPARE(OpenWithSettings::load().enabled, QStringList{"zip"}); QVERIFY(!OpenWithSettings::load().icons); options.reject();
        controller.enqueue({a}); QTRY_VERIFY(controller.currentMenu()); menu = controller.currentMenu(); QVERIFY(!menu->findChild<QPushButton *>("openWith_7z")); QVERIFY(menu->findChild<QPushButton *>("openWith_zip")->icon().isNull());
        QFileOpenEvent queued(b); QApplication::sendEvent(qApp, &queued); QTest::qWait(150); QCOMPARE(menu->paths(), QStringList{a}); menu->reject(); QTRY_VERIFY(controller.currentMenu()); QCOMPARE(controller.currentMenu()->paths(), QStringList{b});
        QTest::mouseClick(controller.currentMenu()->findChild<QPushButton *>("openWith_manager"), Qt::LeftButton); QTRY_VERIFY(window.isVisible()); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(window.currentDirectory(), QFileInfo(b).absolutePath());
        QCOMPARE(panelWidget<FileList>(window, "fileList")->selectedItems().size(), 1);
        QFileOpenEvent remote(QUrl("https://example.invalid/never-open")); QApplication::sendEvent(qApp, &remote); QTest::qWait(150); QVERIFY(!controller.currentMenu());
        OpenWithSettings empty; QVERIFY(empty.save()); OpenWithMenu onlyManager({a}); QCOMPARE(onlyManager.findChildren<QPushButton *>().size(), 1);
        QSettings().remove("OpenWith"); QCOMPARE(openWithExtractName("example.7z.001"), QString("example")); QCOMPARE(openWithExtractName("example.part1.rar"), QString("example"));
        const auto collision = temp.filePath("open delivery/example.7z"); writeFile(collision, "source"); QCOMPARE(openWithArchiveName({collision}), QString("example_2"));
        QCOMPARE(openWithArchiveName({temp.filePath("open delivery/a.tar.gz")}), QString("a.tar.gz")); window.close();
    }
    void openWithArchiveCommands() {
        QSettings().remove("Compression"); QSettings().remove("View");
        const auto folder = temp.filePath("menu inputs"), source = folder + "/日本語 file.txt"; writeFile(source, "Finder menu roundtrip\n"); QDir().mkpath(folder + "/empty directory");
        MainWindow window(executable); const QStringList sources{source, folder + "/empty directory"};
        for (const auto &format : {QString("7z"), QString("zip")}) {
            auto result = menuExecute(window, format, sources); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(QFileInfo::exists(folder + "/menu inputs." + format));
            for (auto progress : window.findChildren<ProgressDialog *>()) QVERIFY(!progress->isVisible());
        }
        auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy tested(backend, &ArchiveBackend::finished); window.executeOpenWith("test", {folder + "/menu inputs.7z", folder + "/menu inputs.zip"}); QTRY_COMPARE_WITH_TIMEOUT(tested.size(), 2, 30000); for (const auto &args : tested) QVERIFY(qvariant_cast<ArchiveResult>(args[0]).success); QCOMPARE(resultDialogs.notices.size(), 2); for (const auto &notice : resultDialogs.notices) { QVERIFY(!notice.error); QVERIFY(notice.text.contains(UiLanguage::resource(3001))); QVERIFY(notice.text.contains(UiLanguage::resource(3907))); }
        const auto here = temp.filePath("menu here"); QDir().mkpath(here); QVERIFY(QFile::copy(folder + "/menu inputs.7z", here + "/archive.7z")); auto result = menuExecute(window, "here", {here + "/archive.7z"}); QVERIFY2(result.success, qPrintable(result.details)); QCOMPARE(hash(source), hash(here + "/日本語 file.txt")); QVERIFY(QFileInfo(here + "/empty directory").isDir());
        const auto to = temp.filePath("menu to"); QDir().mkpath(to); QVERIFY(QFile::copy(folder + "/menu inputs.zip", to + "/archive.zip")); result = menuExecute(window, "to", {to + "/archive.zip"}); QVERIFY(result.success); QCOMPARE(hash(source), hash(to + "/archive/日本語 file.txt"));
        QTimer::singleShot(100, [&] { auto d = window.findChild<AddDialog *>(); QVERIFY(d); d->findChild<QLineEdit *>("archivePath")->setText(folder + "/dialog.7z"); d->accept(); }); result = menuExecute(window, "add", {source}); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(QFileInfo::exists(folder + "/dialog.7z"));
        const auto dialogOut = temp.filePath("menu dialog"); QDir().mkpath(dialogOut); QVERIFY(QFile::copy(folder + "/dialog.7z", dialogOut + "/dialog.7z")); QTimer::singleShot(100, [&] { auto d = window.findChild<ExtractDialog *>(); QVERIFY(d); d->accept(); }); result = menuExecute(window, "extract", {dialogOut + "/dialog.7z"}); QVERIFY(result.success); QCOMPARE(hash(source), hash(dialogOut + "/dialog/日本語 file.txt"));
        SevenZipProcessBackend encrypted(executable); auto r = addRequest(temp.filePath("menu encrypted/secret.7z")); QDir().mkpath(QFileInfo(r.archive).absolutePath()); r.workingDirectory = folder; r.files = {"日本語 file.txt"}; r.password = "menu secret"; r.encryptNames = true; QVERIFY(execute(encrypted, r).success);
        QTimer passwordPrompt; passwordPrompt.setInterval(50); connect(&passwordPrompt, &QTimer::timeout, &window, [&] { if (auto prompt = testInput(&window)) { prompt->setTextValue("menu secret"); prompt->accept(); passwordPrompt.stop(); } }); passwordPrompt.start();
        QSignalSpy extraction(backend, &ArchiveBackend::finished); window.executeOpenWith("here", {r.archive}); QTRY_VERIFY_WITH_TIMEOUT(extraction.size() >= 2, 15000); QVERIFY(qvariant_cast<ArchiveResult>(extraction.last()[0]).success); QCOMPARE(hash(source), hash(temp.filePath("menu encrypted/日本語 file.txt"))); window.close();
        QVERIFY(QFile::copy(folder + "/dialog.7z", folder + "/archive.txt")); window.openPath(folder); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto list = panelWidget<FileList>(window, "fileList"); for (int n = 0; n < list->topLevelItemCount(); ++n) if (list->topLevelItem(n)->text(0) == "archive.txt") list->setCurrentItem(list->topLevelItem(n));
        QSignalSpy inside(backend, &ArchiveBackend::finished); window.findChild<QAction *>("insideAction")->trigger(); QVERIFY(inside.wait(10000)); QVERIFY(qvariant_cast<ArchiveResult>(inside.last()[0]).success); QCOMPARE(window.currentArchivePath(), folder + "/archive.txt");
    }
    void officialChecksumsAndHashFormat() {
        const auto source = temp.filePath("menu hashes/日本語 file.txt"); writeFile(source, "checksum data\n"); MainWindow window(executable);
        for (const auto &method : checksumMethods()) { auto result = menuExecute(window, "hash:" + method.second, {source}); QVERIFY2(result.success, qPrintable(method.first + result.details)); if (method.second == "SHA256") QVERIFY(result.details.contains(QString::fromLatin1(hash(source).toHex()), Qt::CaseInsensitive)); }
        auto result = menuExecute(window, "hashFile", {source}); QVERIFY2(result.success, qPrintable(result.message + result.details)); const auto manifest = source + ".sha256"; QFile file(manifest); QVERIFY(file.open(QIODevice::ReadOnly)); const auto contents = file.readAll(); QVERIFY(contents.contains(hash(source).toHex())); QVERIFY(contents.contains(QString("日本語 file.txt").toUtf8()));
        result = menuExecute(window, "hashTest", {manifest}); QVERIFY2(result.success, qPrintable(result.details)); result = menuExecute(window, "hashFile", {source}); QVERIFY(!result.success); QFile preserved(manifest); QVERIFY(preserved.open(QIODevice::ReadOnly)); QCOMPARE(preserved.readAll(), contents);
        window.openPath(QFileInfo(manifest).absolutePath()); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto list = panelWidget<FileList>(window, "fileList");
        for (int n = 0; n < list->topLevelItemCount(); ++n) if (list->topLevelItem(n)->text(0) == QFileInfo(manifest).fileName()) list->setCurrentItem(list->topLevelItem(n));
        QVERIFY(window.findChild<QAction *>("testAction")->isEnabled()); QSignalSpy opened(window.findChild<SevenZipProcessBackend *>(), &ArchiveBackend::finished);
        window.findChild<QAction *>("openAction")->trigger(); QVERIFY(opened.wait(10000)); QVERIFY(qvariant_cast<ArchiveResult>(opened.last()[0]).success); QCOMPARE(window.currentArchivePath(), manifest);
        SevenZipProcessBackend backend(executable);
        for (const auto &method : {QString("SHA256"), QString("SHA1")}) { AddDialog dialog(temp.filePath("menu hashes/result.7z")); dialog.findChild<QComboBox *>("archiveFormat")->setCurrentText("Hash"); dialog.findChild<QComboBox *>("compressionMethod")->setCurrentText(method); auto r = dialog.options(); QCOMPARE(r.method, method); QVERIFY(!dialog.findChild<QCheckBox *>("deleteAfterCompression")->isEnabled()); r.workingDirectory = QFileInfo(source).absolutePath(); r.files = {QFileInfo(source).fileName()}; r.archive = temp.filePath("menu hashes/" + method + (method == "SHA1" ? ".sha1" : ".sha256")); result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.message + result.details)); r.operation = ArchiveOperation::Test; r.files.clear(); r.hashTest = true; result = execute(backend, r); QVERIFY2(result.success, qPrintable(result.details)); }
        writeFile(source, "tampered\n"); result = menuExecute(window, "hashTest", {manifest}); QVERIFY(!result.success); QVERIFY(result.exitCode != 0); window.close();
    }
    void openWithCancelAndBusyQueue() {
        QSettings().remove("Compression"); QSettings().remove("View"); QSettings().remove("OpenWith");
        const auto folder = temp.filePath("menu cancel"); QDir().mkpath(folder); QVERIFY(QFile::copy(input + "/large.bin", folder + "/large.bin")); const auto after = folder + "/after.txt"; writeFile(after, "still usable\n");
        MainWindow window(executable); OpenWithController controller(&window); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy finished(backend, &ArchiveBackend::finished); int heartbeats = 0; QTimer heartbeat; heartbeat.setInterval(5); connect(&heartbeat, &QTimer::timeout, &window, [&] { ++heartbeats; }); heartbeat.start();
        window.executeOpenWith("7z", {folder + "/large.bin"}); QVERIFY(window.operationBusy()); controller.enqueue({after}); QTest::qWait(100); QVERIFY(!controller.currentMenu()); auto progress = window.findChild<ProgressDialog *>(); QVERIFY(progress); auto cancel = progress->findChild<QPushButton *>("cancelOperation"); QVERIFY(cancel); confirmProgressCancellation(progress); QTest::mouseClick(cancel, Qt::LeftButton); QTRY_VERIFY_WITH_TIMEOUT(!finished.isEmpty(), 10000); QVERIFY(qvariant_cast<ArchiveResult>(finished.last()[0]).cancelled); QVERIFY(heartbeats > 2); QVERIFY(!QFileInfo::exists(folder + "/large.7z")); QVERIFY(QFileInfo::exists(folder + "/large.bin"));
        QTRY_VERIFY(controller.currentMenu()); QCOMPARE(controller.currentMenu()->paths(), QStringList{after}); controller.currentMenu()->reject(); QTRY_VERIFY(!controller.currentMenu()); auto result = menuExecute(window, "hash:SHA256", {after}); QVERIFY(result.success); window.close();
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("Integration");
    Integration test; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&test, args.size() - 1, args.data());
}
#include "integration.moc"
