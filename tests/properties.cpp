// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "ArchiveProperties.h"
#include "ArchiveMetadata.h"
#include "MainWindow.h"
#include "PropertiesDialog.h"
#include "PortStyle.h"
#include "UiLanguage.h"
#include "progress-dialog-driver.h"
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDialogButtonBox>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QHeaderView>
#include "input-driver.h"
#include <QMessageBox>
#include <QMimeData>
#include <QScopeGuard>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <fcntl.h>

static QString executable;
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static bool write(QString path, QByteArray bytes) { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static QString value(const ArchivePropertyList &rows, QString name) { for (const auto &property : rows) if (property.name == name) return property.value; return {}; }
static ArchivePropertyList rows(PropertiesDialog *dialog) { ArchivePropertyList result; for (int n = 0; n < dialog->table()->topLevelItemCount(); ++n) { auto item = dialog->table()->topLevelItem(n); result.append({item->text(0), item->data(1, Qt::UserRole).toString()}); } return result; }
static FileList *tree(MainWindow &window) { for (auto list : window.findChildren<FileList *>("fileList")) { bool second = false; for (auto p = list->parent(); p && p != &window; p = p->parent()) second |= p->objectName() == "secondPanel"; if (!second) return list; } return nullptr; }
static QTreeWidgetItem *item(MainWindow &window, QString name) { const auto matches = tree(window)->findItems(name, Qt::MatchExactly); return matches.size() == 1 ? matches.first() : nullptr; }
class PropertyTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
    QString zip, parentArchive, split;
    QByteArray zipBytes, parentBytes;
    ArchiveResult execute(ArchiveRequest request) { SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request); if (done.isEmpty() && !done.wait(30000)) return {}; return qvariant_cast<ArchiveResult>(done.last().first()); }
    ArchiveResult create(QString path, QStringList files) { ArchiveRequest r; r.operation = ArchiveOperation::Add; r.archive = path; r.format = path.endsWith(".zip") ? "zip" : "7z"; r.method = r.format == "zip" ? "Deflate" : "LZMA2"; r.level = 1; r.workingDirectory = temporary.path(); r.files = files; return execute(r); }
    PropertiesDialog *show(MainWindow &window) { window.findChild<QAction *>("infoAction")->trigger(); return window.findChild<PropertiesDialog *>("propertiesDialog"); }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
        QVERIFY(write(temporary.filePath("root.txt"), "root payload")); QVERIFY(write(temporary.filePath("folder/日本語 space.txt"), "nested payload 日本語")); QVERIFY(write(temporary.filePath("folder/sub/deep.txt"), QByteArray(2345, 'x'))); QVERIFY(QDir().mkpath(temporary.filePath("empty")));
        zip = temporary.filePath("plain.zip"); QVERIFY(create(zip, {"root.txt", "folder", "empty"}).success); zipBytes = read(zip);
        parentArchive = temporary.filePath("parent.7z"); QVERIFY(create(parentArchive, {"plain.zip", "root.txt"}).success); parentBytes = read(parentArchive);
        split = temporary.filePath("container.bin.001"); for (int n = 0, number = 1; n < zipBytes.size(); n += 100, ++number) QVERIFY(write(temporary.filePath("container.bin." + QString::number(number).rightJustified(3, '0')), zipBytes.mid(n, 100)));
    }
    void init() { QSettings().clear(); QSettings().setValue("View/LastPath", temporary.path()); QSettings().setValue("View/AutoRefresh", false); FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save()); UiLanguage::set("en"); }
    void orderedListingAndLayers() {
        QString error; auto entries = SevenZipProcessBackend::parseListing("Path = folder\\file\nSize = 1234\nPacked Size = 56\nCRC = AABBCCDD\nComment = Cancel\n\n", &error); QVERIFY(error.isEmpty()); QCOMPARE(entries.size(), 1); QCOMPARE(entries.first().orderedProperties.size(), 5); QCOMPARE(entries.first().orderedProperties[0].name, QString("Path")); QCOMPARE(entries.first().orderedProperties[4].value, QString("Cancel"));
        const auto layers = SevenZipProcessBackend::parseArchiveLayers("noise\n--\nPath = outer\nType = Split\nPhysical Size = 75\n----\nPath = main\nSize = 250\n--\nPath = main\nType = zip\nComment = \n{\nline one\nline two\n}\nPhysical Size = 250\n"); QCOMPARE(layers.size(), 2); QCOMPARE(value(layers[0].properties, "Type"), QString("Split")); QCOMPARE(value(layers[0].childProperties, "Size"), QString("250")); QCOMPARE(value(layers[1].properties, "Type"), QString("zip")); QCOMPARE(value(layers[1].properties, "Comment"), QString("line one\nline two"));
        auto ordered = archiveLayerProperties(layers[1]); QCOMPARE(ordered[0].name, QString("Path")); QCOMPARE(ordered[1].name, QString("Type")); QCOMPARE(ordered[2].name, QString("Physical Size")); QCOMPARE(ordered[3].name, QString("Comment"));
        auto diagnostics = SevenZipProcessBackend::parseArchiveLayers("--\nPath = outer\nOpen WARNING: Cannot open the file as [7z] archive\nType = zip\nERRORS:\nHeaders Error\nUnexpected end of archive\n\nWARNINGS:\nData after the end of archive\n\nWARNING = comment\nOffset = 1000\nPhysical Size = 12345\n"); QCOMPARE(diagnostics.size(), 1); QCOMPARE(value(diagnostics[0].properties, "Error Type"), QString("7z")); QCOMPARE(value(diagnostics[0].properties, "Error Flags"), QString("Headers Error\nUnexpected end of archive")); QCOMPARE(value(diagnostics[0].properties, "Warning Flags"), QString("Data after the end of archive")); ordered = archiveLayerProperties(diagnostics[0]); QCOMPARE(value(ordered, "Physical Size"), QString("12 345")); QVERIFY(!value(ordered, "Open WARNING:").isEmpty()); QCOMPARE(value(archivePropertyRows({{"Size", "12345"}}), "Size"), QString("12 345"));
        ArchiveRequest request; request.archive = split; const auto listed = execute(request); QVERIFY2(listed.success, qPrintable(listed.details)); QCOMPARE(listed.layers.size(), 2); QCOMPARE(value(listed.layers[0].properties, "Type"), QString("Split")); QCOMPARE(value(listed.layers[1].properties, "Type"), QString("zip")); QCOMPARE(value(listed.layers[0].childProperties, "Path"), QString("container.bin"));
    }
    void folderIndexImplicitAndFlat() {
        QString error; auto entries = SevenZipProcessBackend::parseListing("Path = a/one.txt\nSize = 1000\nPacked Size = 100\nCRC = FFFFFFFF\n\nPath = a/b/two.txt\nSize = 2000\nPacked Size = 200\nCRC = 00000002\n\nPath = empty\nFolder = +\nSize = 5\nPacked Size = 7\n\nPath = zero\nSize = 0\n\n", &error); QVERIFY(error.isEmpty()); ArchiveFolderIndex index(entries); const auto root = index.totals(); QCOMPARE(root.size, quint64(3000)); QCOMPARE(root.packed, quint64(300)); QCOMPARE(root.folders, quint64(3)); QCOMPARE(root.files, quint64(3)); QVERIFY(root.crcDefined); QCOMPARE(root.crc, quint32(1));
        auto folder = index.itemProperties("a/", false); QCOMPARE(value(folder, "Name"), QString("a")); QCOMPARE(value(folder, "Size"), QString("3 000")); QCOMPARE(value(folder, "Packed Size"), QString("300")); QCOMPARE(value(folder, "Folders"), QString("1")); QCOMPARE(value(folder, "Files"), QString("2")); QCOMPARE(value(folder, "CRC"), QString("00000001"));
        auto empty = index.itemProperties("empty/", false); QCOMPARE(value(empty, "Size"), QString("0")); QCOMPARE(value(empty, "Folders"), QString("0")); QCOMPARE(value(index.itemProperties("empty/", true), "Size"), QString("5"));
        auto flat = index.selectionProperties({"a/", "zero"}, true); QCOMPARE(value(flat, "Size"), QString("0")); QCOMPARE(value(flat, "Files"), QString("3"));
        auto multiple = index.selectionProperties({"a/", "zero"}, false); QCOMPARE(value(multiple, "Folders"), QString("2")); QCOMPARE(value(multiple, "Files"), QString("3")); QCOMPARE(value(multiple, "Size"), QString("3 000"));
        auto sub = index.folderProperties("a/"); QCOMPARE(value(sub, "Name"), QString("a/")); QCOMPARE(value(sub, "Folders"), QString("1")); QCOMPARE(value(index.itemProperties("a/b/two.txt", true, "a/"), "Path Prefix"), QString("b/"));
        QVERIFY(index.selectionProperties({}, false).isEmpty()); QVERIFY(value(index.folderProperties({}), "Name").isEmpty());
        entries[0].crc.clear(); ArchiveFolderIndex noCrc(entries); QVERIFY(!noCrc.totals().crcDefined); QVERIFY(value(noCrc.folderProperties({}), "CRC").isEmpty());
    }
    void dialogCopyFullValueAndButtons() {
        auto previousClipboard = std::make_unique<QMimeData>();
        if (const auto mime = QApplication::clipboard()->mimeData()) for (const auto &format : mime->formats()) previousClipboard->setData(format, mime->data(format));
        QString ownedClipboard;
        const auto restoreClipboard = qScopeGuard([&] { if (!ownedClipboard.isEmpty() && QApplication::clipboard()->text() == ownedClipboard) QApplication::clipboard()->setMimeData(previousClipboard.release()); });
        const QString longValue = "first\r\nsecond\n" + QString(1300, 'x') + " 日本語"; PropertiesDialog dialog({{"Name", "Cancel"}, {"Comment", longValue}, {"------------------------", {}}}); dialog.show();
        QCOMPARE(dialog.windowModality(), Qt::ApplicationModal); QCOMPARE(dialog.table()->columnCount(), 2); QVERIFY(dialog.table()->header()->isVisible()); QCOMPARE(dialog.table()->headerItem()->text(0), QString()); QVERIFY(dialog.table()->selectedItems().isEmpty()); auto line = dialog.table()->topLevelItem(1); QCOMPARE(line->text(1), QString(longValue.left(1024) + " ...").replace("\r\n", " ").replace('\n', ' ')); QVERIFY(line->text(1).size() < longValue.size());
        dialog.table()->setCurrentItem(line); QTest::keyClick(dialog.table(), Qt::Key_C, Qt::ControlModifier); ownedClipboard = QApplication::clipboard()->text(); QCOMPARE(ownedClipboard, "Comment: " + longValue + "\r\n"); QTest::keyClick(dialog.table(), Qt::Key_Insert, Qt::ControlModifier); ownedClipboard = QApplication::clipboard()->text(); QCOMPARE(ownedClipboard, dialog.selectedText());
        QTest::keyClick(dialog.table(), Qt::Key_Return); auto viewer = dialog.findChild<QDialog *>("propertyValueDialog"); QVERIFY(viewer); QVERIFY(viewer->isVisible()); auto body = viewer->findChild<QPlainTextEdit *>("propertyValueText"); QVERIFY(body->isReadOnly()); QCOMPARE(body->toPlainText(), QString(longValue).replace("\r\n", "\n")); QTest::mouseClick(viewer->findChild<QPushButton *>("propertyValueClose"), Qt::LeftButton); QTRY_VERIFY(!dialog.findChild<QDialog *>("propertyValueDialog"));
        const auto clickPoint = dialog.table()->visualItemRect(line).intersected(dialog.table()->viewport()->rect()).center(); QTest::mouseClick(dialog.table()->viewport(), Qt::LeftButton, Qt::NoModifier, clickPoint); QTest::mouseDClick(dialog.table()->viewport(), Qt::LeftButton, Qt::NoModifier, clickPoint); QTRY_VERIFY(dialog.findChild<QDialog *>("propertyValueDialog")); viewer = dialog.findChild<QDialog *>("propertyValueDialog"); QCOMPARE(viewer->findChild<QPlainTextEdit *>("propertyValueText")->toPlainText(), QString(longValue).replace("\r\n", "\n")); viewer->close(); QTRY_VERIFY(!dialog.findChild<QDialog *>("propertyValueDialog"));
        QTest::keyClick(dialog.table(), Qt::Key_A, Qt::ControlModifier); QCOMPARE(dialog.table()->selectedItems().size(), 3); const auto before = dialog.selectedText(); QTest::keyClick(dialog.table(), Qt::Key_Delete); QCOMPARE(dialog.table()->topLevelItemCount(), 3); QCOMPARE(dialog.selectedText(), before);
        QTest::mouseClick(dialog.findChild<QDialogButtonBox *>("propertiesButtons")->button(QDialogButtonBox::Ok), Qt::LeftButton); QCOMPARE(dialog.result(), int(QDialog::Accepted));
        UiLanguage::set("ja"); PropertiesDialog localized({{"Comment", "Cancel"}}); QCOMPARE(localized.table()->topLevelItem(0)->text(1), QString("Cancel")); localized.show(); QTest::mouseClick(localized.findChild<QDialogButtonBox *>("propertiesButtons")->button(QDialogButtonBox::Cancel), Qt::LeftButton); QCOMPARE(localized.result(), int(QDialog::Rejected)); UiLanguage::set("en");
    }
    void nativeSchemaAndAllColumns() {
        ArchiveRequest request; request.archive = parentArchive; const auto result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.metadata.native); QVERIFY(!result.metadata.schema.isEmpty());
        const auto typed = result.entries.first().orderedProperties; QVERIFY(!typed.isEmpty());
        for (qsizetype i = 0; i < result.metadata.schema.size(); ++i) { QCOMPARE(typed[i].id, result.metadata.schema[i].id); QVERIFY(typed[i].native); }
        ArchiveFolderIndex index(result.entries, result.metadata); const auto columns = index.columns(false);
        QCOMPARE(columns.first().id, quint32(4)); for (const auto &column : columns) QVERIFY(column.raw || column.id != 6);
        QVERIFY(std::any_of(columns.cbegin(), columns.cend(), [](const auto &p) { return !p.raw && p.id == 27; })); // 7z Block, omitted by the old fixed list.
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(parentArchive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QCOMPARE(tree(window)->columnCount(), columns.size()); QVERIFY(tree(window)->headerItem()->text(columns.size() - 1) == "Files");
        const auto file = item(window, "plain.zip"); QVERIFY(file); QCOMPARE(file->data(0, Qt::UserRole + 110).toLongLong(), result.entries.first().archiveIndex);
        int block = -1; for (int column = 0; column < columns.size(); ++column) if (columns[column].id == 27) block = column;
        QVERIFY(block >= 0); QCOMPARE(file->text(block), result.entries.first().properties.value("Block"));
    }
    void nativeFlatNameAndPrefixColumns() {
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(zip); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.findChild<QAction *>("flatAction")->trigger();
        const auto file = item(window, "deep.txt"); QVERIFY(file); QCOMPARE(file->data(0, Qt::UserRole).toString(), QString("folder/sub/deep.txt"));
        int prefix = -1; for (int column = 0; column < tree(window)->columnCount(); ++column) if (tree(window)->headerItem()->text(column) == "Path Prefix") prefix = column;
        QVERIFY(prefix >= 0); QCOMPARE(file->text(prefix), QString("folder/sub/"));
        window.findChild<QAction *>("flatAction")->trigger(); QVERIFY(item(window, "folder"));
    }
    void nativeEmptyArchiveKeepsSchema() {
        const auto path = temporary.filePath("empty-native.zip"); QByteArray empty("PK\x05\x06", 4); empty += QByteArray(18, '\0'); QVERIFY(write(path, empty));
        ArchiveRequest request; request.archive = path; const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.entries.isEmpty()); QVERIFY(result.metadata.native); QVERIFY(result.metadata.schema.size() > 10);
        ArchiveFolderIndex index(result.entries, result.metadata); QCOMPARE(index.totals().files, quint64(0)); QVERIFY(index.totals().crcDefined);
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(path); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(tree(window)->columnCount(), index.columns(false).size());
    }
    void nativeMultilineCommentsAndLiteralPasswordNames() {
        const QString literal = "literal-password"; const auto filename = literal + " 日本語.txt"; QVERIFY(write(temporary.filePath(filename), "secret-named file payload"));
        const auto path = temporary.filePath("native-secret.7z"); ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = path; add.workingDirectory = temporary.path(); add.files = {filename}; add.password = literal; add.encryptNames = true; add.level = 1; QVERIFY(execute(add).success);
        ArchiveRequest request; request.archive = path; request.password = literal; const auto encrypted = execute(request); QVERIFY2(encrypted.success, qPrintable(encrypted.message + encrypted.details)); QCOMPARE(encrypted.entries.size(), 1); QCOMPARE(encrypted.entries.first().path, filename); QVERIFY(encrypted.metadata.native);
        request.password = "incorrect"; QVERIFY(!execute(request).success); request.password = literal; QVERIFY(execute(request).success);
        const auto commentZip = temporary.filePath("native-comment.zip"); QVERIFY(create(commentZip, {"root.txt"}).success);
        const auto comment = QString("line one\n\nPath = spoofed-file\nSize = 999999\n日本語\r\n") + QString(16000, 'x');
        ArchiveRequest edit; edit.operation = ArchiveOperation::Comment; edit.archive = commentZip; edit.format = "zip"; edit.files = {"root.txt"}; edit.comment = comment; QVERIFY2(execute(edit).success, "Native ZIP comment mutation failed");
        request.archive = commentZip; request.password.clear(); const auto zipResult = execute(request); QVERIFY2(zipResult.success, qPrintable(zipResult.message + zipResult.details)); QCOMPARE(zipResult.entries.size(), 1); QCOMPARE(zipResult.entries.first().properties.value("Comment"), comment);
        ArchiveFolderIndex index(zipResult.entries, zipResult.metadata); QCOMPARE(value(index.itemProperties("root.txt", false), "Comment"), comment);
    }
    void nativeWimRawHashAndProxy() {
        const auto path = temporary.filePath("native.wim"); ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = path; add.format = "wim"; add.workingDirectory = temporary.path(); add.files = {"folder", "root.txt", "empty"}; add.useArchiveDefaults = true; QVERIFY2(execute(add).success, "WIM creation failed");
        ArchiveRequest request; request.archive = path; const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.metadata.native); QVERIFY(!result.metadata.rawSchema.isEmpty());
        bool hash = false; for (const auto &entry : result.entries) if (entry.path.endsWith("root.txt")) for (const auto &property : entry.orderedProperties) if (property.raw && property.id == 67) { hash = true; QCOMPARE(property.rawSize, quint32(20)); const auto digest=QCryptographicHash::hash(read(temporary.filePath("root.txt")), QCryptographicHash::Sha1); QCOMPARE(property.value, QString::fromLatin1(digest.toHex())); QCOMPARE(property.listValue, property.value); QCOMPARE(property.sortData,digest); }
        QVERIFY(hash); ArchiveFolderIndex index(result.entries, result.metadata); QCOMPARE(index.totals().files, quint64(3)); QCOMPARE(index.totals().folders, quint64(3));
        const auto columns = index.columns(false); QVERIFY(columns.last().raw); const auto rows = index.itemProperties("folder/", false); QCOMPARE(value(rows, "Files"), QString("2"));
    }
    void nativeFileTimePrecisionAndUndefinedValues() {
        const auto source = temporary.filePath("precision.txt"); QVERIFY(write(source, "precision payload"));
        const timespec times[2]{{1700000000, 123456789}, {1700000000, 123456789}};
        QCOMPARE(::utimensat(AT_FDCWD, QFile::encodeName(source).constData(), times, 0), 0);
        const auto path = temporary.filePath("precision.7z"); ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = path; add.workingDirectory = temporary.path(); add.files = {"precision.txt", "root.txt"}; add.timestampPrecision = 3; add.modificationTime = 1; add.creationTime = 1; add.accessTime = 1; add.level = 1; QVERIFY(execute(add).success);
        ArchiveRequest request; request.archive = path; const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        bool found = false, undefined = false;
        for (const auto &entry : result.entries) for (const auto &p : entry.orderedProperties) {
            if (entry.path == "precision.txt" && p.id == 12 && !p.raw) { found = true; QCOMPARE(p.type, quint16(64)); QCOMPARE(p.timePrecision.size(), 3); QVERIFY(!p.fileTime.isEmpty()); QVERIFY2(p.value.endsWith(".1234567"), qPrintable(p.value)); }
            if (!p.raw && p.type == 0) { undefined = true; QVERIFY(p.value.isEmpty()); }
        }
        QVERIFY(found); QVERIFY(undefined);
    }
    void nativeTypedIntegersAndRawIdentity() {
        const auto prop = [](int id, int type, QString value) { return QJsonObject{{"id", id}, {"type", type}, {"value", value}}; };
        auto size = prop(7, 21, "18446744073709551615"); size["number"] = "18446744073709551615";
        auto raw = prop(46, 1, QString(400, 'a')); raw["size"] = 200; raw["list"] = "data:200";
        QJsonObject root{{"version", 1}, {"schema", QJsonArray{QJsonObject{{"id", 3}, {"type", 8}, {"name", "Path"}}, QJsonObject{{"id", 7}, {"type", 21}, {"name", "Size"}}}}, {"rawSchema", QJsonArray{QJsonObject{{"id", 46}, {"type", 0}, {"name", "Custom raw"}}}}, {"entries", QJsonArray{QJsonObject{{"index", 0}, {"path", "same\nPath = fake"}, {"directory", false}, {"properties", QJsonArray{prop(3, 8, "same\nPath = fake"), size}}, {"raw", QJsonArray{raw}}}}}, {"folders", QJsonArray{}}, {"layers", QJsonArray{QJsonObject{{"properties", QJsonArray{QJsonObject{{"id", 20}, {"type", 8}, {"name", "Type"}, {"value", "test"}}}}, {"childProperties", QJsonArray{}}}}}, {"nonOpen", QJsonArray{}}};
        ArchiveResult result; QVERIFY(parseNativeMetadata(QJsonDocument(root).toJson(), result).isEmpty()); QCOMPARE(result.entries.first().size, std::numeric_limits<quint64>::max()); QCOMPARE(result.entries.first().path, QString("same\nPath = fake")); QCOMPARE(result.entries.first().orderedProperties.last().rawSize, quint32(200));
        ArchiveFolderIndex index(result.entries, result.metadata); const auto rows = index.itemProperties(result.entries.first().path, false); QCOMPARE(value(rows, "Size"), QString("18 446 744 073 709 551 615")); QCOMPARE(rows.last().value.size(), 400); QVERIFY(rows.last().raw);
        root["entries"] = QJsonArray{QJsonObject{{"index", 5}}}; QVERIFY(!parseNativeMetadata(QJsonDocument(root).toJson(), result).isEmpty());
    }
    void nativeControlCharacterFilenameRoundTrip() {
        const QString filename = "日本語\nPath = fake\rfile.txt"; const QByteArray payload("literal control-character filename payload");
        QVERIFY(write(temporary.filePath(filename), payload)); const auto archive = temporary.filePath("literal-names.7z"); QVERIFY(create(archive, {filename}).success);
        ArchiveRequest request; request.archive = archive; const auto listed = execute(request); QVERIFY2(listed.success, qPrintable(listed.message + listed.details)); QCOMPARE(listed.entries.size(), 1); QCOMPARE(listed.entries.first().path, filename);
        request.operation = ArchiveOperation::Extract; request.outputDirectory = temporary.filePath("literal-extracted"); request.overwriteMode = "overwrite"; const auto extracted = execute(request); QVERIFY2(extracted.success, qPrintable(extracted.message + extracted.details)); QCOMPARE(read(request.outputDirectory + '/' + filename), payload); QVERIFY(!QFileInfo::exists(request.outputDirectory + "/fake"));
    }
    void nativeMetadataDestinationProtection() {
        const auto helper = QFileInfo(executable).absolutePath() + "/7zz-progress";
        const auto path = temporary.filePath("protected-metadata.json"); const QByteArray sentinel("owned test sentinel"); QVERIFY(write(path, sentinel));
        auto run = [&] { QProcess process; auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_METADATA_PATH", path); process.setProcessEnvironment(environment); process.start(helper, {"l", "-slt", "--", zip}); if (!process.waitForStarted(5000) || !process.waitForFinished(10000)) return false; return process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0; };
        QVERIFY(run()); QCOMPARE(read(path), sentinel);
        QVERIFY(QFile::remove(path)); const auto target = temporary.filePath("owned-metadata-target"); QVERIFY(write(target, sentinel)); QVERIFY(QFile::link(target, path)); QVERIFY(run()); QCOMPARE(read(target), sentinel); QCOMPARE(read(zip), zipBytes);
    }
    void guiSingleMultipleUnselectedAndPrefix() {
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(zip); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); const auto before = read(zip); auto list = tree(window); auto folder = item(window, "folder"); QVERIFY(folder); list->setCurrentItem(folder); auto dialog = show(window); QVERIFY(dialog); auto single = rows(dialog); QCOMPARE(value(single, "Name"), QString("folder")); QCOMPARE(value(single, "Folders"), QString("1")); QCOMPARE(value(single, "Files"), QString("2")); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
        list->clearSelection(); folder->setSelected(true); item(window, "root.txt")->setSelected(true); dialog = show(window); QVERIFY(dialog); auto multiple = rows(dialog); QCOMPARE(multiple[0].value, QString("2 object(s) selected")); QCOMPARE(value(multiple, "Folders"), QString("2")); QCOMPARE(value(multiple, "Files"), QString("3")); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
        list->clearSelection(); dialog = show(window); QVERIFY(dialog); auto unselected = rows(dialog); QCOMPARE(unselected[0].name, QString("Size")); QCOMPARE(value(unselected, "Folders"), QString("3")); QCOMPARE(value(unselected, "Files"), QString("3")); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
        list->setCurrentItem(folder); window.findChild<QAction *>("insideAction")->trigger(); QVERIFY(item(window, "sub")); list->clearSelection(); dialog = show(window); QVERIFY(dialog); auto prefix = rows(dialog); QCOMPARE(prefix[0].name, QString("Name")); QCOMPARE(prefix[0].value, QString("folder/")); QCOMPARE(value(prefix, "Files"), QString("2")); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>()); QCOMPARE(read(zip), before);
    }
    void guiNestedAndLayerRestore() {
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(parentArchive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); tree(window)->setCurrentItem(item(window, "plain.zip")); window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(window.currentArchivePath() != parentArchive); tree(window)->clearSelection(); auto dialog = show(window); QVERIFY(dialog); auto values = rows(dialog); QStringList types, paths; for (auto row : values) { if (row.name == "Type") types << row.value; if (row.name == "Path") paths << row.value; } QCOMPARE(types, QStringList({"zip", "7z"})); QVERIFY(paths.contains(parentArchive + "/plain.zip")); QVERIFY(paths.contains(parentArchive)); for (const auto &path : paths) QVERIFY(!path.contains(".7zip-nested-")); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
        window.findChild<QAction *>("upAction")->trigger(); QCOMPARE(window.currentArchivePath(), parentArchive); tree(window)->clearSelection(); dialog = show(window); QVERIFY(dialog); values = rows(dialog); types.clear(); for (auto row : values) if (row.name == "Type") types << row.value; QCOMPARE(types, QStringList({"7z"})); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
        window.openPath(split); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); tree(window)->clearSelection(); dialog = show(window); QVERIFY(dialog); values = rows(dialog); types.clear(); QStringList physical; for (auto row : values) { if (row.name == "Type") types << row.value; if (row.name == "Physical Size") physical << row.value; } QCOMPARE(types, QStringList({"zip", "Split"})); QCOMPARE(physical.size(), 2); QVERIFY(physical[0] != physical[1]); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>()); QCOMPARE(read(zip), zipBytes); QCOMPARE(read(parentArchive), parentBytes);
    }
    void nestedWritebackRefreshesProperties() {
        const auto writable = temporary.filePath("writeback-parent.7z"); QVERIFY(create(writable, {"plain.zip", "root.txt"}).success);
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(writable); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); tree(window)->setCurrentItem(item(window, "plain.zip")); window.findChild<QAction *>("insideAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        bool entered = false; QTimer input; input.setInterval(5); connect(&input, &QTimer::timeout, &window, [&] { if (auto name = testInput(&window); name && name->isVisible()) { name->setTextValue("added directory"); entered = true; name->accept(); } }); input.start(); window.findChild<QAction *>("folderAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); input.stop(); QVERIFY(entered); QVERIFY(item(window, "added directory"));
        QSignalSpy savedOperations(window.findChild<SevenZipProcessBackend *>(), &ArchiveBackend::finished);
        bool confirmed = false; QTimer confirm; confirm.setInterval(5); connect(&confirm, &QTimer::timeout, &window, [&] { for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible() && box->icon() == QMessageBox::Question && box->text().contains("was modified")) { confirmed = true; QVERIFY(box->button(QMessageBox::Yes)); box->button(QMessageBox::Yes)->click(); } }); confirm.start(); window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); confirm.stop(); QVERIFY(confirmed); QCOMPARE(window.currentArchivePath(), writable);
        bool replaced = false; for (const auto &operation : savedOperations) { const auto result = qvariant_cast<ArchiveResult>(operation[0]); if (result.operation == ArchiveOperation::ReplaceFile) { QVERIFY2(result.success, qPrintable(result.message + result.details)); replaced = true; } } QVERIFY(replaced);
        ArchiveRequest request; request.archive = writable; const auto listed = execute(request); QVERIFY(listed.success);
        // Keep the index alive while reading its entry; its data is a snapshot.
        ArchiveFolderIndex index(listed.entries); const auto saved = index.entry("plain.zip"); QVERIFY(saved); QVERIFY(saved->size != quint64(zipBytes.size()));
        ArchiveRequest restored; restored.operation = ArchiveOperation::Extract; restored.archive = writable; restored.files = {"plain.zip"}; restored.outputDirectory = temporary.filePath("writeback-verified"); restored.overwriteMode = "overwrite"; QVERIFY(execute(restored).success);
        const auto child = temporary.filePath("writeback-verified/plain.zip"); QCOMPARE(quint64(QFileInfo(child).size()), saved->size); QVERIFY(QCryptographicHash::hash(read(child), QCryptographicHash::Sha256) != QCryptographicHash::hash(zipBytes, QCryptographicHash::Sha256)); ArchiveRequest childList; childList.archive = child; const auto childResult = execute(childList); QVERIFY(childResult.success); QVERIFY(ArchiveFolderIndex(childResult.entries).entry("added directory"));
        for (auto progress : window.findChildren<ProgressDialog *>()) progress->close(); auto dialog = show(window); QVERIFY(dialog); const auto values = rows(dialog); QCOMPARE(value(values, "Size"), archivePropertyNumber(saved->size)); QCOMPARE(value(values, "CRC"), saved->crc); QCOMPARE(value(values, "Physical Size"), archivePropertyNumber(quint64(QFileInfo(writable).size()))); dialog->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
    }
    void singleClickAndFilesystemSubstitute() {
        FileManagerSettings settings; settings.singleClick = true; QVERIFY(settings.save()); PropertiesDialog dialog({{"Name", "literal"}}); dialog.show(); auto row = dialog.table()->topLevelItem(0); QTest::mouseClick(dialog.table()->viewport(), Qt::LeftButton, Qt::NoModifier, dialog.table()->visualItemRect(row).center()); QTRY_VERIFY(dialog.findChild<QDialog *>("propertyValueDialog")); dialog.findChild<QDialog *>("propertyValueDialog")->close(); QTRY_VERIFY(!dialog.findChild<QDialog *>("propertyValueDialog")); dialog.close();
        settings.singleClick = false; QVERIFY(settings.save()); MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); tree(window)->setCurrentItem(item(window, "root.txt")); auto properties = show(window); QVERIFY(properties); const auto values = rows(properties); QCOMPARE(value(values, "Name"), QString("root.txt")); QCOMPARE(value(values, "Path"), temporary.filePath("root.txt")); properties->close(); QTRY_VERIFY(!window.findChild<PropertiesDialog *>());
    }
    void destroyAfterClosedProgress() {
        auto window = std::make_unique<MainWindow>(executable); window->show(); QTRY_VERIFY_WITH_TIMEOUT(!window->operationBusy(), 10000); window->openPath(zip); QTRY_VERIFY_WITH_TIMEOUT(!window->operationBusy(), 10000);
        resultDialogs.clear(); window->findChild<QAction *>("testAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window->operationBusy(), 10000);
        QCOMPARE(resultDialogs.notices.size(),1); QVERIFY(!resultDialogs.notices.first().error); QTRY_VERIFY(window->findChildren<ProgressDialog *>().isEmpty());
        // Member-worker destructor notifications must not reach UI members
        // whose C++ lifetimes have already ended.
        window.reset();
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]); if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")}); QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("Properties"); app.setQuitOnLastWindowClosed(false); PropertyTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "properties.moc"
