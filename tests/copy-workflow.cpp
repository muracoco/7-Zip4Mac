// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "CopyDialog.h"
#include "FileToolDialogs.h"
#include "PanelDisplay.h"
#include "PortStyle.h"
#include "ResourceDialogs.h"
#include "UiLanguage.h"
#include "OverwriteDialog.h"
#include "progress-dialog-driver.h"
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QProcess>
#include <QStandardPaths>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTest>
#include <sys/stat.h>
static QString executable;
static bool write(QString path, QByteArray bytes = "owned Unicode 日本語 payload") { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static FileList *list(MainWindow &window) {
    for (auto view : window.findChildren<FileList *>("fileList")) {
        auto parent = view->parentWidget(); while (parent && !qobject_cast<MainWindow *>(parent)) parent = parent->parentWidget(); if (parent == &window) return view;
    } return nullptr;
}
static QTreeWidgetItem *find(MainWindow &window, QString name) { for (int i = 0; i < list(window)->topLevelItemCount(); ++i) if (panelItemName(list(window)->topLevelItem(i)) == name) return list(window)->topLevelItem(i); return nullptr; }
static bool select(MainWindow &window, QString name, bool clear = true) { auto item = find(window, name); if (!item) return false; if (clear) list(window)->clearSelection(); list(window)->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate); list(window)->setItemSelected(item, true); list(window)->setFocus(); return true; }
class CopyResponder : public QObject {
    QTimer timer; MainWindow &window;
public:
    QString target, initial, label, summary, overwriteButton = "overwriteYes"; bool accepted = true, inputLocked = true; int dialogs = 0, overwriteDialogs = 0; QStringList errors;
    CopyResponder(MainWindow &window, QString target = {}, bool accepted = true) : window(window), target(target), accepted(accepted) {
        timer.setInterval(10); connect(&timer, &QTimer::timeout, this, [this] {
            for (auto dialog : this->window.findChildren<CopyDialog *>()) if (dialog->isVisible() && !dialog->property("testAnswered").toBool()) {
                dialog->setProperty("testAnswered", true); ++dialogs; initial = dialog->textValue(); label = dialog->findChild<QLabel *>("copyLabel")->text(); summary = dialog->findChild<QLabel *>("copyInfo")->text();
                inputLocked &= this->window.operationBusy() && !this->window.findChild<QAction *>("folderAction")->isEnabled();
                if (this->accepted) { if (!this->target.isNull()) dialog->setTextValue(this->target); dialog->accept(); } else dialog->reject();
            }
            for (auto dialog : this->window.findChildren<QMessageBox *>()) if (dialog->isVisible() && dialog->objectName() != "progressFinalMessage") { errors << dialog->text(); dialog->accept(); }
            for (auto dialog : this->window.findChildren<OverwriteDialog *>()) if (dialog->isVisible() && !dialog->property("testAnswered").toBool()) {
                dialog->setProperty("testAnswered", true); ++overwriteDialogs; inputLocked &= this->window.operationBusy();
                dialog->findChild<QPushButton *>(overwriteButton)->click();
            }
        }); timer.start();
    }
};
class CopyWorkflowTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp; ProgressDialogDriver results;
    ArchiveResult execute(ArchiveRequest request) {
        SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request); if (done.isEmpty() && !done.wait(30000)) return {}; return qvariant_cast<ArchiveResult>(done.last().first());
    }
    ArchiveRequest seed(QString root, QString format, QString password = {}) {
        write(root + "/input/folder/日本語 space.txt"); write(root + "/input/folder/keep.txt", "keep"); QDir().mkpath(root + "/input/folder/empty");
        ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = root + "/input." + format; request.format = format; request.method = format == "zip" ? "Deflate" : "LZMA2"; request.level = 1; request.workingDirectory = root + "/input"; request.files = {"folder"}; request.password = password; request.encryptNames = !password.isEmpty() && format == "7z"; return request;
    }
    void settings(QString root, bool panels = false, QString second = {}) { QSettings s; s.clear(); s.setValue("View/LastPath", root); s.setValue("View/AutoRefresh", false); s.setValue("View/TwoPanels", panels); if (panels) s.setValue("View/Panel2Path", second); }
