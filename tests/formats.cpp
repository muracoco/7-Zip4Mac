// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "ArchiveFormats.h"
#include "ArchiveBackend.h"
#include "MainWindow.h"
#include "OpenWith.h"
#include "PortStyle.h"
#include "progress-dialog-driver.h"
#include <QCryptographicHash>
#include <QContextMenuEvent>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <QPushButton>

static QString engine, manifestPath;
class FormatTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    QJsonObject manifest;
    ProgressDialogDriver resultDialogs;
    ArchiveResult execute(SevenZipProcessBackend &backend, const ArchiveRequest &request) {
        QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(90000)) qFatal("Format operation timed out");
        return qvariant_cast<ArchiveResult>(done.last()[0]);
    }
    QByteArray digest(const QString &path) { QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; QCryptographicHash hash(QCryptographicHash::Sha256); hash.addData(&file); return hash.result().toHex(); }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QFile file(manifestPath); QVERIFY(file.open(QIODevice::ReadOnly)); manifest = QJsonDocument::fromJson(file.readAll()).object();
        QCOMPARE(manifest["version"].toString(), QString("26.03"));
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        qInfo() << "Genuine format cases" << manifest["cases"].toArray().size() << "PENDING registration fixtures" << manifest["pending"].toArray().size();
    }
    void inventory() {
        QCOMPARE(ArchiveFormats::all().size(), 61); QCOMPARE(ArchiveFormats::extensions().size(), 138);
        for (const auto &extension : ArchiveFormats::extensions()) { QVERIFY(ArchiveFormats::recognizes("file." + extension.toUpper())); QVERIFY(!ArchiveFormats::recognizes("file." + extension + ".unsupported")); }
        QCOMPARE(ArchiveFormats::unnamedEntry("example.txz"), QString("example.tar")); QCOMPARE(ArchiveFormats::unnamedEntry("example.tar.txz"), QString("example.tar.tar")); QCOMPARE(ArchiveFormats::unnamedEntry("example.swf"), QString("example~.swf"));
    }
    void genuineFormats_data() {
        QTest::addColumn<QByteArray>("data");
        for (auto row : manifest["cases"].toArray()) { const auto object = row.toObject(); const auto name = object["format"].toString() + '.' + object["extension"].toString(); QTest::newRow(name.toUtf8()) << QJsonDocument(object).toJson(QJsonDocument::Compact); }
    }
    void genuineFormats() {
        QFETCH(QByteArray, data); const auto fixture = QJsonDocument::fromJson(data).object(); const auto archive = fixture["archive"].toString(), format = fixture["format"].toString();
        QVERIFY(ArchiveFormats::recognizes(archive)); QVERIFY(openWithArchiveCandidate(archive));
        // Force the intended official handler independently of extension guessing.
        QProcess identified; identified.start(engine, {"l", "-slt", "-t" + format, "-sccUTF-8", "--", archive}); QVERIFY(identified.waitForStarted(5000)); identified.closeWriteChannel(); QVERIFY(identified.waitForFinished(30000));
        const auto identification = identified.readAllStandardOutput() + identified.readAllStandardError();
        QVERIFY2(identified.exitCode() == 0 && identified.exitStatus() == QProcess::NormalExit, identification.constData()); QVERIFY2(identification.contains(("Type = " + format + '\n').toUtf8()), identification.constData());
        SevenZipProcessBackend backend(engine); ArchiveRequest request; request.archive = archive;
        auto result = execute(backend, request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!result.entries.isEmpty());
        QVERIFY(!result.archiveType.isEmpty()); QVERIFY(!result.properties.isEmpty());
        const auto entries = result.entries;
        const auto expectedTestExit = fixture["expectedTestExit"].toInt();
        request.operation = ArchiveOperation::Test; result = execute(backend, request);
        if (expectedTestExit) { QVERIFY(!result.success); QCOMPARE(result.exitCode, expectedTestExit); QVERIFY2(result.details.contains("Unsupported", Qt::CaseInsensitive), qPrintable(result.details)); }
        else QVERIFY2(result.success, qPrintable(result.message + result.details));
        request.operation = ArchiveOperation::Extract; request.outputDirectory = temporary.filePath(format + '/' + fixture["extension"].toString()); request.overwriteMode = "skip"; request.pathMode = "full";
        for (const auto &entry : entries) if (!entry.directory && !entry.link) request.files << entry.path;
        QVERIFY(!request.files.isEmpty()); result = execute(backend, request);
        const auto expectedExtractExit = fixture["expectedExtractExit"].toInt();
        if (expectedExtractExit) { QVERIFY(!result.success); QCOMPARE(result.exitCode, expectedExtractExit); QVERIFY2(result.details.contains("E_NOTIMPL") || result.details.contains("Unsupported", Qt::CaseInsensitive), qPrintable(result.details)); }
        else QVERIFY2(result.success, qPrintable(result.message + result.details));
        QList<QByteArray> hashes; QDirIterator output(request.outputDirectory, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories); while (output.hasNext()) hashes << digest(output.next());
        for (auto expected : fixture["expectedHashes"].toArray()) QVERIFY2(hashes.contains(expected.toString().toLatin1()), qPrintable("Extracted payload SHA-256 mismatch: " + archive + " expected " + expected.toString() + " got " + [&] { QStringList list; for (auto value : hashes) list << value; return list.join(", "); }()));
        // Exercise the File Manager reader explicitly. Original Open profiles
        // start Office documents/media/installers externally; that is tested
        // separately with owned application fixtures in panel-open.cpp.
        MainWindow window(engine); window.openPath(QFileInfo(archive).absolutePath()); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto list = window.findChild<FileList *>("fileList");
        const auto rows = list->findItems(QFileInfo(archive).fileName(), Qt::MatchExactly); QCOMPARE(rows.size(), 1); list->setCurrentItem(rows.first());
        auto guiBackend = window.findChild<SevenZipProcessBackend *>(); QSignalSpy opened(guiBackend, &ArchiveBackend::finished); window.findChild<QAction *>("insideAction")->trigger();
        if (opened.isEmpty()) QVERIFY(opened.wait(10000)); QVERIFY(qvariant_cast<ArchiveResult>(opened.last()[0]).success); QCOMPARE(window.currentArchivePath(), archive);
    }
    void contextCompressionExtraction() {
        QSettings().remove("OpenWith"); const auto base = temporary.filePath("right click 日本語 files"); QVERIFY(QDir().mkpath(base));
        const auto source = base + "/日本語 space.txt"; QFile file(source); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("right click round trip\n"); file.close(); const auto sourceHash = digest(source);
        MainWindow window(engine); window.show(); auto list = window.findChild<FileList *>("fileList"); auto backend = window.findChild<SevenZipProcessBackend *>();
        auto clickCommand = [&](QString path, QString id, QString checksum = {}) {
            window.openPath(base); if (!QTest::qWaitFor([&] { return !window.operationBusy(); }, 10000)) qFatal("Context directory read timed out"); const auto rows = list->findItems(QFileInfo(path).fileName(), Qt::MatchExactly); if (rows.size() != 1) qFatal("Context source missing"); list->setCurrentItem(rows.first());
            QSignalSpy done(backend, &ArchiveBackend::finished); bool clicked = false; QTimer select, limit; select.setInterval(20); limit.setSingleShot(true);
            connect(&select, &QTimer::timeout, &window, [&] {
                auto root = window.findChild<QMenu *>("fileContextMenu"); auto menu = window.findChild<QMenu *>("sevenZipContextMenu"); if (!root || !root->isVisible() || !menu) return;
                auto action = menu->findChild<QAction *>("context_" + id); if (!action) qFatal("Context command missing");
                menu->popup(root->mapToGlobal(QPoint(root->width(), 0))); select.stop(); clicked = true;
                // Qt-delivered mouse events exercise menu hit-testing. Native
                // AppKit/right-click verification remains a separate desktop test.
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, menu->actionGeometry(action).center());
                if (!checksum.isEmpty()) {
                    auto hashes = action->menu();
                    if (!hashes || !QTest::qWaitFor([hashes] { return hashes->isVisible(); }, 1000)) qFatal("Checksum submenu did not open");
                    QAction *method = nullptr;
                    for (auto candidate : hashes->actions()) if (candidate->text() == checksum) method = candidate;
                    if (!method) qFatal("Checksum command missing");
                    QTest::mouseClick(hashes, Qt::LeftButton, Qt::NoModifier, hashes->actionGeometry(method).center());
                }
            });
            connect(&limit, &QTimer::timeout, &window, [&] { select.stop(); for (auto menu : window.findChildren<QMenu *>()) menu->close(); }); select.start(); limit.start(5000);
            const auto point = list->visualItemRect(rows.first()).center(); QContextMenuEvent event(QContextMenuEvent::Mouse, point, list->viewport()->mapToGlobal(point)); QApplication::sendEvent(list->viewport(), &event);
            select.stop(); limit.stop(); if (!clicked) qFatal("Context menu did not accept a Qt mouse click"); if (done.isEmpty() && !done.wait(30000)) qFatal("Context operation timed out");
            return qvariant_cast<ArchiveResult>(done.last()[0]);
        };
        for (const auto &format : {QString("7z"), QString("zip")}) {
            auto result = clickCommand(source, format); QVERIFY2(result.success, qPrintable(result.message + result.details));
            const auto archive = base + "/日本語 space." + format; QVERIFY(QFileInfo::exists(archive)); result = clickCommand(archive, "test"); QVERIFY(result.success);
            result = clickCommand(archive, "to"); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(digest(base + "/日本語 space/日本語 space.txt"), sourceHash);
            // Remove only this test's own extracted output before the ZIP iteration.
            QVERIFY(QDir(base + "/日本語 space").removeRecursively());
        }
        auto result = clickCommand(source, "checksum", "SHA-256");
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.details.contains(QString::fromLatin1(sourceHash), Qt::CaseInsensitive));
        result = clickCommand(source, "checksum", "SHA-256 -> file.sha256");
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QFile sums(source + ".sha256"); QVERIFY(sums.open(QIODevice::ReadOnly)); QVERIFY(sums.readAll().contains(sourceHash)); sums.close();
        result = clickCommand(source + ".sha256", "checksum", "Checksum : Test");
        QVERIFY2(result.success, qPrintable(result.message + result.details));
        qInfo() << "Qt context-menu clicks: 7z / ZIP creation, Test, Extract to, Japanese paths and SHA-256 comparison passed";
        qInfo() << "Shared CRC submenu: SHA-256, manifest creation and manifest Test clicks passed";
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 3) return 1; engine = QString::fromLocal8Bit(argv[1]); manifestPath = QString::fromLocal8Bit(argv[2]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("Formats");
    FormatTests tests; QList<char *> arguments{argv[0]}; for (int n = 3; n < argc; ++n) arguments << argv[n]; arguments << nullptr; return QTest::qExec(&tests, arguments.size()-1, arguments.data());
}
#include "formats.moc"
