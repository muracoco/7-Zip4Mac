// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "MainWindow.h"
#include "PortStyle.h"
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include "input-driver.h"
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include <QUrl>
#include <unistd.h>

static QString executable;
static QByteArray read(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
static bool write(const QString &path, const QByteArray &bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path); return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size(); }
static ArchiveResult result(const QSignalSpy &spy) { return spy.isEmpty() ? ArchiveResult{} : qvariant_cast<ArchiveResult>(spy.last().first()); }
static FileList *list(MainWindow &w) {
    for (auto view : w.findChildren<FileList *>("fileList")) {
        auto parent = view->parentWidget(); while (parent && !qobject_cast<MainWindow *>(parent)) parent = parent->parentWidget();
        if (parent == &w) return view;
    } return nullptr;
}
static bool select(MainWindow &w, QString name) {
    auto view = list(w); if (!view) return false;
    const auto matches = view->findItems(name, Qt::MatchExactly); if (matches.size() != 1) return false;
    view->clearSelection(); view->setCurrentItem(matches.first()); matches.first()->setSelected(true); return true;
}

class ArchiveTransferTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ArchiveResult execute(ArchiveRequest r) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(r);
        if (done.isEmpty() && !done.wait(60000)) return {}; return result(done);
    }
    ArchiveRequest seed(const QString &root, QString format, QString password = {}) {
        write(root + "/seed/dest/same.txt", "old"); write(root + "/seed/dest/keep.txt", "keep"); write(root + "/seed/unrelated.txt", "unrelated");
        ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = root + "/target." + format;
        r.format = format; r.workingDirectory = root + "/seed"; r.files = {"dest", "unrelated.txt"};
        r.level = 1; r.method = format == "zip" ? "Deflate" : format == "tar" ? "PAX" : "LZMA2";
        r.password = password; r.encryptNames = !password.isEmpty() && format == "7z";
        return r;
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("prefs"));
    }
    void copyDefaultsAndLogicalPrefix_data() {
        QTest::addColumn<QString>("format");
        for (const auto &f : {"7z", "zip", "tar", "wim"}) QTest::newRow(f) << QString(f);
    }
    void copyDefaultsAndLogicalPrefix() {
        QFETCH(QString, format); const auto root = temporary.filePath("copy-" + format);
        auto r = seed(root, format); QVERIFY(write(root + "/seed/dest/same.txt", QByteArray(4096, 'o'))); QVERIFY2(execute(r).success, qPrintable(r.archive));
        QVERIFY(write(root + "/sources/deep/same.txt", QByteArray(4096, 'n')));
        QFile source(root + "/sources/deep/same.txt"); QVERIFY(source.open(QIODevice::ReadWrite));
        QVERIFY(source.setFileTime(QDateTime(QDate(2000, 1, 1), QTime()), QFileDevice::FileModificationTime)); source.close();
        QVERIFY(write(root + "/elsewhere/日本語 space.txt", "Unicode payload 日本語\n"));
        QVERIFY(write(root + "/sources/folder/child/data.txt", "descendant")); QVERIFY(QDir().mkpath(root + "/sources/folder/empty"));
        r.useArchiveDefaults = true; r.archivePrefix = "dest/"; r.files = {root + "/sources/deep/same.txt", root + "/elsewhere/日本語 space.txt", root + "/sources/folder"};
        r.level = 0; r.method = "deliberately invalid saved method"; r.dictionary = "invalid"; r.parameters = "invalid saved parameters"; r.solid = "invalid";
        const auto added = execute(r); QVERIFY2(added.success, qPrintable(added.message + added.details)); QVERIFY(QFileInfo::exists(r.files.first()));
        r.operation = ArchiveOperation::List; r.files.clear(); const auto listed = execute(r); QVERIFY2(listed.success, qPrintable(listed.message + listed.details));
        bool same = false; for (const auto &entry : listed.entries) {
            QVERIFY(!entry.path.startsWith("dest/deep/"));
            if (entry.path == "dest/same.txt") { same = true; if (format == "7z") QVERIFY(entry.method.startsWith("LZMA2")); if (format == "zip") QVERIFY(entry.method.startsWith("Deflate")); }
        } QVERIFY(same);
        r.operation = ArchiveOperation::Extract; r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; const auto extracted = execute(r); QVERIFY2(extracted.success, qPrintable(extracted.message + extracted.details));
        QCOMPARE(read(root + "/out/dest/same.txt"), QByteArray(4096, 'n')); QCOMPARE(read(root + "/out/dest/keep.txt"), QByteArray("keep"));
        QCOMPARE(read(root + "/out/unrelated.txt"), QByteArray("unrelated")); QCOMPARE(read(root + "/out/dest/日本語 space.txt"), QByteArray("Unicode payload 日本語\n"));
        QCOMPARE(read(root + "/out/dest/folder/child/data.txt"), QByteArray("descendant")); QVERIFY(QFileInfo(root + "/out/dest/folder/empty").isDir());
    }
    void encryptionInherited_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z-header") << QString("7z"); QTest::newRow("zip-AES") << QString("zip"); }
    void encryptionInherited() {
        QFETCH(QString, format); const auto root = temporary.filePath("encrypted-" + format); auto r = seed(root, format, "owned fixture secret");
        QVERIFY(execute(r).success); const auto source = root + "/source/new.txt"; QVERIFY(write(source, "encrypted addition"));
        r.useArchiveDefaults = true; r.encryptNames = false; r.encryptionMethod = "ZipCrypto"; r.files = {source}; r.archivePrefix = "dest/";
        auto added = execute(r); QVERIFY2(added.success, qPrintable(added.message + added.details));
        r.operation = ArchiveOperation::List; r.files.clear(); const auto listed = execute(r); QVERIFY(listed.success);
        bool found = false; for (const auto &entry : listed.entries) if (entry.path == "dest/new.txt") { found = true; QVERIFY(entry.encrypted); if (format == "zip") QVERIFY(entry.method.contains("AES-256")); } QVERIFY(found);
        r.operation = ArchiveOperation::Test; const auto correct = execute(r); QVERIFY(correct.success); const auto before = read(r.archive);
        r.password = "wrong fixture"; const auto wrong = execute(r); QVERIFY(!wrong.success); QVERIFY(wrong.passwordRequired); QCOMPARE(read(r.archive), before);
        // ZIP can copy old encrypted streams without their data password.
        // Header-encrypted 7z must authenticate even for an Add operation.
        if (format == "7z") {
            r.operation = ArchiveOperation::Add; r.files = {source}; QVERIFY(!execute(r).success); QCOMPARE(read(r.archive), before); QVERIFY(QFileInfo::exists(source));
            r.operation = ArchiveOperation::List; r.files.clear(); r.password.clear(); QVERIFY(!execute(r).success);
        }
    }
    void moveVerifiesPrefixedRoots_data() { QTest::addColumn<QString>("format"); for (const auto &f : {"7z", "zip", "tar", "wim"}) QTest::newRow(f) << QString(f); }
    void moveVerifiesPrefixedRoots() {
        QFETCH(QString, format); const auto root = temporary.filePath("move-" + format); auto r = seed(root, format); QVERIFY(execute(r).success);
        QVERIFY(write(root + "/source/日本語 folder/deep/file.txt", QByteArray(1024 * 1024, 'm'))); QVERIFY(QDir().mkpath(root + "/source/日本語 folder/empty")); QVERIFY(write(root + "/another/empty.txt", {}));
        r.useArchiveDefaults = true; r.archivePrefix = "dest/"; r.deleteAfter = true; r.files = {root + "/source/日本語 folder", root + "/another/empty.txt"};
        const auto moved = execute(r); QVERIFY2(moved.success, qPrintable(moved.message + moved.details)); for (const auto &path : r.files) QVERIFY(!QFileInfo::exists(path));
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.deleteAfter = false; r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(r).success);
        QCOMPARE(read(root + "/out/dest/日本語 folder/deep/file.txt"), QByteArray(1024 * 1024, 'm')); QVERIFY(QFileInfo(root + "/out/dest/日本語 folder/empty").isDir()); QVERIFY(QFileInfo(root + "/out/dest/empty.txt").isFile());
    }
    void invalidPrefixAndDuplicateInputs() {
        const auto root = temporary.filePath("invalid"); auto r = seed(root, "7z"); QVERIFY(execute(r).success); const auto before = read(r.archive);
        QVERIFY(write(root + "/a/collision.txt", "one")); QVERIFY(write(root + "/b/collision.txt", "two"));
        r.useArchiveDefaults = true; r.deleteAfter = true; r.files = {root + "/a/collision.txt"};
        for (const auto &prefix : {"../", "/absolute/", "safe/../", "safe//", "safe/./", "C:/", "missing-slash"}) {
            r.archivePrefix = prefix; const auto failed = execute(r); QVERIFY(!failed.success); QCOMPARE(failed.exitCode, -1); QCOMPARE(read(r.archive), before); QVERIFY(QFileInfo::exists(r.files.first()));
        }
        r.archivePrefix = "dest/"; r.files << root + "/b/collision.txt"; const auto duplicated = execute(r); QVERIFY(!duplicated.success); QCOMPARE(duplicated.exitCode, 2); QVERIFY(duplicated.details.contains("Duplicate filename")); QCOMPARE(read(r.archive), before); for (const auto &path : r.files) QVERIFY(QFileInfo::exists(path));
        r.files.removeLast(); r.deleteAfter = false; QVERIFY(execute(r).success);
    }
    void streamCopyAndMove_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("move");
        for (const auto &f : {"gzip", "bzip2", "xz"}) {
            QTest::newRow((QString(f) + "-copy").toLatin1().constData()) << QString(f) << false;
            QTest::newRow((QString(f) + "-move").toLatin1().constData()) << QString(f) << true;
        }
    }
    void streamCopyAndMove() {
        QFETCH(QString, format); QFETCH(bool, move); const auto root = temporary.filePath(format + (move ? "-move" : "-copy"));
        QVERIFY(write(root + "/original.txt", "old stream")); ArchiveRequest r; r.operation = ArchiveOperation::Add;
        r.archive = root + "/stream." + format; r.format = format; r.workingDirectory = root; r.files = {"original.txt"}; QVERIFY(execute(r).success);
        r.operation = ArchiveOperation::List; r.files.clear(); const auto original = execute(r); QVERIFY(original.success); QCOMPARE(original.entries.size(), 1);
        // Single-stream handlers permit one logical item, not a second name.
        const auto source = root + "/different/" + original.entries.first().path;
        QVERIFY(write(source, QByteArray(1024 * 1024, 's')));
        r.operation = ArchiveOperation::Add; r.files = {source}; r.useArchiveDefaults = true; r.deleteAfter = move;
        const auto transferred = execute(r); QVERIFY2(transferred.success, qPrintable(transferred.message + transferred.details)); QCOMPARE(QFileInfo::exists(r.files.first()), !move);
        r.operation = ArchiveOperation::List; r.files.clear(); const auto listing = execute(r); QVERIFY(listing.success); QCOMPARE(listing.entries.size(), 1);
        r.operation = ArchiveOperation::Extract; r.deleteAfter = false; r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(r).success);
        QCOMPARE(read(r.outputDirectory + '/' + listing.entries.first().path), QByteArray(1024 * 1024, 's'));
        if (!move) {
            const auto before = read(r.archive); QVERIFY(write(root + "/second-logical-name.txt", "second"));
            r.operation = ArchiveOperation::Add; r.files = {root + "/second-logical-name.txt"}; r.deleteAfter = true;
            const auto rejected = execute(r); QVERIFY(!rejected.success); QCOMPARE(rejected.exitCode, 2); QVERIFY(rejected.details.contains("E_INVALIDARG")); QCOMPARE(read(r.archive), before); QVERIFY(QFileInfo::exists(r.files.first()));
        }
    }
    void moveFileLinkRetainsTarget() {
        const auto root = temporary.filePath("file-link"); auto r = seed(root, "7z"); QVERIFY(execute(r).success);
        const auto target = root + "/target-data.txt", link = root + "/source/link.txt"; const QByteArray payload(8192, 'l'); QVERIFY(write(target, payload)); QVERIFY(QDir().mkpath(root + "/source"));
        QCOMPARE(::symlink(QFile::encodeName(target).constData(), QFile::encodeName(link).constData()), 0);
        r.useArchiveDefaults = true; r.archivePrefix = "dest/"; r.deleteAfter = true; r.files = {link}; const auto transferred = execute(r); QVERIFY2(transferred.success, qPrintable(transferred.message + transferred.details));
        QVERIFY(!QFileInfo(link).isSymLink()); QCOMPARE(read(target), payload);
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.deleteAfter = false; r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(r).success); QCOMPARE(read(root + "/out/dest/link.txt"), payload); QVERIFY(!QFileInfo(root + "/out/dest/link.txt").isSymLink());
    }
    void moveDirectoryLinkRetainsTargets_data() { QTest::addColumn<QString>("format"); for (const auto &format : {"7z", "zip", "tar", "wim"}) QTest::newRow(format) << QString(format); }
    void moveDirectoryLinkRetainsTargets() {
        QFETCH(QString, format); const auto root = temporary.filePath("directory-link-" + format); auto r = seed(root, format); QVERIFY(execute(r).success);
        const QByteArray payload(1024 * 1024, 'd'); QVERIFY(write(root + "/outside/日本語 space.txt", payload)); QVERIFY(QDir().mkpath(root + "/outside/empty"));
        QVERIFY(write(root + "/separate/nested.txt", "nested target")); QVERIFY(QDir().mkpath(root + "/source"));
        const auto link = root + "/source/linked-directory", nested = root + "/outside/nested-directory";
        QCOMPARE(::symlink(QFile::encodeName(root + "/separate").constData(), QFile::encodeName(nested).constData()), 0);
        QCOMPARE(::symlink(QFile::encodeName(root + "/outside").constData(), QFile::encodeName(link).constData()), 0);
        r.useArchiveDefaults = true; r.archivePrefix = "dest/"; r.deleteAfter = true; r.files = {link};
        const auto moved = execute(r); QVERIFY2(moved.success, qPrintable(moved.message + moved.details)); QVERIFY(!QFileInfo(link).isSymLink());
        QVERIFY(QFileInfo(nested).isSymLink()); QCOMPARE(read(root + "/outside/日本語 space.txt"), payload); QCOMPARE(read(root + "/separate/nested.txt"), QByteArray("nested target")); QVERIFY(QFileInfo(root + "/outside/empty").isDir());
        r.operation = ArchiveOperation::Extract; r.deleteAfter = false; r.files.clear(); r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite";
        const auto restored = execute(r); QVERIFY2(restored.success, qPrintable(restored.message + restored.details));
        QCOMPARE(read(root + "/out/dest/linked-directory/日本語 space.txt"), payload); QCOMPARE(read(root + "/out/dest/linked-directory/nested-directory/nested.txt"), QByteArray("nested target"));
        QVERIFY(QFileInfo(root + "/out/dest/linked-directory/empty").isDir()); QVERIFY(!QFileInfo(root + "/out/dest/linked-directory").isSymLink());
    }
    void pauseCancelExistingArchive() {
        const auto root = temporary.filePath("cancel"); auto r = seed(root, "7z"); QVERIFY(execute(r).success); const auto before = read(r.archive);
        QFile random("/dev/urandom"); QVERIFY(random.open(QIODevice::ReadOnly)); QVERIFY(write(root + "/large.bin", random.read(32 * 1024 * 1024)));
        r.useArchiveDefaults = true; r.deleteAfter = true; r.archivePrefix = "dest/"; r.files = {root + "/large.bin"};
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(r);
        QTRY_VERIFY_WITH_TIMEOUT(backend.canPause(), 10000); QVERIFY(backend.setPaused(true)); int beats = 0; QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, [&] { ++beats; }); heartbeat.start(); QTest::qWait(250); QVERIFY(beats >= 10); QVERIFY(done.isEmpty()); backend.cancel();
        if (done.isEmpty()) QVERIFY(done.wait(10000)); QVERIFY(result(done).cancelled); QCOMPARE(read(r.archive), before); QVERIFY(QFileInfo::exists(r.files.first()));
        r.operation = ArchiveOperation::Test; r.files.clear(); QVERIFY(execute(r).success);
    }
    void guiCopyMoveAndDestinationRefresh_data() { QTest::addColumn<bool>("move"); QTest::newRow("Copy-context") << false; QTest::newRow("Move-action") << true; }
    void guiCopyMoveAndDestinationRefresh() {
        QFETCH(bool, move); const auto root = temporary.filePath(move ? "gui-move" : "gui-copy"); auto r = seed(root, "7z"); QVERIFY(execute(r).success); QVERIFY(write(root + "/source/日本語 selected.txt", "GUI transfer"));
        QSettings s; s.clear(); s.setValue("View/TwoPanels", true); s.setValue("View/LastPath", root + "/source"); s.setValue("View/Panel2Path", root); s.setValue("View/AutoRefresh", false);
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other);
        other->openPath(r.archive + "/dest"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(window.currentDirectory(), root + "/source"); QVERIFY(select(window, "日本語 selected.txt")); list(window)->setFocus();
        auto destination = other->findChild<QLineEdit *>("currentPath"); QVERIFY(destination);
        const auto expected = destination->text(); bool dialogSeen = false, contextClicked = false, addSeen = false; QString warning;
        QTimer responder; responder.setInterval(5); connect(&responder, &QTimer::timeout, &window, [&] {
            if (!move && !contextClicked) for (auto menu : window.findChildren<QMenu *>()) if (menu->objectName() == "fileContextMenu" && menu->isVisible()) {
                for (auto action : menu->actions()) if (action->objectName() == "copyAction") {
                    contextClicked = true; const auto point = menu->actionGeometry(action).center();
                    // Leave the responder timer before a click opens the nested
                    // Copy dialog; the same timer must remain able to accept it.
                    QTimer::singleShot(0, menu, [menu, point] { QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, point); }); break;
                }
            }
            for (auto d : testInputs(&window)) if (d->isVisible()) { dialogSeen = true; if (d->textValue() != expected) warning = "Wrong default target: " + d->textValue(); d->accept(); }
            for (auto d : window.findChildren<AddDialog *>()) if (d->isVisible()) { addSeen = true; d->reject(); }
            for (auto d : window.findChildren<QMessageBox *>()) if (d->isVisible()) { warning += d->text(); d->accept(); }
        }); responder.start();
        if (move) window.findChild<QAction *>("moveAction", Qt::FindDirectChildrenOnly)->trigger();
        else { auto view = list(window); const auto point = view->visualItemRect(view->currentItem()).center(); QContextMenuEvent event(QContextMenuEvent::Mouse, point, view->viewport()->mapToGlobal(point)); QApplication::sendEvent(view->viewport(), &event); }
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); responder.stop(); QVERIFY(dialogSeen); if (!move) QVERIFY(contextClicked); QVERIFY(!addSeen); QVERIFY2(warning.isEmpty(), qPrintable(warning)); QCOMPARE(destination->text(), expected); QVERIFY(select(*other, "日本語 selected.txt"));
        QCOMPARE(QFileInfo::exists(root + "/source/日本語 selected.txt"), !move); if (move) QVERIFY(list(window)->findItems("日本語 selected.txt", Qt::MatchExactly).isEmpty());
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(r).success); QCOMPARE(read(root + "/out/dest/日本語 selected.txt"), QByteArray("GUI transfer")); window.close();
    }
    void guiDropIntoFolderFromDifferentParents() {
        const auto root = temporary.filePath("gui-drop"); auto r = seed(root, "zip"); QVERIFY(execute(r).success); QVERIFY(write(root + "/a/first.txt", "first")); QVERIFY(write(root + "/b/日本語 second.txt", "second"));
        QSettings s; s.clear(); s.setValue("View/LastPath", root); MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(r.archive + "/dest"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        bool addSeen = false; QString warning; QTimer responder; responder.setInterval(5); connect(&responder, &QTimer::timeout, &window, [&] { for (auto d : window.findChildren<AddDialog *>()) if (d->isVisible()) { addSeen = true; d->reject(); } for (auto d : window.findChildren<QMessageBox *>()) if (d->isVisible()) { if (d->objectName() == "archiveDropConfirmation") d->done(QMessageBox::Yes); else { warning += d->text(); d->accept(); } } }); responder.start();
        QMimeData mime; mime.setUrls({QUrl::fromLocalFile(root + "/a/first.txt"), QUrl::fromLocalFile(root + "/b/日本語 second.txt")}); auto view = list(window);
        QDragEnterEvent enter(QPoint(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(view->viewport(), &enter); QVERIFY(enter.isAccepted());
        QDropEvent drop(QPointF(20, 20), Qt::CopyAction, &mime, Qt::LeftButton, Qt::NoModifier); QApplication::sendEvent(view->viewport(), &drop); QVERIFY(drop.isAccepted());
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); responder.stop(); QVERIFY(!addSeen); QVERIFY2(warning.isEmpty(), qPrintable(warning)); QVERIFY(select(window, "first.txt")); QVERIFY(select(window, "日本語 second.txt"));
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(r).success); QCOMPARE(read(root + "/out/dest/first.txt"), QByteArray("first")); QCOMPARE(read(root + "/out/dest/日本語 second.txt"), QByteArray("second")); window.close();
    }
    void guiDirectoryLinkMoveRefreshesBothPanels() {
        const auto root = temporary.filePath("gui-directory-link-move"); auto r = seed(root, "7z"); QVERIFY(execute(r).success);
        QVERIFY(write(root + "/target-directory/inside.txt", "followed directory data")); QVERIFY(QDir().mkpath(root + "/source"));
        const auto link = root + "/source/linked-directory"; QCOMPARE(::symlink(QFile::encodeName(root + "/target-directory").constData(), QFile::encodeName(link).constData()), 0);
        QSettings s; s.clear(); s.setValue("View/TwoPanels", true); s.setValue("View/LastPath", root + "/source"); s.setValue("View/Panel2Path", root); s.setValue("View/AutoRefresh", false);
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other);
        other->openPath(r.archive + "/dest"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(select(window, "linked-directory")); list(window)->setFocus();
        auto backend = other->findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished); QTimer responder; responder.setInterval(5);
        connect(&responder, &QTimer::timeout, &window, [&] { for (auto d : testInputs(&window)) if (d->isVisible()) d->accept(); }); responder.start();
        window.findChild<QAction *>("moveAction", Qt::FindDirectChildrenOnly)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); responder.stop();
        bool moved = false; for (const auto &event : done) { const auto completed = qvariant_cast<ArchiveResult>(event.first()); if (completed.operation == ArchiveOperation::Add) { QVERIFY2(completed.success, qPrintable(completed.message + completed.details)); moved = true; } }
        QVERIFY(moved); QVERIFY(!QFileInfo(link).isSymLink()); QCOMPARE(read(root + "/target-directory/inside.txt"), QByteArray("followed directory data")); QVERIFY(select(*other, "linked-directory")); QVERIFY(!select(window, "linked-directory"));
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(r).success); QCOMPARE(read(root + "/out/dest/linked-directory/inside.txt"), QByteArray("followed directory data")); window.close();
    }
    void guiVerificationFailureRefreshesCommittedArchive() {
        const auto root = temporary.filePath("gui-changed-source"); auto r = seed(root, "7z"); QVERIFY(execute(r).success);
        const auto source = root + "/source/changing.txt"; QVERIFY(write(source, "original verified payload"));
        QSettings s; s.clear(); s.setValue("View/TwoPanels", true); s.setValue("View/LastPath", root + "/source"); s.setValue("View/Panel2Path", root); s.setValue("View/AutoRefresh", false);
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other);
        other->openPath(r.archive + "/dest"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(select(window, "changing.txt")); list(window)->setFocus();
        auto backend = other->findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished); bool changed = false;
        connect(backend, &ArchiveBackend::output, &window, [&](const QString &text) { if (!changed && text.contains("Everything is Ok")) { changed = true; QVERIFY(write(source, "modified source payload")); } });
        QTimer responder; responder.setInterval(5); connect(&responder, &QTimer::timeout, &window, [&] { for (auto d : testInputs(&window)) if (d->isVisible()) d->accept(); for (auto d : window.findChildren<QMessageBox *>()) if (d->isVisible()) d->accept(); }); responder.start();
        window.findChild<QAction *>("moveAction", Qt::FindDirectChildrenOnly)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); responder.stop(); QVERIFY(changed);
        bool retained = false; for (const auto &event : done) { const auto completed = qvariant_cast<ArchiveResult>(event.first()); if (completed.operation == ArchiveOperation::Add) { QVERIFY(!completed.success); QVERIFY(completed.message.contains("sources were retained")); retained = true; } }
        QVERIFY(retained); QCOMPARE(read(source), QByteArray("modified source payload")); QVERIFY(select(window, "changing.txt")); QVERIFY(select(*other, "changing.txt"));
        r.operation = ArchiveOperation::Extract; r.files.clear(); r.outputDirectory = root + "/out"; r.overwriteMode = "overwrite"; QVERIFY(execute(r).success); QCOMPARE(read(root + "/out/dest/changing.txt"), QByteArray("original verified payload")); window.close();
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    const auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("ArchiveTransfer"); app.setQuitOnLastWindowClosed(false);
    ArchiveTransferTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "archive-transfer.moc"
