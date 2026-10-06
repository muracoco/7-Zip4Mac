// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QLocale>
#include "PanelSelection.h"
#include "MainWindow.h"
#include "FileListItem.h"
#include "PortStyle.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QHeaderView>
#include <QKeyEvent>
#include "input-driver.h"
#include <QPushButton>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>

static QString executable;
constexpr int ParentRole = Qt::UserRole + 2;
static QStringList names(const QList<QTreeWidgetItem *> &items) { QStringList result; for (auto item : items) result << item->text(0); result.sort(); return result; }
struct Fixture {
    QWidget host;
    FileList *files = new FileList(&host);
    IconFileList *icons = new IconFileList(&host);
    QAbstractItemView *view;
    Fixture(bool alternative, bool iconMode) {
        files->setColumnCount(2); files->setHeaderLabels({"Name", "Size"}); files->headerItem()->setData(0,ColumnPropertyRole,OfficialSort::kpidName); files->headerItem()->setData(1,ColumnPropertyRole,OfficialSort::kpidSize); files->setRootIsDecorated(false); files->setColumnWidth(0, 240);
        auto parent = new FileItem(files, {".."}); parent->setData(0, ParentRole, true);
        for (auto text : {"alpha.txt", "日本語.txt", "space name.txt", "zeta.txt"}) { auto item = new FileItem(files, {text, "123"}); item->setData(0, Qt::UserRole, text); item->setSortProperties({},files->topLevelItemCount()-1); }
        icons->setModel(files->model()); icons->setSelectionModel(files->selectionModel()); icons->setModelColumn(0); icons->setViewMode(QListView::ListMode);
        new PanelSelection(files, icons); files->setItemDelegate(new PanelSelectionDelegate(files)); icons->setItemDelegate(new PanelSelectionDelegate(icons));
        for (auto control : {static_cast<QAbstractItemView *>(files), static_cast<QAbstractItemView *>(icons)}) { control->setProperty("alternativeSelection", alternative); control->setSelectionMode(alternative ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection); control->setSelectionBehavior(QAbstractItemView::SelectRows); }
        view = iconMode ? static_cast<QAbstractItemView *>(icons) : files; auto layout = new QVBoxLayout(&host); layout->addWidget(view); (iconMode ? static_cast<QWidget *>(files) : icons)->hide(); host.resize(480, 300); host.show(); QApplication::processEvents();
    }
    void focus(int row, bool selected = true) { files->setCurrentItem(files->topLevelItem(row), 0, selected ? QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows : QItemSelectionModel::NoUpdate); }
    void click(int row, Qt::KeyboardModifiers modifiers = Qt::NoModifier) { const auto index = files->indexFromItem(files->topLevelItem(row)); QTest::mouseClick(view->viewport(), Qt::LeftButton, modifiers, view->visualRect(index).center()); }
    void key(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) { QKeyEvent event(QEvent::KeyPress, key, modifiers); QApplication::sendEvent(view, &event); }
};
class PanelSelectionTests : public QObject {
    Q_OBJECT
    QTemporaryDir temp;
    static void modes() { QTest::addColumn<bool>("icons"); QTest::newRow("details") << false; QTest::newRow("icons") << true; }
    ArchiveResult execute(SevenZipProcessBackend &backend, const ArchiveRequest &request) { QSignalSpy done(&backend, &ArchiveBackend::finished); backend.start(request); if (done.isEmpty() && !done.wait(20000)) qFatal("7-Zip timeout"); return qvariant_cast<ArchiveResult>(done.last().first()); }
private slots:
    void initTestCase() { QVERIFY(temp.isValid()); QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temp.filePath("settings")); }
    void independentMarks_data() { modes(); }
    void independentMarks() {
        QFETCH(bool, icons); Fixture f(true, icons); f.click(1); QVERIFY(f.files->markedItems().isEmpty()); QCOMPARE(names(f.files->operatedItems()), QStringList{"alpha.txt"});
        f.click(1, Qt::ControlModifier); QCOMPARE(names(f.files->markedItems()), QStringList{"alpha.txt"});
        f.click(3); QCOMPARE(f.files->currentItem()->text(0), QString("space name.txt")); QCOMPARE(names(f.files->operatedItems()), QStringList{"alpha.txt"});
        f.files->selection->selectAll(false); QVERIFY(f.files->markedItems().isEmpty()); QCOMPARE(names(f.files->operatedItems()), QStringList{"space name.txt"});
        f.files->clearSelection(); QCOMPARE(f.files->currentItem()->text(0), QString("space name.txt")); QVERIFY(f.files->operatedItems().isEmpty());
    }
    void insertAndParent_data() { modes(); }
    void insertAndParent() {
        QFETCH(bool, icons); Fixture f(true, icons); f.focus(0); f.key(Qt::Key_Insert); QCOMPARE(f.files->currentItem()->text(0), QString("alpha.txt")); QVERIFY(f.files->markedItems().isEmpty());
        f.key(Qt::Key_Insert); QCOMPARE(names(f.files->markedItems()), QStringList{"alpha.txt"}); QCOMPARE(f.files->currentItem()->text(0), QString("日本語.txt"));
        f.focus(1); f.key(Qt::Key_Insert); QVERIFY(f.files->markedItems().isEmpty()); f.focus(4); f.key(Qt::Key_Insert); QCOMPARE(f.files->currentItem()->text(0), QString("zeta.txt")); QCOMPARE(names(f.files->markedItems()), QStringList{"zeta.txt"});
        f.key(Qt::Key_Insert, Qt::AltModifier); QCOMPARE(names(f.files->markedItems()), QStringList{"zeta.txt"});
    }
    void shiftArrowMarks_data() { modes(); }
    void shiftArrowMarks() {
        QFETCH(bool, icons); Fixture f(true, icons); f.focus(2); f.key(Qt::Key_Shift); f.key(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(names(f.files->markedItems()), QStringList({"space name.txt", "日本語.txt"}));
        f.key(Qt::Key_Up, Qt::ShiftModifier); f.key(Qt::Key_Up, Qt::ShiftModifier); QCOMPARE(names(f.files->markedItems()), QStringList({"alpha.txt", "space name.txt", "日本語.txt"}));
        f.key(Qt::Key_Shift); f.key(Qt::Key_Down, Qt::ShiftModifier); QCOMPARE(names(f.files->markedItems()), QStringList{"space name.txt"});
        f.files->selection->selectAll(false); f.focus(0); f.key(Qt::Key_Shift); f.key(Qt::Key_Down, Qt::ShiftModifier); QCOMPARE(names(f.files->markedItems()), QStringList{"alpha.txt"}); QVERIFY(!f.files->topLevelItem(0)->data(0, PanelMarkRole).toBool());
    }
    void ctrlShiftClickRange_data() { modes(); }
    void ctrlShiftClickRange() {
        QFETCH(bool, icons); Fixture f(true, icons); f.click(1, Qt::ControlModifier); f.click(4, Qt::ControlModifier); QCOMPARE(names(f.files->markedItems()), QStringList({"alpha.txt", "zeta.txt"}));
        f.click(2, Qt::ShiftModifier); QCOMPARE(names(f.files->markedItems()), QStringList({"space name.txt", "zeta.txt", "日本語.txt"}));
        f.files->selection->invert(); QCOMPARE(names(f.files->markedItems()), QStringList{"alpha.txt"}); f.files->selection->selectAll(true); QCOMPARE(f.files->markedItems().size(), 4);
        f.files->selection->selectAll(false); QVERIFY(f.files->markedItems().isEmpty()); QCOMPARE(names(f.files->operatedItems()), QStringList{"日本語.txt"});
    }
    void standardSelection_data() { modes(); }
    void standardSelection() {
        QFETCH(bool, icons); Fixture f(false, icons); f.click(1); f.click(3, Qt::ControlModifier); QCOMPARE(names(f.files->operatedItems()), QStringList({"alpha.txt", "space name.txt"}));
        f.files->selection->invert(); QCOMPARE(names(f.files->operatedItems()), QStringList({"zeta.txt", "日本語.txt"})); f.files->selection->selectAll(false); QVERIFY(f.files->operatedItems().isEmpty());
        f.files->selection->selectAll(true); QCOMPARE(f.files->operatedItems().size(), 4); f.files->selection->selectAll(false); f.click(2); QCOMPARE(names(f.files->operatedItems()), QStringList{"日本語.txt"});
    }
    void paintAndSort() {
        Fixture f(true, false); f.click(1, Qt::ControlModifier); f.click(3); auto marked = f.files->topLevelItem(1);
        class InspectDelegate : public PanelSelectionDelegate { public: using PanelSelectionDelegate::PanelSelectionDelegate; using PanelSelectionDelegate::initStyleOption; } delegate(f.files);
        QStyleOptionViewItem option; delegate.initStyleOption(&option, f.files->indexFromItem(marked)); QCOMPARE(option.backgroundBrush.color(), QColor(255,192,192));
        f.files->setSortingEnabled(true); f.files->sortItems(0, Qt::DescendingOrder); QCOMPARE(names(f.files->markedItems()), QStringList{"alpha.txt"}); QCOMPARE(names(f.files->operatedItems()), QStringList{"alpha.txt"}); QCOMPARE(f.files->currentItem()->text(0), QString("space name.txt"));
        for (int row = 0; row < f.files->topLevelItemCount(); ++row) if (f.files->topLevelItem(row)->text(0) == "space name.txt") f.files->setItemSelected(f.files->topLevelItem(row), true);
        const auto operated = f.files->operatedItems(); QCOMPARE(operated.size(), 2); QCOMPARE(operated[0]->text(0), QString("alpha.txt")); QCOMPARE(operated[1]->text(0), QString("space name.txt"));
    }
    void mainWindowRoundtrip_data() { QTest::addColumn<QString>("format"); QTest::newRow("7z") << QString("7z"); QTest::newRow("zip") << QString("zip"); }
    void mainWindowRoundtrip() {
        QFETCH(QString, format); const auto root = temp.filePath(format); QVERIFY(QDir().mkpath(root));
        for (auto name : {QString("alpha.txt"), QString("日本語.txt"), QString("space name.txt")}) { QFile file(root + '/' + name); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(("contents of " + name).toUtf8()); }
        auto settings = FileManagerSettings::load(); settings.alternativeSelection = true; settings.save(); QSettings().setValue("View/LastPath", root); QSettings().setValue("View/AutoRefresh", false);
        MainWindow window(executable); window.show(); auto files = window.findChild<FileList *>("fileList"); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QCOMPARE(files->selectionMode(), QAbstractItemView::SingleSelection);
        auto item = [&](QString name) { for (int row = 0; row < files->topLevelItemCount(); ++row) if (files->topLevelItem(row)->text(0) == name) return files->topLevelItem(row); return static_cast<QTreeWidgetItem *>(nullptr); };
        QVERIFY(item("alpha.txt") && item("日本語.txt") && item("space name.txt")); files->setItemSelected(item("alpha.txt"), true); files->setItemSelected(item("日本語.txt"), true); files->setCurrentItem(item("space name.txt"), 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        QKeyEvent copy(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier); QApplication::sendEvent(files, &copy); QCOMPARE(QApplication::clipboard()->text().split("\r\n").size(), 2); QVERIFY(!QApplication::clipboard()->text().contains("space name"));
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QCOMPARE(names(files->markedItems()), QStringList({"alpha.txt", "日本語.txt"})); QCOMPARE(files->currentItem()->text(0), QString("space name.txt")); QVERIFY(files->currentItem()->isSelected());
        const auto archive = root + "/selected." + format; QSignalSpy finished(window.findChild<SevenZipProcessBackend *>(), &ArchiveBackend::finished); int dialogs = 0;
        QTimer::singleShot(0, &window, [&] { auto add = window.findChild<AddDialog *>("addDialog"); QVERIFY(add); auto close = qScopeGuard([add] { add->reject(); }); add->findChild<QComboBox *>("archiveFormat")->setCurrentText(format); add->findChild<QLineEdit *>("archivePath")->setText(archive); ++dialogs; add->accept(); close.dismiss(); });
        window.findChild<QAction *>("addAction")->trigger(); QCOMPARE(dialogs, 1); QTRY_VERIFY_WITH_TIMEOUT(finished.size() > 0 && !window.operationBusy(), 30000); auto result = qvariant_cast<ArchiveResult>(finished.last().first()); QVERIFY2(result.success, qPrintable(result.message + result.details)); QVERIFY(QFileInfo::exists(archive));
        SevenZipProcessBackend backend(executable); ArchiveRequest request; request.archive = archive; result = execute(backend, request); QVERIFY(result.success); QCOMPARE(result.entries.size(), 2); for (const auto &entry : result.entries) QVERIFY(entry.path != "space name.txt");
        request.operation = ArchiveOperation::Extract; request.outputDirectory = root + "/out"; request.overwriteMode = "overwrite"; result = execute(backend, request); QVERIFY(result.success);
        for (const auto &name : QStringList{"alpha.txt", "日本語.txt"}) { QFile file(request.outputDirectory + '/' + name); QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), ("contents of " + name).toUtf8()); }
        window.openPath(archive); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); files->setItemSelected(item("alpha.txt"), true); files->setCurrentItem(item("日本語.txt"), 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        window.findChild<QAction *>("refreshAction")->trigger(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QCOMPARE(names(files->markedItems()), QStringList{"alpha.txt"}); QCOMPARE(files->currentItem()->text(0), QString("日本語.txt")); QVERIFY(files->currentItem()->isSelected());
        files->selection->selectAll(false); QKeyEvent copyEmpty(QEvent::KeyPress, Qt::Key_C, Qt::ControlModifier); QApplication::sendEvent(files, &copyEmpty); QVERIFY(QApplication::clipboard()->text().isEmpty()); QCOMPARE(names(files->operatedItems()), QStringList{"日本語.txt"});
        if (format == "zip") {
            window.findChild<QAction *>("commentAction")->trigger(); auto comment = testInput(&window, "commentDialog"); QVERIFY(comment); comment->setTextValue("selection fixture comment"); comment->accept();
            QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 20000); QVERIFY(files->markedItems().isEmpty()); QCOMPARE(files->currentItem()->text(0), QString("日本語.txt")); QVERIFY(files->currentItem()->isSelected());
            request.operation = ArchiveOperation::List; result = execute(backend, request); QVERIFY(result.success); bool found = false; for (const auto &entry : result.entries) if (entry.path == "日本語.txt") { QCOMPARE(entry.properties.value("Comment"), QString("selection fixture comment")); found = true; } QVERIFY(found);
        }
    }
    void independentPanelsAndModeSwitch() {
        const auto root = temp.filePath("panels"); QVERIFY(QDir().mkpath(root));
        for (auto name : {"a.txt", "b.txt"}) { QFile file(root + '/' + name); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("owned panel fixture"); }
        auto settings = FileManagerSettings::load(); settings.alternativeSelection = true; settings.save(); QSettings().setValue("View/LastPath", root); QSettings().setValue("View/Panel2Path", root); QSettings().setValue("View/TwoPanels", true);
        MainWindow window(executable); window.show(); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); auto other = window.findChild<MainWindow *>("secondPanel"); QVERIFY(other); QTRY_VERIFY_WITH_TIMEOUT(!other->operationBusy(), 15000);
        FileList *first = nullptr; auto second = other->findChild<FileList *>("fileList");
        for (auto list : window.findChildren<FileList *>("fileList")) if (!other->isAncestorOf(list)) first = list;
        QVERIFY(first && second && first != second);
        auto find = [](FileList *list, QString name) { for (int row = 0; row < list->topLevelItemCount(); ++row) if (list->topLevelItem(row)->text(0) == name) return list->topLevelItem(row); return static_cast<QTreeWidgetItem *>(nullptr); };
        first->setItemSelected(find(first, "a.txt"), true); second->setItemSelected(find(second, "b.txt"), true); QCOMPARE(names(first->markedItems()), QStringList{"a.txt"}); QCOMPARE(names(second->markedItems()), QStringList{"b.txt"});
        second->selection->invert(); QCOMPARE(names(second->markedItems()), QStringList{"a.txt"}); QCOMPARE(names(first->markedItems()), QStringList{"a.txt"});
        for (bool alternative : {false, true}) {
            int visited = 0; QTimer::singleShot(0, &window, [&] { auto options = window.findChild<OptionsDialog *>(); QVERIFY(options); auto close = qScopeGuard([options] { options->reject(); }); auto check = options->findChild<QCheckBox *>("alternativeSelection"); QVERIFY(check); check->setChecked(alternative); options->findChild<QDialogButtonBox *>("optionsButtons")->button(QDialogButtonBox::Apply)->click(); ++visited; });
            window.findChild<QAction *>("optionsAction")->trigger(); QCOMPARE(visited, 1); QTRY_VERIFY_WITH_TIMEOUT(!window.operationBusy(), 15000); QCOMPARE(first->selectionMode(), alternative ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection); QCOMPARE(names(first->markedItems()), QStringList{"a.txt"}); QCOMPARE(names(second->markedItems()), QStringList{"a.txt"});
        }
        QSettings().setValue("View/TwoPanels", false);
    }
    void cleanupTestCase() { QSettings().clear(); }
};
int main(int argc, char **argv) {
    QLocale::setDefault(QLocale(QLocale::English)); QApplication::setDesktopSettingsAware(false); const auto plugins = qgetenv("PORT_TEST_PLUGIN_ROOT"); if (!plugins.isEmpty()) QCoreApplication::setLibraryPaths({QString::fromUtf8(plugins)}); QApplication app(argc, argv); applyPortAppearance(app); QCoreApplication::setOrganizationName("7zip-mac-port-tests"); QCoreApplication::setApplicationName("panel-selection"); if (argc < 2) return 2; executable = QString::fromLocal8Bit(argv[1]); --argc; for (int n = 1; n < argc; ++n) argv[n] = argv[n+1]; PanelSelectionTests tests; return QTest::qExec(&tests, argc, argv); }
#include "panel-selection.moc"
