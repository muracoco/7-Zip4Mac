// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "ComboDialog.h"
#include "ChecksumResultsDialog.h"
#include "ResourceDialogs.h"
#include "PanelSelection.h"
#include "PanelDisplay.h"
#include "PortStyle.h"
#include "UiLanguage.h"
#include "input-driver.h"
#include "progress-dialog-driver.h"
#include <QAction>
#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QKeyEvent>
#include <QPushButton>
#include <QScopeGuard>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <sys/xattr.h>
#include <unistd.h>
static QString executable;
static bool write(QString path, QByteArray value = "owned payload") { QDir().mkpath(QFileInfo(path).absolutePath()); QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(value) == value.size(); }
static QByteArray read(QString path) { QFile file(path); return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(); }
static FileList *list(MainWindow &window) { return window.findChild<FileList *>("fileList"); }
static QTreeWidgetItem *find(MainWindow &window, QString name) { for (int i = 0; i < list(window)->topLevelItemCount(); ++i) if (panelItemName(list(window)->topLevelItem(i)) == name) return list(window)->topLevelItem(i); return nullptr; }
class CommandEntryTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    ProgressDialogDriver results;
    ArchiveResult execute(ArchiveRequest request) { SevenZipProcessBackend backend(executable); QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request); if (done.isEmpty() && !done.wait(30000)) return {}; return qvariant_cast<ArchiveResult>(done.last().first()); }
