// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "MainWindow.h"
#include "PropertiesDialog.h"
#include "PortStyle.h"
#include "UiLanguage.h"
#include "progress-dialog-driver.h"
#include <QApplication>
#include <QAbstractButton>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include "input-driver.h"
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

static QString executable;
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static bool write(QString path, const QByteArray &bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static FileList *tree(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
static bool focus(MainWindow &window, QString name, bool selected = true) {
    auto list = tree(window); auto found = list->findItems(name, Qt::MatchExactly); if (found.size() != 1) return false;
    list->clearSelection(); list->setCurrentItem(found.first(), 0, QItemSelectionModel::NoUpdate); found.first()->setSelected(selected); return true;
}
static ArchiveResult result(const QSignalSpy &spy) { return spy.isEmpty() ? ArchiveResult() : qvariant_cast<ArchiveResult>(spy.last().first()); }

class OpenModeTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
    QString zip, parser, split;
    QByteArray zipBytes, regions;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(30000)) return {}; return result(done);
    }
    ArchiveResult create(QString archive, QStringList files, QString password = {}) {
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = archive; request.workingDirectory = temporary.path(); request.files = files;
        request.format = archive.endsWith(".zip") ? "zip" : "7z"; request.method = request.format == "zip" ? "Deflate" : "LZMA2"; request.level = 1; request.password = password; request.encryptNames = !password.isEmpty(); return execute(request);
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        QVERIFY(write(temporary.filePath("日本語 space.txt"), "independent payload 日本語\n"));
        zip = temporary.filePath("plain.zip"); QVERIFY(create(zip, {"日本語 space.txt"}).success); zipBytes = read(zip); QVERIFY(!zipBytes.isEmpty());
        parser = temporary.filePath("regions.bin"); regions = QByteArray(60, 'P') + zipBytes + QByteArray(80, 'S'); QVERIFY(write(parser, regions));
        split = temporary.filePath("container.bin.001");
        for (qsizetype position = 0, index = 1; position < zipBytes.size(); position += 75, ++index) QVERIFY(write(temporary.filePath("container.bin." + QString::number(index).rightJustified(3, '0')), zipBytes.mid(position, 75)));
        qInfo() << "Owned open-mode fixtures:" << temporary.path();
    }
    void init() { resultDialogs.clear(); QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/AutoRefresh", false); FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save()); UiLanguage::set("en"); }
    void oneLevelAndNormalExtraction() {
        ArchiveRequest request; request.archive = split; auto normal = execute(request); QVERIFY2(normal.success, qPrintable(normal.details)); QCOMPARE(normal.archiveType, QString("zip")); QCOMPARE(normal.entries.first().path, QString("日本語 space.txt")); QCOMPARE(normal.properties.value("Open Layers"), QString("2"));
        request.readMode = "*"; auto outer = execute(request); QVERIFY2(outer.success, qPrintable(outer.details)); QCOMPARE(outer.archiveType, QString("Split")); QCOMPARE(outer.entries.size(), 1); QCOMPARE(outer.entries.first().path, QString("container.bin")); QCOMPARE(outer.properties.value("Open Layers"), QString("1"));
        request.operation = ArchiveOperation::Test; QVERIFY(execute(request).success);
        request.operation = ArchiveOperation::Extract; request.files = {"container.bin"}; request.outputDirectory = temporary.filePath("one-level extract"); request.overwriteMode = "overwrite"; QVERIFY(execute(request).success); QCOMPARE(read(request.outputDirectory + "/container.bin"), zipBytes);
        request.readMode.clear(); request.files = {"日本語 space.txt"}; request.outputDirectory = temporary.filePath("recursive extract"); QVERIFY(execute(request).success); QCOMPARE(read(request.outputDirectory + "/日本語 space.txt"), read(temporary.filePath("日本語 space.txt")));
    }
    void parserRegionsExtractAndHash() {
        ArchiveRequest request; request.archive = parser; request.readMode = "#"; auto listing = execute(request); QVERIFY2(listing.success, qPrintable(listing.details)); QCOMPARE(listing.archiveType, QString("#")); QCOMPARE(listing.entries.size(), 3);
        QCOMPARE(listing.entries[1].properties.value("Type"), QString("zip")); QCOMPARE(listing.entries[1].properties.value("Offset").toULongLong(), 60ULL);
        request.operation = ArchiveOperation::Test; QVERIFY(execute(request).success);
        request.operation = ArchiveOperation::Extract; request.outputDirectory = temporary.filePath("parser extract"); request.overwriteMode = "overwrite"; QVERIFY(execute(request).success);
        for (const auto &entry : listing.entries) QCOMPARE(read(request.outputDirectory + '/' + entry.path), regions.mid(entry.properties.value("Offset").toLongLong(), entry.size));
        request.operation = ArchiveOperation::HashArchive; request.files = {listing.entries[1].path}; request.hashMethod = "SHA256"; const auto hashed = execute(request); QVERIFY2(hashed.success, qPrintable(hashed.details)); QVERIFY2(hashed.details.contains(QString::fromLatin1(QCryptographicHash::hash(zipBytes, QCryptographicHash::Sha256).toHex()), Qt::CaseInsensitive), qPrintable(hashed.details));
    }
    void oneLevelWritableZip_data() { QTest::addColumn<ArchiveOperation>("operation"); QTest::newRow("comment") << ArchiveOperation::Comment; QTest::newRow("create-folder") << ArchiveOperation::CreateFolder; QTest::newRow("replace-file") << ArchiveOperation::ReplaceFile; }
    void oneLevelWritableZip() {
        QFETCH(ArchiveOperation, operation); const auto archive = temporary.filePath("writable-" + QString::number(int(operation)) + ".zip"); QVERIFY(write(archive, zipBytes));
        ArchiveRequest request; request.archive = archive; request.readMode = "*"; request.operation = operation; request.format = "zip"; request.files = {operation == ArchiveOperation::CreateFolder ? "new empty folder" : "日本語 space.txt"}; request.comment = "one level コメント";
        request.replacementSource = temporary.filePath("replacement.txt"); QVERIFY(write(request.replacementSource, "changed payload")); const auto updated = execute(request); QVERIFY2(updated.success, qPrintable(updated.message + updated.details));
        if (operation == ArchiveOperation::Comment) QCOMPARE(updated.entries.first().properties.value("Comment"), request.comment);
        request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = temporary.filePath("writable-restored-" + QString::number(int(operation))); request.overwriteMode = "overwrite";
        QVERIFY(execute(request).success); QCOMPARE(read(request.outputDirectory + "/日本語 space.txt"), operation == ArchiveOperation::ReplaceFile ? QByteArray("changed payload") : read(temporary.filePath("日本語 space.txt")));
        if (operation == ArchiveOperation::CreateFolder) QVERIFY(QFileInfo(request.outputDirectory + "/new empty folder").isDir());
    }
    void prefixedUpdates_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<int>("encryption");
        QTest::newRow("7z") << QString("7z") << 0;
        QTest::newRow("7z-data-password") << QString("7z") << 1;
        QTest::newRow("7z-header-password") << QString("7z") << 2;
        QTest::newRow("zip") << QString("zip") << 0;
        QTest::newRow("zip-AES-password") << QString("zip") << 1;
        QTest::newRow("tar") << QString("tar") << 0;
        QTest::newRow("wim") << QString("wim") << 0;
    }
    void prefixedUpdates() {
        QFETCH(QString, format); QFETCH(int, encryption);
        const auto root = temporary.filePath("prefix " + format + QString::number(encryption)); QVERIFY(QDir().mkpath(root));
        const auto original = root + "/source." + format, archive = root + "/prefixed.bin";
        QVERIFY(write(root + "/日本語 space.txt", "prefix payload 日本語\n")); QVERIFY(write(root + "/unchanged.txt", "unrelated unchanged payload\n"));
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = original; request.format = format;
        request.method = format == "7z" ? "LZMA2" : format == "zip" ? "Deflate" : format == "tar" ? "posix" : QString(); request.level = 1; request.workingDirectory = root;
        request.files = {"日本語 space.txt", "unchanged.txt"}; if (encryption) request.password = "prefix test password"; request.encryptNames = encryption == 2;
        auto outcome = execute(request); QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details));
        QByteArray prefix(8192, 'P'); prefix[0] = 'M'; prefix[1] = 'Z'; prefix[4095] = char(0xff); prefix[4096] = '\0';
        QVERIFY(write(archive, prefix + read(original))); request.archive = archive; request.files.clear(); request.operation = ArchiveOperation::List;
        outcome = execute(request); QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(outcome.archiveType, format);
        QCOMPARE(outcome.properties.value("Open Layers"), QString("1")); QCOMPARE(outcome.properties.value("Offset").toULongLong(), quint64(prefix.size()));
        request.operation = ArchiveOperation::CreateFolder; request.files = {"日本語 new empty"}; outcome = execute(request);
        QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(read(archive).first(prefix.size()), prefix);
        QVERIFY(write(root + "/replacement.txt", "replacement payload 日本語\n")); request.operation = ArchiveOperation::ReplaceFile;
        request.files = {"日本語 space.txt"}; request.replacementSource = root + "/replacement.txt"; outcome = execute(request);
        QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(read(archive).first(prefix.size()), prefix);
        if (format == "zip") {
            request.operation = ArchiveOperation::Comment; request.comment = "prefix コメント"; outcome = execute(request);
            QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(read(archive).first(prefix.size()), prefix);
        }
        // Existing console CopyFrom/u/rn/d already use the official stream
        // boundary. These assertions cover the complete shared update path.
        QVERIFY(write(root + "/added file.txt", "copy payload\n")); request.operation = ArchiveOperation::Add;
        request.files = {root + "/added file.txt"}; request.useArchiveDefaults = true; request.archivePrefix = "日本語 new empty/";
        outcome = execute(request); QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(read(archive).first(prefix.size()), prefix);
        request.operation = ArchiveOperation::Rename; request.files = {"日本語 new empty/added file.txt", "日本語 new empty/renamed.txt"}; outcome = execute(request);
        QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(read(archive).first(prefix.size()), prefix);
        request.operation = ArchiveOperation::Delete; request.files = {"日本語 new empty/renamed.txt"}; outcome = execute(request);
        QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(read(archive).first(prefix.size()), prefix);
        request.operation = ArchiveOperation::Test; request.files.clear(); outcome = execute(request); QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = root + "/out"; request.overwriteMode = "overwrite"; outcome = execute(request);
        QVERIFY2(outcome.success, qPrintable(outcome.message + outcome.details)); QCOMPARE(read(root + "/out/日本語 space.txt"), read(root + "/replacement.txt"));
        QCOMPARE(read(root + "/out/unchanged.txt"), read(root + "/unchanged.txt")); QVERIFY(QFileInfo(root + "/out/日本語 new empty").isDir());
        QVERIFY(!QFileInfo::exists(root + "/out/日本語 new empty/renamed.txt"));
    }
    void prefixedGuiAndNestedWriteback_data() {
        QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip");
    }
    void prefixedGuiAndNestedWriteback() {
        QFETCH(QString, format); const auto root = temporary.filePath("prefix GUI " + format); QVERIFY(QDir().mkpath(root));
        QVERIFY(write(root + "/data.txt", "prefixed nested payload"));
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/inner.zip"; request.format = "zip";
        request.method = "Deflate"; request.level = 1; request.workingDirectory = root; request.files = {"data.txt"}; QVERIFY(execute(request).success);
        request.archive = root + "/outer." + format; request.format = format; request.method = format == "7z" ? "LZMA2" : "Deflate"; request.files = {"inner.zip"}; QVERIFY(execute(request).success);
        const auto archive = root + "/with-prefix.bin"; const auto prefix = QByteArray(4096, 'S'); QVERIFY(write(archive, prefix + read(request.archive)));
        MainWindow window(executable); QString error; int confirmations = 0; QTimer responder; responder.setInterval(5);
        connect(&responder, &QTimer::timeout, &window, [&] {
            for (auto dialog : testInputs(&window)) if (dialog->isVisible()) { dialog->setTextValue("nested new folder"); dialog->accept(); }
            for (auto dialog : window.findChildren<QMessageBox *>()) if (dialog->isVisible()) {
                if (dialog->standardButtons() & QMessageBox::Yes) { ++confirmations; dialog->button(QMessageBox::Yes)->click(); }
                else { error = dialog->text(); dialog->accept(); }
            }
        }); responder.start();
        window.openPath(archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(error.isEmpty());
        QVERIFY(window.findChild<QAction *>("folderAction")->isEnabled());
        QVERIFY(focus(window, "inner.zip")); for (const auto *name : {"renameAction", "deleteAction"}) QVERIFY(window.findChild<QAction *>(name)->isEnabled());
        window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY2(window.currentArchivePath() != archive, qPrintable(error));
        window.findChild<QAction *>("folderAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(error.isEmpty());
        QCOMPARE(tree(window)->findItems("nested new folder", Qt::MatchExactly).size(), 1);
        window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QCOMPARE(window.currentArchivePath(), archive);
        QCOMPARE(confirmations, 1); QVERIFY2(error.isEmpty(), qPrintable(error)); QCOMPARE(read(archive).first(prefix.size()), prefix);
        request.archive = archive; request.operation = ArchiveOperation::Extract; request.files = {"inner.zip"}; request.outputDirectory = root + "/verified parent"; request.overwriteMode = "overwrite";
        auto verified = execute(request); QVERIFY2(verified.success, qPrintable(verified.message + verified.details));
        request.archive = request.outputDirectory + "/inner.zip"; request.operation = ArchiveOperation::List; request.files.clear(); verified = execute(request);
        QVERIFY2(verified.success, qPrintable(verified.message + verified.details)); bool persisted = false; for (const auto &entry : verified.entries) persisted |= entry.directory && entry.path == "nested new folder";
        QVERIFY2(persisted, qPrintable(verified.details));
        QVERIFY(focus(window, "inner.zip"));
        window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY2(window.currentArchivePath() != archive, qPrintable(error));
        QCOMPARE(tree(window)->findItems("nested new folder", Qt::MatchExactly).size(), 1);
        responder.stop(); window.close();
    }
    void updateRejectedLayouts() {
        for (const auto *layout : {"tail", "split"}) {
            const auto archive = QString(layout) == "tail" ? temporary.filePath("tail-refused.zip") : split;
            if (QString(layout) == "tail") QVERIFY(write(archive, zipBytes + QByteArray(16, 'T')));
            ArchiveRequest request; request.archive = archive; request.operation = ArchiveOperation::CreateFolder;
            request.format = "zip"; request.files = {"must not be added"}; const auto before = read(request.archive);
            const auto rejected = execute(request); QVERIFY(!rejected.success); QCOMPARE(read(request.archive), before);
            MainWindow window(executable); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
            QCOMPARE(window.currentArchivePath(), request.archive);
            QVERIFY(!window.findChild<QAction *>("folderAction")->isEnabled()); window.close();
        }
    }
    void explicitModeFailures_data() { QTest::addColumn<QString>("mode"); QTest::addColumn<QString>("name"); QTest::newRow("whole-zip-not-parser") << QString("#") << QString("plain.zip"); QTest::newRow("unrecognized-parser") << QString("#") << QString("日本語 space.txt"); QTest::newRow("invalid-mode") << QString("zip:invalid") << QString("plain.zip"); }
    void explicitModeFailures() {
        QFETCH(QString, mode); QFETCH(QString, name); const auto source = temporary.filePath(name); const auto before = read(source);
        ArchiveRequest request; request.archive = source; request.readMode = mode; const auto failure = execute(request); QVERIFY(!failure.success); QCOMPARE(read(source), before); QCOMPARE(failure.operation, ArchiveOperation::List); QCOMPARE(failure.target, source); QVERIFY(!failure.message.isEmpty());
        if (mode == "#") QCOMPARE(failure.exitCode, 2); else QCOMPARE(failure.exitCode, -1);
        request.readMode.clear(); request.archive = zip; QVERIFY(execute(request).success);
    }
    void guiOneLevelFocusedAndModeReset() {
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished);
        QVERIFY(focus(window, "container.bin.001", false)); auto inside = window.findChild<QAction *>("insideOneAction"); QVERIFY(inside->isEnabled()); QVERIFY(inside->shortcut().isEmpty()); inside->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("Split")); QVERIFY(focus(window, "container.bin"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("Split")); QVERIFY(focus(window, "container.bin"));
        QTimer accept; accept.setInterval(5); connect(&accept, &QTimer::timeout, &window, [&] { if (auto dialog = window.findChild<ExtractDialog *>(); dialog && dialog->isVisible()) dialog->accept(); }); accept.start();
        window.findChild<QAction *>("extractAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); accept.stop(); QVERIFY2(result(done).success, qPrintable(result(done).details)); QCOMPARE(read(temporary.filePath("container.bin/container.bin")), zipBytes);
        window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(window.currentArchivePath().isEmpty()); QVERIFY(focus(window, "container.bin.001", false)); window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("zip")); QVERIFY(focus(window, "日本語 space.txt")); QVERIFY(!window.findChild<QAction *>("commentAction")->isEnabled());
        UiLanguage::set("ja"); auto expected = UiLanguage::text("Open &Inside"); expected.remove('&'); QCOMPARE(UiLanguage::text("Open Inside *"), expected + " *"); QCOMPARE(UiLanguage::text("Open Inside #"), expected + " #"); UiLanguage::set("en");
    }
    void guiParserNestedAndRestored() {
        MainWindow window(executable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto backend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy done(backend, &ArchiveBackend::finished);
        QVERIFY(focus(window, "regions.bin", false)); auto inside = window.findChild<QAction *>("insideParserAction"); QVERIFY(inside->isEnabled()); QVERIFY(inside->shortcut().isEmpty()); inside->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("#"));
        QVERIFY(focus(window, "2.zip")); window.findChild<QAction *>("infoAction")->trigger(); auto info = window.findChild<PropertiesDialog *>("propertiesDialog"); QVERIFY(info); bool offset = false; for (int n = 0; n < info->table()->topLevelItemCount(); ++n) { auto row = info->table()->topLevelItem(n); offset |= row->text(0) == "Offset" && row->text(1) == "60"; } QVERIFY(offset); info->close();
        window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(window.currentArchivePath() != parser); const auto child = window.currentArchivePath(); QVERIFY(focus(window, "日本語 space.txt")); QVERIFY(!window.findChild<QAction *>("commentAction")->isEnabled());
        window.findChild<QAction *>("upAction")->trigger(); QCOMPARE(window.currentArchivePath(), parser); QVERIFY(!QFileInfo::exists(child)); QCOMPARE(tree(window)->currentItem()->text(0), QString("2.zip"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(result(done).archiveType, QString("#")); QVERIFY(focus(window, "2.zip"));
        window.findChild<QAction *>("hashSHA256Action")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(result(done).success); QVERIFY(result(done).details.contains(QString::fromLatin1(QCryptographicHash::hash(zipBytes, QCryptographicHash::Sha256).toHex()), Qt::CaseInsensitive));
        QTimer accept; accept.setInterval(5); connect(&accept, &QTimer::timeout, &window, [&] { if (auto dialog = window.findChild<ExtractDialog *>(); dialog && dialog->isVisible()) dialog->accept(); }); accept.start();
        window.findChild<QAction *>("extractAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); accept.stop(); QVERIFY2(result(done).success, qPrintable(result(done).details)); QCOMPARE(read(temporary.filePath("regions/2.zip")), zipBytes);
    }
    void guiNestedParserAndPassword_data() { QTest::addColumn<bool>("encrypted"); QTest::newRow("plain-parent") << false; QTest::newRow("encrypted-parent") << true; }
    void guiNestedParserAndPassword() {
        QFETCH(bool, encrypted); const auto wrapper = temporary.filePath(encrypted ? "encrypted-wrapper.7z" : "wrapper.7z"); QVERIFY(create(wrapper, {"regions.bin", "日本語 space.txt"}, encrypted ? "parent password" : QString()).success); const auto original = read(wrapper);
        MainWindow window(executable); QTimer responder; QString error; responder.setInterval(5);
        connect(&responder, &QTimer::timeout, &window, [&] { if (auto password = testInput(&window); password && password->isVisible()) { password->setTextValue("parent password"); password->accept(); } for (auto warning : window.findChildren<QMessageBox *>()) if (warning->isVisible() && warning->objectName() != "progressFinalMessage") { error = warning->text(); warning->accept(); } }); responder.start();
        window.openPath(wrapper); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "regions.bin")); window.findChild<QAction *>("insideParserAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "2.zip"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "2.zip")); window.findChild<QAction *>("insideOneAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "日本語 space.txt"));
        window.findChild<QAction *>("upAction")->trigger(); QVERIFY(focus(window, "2.zip")); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(focus(window, "2.zip"));
        window.findChild<QAction *>("upAction")->trigger(); QCOMPARE(window.currentArchivePath(), wrapper); QVERIFY(focus(window, "日本語 space.txt")); QSignalSpy failedOpen(window.findChild<SevenZipProcessBackend *>(), &ArchiveBackend::finished); window.findChild<QAction *>("insideParserAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); const auto failed = result(failedOpen); QVERIFY2(!failed.success && failed.exitCode == 2, qPrintable(failed.message + failed.details)); QVERIFY2(error.contains("Exit code: 2"), qPrintable(error)); QCOMPARE(window.currentArchivePath(), wrapper);
        error.clear(); window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(error.isEmpty()); QCOMPARE(read(wrapper), original); responder.stop();
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("ArchiveOpenModes"); app.setQuitOnLastWindowClosed(false);
    OpenModeTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "archive-open-modes.moc"
