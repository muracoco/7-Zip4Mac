// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "FileComments.h"
#include "MainWindow.h"
#include "PanelSort.h"
#include "PortStyle.h"
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include "input-driver.h"
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <QTemporaryDir>
#include <QtConcurrent>
#include <sys/xattr.h>
#include <unistd.h>

static QString executable;
static bool write(QString path, QByteArray bytes = {}) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
static QByteArray read(QString path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
static QByteArray hash(QString path) { return QCryptographicHash::hash(read(path), QCryptographicHash::Sha256); }
static FileList *tree(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
static QTreeWidgetItem *item(MainWindow &window, QString name) { const auto found = tree(window)->findItems(name, Qt::MatchExactly); return found.size() == 1 ? found[0] : nullptr; }

class CommentTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(45000)) return {}; return qvariant_cast<ArchiveResult>(done.first()[0]);
    }
    ArchiveRequest zip(QString folder, QString encryption = {}) {
        write(folder + "/input/日本語 space.txt", "target content 日本語"); write(folder + "/input/keep.txt", "keep content"); QDir().mkpath(folder + "/input/empty");
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = folder + "/archive.zip"; request.workingDirectory = folder + "/input"; request.files = {"日本語 space.txt", "keep.txt", "empty"}; request.format = "zip"; request.method = "Deflate"; request.level = 1;
        if (!encryption.isEmpty()) { request.password = "comment-password"; request.encryptionMethod = encryption; } return request;
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        qInfo() << "Owned comment fixtures:" << temporary.path();
    }
    void init() { QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/AutoRefresh", false); FileManagerSettings settings; settings.realIcons = false; QVERIFY(settings.save()); }
    void textPairsCompatibility() {
        auto document = FileComments::parse("\xef\xbb\xbf\r\n\"space name.txt\"  value \r\nASCII.txt comment\r\n日本語.txt 説明\r\n");
        QVERIFY(document.error.isEmpty()); QCOMPARE(document.value("ascii.TXT"), QString("comment")); QCOMPARE(document.value("space name.txt"), QString("value ")); QCOMPARE(document.value("日本語.txt"), QString("説明"));
        QCOMPARE(FileComments::display("visible" + QString(QChar(4)) + "hidden"), QString("visible"));
        QVERIFY(!FileComments::parse(QByteArray("bad.txt \xff\n")).error.isEmpty()); QVERIFY(!FileComments::parse(QByteArray("a x\0b y", 9)).error.isEmpty());
    }
    void originalDuplicateLookup() {
        // Original MyVector pointer heap sort produces second/third/first for
        // three equal IDs; TextPairs::FindID returns the middle matching pair.
        // Writes still refuse duplicate IDs rather than destroying user data.
        const auto document = FileComments::parse("a.txt first\r\nA.txt second\r\na.TXT third\r\n");
        QVERIFY(document.error.isEmpty()); QCOMPARE(document.pairs.size(), 3);
        QCOMPARE(document.pairs[0].value, QString("second"));
        QCOMPARE(document.pairs[1].value, QString("third"));
        QCOMPARE(document.pairs[2].value, QString("first"));
        QCOMPARE(document.value("A.TXT"), QString("third"));
        QVERIFY(document.value("absent.txt").isEmpty());
    }
    void filesystemRoundtripAndMetadata() {
        const auto root = temporary.filePath("filesystem"); QVERIFY(write(root + "/日本語 space.txt", "payload")); QVERIFY(write(root + "/keep.txt", "keep"));
        const auto path = root + "/descript.ion"; QVERIFY(write(path, "keep.txt unrelated\r\n")); QVERIFY(::chmod(QFile::encodeName(path).constData(), 0640) == 0);
        QVERIFY(::setxattr(QFile::encodeName(path).constData(), "org.7zip.tests", "metadata", 8, 0, 0) == 0);
        auto snapshot = FileComments::read(root); QVERIFY(snapshot.error.isEmpty()); auto error = FileComments::write(snapshot, "日本語 space.txt", "  コメント 日本語  "); QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(read(path).startsWith("\xef\xbb\xbf\r\n")); QVERIFY(read(path).contains("\"日本語 space.txt\" コメント 日本語\r\n"));
        snapshot = FileComments::read(root); QCOMPARE(snapshot.value("keep.txt"), QString("unrelated")); QCOMPARE(snapshot.value("日本語 space.txt"), QString("コメント 日本語")); QCOMPARE(snapshot.file.st_mode & 0777, 0640);
        char attribute[8]{}; QCOMPARE(::getxattr(QFile::encodeName(path).constData(), "org.7zip.tests", attribute, 8, 0, 0), 8); QCOMPARE(QByteArray(attribute, 8), QByteArray("metadata"));
        QVERIFY(FileComments::write(snapshot, "日本語 space.txt", "").isEmpty()); snapshot = FileComments::read(root); QVERIFY(snapshot.value("日本語 space.txt").isEmpty()); QCOMPARE(snapshot.value("keep.txt"), QString("unrelated"));
        QVERIFY(FileComments::write(snapshot, "keep.txt", "").isEmpty()); QVERIFY(QFileInfo::exists(path)); QCOMPARE(read(path), QByteArray());
    }
    void refusedWrites_data() {
        QTest::addColumn<QString>("mode"); for (const auto &mode : {"invalid-utf8", "symlink", "read-only", "hardlink", "duplicate-pairs", "changed", "created", "trim-collision", "quote-name", "newline-value", "permission", "folder-changed"}) QTest::newRow(mode) << QString(mode);
    }
    void refusedWrites() {
        QFETCH(QString, mode); const auto root = temporary.filePath("refused " + mode), path = root + "/descript.ion"; QVERIFY(write(root + "/target.txt", "payload"));
        QVERIFY(write(path, mode == "invalid-utf8" ? QByteArray("target.txt \xff\n") : mode == "duplicate-pairs" ? QByteArray("target.txt first\r\nTARGET.TXT second\r\n") : QByteArray("target.txt original\r\n")));
        if (mode == "symlink") { QVERIFY(QFile::rename(path, root + "/real-comment")); QVERIFY(::symlink("real-comment", QFile::encodeName(path).constData()) == 0); }
        if (mode == "read-only") QVERIFY(::chmod(QFile::encodeName(path).constData(), 0444) == 0);
        if (mode == "hardlink") QVERIFY(::link(QFile::encodeName(path).constData(), QFile::encodeName(root + "/other-comment").constData()) == 0);
        if (mode == "created") QVERIFY(QFile::remove(path));
        auto snapshot = FileComments::read(root); QString name = "target.txt", value = "new comment";
        if (mode == "changed" || mode == "created") QVERIFY(write(path, "externally changed\r\n"));
        if (mode == "trim-collision") QVERIFY(write(root + "/ target.txt", "other"));
        if (mode == "quote-name") name = "quote\"name";
        if (mode == "newline-value") value = "first\nsecond";
        const auto original = hash(path); if (mode == "permission") QVERIFY(::chmod(QFile::encodeName(root).constData(), 0555) == 0);
        const auto restore = qScopeGuard([&] { ::chmod(QFile::encodeName(root).constData(), 0755); });
        if (mode == "folder-changed") { QVERIFY(QDir().rename(root, root + " moved")); QVERIFY(QDir().mkpath(root)); QVERIFY(write(root + "/target.txt", "other")); QVERIFY(write(path, "target.txt replacement\r\n")); }
        const auto expected = mode == "folder-changed" ? hash(path) : original;
        const auto error = FileComments::write(snapshot, name, value); QVERIFY2(!error.isEmpty(), qPrintable(mode)); QCOMPARE(hash(path), expected);
        QVERIFY(QDir(root).entryList({".7zip-comment-*"}, QDir::Files | QDir::Hidden).isEmpty());
    }
    void pauseCancelRetainsOriginal() {
        const auto root = temporary.filePath("pause"); QVERIFY(write(root + "/target.txt")); QVERIFY(write(root + "/descript.ion", "target.txt original\r\n")); const auto before = hash(root + "/descript.ion");
        const auto document = FileComments::read(root); auto control = std::make_shared<OperationControl>(); control->setPaused(true); QFutureWatcher<QString> watcher;
        watcher.setFuture(QtConcurrent::run([document, control] { return FileComments::write(document, "target.txt", "changed", control); }));
        QTest::qWait(80); QVERIFY(!watcher.isFinished()); control->cancel(); QTRY_VERIFY(watcher.isFinished()); QVERIFY(watcher.result().contains("cancelled")); QCOMPARE(hash(root + "/descript.ion"), before);
        const auto cancelled = FileComments::read(root, control); QVERIFY(cancelled.cancelled);
    }
    void inheritedGroupPreservesOriginal() {
        const auto root = temporary.filePath("inherited group"), path = root + "/descript.ion"; QVERIFY(write(root + "/target.txt")); QVERIFY(write(path, "target.txt original\r\n"));
        gid_t groups[64]{}; const auto count = ::getgroups(64, groups); QVERIFY(count > 0); gid_t other = ::getegid(); for (int n = 0; n < count; ++n) if (groups[n] != ::getegid()) { other = groups[n]; break; }
        QVERIFY2(other != ::getegid(), "Test requires a secondary group (available on the verified Mac)"); QVERIFY(::chown(QFile::encodeName(root).constData(), uid_t(-1), other) == 0); QVERIFY(::chmod(QFile::encodeName(root).constData(), 02775) == 0);
        auto original = FileComments::read(root); QVERIFY(original.error.isEmpty()); QCOMPARE(original.file.st_gid, ::getegid());
        auto error = FileComments::write(original, "target.txt", "changed"); QVERIFY2(error.isEmpty(), qPrintable(error)); const auto updated = FileComments::read(root); QCOMPARE(updated.file.st_gid, original.file.st_gid); QCOMPARE(updated.file.st_uid, original.file.st_uid);
    }
    void filesystemFocusedItemAndFlatView() {
        const auto root = temporary.filePath("gui filesystem"); QVERIFY(write(root + "/focus 日本語.txt", "focus")); QVERIFY(write(root + "/selected.txt", "selected")); QVERIFY(write(root + "/child/nested.txt", "nested"));
        QVERIFY(write(root + "/descript.ion", "selected.txt unrelated\r\n")); QVERIFY(write(root + "/child/descript.ion", "nested.txt child comment\r\n"));
        MainWindow window(executable); window.openPath(root); QTRY_VERIFY(!window.operationBusy()); auto list = tree(window); auto selected = item(window, "selected.txt"), focused = item(window, "focus 日本語.txt"); QVERIFY(selected && focused);
        list->clearSelection(); selected->setSelected(true); list->setCurrentItem(focused, 0, QItemSelectionModel::NoUpdate);
        auto action = window.findChild<QAction *>("commentAction"); QVERIFY(action && action->isEnabled()); QCOMPARE(action->shortcut(), QKeySequence("Ctrl+Z")); action->trigger();
        QTRY_VERIFY(testInput(&window, "commentDialog")); auto input = testInput(&window, "commentDialog"); QCOMPARE(input->textValue(), QString()); QVERIFY(window.operationBusy()); input->setTextValue("GUI コメント"); input->accept();
        QTRY_VERIFY(!window.operationBusy()); QCOMPARE(item(window, "focus 日本語.txt")->text(propertyColumn(list,OfficialSort::kpidComment)), QString("GUI コメント")); QCOMPARE(list->currentItem()->text(0), QString("focus 日本語.txt")); QCOMPARE(list->selectedItems().size(), 1); QCOMPARE(list->selectedItems()[0]->text(0), QString("selected.txt"));
        action->trigger(); QTRY_VERIFY(testInput(&window, "commentDialog")); input = testInput(&window, "commentDialog"); QCOMPARE(input->textValue(), QString("GUI コメント")); input->reject(); QTRY_VERIFY(!window.operationBusy());
        window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY(!window.operationBusy()); focused = item(window, "nested.txt"); QVERIFY(focused); list->setCurrentItem(focused); QVERIFY(!action->isEnabled()); QCOMPARE(focused->text(propertyColumn(list,OfficialSort::kpidComment)), QString());
        focused = item(window, "focus 日本語.txt"); list->setCurrentItem(focused); QVERIFY(action->isEnabled()); window.close();
    }
    void zipCommentRoundtrip_data() {
        QTest::addColumn<QString>("encryption"); QTest::newRow("plain") << QString(); QTest::newRow("AES") << QString("AES256"); QTest::newRow("ZipCrypto") << QString("ZipCrypto");
    }
    void zipCommentRoundtrip() {
        QFETCH(QString, encryption); const auto root = temporary.filePath("zip " + encryption); auto request = zip(root, encryption); auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(QFile::copy(request.archive, root + "/original.zip"));
        request.operation = ArchiveOperation::Comment; request.files = {"日本語 space.txt"}; request.comment = "ZIP コメント 日本語"; result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QProcess raw; raw.start("/usr/bin/python3", {QString(COMMENT_FIXTURE_SCRIPT), "verify-encrypted", root}); QVERIFY(raw.waitForFinished(10000)); QVERIFY2(raw.exitCode() == 0, raw.readAllStandardError().constData()); qInfo().noquote() << raw.readAllStandardOutput().trimmed();
        request.operation = ArchiveOperation::List; result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        bool target = false; for (const auto &entry : result.entries) { if (entry.path == "日本語 space.txt") { QCOMPARE(entry.properties.value("Comment"), request.comment); target = true; } else QVERIFY(entry.properties.value("Comment").isEmpty()); } QVERIFY(target);
        request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite"; result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(hash(root + "/input/日本語 space.txt"), hash(root + "/restored/日本語 space.txt")); QCOMPARE(hash(root + "/input/keep.txt"), hash(root + "/restored/keep.txt")); QVERIFY(QFileInfo(root + "/restored/empty").isDir());
        request.operation = ArchiveOperation::Comment; request.files = {"empty"}; request.comment = "folder comment"; result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        request.files = {"日本語 space.txt"}; request.comment.clear(); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
    }
    void zipCommentFailureAndCancel() {
        const auto root = temporary.filePath("zip failure"); auto request = zip(root, "AES256"); QVERIFY(execute(request).success);
        request.operation = ArchiveOperation::Comment; request.files = {"日本語 space.txt"}; request.comment = "new"; request.password = "wrong"; auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!result.passwordRequired);
        // The official Agent copies encrypted packed data for metadata-only
        // comments; a supplied wrong data password is irrelevant here.
        auto test = request; test.operation = ArchiveOperation::Test; QVERIFY(!execute(test).success); test.password = "comment-password"; QVERIFY(execute(test).success);
        const auto original = hash(request.archive);
        request.password = "comment-password"; request.comment = QString(65536, 'x'); QVERIFY(!execute(request).success); QCOMPARE(hash(request.archive), original);
        request.comment = "new"; request.files = {"not-real"}; QVERIFY(!execute(request).success); QCOMPARE(hash(request.archive), original);
        request.files = {"日本語 space.txt"}; SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request); backend.cancel(); QTRY_COMPARE(done.size(), 1); QVERIFY(qvariant_cast<ArchiveResult>(done[0][0]).cancelled); QCOMPARE(hash(request.archive), original);
        QVERIFY(execute(request).success);
    }
    void zipIndependentPayloadVerification_data() {
        QTest::addColumn<QString>("mode"); for (const auto &mode : {"create-bzip2", "create-lzma", "create-zip64"}) QTest::newRow(mode) << QString(mode);
    }
    void zipIndependentPayloadVerification() {
        QFETCH(QString, mode); const auto root = temporary.filePath(mode); const auto script = QString(COMMENT_FIXTURE_SCRIPT);
        QProcess generated; generated.start("/usr/bin/python3", {script, mode, root}); QVERIFY(generated.waitForFinished(30000)); QCOMPARE(generated.exitCode(), 0); QVERIFY(QFile::copy(root + "/fixture.zip", root + "/original.zip"));
        ArchiveRequest request; request.archive = root + "/fixture.zip"; auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        bool found = false; for (const auto &entry : result.entries) if (entry.path == "keep.txt") { QCOMPARE(entry.properties.value("Comment"), QString("two\nlines\nPath = bogus\n}")); found = true; } QVERIFY(found);
        request.operation = ArchiveOperation::Comment; request.format = "zip"; request.files = {"日本語 space.txt"}; request.comment = "updated コメント";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QProcess verified; verified.start("/usr/bin/python3", {script, "verify", root}); QVERIFY(verified.waitForFinished(30000)); QVERIFY2(verified.exitCode() == 0, verified.readAllStandardError().constData()); qInfo().noquote() << verified.readAllStandardOutput().trimmed();
    }
    void zipCryptoDescriptorUpdated() {
        const auto root = temporary.filePath("crypto descriptor"); QVERIFY(write(root + "/target.txt", QByteArray(1024 * 1024, 'a')));
        QProcess stream; stream.setWorkingDirectory(root); stream.setStandardOutputFile(root + "/fixture.zip");
        stream.start(executable, {"a", "-tzip", "-so", "-mem=ZipCrypto", "-p", "--", "unused.zip", "target.txt"}); QVERIFY(stream.waitForStarted()); stream.write("fixture-pass\nfixture-pass\n"); stream.closeWriteChannel(); QVERIFY(stream.waitForFinished(10000)); QCOMPARE(stream.exitCode(), 0);
        ArchiveRequest request; request.archive = root + "/fixture.zip"; request.password = "fixture-pass"; request.operation = ArchiveOperation::Test; QVERIFY(execute(request).success); const auto original = hash(request.archive);
        request.operation = ArchiveOperation::Comment; request.format = "zip"; request.files = {"target.txt"}; request.comment = "new comment";
        request.password = "wrong-data-password";
        const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(hash(request.archive) != original); QVERIFY(!result.passwordRequired);
        request.password = "fixture-pass";
        request.operation = ArchiveOperation::Test; QVERIFY(execute(request).success);
    }
    void zipVirtualFolderNotEditableAndMaximumLength() {
        const auto root = temporary.filePath("virtual maximum"); QProcess generated; generated.start("/usr/bin/python3", {QString(COMMENT_FIXTURE_SCRIPT), "create-bzip2", root}); QVERIFY(generated.waitForFinished(10000)); QCOMPARE(generated.exitCode(), 0);
        MainWindow window(executable); window.openPath(root + "/fixture.zip"); QTRY_VERIFY(!window.operationBusy()); auto folder = item(window, "virtual"); QVERIFY(folder); tree(window)->setCurrentItem(folder); QVERIFY(!window.findChild<QAction *>("commentAction")->isEnabled()); window.close();
        ArchiveRequest request; request.archive = root + "/fixture.zip"; request.operation = ArchiveOperation::Comment; request.format = "zip"; request.files = {"日本語 space.txt"}; request.comment = QString(65535, 'x');
        const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); for (const auto &entry : result.entries) if (entry.path == request.files[0]) QCOMPARE(entry.properties.value("Comment").size(), 65535);
    }
    void zipDuplicateNamesCanBeListed() {
        const auto root = temporary.filePath("duplicate ZIP"); QProcess generated; generated.start("/usr/bin/python3", {QString(COMMENT_FIXTURE_SCRIPT), "create-duplicate", root}); QVERIFY(generated.waitForFinished(10000)); QCOMPARE(generated.exitCode(), 0);
        ArchiveRequest request; request.archive = root + "/fixture.zip"; auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QStringList values; for (const auto &entry : result.entries) if (entry.path == "duplicate.txt") values << entry.properties.value("Comment"); QCOMPARE(values, QStringList({"first", "second"}));
        const auto original = hash(request.archive); request.operation = ArchiveOperation::Comment; request.format = "zip"; request.files = {"duplicate.txt"}; request.comment = "ambiguous"; QVERIFY(!execute(request).success); QCOMPARE(hash(request.archive), original);
    }
    void splitZipReadDoesNotRequireDirectHandler() {
        const auto root = temporary.filePath("split ZIP"); QVERIFY(write(root + "/target.txt", QByteArray(8000, 'a'))); QProcess created; created.setWorkingDirectory(root);
        created.start(executable, {"a", "-tzip", "-mx=0", "-v1k", "--", root + "/split.zip", "target.txt"}); QVERIFY(created.waitForFinished(10000)); QCOMPARE(created.exitCode(), 0);
        ArchiveRequest request; request.archive = root + "/split.zip.001"; auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.properties.value("Volumes").toInt() > 1); QCOMPARE(result.entries.size(), 1); QCOMPARE(result.entries[0].path, QString("target.txt"));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite"; result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(hash(root + "/target.txt"), hash(root + "/restored/target.txt"));
    }
    void zipGuiCommentAndColumnTransition() {
        const auto root = temporary.filePath("zip gui"); auto request = zip(root); QVERIFY(execute(request).success);
        MainWindow window(executable); window.openPath(request.archive); QTRY_VERIFY(!window.operationBusy()); auto list = tree(window); auto focused = item(window, "日本語 space.txt"), selected = item(window, "keep.txt"); QVERIFY(focused && selected); list->clearSelection(); selected->setSelected(true); list->setCurrentItem(focused, 0, QItemSelectionModel::NoUpdate);
        auto action = window.findChild<QAction *>("commentAction"); QVERIFY(action->isEnabled()); action->trigger(); QTRY_VERIFY(testInput(&window, "commentDialog")); auto input = testInput(&window, "commentDialog"); input->setTextValue("archive GUI 日本語"); input->accept(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        int commentColumn = -1; for (int column = 0; column < list->columnCount(); ++column) if (list->headerItem()->text(column) == "Comment") commentColumn = column; QVERIFY(commentColumn >= 0); QVERIFY(list->columnCount() > 10); QCOMPARE(item(window, "日本語 space.txt")->text(commentColumn), QString("archive GUI 日本語")); QCOMPARE(list->currentItem()->text(0), QString("日本語 space.txt")); QCOMPARE(list->selectedItems().size(), 1); QCOMPARE(list->selectedItems()[0]->text(0), QString("keep.txt"));
        window.openPath(root + "/input"); QTRY_VERIFY(!window.operationBusy()); QCOMPARE(list->columnCount(), filesystemColumns(false).size()); QCOMPARE(list->headerItem()->text(propertyColumn(list,OfficialSort::kpidExtension)), QString("Type")); window.close();
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("FileComments"); app.setQuitOnLastWindowClosed(false);
    CommentTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "file-comments.moc"