private slots:
    void initTestCase() { QVERIFY(temp.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.filePath("preferences")); qInfo() << "Owned command-entry fixtures:" << temp.path(); }
    void init() { results.clear(); QSettings().clear(); UiLanguage::set("en"); QSettings().setValue("View/LastPath", temp.path()); QSettings().setValue("View/AutoRefresh", false); FileManagerSettings settings; settings.realIcons = false; QVERIFY(settings.save()); }
    void comboResourceAndResize() {
        ComboDialog dialog("Select", "Mask:", "*", nullptr, {"first", "second"}); dialog.show(); QTest::qWait(10);
        auto layout = dialog.findChild<ResourceDialogLayout *>(); auto combo = dialog.findChild<QComboBox *>("comboValue"); QVERIFY(layout && combo);
        QCOMPARE(dialog.size(), layout->dialogSize()); QCOMPARE(combo->geometry(), layout->toPixels({8, 20, 240, 14})); QCOMPARE(combo->count(), 2); QCOMPARE(dialog.textValue(), QString("*"));
        const auto before = combo->width(); dialog.resize(dialog.width() + 180, dialog.height() + 100); QTest::qWait(10); QCOMPARE(combo->width(), before + 180);
        const auto margin = layout->toPixels({0, 0, 8, 8}).size(); auto cancel = dialog.findChild<QPushButton *>("comboCancel"); QCOMPARE(cancel->geometry().right() + 1, dialog.width() - margin.width()); QCOMPARE(cancel->geometry().bottom() + 1, dialog.height() - margin.height());
        QCOMPARE(combo->lineEdit()->selectedText(), QString("*")); dialog.setTextValue("日本語 space*"); QCOMPARE(dialog.textValue(), QString("日本語 space*"));
    }
    void sourceWildcard_data() { QTest::addColumn<bool>("alternative"); QTest::newRow("normal") << false; QTest::newRow("alternative") << true; }
    void sourceWildcard() {
        QFETCH(bool, alternative); FileList files; IconFileList icons; files.setProperty("alternativeSelection", alternative); files.setSelectionMode(alternative ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
        for (const auto &name : QStringList{"..", "[a].txt", "a.txt", "A.TXT", "日本語.txt", QString::fromUtf8("🙂.txt"), "extensionless"}) { auto item = new QTreeWidgetItem(&files, {name}); item->setData(0, PanelNameRole, name); if (name == "..") item->setData(0, Qt::UserRole + 2, true); }
        icons.setModel(files.model()); icons.setSelectionModel(files.selectionModel()); PanelSelection selection(&files, &icons);
        selection.selectMask("[a].txt", true); QCOMPARE(files.markedItems().size(), 1); QCOMPARE(panelItemName(files.markedItems().first()), QString("[a].txt"));
        selection.selectAll(false); selection.selectMask("a.txt", true); QCOMPARE(files.markedItems().size(), 2);
        selection.selectAll(false); selection.selectMask("??.txt", true); QCOMPARE(files.markedItems().size(), 1); QCOMPARE(panelItemName(files.markedItems().first()), QString::fromUtf8("🙂.txt"));
        selection.selectAll(true); selection.selectMask("*.*", false); QCOMPARE(files.markedItems().size(), 1); QCOMPARE(panelItemName(files.markedItems().first()), QString("extensionless"));
    }
    void inlineRename_data() { QTest::addColumn<int>("mode"); QTest::addColumn<bool>("alternative"); for (int view = 0; view < 4; ++view) for (bool alternative : {false, true}) QTest::newRow(qPrintable(QString("view%1-%2").arg(view).arg(alternative))) << view << alternative; }
    void inlineRename() {
        QFETCH(int, mode); QFETCH(bool, alternative); auto root = temp.filePath(QString("inline-%1-%2").arg(mode).arg(alternative)); QVERIFY(write(root + "/[日本語] space.txt")); QVERIFY(write(root + "/marked.txt"));
        QSettings().setValue("View/LastPath", root); QSettings().setValue("View/Mode", mode); auto settings = FileManagerSettings::load(); settings.alternativeSelection = alternative; QVERIFY(settings.save()); MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
        auto files = list(window); auto target = find(window, "[日本語] space.txt"); QVERIFY(target); files->setCurrentItem(target); files->setItemSelected(target, true);
        auto action = window.findChild<QAction *>("renameAction"); QVERIFY(action->isEnabled()); action->trigger(); auto editor = window.findChild<QLineEdit *>("renameEditor"); QVERIFY(editor); QVERIFY(editor->isVisible()); QVERIFY(window.operationBusy()); QVERIFY(files->isEnabled()); QCOMPARE(editor->text(), QString("[日本語] space.txt")); QCOMPARE(editor->selectedText(), editor->text());
        QVERIFY(!window.findChild<QAction *>("folderAction")->isEnabled()); auto input = testInput(&window, "renameEditor"); input->setTextValue("renamed 日本語.txt"); input->accept(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); QVERIFY(QFileInfo::exists(root + "/renamed 日本語.txt")); QCOMPARE(read(root + "/renamed 日本語.txt"), QByteArray("owned payload")); QCOMPARE(panelItemName(files->currentItem()), QString("renamed 日本語.txt"));
        action->trigger(); auto cancel = testInput(&window, "renameEditor"); QVERIFY(cancel); cancel->setTextValue("cancelled.txt"); cancel->reject(); QTRY_VERIFY(!window.operationBusy()); QVERIFY(!QFileInfo::exists(root + "/cancelled.txt")); QVERIFY(QFileInfo::exists(root + "/renamed 日本語.txt")); window.close();
    }
    void partialRowSelection() {
        FileList files; IconFileList icons; files.setColumnCount(3); files.setSelectionBehavior(QAbstractItemView::SelectItems);
        auto item = new QTreeWidgetItem(&files, {"selected.txt", "7", "type"}); item->setData(0, PanelNameRole, "selected.txt");
        icons.setModel(files.model()); icons.setSelectionModel(files.selectionModel()); PanelSelection selection(&files, &icons);
        files.selectionModel()->select(files.indexFromItem(item, 1), QItemSelectionModel::ClearAndSelect);
        QCOMPARE(files.markedItems(), QList<QTreeWidgetItem *>{item}); QCOMPARE(files.operatedItems(), QList<QTreeWidgetItem *>{item});
    }
    void closeDuringInlineEdit() {
        const auto root = temp.filePath("close-edit"); QVERIFY(write(root + "/original.txt")); QSettings().setValue("View/LastPath", root);
        MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); auto item = find(window, "original.txt"); QVERIFY(item); list(window)->setCurrentItem(item); item->setSelected(true);
        window.findChild<QAction *>("renameAction")->trigger(); auto input = testInput(&window, "renameEditor"); QVERIFY(input); input->setTextValue("must-not-commit.txt"); window.close();
        QTRY_VERIFY(!window.isVisible()); QVERIFY(QFileInfo::exists(root + "/original.txt")); QVERIFY(!QFileInfo::exists(root + "/must-not-commit.txt"));
    }
    void exclusiveRename_data() { QTest::addColumn<QString>("scenario"); for (const auto &s : {"case", "symlink", "hardlink", "existing", "readonly", "cancel", "parent-permission", "folder"}) QTest::newRow(s) << QString(s); }
    void exclusiveRename() {
        QFETCH(QString, scenario); auto root = temp.filePath("rename-" + scenario), source = root + "/original.txt", target = root + "/renamed.txt"; QVERIFY(write(source)); auto restore = qScopeGuard([&] { ::chmod(QFile::encodeName(root).constData(), 0755); });
        if (scenario == "case") target = root + "/ORIGINAL.txt";
        if (scenario == "existing") QVERIFY(write(target, "unrelated destination"));
        if (scenario == "hardlink") QVERIFY(::link(QFile::encodeName(source).constData(), QFile::encodeName(target).constData()) == 0);
        if (scenario == "symlink") { source = root + "/link"; QVERIFY(::symlink("original.txt", QFile::encodeName(source).constData()) == 0); }
        if (scenario == "readonly") QVERIFY(::chmod(QFile::encodeName(source).constData(), 0444) == 0);
        if (scenario == "folder") { source = root + "/nested"; QDir().mkpath(source + "/empty"); QVERIFY(write(source + "/inside.txt")); }
        auto before = FileInstall::capture(source, true); auto destination = FileInstall::capture(target, true); auto control = std::make_shared<OperationControl>(); if (scenario == "cancel") control->cancel(); if (scenario == "parent-permission") QVERIFY(::chmod(QFile::encodeName(root).constData(), 0555) == 0);
        auto result = FileInstall::rename(before, destination, control);
        const bool failure = QStringList{"existing", "hardlink", "cancel", "parent-permission"}.contains(scenario); QCOMPARE(result.error.isEmpty(), !failure);
        if (failure) { QVERIFY(QFileInfo::exists(source)); if (scenario == "existing") QCOMPARE(read(target), QByteArray("unrelated destination")); }
        else { auto after = FileInstall::capture(target, true); QCOMPARE(after.stamp.st_ino, before.stamp.st_ino); QCOMPARE(after.stamp.st_mode, before.stamp.st_mode); QCOMPARE(after.linkTarget, before.linkTarget); if (scenario == "folder") QVERIFY(QFileInfo(target + "/empty").isDir()); }
    }
    void creationAndSelection() {
        auto root = temp.filePath("creation"); QDir().mkpath(root); QSettings().setValue("View/LastPath", root); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy());
        for (const auto &command : QStringList{"newfile", "folder"}) {
            QString entered = command == "folder" ? "日本語 parents/space folder/empty" : UiLanguage::resource(6305); bool seen = false;
            QTimer::singleShot(10, &window, [&] { auto dialog = window.findChild<ComboDialog *>(); QVERIFY(dialog); QCOMPARE(dialog->textValue(), UiLanguage::resource(command == "folder" ? 6304 : 6305)); QCOMPARE(dialog->findChild<QComboBox *>()->count(), 0); seen = true; dialog->setTextValue(entered); dialog->accept(); });
            window.findChild<QAction *>(command + "Action")->trigger(); QVERIFY(seen); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000);
            QVERIFY(QFileInfo::exists(root + '/' + entered)); QCOMPARE(panelItemName(list(window)->currentItem()), entered.section('/', 0, 0));
        }
        QVERIFY(QFileInfo(root + "/日本語 parents/space folder/empty").isDir()); QCOMPARE(read(root + '/' + UiLanguage::resource(6305)), QByteArray()); window.close();
    }
    void archiveRenameAndCreate_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void archiveRenameAndCreate() {
        QFETCH(QString, format); auto root = temp.filePath("archive-input-" + format); QVERIFY(write(root + "/日本語 original.txt")); ArchiveRequest request; request.operation = ArchiveOperation::Add; request.archive = temp.filePath("commands." + format); request.format = format; request.method = format == "zip" ? "Deflate" : "LZMA2"; request.workingDirectory = root; request.files = {"日本語 original.txt"}; auto result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details));
        MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy()); window.openPath(request.archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 10000); auto item = find(window, "日本語 original.txt"); QVERIFY(item); list(window)->setCurrentItem(item); item->setSelected(true);
        window.findChild<QAction *>("renameAction")->trigger(); auto editor = testInput(&window, "renameEditor"); QVERIFY(editor); editor->setTextValue("renamed space 日本語.txt"); editor->accept(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QVERIFY(find(window, "renamed space 日本語.txt"));
        QTimer::singleShot(10, &window, [&] { auto dialog = window.findChild<ComboDialog *>(); QVERIFY(dialog); dialog->setTextValue("nested parents/日本語 empty"); dialog->accept(); }); window.findChild<QAction *>("folderAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QVERIFY(find(window, "nested parents"));
        request.operation = ArchiveOperation::Test; request.files.clear(); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); request.operation = ArchiveOperation::Extract; request.outputDirectory = temp.filePath("command-output-" + format); result = execute(request); QVERIFY2(result.success, qPrintable(result.message + result.details)); QCOMPARE(read(request.outputDirectory + "/renamed space 日本語.txt"), QByteArray("owned payload")); QVERIFY(QFileInfo(request.outputDirectory + "/nested parents/日本語 empty").isDir()); window.close();
    }
    void historyDeleteAcceptAndCancel() {
        auto first = temp.filePath("history first"), second = temp.filePath("history 日本語"); QDir().mkpath(first); QDir().mkpath(second); MainWindow window(executable); window.show(); QTRY_VERIFY(!window.operationBusy());
        for (bool accept : {false, true}) {
            QSettings().setValue("View/History", QStringList{first, second}); bool seen = false;
            QTimer::singleShot(10, &window, [&] { auto dialog = window.findChild<ChecksumResultsDialog *>("folderHistoryDialog"); QVERIFY(dialog); auto tree = dialog->findChild<QTreeWidget *>("folderHistoryList"); QVERIFY(tree); QCOMPARE(tree->currentItem()->text(0), first); dialog->findChild<QAction *>("deleteChecksumRows")->trigger(); QCOMPARE(dialog->strings(), QStringList{second}); QVERIFY(dialog->stringsWereChanged()); seen = true; if (accept) dialog->accept(); else dialog->reject(); });
            window.findChild<QAction *>("historyAction")->trigger(); QVERIFY(seen); QTRY_VERIFY(!window.operationBusy()); if (accept) { QVERIFY(!QSettings().value("View/History").toStringList().contains(first)); QCOMPARE(window.currentDirectory(), second); } else QCOMPARE(QSettings().value("View/History").toStringList(), QStringList({first, second}));
        } window.close();
    }
    void cleanupTestCase() { QSettings().clear(); }
};
int main(int argc, char **argv) {
    if (argc < 2) return 1; executable = QString::fromLocal8Bit(argv[1]);
    if (!qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT").isEmpty()) QCoreApplication::setLibraryPaths({qEnvironmentVariable("PORT_TEST_PLUGIN_ROOT")});
    QCoreApplication::setAttribute(Qt::AA_MacDontSwapCtrlAndMeta); QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv); applyPortAppearance(app); app.setApplicationName("7zip-command-entry-test"); app.setOrganizationName("7zip-command-entry-test"); app.setQuitOnLastWindowClosed(false);
    CommandEntryTests tests; QList<char *> args{argv[0]}; for (int n = 2; n < argc; ++n) args << argv[n]; args << nullptr; return QTest::qExec(&tests, args.size() - 1, args.data());
}
#include "command-entry.moc"
