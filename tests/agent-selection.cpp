// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "MainWindow.h"
#include <QToolButton>
#include "PortStyle.h"
#include "Dialogs.h"
#include <QApplication>
#include <QAbstractButton>
#include <QCryptographicHash>
#include <QContextMenuEvent>
#include <QDirIterator>
#include <QFile>
#include <QSettings>
#include <QMenu>
#include "input-driver.h"
#include "progress-dialog-driver.h"
#include <QMessageBox>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <QProcess>
#include <signal.h>

static QString engine, fixtures;
static QByteArray contents(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
class AgentSelectionTests : public QObject {
    Q_OBJECT
    QTemporaryDir temporary;
    ProgressDialogDriver resultDialogs;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(engine); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request);
        if (done.isEmpty() && !done.wait(30000)) qFatal("Native selection operation timed out");
        return qvariant_cast<ArchiveResult>(done.last()[0]);
    }
    ArchiveResult listing(QString name, QString password = {}) {
        ArchiveRequest request; request.archive = name.startsWith('/') ? name : fixtures + '/' + name; request.password = password;
        auto result = execute(request); if (!result.success) qFatal("Listing failed: %s", qPrintable(result.message + result.details)); return result;
    }
    ArchiveRequest selection(const ArchiveResult &snapshot, QList<ArchiveRow> rows, ArchiveOperation op, int base = 0, bool flat = false) {
        ArchiveRequest request; request.archive = snapshot.target; request.operation = op;
        request.selectionSnapshot = snapshot.metadata.sourceSnapshot; request.selectionType = snapshot.archiveType;
        request.selectionEntryCount = snapshot.entries.size(); request.selectionDirectory = base; request.selectionFlat = flat;
        for (const auto &row : rows) { request.selection.append({row.identity, row.archiveIndex, row.name}); request.files << row.path; }
        request.outputDirectory = temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); request.overwriteMode = "overwrite";
        return request;
    }
    QList<ArchiveRow> duplicates(const ArchiveResult &snapshot) {
        QList<ArchiveRow> rows; for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "same.txt") rows << row; return rows;
    }
    QString ownedCopy(QString source) {
        const auto path = temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + '.' + QFileInfo(source).suffix());
        if (!QFile::copy(fixtures + '/' + source, path)) qFatal("Cannot copy owned update fixture");
        return path;
    }
