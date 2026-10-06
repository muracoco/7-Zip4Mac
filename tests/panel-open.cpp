// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "MainWindow.h"
#include "PanelDisplay.h"
#include "PanelOpen.h"
#include "PanelSelection.h"
#include "OpenProfile.h"
#include "PortStyle.h"
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include "input-driver.h"
#include <QProcess>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QUuid>

static QString executable;
static const QString registrar = "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister";
static bool write(const QString &path, const QByteArray &bytes = {}) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static QByteArray read(const QString &path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static QList<QJsonObject> records(const QString &gate) { QList<QJsonObject> result; for (const auto &line : read(gate + ".requests").split('\n')) if (!line.isEmpty()) result << QJsonDocument::fromJson(line).object(); return result; }

class UrlWitness : public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;
public slots:
    void open(const QUrl &url) { urls << url; }
};

class PanelOpenTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(20000)) return {};
        return qvariant_cast<ArchiveResult>(done.last()[0]);
    }
    bool handler(const QString &root, const QString &extension, const QString &gate, QString *application) {
        const auto identifier = "org.sevenzipmacport.tests." + extension;
        *application = root + "/Owned Open Witness.app";
        const auto binary = *application + "/Contents/MacOS/Witness";
        if (!QDir().mkpath(QFileInfo(binary).absolutePath()) || !QFile::copy(QString::fromUtf8(PORT_OPEN_APPLICATION), binary) ||
            !QFile::setPermissions(binary, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner | QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther)) return false;
        const QString plist = "<?xml version=\"1.0\"?><!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\"><plist version=\"1.0\"><dict>"
            "<key>CFBundleIdentifier</key><string>" + identifier + "</string><key>CFBundleExecutable</key><string>Witness</string>"
            "<key>CFBundleName</key><string>Owned Open Witness</string><key>CFBundlePackageType</key><string>APPL</string>"
            "<key>CFBundleVersion</key><string>1</string><key>CFBundleShortVersionString</key><string>1.0</string>"
            "<key>LSUIElement</key><true/><key>CFBundleDocumentTypes</key><array><dict><key>CFBundleTypeName</key><string>Owned test only</string>"
            "<key>CFBundleTypeRole</key><string>Editor</string><key>LSHandlerRank</key><string>Owner</string><key>CFBundleTypeExtensions</key><array><string>" + extension + "</string></array>"
            "<key>LSItemContentTypes</key><array><string>" + identifier + "</string></array></dict></array>"
            "<key>UTExportedTypeDeclarations</key><array><dict><key>UTTypeIdentifier</key><string>" + identifier + "</string><key>UTTypeConformsTo</key><array><string>public.data</string></array>"
            "<key>UTTypeTagSpecification</key><dict><key>public.filename-extension</key><string>" + extension + "</string></dict></dict></array></dict></plist>";
        if (!write(*application + "/Contents/Info.plist", plist.toUtf8()) || !write(*application + "/Contents/Resources/gate", gate.toUtf8()) || !write(*application + "/Contents/Resources/plugins", qgetenv("PORT_TEST_PLUGIN_ROOT"))) return false;
        QProcess sign; sign.start("/usr/bin/codesign", {"--force", "--deep", "--sign", "-", *application});
        if (!sign.waitForFinished() || sign.exitCode()) return false;
        QProcess registration; registration.start(registrar, {"-lint", "-v", "-f", *application});
        const bool registered = registration.waitForFinished() && registration.exitCode() == 0;
        qInfo().noquote() << registration.readAllStandardOutput() << registration.readAllStandardError();
        return registered;
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
    }
    void init() { QSettings().clear(); }
    void officialNameProfile_data() {
        QTest::addColumn<QString>("name"); QTest::addColumn<bool>("external"); QTest::addColumn<bool>("warning");
        QTest::newRow("ZIP-document") << QString("Document.DOcx") << true << false;
        QTest::newRow("source") << QString("日本語.cpp") << true << false;
        QTest::newRow("archive") << QString("archive.7z") << false << false;
        QTest::newRow("no-extension") << QString("日本語 space") << false << false;
        QTest::newRow("directory-dot") << QString("parent.txt/no-extension") << false << false;
        QTest::newRow("non-ASCII-casefold") << QString::fromUtf8("file.ſwf") << false << false;
        QTest::newRow("33-character-extension") << ("file." + QString(33, 'x')) << false << false;
        QTest::newRow("four-spaces") << QString("a    .txt") << true << false;
        QTest::newRow("five-spaces") << QString("a     .txt") << true << true;
        QTest::newRow("mixed-space-characters") << (QString("a") + QChar(0x00a0) + QChar(0x2002) + '\t' + QChar(0x3000) + QChar(0x200b) + ".7z") << false << true;
        QTest::newRow("RLO") << (QString("safe") + QChar(0x202e) + ".7z") << false << true;
        QTest::newRow("POSIX-trailing-dot-space") << QString("file.exe. ") << false << false;
    }
    void officialNameProfile() {
        QFETCH(QString, name); QFETCH(bool, external); QFETCH(bool, warning);
        QCOMPARE(openProfileAlwaysExternal(name), external);
        const auto message = openProfileFilenameWarning(name); QCOMPARE(!message.isEmpty(), warning);
        if (name.contains(QChar(0x202e))) QVERIFY(message.contains("[RLO]"));
    }
    void filesystemContentDetection_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("suffix");
        QTest::newRow("7z-unknown") << QString("7z") << QString(".mystery");
        QTest::newRow("ZIP-no-extension") << QString("zip") << QString();
    }
    void filesystemContentDetection() {
        QFETCH(QString, format); QFETCH(QString, suffix);
        const auto root = temporary.filePath("content " + format); QVERIFY(write(root + "/input/inside 日本語.txt", "decoded bytes"));
        const auto archive = root + "/archive 日本語" + suffix;
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = suffix.isEmpty() ? archive + ".zip" : archive; request.format = format;
        request.workingDirectory = root + "/input"; request.files = {"inside 日本語.txt"}; request.method = format == "zip" ? "Deflate" : "LZMA2"; QVERIFY(execute(request).success);
        if (suffix.isEmpty()) { QVERIFY(QFile::rename(request.archive, archive)); request.archive = archive; }
        UrlWitness witness; QDesktopServices::setUrlHandler("file", &witness, "open"); const auto reset = qScopeGuard([] { QDesktopServices::unsetUrlHandler("file"); });
        MainWindow window(executable); window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto list = window.findChild<FileList *>("fileList"); auto rows = list->findItems(QFileInfo(request.archive).fileName(), Qt::MatchExactly); QCOMPARE(rows.size(), 1);
        list->setCurrentItem(rows.first()); rows.first()->setSelected(true); window.findChild<QAction *>("openAction")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!list->findItems("inside 日本語.txt", Qt::MatchExactly).isEmpty()); QVERIFY(witness.urls.isEmpty());
    }
    void filesystemFallbackAndErrors_data() {
        QTest::addColumn<QString>("mode");
        QTest::newRow("regular-fallback") << QString("regular");
        QTest::newRow("permission-error") << QString("permission");
        QTest::newRow("Open-Inside-never-falls-back") << QString("inside");
    }
    void filesystemFallbackAndErrors() {
        QFETCH(QString, mode); const auto root = temporary.filePath("fallback " + mode), path = root + "/ordinary 日本語.mystery";
        QVERIFY(write(path, "ordinary owned file with no archive signature"));
        const auto permissions = qScopeGuard([&] { QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner); });
        if (mode == "permission") QVERIFY(QFile::setPermissions(path, {}));
        UrlWitness witness; QDesktopServices::setUrlHandler("file", &witness, "open"); const auto reset = qScopeGuard([] { QDesktopServices::unsetUrlHandler("file"); });
        MainWindow window(executable); QString errors; QTimer dialogs; dialogs.setInterval(10);
        connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { errors += box->text(); box->accept(); return; } }); dialogs.start();
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto list = window.findChild<FileList *>("fileList");
        auto rows = list->findItems(QFileInfo(path).fileName(), Qt::MatchExactly); QCOMPARE(rows.size(), 1); list->setCurrentItem(rows.first()); rows.first()->setSelected(true);
        window.findChild<QAction *>(mode == "inside" ? "insideAction" : "openAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        if (mode == "regular") { QCOMPARE(witness.urls.size(), 1); QCOMPARE(witness.urls.first().toLocalFile(), path); QVERIFY2(errors.isEmpty(), qPrintable(errors)); }
        else { QVERIFY2(witness.urls.isEmpty(), qPrintable("Unexpected fallback: " + errors)); QVERIFY(errors.contains("Exit code:")); QVERIFY(errors.contains(path)); }
        QVERIFY(!list->findItems(QFileInfo(path).fileName(), Qt::MatchExactly).isEmpty());
    }
    void encryptedUnknownDefaultOpen_data() { QTest::addColumn<bool>("accept"); QTest::newRow("password") << true; QTest::newRow("password-cancel") << false; }
    void encryptedUnknownDefaultOpen() {
        QFETCH(bool, accept); const auto root = temporary.filePath("encrypted " + QString::number(accept)); QVERIFY(write(root + "/input/inside.txt", "secret owned bytes"));
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/encrypted.mystery"; request.format = "7z"; request.workingDirectory = root + "/input"; request.files = {"inside.txt"}; request.password = "owned fixture password"; request.encryptNames = true; QVERIFY(execute(request).success);
        UrlWitness witness; QDesktopServices::setUrlHandler("file", &witness, "open"); const auto reset = qScopeGuard([] { QDesktopServices::unsetUrlHandler("file"); });
        MainWindow window(executable); int prompts = 0; QTimer dialogs; dialogs.setInterval(10);
        connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto input : testInputs(&window)) if (input->isVisible()) { ++prompts; if (accept) { input->setTextValue(request.password); input->accept(); } else input->reject(); return; } }); dialogs.start();
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto list = window.findChild<FileList *>("fileList");
        auto item = list->findItems("encrypted.mystery", Qt::MatchExactly).first(); list->setCurrentItem(item); item->setSelected(true); window.findChild<QAction *>("openAction")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(prompts, 1); QVERIFY(witness.urls.isEmpty());
        QVERIFY(accept ? !list->findItems("inside.txt", Qt::MatchExactly).isEmpty() : !list->findItems("encrypted.mystery", Qt::MatchExactly).isEmpty());
    }
    void filenameWarningBlocksOnlyExternalOpening() {
        const auto root = temporary.filePath("warning"), name = QString("name<html>") + QChar(0x202e) + ".7z";
        QVERIFY(write(root + "/input/inside.txt", "owned bytes"));
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + '/' + name; request.format = "7z"; request.workingDirectory = root + "/input"; request.files = {"inside.txt"}; QVERIFY(execute(request).success);
        MainWindow window(executable); int warnings = 0; QString shown; QTimer dialogs; dialogs.setInterval(10);
        connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { ++warnings; shown = box->text(); QCOMPARE(box->textFormat(), Qt::PlainText); box->accept(); return; } }); dialogs.start();
        window.openPath(root); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto list = window.findChild<FileList *>("fileList");
        const auto candidates = list->findItems(officialPanelName(name), Qt::MatchExactly); QCOMPARE(candidates.size(),1);
        auto item = candidates.first(); QCOMPARE(item->data(0,Qt::UserRole).toString(),request.archive); list->setCurrentItem(item); item->setSelected(true); window.findChild<QAction *>("openAction")->trigger();
        QCOMPARE(warnings, 1); QVERIFY(shown.contains("[RLO]") && shown.contains("<html>")); QVERIFY(!window.operationBusy());
        window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(warnings, 1); QVERIFY(!list->findItems("inside.txt", Qt::MatchExactly).isEmpty());
    }
    void officialPlan() {
        FileList list; IconFileList icons; new PanelSelection(&list, &icons);
        list.setProperty("alternativeSelection", true); list.setSelectionMode(QAbstractItemView::SingleSelection);
        auto parent = new QTreeWidgetItem(&list, {".."}); parent->setData(0, Qt::UserRole + 2, true);
        QList<QTreeWidgetItem *> items;
        for (int n = 0; n < 21; ++n) { auto item = new QTreeWidgetItem(&list, {QString::number(n)}); item->setData(0, Qt::UserRole, QString::number(n)); items << item; }
        list.setCurrentItem(items[0], 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        auto plan = panelOpenPlan(&list, true); QCOMPARE(plan.actions.size(), 1); QVERIFY(plan.actions.first().tryInternal);
        list.setItemSelected(items[0], true); list.setItemSelected(items[1], true);
        plan = panelOpenPlan(&list, true); QCOMPARE(plan.actions.size(), 2); QVERIFY(!plan.actions[0].tryInternal && !plan.actions[1].tryInternal);
        QCOMPARE(plan.actions[0].item, items[0]); QCOMPARE(plan.actions[1].item, items[1]);
        items[1]->setData(0, Qt::UserRole + 1, true); list.setItemSelected(items[2], true);
        plan = panelOpenPlan(&list, true); QCOMPARE(plan.actions.size(), 2); QCOMPARE(plan.actions[1].kind, PanelOpenAction::BrowseFolder);
        plan = panelOpenPlan(&list, false); QCOMPARE(plan.actions.size(), 3); QCOMPARE(plan.actions[1].kind, PanelOpenAction::ExternalFolder);
        list.setCurrentItem(parent, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        plan = panelOpenPlan(&list, true); QCOMPARE(plan.actions.size(), 1); QCOMPARE(plan.actions.first().item, parent);
        plan = panelOpenPlan(&list, false); QCOMPARE(plan.actions.size(), 3); QCOMPARE(plan.actions[0].item, items[0]);
        list.selection->selectAll(false); plan = panelOpenPlan(&list, false); QCOMPARE(plan.actions.size(), 1); QCOMPARE(plan.actions.first().item, parent);
        list.clearSelection(); QVERIFY(panelOpenPlan(&list, true).actions.isEmpty());
        for (auto item : items) list.setItemSelected(item, true);
        plan = panelOpenPlan(&list, true); QVERIFY(plan.tooManyItems); QVERIFY(plan.actions.isEmpty());
        list.setItemSelected(items.last(), false); QVERIFY(!panelOpenPlan(&list, false).tooManyItems);
    }
    void multipleArchiveOpen_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("duplicate"); QTest::addColumn<bool>("outside");
        QTest::newRow("7z-Open") << QString("7z") << false << false;
        QTest::newRow("ZIP-Open-Outside") << QString("zip") << false << true;
        QTest::newRow("ZIP-same-name-Open") << QString("zip") << true << false;
    }
    void multipleArchiveOpen() {
        QFETCH(QString, format); QFETCH(bool, duplicate); QFETCH(bool, outside);
        const auto root = temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces));
        const auto extension = "portopen" + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
        const auto gate = root + "/gate"; QString application;
        // LaunchServices does not register this owned receiver under the
        // system temporary tree. Use an owned temporary Applications folder;
        // existing handlers and default associations remain untouched.
        const auto applications = QDir::homePath() + "/Applications";
        QVERIFY(QDir().mkpath(applications));
        QTemporaryDir receiver(applications + "/7ZipPort-Test-XXXXXX"); QVERIFY(receiver.isValid());
        QVERIFY(handler(receiver.path(), extension, gate, &application));
        const auto cleanup = qScopeGuard([&] { write(gate + ".release"); QProcess unregister; unregister.start(registrar, {"-u", application}); unregister.waitForFinished(); });
        QVERIFY(write(gate + ".edit"));
        const QString first = duplicate ? "same." + extension : "a." + extension, second = duplicate ? first : "日本語 space." + extension;
        QByteArray firstBytes("first unique payload"), secondBytes("second 日本語 payload");
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/outer." + format; request.format = format; request.workingDirectory = root + "/input"; request.method = format == "zip" ? "Deflate" : "LZMA2"; request.level = 1;
        if (duplicate) {
            QProcess python; python.start("python3", {"-c", "import sys,zipfile,warnings;warnings.filterwarnings('ignore');z=zipfile.ZipFile(sys.argv[1],'w');z.writestr(sys.argv[2],b'first unique payload');z.writestr(sys.argv[2],'second 日本語 payload'.encode());z.close()", request.archive, first});
            QVERIFY(python.waitForFinished()); QVERIFY2(python.exitCode() == 0, python.readAllStandardError().constData());
        } else {
            // One selected payload is a genuine archive, but multi-item Open
            // must launch it externally instead of entering that archive.
            QVERIFY(write(root + "/inner-input/inside.txt", "nested payload"));
            ArchiveRequest inner; inner.operation = ArchiveOperation::Add; inner.archive = root + "/inner.7z"; inner.format = "7z"; inner.workingDirectory = root + "/inner-input"; inner.files = {"inside.txt"}; QVERIFY(execute(inner).success);
            firstBytes = read(inner.archive);
            QVERIFY(write(request.workingDirectory + '/' + first, firstBytes)); QVERIFY(write(request.workingDirectory + '/' + second, secondBytes)); request.files = {first, second}; QVERIFY(execute(request).success);
        }
        auto settings = FileManagerSettings::load(); settings.alternativeSelection = true; settings.workMode = 2; settings.workPath = root; settings.removableOnly = false; QVERIFY(settings.save());
        MainWindow window(executable); int questions = 0, updates = 0, extractions = 0, lists = 0, ticks = 0; QString errors;
        connect(window.findChild<SevenZipProcessBackend *>(), &ArchiveBackend::finished, &window, [&](const ArchiveResult &result) {
            if (result.operation == ArchiveOperation::ReplaceFile && result.success) ++updates;
            if (result.operation == ArchiveOperation::Extract && result.success) ++extractions;
            if (result.operation == ArchiveOperation::List && result.success) ++lists;
        });
        QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, &window, [&] { ++ticks; }); heartbeat.start();
        QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) {
                if (box->text().contains("was modified")) { ++questions; QTest::mouseClick(box->button(QMessageBox::Yes), Qt::LeftButton); }
                else { errors += box->text(); box->accept(); }
                return;
            }
        }); dialogs.start();
        window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto files = window.findChild<FileList *>("fileList"); files->selection->selectAll(true); QCOMPARE(files->markedItems().size(), 2);
        for (int n = 0; n < files->topLevelItemCount(); ++n) if (!files->topLevelItem(n)->data(0, Qt::UserRole + 2).toBool()) { files->setCurrentItem(files->topLevelItem(n), 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows); break; }
        window.findChild<QAction *>(outside ? "outsideAction" : "openAction")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(records(gate).size() == 2 || !errors.isEmpty(), 15000);
        if (!errors.isEmpty()) {
            const auto diagnostic = root + "/diagnostic." + extension; QVERIFY(write(diagnostic, "owned diagnostic"));
            QProcess open; open.start("/usr/bin/open", {"-W", diagnostic});
            if (!open.waitForFinished(2000)) { open.kill(); open.waitForFinished(); }
            errors += "\nDefault-handler diagnostic: " + QString::fromUtf8(open.readAllStandardError());
        }
        QVERIFY2(errors.isEmpty() && records(gate).size() == 2, qPrintable(errors + " requests=" + QString::number(records(gate).size())));
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(extractions, 2); QCOMPARE(lists, 1); QVERIFY(ticks > 5); QCOMPARE(files->markedItems().size(), 2);
        const auto opened = records(gate); QVERIFY(opened[0]["path"].toString() != opened[1]["path"].toString());
        QCOMPARE(QByteArray::fromBase64(opened[0]["data"].toString().toLatin1()), firstBytes);
        QCOMPARE(QByteArray::fromBase64(opened[1]["data"].toString().toLatin1()), secondBytes);
        QVERIFY(write(gate + ".release")); QTRY_COMPARE_WITH_TIMEOUT(updates, 2, 15000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QCOMPARE(questions, 2); QVERIFY2(errors.isEmpty(), qPrintable(errors));
        if (duplicate) {
            QProcess python; python.start("python3", {"-c", "import sys,zipfile,base64;z=zipfile.ZipFile(sys.argv[1]);print('\\n'.join(base64.b64encode(z.read(i)).decode() for i in z.infolist()))", request.archive});
            QVERIFY(python.waitForFinished()); QCOMPARE(python.exitCode(), 0);
            const auto rows = python.readAllStandardOutput().trimmed().split('\n'); QCOMPARE(rows.size(), 2);
            QCOMPARE(QByteArray::fromBase64(rows[0]), QByteArray("edited:") + firstBytes); QCOMPARE(QByteArray::fromBase64(rows[1]), QByteArray("edited:") + secondBytes);
        } else {
            request.operation = ArchiveOperation::Extract; request.files.clear(); request.outputDirectory = root + "/restored"; request.overwriteMode = "overwrite";
            const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
            QCOMPARE(read(request.outputDirectory + '/' + first), QByteArray("edited:") + firstBytes); QCOMPARE(read(request.outputDirectory + '/' + second), QByteArray("edited:") + secondBytes);
        }
    }
    void markedNestedArchiveUsesItsOwnIdentity() {
        const auto root = temporary.filePath("marked nested"); QVERIFY(write(root + "/input/inside.txt", "nested bytes"));
        QVERIFY(QDir().mkpath(root + "/outer-input"));
        ArchiveRequest inner; inner.operation = ArchiveOperation::Add; inner.archive = root + "/outer-input/child.7z"; inner.format = "7z"; inner.workingDirectory = root + "/input"; inner.files = {"inside.txt"}; QVERIFY(execute(inner).success);
        QVERIFY(write(root + "/outer-input/unmarked.txt", "focused sibling"));
        ArchiveRequest outer = inner; outer.archive = root + "/outer.7z"; outer.workingDirectory = root + "/outer-input"; outer.files = {"child.7z", "unmarked.txt"}; QVERIFY(execute(outer).success);
        auto settings = FileManagerSettings::load(); settings.alternativeSelection = true; QVERIFY(settings.save());
        MainWindow window(executable); window.openPath(outer.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto files = window.findChild<FileList *>("fileList"); auto child = files->findItems("child.7z", Qt::MatchExactly).first(), sibling = files->findItems("unmarked.txt", Qt::MatchExactly).first();
        files->setItemSelected(child, true); files->setCurrentItem(sibling, 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        window.findChild<QAction *>("openAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY(!files->findItems("inside.txt", Qt::MatchExactly).isEmpty()); QVERIFY(window.findChild<QLineEdit *>("currentPath")->text().contains("child.7z"));
        window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(!files->findItems("unmarked.txt", Qt::MatchExactly).isEmpty());
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("PanelOpen"); app.setQuitOnLastWindowClosed(false);
    PanelOpenTests test; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr;
    return QTest::qExec(&test, args.size() - 1, args.data());
}
#include "panel-open.moc"