private slots:
    void initTestCase() { QVERIFY(temp.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.filePath("prefs")); }
    void originalDialogAndSummary() {
        CopyDialog dialog("Copy", "Copy to:", "relative/", "a & b\nlong source line", {"one/", "two/"}); dialog.show(); auto layout = dialog.findChild<ResourceDialogLayout *>(); QVERIFY(layout);
        QCOMPARE(dialog.size(), layout->dialogSize()); const auto combo = dialog.findChild<QComboBox *>("copyDestination"); QCOMPARE(combo->count(), 2); QCOMPARE(combo->currentText(), QString("relative/")); QCOMPARE(combo->lineEdit()->selectedText(), QString("relative/"));
        const auto resourceRect = layout->controlRect("IDC_COPY"); QCOMPARE(combo->pos(), resourceRect.topLeft()); QCOMPARE(combo->height(), resourceRect.height()); QVERIFY(qAbs(combo->width() - resourceRect.width()) <= 1); auto info = dialog.findChild<QLabel *>("copyInfo"); QVERIFY(!info->wordWrap()); QCOMPARE(info->textFormat(), Qt::PlainText);
        auto browse = dialog.findChild<QPushButton *>("copyBrowse"), ok = dialog.findChild<QPushButton *>("copyOk"), cancel = dialog.findChild<QPushButton *>("copyCancel"); QVERIFY(ok->isDefault()); const auto width = combo->width(), x = browse->x(), y = ok->y(); dialog.resize(dialog.width() + 100, dialog.height() + 60); QCoreApplication::processEvents(); QCOMPARE(combo->width(), width + 100); QCOMPARE(browse->x(), x + 100); QCOMPARE(ok->y(), y + 60); QVERIFY(info->geometry().bottom() < ok->y()); QVERIFY(ok->x() < cancel->x());
        QList<CopySummaryItem> items{{"folder", true, {}}, {"one.txt", false, 1000}, {"two.txt", false, 2000}};
        const auto text = copyItemsInfo(items, "/owned/"); QVERIFY(text.contains("Folders: 1\n")); QVERIFY(!text.contains("Folders: 1    (")); QVERIFY(text.contains("Files: 2    (")); QVERIFY(text.contains("3 000")); QVERIFY(text.endsWith("\n  folder/\n  one.txt\n  two.txt"));
        for (int n = 0; n < 10; ++n) items << CopySummaryItem{QString::number(n), false, 0}; const auto limited = copyItemsInfo(items, "/owned/"); QVERIFY(limited.endsWith("\n  ...")); QVERIFY(!limited.contains("\n  9"));
        dialog.reject();
    }
    void copyHistoryLimitAndCancellation() {
        QSettings().clear(); for (int n = 0; n < 25; ++n) CopyDialog::remember(QString("/%1/").arg(n)); CopyDialog::remember("/10/"); const auto history = QSettings().value("Copy/History").toStringList(); QCOMPARE(history.size(), 20); QCOMPARE(history.first(), QString("/10/")); QCOMPARE(history.count("/10/"), 1);
        const auto root = temp.filePath("cancel"); QVERIFY(write(root + "/source.txt")); settings(root); QSettings().setValue("Copy/History", history); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "source.txt"));
        CopyResponder responder(window, root + "/cancelled/", false); window.findChild<QAction *>("copyAction")->trigger(); QCOMPARE(responder.dialogs, 1); QVERIFY(responder.inputLocked); QCOMPARE(QSettings().value("Copy/History").toStringList(), history); QVERIFY(!QFileInfo::exists(root + "/cancelled")); window.close();
    }
    void filesystemDestination_data() { QTest::addColumn<bool>("move"); QTest::addColumn<bool>("directory"); for (bool move : {false, true}) for (bool folder : {false, true}) QTest::newRow(qPrintable(QString("%1-%2").arg(move ? "move" : "copy", folder ? "directory" : "filename"))) << move << folder; }
    void filesystemDestination() {
        QFETCH(bool, move); QFETCH(bool, directory); const auto root = temp.filePath(QString("fs-%1-%2").arg(move).arg(directory)); QVERIFY(write(root + "/source.txt")); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "source.txt"));
        const auto target = directory ? "new/deep/" : "new/deep/renamed 日本語.txt"; CopyResponder responder(window, target); window.findChild<QAction *>(move ? "moveAction" : "copyAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        const auto output = root + (directory ? "/new/deep/source.txt" : "/new/deep/renamed 日本語.txt"); QCOMPARE(read(output), QByteArray("owned Unicode 日本語 payload")); QCOMPARE(QFileInfo::exists(root + "/source.txt"), !move); QCOMPARE(QSettings().value("Copy/History").toStringList().first(), root + '/' + target); QVERIFY(responder.errors.isEmpty()); window.close();
    }
    void focusedShiftCopyMove_data() { QTest::addColumn<bool>("move"); QTest::newRow("shift-copy") << false; QTest::newRow("shift-move") << true; }
    void destinationPreparationFailureKeepsHistoryAndSource() {
        const auto root = temp.filePath("prepare-failure"); QVERIFY(write(root + "/source.txt")); QVERIFY(QDir().mkpath(root + "/blocked")); settings(root); QSettings().setValue("Copy/History", QStringList{"/previous/"}); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "source.txt"));
        QVERIFY(::chmod(QFile::encodeName(root + "/blocked").constData(), 0555) == 0); auto restore = qScopeGuard([&] { ::chmod(QFile::encodeName(root + "/blocked").constData(), 0755); });
        CopyResponder responder(window, "blocked/new/"); window.findChild<QAction *>("copyAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(QSettings().value("Copy/History").toStringList(), QStringList{"/previous/"}); QVERIFY(QFileInfo::exists(root + "/source.txt")); QVERIFY(!QFileInfo::exists(root + "/blocked/new")); QVERIFY(!results.notices.isEmpty()); window.close();
    }
    void focusedShiftCopyMove() {
        QFETCH(bool, move); const auto root = temp.filePath(move ? "shift-move" : "shift-copy"); QVERIFY(write(root + "/focused.txt")); QVERIFY(write(root + "/marked.txt", "must remain")); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "marked.txt")); list(window)->setCurrentItem(find(window, "focused.txt"), 0, QItemSelectionModel::NoUpdate); list(window)->setFocus();
        CopyResponder responder(window, "renamed.txt"); QTest::keyClick(list(window), move ? Qt::Key_F6 : Qt::Key_F5, Qt::ShiftModifier); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(responder.initial, QString("focused.txt")); QCOMPARE(read(root + "/renamed.txt"), QByteArray("owned Unicode 日本語 payload")); QCOMPARE(read(root + "/marked.txt"), QByteArray("must remain")); QCOMPARE(QFileInfo::exists(root + "/focused.txt"), !move); window.close();
    }
    void archiveToFilesystem_data() { QTest::addColumn<QString>("format"); QTest::addColumn<bool>("flat"); for (const auto &format : {"7z", "zip"}) for (bool flat : {false, true}) QTest::newRow(qPrintable(QString("%1-%2").arg(format).arg(flat))) << QString(format) << flat; }
    void archiveToFilesystem() {
        QFETCH(QString, format); QFETCH(bool, flat); const auto root = temp.filePath(QString("archive-fs-%1-%2").arg(format).arg(flat)); const auto archive = seed(root, format); QVERIFY(execute(archive).success); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); window.openPath(archive.archive + "/folder"); QTRY_VERIFY(!window.operationBusy());
        if (flat) { window.findChild<QAction *>("flatAction")->trigger(); QTRY_VERIFY(!window.operationBusy()); } QVERIFY(select(window, "日本語 space.txt")); const auto before = read(archive.archive);
        CopyResponder responder(window, root + "/output/"); window.findChild<QAction *>("copyAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); QCOMPARE(responder.dialogs, 1); QCOMPARE(read(root + "/output/日本語 space.txt"), QByteArray("owned Unicode 日本語 payload")); QVERIFY(!QFileInfo::exists(root + "/output/folder")); QCOMPARE(read(archive.archive), before); QVERIFY(responder.errors.isEmpty()); window.close();
    }
    void archiveBetweenPanels_data() { QTest::addColumn<QString>("sourceFormat"); QTest::addColumn<QString>("destinationFormat"); for (const auto &source : {"7z", "zip"}) for (const auto &destination : {"7z", "zip"}) QTest::newRow(qPrintable(QString("%1-%2").arg(source, destination))) << QString(source) << QString(destination); }
    void archiveBetweenPanels() {
        QFETCH(QString, sourceFormat); QFETCH(QString, destinationFormat); const auto root = temp.filePath("panels-" + sourceFormat + destinationFormat); auto source = seed(root + "/source", sourceFormat); auto destination = seed(root + "/destination", destinationFormat); QVERIFY(write(root + "/source/input/folder/日本語 space.txt", "new source bytes")); QVERIFY(execute(source).success); QVERIFY(execute(destination).success);
        settings(root, true, root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other); window.openPath(source.archive + "/folder"); QTRY_VERIFY(!window.operationBusy()); other->openPath(destination.archive + "/folder"); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "日本語 space.txt")); QVERIFY(select(window, "empty", false)); QCOMPARE(list(window)->markedItems().size(), 2); list(window)->setFocus();
        QSignalSpy sourceDone(window.findChild<SevenZipProcessBackend *>(QString(), Qt::FindDirectChildrenOnly), &ArchiveBackend::finished); QSignalSpy destinationDone(other->findChild<SevenZipProcessBackend *>(QString(), Qt::FindDirectChildrenOnly), &ArchiveBackend::finished);
        CopyResponder responder(window); window.findChild<QAction *>("copyAction", Qt::FindDirectChildrenOnly)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); QCOMPARE(responder.dialogs, 1); QCOMPARE(responder.initial, destination.archive + "/folder/"); QVERIFY(responder.errors.isEmpty()); QVERIFY(find(window, "日本語 space.txt")); QVERIFY(find(*other, "日本語 space.txt")); QVERIFY(find(*other, "empty"));
        bool copied = false, added = false; for (const auto &signal : sourceDone) { const auto value = qvariant_cast<ArchiveResult>(signal.first()); if (value.operation == ArchiveOperation::Extract) { QVERIFY2(value.success, qPrintable(value.message + value.details)); copied = true; } } for (const auto &signal : destinationDone) { const auto value = qvariant_cast<ArchiveResult>(signal.first()); if (value.operation == ArchiveOperation::Add) { QVERIFY2(value.success, qPrintable(value.message + value.details)); added = true; } } QVERIFY(copied); QVERIFY(added);
        destination.operation = ArchiveOperation::Extract; destination.outputDirectory = root + "/verify"; destination.overwriteMode = "overwrite"; QVERIFY(execute(destination).success); QCOMPARE(read(root + "/verify/folder/日本語 space.txt"), QByteArray("new source bytes")); QCOMPARE(read(root + "/verify/folder/keep.txt"), QByteArray("keep")); QVERIFY(QFileInfo(root + "/verify/folder/empty").isDir()); destination.operation = ArchiveOperation::Test; QVERIFY(execute(destination).success); window.close();
    }
    void selfCopyAndWindowsArchiveMoveError() {
        const auto root = temp.filePath("errors"); const auto archive = seed(root, "7z"); QVERIFY(execute(archive).success); settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "input.7z")); CopyResponder responder(window); window.findChild<QAction *>("copyAction")->trigger(); QCOMPARE(responder.dialogs, 1); QVERIFY(responder.errors.join('\n').contains("Cannot copy files onto itself"));
        window.openPath(archive.archive + "/folder"); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "日本語 space.txt")); const auto before = read(archive.archive); responder.target = root + "/must-not-move/"; window.findChild<QAction *>("moveAction")->trigger(); QVERIFY(responder.errors.join('\n').contains("E_NOTIMPL")); QCOMPARE(read(archive.archive), before); QVERIFY(!QFileInfo::exists(root + "/must-not-move")); QVERIFY(!window.operationBusy()); window.close();
    }
    void cancelArchiveBetweenPanelsRetainsInputs() {
        const auto root = temp.filePath("cancel-panels"); auto source = seed(root + "/source", "7z"), destination = seed(root + "/destination", "zip"); QVERIFY(write(root + "/source/input/folder/large.bin", QByteArray(48 * 1024 * 1024, 'c'))); source.level = 0; QVERIFY(execute(source).success); QVERIFY(execute(destination).success);
        const auto sourceBytes = read(source.archive), destinationBytes = read(destination.archive); settings(root, true, root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other); window.openPath(source.archive + "/folder"); QTRY_VERIFY(!window.operationBusy()); other->openPath(destination.archive + "/folder"); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "large.bin"));
        auto backend = window.findChild<SevenZipProcessBackend *>(QString(), Qt::FindDirectChildrenOnly); QVERIFY(backend); QSignalSpy done(backend, &ArchiveBackend::finished); bool cancelled = false; QTimer cancellation; cancellation.setInterval(1); connect(&cancellation, &QTimer::timeout, &window, [&] { if (backend->busy() && !cancelled) { cancelled = true; backend->cancel(); } });
        bool reloadStayedBusy = false;
        auto destinationBackend = other->findChild<SevenZipProcessBackend *>(QString(), Qt::FindDirectChildrenOnly);
        connect(destinationBackend, &ArchiveBackend::finished, &window, [&](ArchiveResult result) {
            if (cancelled && result.operation == ArchiveOperation::List) reloadStayedBusy = window.operationBusy();
        });
        CopyResponder responder(window); cancellation.start(); window.findChild<QAction *>("copyAction", Qt::FindDirectChildrenOnly)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); cancellation.stop(); QVERIFY(cancelled); bool observed = false; for (const auto &signal : done) { const auto value = qvariant_cast<ArchiveResult>(signal.first()); if (value.operation == ArchiveOperation::Extract) { QVERIFY(!value.success); QVERIFY(value.cancelled); observed = true; } } QVERIFY(observed); QCOMPARE(read(source.archive), sourceBytes); QCOMPARE(read(destination.archive), destinationBytes); QVERIFY(reloadStayedBusy); QVERIFY(list(window)->isEnabled()); QVERIFY(list(*other)->isEnabled()); QVERIFY(window.findChild<QAction *>("optionsAction", Qt::FindDirectChildrenOnly)->isEnabled()); QVERIFY(list(window)->markedItems().isEmpty()); QVERIFY(find(window, "large.bin")); QCOMPARE(list(window)->currentItem(), find(window, "large.bin")); QTRY_VERIFY(!QApplication::activeModalWidget()); QVERIFY(select(window, "large.bin")); QTRY_VERIFY2(window.findChild<QAction *>("copyAction", Qt::FindDirectChildrenOnly)->isEnabled(), qPrintable(QString("focus=%1 active=%2 sourceFocus=%3 destinationFocus=%4 operated=%5 childAction=%6")
            .arg(QApplication::focusWidget() ? QApplication::focusWidget()->objectName() : QString("none"))
            .arg(QApplication::activeWindow() ? QApplication::activeWindow()->objectName() : QString("none"))
            .arg(list(window)->hasFocus()).arg(list(*other)->hasFocus()).arg(list(window)->operatedItems().size())
            .arg(other->findChild<QAction *>("copyAction", Qt::FindDirectChildrenOnly)->isEnabled()))); window.close();
    }
    void duplicateRowsDoNotBecomeSingleArchiveInput_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<QString>("choice"); QTest::addColumn<bool>("flat");
        for (const auto format : {"7z", "zip"}) for (const auto button : {"Yes", "YesAll", "No", "NoAll", "Rename", "Cancel"})
            QTest::newRow(qPrintable(QString("%1-%2").arg(format, button))) << QString(format) << QString("overwrite") + button << false;
        QTest::newRow("flat-path-collision") << QString("7z") << QString("overwriteYes") << true;
    }
    void duplicateRowsDoNotBecomeSingleArchiveInput() {
        QFETCH(QString, format); QFETCH(QString, choice); QFETCH(bool, flat);
        const auto root = temp.filePath("duplicate-panels-" + format + choice + QString::number(flat)); auto destination = seed(root + "/destination", format); QVERIFY(execute(destination).success); const auto archive = root + "/same-name.zip";
        const auto python = QStandardPaths::findExecutable("python3"); QVERIFY(!python.isEmpty()); QCOMPARE(QProcess::execute(python, {"-c", "import sys,zipfile,warnings; warnings.simplefilter('ignore'); z=zipfile.ZipFile(sys.argv[1],'w'); z.writestr('same.txt','first'); z.writestr('same.txt','second'); z.close()", archive}), 0);
        if (flat) QCOMPARE(QProcess::execute(python, {"-c", "import sys,zipfile; z=zipfile.ZipFile(sys.argv[1],'w'); z.writestr('one/same.txt','first'); z.writestr('two/same.txt','second'); z.close()", archive}), 0);
        const auto input = read(archive), output = read(destination.archive); settings(root, true, root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other);
        window.openPath(archive); QTRY_VERIFY(!window.operationBusy()); other->openPath(destination.archive + "/folder"); QTRY_VERIFY(!window.operationBusy());
        if (flat) { window.findChild<QAction *>("flatAction", Qt::FindDirectChildrenOnly)->trigger(); QTRY_VERIFY(!window.operationBusy()); }
        auto files = list(window); files->clearSelection(); int selected = 0;
        for (int i = 0; i < files->topLevelItemCount(); ++i) if (panelItemName(files->topLevelItem(i)) == "same.txt") { files->setItemSelected(files->topLevelItem(i), true); ++selected; }
        QCOMPARE(selected, 2); files->setFocus();
        auto sourceBackend = window.findChild<SevenZipProcessBackend *>(QString(), Qt::FindDirectChildrenOnly), destinationBackend = other->findChild<SevenZipProcessBackend *>(QString(), Qt::FindDirectChildrenOnly);
        QSignalSpy sourceDone(sourceBackend, &ArchiveBackend::finished), destinationDone(destinationBackend, &ArchiveBackend::finished);
        CopyResponder responder(window); responder.overwriteButton = choice; window.findChild<QAction *>("copyAction", Qt::FindDirectChildrenOnly)->trigger();
        QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); QCOMPARE(responder.dialogs, 1); QCOMPARE(responder.overwriteDialogs, 1); QVERIFY(responder.inputLocked);
        bool extracted = false, added = false;
        for (const auto &signal : sourceDone) { const auto result = qvariant_cast<ArchiveResult>(signal.first()); if (result.operation == ArchiveOperation::Extract) { extracted = true; QCOMPARE(result.cancelled, choice == "overwriteCancel"); QCOMPARE(result.success, choice != "overwriteCancel"); } }
        for (const auto &signal : destinationDone) { const auto result = qvariant_cast<ArchiveResult>(signal.first()); if (result.operation == ArchiveOperation::Add) { added = true; QVERIFY(!result.success); QCOMPARE(result.exitCode, 2); QVERIFY2(result.details.contains("Duplicate filename"), qPrintable(result.message + result.details)); } }
        QVERIFY(extracted); QCOMPARE(added, choice != "overwriteCancel"); QCOMPARE(read(archive), input); QCOMPARE(read(destination.archive), output); QVERIFY(find(window, "same.txt")); window.close();
    }
    void archiveSingleDestinationIsDirectory_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void archiveSingleDestinationIsDirectory() {
        QFETCH(QString, format); const auto root = temp.filePath("single-destination-" + format); auto archive = seed(root, format); QVERIFY(execute(archive).success); const auto before = read(archive.archive);
        settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); window.openPath(archive.archive + "/folder"); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "日本語 space.txt"));
        CopyResponder responder(window, root + "/entered-name.txt"); window.findChild<QAction *>("copyAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000);
        // Agent::Extract normalizes the whole Copy destination to a directory,
        // even for one file and without a trailing slash. FSFolder differs.
        QVERIFY(QFileInfo(root + "/entered-name.txt").isDir()); QCOMPARE(read(root + "/entered-name.txt/日本語 space.txt"), QByteArray("owned Unicode 日本語 payload")); QCOMPARE(read(archive.archive), before); QVERIFY(responder.errors.isEmpty()); window.close();
    }
    void filesystemFlatPathsToArchive_data() {
        QTest::addColumn<QString>("format"); QTest::addColumn<bool>("collision");
        for (const auto format : {"7z", "zip"}) for (bool collision : {false, true}) QTest::newRow(qPrintable(QString("%1-%2").arg(format).arg(collision))) << QString(format) << collision;
    }
    void filesystemFlatPathsToArchive() {
        QFETCH(QString, format); QFETCH(bool, collision); const auto root = temp.filePath("flat-fs-" + format + QString::number(collision)); auto destination = seed(root + "/destination", format); QVERIFY(execute(destination).success); const auto before = read(destination.archive);
        const auto first = collision ? "same.txt" : "first.txt", second = collision ? "same.txt" : "second.txt";
        QVERIFY(write(root + "/source/one/" + first, "first")); QVERIFY(write(root + "/source/two/" + second, "second"));
        settings(root + "/source", true, root); QSettings().setValue("View/Flat", true); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other);
        other->openPath(destination.archive + "/folder"); QTRY_VERIFY(!window.operationBusy()); auto files = list(window); files->clearSelection(); int selected = 0;
        for (int i = 0; i < files->topLevelItemCount(); ++i) if (panelItemName(files->topLevelItem(i)) == first || panelItemName(files->topLevelItem(i)) == second) { files->setItemSelected(files->topLevelItem(i), true); ++selected; }
        QSignalSpy done(other->findChild<SevenZipProcessBackend *>(QString(), Qt::FindDirectChildrenOnly), &ArchiveBackend::finished);
        QCOMPARE(selected, 2); CopyResponder responder(window); window.findChild<QAction *>("copyAction", Qt::FindDirectChildrenOnly)->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 30000); QVERIFY(responder.errors.isEmpty());
        bool added = false; for (const auto &signal : done) { const auto result = qvariant_cast<ArchiveResult>(signal.first()); if (result.operation == ArchiveOperation::Add) { added = true; QCOMPARE(result.success, !collision); if (collision) { QCOMPARE(result.exitCode, 2); QVERIFY(result.details.contains("Duplicate filename")); } } } QVERIFY(added);
        QCOMPARE(read(root + "/source/one/" + first), QByteArray("first")); QCOMPARE(read(root + "/source/two/" + second), QByteArray("second"));
        if (collision) { QCOMPARE(read(destination.archive), before); window.close(); return; }
        destination.operation = ArchiveOperation::Extract; destination.outputDirectory = root + "/verify"; destination.files.clear(); destination.overwriteMode = "overwrite"; auto extracted = execute(destination); QVERIFY2(extracted.success, qPrintable(extracted.message + extracted.details));
        // Official EnumerateItems2 deliberately discards each selected input's
        // directory prefix, including FS Flat paths. Equal basenames fail.
        QCOMPARE(read(root + "/verify/folder/first.txt"), QByteArray("first")); QCOMPARE(read(root + "/verify/folder/second.txt"), QByteArray("second")); QVERIFY(!QFileInfo::exists(root + "/verify/folder/one")); QVERIFY(!QFileInfo::exists(root + "/verify/folder/two")); window.close();
    }
    void combineUsesOriginalCopyDialog() {
        const auto root = temp.filePath("combine"); QVERIFY(write(root + "/file.001", "one")); QVERIFY(write(root + "/file.002", "two")); QVERIFY(write(root + "/file.003", "three")); QSettings().setValue("Combine/History", QStringList{"obsolete artificial history"}); CombineDialog dialog(root + "/file.001", root + "/output/", nullptr); dialog.show(); auto layout = dialog.findChild<ResourceDialogLayout *>(); QVERIFY(layout); QCOMPARE(dialog.size(), layout->dialogSize()); auto combo = dialog.findChild<QComboBox *>("combineDestination"); QCOMPARE(combo->count(), 0); QCOMPARE(dialog.findChild<QLabel *>("copyLabel")->text(), UiLanguage::resource(7401)); const auto info = dialog.findChild<QLabel *>("copyInfo")->text(); QVERIFY(info.contains("\n  file.001\n  file.002\n  file.003")); dialog.reject();
        settings(root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(select(window, "file.001")); CopyResponder responder(window, "output/"); window.findChild<QAction *>("combineAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QCOMPARE(read(root + "/output/file"), QByteArray("onetwothree")); window.close();
    }
};
int main(int argc, char **argv) {
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]); const auto plugins = qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({plugins}); QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setOrganizationName("SevenZipMacPortTests"); app.setApplicationName("CopyWorkflow"); app.setQuitOnLastWindowClosed(false); CopyWorkflowTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "copy-workflow.moc"
