// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "FileInstall.h"
#include "MainWindow.h"
#include "OverwriteDialog.h"
#include "PortStyle.h"
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <sys/xattr.h>
#include <unistd.h>

static QString executable;
static bool write(QString path, QByteArray bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static bool finished(QSignalSpy &spy, int timeout = 10000) { return !spy.isEmpty() || spy.wait(timeout); }
class OverwriteTests : public QObject {
    Q_OBJECT
    QTemporaryDir preferences;
private slots:
    void initTestCase() {
        QVERIFY(preferences.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, preferences.path());
        QSettings().setValue("View/LastPath", preferences.path()); QSettings().setValue("View/AutoRefresh", false);
    }
    void choices_data() {
        QTest::addColumn<int>("answer"); QTest::addColumn<int>("prompts"); QTest::addColumn<QByteArray>("first"); QTest::addColumn<QByteArray>("second");
        QTest::newRow("individual") << int(OverwriteAnswer::No) << 2 << QByteArray("old-a") << QByteArray("new-b");
        QTest::newRow("yes-all") << int(OverwriteAnswer::YesToAll) << 1 << QByteArray("new-a") << QByteArray("new-b");
        QTest::newRow("no-all") << int(OverwriteAnswer::NoToAll) << 1 << QByteArray("old-a") << QByteArray("old-b");
        QTest::newRow("auto-rename") << int(OverwriteAnswer::AutoRename) << 1 << QByteArray("old-a") << QByteArray("old-b");
        QTest::newRow("cancel") << int(OverwriteAnswer::Cancel) << 1 << QByteArray("old-a") << QByteArray("old-b");
    }
    void choices() {
        QFETCH(int, answer); QFETCH(int, prompts); QFETCH(QByteArray, first); QFETCH(QByteArray, second);
        for (const QString format : {QString(), QString("move"), QString("7z"), QString("zip")}) {
            QTemporaryDir temp; const auto source = temp.filePath("source"), output = temp.filePath("output");
            QVERIFY(write(source + "/a.tar.gz", "new-a")); QVERIFY(write(source + "/b space.txt", "new-b")); QVERIFY(write(output + "/a.tar.gz", "old-a")); QVERIFY(write(output + "/b space.txt", "old-b"));
            int count = 0; FileOperations files; SevenZipProcessBackend backend(executable);
            auto reply = [&](OverwriteConflict conflict, auto &owner) {
                ++count; QVERIFY(conflict.existing.sizeDefined); QVERIFY(conflict.incoming.sizeDefined); QVERIFY(conflict.existing.modified.isValid()); QVERIFY(!owner.setPaused(true));
                QVERIFY(owner.resolveOverwrite(conflict.id, count == 1 ? OverwriteAnswer(answer) : OverwriteAnswer::Yes));
                QVERIFY(!owner.resolveOverwrite(conflict.id, OverwriteAnswer::Yes));
            };
            connect(&files, &FileOperations::overwriteRequested, &files, [&](OverwriteConflict conflict) { reply(conflict, files); });
            connect(&backend, &ArchiveBackend::overwriteRequested, &backend, [&](OverwriteConflict conflict) { reply(conflict, backend); });
            if (format.isEmpty() || format == "move") {
                QSignalSpy done(&files, &FileOperations::finished); files.transfer({source + "/a.tar.gz", source + "/b space.txt"}, output, format == "move"); QVERIFY(finished(done));
                QCOMPARE(done.last().first().toString().isEmpty(), answer != int(OverwriteAnswer::Cancel));
                if (format == "move") { QCOMPARE(QFileInfo::exists(source + "/a.tar.gz"), answer == int(OverwriteAnswer::No) || answer == int(OverwriteAnswer::NoToAll) || answer == int(OverwriteAnswer::Cancel)); QCOMPARE(QFileInfo::exists(source + "/b space.txt"), answer == int(OverwriteAnswer::NoToAll) || answer == int(OverwriteAnswer::Cancel)); }
            } else {
                ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = temp.filePath("archive." + format); r.format = format; r.method = format == "zip" ? "Deflate" : "LZMA2"; r.level = 1; r.workingDirectory = source; r.files = {"a.tar.gz", "b space.txt"};
                QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(r); QVERIFY(finished(done)); QVERIFY(qvariant_cast<ArchiveResult>(done.last().first()).success); done.clear();
                r.operation = ArchiveOperation::Extract; r.outputDirectory = output; r.files.clear(); r.overwriteMode = "ask"; backend.start(r); QVERIFY(finished(done)); const auto result = qvariant_cast<ArchiveResult>(done.last().first());
                QCOMPARE(result.success, answer != int(OverwriteAnswer::Cancel)); QCOMPARE(result.cancelled, answer == int(OverwriteAnswer::Cancel));
            }
            QCOMPARE(count, prompts); QCOMPARE(read(output + "/a.tar.gz"), first); QCOMPARE(read(output + "/b space.txt"), second);
            if (answer == int(OverwriteAnswer::AutoRename)) { QCOMPARE(read(output + "/a.tar_1.gz"), QByteArray("new-a")); QCOMPARE(read(output + "/b space_1.txt"), QByteArray("new-b")); }
        }
    }
    void folderMergeAndSkippedMove() {
        QTemporaryDir temp; const auto source = temp.filePath("source/folder"), output = temp.filePath("destination");
        QVERIFY(write(source + "/a.txt", "new-a")); QVERIFY(write(source + "/b.txt", "new-b")); QVERIFY(QDir().mkpath(source + "/empty")); QVERIFY(write(output + "/folder/a.txt", "old-a"));
        FileOperations files; int prompts = 0; connect(&files, &FileOperations::overwriteRequested, &files, [&](OverwriteConflict conflict) { ++prompts; QVERIFY(files.resolveOverwrite(conflict.id, OverwriteAnswer::No)); });
        QSignalSpy done(&files, &FileOperations::finished); files.transfer({source}, output, true); QVERIFY(finished(done)); QVERIFY(!done.last().first().toString().isEmpty()); QCOMPARE(prompts, 1);
        QCOMPARE(read(source + "/a.txt"), QByteArray("new-a")); QCOMPARE(read(output + "/folder/a.txt"), QByteArray("old-a")); QCOMPARE(read(output + "/folder/b.txt"), QByteArray("new-b")); QVERIFY(!QFileInfo::exists(source + "/b.txt")); QVERIFY(QFileInfo(output + "/folder/empty").isDir());
        done.clear(); QVERIFY(write(temp.filePath("single.txt"), "rename destination")); files.transfer({temp.filePath("single.txt")}, output + "/new name.txt", false); QVERIFY(finished(done)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QCOMPARE(read(output + "/new name.txt"), QByteArray("rename destination"));
    }
    void renameMoveIdentityAndMetadata() {
        QTemporaryDir temp; const auto source = temp.filePath("日本語 source"), output = temp.filePath("out"); QVERIFY(QDir().mkpath(output));
        QVERIFY(write(source, QByteArray(4 * 1024 * 1024, 'x'))); QVERIFY(::setxattr(QFile::encodeName(source).constData(), "com.7zip-port.test", "value", 5, 0, 0) == 0); QVERIFY(QFile::setPermissions(source, QFile::ReadOwner));
        const auto before = FileInstall::capture(source, true); FileOperations files; QSignalSpy done(&files, &FileOperations::finished); files.transfer({source}, output, true); QVERIFY(finished(done)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString()));
        const auto moved = FileInstall::capture(output + "/日本語 source", true); QVERIFY(!QFileInfo::exists(source)); QCOMPARE(moved.stamp.st_ino, before.stamp.st_ino); QCOMPARE(moved.stamp.st_mode, before.stamp.st_mode); QCOMPARE(moved.stamp.st_birthtimespec.tv_sec, before.stamp.st_birthtimespec.tv_sec); QCOMPARE(moved.stamp.st_birthtimespec.tv_nsec, before.stamp.st_birthtimespec.tv_nsec);
        char value[10]{}; QCOMPARE(::getxattr(QFile::encodeName(moved.path).constData(), "com.7zip-port.test", value, sizeof(value), 0, 0), ssize_t(5)); QCOMPARE(QByteArray(value, 5), QByteArray("value"));
        const auto folder = temp.filePath("folder"); QVERIFY(write(folder + "/child", "child")); QVERIFY(QDir().mkpath(folder + "/empty")); QVERIFY(::symlink("child", QFile::encodeName(folder + "/relative link").constData()) == 0); const auto oldFolder = FileInstall::capture(folder, true), child = FileInstall::capture(folder + "/child", true); done.clear(); files.transfer({folder}, output, true); QVERIFY(finished(done)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString()));
        QCOMPARE(FileInstall::capture(output + "/folder", true).stamp.st_ino, oldFolder.stamp.st_ino); QCOMPARE(FileInstall::capture(output + "/folder/child", true).stamp.st_ino, child.stamp.st_ino); QCOMPARE(FileInstall::capture(output + "/folder/relative link", true).linkTarget, QByteArray("child")); QVERIFY(QFileInfo(output + "/folder/empty").isDir());
    }
    void renameMoveCollisionAndRollback() {
        for (const QString scenario : {QString("changed"), QString("occupied"), QString("cancel"), QString("source-reused")}) {
            QTemporaryDir temp; const auto source = temp.filePath("source"), target = temp.filePath("target"); QVERIFY(write(source, "incoming")); if (scenario != "occupied") QVERIFY(write(target, "old"));
            const auto original = FileInstall::capture(source, true); auto control = std::make_shared<OperationControl>();
            const auto result = FileInstall::tryMove(original, FileInstall::capture(target, true), control, false, [&](FileInstallPhase phase, const QString &) {
                if (phase == FileInstallPhase::Validated && scenario == "changed") QVERIFY(write(target, "newer destination"));
                if (phase == FileInstallPhase::Validated && scenario == "occupied") QVERIFY(write(target, "concurrent output"));
                if (phase == FileInstallPhase::Prepared && scenario == "cancel") control->cancel();
                if (phase == FileInstallPhase::Prepared && scenario == "source-reused") QVERIFY(write(source, "new source"));
            });
            QVERIFY(result); QVERIFY(!result->error.isEmpty()); QVERIFY(!result->installed);
            if (scenario == "source-reused") { QCOMPARE(read(source), QByteArray("new source")); QVERIFY(!result->recovery.isEmpty()); QCOMPARE(read(result->recovery + "/replacement"), QByteArray("incoming")); }
            else { QCOMPARE(read(source), QByteArray("incoming")); QCOMPARE(FileInstall::capture(source, true).stamp.st_ino, original.stamp.st_ino); QVERIFY(QDir(temp.path()).entryList({".7zip-file-install-*"}, QDir::Dirs | QDir::Hidden).isEmpty()); }
            QCOMPARE(read(target), scenario == "changed" ? QByteArray("newer destination") : scenario == "occupied" ? QByteArray("concurrent output") : QByteArray("old"));
        }
    }
    void renameMoveBackupAndLinks() {
        QTemporaryDir temp; const auto source = temp.filePath("source"), target = temp.filePath("target.txt"); QVERIFY(write(source, "incoming")); QVERIFY(write(target, "old")); const auto original = FileInstall::capture(source, true), old = FileInstall::capture(target, true);
        auto control = std::make_shared<OperationControl>(); OverwriteBroker broker; QString mode = "renameExisting"; const auto moved = FileInstall::transfer(original, old, mode, broker, control, {}, true, {}, true); QVERIFY2(moved.error.isEmpty(), qPrintable(moved.error)); QVERIFY(moved.sourceMoved); QCOMPARE(FileInstall::capture(target, true).stamp.st_ino, original.stamp.st_ino); QCOMPARE(FileInstall::capture(temp.filePath("target_1.txt"), true).stamp.st_ino, old.stamp.st_ino); QCOMPARE(read(temp.filePath("target_1.txt")), QByteArray("old"));
        const auto link = temp.filePath("link"); QVERIFY(::symlink("missing", QFile::encodeName(link).constData()) == 0); const auto oldLink = FileInstall::capture(link, true); mode = "overwrite"; const auto linked = FileInstall::transfer(oldLink, FileInstall::capture(target, true), mode, broker, control, {}, true, {}, true); QVERIFY2(linked.error.isEmpty(), qPrintable(linked.error)); QVERIFY(linked.sourceMoved); QCOMPARE(FileInstall::capture(target, true).stamp.st_ino, oldLink.stamp.st_ino); QCOMPARE(FileInstall::capture(target, true).linkTarget, QByteArray("missing"));
    }
    void moveOntoSelfAndAcrossVolumes() {
        QTemporaryDir temp; const auto source = temp.filePath("source"); QVERIFY(write(source, "same-volume guard"));
        FileOperations files; int prompts = 0; connect(&files, &FileOperations::overwriteRequested, &files, [&](OverwriteConflict conflict) { ++prompts; QVERIFY(files.resolveOverwrite(conflict.id, OverwriteAnswer::Yes)); }); QSignalSpy done(&files, &FileOperations::finished); files.transfer({source}, source, true); QVERIFY(finished(done, 60000)); QVERIFY(done.last().first().toString().contains("itself")); QCOMPARE(prompts, 0); QCOMPARE(read(source), QByteArray("same-volume guard"));
        // Network metadata/ACL operations can outlast the local-disk budget.
        const auto volume = qEnvironmentVariable("PORT_TEST_SECOND_VOLUME"); if (volume.isEmpty()) QSKIP("Set PORT_TEST_SECOND_VOLUME to a writable folder on another volume for cross-device verification.");
        QTemporaryDir other(volume + "/7zip-cross-volume-test-XXXXXX"); QVERIFY(other.isValid()); const auto original = FileInstall::capture(source, true), target = FileInstall::capture(other.filePath("moved"), true); if (original.stamp.st_dev == target.parent->stamp.st_dev) QSKIP("Requested second volume is on the source filesystem.");
        QVERIFY(!FileInstall::tryMove(original, target, std::make_shared<OperationControl>())); done.clear(); files.transfer({source}, target.path, true); QVERIFY(finished(done, 60000)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QVERIFY(!QFileInfo::exists(source)); QCOMPARE(read(target.path), QByteArray("same-volume guard"));
        QVERIFY(write(source, "replacement")); done.clear(); files.transfer({source}, target.path, true); QVERIFY(finished(done, 60000)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QCOMPARE(prompts, 1); QVERIFY(!QFileInfo::exists(source)); QCOMPARE(read(target.path), QByteArray("replacement"));
        QVERIFY(write(source, "backup replacement")); QString mode = "renameExisting"; OverwriteBroker broker; const auto backed = FileInstall::transfer(FileInstall::capture(source, true), FileInstall::capture(target.path, true), mode, broker, std::make_shared<OperationControl>()); QVERIFY2(backed.error.isEmpty(), qPrintable(backed.error)); QCOMPARE(read(target.path), QByteArray("backup replacement")); QCOMPARE(read(other.filePath("moved_1")), QByteArray("replacement"));
        done.clear(); files.transfer({target.path}, temp.filePath("back"), true); QVERIFY(finished(done, 60000)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QVERIFY(!QFileInfo::exists(target.path)); QCOMPARE(read(temp.filePath("back")), QByteArray("backup replacement"));
        const auto folder = temp.filePath("folder"); QVERIFY(write(folder + "/日本語 space.txt", "cross-volume child")); QVERIFY(QDir().mkpath(folder + "/empty")); done.clear(); files.transfer({folder}, other.path() + '/', true); QVERIFY(finished(done, 60000)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QVERIFY(!QFileInfo::exists(folder)); QCOMPARE(read(other.filePath("folder/日本語 space.txt")), QByteArray("cross-volume child")); QVERIFY(QFileInfo(other.filePath("folder/empty")).isDir());
        done.clear(); files.transfer({other.filePath("folder")}, temp.path() + '/', true); QVERIFY(finished(done, 60000)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QVERIFY(!QFileInfo::exists(other.filePath("folder"))); QCOMPARE(read(temp.filePath("folder/日本語 space.txt")), QByteArray("cross-volume child")); QVERIFY(QFileInfo(temp.filePath("folder/empty")).isDir());
    }
    void changedDestinationAndRecovery() {
        QTemporaryDir temp; const auto source = temp.filePath("source"), target = temp.filePath("target"); QVERIFY(write(source, "incoming")); QVERIFY(write(target, "old"));
        FileOperations files; connect(&files, &FileOperations::overwriteRequested, &files, [&](OverwriteConflict conflict) { QVERIFY(write(target, "changed while asking")); QVERIFY(files.resolveOverwrite(conflict.id, OverwriteAnswer::Yes)); });
        QSignalSpy done(&files, &FileOperations::finished); files.transfer({source}, target, true); QVERIFY(finished(done)); QVERIFY(done.last().first().toString().contains("changed")); QCOMPARE(read(target), QByteArray("changed while asking")); QCOMPARE(read(source), QByteArray("incoming"));
        auto control = std::make_shared<OperationControl>(); const auto result = FileInstall::install(FileInstall::capture(source, true), FileInstall::capture(target, true), control, false, [&](FileInstallPhase phase, const QString &) { if (phase == FileInstallPhase::Validated) QVERIFY(write(target, "newer destination")); });
        QVERIFY(!result.error.isEmpty()); QVERIFY(!result.installed); QCOMPARE(read(target), QByteArray("newer destination")); QCOMPARE(read(source), QByteArray("incoming")); QVERIFY(QDir(temp.path()).entryList({".7zip-file-install-*"}, QDir::Dirs | QDir::Hidden).isEmpty());
    }
    void metadataLinksAndNames() {
        QTemporaryDir temp; const auto source = temp.filePath("日本語 space.txt"), output = temp.filePath("out"); QVERIFY(write(source, "payload")); QVERIFY(QDir().mkpath(output));
        QFile sourceFile(source); QVERIFY(sourceFile.open(QIODevice::ReadOnly)); QVERIFY(sourceFile.setFileTime(QDateTime::fromSecsSinceEpoch(1700000000), QFileDevice::FileModificationTime)); sourceFile.close();
        QVERIFY(::setxattr(QFile::encodeName(source).constData(), "com.7zip-port.test", "value", 5, 0, 0) == 0);
        QVERIFY(QFile::setPermissions(source, QFile::ReadOwner));
        FileOperations files; QSignalSpy done(&files, &FileOperations::finished); files.transfer({source}, output + "/copied", false); QVERIFY(finished(done)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QCOMPARE(read(output + "/copied"), QByteArray("payload")); QCOMPARE(QFileInfo(output + "/copied").lastModified(), QFileInfo(source).lastModified());
        char value[10]{}; QCOMPARE(::getxattr(QFile::encodeName(output + "/copied").constData(), "com.7zip-port.test", value, sizeof(value), 0, 0), ssize_t(5)); QCOMPARE(QByteArray(value, 5), QByteArray("value"));
        const auto link = temp.filePath("relative link"); QVERIFY(::symlink("missing", QFile::encodeName(link).constData()) == 0); done.clear(); files.transfer({link}, output, false); QVERIFY(finished(done)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); char bytes[100]{}; QCOMPARE(::readlink(QFile::encodeName(output + "/relative link").constData(), bytes, sizeof(bytes)), ssize_t(7)); QCOMPARE(QByteArray(bytes, 7), QByteArray("missing"));
        QVERIFY(write(output + "/.profile", "existing")); QVERIFY(write(output + "/a.tar.gz", "existing")); QVERIFY(::symlink("missing", QFile::encodeName(output + "/a.tar_1.gz").constData()) == 0); const auto canonical = QFileInfo(output).canonicalFilePath(); QCOMPARE(FileInstall::autoName(FileInstall::capture(output + "/.profile", true)), canonical + "/.profile_1"); QCOMPARE(FileInstall::autoName(FileInstall::capture(output + "/a.tar.gz", true)), canonical + "/a.tar_2.gz");
        QFile::setPermissions(source, QFile::ReadOwner | QFile::WriteOwner);
    }
    void extractionRenameExisting_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void extractionRenameExisting() {
        QFETCH(QString, format); QTemporaryDir temp; QVERIFY(write(temp.filePath("source/a.tar.gz"), "new")); QVERIFY(write(temp.filePath("output/a.tar.gz"), "old")); SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished);
        ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = temp.filePath("archive." + format); r.format = format; r.method = format == "zip" ? "Deflate" : "LZMA2"; r.level = 1; r.workingDirectory = temp.filePath("source"); r.files = {"a.tar.gz"}; backend.start(r); QVERIFY(finished(done)); QVERIFY(qvariant_cast<ArchiveResult>(done.last().first()).success); done.clear();
        r.operation = ArchiveOperation::Extract; r.outputDirectory = temp.filePath("output"); r.files.clear(); r.overwriteMode = "renameExisting"; backend.start(r); QVERIFY(finished(done)); QVERIFY2(qvariant_cast<ArchiveResult>(done.last().first()).success, qPrintable(qvariant_cast<ArchiveResult>(done.last().first()).message)); QCOMPARE(read(temp.filePath("output/a.tar.gz")), QByteArray("new")); QCOMPARE(read(temp.filePath("output/a.tar_1.gz")), QByteArray("old"));
    }
    void readOnlyDestination_data() { QTest::addColumn<bool>("move"); QTest::newRow("copy") << false; QTest::newRow("rename-first-move") << true; }
    void readOnlyDestination() {
        QFETCH(bool, move); QTemporaryDir temp; const auto source = temp.filePath("source"), target = temp.filePath("target");
        QVERIFY(write(source, "new payload")); QVERIFY(write(target, "old protected payload")); QVERIFY(QFile::setPermissions(target, QFileDevice::ReadOwner));
        FileOperations files; QSignalSpy done(&files, &FileOperations::finished);
        connect(&files, &FileOperations::overwriteRequested, &files, [&](OverwriteConflict conflict) { QVERIFY(files.resolveOverwrite(conflict.id, OverwriteAnswer::Yes)); });
        files.transfer({source}, target, move); QVERIFY(finished(done)); QVERIFY(done.last().first().toString().contains("read-only")); QCOMPARE(read(target), QByteArray("old protected payload")); QCOMPARE(read(source), QByteArray("new payload"));
        QVERIFY(QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner));
    }
    void waitingCancelAndDestruction() {
        QTemporaryDir temp; QVERIFY(write(temp.filePath("source"), "new")); QVERIFY(write(temp.filePath("target"), "old"));
        auto files = std::make_unique<FileOperations>(); QSignalSpy ask(files.get(), &FileOperations::overwriteRequested), done(files.get(), &FileOperations::finished); files->transfer({temp.filePath("source")}, temp.filePath("target"), false); QVERIFY(ask.wait(10000)); const auto previous = qvariant_cast<OverwriteConflict>(ask.first().first()); files->cancel(); QVERIFY(finished(done)); QVERIFY(!done.last().first().toString().isEmpty()); QCOMPARE(read(temp.filePath("target")), QByteArray("old"));
        ask.clear(); done.clear(); files->transfer({temp.filePath("source")}, temp.filePath("target"), false); QVERIFY(ask.wait(10000)); QVERIFY(!files->resolveOverwrite(previous.id, OverwriteAnswer::Yes)); files.reset(); QCOMPARE(read(temp.filePath("target")), QByteArray("old"));
    }
    void dialogButtonsAndGuiConnection() {
        const OverwriteConflict conflict{1, {"/destination/old.txt", 1234, QDateTime::fromSecsSinceEpoch(1700000000), true, false}, {"folder/new.txt", 34, {}, true, false}};
        const QStringList names{"overwriteYes", "overwriteNo", "overwriteYesAll", "overwriteNoAll", "overwriteRename", "overwriteCancel"}; const QList<OverwriteAnswer> answers{OverwriteAnswer::Yes, OverwriteAnswer::No, OverwriteAnswer::YesToAll, OverwriteAnswer::NoToAll, OverwriteAnswer::AutoRename, OverwriteAnswer::Cancel};
        for (int n = 0; n < names.size(); ++n) { OverwriteDialog dialog(conflict); dialog.show(); QCOMPARE(dialog.windowModality(), Qt::ApplicationModal); QVERIFY(dialog.findChild<QLabel *>("overwriteExisting")->text().contains("1234")); QTest::mouseClick(dialog.findChild<QPushButton *>(names[n]), Qt::LeftButton); QCOMPARE(dialog.answer(), answers[n]); }
        QTemporaryDir temp; QVERIFY(write(temp.filePath("source"), "new")); QVERIFY(write(temp.filePath("target"), "old")); MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto jobs = window.findChild<FileOperations *>(); QVERIFY(jobs); QSignalSpy done(jobs, &FileOperations::finished); jobs->transfer({temp.filePath("source")}, temp.filePath("target"), false);
        QTRY_VERIFY_WITH_TIMEOUT(window.findChild<OverwriteDialog *>("overwriteDialog"), 10000); auto dialog = window.findChild<OverwriteDialog *>("overwriteDialog"); QVERIFY(dialog->isVisible()); QVERIFY(window.operationBusy()); QTest::mouseClick(dialog->findChild<QPushButton *>("overwriteYes"), Qt::LeftButton); QVERIFY(finished(done)); QVERIFY2(done.last().first().toString().isEmpty(), qPrintable(done.last().first().toString())); QCOMPARE(read(temp.filePath("target")), QByteArray("new"));
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]); if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")}); QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("Overwrite"); app.setQuitOnLastWindowClosed(false); OverwriteTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "overwrite.moc"