private slots:
    void initTestCase() {
        QVERIFY(temporary.isValid()); QVERIFY(QFile::exists(fixtures + "/corrupt-duplicate.zip"));
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.filePath("preferences"));
    }
    void init() {
        resultDialogs.clear();
        QSettings().clear(); QSettings().setValue("View/LastPath", fixtures); QSettings().setValue("View/AutoRefresh", false);
        FileManagerSettings settings; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save());
    }
    void sameNameExtractAndHash_data() { QTest::addColumn<int>("item"); QTest::newRow("first") << 0; QTest::newRow("second") << 1; }
    void sameNameExtractAndHash() {
        QFETCH(int, item); const auto snapshot = listing("duplicate.zip"); const auto rows = duplicates(snapshot); QCOMPARE(rows.size(), 2);
        const QByteArray expected = item == 0 ? QByteArray("one") : QByteArray("a different second payload");
        auto request = selection(snapshot, {rows[item]}, ArchiveOperation::Extract); auto result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(contents(request.outputDirectory + "/same.txt"), expected);
        QCOMPARE(QDir(request.outputDirectory).entryList(QDir::Files), QStringList{"same.txt"});
        request.operation = ArchiveOperation::HashArchive; request.hashMethod = "SHA256"; result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details));
        QVERIFY2(result.details.contains(QString::fromLatin1(QCryptographicHash::hash(expected, QCryptographicHash::Sha256).toHex()), Qt::CaseInsensitive), qPrintable(result.details));
        QVERIFY(result.metadata.selectionResolved); QCOMPARE(result.metadata.selectedIndices, QList<quint32>{quint32(rows[item].archiveIndex)});
    }
    void testOnlyChosenSiblingAndRecover() {
        const auto snapshot = listing("corrupt-duplicate.zip"); const auto rows = duplicates(snapshot); QCOMPARE(rows.size(), 2);
        auto good = execute(selection(snapshot, {rows[0]}, ArchiveOperation::Test)); QVERIFY2(good.success, qPrintable(good.details));
        auto bad = execute(selection(snapshot, {rows[1]}, ArchiveOperation::Test)); QVERIFY(!bad.success); QCOMPARE(bad.exitCode, 2); QVERIFY(bad.details.contains("CRC", Qt::CaseInsensitive));
        good = execute(selection(snapshot, {rows[0]}, ArchiveOperation::Test)); QVERIFY(good.success);
        ArchiveRequest all; all.archive = snapshot.target; all.operation = ArchiveOperation::Test; QVERIFY(!execute(all).success);
    }
    void nativeReplacementSameName_data() { QTest::addColumn<int>("item"); QTest::newRow("first") << 0; QTest::newRow("second") << 1; }
    void nativeReplacementSameName() {
        QFETCH(int, item); const auto path = ownedCopy("duplicate.zip"); auto snapshot = listing(path); auto rows = duplicates(snapshot); QCOMPARE(rows.size(), 2);
        const auto input = temporary.filePath("replacement 日本語 space.txt"); QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate)); QCOMPARE(file.write("replacement selected bytes"), qint64(26)); file.close();
        auto request = selection(snapshot, {rows[item]}, ArchiveOperation::ReplaceFile); request.replacementSource = input;
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(result.previousSnapshot, snapshot.metadata.sourceSnapshot); QCOMPARE(result.itemIndexMap.size(), snapshot.entries.size());
        const auto target = result.itemIndexMap[rows[item].archiveIndex], other = result.itemIndexMap[rows[1 - item].archiveIndex]; QVERIFY(target >= 0 && other >= 0 && target != other);
        QCOMPARE(result.entries[target].path, QString("same.txt")); snapshot = listing(path);
        ArchiveRow changed, unchanged; for (const auto &row : snapshot.metadata.directories[0].children) { if (row.archiveIndex == target) changed = row; if (row.archiveIndex == other) unchanged = row; }
        request = selection(snapshot, {changed}, ArchiveOperation::Extract); result = execute(request); QVERIFY(result.success); QCOMPARE(contents(request.outputDirectory + "/same.txt"), QByteArray("replacement selected bytes"));
        request = selection(snapshot, {unchanged}, ArchiveOperation::Extract); result = execute(request); QVERIFY(result.success); QCOMPARE(contents(request.outputDirectory + "/same.txt"), item == 0 ? QByteArray("a different second payload") : QByteArray("one"));
    }
    void nativeReplacementUnrelatedCorruption() {
        const auto path = ownedCopy("corrupt-duplicate.zip"); auto snapshot = listing(path); auto rows = duplicates(snapshot);
        const auto input = temporary.filePath("corruption replacement.txt"); QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("new chosen bytes"), qint64(16)); file.close();
        auto request = selection(snapshot, {rows[0]}, ArchiveOperation::ReplaceFile); request.replacementSource = input;
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); const auto bad = result.itemIndexMap[rows[1].archiveIndex]; snapshot = listing(path);
        ArchiveRow row; for (const auto &candidate : snapshot.metadata.directories[0].children) if (candidate.archiveIndex == bad) row = candidate;
        result = execute(selection(snapshot, {row}, ArchiveOperation::Test)); QVERIFY(!result.success); QVERIFY(result.details.contains("CRC", Qt::CaseInsensitive));
        const auto original = contents(path); request = selection(snapshot, {row}, ArchiveOperation::ReplaceFile); request.replacementSource = input; request.selection[0].identity.item = 99999;
        QVERIFY(!execute(request).success); QCOMPARE(contents(path), original);
    }
    void guiSameNameEditorWriteBack_data() {
        QTest::addColumn<bool>("rename"); QTest::addColumn<bool>("ancestor");
        QTest::newRow("original-name") << false << false; QTest::newRow("renamed-while-editing") << true << false; QTest::newRow("ancestor-renamed-while-editing") << true << true;
    }
    void guiSameNameEditorWriteBack() {
        QFETCH(bool, rename); QFETCH(bool, ancestor);
        const auto path = ownedCopy("duplicate.zip"), script = temporary.filePath("duplicate editor.sh"), gate = temporary.filePath("duplicate-editor-gate-" + QUuid::createUuid().toString(QUuid::WithoutBraces));
        QFile file(script); QVERIFY(file.open(QIODevice::WriteOnly)); const QByteArray code = "#!/bin/sh\nprintf '%s' \"$2\" > \"$1.path\"\nprintf 'editor chosen bytes' > \"$2\"\nwhile [ ! -f \"$1.release\" ]; do /bin/sleep 0.05; done\n"; QCOMPARE(file.write(code), qint64(code.size())); file.close();
        const auto release = qScopeGuard([&] { QFile signal(gate + ".release"); const bool opened = signal.open(QIODevice::WriteOnly); Q_UNUSED(opened); });
        FileManagerSettings settings; settings.editor = "/bin/sh \"" + script + "\" \"" + gate + "\""; settings.realIcons = false; settings.language = "en"; QVERIFY(settings.save());
        MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(path); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        if (ancestor) window.openPath(path + "/implicit/sub/");
        auto address = window.findChild<QLineEdit *>("currentPath"); QVERIFY(address);
        auto list = window.findChild<FileList *>("fileList"); auto backend = window.findChild<SevenZipProcessBackend *>(); QVERIFY(list && backend); auto rows = list->findItems(ancestor ? "deep.txt" : "same.txt", Qt::MatchExactly); QCOMPARE(rows.size(), ancestor ? 1 : 2);
        auto first = ancestor || rows[0]->data(0, Qt::UserRole + 110).toLongLong() == 0 ? rows[0] : rows[1]; auto second = ancestor ? first : first == rows[0] ? rows[1] : rows[0]; list->clearSelection(); second->setSelected(true); list->setCurrentItem(first, 0, QItemSelectionModel::NoUpdate);
        QString errors; QTimer dialogs; dialogs.setInterval(10); connect(&dialogs, &QTimer::timeout, &window, [&] { for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue("renamed.txt"); input->accept(); return; } for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) { if (box->text().contains("was modified")) box->button(QMessageBox::Yes)->click(); else { errors += box->text(); box->accept(); } return; } }); dialogs.start();
        QSignalSpy done(backend, &ArchiveBackend::finished); window.findChild<QAction *>("editAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(gate + ".path"), 10000); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        qint64 sourceIndex = ancestor ? 2 : 0, selectedIndex = ancestor ? 2 : 1;
        if (rename) {
            ArchiveResult renamed;
            if (ancestor) {
                window.findChild<QAction *>("twoPanelsAction")->trigger(); auto panel = window.findChild<MainWindow *>("secondPanel"); QVERIFY(panel);
                panel->openPath(path); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto other = panel->findChild<FileList *>("fileList"); QVERIFY(other);
                rows = other->findItems("implicit", Qt::MatchExactly); QCOMPARE(rows.size(), 1); other->clearSelection(); other->setCurrentItem(rows[0]); rows[0]->setSelected(true);
                QSignalSpy otherDone(panel->findChild<SevenZipProcessBackend *>(), &ArchiveBackend::finished); panel->findChild<QAction *>("renameAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
                for (const auto &args : otherDone) if (qvariant_cast<ArchiveResult>(args[0]).operation == ArchiveOperation::Rename) renamed = qvariant_cast<ArchiveResult>(args[0]);
                QCOMPARE(address->text(), path + "/renamed.txt/sub/");
            } else {
                rows = list->findItems("same.txt", Qt::MatchExactly); list->clearSelection();
                for (auto item : rows) if (item->data(0, Qt::UserRole + 110).toLongLong() == 0) { item->setSelected(true); list->setCurrentItem(item); }
                auto renameAction = window.findChild<QAction *>("renameAction");
                QTRY_VERIFY(renameAction->isEnabled()); renameAction->trigger();
                QTRY_VERIFY_WITH_TIMEOUT(std::any_of(done.begin(), done.end(), [](const QList<QVariant> &args) { return qvariant_cast<ArchiveResult>(args[0]).operation == ArchiveOperation::Rename; }), 10000);
                QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
                for (const auto &args : done) if (qvariant_cast<ArchiveResult>(args[0]).operation == ArchiveOperation::Rename) renamed = qvariant_cast<ArchiveResult>(args[0]);
            }
            QCOMPARE(renamed.operation, ArchiveOperation::Rename); QVERIFY2(renamed.success, qPrintable(renamed.message + renamed.details));
            sourceIndex = renamed.itemIndexMap[sourceIndex]; selectedIndex = sourceIndex;
        }
        QFile signal(gate + ".release"); QVERIFY(signal.open(QIODevice::WriteOnly)); signal.close();
        QTRY_VERIFY2_WITH_TIMEOUT(std::any_of(done.begin(), done.end(), [](const QList<QVariant> &args) { return qvariant_cast<ArchiveResult>(args[0]).operation == ArchiveOperation::ReplaceFile; }), qPrintable(errors), 10000);
        ArchiveResult result; for (const auto &args : done) if (qvariant_cast<ArchiveResult>(args[0]).operation == ArchiveOperation::ReplaceFile) result = qvariant_cast<ArchiveResult>(args[0]); QVERIFY2(result.success, qPrintable(result.message + result.details + errors));
        QVERIFY(list->currentItem()); QCOMPARE(list->currentItem()->data(0, Qt::UserRole + 110).toLongLong(), result.itemIndexMap[sourceIndex]); QCOMPARE(list->selectedItems().size(), 1); QCOMPARE(list->selectedItems().first()->data(0, Qt::UserRole + 110).toLongLong(), result.itemIndexMap[selectedIndex]);
        const auto snapshot = listing(path); ArchiveRow row; for (const auto &directory : snapshot.metadata.directories) for (const auto &candidate : directory.children) if (candidate.archiveIndex == result.itemIndexMap[sourceIndex]) row = candidate;
        auto request = selection(snapshot, {row}, ArchiveOperation::Extract, 0, ancestor); result = execute(request); QVERIFY(result.success); QCOMPARE(contents(request.outputDirectory + '/' + (ancestor ? "renamed.txt/sub/deep.txt" : rename ? "renamed.txt" : "same.txt")), QByteArray("editor chosen bytes")); window.close();
    }
    void nativeCommentSameName_data() { QTest::addColumn<int>("item"); QTest::newRow("first") << 0; QTest::newRow("second") << 1; }
    void nativeCommentSameName() {
        QFETCH(int, item); const auto path = ownedCopy("duplicate-comments.zip"); auto snapshot = listing(path); auto rows = duplicates(snapshot); QCOMPARE(rows.size(), 2);
        auto request = selection(snapshot, {rows[item]}, ArchiveOperation::Comment); request.comment = "コメント 日本語\nsecond line";
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(result.metadata.selectedIndices, QList<quint32>{quint32(rows[item].archiveIndex)});
        snapshot = listing(path); rows = duplicates(snapshot); QCOMPARE(rows.size(), 2);
        QCOMPARE(snapshot.entries[rows[item].archiveIndex].properties.value("Comment"), request.comment);
        QCOMPARE(snapshot.entries[rows[1 - item].archiveIndex].properties.value("Comment"), item == 0 ? QString("second comment") : QString("first comment"));
        request = selection(snapshot, {rows[item]}, ArchiveOperation::Extract); result = execute(request); QVERIFY(result.success);
        QCOMPARE(contents(request.outputDirectory + "/same.txt"), item == 0 ? QByteArray("one") : QByteArray("a different second payload"));
        request = selection(snapshot, {rows[item]}, ArchiveOperation::Comment); request.comment.clear(); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(result.entries[rows[item].archiveIndex].properties.value("Comment"), QString());
    }
    void nativeCommentNoUnrelatedTest() {
        const auto path = ownedCopy("corrupt-duplicate.zip"); auto snapshot = listing(path); auto rows = duplicates(snapshot);
        auto request = selection(snapshot, {rows[0]}, ArchiveOperation::Comment); request.comment = "packed data retained";
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        snapshot = listing(path); rows = duplicates(snapshot);
        QVERIFY(execute(selection(snapshot, {rows[0]}, ArchiveOperation::Test)).success);
        result = execute(selection(snapshot, {rows[1]}, ArchiveOperation::Test)); QVERIFY(!result.success); QCOMPARE(result.exitCode, 2); QVERIFY(result.details.contains("CRC", Qt::CaseInsensitive));
        // CommentItem selects a real folder record and refuses implicit folders.
        ArchiveRow empty, implicit; for (const auto &row : snapshot.metadata.directories[0].children) { if (row.name == "empty") empty = row; if (row.name == "implicit") implicit = row; }
        request = selection(snapshot, {empty}, ArchiveOperation::Comment); request.comment = "folder only";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(result.entries[empty.archiveIndex].properties.value("Comment"), request.comment);
        snapshot = listing(path); for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "implicit") implicit = row;
        const auto original = contents(path); request = selection(snapshot, {implicit}, ArchiveOperation::Comment); request.comment = "not a real item";
        QVERIFY(!execute(request).success); QCOMPARE(contents(path), original);
        ArchiveFolderIndex index(snapshot.entries, snapshot.metadata); ArchiveRow deep;
        for (const auto &row : index.directoryRows(0, true)) if (row.path == "implicit/sub/deep.txt") deep = row;
        QVERIFY(deep.archiveIndex >= 0); request = selection(snapshot, {deep}, ArchiveOperation::Comment, 0, true); request.comment = "Flat descendant";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(result.entries[deep.archiveIndex].properties.value("Comment"), request.comment);
    }
    void nativeCommentEncryptedPackedData() {
        const auto root = temporary.filePath("native comment AES"); QVERIFY(QDir().mkpath(root));
        QFile file(root + "/secret 日本語.txt"); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("secret payload"), qint64(14)); file.close();
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = root + "/secret.zip"; add.format = "zip"; add.workingDirectory = root; add.files = {"secret 日本語.txt"}; add.methodAutomatic = true; add.password = "comment secret";
        auto result = execute(add); QVERIFY2(result.success, qPrintable(result.details)); auto snapshot = listing(add.archive); const auto row = snapshot.metadata.directories[0].children.first();
        auto request = selection(snapshot, {row}, ArchiveOperation::Comment); request.comment = "no data password needed";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.entries.first().encrypted);
        snapshot = listing(add.archive); request = selection(snapshot, {snapshot.metadata.directories[0].children.first()}, ArchiveOperation::Comment); request.comment = "still unchanged encryption"; request.password = "irrelevant wrong password";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(result.entries.first().encrypted); QVERIFY(!result.details.contains(add.password));
        ArchiveRequest test; test.operation = ArchiveOperation::Test; test.archive = add.archive; test.password = add.password; QVERIFY(execute(test).success);
        test.password = "wrong"; QVERIFY(!execute(test).success);
    }
    void guiSameNameComment() {
        const auto path = ownedCopy("duplicate-comments.zip"); MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(path); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto list = window.findChild<FileList *>("fileList"); auto backend = window.findChild<SevenZipProcessBackend *>(); QVERIFY(list); QVERIFY(backend);
        auto rows = list->findItems("same.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 2);
        auto first = rows[0]->data(0, Qt::UserRole + 110).toLongLong() == 0 ? rows[0] : rows[1]; auto second = first == rows[0] ? rows[1] : rows[0];
        list->clearSelection(); second->setSelected(true); list->setCurrentItem(first, 0, QItemSelectionModel::NoUpdate);
        auto action = window.findChild<QAction *>("commentAction"); QVERIFY(action && action->isEnabled()); action->trigger(); QTRY_VERIFY(testInput(&window, "commentDialog"));
        auto dialog = testInput(&window, "commentDialog"); QCOMPARE(dialog->textValue(), QString("first comment")); dialog->setTextValue("focused first 日本語");
        QSignalSpy done(backend, &ArchiveBackend::finished); dialog->accept(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000);
        const auto result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(list->currentItem()->data(0, Qt::UserRole + 110).toLongLong(), qint64(0)); QCOMPARE(list->selectedItems().size(), 1); QCOMPARE(list->selectedItems().first()->data(0, Qt::UserRole + 110).toLongLong(), qint64(1));
        auto snapshot = listing(path); QCOMPARE(snapshot.entries[0].properties.value("Comment"), QString("focused first 日本語")); QCOMPARE(snapshot.entries[1].properties.value("Comment"), QString("second comment"));
        auto implicit = list->findItems("implicit", Qt::MatchExactly); QCOMPARE(implicit.size(), 1); list->setCurrentItem(implicit.first()); QVERIFY(!action->isEnabled()); window.close();
    }
    void normalAndFlatFolderRules() {
        const auto snapshot = listing("corrupt-duplicate.zip"); ArchiveFolderIndex index(snapshot.entries, snapshot.metadata);
        ArchiveRow implicit, empty; for (const auto &row : index.directoryRows(0, false)) { if (row.name == "implicit") implicit = row; if (row.name == "empty") empty = row; }
        QCOMPARE(implicit.archiveIndex, qint64(-1)); QVERIFY(implicit.childDirectory > 0);
        auto request = selection(snapshot, {implicit}, ArchiveOperation::Extract); auto result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(contents(request.outputDirectory + "/implicit/sub/deep.txt"), QByteArray("deep payload"));
        request = selection(snapshot, {implicit}, ArchiveOperation::Test); QVERIFY(execute(request).success);
        request = selection(snapshot, {implicit}, ArchiveOperation::Extract, 0, true); result = execute(request);
        QVERIFY(result.success); QVERIFY(result.metadata.selectionResolved); QVERIFY(result.metadata.selectedIndices.isEmpty()); QVERIFY(!QFile::exists(request.outputDirectory));
        request = selection(snapshot, {empty}, ArchiveOperation::Extract, 0, true); result = execute(request);
        QVERIFY2(result.success, qPrintable(result.details)); QVERIFY(QFileInfo(request.outputDirectory + "/empty").isDir());
        const auto sub = index.directory(implicit.childDirectory)->children.first();
        request = selection(snapshot, {sub}, ArchiveOperation::Extract, implicit.childDirectory); result = execute(request);
        QVERIFY2(result.success, qPrintable(result.details)); QCOMPARE(contents(request.outputDirectory + "/sub/deep.txt"), QByteArray("deep payload")); QVERIFY(!QFile::exists(request.outputDirectory + "/implicit"));
        request = selection(snapshot, {sub}, ArchiveOperation::Extract, implicit.childDirectory); request.selectionPreservePaths = true; result = execute(request);
        QVERIFY(result.success); QCOMPARE(contents(request.outputDirectory + "/implicit/sub/deep.txt"), QByteArray("deep payload"));
    }
    void alternateStreamSelection() {
        const auto snapshot = listing("streams.ntfs"); ArchiveFolderIndex index(snapshot.entries, snapshot.metadata);
        ArchiveRow payload; for (const auto &row : index.directoryRows(0, false)) if (row.name == "payload.txt") payload = row;
        QVERIFY(payload.alternateDirectory > 0); const auto stream = index.directory(payload.alternateDirectory)->children.first();
        auto request = selection(snapshot, {stream}, ArchiveOperation::Test, payload.alternateDirectory); auto result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(result.metadata.selectedIndices, QList<quint32>{quint32(stream.archiveIndex)});
        request.operation = ArchiveOperation::HashArchive; request.hashMethod = "SHA256"; result = execute(request); QVERIFY2(result.success, qPrintable(result.details));
        // Upstream excludes ADS from "hash for data" and reports their
        // data/name aggregate separately. Compare that to the unmodified
        // console path for this unambiguous stream; extraction below compares bytes.
        auto reference = request; reference.selectionDirectory = -1; reference.selection.clear();
        const auto expected = execute(reference); QVERIFY(expected.success);
        QString streamDigest; for (const auto &line : expected.details.split('\n')) if (line.startsWith("SHA256 for streams and names:")) streamDigest = line;
        QVERIFY(!streamDigest.isEmpty()); QVERIFY2(result.details.contains(streamDigest), qPrintable(result.details));
        request = selection(snapshot, {payload}, ArchiveOperation::Test); result = execute(request); QVERIFY(result.success); QCOMPARE(result.metadata.selectedIndices.size(), 2);
        request = selection(snapshot, {stream}, ArchiveOperation::Extract, payload.alternateDirectory); result = execute(request);
        QVERIFY2(result.success, qPrintable(result.message + result.details));
        QDirIterator files(request.outputDirectory, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories); QList<QByteArray> data; while (files.hasNext()) data << contents(files.next()); QCOMPARE(data, QList<QByteArray>{"alternate data"});
    }
    void rejectStaleOrInvalidIdentity() {
        const auto path = temporary.filePath("changed.zip"); QVERIFY(QFile::copy(fixtures + "/duplicate.zip", path)); const auto snapshot = listing(path); const auto rows = duplicates(snapshot);
        auto request = selection(snapshot, {rows[0]}, ArchiveOperation::Extract); request.selection[0].identity.item = 999999; auto result = execute(request); QVERIFY(!result.success); QVERIFY(!QFile::exists(request.outputDirectory));
        request = selection(snapshot, {rows[0]}, ArchiveOperation::Extract); request.selection[0].name = "wrong"; result = execute(request); QVERIFY(!result.success); QVERIFY(!QFile::exists(request.outputDirectory));
        request = selection(snapshot, {rows[0], rows[0]}, ArchiveOperation::Test); result = execute(request); QVERIFY(!result.success);
        request = selection(snapshot, {rows[0]}, ArchiveOperation::Extract); QFile file(path); QVERIFY(file.open(QIODevice::Append)); QCOMPARE(file.write("changed"), qint64(7)); file.close(); result = execute(request); QVERIFY(!result.success); QVERIFY(result.details.contains("selection", Qt::CaseInsensitive)); QVERIFY(!QFile::exists(request.outputDirectory));
    }
    void selectedPathsRemainGuarded() {
        const auto snapshot = listing("selected-safety.zip"); ArchiveFolderIndex index(snapshot.entries, snapshot.metadata);
        ArchiveRow good, traversal, link; for (const auto &row : index.directoryRows(0, true)) { if (row.name == "safe.txt") good = row; if (row.name == "escaped.txt") traversal = row; if (row.name == "unsafe-link") link = row; }
        auto request = selection(snapshot, {good}, ArchiveOperation::Extract, 0, true); auto result = execute(request); QVERIFY2(result.success, qPrintable(result.details)); QCOMPARE(contents(request.outputDirectory + "/safe.txt"), QByteArray("safe selected payload"));
        for (const auto &row : {traversal, link}) {
            request = selection(snapshot, {row}, ArchiveOperation::Extract, 0, true); result = execute(request); QVERIFY(!result.success); QVERIFY(result.message.contains("Unsafe"));
            // Safe links are now extracted by the original callback. It can
            // create the output root before rejecting this dangerous target.
            QVERIFY(!QFileInfo(request.outputDirectory + '/' + row.name).isSymLink()); QVERIFY(!QFile::exists(request.outputDirectory + '/' + row.name));
        }
        QVERIFY(!QFile::exists(temporary.filePath("escaped.txt"))); QVERIFY(!QFile::exists(temporary.filePath("outside")));
    }
    void encryptedJapaneseSelection() {
        const auto source = temporary.filePath("encrypted-source"); QVERIFY(QDir().mkpath(source));
        const auto name = QString::fromUtf8("日本語 space.txt"); QFile file(source + '/' + name); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("secret data"), qint64(11)); file.close();
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = temporary.filePath("selected.7z"); add.workingDirectory = source; add.files = {name}; add.password = "port-test-password"; add.encryptNames = true; add.level = 1; add.dictionary = "1m";
        auto result = execute(add); QVERIFY2(result.success, qPrintable(result.message + result.details)); const auto snapshot = listing(add.archive, add.password); QCOMPARE(snapshot.metadata.directories[0].children.size(), 1);
        auto request = selection(snapshot, {snapshot.metadata.directories[0].children.first()}, ArchiveOperation::Extract); request.password = add.password; result = execute(request);
        QVERIFY2(result.success, qPrintable(result.details)); QCOMPARE(contents(request.outputDirectory + '/' + name), QByteArray("secret data")); QVERIFY(!result.details.contains(add.password));
        request.operation = ArchiveOperation::Test; request.password = "wrong-password"; result = execute(request); QVERIFY(!result.success); QVERIFY(result.passwordRequired); QVERIFY(!result.details.contains(request.password));
        request.password = add.password; result = execute(request); QVERIFY(result.success);
    }
    void sameNameUpdate_data() {
        QTest::addColumn<int>("chosen"); QTest::addColumn<bool>("rename");
        for (int chosen = 0; chosen < 2; ++chosen) for (bool rename : {false, true})
            QTest::newRow(qPrintable(QString("%1-%2").arg(rename ? "rename" : "delete").arg(chosen))) << chosen << rename;
    }
    void sameNameUpdate() {
        QFETCH(int, chosen); QFETCH(bool, rename);
        const auto path = ownedCopy("duplicate.zip"); const auto before = listing(path); const auto rows = duplicates(before);
        auto request = selection(before, {rows[chosen]}, rename ? ArchiveOperation::Rename : ArchiveOperation::Delete);
        if (rename) request.files << QString::fromUtf8("日本語 renamed.txt");
        const auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        const auto after = listing(path); const auto remaining = duplicates(after); QCOMPARE(remaining.size(), 1);
        auto extracted = selection(after, {remaining.first()}, ArchiveOperation::Extract); QVERIFY(execute(extracted).success);
        QCOMPARE(contents(extracted.outputDirectory + "/same.txt"), chosen == 0 ? QByteArray("a different second payload") : QByteArray("one"));
        if (rename) {
            ArchiveRow changed; for (const auto &row : after.metadata.directories[0].children) if (row.name == request.files[1]) changed = row;
            QVERIFY(changed.archiveIndex >= 0); extracted = selection(after, {changed}, ArchiveOperation::Extract); QVERIFY(execute(extracted).success);
            QCOMPARE(contents(extracted.outputDirectory + '/' + changed.name), chosen == 0 ? QByteArray("one") : QByteArray("a different second payload"));
        }
        ArchiveRequest test; test.archive = path; test.operation = ArchiveOperation::Test; QVERIFY(execute(test).success);
    }
    void updateRetainsUnrelatedCorruptPayload() {
        const auto path = ownedCopy("corrupt-duplicate.zip"); auto snapshot = listing(path); auto rows = duplicates(snapshot);
        auto request = selection(snapshot, {rows[0]}, ArchiveOperation::Rename); request.files << "good.txt";
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        snapshot = listing(path); rows = duplicates(snapshot); QCOMPARE(rows.size(), 1);
        QVERIFY(!execute(selection(snapshot, rows, ArchiveOperation::Test)).success);
        request = selection(snapshot, rows, ArchiveOperation::Delete); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        ArchiveRequest test; test.archive = path; test.operation = ArchiveOperation::Test; QVERIFY(execute(test).success);
    }
    void implicitFlatUpdatePolicies() {
        auto path = ownedCopy("duplicate.zip"); auto snapshot = listing(path); ArchiveRow implicit;
        for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "implicit") implicit = row;
        auto request = selection(snapshot, {implicit}, ArchiveOperation::Delete, 0, true); const auto original = contents(path);
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(contents(path), original);
        request = selection(snapshot, {implicit}, ArchiveOperation::Rename, 0, true); request.files[0] = "implicit"; request.files << "renamed";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); snapshot = listing(path);
        ArchiveRow renamed; for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "renamed") renamed = row;
        QVERIFY(renamed.childDirectory > 0);
        request = selection(snapshot, {renamed}, ArchiveOperation::Delete); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        snapshot = listing(path); for (const auto &entry : snapshot.entries) QVERIFY(!entry.path.startsWith("renamed"));
    }
    void nativeUpdatesAcrossWriters_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("prefix");
        for (const auto &format : {"7z", "zip", "tar", "wim"}) QTest::newRow(format) << QString(format) << false;
        for (const auto &format : {"7z", "zip"}) QTest::newRow(qPrintable(QString(format) + "-prefix")) << QString(format) << true;
    }
    void nativeUpdatesAcrossWriters() {
        QFETCH(QString, format); QFETCH(bool, prefix);
        const auto root = temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); QVERIFY(QDir().mkpath(root + "/folder/sub")); QVERIFY(QDir().mkpath(root + "/empty"));
        for (const auto &name : {QString("root.txt"), QString::fromUtf8("folder/sub/日本語 space.txt")}) { QFile file(root + '/' + name); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(name.toUtf8()), qint64(name.toUtf8().size())); }
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = root + "/archive." + format; add.format = format; add.workingDirectory = root; add.files = {"root.txt", "folder", "empty"}; add.useArchiveDefaults = true;
        auto result = execute(add); QVERIFY2(result.success, qPrintable(result.message + result.details));
        const QByteArray stub("PORT-OWNED-PREFIX\n");
        if (prefix) { const auto bytes = contents(add.archive); QFile file(add.archive); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(stub + bytes), qint64(stub.size() + bytes.size())); }
        auto snapshot = listing(add.archive); ArchiveRow file;
        for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "root.txt") file = row;
        QVERIFY(file.archiveIndex >= 0); auto request = selection(snapshot, {file}, ArchiveOperation::Rename); request.files << "renamed.txt";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        snapshot = listing(add.archive); ArchiveRow folder;
        for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "folder") folder = row;
        QVERIFY(folder.childDirectory > 0); request = selection(snapshot, {folder}, ArchiveOperation::Rename, 0, true); request.files[0] = "folder"; request.files << "renamed-folder";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        snapshot = listing(add.archive); for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "renamed-folder") folder = row;
        QCOMPARE(folder.name, QString("renamed-folder")); request = selection(snapshot, {folder}, ArchiveOperation::Delete); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        ArchiveRequest test; test.archive = add.archive; test.operation = ArchiveOperation::Test; QVERIFY(execute(test).success);
        if (prefix) QVERIFY(contents(add.archive).startsWith(stub));
        snapshot = listing(add.archive); for (const auto &entry : snapshot.entries) QVERIFY(!entry.path.startsWith("renamed-folder"));
    }
    void singleStreamUpdates_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("extension"); QTest::addColumn<bool>("deleting");
        for (const auto &pair : {qMakePair(QString("gzip"), QString("gz")), qMakePair(QString("bzip2"), QString("bz2")), qMakePair(QString("xz"), QString("xz"))})
            for (bool deleting : {false, true}) QTest::newRow(qPrintable(pair.first + (deleting ? "-delete" : "-rename"))) << pair.first << pair.second << deleting;
    }
    void singleStreamUpdates() {
        QFETCH(QString, format); QFETCH(QString, extension); QFETCH(bool, deleting);
        QTemporaryDir fixture; const auto file = fixture.filePath("日本語 space.txt");
        const auto payload = QByteArray("independent official stream update fixture\n").repeated(1024);
        { QFile output(file); QVERIFY(output.open(QIODevice::WriteOnly)); QCOMPARE(output.write(payload), qint64(payload.size())); }
        const auto source = fixture.filePath("archive." + extension); QProcess original;
        original.setWorkingDirectory(fixture.path()); original.start(engine, {"a", "-t" + format, "--", source, QFileInfo(file).fileName()});
        QVERIFY(original.waitForFinished(30000)); QCOMPARE(original.exitCode(), 0);
        QVERIFY(QDir().mkpath(fixture.filePath("official"))); QVERIFY(QDir().mkpath(fixture.filePath("application")));
        const auto baseline = fixture.filePath("official/archive." + extension), path = fixture.filePath("application/archive." + extension);
        QVERIFY(QFile::copy(source, baseline)); QVERIFY(QFile::copy(source, path));
        const auto before = contents(path); const auto snapshot = listing(path); QCOMPARE(snapshot.metadata.directories[0].children.size(), 1);
        const auto row = snapshot.metadata.directories[0].children[0]; const QString renamed = "renamed space.txt";
        QStringList arguments{deleting ? "d" : "rn", "--", baseline, row.name}; if (!deleting) arguments << renamed;
        original.start(engine, arguments); QVERIFY(original.waitForFinished(30000));
        const auto officialExit = original.exitCode(); QCOMPARE(original.exitStatus(), QProcess::NormalExit);
        auto request = selection(snapshot, {row}, deleting ? ArchiveOperation::Delete : ArchiveOperation::Rename); if (!deleting) request.files << renamed;
        const auto result = execute(request); QCOMPARE(result.success, officialExit == 0); QCOMPARE(result.target, path);
        QVERIFY2(result.success || result.exitCode > 0, qPrintable(result.message + result.details));
        QCOMPARE(contents(path), contents(baseline));
        QVERIFY(QDir().mkpath(fixture.filePath("gui"))); const auto guiArchive = fixture.filePath("gui/archive." + extension); QVERIFY(QFile::copy(source, guiArchive));
        MainWindow window(engine); window.show(); window.openPath(guiArchive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto list = window.findChild<FileList *>("fileList"); QVERIFY(list); auto rows = list->findItems(row.name, Qt::MatchExactly); QCOMPARE(rows.size(), 1);
        list->setCurrentItem(rows[0]); rows[0]->setSelected(true); QTimer dialogs; dialogs.setInterval(10);
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue(renamed); input->accept(); return; }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) {
                if (auto yes = box->button(QMessageBox::Yes)) yes->click(); else box->accept(); return;
            }
        }); dialogs.start(); auto backend = window.findChild<SevenZipProcessBackend *>(); QVERIFY(backend); QSignalSpy done(backend, &ArchiveBackend::finished);
        auto action = window.findChild<QAction *>(deleting ? "deleteAction" : "renameAction"); QVERIFY(action && action->isEnabled()); action->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        ArchiveResult guiResult; for (const auto &args : done) if (qvariant_cast<ArchiveResult>(args[0]).operation == request.operation) guiResult = qvariant_cast<ArchiveResult>(args[0]);
        QCOMPARE(guiResult.operation, request.operation); QCOMPARE(guiResult.success, result.success); QCOMPARE(contents(guiArchive), contents(baseline)); window.close();
        if (officialExit != 0) {
            QCOMPARE(contents(path), before); QCOMPARE(result.exitCode, officialExit); QVERIFY(!result.details.isEmpty());
        } else {
            ArchiveRequest test; test.operation = ArchiveOperation::Test; test.archive = path; QVERIFY(execute(test).success);
            original.start(engine, {"x", "-so", "-bso0", "-bsp0", "--", path}); QVERIFY(original.waitForFinished(30000)); QCOMPARE(original.exitCode(), 0);
            QCOMPARE(original.readAllStandardOutput(), deleting ? QByteArray() : payload);
            if (!deleting) {
                QCOMPARE(result.itemIndexMap, QList<qint64>{0});
                const auto updated = listing(path); QCOMPARE(updated.metadata.directories[0].children[0].name, format == "gzip" ? renamed : row.name);
            }
        }
    }
    void nestedParentRenameWriteBack_data() { QTest::addColumn<bool>("ancestor"); QTest::newRow("child-name") << false; QTest::newRow("ancestor-folder") << true; }
    void nestedParentRenameWriteBack() {
        QFETCH(bool, ancestor);
        QTemporaryDir fixture; const auto input = fixture.filePath("payload.txt"), inner = fixture.filePath("inner.zip"), outer = fixture.filePath("outer.7z");
        { QFile file(input); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("nested rename payload"), qint64(21)); }
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = inner; add.format = "zip"; add.useArchiveDefaults = true; add.workingDirectory = fixture.path(); add.files = {"payload.txt"}; QVERIFY(execute(add).success);
        if (ancestor) { QVERIFY(QDir().mkpath(fixture.filePath("folder/sub"))); QVERIFY(QFile::copy(inner, fixture.filePath("folder/sub/inner.zip"))); }
        add.archive = outer; add.format = "7z"; add.files = {ancestor ? "folder" : "inner.zip"}; QVERIFY(execute(add).success);
        MainWindow window(engine); window.show(); window.openPath(outer); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        if (ancestor) window.openPath(outer + "/folder/sub/");
        auto address = window.findChild<QLineEdit *>("currentPath"); QVERIFY(address);
        auto list = window.findChild<FileList *>("fileList"); QVERIFY(list); auto rows = list->findItems("inner.zip", Qt::MatchExactly); QCOMPARE(rows.size(), 1);
        list->setCurrentItem(rows[0]); rows[0]->setSelected(true); window.findChild<QAction *>("insideAction")->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(list->findItems("payload.txt", Qt::MatchExactly).size(), 1);
        window.findChild<QAction *>("twoPanelsAction")->trigger(); auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second);
        second->openPath(outer); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto other = second->findChild<FileList *>("fileList"); QVERIFY(other); rows = other->findItems(ancestor ? "folder" : "inner.zip", Qt::MatchExactly); QCOMPARE(rows.size(), 1);
        other->setCurrentItem(rows[0]); rows[0]->setSelected(true);
        QString rename = ancestor ? "renamed-folder" : "renamed-inner.zip", errors; QTimer dialogs; dialogs.setInterval(10);
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue(rename); input->accept(); return; }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) {
                if (box->text().contains("modified")) box->button(QMessageBox::Yes)->click(); else { errors += box->text(); box->accept(); } return;
            }
        }); dialogs.start(); second->findChild<QAction *>("renameAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QCOMPARE(other->findItems(rename, Qt::MatchExactly).size(), 1);
        const auto renamedChild = ancestor ? QString("renamed-folder/sub/inner.zip") : QString("renamed-inner.zip");
        QCOMPARE(address->text(), outer + '/' + renamedChild + '/');
        list->setFocus(); rows = list->findItems("payload.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 1); list->clearSelection(); list->setCurrentItem(rows[0]); rows[0]->setSelected(true);
        rename = "edited-child.txt"; window.findChild<QAction *>("renameAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QCOMPARE(list->findItems("edited-child.txt", Qt::MatchExactly).size(), 1); window.findChild<QAction *>("upAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors)); QCOMPARE(list->findItems(ancestor ? "inner.zip" : "renamed-inner.zip", Qt::MatchExactly).size(), 1);
        QCOMPARE(address->text(), outer + '/' + (ancestor ? "renamed-folder/sub/" : ""));
        ArchiveRequest extract; extract.operation = ArchiveOperation::Extract; extract.archive = outer; extract.outputDirectory = fixture.filePath("verify-outer"); extract.overwriteMode = "overwrite"; extract.pathMode = "full";
        auto result = execute(extract); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!QFile::exists(extract.outputDirectory + (ancestor ? "/folder/sub/inner.zip" : "/inner.zip")));
        extract.archive = extract.outputDirectory + '/' + renamedChild; extract.outputDirectory = fixture.filePath("verify-inner"); result = execute(extract); QVERIFY2(result.success, qPrintable(result.message + result.details));
        QCOMPARE(contents(extract.outputDirectory + "/edited-child.txt"), QByteArray("nested rename payload")); QVERIFY(!QFile::exists(extract.outputDirectory + "/payload.txt")); window.close();
    }
    void updateRejectsStaleSelectionAndCollision() {
        const auto path = ownedCopy("duplicate.zip"); const auto snapshot = listing(path); const auto rows = duplicates(snapshot); const auto before = contents(path);
        auto request = selection(snapshot, {rows[0]}, ArchiveOperation::Rename); request.files << "empty";
        auto result = execute(request); QVERIFY(!result.success); QCOMPARE(contents(path), before);
        request = selection(snapshot, {rows[0]}, ArchiveOperation::Delete); request.selection[0].identity.item = 999999;
        result = execute(request); QVERIFY(!result.success); QCOMPARE(contents(path), before);
        QFile file(path); QVERIFY(file.open(QIODevice::Append)); QCOMPARE(file.write("changed"), qint64(7)); file.close(); const auto changed = contents(path);
        request = selection(snapshot, {rows[0]}, ArchiveOperation::Delete); result = execute(request); QVERIFY(!result.success); QVERIFY(result.message.contains("selection")); QCOMPARE(contents(path), changed);
    }
    void paxRenameFollowsOfficialDefaults() {
        const auto path = ownedCopy("pax.tar"); const auto snapshot = listing(path); ArchiveRow folder;
        for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "long") folder = row;
        QVERIFY(folder.childDirectory > 0); const auto row = snapshot.metadata.directories[folder.childDirectory].children.first();
        auto request = selection(snapshot, {row}, ArchiveOperation::Rename, folder.childDirectory); request.files << "long/renamed.txt";
        auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        const auto after = listing(path); QCOMPARE(after.entries.size(), 2);
        QStringList paths; for (const auto &entry : after.entries) paths << entry.path;
        QVERIFY(paths.contains("after.txt")); QVERIFY(paths.contains("long/renamed.txt"));
        ArchiveRequest extract; extract.operation = ArchiveOperation::Extract; extract.archive = path; extract.outputDirectory = temporary.filePath("pax-restored"); extract.overwriteMode = "overwrite"; result = execute(extract); QVERIFY(result.success);
        QCOMPARE(contents(extract.outputDirectory + "/long/renamed.txt"), QByteArray("PAX long path")); QCOMPARE(contents(extract.outputDirectory + "/after.txt"), QByteArray("unchanged PAX entry"));
    }
    void encryptedNativeUpdates_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("headers");
        QTest::newRow("7z-data") << QString("7z") << false; QTest::newRow("7z-header") << QString("7z") << true;
        QTest::newRow("zip-AES") << QString("zip") << false;
    }
    void encryptedNativeUpdates() {
        QFETCH(QString, format); QFETCH(bool, headers);
        const auto root = temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); QVERIFY(QDir().mkpath(root));
        for (const auto &name : {"first.txt", "second.txt"}) { QFile file(root + '/' + name); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("secret shared solid data"), qint64(24)); }
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = root + "/encrypted." + format; add.format = format; add.workingDirectory = root; add.files = {"first.txt", "second.txt"}; add.methodAutomatic = true; add.password = "native update secret"; add.encryptNames = headers;
        auto result = execute(add); QVERIFY2(result.success, qPrintable(result.message + result.details)); auto snapshot = listing(add.archive, add.password);
        ArchiveRow first; for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "first.txt") first = row;
        auto request = selection(snapshot, {first}, ArchiveOperation::Rename); request.files << "renamed.txt";
        // Packed ZIP payload and unencrypted 7z headers can be renamed without
        // an unrelated data password; encrypted headers require it to open.
        if (headers) request.password = add.password;
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(!result.details.contains(add.password));
        snapshot = listing(add.archive, headers ? add.password : QString());
        ArchiveRow second; for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "second.txt") second = row;
        request = selection(snapshot, {second}, ArchiveOperation::Delete); request.password = "wrong update secret"; const auto before = contents(add.archive);
        result = execute(request);
        if (format == "7z") { QVERIFY(!result.success); QVERIFY(result.passwordRequired); QCOMPARE(contents(add.archive), before); }
        else { QVERIFY2(result.success, qPrintable(result.message + result.details)); }
        if (format == "7z") { request.password = add.password; result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); }
        QVERIFY(!result.details.contains(add.password)); ArchiveRequest test; test.archive = add.archive; test.operation = ArchiveOperation::Test; test.password = add.password; result = execute(test); QVERIFY2(result.success, qPrintable(result.details));
        snapshot = listing(add.archive, headers ? add.password : QString()); QCOMPARE(snapshot.entries.size(), 1); QCOMPARE(snapshot.entries.first().path, QString("renamed.txt"));
    }
    void guiAncestorFolderRestoration_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("flat");
        for (const auto &format : {"zip", "7z"}) for (const bool flat : {false, true})
            QTest::newRow(qPrintable(QString(format) + (flat ? "-flat" : "-normal"))) << QString(format) << flat;
    }
    void guiAncestorFolderRestoration() {
        QFETCH(QString, format); QFETCH(bool, flat);
        QString path;
        if (format == "zip") path = ownedCopy("duplicate.zip");
        else {
            const auto root = temporary.filePath(QUuid::createUuid().toString(QUuid::WithoutBraces)); QVERIFY(QDir().mkpath(root + "/implicit/sub"));
            QFile file(root + "/implicit/sub/deep.txt"); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write("deep payload"), qint64(12)); file.close();
            ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = root + "/tree.7z"; add.format = "7z"; add.workingDirectory = root; add.files = {"implicit"};
            const auto result = execute(add); QVERIFY2(result.success, qPrintable(result.message + result.details)); path = add.archive;
        }
        MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        window.openPath(path + "/implicit/sub/"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto list = window.findChild<FileList *>("fileList"); auto address = window.findChild<QLineEdit *>("currentPath"); QVERIFY(list && address);
        if (flat) window.findChild<QAction *>("flatAction")->trigger();
        auto rows = list->findItems("deep.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 1); list->clearSelection(); list->setCurrentItem(rows[0]); rows[0]->setSelected(true);
        window.findChild<QAction *>("twoPanelsAction")->trigger(); auto second = window.findChild<MainWindow *>("secondPanel"); QVERIFY(second);
        second->openPath(path); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        if (flat) second->findChild<QAction *>("flatAction")->trigger();
        auto other = second->findChild<FileList *>("fileList"); QVERIFY(other);
        rows = other->findItems("implicit", Qt::MatchExactly); QCOMPARE(rows.size(), 1);
        if (format == "zip") QCOMPARE(rows[0]->data(0, Qt::UserRole + 110).toLongLong(), qint64(-1));
        other->clearSelection(); other->setCurrentItem(rows[0]); rows[0]->setSelected(true);
        QString errors; QTimer dialogs; dialogs.setInterval(10);
        connect(&dialogs, &QTimer::timeout, &window, [&] {
            for (auto input : testInputs(&window)) if (input->isVisible()) { input->setTextValue("renamed ancestor"); input->accept(); return; }
            for (auto box : window.findChildren<QMessageBox *>()) if (box->isVisible()) {
                if (box->standardButtons().testFlag(QMessageBox::Yes)) box->button(QMessageBox::Yes)->click(); else { errors += box->text(); box->accept(); } return;
            }
        }); dialogs.start(); second->findChild<QAction *>("renameAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors)); QCOMPARE(address->text(), path + "/renamed ancestor/sub/");
        QCOMPARE(list->selectedItems().size(), 1); QCOMPARE(list->selectedItems()[0]->text(0), QString("deep.txt")); QVERIFY(list->currentItem()); QCOMPARE(list->currentItem()->text(0), QString("deep.txt"));
        QCOMPARE(other->selectedItems().size(), 1); QCOMPARE(other->selectedItems()[0]->text(0), QString("renamed ancestor"));
        QVERIFY(other->currentItem()); QCOMPARE(other->currentItem()->text(0), QString("renamed ancestor"));
        // Ordinary Delete removes the folder; Flat Delete of an implicit
        // folder intentionally has the official no-descendant policy.
        if (flat) second->findChild<QAction *>("flatAction")->trigger();
        rows = other->findItems("renamed ancestor", Qt::MatchExactly); QCOMPARE(rows.size(), 1); other->clearSelection(); other->setCurrentItem(rows[0]); rows[0]->setSelected(true);
        second->findChild<QAction *>("deleteAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        QVERIFY2(errors.isEmpty(), qPrintable(errors)); QCOMPARE(address->text(), path + '/'); QCOMPARE(list->findItems("deep.txt", Qt::MatchExactly).size(), 0);
        dialogs.stop(); window.close();
    }
    void guiSameNameRenameAndDelete() {
        const auto path = ownedCopy("duplicate.zip"); MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(path); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto list = window.findChild<FileList *>("fileList"); auto backend = window.findChild<SevenZipProcessBackend *>(); QVERIFY(list); QVERIFY(backend);
        auto rows = list->findItems("same.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 2);
        auto first = rows[0]->data(0, Qt::UserRole + 110).toLongLong() == 0 ? rows[0] : rows[1]; list->clearSelection(); list->setCurrentItem(first); first->setSelected(true);
        auto rename = window.findChild<QAction *>("renameAction"); auto remove = window.findChild<QAction *>("deleteAction"); QVERIFY(rename); QVERIFY(remove); QTRY_VERIFY(rename->isEnabled()); QVERIFY(remove->isEnabled());
        QSignalSpy done(backend, &ArchiveBackend::finished); QTimer respond, limit; respond.setInterval(10); limit.setSingleShot(true);
        connect(&limit, &QTimer::timeout, &window, [&] { for (auto widget : QApplication::topLevelWidgets()) if (auto dialog = qobject_cast<QDialog *>(widget)) dialog->reject(); });
        connect(&respond, &QTimer::timeout, &window, [&] {
            if (auto dialog = testInput(&window)) { dialog->setTextValue("GUI renamed.txt"); dialog->accept(); }
            for (auto widget : QApplication::topLevelWidgets())
                if (auto box = qobject_cast<QMessageBox *>(widget); box && box->isVisible() && box->standardButtons().testFlag(QMessageBox::Yes)) box->button(QMessageBox::Yes)->click();
        }); respond.start(); limit.start(5000); rename->trigger(); limit.stop(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 15000); auto result = qvariant_cast<ArchiveResult>(done.first()[0]); QVERIFY2(result.success, qPrintable(result.message + result.details));
        if (auto dialog = window.findChild<ProgressDialog *>()) dialog->close(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); rows = list->findItems("same.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 1);
        list->clearSelection(); list->setCurrentItem(rows.first()); rows.first()->setSelected(true); done.clear(); QTRY_VERIFY(remove->isEnabled()); limit.start(5000); remove->trigger(); limit.stop(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 15000); result = qvariant_cast<ArchiveResult>(done.first()[0]); QVERIFY2(result.success, qPrintable(result.message + result.details));
        if (auto dialog = window.findChild<ProgressDialog *>()) dialog->close(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); respond.stop(); QCOMPARE(list->findItems("same.txt", Qt::MatchExactly).size(), 0); QCOMPARE(list->findItems("GUI renamed.txt", Qt::MatchExactly).size(), 1);
        const auto snapshot = listing(path); auto request = selection(snapshot, {snapshot.metadata.directories[0].children.last()}, ArchiveOperation::Test); QVERIFY(execute(request).success);
    }
    void multiImageWimNativeSelection() {
        const auto root = temporary.filePath("native-update-images"); QVERIFY(QDir().mkpath(root + "/1/dest")); QVERIFY(QDir().mkpath(root + "/2/dest"));
        for (const auto &name : {"1/dest/one.txt", "2/dest/two.txt"}) { QFile file(root + '/' + name); QVERIFY(file.open(QIODevice::WriteOnly)); QCOMPARE(file.write(name), qint64(QByteArray(name).size())); }
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = root + "/images.wim"; add.format = "wim"; add.parameters = "is=on"; add.workingDirectory = root; add.files = {"1", "2"};
        auto result = execute(add); QVERIFY2(result.success, qPrintable(result.message + result.details));
        auto snapshot = listing(add.archive); ArchiveRow image;
        for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "2") image = row;
        QVERIFY(image.childDirectory > 0); const auto dest = snapshot.metadata.directories[image.childDirectory].children.first(); QVERIFY(dest.childDirectory > 0);
        const auto payload = snapshot.metadata.directories[dest.childDirectory].children.first(); auto request = selection(snapshot, {payload}, ArchiveOperation::Rename, dest.childDirectory); request.files << "2/dest/renamed.txt";
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); snapshot = listing(add.archive); QCOMPARE(snapshot.properties.value("Images"), QString("2"));
        for (const auto &row : snapshot.metadata.directories[0].children) if (row.name == "2") image = row;
        const auto folder = snapshot.metadata.directories[image.childDirectory].children.first(); request = selection(snapshot, {folder}, ArchiveOperation::Delete, image.childDirectory);
        result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        ArchiveRequest extract; extract.operation = ArchiveOperation::Extract; extract.archive = add.archive; extract.outputDirectory = root + "/restored"; extract.overwriteMode = "overwrite";
        result = execute(extract); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(contents(root + "/restored/1/dest/one.txt"), QByteArray("1/dest/one.txt")); QVERIFY(!QFile::exists(root + "/restored/2/dest/renamed.txt"));
    }
    void nativeUpdatePauseCancelRetainsOriginal() {
        const auto root = temporary.filePath("cancel-native-update"); QVERIFY(QDir().mkpath(root)); QFile random("/dev/urandom"); QVERIFY(random.open(QIODevice::ReadOnly));
        const auto payload = random.read(16 * 1024 * 1024); QCOMPARE(payload.size(), qsizetype(16 * 1024 * 1024));
        QFile large(root + "/zzz-large.bin"); QVERIFY(large.open(QIODevice::WriteOnly)); for (int i = 0; i < 2; ++i) QCOMPARE(large.write(payload), qint64(payload.size())); large.close();
        QFile removed(root + "/aaa-remove.bin"); QVERIFY(removed.open(QIODevice::WriteOnly)); QCOMPARE(removed.write(payload.left(512 * 1024)), qint64(512 * 1024)); removed.close();
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = root + "/cancel.7z"; add.workingDirectory = root; add.files = {"aaa-remove.bin", "zzz-large.bin"}; add.method = "BZip2"; add.level = 1; add.password = "cancel native secret";
        auto result = execute(add); QVERIFY2(result.success, qPrintable(result.message + result.details)); const auto snapshot = listing(add.archive);
        ArchiveRow row; for (const auto &item : snapshot.metadata.directories[0].children) if (item.name == "aaa-remove.bin") row = item;
        auto request = selection(snapshot, {row}, ArchiveOperation::Delete); request.password = add.password; const auto before = QCryptographicHash::hash(contents(add.archive), QCryptographicHash::Sha256);
        SevenZipProcessBackend backend(engine); QSignalSpy done(&backend, &ArchiveBackend::finished); bool paused = false;
        connect(&backend, &ArchiveBackend::progressDetails, &backend, [&](ArchiveProgress progress) { if (!paused && progress.mode == "compress" && backend.canPause()) paused = backend.setPaused(true); });
        backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(paused || !done.isEmpty(), 15000); QVERIFY(paused); QVERIFY(done.isEmpty());
        int ticks = 0; QTimer heartbeat; heartbeat.setInterval(10); connect(&heartbeat, &QTimer::timeout, [&] { ++ticks; }); heartbeat.start(); QTest::qWait(150); QVERIFY(ticks >= 5); QVERIFY(done.isEmpty());
        backend.cancel(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000); result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY(result.cancelled); QVERIFY(!result.success); QCOMPARE(QCryptographicHash::hash(contents(add.archive), QCryptographicHash::Sha256), before);
        paused = true; done.clear(); backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 15000); result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY2(result.success, qPrintable(result.message + result.details));
        request.selectionDirectory = -1; request.selection.clear(); request.files.clear(); request.operation = ArchiveOperation::Test; done.clear(); backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 15000); QVERIFY(qvariant_cast<ArchiveResult>(done.last()[0]).success);
    }
    void selectedExtractionPauseCancelAndReuse() {
        const auto source = temporary.filePath("large-source"); QVERIFY(QDir().mkpath(source));
        QFile random("/dev/urandom"); QVERIFY(random.open(QIODevice::ReadOnly)); const auto payload = random.read(16 * 1024 * 1024); QCOMPARE(payload.size(), qsizetype(16 * 1024 * 1024));
        QFile file(source + "/large.bin"); QVERIFY(file.open(QIODevice::WriteOnly)); for (int i = 0; i < 4; ++i) QCOMPARE(file.write(payload), qint64(payload.size())); file.close();
        ArchiveRequest add; add.operation = ArchiveOperation::Add; add.archive = temporary.filePath("large.7z"); add.workingDirectory = source; add.files = {"large.bin"}; add.level = 1; add.method = "BZip2";
        QVERIFY(execute(add).success); const auto snapshot = listing(add.archive); auto request = selection(snapshot, {snapshot.metadata.directories[0].children.first()}, ArchiveOperation::Extract);
        SevenZipProcessBackend backend(engine); QSignalSpy done(&backend, &ArchiveBackend::finished); bool paused = false; QStringList modes;
        connect(&backend, &ArchiveBackend::progressDetails, &backend, [&](ArchiveProgress progress) {
            modes << progress.mode;
            // Initial extract telemetry precedes GetStream; cancelling there
            // correctly creates no file. Exercise cancellation after decoder
            // output starts, matching the stock comparison below.
            if (!paused && progress.mode == "extract" && progress.processed().value_or(0) > 0 && backend.canPause()) paused = backend.setPaused(true);
        });
        backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(paused || !done.isEmpty(), 10000);
        QVERIFY2(paused, qPrintable("Progress modes: " + modes.join(',') + '\n' + (done.isEmpty() ? QString("No extraction progress received.") : qvariant_cast<ArchiveResult>(done.last()[0]).message + qvariant_cast<ArchiveResult>(done.last()[0]).details))); QVERIFY(backend.busy()); QVERIFY(done.isEmpty());
        int heartbeats = 0; QTimer timer; timer.setInterval(10); connect(&timer, &QTimer::timeout, [&] { ++heartbeats; }); timer.start(); QTest::qWait(150); QVERIFY(heartbeats >= 5); QVERIFY(done.isEmpty());
        backend.cancel(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000); const auto result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY(result.cancelled); QVERIFY(!result.success); QVERIFY(!backend.busy()); QCOMPARE(result.exitCode, 255);
        // Original CloseArc retains the open stream on Cancel, including an
        // empty file if decoding had not started. Verify it with untouched 7zz.
        const auto officialOutput = temporary.filePath("official-cancel");
        QProcess original; original.start(engine, {"x", "-aoa", "-o" + officialOutput, "--", add.archive}); QVERIFY(original.waitForStarted(10000));
        QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(officialOutput + "/large.bin") || original.state() == QProcess::NotRunning, 10000);
        QVERIFY(original.state() == QProcess::Running); QVERIFY(::kill(pid_t(original.processId()), SIGSTOP) == 0);
        QVERIFY(::kill(pid_t(original.processId()), SIGTERM) == 0); QVERIFY(::kill(pid_t(original.processId()), SIGCONT) == 0);
        QSignalSpy originalDone(&original, &QProcess::finished); if (originalDone.isEmpty()) QVERIFY(originalDone.wait(10000));
        QCOMPARE(original.exitStatus(), QProcess::NormalExit); QCOMPARE(original.exitCode(), 255);
        for (const auto &root : {request.outputDirectory, officialOutput}) {
            QFile partial(root + "/large.bin"), complete(source + "/large.bin"); QVERIFY(partial.open(QIODevice::ReadOnly)); QVERIFY(complete.open(QIODevice::ReadOnly));
            QVERIFY(partial.size() < complete.size()); const auto bytes = partial.readAll(); QCOMPARE(bytes, complete.read(bytes.size()));
        }
        request.operation = ArchiveOperation::Test; done.clear(); backend.start(request); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 10000); QVERIFY(qvariant_cast<ArchiveResult>(done.last()[0]).success);
    }
    void guiToolbarTestUsesSelectedRow() {
        MainWindow window(engine); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); window.openPath(fixtures + "/corrupt-duplicate.zip"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto list = window.findChild<FileList *>("fileList"); QVERIFY(list); auto rows = list->findItems("same.txt", Qt::MatchExactly); QCOMPARE(rows.size(), 2);
        QTreeWidgetItem *good = nullptr; for (auto row : rows) if (row->data(0, Qt::UserRole + 110).toLongLong() == 0) good = row; QVERIFY(good);
        list->clearSelection(); list->setCurrentItem(good); good->setSelected(true); auto backend = window.findChild<SevenZipProcessBackend *>(); QVERIFY(backend); QSignalSpy done(backend, &ArchiveBackend::finished);
        // Upstream archive-internal context menus contain the File menu, not
        // toolbar Add/Extract/Test commands. Exercise the genuine Test button.
        const auto button = window.findChild<QToolButton *>("testButton"); QVERIFY(button); QVERIFY(button->isEnabled());
        QTest::mouseClick(button, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 15000); auto result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY2(result.success, qPrintable(result.details)); QCOMPARE(result.metadata.selectedIndices, QList<quint32>{0});
        if (auto dialog = window.findChild<ProgressDialog *>()) dialog->close(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto bad = rows[0] == good ? rows[1] : rows[0]; list->clearSelection(); list->setCurrentItem(bad); bad->setSelected(true); done.clear(); window.findChild<QAction *>("testAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!done.isEmpty(), 15000); result = qvariant_cast<ArchiveResult>(done.last()[0]); QVERIFY(!result.success); QCOMPARE(result.exitCode, 2);
        if (auto dialog = window.findChild<ProgressDialog *>()) dialog->close(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(window.findChild<QAction *>("testAction")->isEnabled());
    }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English));
    if (argc < 3) return 1; engine = QString::fromLocal8Bit(argv[1]); fixtures = QString::fromLocal8Bit(argv[2]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs); QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("AgentSelection"); app.setQuitOnLastWindowClosed(false);
    AgentSelectionTests tests; QList<char *> args{argv[0]}; for (int i = 3; i < argc; ++i) args << argv[i]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "agent-selection.moc"
