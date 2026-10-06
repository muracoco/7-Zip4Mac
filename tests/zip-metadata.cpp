// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "PortStyle.h"
#include "input-driver.h"
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

static QString engine;
static QByteArray contents(const QString &path) { QFile f(path); return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray(); }
static QByteArray digest(const QString &path) { return QCryptographicHash::hash(contents(path), QCryptographicHash::Sha256); }
static bool write(const QString &path, const QByteArray &bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath()); QFile f(path);
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size();
}

class ZipMetadataTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(engine); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(45000)) qFatal("ZIP metadata operation timed out");
        return qvariant_cast<ArchiveResult>(done.last()[0]);
    }
    bool script(const QString &mode, const QString &root) {
        QProcess process; process.start("/usr/bin/python3", {QString(ZIP_METADATA_FIXTURE_SCRIPT), mode, root});
        if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode()) {
            qWarning().noquote() << process.readAllStandardError(); return false;
        }
        const auto output = process.readAllStandardOutput().trimmed(); if (!output.isEmpty()) qInfo().noquote() << output;
        return true;
    }
    bool pristine(const QString &root, QStringList arguments, bool password = true, const QString &output = {}) {
        // For read commands, an empty -p means an empty password rather than
        // requesting stdin. Creation uses -p to request the two stdin lines.
        if (!arguments.isEmpty() && arguments.first() != "a") arguments.removeAll("-p");
        QProcess process; process.setWorkingDirectory(root);
        if (!output.isEmpty()) process.setStandardOutputFile(output);
        process.start(engine, arguments); if (!process.waitForStarted()) return false;
        if (password) process.write("fixture-private-password\nfixture-private-password\n");
        process.closeWriteChannel();
        if (!process.waitForFinished(30000) || process.exitStatus() != QProcess::NormalExit || process.exitCode()) {
            qWarning().noquote() << process.readAllStandardError(); return false;
        }
        return true;
    }
    bool fixture(const QString &root, const QString &encryption, int level, const QString &variant) {
        if (!write(root + "/input/日本語 space.txt", QByteArray(1024 * 1024, 'x') + "日本語 payload") ||
            !write(root + "/input/keep.txt", "keep payload") || !write(root + "/input/folder/child.txt", "child payload") ||
            !QDir().mkpath(root + "/input/empty")) return false;
        for (const auto &name : {"日本語 space.txt", "keep.txt", "folder/child.txt"}) {
            QFile f(root + "/input/" + name); if (!f.open(QIODevice::ReadWrite)) return false;
            if (!f.setFileTime(QDateTime::fromString("2020-03-04T05:06:08Z", Qt::ISODate), QFileDevice::FileModificationTime)) return false;
        }
        if (level == 0 && !script("store-input", root)) return false;
        QStringList args{"a", "-tzip", "-mx=" + QString::number(level)};
        if (level != 0) args << "-so";
        if (!encryption.isEmpty()) args << "-mem=" + encryption << "-p";
        args << "--" << (level == 0 ? root + "/fixture.zip" : "unused.zip") << ".";
        if (!pristine(root + "/input", args, !encryption.isEmpty(), level == 0 ? QString() : root + "/fixture.zip")) return false;
        return script("prepare-" + variant, root);
    }
    ArchiveRequest selected(const QString &archive, const QString &name, ArchiveOperation operation) {
        ArchiveRequest list; list.archive = archive; const auto result = execute(list);
        if (!result.success) qFatal("ZIP fixture list failed: %s", qPrintable(result.message + result.details));
        ArchiveRequest request; request.archive = archive; request.operation = operation; request.format = "zip";
        request.selectionSnapshot = result.metadata.sourceSnapshot; request.selectionType = result.archiveType;
        request.selectionEntryCount = result.entries.size(); request.selectionDirectory = 0;
        for (const auto &row : result.metadata.directories[0].children) if (row.name == name) {
            request.selection = {{row.identity, row.archiveIndex, row.name}}; request.files = {row.path}; return request;
        }
        qFatal("ZIP fixture row absent"); return {};
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
    }
    void init() {
        QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/AutoRefresh", false);
        FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save());
    }
    void nativeCommentRenameAndExtract_data() {
        QTest::addColumn<QString>("encryption"); QTest::addColumn<int>("level"); QTest::addColumn<QString>("variant");
        QTest::newRow("crypto-deflate-dd32") << QString("ZipCrypto") << 5 << QString("normal");
        QTest::newRow("crypto-store-dd32") << QString("ZipCrypto") << 0 << QString("normal");
        QTest::newRow("crypto-forced64") << QString("ZipCrypto") << 5 << QString("forced64");
        QTest::newRow("crypto-unsigned32") << QString("ZipCrypto") << 0 << QString("unsigned32");
        QTest::newRow("crypto-unsigned64") << QString("ZipCrypto") << 5 << QString("unsigned64");
        QTest::newRow("crypto-skewed-time") << QString("ZipCrypto") << 5 << QString("time-skew");
        QTest::newRow("AES-descriptor") << QString("AES256") << 5 << QString("normal");
        QTest::newRow("plain-descriptor") << QString() << 5 << QString("normal");
    }
    void nativeCommentRenameAndExtract() {
        QFETCH(QString, encryption); QFETCH(int, level); QFETCH(QString, variant);
        const auto root = temporary.filePath(QString::fromLatin1(QTest::currentDataTag()));
        QVERIFY(fixture(root, encryption, level, variant));
        const QStringList testArgs{"t", "-p", "--", root + "/fixture.zip"}; QVERIFY(pristine(root, testArgs));
        auto request = selected(root + "/fixture.zip", "日本語 space.txt", ArchiveOperation::Comment);
        request.comment = "descriptor コメント\nsecond line"; request.password = "wrong-data-password";
        const auto original = digest(request.archive); auto result = execute(request);
        if (variant.startsWith("unsigned")) {
            // Windows uses this same handler: its full local-record reader
            // requires the descriptor signature for updates, but not decoding.
            QVERIFY(!result.success); QVERIFY2(result.details.contains("E_NOTIMPL"), qPrintable(result.message + result.details));
            QCOMPARE(digest(request.archive), original);
            request = selected(request.archive, "日本語 space.txt", ArchiveOperation::Rename); request.files << "renamed 日本語.txt";
            result = execute(request); QVERIFY(!result.success); QVERIFY(result.details.contains("E_NOTIMPL")); QCOMPARE(digest(request.archive), original);
            const auto reference = root + "/official-rename.zip"; QVERIFY(QFile::copy(request.archive, reference));
            QProcess process; process.start(engine, {"rn", "--", reference, "日本語 space.txt", "renamed 日本語.txt"});
            QVERIFY(process.waitForFinished(30000)); QVERIFY(process.exitCode() != 0); QVERIFY(process.readAllStandardError().contains("E_NOTIMPL")); QCOMPARE(digest(reference), original);
            QVERIFY(pristine(root, testArgs));
            QVERIFY(pristine(root, {"x", "-y", "-p", "-o" + root + "/extracted", "--", request.archive}));
            QVERIFY(script("extraction-original", root));
            qInfo("Unsigned descriptor: original handler refuses metadata update; original retained; Test/extraction passed");
            return;
        }
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!result.passwordRequired);
        QVERIFY(script("comment", root)); QVERIFY(pristine(root, testArgs));
        request = selected(root + "/fixture.zip", "日本語 space.txt", ArchiveOperation::Rename);
        request.files << "renamed 日本語.txt"; request.password = "wrong-data-password";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!result.passwordRequired);
        QVERIFY(script("rename", root)); QVERIFY(pristine(root, testArgs));
        QVERIFY(pristine(root, {"x", "-y", "-p", "-o" + root + "/extracted", "--", request.archive}));
        QVERIFY(script("extraction", root));
        request.operation = ArchiveOperation::Test; request.selection.clear(); request.selectionDirectory = -1; request.files.clear();
        request.password = "incorrect"; if (!encryption.isEmpty()) QVERIFY(!execute(request).success);
        request.password = "fixture-private-password"; QVERIFY(execute(request).success);
    }
    void legacyCommentHelperDescriptor() {
        const auto root = temporary.filePath("legacy helper"); QVERIFY(fixture(root, "ZipCrypto", 5, "forced64"));
        QVERIFY(write(root + "/comment.txt", "descriptor コメント\nsecond line"));
        const auto helper = QFileInfo(engine).absolutePath() + "/7zip-comment";
        QProcess process; process.start(helper, {"write", root + "/fixture.zip", root + "/updated.zip", "日本語 space.txt", root + "/comment.txt"});
        QVERIFY(process.waitForFinished(30000)); QVERIFY2(process.exitCode() == 0, process.readAllStandardError().constData());
        QVERIFY(QFile::remove(root + "/fixture.zip")); QVERIFY(QFile::rename(root + "/updated.zip", root + "/fixture.zip"));
        QVERIFY(script("comment", root)); QVERIFY(pristine(root, {"t", "-p", "--", root + "/fixture.zip"}));
    }
    void encryptedFolderRenameAndReplacement() {
        const auto root = temporary.filePath("folder replacement"); QVERIFY(fixture(root, "ZipCrypto", 5, "forced64"));
        auto request = selected(root + "/fixture.zip", "folder", ArchiveOperation::Rename); request.files << "renamed-folder";
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QVERIFY(pristine(root, {"t", "-p", "--", request.archive}));
        request = selected(request.archive, "日本語 space.txt", ArchiveOperation::ReplaceFile);
        request.replacementSource = root + "/replacement.txt"; request.password = "fixture-private-password";
        QVERIFY(write(request.replacementSource, "new replacement 日本語")); result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(pristine(root, {"t", "-p", "--", request.archive}));
        ArchiveRequest extract; extract.archive = request.archive; extract.operation = ArchiveOperation::Extract;
        extract.password = request.password; extract.outputDirectory = root + "/restored"; extract.overwriteMode = "overwrite";
        result = execute(extract); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(contents(root + "/restored/日本語 space.txt"), contents(request.replacementSource));
        QCOMPARE(contents(root + "/restored/renamed-folder/child.txt"), contents(root + "/input/folder/child.txt"));
        QCOMPARE(contents(root + "/restored/keep.txt"), contents(root + "/input/keep.txt"));
    }
    void writableHeaderWarnings_data() {
        QTest::addColumn<QString>("variant");
        QTest::newRow("invalid-local-DOS-time") << QString("warning-time");
        QTest::newRow("truncated-optional-extra-field") << QString("warning-extra");
    }
    void writableHeaderWarnings() {
        QFETCH(QString, variant);
        const auto root = temporary.filePath(variant), archive = root + "/fixture.zip";
        QVERIFY(fixture(root, {}, 0, variant));
        ArchiveRequest list; list.archive = archive; auto result = execute(list);
        QVERIFY2(result.success, qPrintable(result.message + result.details));
        // Native property names come from PropIDUtils: Warnings / Errors.
        // The console text parser calls the corresponding sections * Flags.
        QVERIFY(!result.properties.value("Warnings").isEmpty());
        QVERIFY(result.properties.value("Errors").isEmpty());
        QVERIFY(result.properties.value("Read-only") != "+");
        const auto tailArchive = root + "/blocked-tail.zip"; QVERIFY(QFile::copy(archive, tailArchive));
        { QFile file(tailArchive); QVERIFY(file.open(QIODevice::Append)); QCOMPARE(file.write("owned trailing bytes"), qint64(20)); }
        const auto tailDigest = digest(tailArchive);
        ArchiveRequest blocked; blocked.archive = tailArchive; blocked.format = "zip"; blocked.operation = ArchiveOperation::CreateFolder;
        blocked.files = {"must not be created"}; QVERIFY(!execute(blocked).success); QCOMPARE(digest(tailArchive), tailDigest);
        // Original console and Windows Agent use this same writable handler.
        const auto reference = root + "/official-rename.zip"; QVERIFY(QFile::copy(archive, reference));
        QVERIFY(pristine(root, {"rn", "--", reference, "日本語 space.txt", "renamed 日本語.txt"}, false));
        {
            MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
            auto backend = window.findChild<SevenZipProcessBackend *>(); QVERIFY(backend);
            QSignalSpy loaded(backend, &ArchiveBackend::finished);
            window.openPath(tailArchive); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty() && !window.operationBusy(), 10000);
            QCOMPARE(window.currentArchivePath(), tailArchive);
            auto create = window.findChild<QAction *>("folderAction"); QVERIFY(create); QVERIFY(!create->isEnabled()); loaded.clear();
            window.openPath(archive); QTRY_VERIFY_WITH_TIMEOUT(!loaded.isEmpty() && !window.operationBusy(), 10000);
            QCOMPARE(window.currentArchivePath(), archive);
            QVERIFY(create->isEnabled());
        }
        auto request = selected(archive, "日本語 space.txt", ArchiveOperation::Comment);
        request.comment = "descriptor コメント\nsecond line"; result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(script("comment", root));
        request = selected(archive, "日本語 space.txt", ArchiveOperation::Rename); request.files << "renamed 日本語.txt";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(script("rename", root));
        request = selected(archive, "folder", ArchiveOperation::Delete);
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        request = {}; request.archive = archive; request.format = "zip"; request.operation = ArchiveOperation::CreateFolder;
        request.files = {"new 日本語 folder"}; result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        request = selected(archive, "renamed 日本語.txt", ArchiveOperation::ReplaceFile); request.replacementSource = root + "/replacement.txt";
        QVERIFY(write(request.replacementSource, "replacement 日本語\n")); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        request = {}; request.archive = archive; request.format = "zip"; request.operation = ArchiveOperation::Add;
        request.useArchiveDefaults = true; request.workingDirectory = root; request.files = {root + "/added 日本語.txt"};
        QVERIFY(write(request.files.first(), "added payload 日本語\n")); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        const auto original = digest(archive);
        request = selected(archive, "renamed 日本語.txt", ArchiveOperation::Rename); request.files << "keep.txt";
        QVERIFY(!execute(request).success); QCOMPARE(digest(archive), original);
        QVERIFY(pristine(root, {"t", "--", archive}, false));
        QVERIFY(pristine(root, {"x", "-y", "-o" + root + "/extracted", "--", archive}, false));
        QVERIFY(script("warning-updates", root));
    }
    void utf8CommentBoundsAndFailedUpdate() {
        const auto root = temporary.filePath("comment bounds"); QVERIFY(fixture(root, "ZipCrypto", 5, "normal"));
        auto request = selected(root + "/fixture.zip", "日本語 space.txt", ArchiveOperation::Comment);
        request.comment = QString(21845, QChar(0x65E5)); QCOMPARE(request.comment.toUtf8().size(), 65535);
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        const auto original = digest(request.archive);
        request = selected(request.archive, "日本語 space.txt", ArchiveOperation::Comment); request.comment = QString(21846, QChar(0x65E5));
        result = execute(request); QVERIFY(!result.success);
        // The original handler, rather than the UTF-8 transport, enforces
        // the encoded ZIP field limit and supplies its real process error.
        QCOMPARE(result.exitCode, 2); QCOMPARE(digest(request.archive), original);
        request.comment = QString("nul") + QChar::Null; QVERIFY(!execute(request).success); QCOMPARE(digest(request.archive), original);
        request.comment = "cancelled"; SevenZipProcessBackend backend(engine); QSignalSpy done(&backend, &ArchiveBackend::finished);
        backend.start(request); backend.cancel(); QTRY_COMPARE(done.size(), 1); QVERIFY(qvariant_cast<ArchiveResult>(done[0][0]).cancelled);
        QCOMPARE(digest(request.archive), original);
        request = selected(request.archive, "日本語 space.txt", ArchiveOperation::Rename); request.files << "keep.txt";
        QVERIFY(!execute(request).success); QCOMPARE(digest(request.archive), original);
        QVERIFY(pristine(root, {"t", "-p", "--", request.archive}));
    }
};

int main(int argc, char **argv) {
    if (argc < 2) return 1; engine = QString::fromLocal8Bit(argv[1]);
    QLocale::setDefault(QLocale(QLocale::English));
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("ZipMetadata"); app.setQuitOnLastWindowClosed(false);
    ZipMetadataTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr;
    return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "zip-metadata.moc"
