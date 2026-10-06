#include "MainWindow.h"
#include "PanelKey.h"
#include "AddressCombo.h"
#include "ComboDialog.h"
#include "OpenProfile.h"
#include "PanelMenu.h"
#include "PanelDrag.h"
#include "WindowPlacement.h"
#include "OverwriteDialog.h"
#include "ArchiveFormats.h"
#include "OpenWith.h"
#include "Benchmark.h"
#include "Help.h"
#include "FileListItem.h"
#include "PanelSelection.h"
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMenuBar>
#include <QMenu>
#include <QStatusBar>
#include <QInputDialog>
#include "CopyDialog.h"
#include <QMessageBox>
#include <QHeaderView>
#include <QMimeData>
#include <QDrag>
#include <QDropEvent>
#include <QCloseEvent>
#include <QShowEvent>
#include <QSettings>
#include <QStyle>
#include <QVBoxLayout>
#include <QToolButton>
#include <QClipboard>
#include <QTimer>
#include <QTimeZone>
#include <QRegularExpression>
#include <QSet>
#include <QBitmap>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QDirIterator>
#include <QSplitter>
#include <QSignalBlocker>
#include <sys/stat.h>
#include "UiLanguage.h"
#include "upstream/UiResourceIds.h"
static void initIconResources() { Q_INIT_RESOURCE(resources); }

namespace {
constexpr int PathRole = Qt::UserRole, DirRole = Qt::UserRole + 1, ParentRole = Qt::UserRole + 2, SizeRole = Qt::UserRole + 3;
bool archiveExtension(QString path) {
    return ArchiveFormats::recognizes(path);
}
QString sizeText(quint64 size) { return officialPanelSize(size); }
class FileDelegate : public PanelSelectionDelegate {
public:
    FileList *files;
    explicit FileDelegate(FileList *list, QObject *owner = nullptr) : PanelSelectionDelegate(owner ? owner : list), files(list) {}
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &index) const override {
        if (index.column() != 0 || !files->property("renameEditing").toBool()) return nullptr;
        auto editor = new QLineEdit(parent); editor->setObjectName("renameEditor");
        const QPersistentModelIndex identity(index);
        connect(editor, &QObject::destroyed, files, [list = files, identity] {
            if (identity.isValid()) if (auto item = list->itemFromIndex(identity)) item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            list->setProperty("renameEditing", false); emit list->renameEditingChanged();
        }); return editor;
    }
    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        auto text = qobject_cast<QLineEdit *>(editor); text->setText(index.data(PanelNameRole).toString()); text->selectAll();
    }
    void setModelData(QWidget *editor, QAbstractItemModel *, const QModelIndex &index) const override {
        files->setProperty("renamePending", true);
        emit files->renameAccepted(QPersistentModelIndex(index), qobject_cast<QLineEdit *>(editor)->text());
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        PanelSelectionDelegate::paint(painter, option, index);
        if (parent()->property("showGrid").toBool()) {
            painter->save(); painter->setPen(QColor(215, 215, 215));
            painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
            painter->drawLine(option.rect.topRight(), option.rect.bottomRight()); painter->restore();
        }
    }
};
}
QItemSelectionModel::SelectionFlags FileList::selectionCommand(const QModelIndex &index, const QEvent *event) const {
    if (property("alternativeSelection").toBool() && index.isValid() && event) return QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;
    return QTreeWidget::selectionCommand(index, event);
}
QList<QTreeWidgetItem *> FileList::markedItems() const {
    return selection ? selection->markedItems() : QTreeWidget::selectedItems();
}
QList<QTreeWidgetItem *> FileList::operatedItems() const { return selection ? selection->operatedItems() : QTreeWidget::selectedItems(); }
void FileList::setItemSelected(QTreeWidgetItem *item, bool selected) {
    if (!item || item->data(0, ParentRole).toBool()) return;
    if (property("alternativeSelection").toBool()) { if (item->data(0, PanelMarkRole).toBool() != selected) { item->setData(0, PanelMarkRole, selected); emit marksChanged(); } }
    else item->setSelected(selected);
}
bool MainWindow::dataOperationBusy() const { const auto other = otherPanel(); return panelBusy() || (other && other->panelBusy()); }
bool MainWindow::operationBusy() const { const auto other = otherPanel(); return dataOperationBusy() || filesystemRead || (files && files->sortingBusy()) || (other && (other->filesystemRead || (other->files && other->files->sortingBusy()))); }
void FileList::dragEnterEvent(QDragEnterEvent *e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void FileList::dragMoveEvent(QDragMoveEvent *e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void FileList::dropEvent(QDropEvent *e) {
    QStringList paths;
    for (const auto &url : e->mimeData()->urls()) if (url.isLocalFile()) paths << url.toLocalFile();
    if (!paths.isEmpty()) { e->acceptProposedAction(); emit filesDropped(paths); }
}
void FileList::startDrag(Qt::DropActions) {
    if (archiveView) { emit archiveDragRequested(); return; }
    emit filesystemDragRequested();
}
MainWindow::MainWindow(QString backendPath, QWidget *parent, bool isEmbedded) : QMainWindow(parent), backend(backendPath, this), fileOps(this), versionControl(this), folderStatistics(this), directoryScanner(this), embedded(isEmbedded), backendExecutable(backendPath), settings(FileManagerSettings::load()) {
    externalDecisions.setSingleShot(true); externalDecisions.setInterval(100);
    connect(&externalDecisions, &QTimer::timeout, this, &MainWindow::considerExternalEdits);
    if (embedded) setWindowFlags(Qt::Widget);
    static const bool resourcesReady = [] { initIconResources(); return true; }(); Q_UNUSED(resourcesReady);
    setWindowTitle("7-Zip File Manager"); setWindowIcon(QIcon(":/icons/FM.png")); setObjectName("mainWindow"); resize(980, 650);
    menuBar()->setNativeMenuBar(false);
    buildMenus(); buildToolbar();
    auto central = new QWidget(this); auto layout = new QVBoxLayout(central); layout->setContentsMargins(3, 3, 3, 0); layout->setSpacing(3);
    auto pathRow = new QHBoxLayout; pathRow->setSpacing(3); auto parentButton = new QToolButton(this); parentButton->setText("↑"); parentButton->setToolTip("Up One Level (Backspace)"); parentButton->setFixedSize(28, 25); pathRow->addWidget(parentButton); connect(parentButton, &QToolButton::clicked, this, &MainWindow::up);
    pathCombo = new AddressCombo(this); pathCombo->currentLocation = [this] { return archivePath.isEmpty() ? fsPath : archiveLocation(); };
    pathBox = pathCombo->lineEdit(); pathBox->setObjectName("currentPath"); pathRow->addWidget(pathCombo);
    connect(pathBox, &QLineEdit::returnPressed, this, [this] { submitAddress(pathBox->text()); });
    connect(pathCombo, &QComboBox::activated, this, [this](int index) { submitAddress(pathCombo->itemData(index).toString()); });
    files = new FileList(this); files->setObjectName("fileList"); files->setRootIsDecorated(false); files->setAlternatingRowColors(false); files->setSelectionMode(QAbstractItemView::ExtendedSelection); files->setSelectionBehavior(QAbstractItemView::SelectRows); files->setSortingEnabled(true); files->setUniformRowHeights(true); files->setIconSize(QSize(16, 16));
    auto sortHeader = new PanelSortHeader(files); files->setHeader(sortHeader);
    connect(sortHeader, &PanelSortHeader::propertyClicked, this, [this](int column) {
        sortByProperty(files->headerItem()->data(column, ColumnPropertyRole).toUInt(), files->headerItem()->data(column, ColumnRawRole).toBool());
    });
    files->setItemDelegate(new FileDelegate(files));
    files->setDragEnabled(true); files->setAcceptDrops(true); files->setDragDropMode(QAbstractItemView::DragDrop); files->header()->setSectionsMovable(true); files->header()->setStretchLastSection(true); setCentralWidget(central); setupViews(layout, pathRow);
    files->setEditTriggers(QAbstractItemView::NoEditTriggers); icons->setEditTriggers(QAbstractItemView::NoEditTriggers);
    icons->setItemDelegate(new FileDelegate(files, icons));
    connect(files, &FileList::renameEditingChanged, this, [this] { if (!closingRequested) { updateState(); if (auto other = otherPanel()) other->updateState(); } });
    connect(files, &FileList::renameAccepted, this, [this](QPersistentModelIndex identity, QString name) {
        // Windows defers its reload after LVN_ENDLABELEDIT. Leave the model
        // intact until Qt has closed the editor and finished commitData.
        QTimer::singleShot(0, this, [this, identity, name] {
            files->setProperty("renameEditing", false); files->setProperty("renamePending", false);
            commitRename(identity, name); updateState(); if (auto other = otherPanel()) other->updateState();
        });
    });
    qApp->installEventFilter(this);
    // Enter/Backspace must remain editing keys in the address field. Keep the
    // upstream file-list shortcuts, scoped to the list that handles them.
    for (const auto &key : {"open", "outside", "up"}) {
        actions[key]->setShortcutContext(Qt::WidgetShortcut); files->addAction(actions[key]);
    }
    connect(files, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) { if (!settings.singleClick) openSelectedItems(true); });
    connect(files, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item, int) {
        if (settings.singleClick && QApplication::keyboardModifiers() == Qt::NoModifier) {
            QPersistentModelIndex index = files->indexFromItem(item);
            QTimer::singleShot(0, this, [this, index] { if (index.isValid()) openSelectedItems(true); });
        }
    });
    connect(files, &QTreeWidget::itemSelectionChanged, this, &MainWindow::updateState);
    connect(files, &FileList::marksChanged, this, &MainWindow::updateState);
    connect(files, &FileList::sortingStateChanged, this, [this](bool busy) {
        updateState(); if (auto other=otherPanel()) other->updateState();
        if (!busy && !closingRequested && (openCommandActive || !openCommands.isEmpty())) QTimer::singleShot(0,this,&MainWindow::continueOpenCommands);
    });
    connect(files, &QTreeWidget::currentItemChanged, this, &MainWindow::updateState);
    connect(files, &FileList::filesDropped, this, [this](QStringList paths) { if (operationBusy()) return; if (archivePath.isEmpty() && paths.size() == 1 && archiveExtension(paths[0])) openPath(paths[0]); else add(paths); });
    connect(files, &FileList::archiveDragRequested, this, [this] { openExternal(true); });
    dragController = new PanelDragController(this);
    files->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(files, &QWidget::customContextMenuRequested, this, [this](QPoint point) {
        if (auto item = files->itemAt(point); item && !item->isSelected()) { files->clearSelection(); files->setCurrentItem(item); item->setSelected(true); }
        showFileContextMenu(files->viewport()->mapToGlobal(point));
    });
    connect(&backend, &ArchiveBackend::progress, this, [this](int p, quint64 done, quint64 total, QString file) { if (progressDialog) progressDialog->update(p, done, total, file); });
    connect(&backend, &ArchiveBackend::progressDetails, this, [this](const ArchiveProgress &progress) { if (progressDialog) progressDialog->updateDetails(progress); });
    connect(&backend, &ArchiveBackend::output, this, [this](QString text) { if (progressDialog) progressDialog->append(text); });
    connect(&backend, &ArchiveBackend::operationError, this, [this](ArchiveOperationError error) { if (progressDialog) progressDialog->reportError(error); });
    connect(&backend, &ArchiveBackend::pausedChanged, this, [this](bool paused) { if (progressDialog) progressDialog->setPaused(paused); });
    connect(&backend, &ArchiveBackend::pauseAvailabilityChanged, this, [this](bool available) { if (progressDialog) progressDialog->setPauseAvailable(available); });
    connect(&backend, &ArchiveBackend::finished, this, [this](ArchiveResult result) { if (overwriteDialog) overwriteDialog->close(); onFinished(result); });
    connect(&backend, &ArchiveBackend::overwriteRequested, this, [this](OverwriteConflict conflict) { askOverwrite(conflict, false); });
    connect(&fileOps, &FileOperations::overwriteRequested, this, [this](OverwriteConflict conflict) { askOverwrite(conflict, true); });
    connect(&fileOps, &FileOperations::progress, this, [this](QString path) { if (progressDialog) progressDialog->update(-1, 0, 0, path); });
    connect(&fileOps, &FileOperations::transferDestinationPrepared, this, [this](QString path) { if (transferOperation && transferOperation->phase == TransferOperation::Filesystem) CopyDialog::remember(path); });
    connect(&fileOps, &FileOperations::pausedChanged, this, [this](bool paused) { if (progressDialog) progressDialog->setPaused(paused); });
    connect(&fileOps, &FileOperations::pauseAvailabilityChanged, this, [this](bool available) { if (progressDialog) progressDialog->setPauseAvailable(available); });
    connect(&fileOps, &FileOperations::byteProgress, this, [this](quint64 done, quint64 total, QString path) { if (progressDialog) progressDialog->update(total ? int(100.0 * done / total) : -1, done, total, path, false); });
    connect(&fileOps, &FileOperations::finished, this, [this](QString error) {
        if (finishFilesystemTransfer(error)) return;
        if (overwriteDialog) overwriteDialog->close(); if (progressDialog) progressDialog->finishFile(error); updateState();
        if (fileOperationSelection) {
            auto selection = *fileOperationSelection; fileOperationSelection.reset();
            if (error.isEmpty()) { selection.focused = fileOperationFocus; if (!settings.alternativeSelection) selection.marked.clear(); selection.marked << fileOperationFocus; selection.focusedSelected = true; }
            showFilesystem(fsPath, [this, selection] { restoreBrowseSelection(selection); });
        } else refresh();
    });
    setupFolderStatistics(); setupFilesystem(); setupComments(); setupVersionControl();
    applySettings(); const QString last = QSettings().value(embedded ? "View/Panel2Path" : "View/LastPath", QDir::homePath()).toString(); fsPath = QFileInfo(last).isDir() ? last : QDir::homePath(); showFilesystem(fsPath);
    if (!embedded) { restoreGeometry(QSettings().value("View/Geometry").toByteArray()); setTwoPanels(QSettings().value("View/TwoPanels", false).toBool()); } else { menuBar()->hide(); toolbar->hide(); statusBar()->hide(); }
    uiReady = true;
}
void MainWindow::askOverwrite(OverwriteConflict conflict, bool filesystem) {
    if (overwriteDialog) overwriteDialog->close();
    auto dialog = new OverwriteDialog(conflict, this); dialog->setAttribute(Qt::WA_DeleteOnClose); overwriteDialog = dialog;
    connect(dialog, &QDialog::finished, this, [this, dialog, id = conflict.id, filesystem] {
        const auto answer = dialog->answer();
        if (overwriteDialog == dialog) overwriteDialog.clear();
        if (filesystem) fileOps.resolveOverwrite(id, answer); else backend.resolveOverwrite(id, answer);
    });
    dialog->open();
}
void MainWindow::showFileContextMenu(QPoint globalPosition) {
    updateState(); QMenu menu(this); menu.setObjectName("fileContextMenu");
    if (archivePath.isEmpty() && !selectedPaths().isEmpty()) {
        auto shell = menu.addMenu("7-Zip"); shell->setObjectName("sevenZipContextMenu");
        const auto paths = selectedPaths(); populateOpenWithContextMenu(shell, paths, [this, paths](QString id) { executeOpenWith(id, paths); });
        shell->setEnabled(!operationBusy());
    }
    const auto operated = files->operatedItems(); FileMenuState state;
    state.filesystem = archivePath.isEmpty(); state.readOnly = !state.filesystem && !canUpdateArchive();
    state.hashFolder = archiveType.compare("Hash", Qt::CaseInsensitive) == 0;
    state.count = operated.size(); state.diff = settings.diff;
    state.versionStore = QSettings().value("7vc").toString(); state.filePath = selectedPaths().value(0);
    for (auto item : operated) if (item->data(0, DirRole).toBool()) state.allFiles = false;
    state.alternateStreams = !state.filesystem && alternateArchiveDirectory() >= 0;
    appendOfficialFileMenu(&menu, menuBar()->actions().first()->menu(), state);
    UiLanguage::apply(&menu); menu.exec(globalPosition);
}
QAction *MainWindow::action(QString name, QString text, QString shortcut, std::function<void()> callback) {
    auto a = new QAction(text, this); a->setObjectName(name + "Action"); a->setMenuRole(QAction::NoRole);
    if (!shortcut.isEmpty()) a->setShortcut(QKeySequence(shortcut));
    connect(a, &QAction::triggered, this, [this, name, callback] { if (secondPanel && secondPanel->isAncestorOf(QApplication::focusWidget())) activePanel = 1; if (secondPanel && activePanel == 1 && !QStringList{"options", "twoPanels", "exit", "help", "about", "autoRefresh"}.contains(name) && !name.startsWith("toolbar") && !name.startsWith("time") && secondPanel->actions.contains(name)) { secondPanel->actions[name]->trigger(); updateState(); } else callback(); }); actions[name] = a; return a;
}
void MainWindow::buildMenus() {
    auto file = menuBar()->addMenu("&File");
    file->addAction(action("open", "&Open", "Return", [this] { openSelectedItems(true); }));
    file->addAction(action("inside", "Open &Inside", "Ctrl+PgDown", [this] { if (files->currentItem()) activateItem(files->currentItem(), true); }));
    file->addAction(action("insideOne", "Open Inside *", "", [this] { if (files->currentItem()) activateItem(files->currentItem(), true, "*"); }));
    file->addAction(action("insideParser", "Open Inside #", "", [this] { if (files->currentItem()) activateItem(files->currentItem(), true, "#"); }));
    file->addAction(action("outside", "Open O&utside", "Shift+Return", [this] { openSelectedItems(false); }));
    file->addAction(action("view", "&View", "F3", [this] { viewFocusedItem(); })); file->addAction(action("edit", "&Edit", "F4", [this] { openExternal(false, settings.editor, true); })); file->addSeparator();
    file->addAction(action("rename", "Rena&me", "F2", [this] { rename(); })); file->addAction(action("copy", "&Copy To...", "F5", [this] { transfer(false); })); file->addAction(action("move", "&Move To...", "F6", [this] { transfer(true); })); file->addAction(action("delete", "&Delete", "Delete", [this] { remove(); })); file->addSeparator();
    file->addAction(action("split", "Split file...", "", [this] { splitFile(); })); file->addAction(action("combine", "Combine files...", "", [this] { combineFiles(); })); file->addSeparator();
    file->addAction(action("info", "P&roperties", "Alt+Return", [this] { info(); })); file->addAction(action("comment", "Comment...", "Ctrl+Z", [this] { comment(); }));
    auto crc = file->addMenu("CRC"); for (const auto &method : checksumMethods()) crc->addAction(action("hash" + method.second, method.first, "", [this, method] { calculateHash(method.second); }));
    file->addAction(action("diff", "Diff", "", [this] { diffFiles(); })); file->addSeparator();
    file->addAction(action("folder", "Create &Folder", "F7", [this] { newFolder(); })); file->addAction(action("newfile", "Create File", "Ctrl+N", [this] { newFile(); })); file->addSeparator(); file->addAction(action("link", "Link...", "", [this] { link(); })); file->addAction(action("alternateStreams", "Alternate streams", "", [this] { openAlternateStreams(); })); file->addSeparator();
    file->addAction(action("exit", "E&xit", "Alt+F4", [this] { if (close() && !embedded) qApp->quit(); }));
    const QStringList versionKeys{"verEdit", "verCommit", "verRevert", "verDiff"};
    const QStringList versionLabels{"Ver Edit (&1)", "Ver Commit", "Ver Revert", "Ver Diff (&0)"};
    for (int n = 0; n < 4; ++n) { auto command = action(versionKeys[n], versionLabels[n], "", [this, n] { versionCommand(VersionCommand(n)); }); command->setProperty("versionCommand", true); command->setVisible(false); file->addAction(command); }
    connect(file, &QMenu::aboutToShow, this, [this, file] {
        updateState(); auto panel = secondPanel && activePanel == 1 ? secondPanel : this;
        FileMenuState state; state.programMenu = true; state.filesystem = panel->archivePath.isEmpty(); state.readOnly = !state.filesystem && !panel->canUpdateArchive();
        state.hashFolder = panel->archiveType.compare("Hash", Qt::CaseInsensitive) == 0; state.count = panel->selectedPaths().size(); state.diff = panel->settings.diff;
        state.filePath = panel->selectedPaths().value(0); state.versionStore = QSettings().value("7vc").toString();
        state.alternateStreams = !state.filesystem && panel->alternateArchiveDirectory() >= 0;
        for (auto item : panel->selectedItems()) if (item->data(0, DirRole).toBool()) state.allFiles = false;
        QMenu filtered; appendOfficialFileMenu(&filtered, file, state);
        for (auto source : file->actions()) if (!source->isSeparator() && !source->menu()) {
            for (auto copy : filtered.actions()) if (copy->objectName() == source->objectName()) source->setEnabled(copy->isEnabled());
        }
    });
    auto edit = menuBar()->addMenu("&Edit");
    auto all = action("selectall", "Select &All", "Ctrl+A", [this] { files->selection->selectAll(true); }); all->setShortcuts({QKeySequence("Ctrl+A"), QKeySequence("Shift++")}); edit->addAction(all);
    edit->addAction(action("deselect", "&Deselect All", "Shift+-", [this] { files->selection->selectAll(false); }));
    edit->addAction(action("invert", "&Invert Selection", "*", [this] { files->selection->invert(); }));
    auto selectPattern = [this](bool select) {
        if (operationBusy()) return;
        bool ok; auto mask = ComboDialog::getText(this, select ? 6402 : 6403, 6404, "*", &ok);
        if (ok) files->selection->selectMask(mask, select);
    };
    edit->addAction(action("selectpattern", "Select...", "+", [selectPattern] { selectPattern(true); })); edit->addAction(action("deselectpattern", "Deselect...", "-", [selectPattern] { selectPattern(false); }));
    edit->addAction(action("selecttype", "Select by Type", "Alt++", [this] { selectByType(true); })); edit->addAction(action("deselecttype", "Deselect by Type", "Alt+-", [this] { selectByType(false); }));
    auto view = menuBar()->addMenu("&View"); buildViewMenus(view);
    auto favorites = menuBar()->addMenu("F&avorites"); auto store = favorites->addMenu("Add folder to Favorites as");
    for (int n = 0; n < 10; ++n) {
        auto label = "Bookmark " + QString::number(n);
        store->addAction(action("storeBookmark" + QString::number(n), label, "Alt+Shift+" + QString::number(n), [this, n] { saveBookmark(n); }));
        favorites->addAction(action("bookmark" + QString::number(n), label, "Alt+" + QString::number(n), [this, n] { openBookmark(n); }));
    }
    connect(favorites, &QMenu::aboutToShow, this, &MainWindow::updateBookmarks);
    auto tools = menuBar()->addMenu("&Tools"); tools->addAction(action("options", "&Options...", "", [this] { options(); })); tools->addSeparator(); tools->addAction(action("benchmark", "Benchmark", "", [this] { BenchmarkDialog dialog(backendExecutable, this); dialog.exec(); })); tools->addSeparator(); tools->addAction(action("temporaryFiles", "Delete Temporary Files...", "", [this] { deleteTemporaryFiles(); }));
    auto help = menuBar()->addMenu("&Help");
    help->addAction(action("help", "Contents...", "F1", [this] { Help::show(this); }));
    help->addSeparator(); help->addAction(action("about", "About 7-Zip...", "", [this] { Help::about(this); }));
    action("add", "Add", "", [this] { add(); }); action("extract", "Extract", "", [this] { extract(); }); action("test", "Test", "", [this] { test(); });
    // MyLoadMenu.cpp maps menu commands to these Lang2 resource IDs.
    const QMap<QString, unsigned> languageIds{
        {"open", OfficialUi::IDM_OPEN},
        {"inside", OfficialUi::IDM_OPEN_INSIDE},
        {"insideOne", OfficialUi::IDM_OPEN_INSIDE_ONE},
        {"insideParser", OfficialUi::IDM_OPEN_INSIDE_PARSER},
        {"outside", OfficialUi::IDM_OPEN_OUTSIDE},
        {"view", OfficialUi::IDM_FILE_VIEW},
        {"edit", OfficialUi::IDM_FILE_EDIT},
        {"rename", OfficialUi::IDM_RENAME},
        {"copy", OfficialUi::IDM_COPY_TO},
        {"move", OfficialUi::IDM_MOVE_TO},
        {"delete", OfficialUi::IDM_DELETE},
        {"split", OfficialUi::IDM_SPLIT},
        {"combine", OfficialUi::IDM_COMBINE},
        {"info", OfficialUi::IDM_PROPERTIES},
        {"comment", OfficialUi::IDM_COMMENT},
        {"diff", OfficialUi::IDM_DIFF},
        {"folder", OfficialUi::IDM_CREATE_FOLDER},
        {"newfile", OfficialUi::IDM_CREATE_FILE},
        {"link", OfficialUi::IDM_LINK},
        {"alternateStreams", OfficialUi::IDM_ALT_STREAMS},
        {"exit", 557u},
        {"selectall", OfficialUi::IDM_SELECT_ALL},
        {"deselect", OfficialUi::IDM_DESELECT_ALL},
        {"invert", OfficialUi::IDM_INVERT_SELECTION},
        {"selectpattern", OfficialUi::IDM_SELECT},
        {"deselectpattern", OfficialUi::IDM_DESELECT},
        {"selecttype", OfficialUi::IDM_SELECT_BY_TYPE},
        {"deselecttype", OfficialUi::IDM_DESELECT_BY_TYPE},
        {"mode0", OfficialUi::IDM_VIEW_LARGE_ICONS},
        {"mode1", OfficialUi::IDM_VIEW_SMALL_ICONS},
        {"mode2", OfficialUi::IDM_VIEW_LIST},
        {"mode3", OfficialUi::IDM_VIEW_DETAILS},
        {"sort0", 1004u},
        {"sort1", 1020u},
        {"sort2", 1012u},
        {"sort3", 1007u},
        {"unsorted", OfficialUi::IDM_VIEW_ARANGE_NO_SORT},
        {"flat", OfficialUi::IDM_VIEW_FLAT_VIEW},
        {"twoPanels", OfficialUi::IDM_VIEW_TWO_PANELS},
        {"toolbarArchive", OfficialUi::IDM_VIEW_ARCHIVE_TOOLBAR},
        {"toolbarStandard", OfficialUi::IDM_VIEW_STANDARD_TOOLBAR},
        {"toolbarLarge", OfficialUi::IDM_VIEW_TOOLBARS_LARGE_BUTTONS},
        {"toolbarText", OfficialUi::IDM_VIEW_TOOLBARS_SHOW_BUTTONS_TEXT},
        {"root", OfficialUi::IDM_OPEN_ROOT_FOLDER},
        {"up", OfficialUi::IDM_OPEN_PARENT_FOLDER},
        {"history", OfficialUi::IDM_FOLDERS_HISTORY},
        {"refresh", OfficialUi::IDM_VIEW_REFRESH},
        {"autoRefresh", OfficialUi::IDM_VIEW_AUTO_REFRESH},
        {"options", OfficialUi::IDM_OPTIONS},
        {"benchmark", OfficialUi::IDM_BENCHMARK},
        {"temporaryFiles", OfficialUi::IDM_TEMP_DIR},
        {"help", OfficialUi::IDM_HELP_CONTENTS},
        {"about", OfficialUi::IDM_ABOUT},
        {"add", OfficialUi::IDS_ADD},
        {"extract", OfficialUi::IDS_EXTRACT},
        {"test", OfficialUi::IDS_TEST}};
    for (auto it = languageIds.cbegin(); it != languageIds.cend(); ++it) UiLanguage::bind(actions.value(it.key()), it.value());
    const unsigned topIds[]{OfficialUi::IDM_FILE, OfficialUi::IDM_EDIT, OfficialUi::IDM_VIEW, OfficialUi::IDM_FAVORITES, OfficialUi::IDM_TOOLS, OfficialUi::IDM_HELP};
    for (int n = 0; n < 6; ++n) UiLanguage::bind(menuBar()->actions()[n], topIds[n]);
    UiLanguage::bind(crc->menuAction(), OfficialUi::IDM_CRC); UiLanguage::bind(store->menuAction(), OfficialUi::IDM_ADD_TO_FAVORITES);
    for (auto a : actions) addAction(a);
}
void MainWindow::buildToolbar() {
    toolbar = addToolBar("Toolbar"); toolbar->setObjectName("archiveToolbar"); toolbar->setMovable(false); toolbar->setIconSize(QSize(24, 24)); toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    const QStringList keys{"add", "extract", "test", "copy", "move", "delete", "info"};
    const QStringList names{"Add", "Extract", "Test", "Copy", "Move", "Delete", "Info"};
    for (int n = 0; n < keys.size(); ++n) {
        QPixmap icon(":/icons/" + names[n] + "2.bmp"); icon.setMask(icon.createMaskFromColor(QColor(255, 0, 255)));
        QAction *a = actions[keys[n]]; a->setIcon(QIcon(icon));
        auto toolAction = new QAction(a->icon(), names[n], toolbar);
        const unsigned toolbarIds[]{OfficialUi::IDS_ADD, OfficialUi::IDS_EXTRACT, OfficialUi::IDS_TEST, OfficialUi::IDS_BUTTON_COPY, OfficialUi::IDS_BUTTON_MOVE, OfficialUi::IDS_BUTTON_DELETE, OfficialUi::IDS_BUTTON_INFO};
        UiLanguage::bind(toolAction, toolbarIds[n], true); toolAction->setToolTip(a->text());
        connect(toolAction, &QAction::triggered, a, &QAction::trigger);
        connect(a, &QAction::changed, toolAction, [a, toolAction] { toolAction->setEnabled(a->isEnabled()); });
        auto button = new QToolButton(toolbar); button->setObjectName(keys[n] + "Button"); button->setDefaultAction(toolAction); button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon); button->setIconSize(QSize(24, 24)); button->setMinimumSize(56, 50); toolbar->addWidget(button);
    }
}
void MainWindow::showArchive() {
    cancelFilesystemRead();
    QSignalBlocker headerChanges(files->header());
    loadingView = true; files->archiveView = true; files->setProperty("archiveView", true); files->setSortingEnabled(false); files->clear();
    const auto columns = archiveFolderIndex.columns(flatView);
    const auto columnDefinitions=std::make_shared<const QList<ArchivePropertyDefinition>>(columns);
    QSettings displaySettings; const int timePrecision=displaySettings.value("View/TimePrecision",1).toInt(); const bool utc=displaySettings.value("View/UTC",false).toBool();
    const auto folderIcon=style()->standardIcon(QStyle::SP_DirIcon), fileIcon=style()->standardIcon(QStyle::SP_FileIcon);
    QList<PanelIconRequest> nativeIcons;
    files->setProperty("nativeMetadata", archiveFolderIndex.native());
    if (!columns.isEmpty()) {
        files->setColumnCount(columns.size()); QStringList labels;
        for (const auto &column : columns) labels << UiLanguage::propertyName(column.id, column.name, false);
        files->setHeaderLabels(labels);
        for (int i = 0; i < columns.size(); ++i) {
            files->headerItem()->setData(i, ColumnPropertyRole, columns[i].id); files->headerItem()->setData(i, ColumnRawRole, columns[i].raw);
            files->headerItem()->setData(i, ColumnWidthRole, officialColumnWidth(columns[i].id,columns[i].type));
            files->setColumnHidden(i, false); files->setColumnWidth(i, officialColumnWidth(columns[i].id, columns[i].type));
        }
        if (settings.showDots) { auto parent = new FileItem(files, {".."}); parent->setData(0, ParentRole, true); parent->setData(0, DirRole, true); parent->setIcon(0, style()->standardIcon(QStyle::SP_ArrowUp)); }
        auto rows = archiveFolderIndex.directoryRows(archiveDirectory, flatView);
        if (!archiveFolderIndex.hasDirectories()) {
        QSet<QString> folders;
        for (const auto &entry : archiveEntries) {
            QString path = entry.path; path.replace('\\', '/');
            if (!path.startsWith(archivePrefix)) continue;
            QString rest = path.mid(archivePrefix.size()); while (rest.endsWith('/')) rest.chop(1); if (rest.isEmpty()) continue;
            const auto slash = flatView ? -1 : rest.indexOf('/'); const auto name = slash < 0 ? rest : rest.left(slash);
            const bool directory = slash >= 0 || entry.directory;
            if (directory && folders.contains(name)) continue;
            if (directory) folders.insert(name);
            const auto logicalPath = archivePrefix + name + (directory ? "/" : "");
            const qint64 nativeIndex = directory ? (archiveFolderIndex.entry(logicalPath) ? archiveFolderIndex.entry(logicalPath)->archiveIndex : -1) : entry.archiveIndex;
            ArchiveRow row; row.name = flatView ? name.section('/', -1) : name; row.path = logicalPath; row.archiveIndex = nativeIndex; row.childDirectory = directory ? 0 : -1;
            row.properties = archiveFolderIndex.itemProperties(logicalPath, flatView, archivePrefix, nativeIndex); rows.append(row);
        }
        }
        qint64 rowOrder = 0;
        for (const auto &row : rows) {
            const bool directory = row.childDirectory >= 0;
            const auto logicalPath = row.path + (directory && !row.path.endsWith('/') ? "/" : "");
            const auto values = archiveFolderIndex.hasDirectories() ? archiveFolderIndex.rowProperties(row.identity, flatView, archiveDirectory) : row.properties;
            auto item = new FileItem(files); item->setData(0, PathRole, logicalPath); item->setData(0, DirRole, directory); item->setData(0, Qt::UserRole + 110, row.archiveIndex);
            if (archiveFolderIndex.hasDirectories()) { item->setData(0, Qt::UserRole + 111, row.childDirectory); item->setData(0, Qt::UserRole + 112, row.identity.directory); item->setData(0, Qt::UserRole + 113, row.identity.item); item->setData(0, Qt::UserRole + 114, row.alternateDirectory); }
            for (int column = 0; column < columns.size(); ++column) {
                const auto &definition = columns[column];
                item->setTextAlignment(column, officialColumnAlignment(definition.id, definition.type));
                for (const auto &property : values) if (property.id == definition.id && property.raw == definition.raw) {
                    if (!property.raw && !property.number.isEmpty()) item->setData(column, SizeRole, property.number.toULongLong());
                    break;
                }
            }
            item->setData(0,PanelNameRole,row.name); item->setText(0,officialPanelName(row.name)); item->setDeferredProperties(values,columnDefinitions,timePrecision,utc);
            item->setData(0,PanelCopyNameRole,row.copyName.isNull()?row.name:row.copyName);
            item->setData(0,PanelIconIdentityRole,int(rowOrder));
            if (settings.realIcons) nativeIcons.append({int(rowOrder),row.name,directory,true});
            QString prefix; for (const auto &property : values) if (!property.raw && property.id == OfficialSort::kpidPrefix) prefix = property.value;
            item->setSortProperties(values, rowOrder++, prefix, true);
            item->setIcon(0, directory ? folderIcon : fileIcon);
        }
    } else {
    files->setColumnCount(10);
    files->setHeaderLabels({"Name", "Size", "Packed Size", "Modified", "Attributes", "CRC", "Encrypted", "Method", "Type", "Comment"});
    const QList<quint32> legacyProperties{OfficialSort::kpidName, OfficialSort::kpidSize, OfficialSort::kpidPackSize, OfficialSort::kpidMTime, OfficialSort::kpidAttrib, OfficialSort::kpidCRC, OfficialSort::kpidEncrypted, OfficialSort::kpidMethod, OfficialSort::kpidExtension, OfficialSort::kpidComment};
    for (int n=0;n<legacyProperties.size();++n) { files->headerItem()->setData(n,ColumnPropertyRole,legacyProperties[n]); files->headerItem()->setData(n,ColumnRawRole,false); files->headerItem()->setData(n,ColumnWidthRole,n==0?235:n==3?160:100); }
    for (int n = 0; n < files->columnCount(); ++n) { files->setColumnHidden(n, false); files->setColumnWidth(n, n == 0 ? 235 : n == 3 ? 160 : 100); }
    files->setColumnHidden(9, true);
    if (settings.showDots) { auto parent = new FileItem(files, {".."}); parent->setData(0, ParentRole, true); parent->setData(0, DirRole, true); parent->setIcon(0, style()->standardIcon(QStyle::SP_ArrowUp)); }
    QSet<QString> seen;
    qint64 legacyOrder = 0;
    for (const auto &e : archiveEntries) {
        QString entryPath = e.path; entryPath.replace('\\', '/');
        if (!entryPath.startsWith(archivePrefix)) continue;
        QString rest = entryPath.mid(archivePrefix.size()); if (rest.isEmpty()) continue;
        int slash = flatView ? -1 : rest.indexOf('/'); QString name = slash < 0 ? rest : rest.left(slash);
        if (name.isEmpty() || seen.contains(name)) continue; seen.insert(name);
        bool dir = slash >= 0 || e.directory;
        auto item = new FileItem(files, {name, dir ? "" : sizeText(e.size), dir ? "" : sizeText(e.packed), timestamp(QDateTime::fromString(e.modified.left(19), "yyyy-MM-dd HH:mm:ss"), e.modified.mid(20)), e.attributes, dir ? "" : e.crc, e.encrypted ? "+" : "-", dir ? "" : e.method, dir ? UiLanguage::text("Folder") : QFileInfo(name).suffix().toUpper(), slash < 0 ? e.properties.value("Comment") : QString()});
        item->setData(0,PanelNameRole,name); item->setText(0,officialPanelName(name));
        item->setData(0, PathRole, archivePrefix + name + (dir ? "/" : "")); item->setData(0, DirRole, dir); item->setData(1, SizeRole, e.size); item->setData(2, SizeRole, e.packed);
        auto values=e.orderedProperties;
        for (const auto &pair : {qMakePair(OfficialSort::kpidSize,dir?0:e.size),qMakePair(OfficialSort::kpidPackSize,dir?0:e.packed)}) { ArchiveProperty p; p.id=pair.first; p.type=21; p.number=QString::number(pair.second); values.append(p); }
        for (const auto &pair : {qMakePair(OfficialSort::kpidMethod,e.method),qMakePair(OfficialSort::kpidComment,e.properties.value("Comment")),qMakePair(OfficialSort::kpidAttrib,e.attributes)}) { ArchiveProperty p; p.id=pair.first; p.type=8; p.value=pair.second; values.append(p); }
        ArchiveProperty crc; crc.id=OfficialSort::kpidCRC; crc.type=19; crc.number=QString::number(e.crc.toULongLong(nullptr,16)); values.append(crc);
        ArchiveProperty encrypted; encrypted.id=OfficialSort::kpidEncrypted; encrypted.type=11; encrypted.number=e.encrypted?"1":"0"; values.append(encrypted);
        const auto modified=QDateTime::fromString(e.modified.left(19),"yyyy-MM-dd HH:mm:ss");
        if (modified.isValid()) { ArchiveProperty date; date.id=OfficialSort::kpidMTime; date.type=64; date.fileTime=QString::number(quint64(modified.toSecsSinceEpoch()+11644473600LL)*10000000+e.modified.mid(20).left(9).leftJustified(9,'0').toULongLong()/100); values.append(date); }
        item->setSortProperties(values,legacyOrder++,flatView?rest.left(qMax(0,rest.lastIndexOf('/')+1)):QString(),true);
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter); item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter); item->setIcon(0, style()->standardIcon(dir ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
    }
    }
    pathBox->setText(archiveLocation()); finishBrowse();
    if (!nativeIcons.isEmpty()) files->loadNativeIcons(std::move(nativeIcons));
}
void MainWindow::refresh() {
    if (dataOperationBusy()) return;
    const auto selection = saveBrowseSelection();
    if (filesystemRead || archivePath.isEmpty()) {
        showFilesystem(requestedDirectory(), [this, selection] { restoreBrowseSelection(selection); });
    } else {
        // openPath treats the current virtual archive path as navigation. A
        // refresh must re-list the physical archive without dropping its prefix.
        pendingArchive = archivePath; pendingArchiveTail.clear(); pendingReopen = false; refreshSelection = selection;
        ArchiveRequest request; request.archive = archivePath; request.password = archivePassword; request.readMode = archiveReadMode; run(request);
    }
}
QList<QTreeWidgetItem *> MainWindow::selectedItems() const { auto list = files->operatedItems(); list.erase(std::remove_if(list.begin(), list.end(), [](auto i) { return i->data(0, ParentRole).toBool(); }), list.end()); return list; }
QStringList MainWindow::selectedPaths() const { QStringList list; for (auto i : selectedItems()) list << i->data(0, PathRole).toString(); return list; }
QStringList MainWindow::markedPaths() const { QStringList list; for (auto i : files->markedItems()) if (!i->data(0, ParentRole).toBool()) list << i->data(0, PathRole).toString(); return list; }
MainWindow::BrowseSelection MainWindow::saveBrowseSelection() const {
    BrowseSelection result; result.marked = markedPaths(); result.focusedRow = files->indexOfTopLevelItem(files->currentItem());
    if (auto focused = files->currentItem()) { result.focused = focused->data(0, PathRole).toString(); result.focusedSelected = focused->isSelected(); } return result;
}
void MainWindow::restoreBrowseSelection(const BrowseSelection &selection) {
    QSignalBlocker guard(files); files->clearSelection(); QTreeWidgetItem *focus = nullptr;
    for (int row = 0; row < files->topLevelItemCount(); ++row) {
        auto item = files->topLevelItem(row); const auto path = item->data(0, PathRole).toString(); files->setItemSelected(item, selection.marked.contains(path));
        if (path == selection.focused && (!focus || row == selection.focusedRow)) focus = item;
    }
    if (!focus && selection.focusedRow >= 0 && files->topLevelItemCount() > 0) focus = files->topLevelItem(qMin(selection.focusedRow, files->topLevelItemCount() - 1));
    if (focus) files->setCurrentItem(focus, 0, settings.alternativeSelection && selection.focusedSelected ? QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows : QItemSelectionModel::NoUpdate);
    files->viewport()->update(); icons->viewport()->update(); guard.unblock(); updateState();
}
void MainWindow::setArchiveSelection(ArchiveRequest &request, const QList<QTreeWidgetItem *> &items, bool preservePaths) const {
    if (archivePath.isEmpty() || !archiveFolderIndex.hasDirectories() || archiveFolderIndex.sourceSnapshot().isEmpty()) return;
    request.selectionDirectory = archiveDirectory; request.selectionFlat = flatView;
    if (const auto directory = archiveFolderIndex.directory(archiveDirectory)) { request.selectionBasePath = directory->path; request.selectionAlternateStreams = directory->alternateStreams; }
    request.selectionSnapshot = archiveFolderIndex.sourceSnapshot(); request.selectionType = archiveType;
    request.selectionEntryCount = archiveEntries.size(); request.selectionPreservePaths = preservePaths;
    for (auto item : items) {
        if (!item || item->data(0, ParentRole).toBool()) continue;
        request.selection.append({{item->data(0, Qt::UserRole + 112).toInt(), item->data(0, Qt::UserRole + 113).toInt()},
            item->data(0, Qt::UserRole + 110).toLongLong(), panelItemName(item)});
    }
}
bool MainWindow::canUseNativeArchiveUpdate() const {
    return archiveFolderIndex.hasDirectories() && !archiveFolderIndex.sourceSnapshot().isEmpty() &&
        QStringList{"7z", "zip", "tar", "wim", "gzip", "bzip2", "xz"}.contains(archiveType.toLower());
}
void MainWindow::add(QStringList sources, QString destination) {
    if (operationBusy()) return;
    if (!archivePath.isEmpty() && !canUpdateArchive()) return;
    if (sources.isEmpty()) { if (!archivePath.isEmpty()) return; sources = selectedPaths(); }
    if (sources.isEmpty()) return;
    if (!archivePath.isEmpty()) { copyIntoArchive(sources); return; }
    QString parent = QFileInfo(sources[0]).absolutePath(); bool same = true; for (const auto &p : sources) same &= QFileInfo(p).absolutePath() == parent;
    QString name = sources.size() == 1 ? QFileInfo(sources[0]).completeBaseName() : QFileInfo(parent).fileName();
    const auto preferredFormat = QSettings().value("Compression/LastFormat", "7z").toString();
    const QString preferredExtension = preferredFormat == "gzip" ? "gz" : preferredFormat == "bzip2" ? "bz2" : preferredFormat == "Hash" ? "sha256" : preferredFormat;
    if (destination.isEmpty()) destination = fsPath;
    AddDialog dialog(archivePath.isEmpty() ? destination + '/' + name + '.' + preferredExtension : archivePath, this);
    if (dialog.exec() != QDialog::Accepted) return;
    auto r = dialog.options(); if (QFileInfo(r.archive).isRelative()) r.archive = QDir(fsPath).absoluteFilePath(r.archive); r.workingDirectory = parent;
    for (const auto &p : sources) r.files << (same ? QFileInfo(p).fileName() : QFileInfo(p).absoluteFilePath());
    run(r);
}
void MainWindow::extract() {
    if (operationBusy()) return;
    QString source = archivePath; if (source.isEmpty()) { auto selected = selectedPaths(); if (selected.size() != 1) return; source = selected[0]; }
    const auto destinationBase = parentArchives.isEmpty() ? QFileInfo(source).absolutePath() : QFileInfo(parentArchives.first().path).absolutePath();
    ExtractDialog dialog(destinationBase + '/' + QFileInfo(source).completeBaseName(), !archivePath.isEmpty() && !selectedPaths().isEmpty(), this);
    if (dialog.exec() != QDialog::Accepted) return;
    auto r = dialog.options(); r.archive = source; if (r.password.isEmpty() && source == archivePath) r.password = archivePassword;
    if (dialog.selectedOnly() && !archivePath.isEmpty()) {
        for (QString path : selectedPaths()) { if (path.endsWith('/')) path.chop(1); r.files << path; }
        setArchiveSelection(r, selectedItems());
    }
    run(r);
}
void MainWindow::test() {
    if (operationBusy()) return;
    ArchiveRequest r; r.operation = ArchiveOperation::Test; r.archive = archivePath; r.password = archivePassword;
    if (r.archive.isEmpty()) { auto selected = selectedPaths(); if (selected.size() != 1) return; r.archive = selected[0]; }
    else {
        auto items = selectedItems();
        if (items.isEmpty()) for (int i = 0; i < files->topLevelItemCount(); ++i) if (!files->topLevelItem(i)->data(0, ParentRole).toBool()) items << files->topLevelItem(i);
        setArchiveSelection(r, items);
    }
    run(r);
}
void MainWindow::remove() {
    if (!archivePath.isEmpty() && !canUpdateArchive()) return;
    if (operationBusy()) return;
    auto paths = selectedPaths(); if (paths.isEmpty()) return;
    bool archive = !archivePath.isEmpty();
    if (archive && !canUseNativeArchiveUpdate() && !archiveFolderIndex.uniquePaths(paths)) return;
    if (QMessageBox::question(this, "Confirm File Delete", archive ? "Delete selected items permanently from the archive?" : "Move selected items to the Trash?", QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (archive) { ArchiveRequest r; r.operation = ArchiveOperation::Delete; r.archive = archivePath; r.password = archivePassword; r.format = archiveType.toLower(); for (auto p : paths) { if (p.endsWith('/')) p.chop(1); r.files << p; } if (canUseNativeArchiveUpdate()) setArchiveSelection(r, selectedItems()); run(r); }
    else { fileProgress("Delete", "Trash"); fileOps.trash(paths); updateState(); }
}
void MainWindow::rename() {
    auto item = files->currentItem();
    if (operationBusy() || !item || item->data(0, ParentRole).toBool() || (!archivePath.isEmpty() && !canUpdateArchive())) return;
    if (!archivePath.isEmpty() && !canUseNativeArchiveUpdate() && !archiveFolderIndex.uniquePaths({item->data(0, PathRole).toString()})) return;
    files->setProperty("renameEditing", true); item->setFlags(item->flags() | Qt::ItemIsEditable);
    if (icons->isVisible()) icons->edit(files->indexFromItem(item)); else files->editItem(item, 0);
    if (!files->findChild<QLineEdit *>("renameEditor") && !icons->findChild<QLineEdit *>("renameEditor")) { item->setFlags(item->flags() & ~Qt::ItemIsEditable); files->setProperty("renameEditing", false); }
    updateState(); if (auto other = otherPanel()) other->updateState();
}
void MainWindow::commitRename(QPersistentModelIndex index, QString name) {
    if (!index.isValid() || operationBusy()) return;
    auto item = files->itemFromIndex(index); if (!item || item->data(0, ParentRole).toBool()) return;
    // The original label operation accepts relative paths. Reject unsafe
    // archive paths separately; filesystem destinations are explicit user input.
    if (!officialRenameName(name) || name.endsWith('/') || (!archivePath.isEmpty() &&
        (name.contains('\\') || !SevenZipProcessBackend::safeArchivePath(name) ||
         name.split('/').contains(QStringLiteral(".")) || name.split('/').contains(QString())))) {
        QMessageBox::warning(this, "7-Zip", "Rename\nThe parameter is incorrect."); return;
    }
    QString path = item->data(0, PathRole).toString(); while (path.endsWith('/')) path.chop(1);
    if (!archivePath.isEmpty()) {
        ArchiveRequest r; r.operation = ArchiveOperation::Rename; r.archive = archivePath; r.password = archivePassword; r.format = archiveType.toLower(); const auto slash = path.lastIndexOf('/');
        const auto prefix = canUseNativeArchiveUpdate() ? path.left(path.size() - panelItemName(item).size()) : path.left(slash + 1);
        r.files = {path, prefix + name}; if (canUseNativeArchiveUpdate()) setArchiveSelection(r, {item}); run(r);
    } else {
        fileOperationSelection = saveBrowseSelection(); fileOperationFocus = QDir::cleanPath(QFileInfo(path).absolutePath() + '/' + name);
        fileProgress("Rename", path); fileOps.rename(path, fileOperationFocus); updateState();
    }
}
void MainWindow::newFolder() {
    if (operationBusy() || (!archivePath.isEmpty() && !canCreateArchiveFolder())) return;
    bool ok; QString name = ComboDialog::getText(this, 6300, 6302, UiLanguage::resource(6304), &ok);
    if (!ok) return;
    if (!officialRenameName(name) || (!archivePath.isEmpty() && !SevenZipProcessBackend::safeArchivePath(name))) {
        QMessageBox::warning(this, "7-Zip", "Create Folder\nThe parameter is incorrect."); return;
    }
    if (!archivePath.isEmpty()) {
        ArchiveRequest request; request.operation = ArchiveOperation::CreateFolder; request.archive = archivePath;
        request.format = archiveType.toLower(); request.files = {archivePrefix + name}; request.password = archivePassword; run(request); return;
    }
    const auto first = name.section('/', 0, 0);
    fileOperationSelection = saveBrowseSelection(); fileOperationFocus = first.isEmpty() ? QString() : QDir(fsPath).absoluteFilePath(first);
    fileProgress("Create Folder", FileOperations::creationPath(fsPath, name)); fileOps.create(fsPath, name, true); updateState();
}
bool MainWindow::canCreateArchiveFolder() const {
    const auto dir = archiveFolderIndex.directory(archiveDirectory);
    if ((dir && dir->alternateStreams) || !archiveFolderIndex.uniquePaths({archivePrefix})) return false;
    return canUpdateArchive() && QStringList{"7z", "zip", "tar", "wim"}.contains(archiveType.toLower());
}
void MainWindow::newFile() {
    if (operationBusy() || !archivePath.isEmpty()) return;
    bool ok; QString name = ComboDialog::getText(this, 6301, 6303, UiLanguage::resource(6305), &ok);
    if (!ok) return;
    const auto first = name.section('/', 0, 0);
    fileOperationSelection = saveBrowseSelection(); fileOperationFocus = first.isEmpty() ? QString() : QDir(fsPath).absoluteFilePath(first);
    fileProgress("Create File", FileOperations::creationPath(fsPath, name)); fileOps.create(fsPath, name, false); updateState();
}
void MainWindow::options() {
    if (operationBusy()) return;
    OptionsDialog dialog(this); connect(&dialog, &OptionsDialog::settingsApplied, this, &MainWindow::applySettings); dialog.exec();
}
void MainWindow::applySettings() {
    const auto selection = saveBrowseSelection(); settings = FileManagerSettings::load();
    UiLanguage::set(settings.language);
    files->setSelectionBehavior(settings.fullRow ? QAbstractItemView::SelectRows : QAbstractItemView::SelectItems);
    files->setProperty("alternativeSelection", settings.alternativeSelection); files->setSelectionMode(settings.alternativeSelection ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
    files->setProperty("showGrid", settings.grid); files->viewport()->update();
    applyViewSettings(); if (secondPanel) secondPanel->applySettings();
    if (!fsPath.isEmpty()) {
        auto restore = [this, selection] { restoreBrowseSelection(selection); };
        if (filesystemRead || archivePath.isEmpty()) showFilesystem(requestedDirectory(), restore); else { showArchive(); restore(); }
    }
    UiLanguage::apply(this); updateState();
}
void MainWindow::launchExternal(const QStringList &paths, const QString &command) {
    if (command.isEmpty()) {
        for (const auto &path : paths) if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) QMessageBox::warning(this, "7-Zip", "Cannot open: " + path);
        return;
    }
    auto parts = QProcess::splitCommand(command); if (parts.isEmpty()) return;
    QString program = parts.takeFirst();
    if (program.endsWith(".app", Qt::CaseInsensitive)) {
        const auto applicationArguments = parts; parts = {"-a", program}; parts << paths;
        if (!applicationArguments.isEmpty()) parts << "--args" << applicationArguments;
        program = "/usr/bin/open";
    } else parts.append(paths);
    QProcess process; process.setProgram(program); process.setArguments(parts); process.setWorkingDirectory(fsPath);
    if (!process.startDetached()) QMessageBox::warning(this, "7-Zip", "Cannot start application: " + program + "\n" + process.errorString());
}
void MainWindow::diffFiles() {
    auto paths = selectedPaths(); if (!operationBusy() && archivePath.isEmpty() && paths.size() == 2 && !settings.diff.isEmpty()) launchExternal(paths, settings.diff);
}
void MainWindow::openExternal(bool drag, const QString &command, bool focusedOnly) {
    if (operationBusy()) return;
    auto items = selectedItems();
    if (focusedOnly) {
        auto item = files->currentItem();
        if (!item || item->data(0, ParentRole).toBool() || item->data(0, DirRole).toBool()) return;
        items = {item};
    }
    openExternalItems(items, drag, command);
}
void MainWindow::run(ArchiveRequest r) {
    if (r.archive == archivePath && r.readMode.isEmpty() && r.operation != ArchiveOperation::List && !r.hashTest) r.readMode = archiveReadMode;
    if (r.operation == ArchiveOperation::Add || r.operation == ArchiveOperation::CreateFolder || r.operation == ArchiveOperation::ReplaceFile || r.operation == ArchiveOperation::Comment || r.operation == ArchiveOperation::Delete || r.operation == ArchiveOperation::Rename) r.temporaryDirectory = settings.workingFolder(QFileInfo(r.archive).absolutePath());
    r.memoryLimitGB = settings.memoryLimitGB;
    lastRequest = r;
    if (r.operation != ArchiveOperation::List) {
        if (progressDialog) { progressDialog->finishForRetry(); if (progressDialog) progressDialog->close(); }
        progressDialog = new ProgressDialog(operationName(r.operation), r.archive, this); progressDialog->setAttribute(Qt::WA_DeleteOnClose); connect(progressDialog, &ProgressDialog::cancelRequested, &backend, &SevenZipProcessBackend::cancel); progressDialog->show();
        connect(progressDialog, &ProgressDialog::pauseRequested, this, [this](bool paused) { if (!backend.setPaused(paused) && progressDialog) progressDialog->setPauseAvailable(backend.canPause()); });
        connect(progressDialog, &ProgressDialog::backgroundRequested, this, [this](bool background) { if (!backend.setBackground(background) && progressDialog && background) progressDialog->setBackground(false); });
    }
    statusBar()->showMessage(operationName(r.operation) + ": " + r.archive); backend.start(r); updateState();
}
void MainWindow::onFinished(ArchiveResult result) {
    const bool retryListFocus = listFocusPending;
    if (!result.success) listFocusPending = false;
    rebindArchiveSessions(result);
    if (finishTransfer(result)) return;
    if (finishNestedOpen(result)) { if (backend.busy() && retryListFocus) listFocusPending = true; return; }
    if (finishNestedUpdate(result)) return;
    if (finishExternalUpdate(result)) return;
    if (result.operation == ArchiveOperation::List) {
        if (result.success) { if (pendingArchive != archivePath || pendingReopen) archivePrefix.clear(); if (pendingArchive != archivePath) { parentArchives.clear(); archiveTemporary.reset(); archiveVirtualPath = pendingArchive; } archivePath = pendingArchive; setArchiveListing(result); archivePassword = lastRequest.password; archiveReadMode = lastRequest.readMode;
            showArchive(); if (refreshSelection) { auto selection = std::move(*refreshSelection); refreshSelection.reset(); restoreBrowseSelection(selection); } if (!pendingArchiveTail.isEmpty()) { auto tail = pendingArchiveTail; pendingArchiveTail.clear(); navigateArchive(tail); } }
        else {
            updateState();
            if (result.passwordRequired) {
                bool ok; QString password = QInputDialog::getText(this, "Password", result.message + "\nEnter password:", QLineEdit::Password, {}, &ok);
                if (ok) { auto r = lastRequest; r.password = password; listFocusPending = retryListFocus; run(r); return; }
            } else if (pendingDefaultOpen == pendingArchive && !pendingDefaultOpen.isEmpty() && openProfileNotArchive(result)) launchExternal({pendingDefaultOpen});
            else if (!result.cancelled) QMessageBox::warning(this, "7-Zip", result.message + "\nTarget: " + result.target + "\nExit code: " + QString::number(result.exitCode) + '\n' + result.details);
            pendingArchiveTail.clear(); refreshSelection.reset();
            if (!hasBrowse) showFilesystem(fsPath);
        }
        pendingReopen = false; pendingDefaultOpen.clear();
    } else {
        if (result.success && lastRequest.archive == archivePath && !lastRequest.password.isEmpty()) archivePassword = lastRequest.password;
        if (result.passwordRequired && (result.operation == ArchiveOperation::Test || result.operation == ArchiveOperation::Extract || result.operation == ArchiveOperation::HashArchive || result.operation == ArchiveOperation::CreateFolder || result.operation == ArchiveOperation::Comment || result.operation == ArchiveOperation::Delete || result.operation == ArchiveOperation::Rename)) {
            bool ok; auto password = QInputDialog::getText(this, "Password", "Enter password:", QLineEdit::Password, {}, &ok);
            if (ok) { auto r = lastRequest; r.password = password; run(r); return; }
        }
        if (progressDialog) progressDialog->finish(result);
        if (externalPending) {
            externalPending = false;
            if (result.success) {
                QList<QUrl> urls;
                for (const auto &p : lastRequest.files) urls << QUrl::fromLocalFile(externalTemp->filePath(p));
                auto temporary = std::shared_ptr<QTemporaryDir>(externalTemp.release());
                if (dragPending) {
                    if (progressDialog) progressDialog->close();
                    QStringList paths; for (const auto &url : urls) paths << url.toLocalFile();
                    QTimer::singleShot(0, this, [this, paths, temporary] { dragController->start(paths, temporary); });
                }
                else launchArchiveExternal(temporary, lastRequest.files, externalCommand, lastRequest.password, lastRequest);
            }
            externalTemp.reset();
            dragPending = false;
            externalCommand.clear();
        }
        if (result.success && result.operation == ArchiveOperation::CreateFolder && result.target == archivePath) {
            setArchiveListing(result);
            const auto name = lastRequest.files.value(0); showArchive(); selectArchiveItem(name + '/');
        }
        if (result.success && result.operation == ArchiveOperation::Comment && result.target == archivePath) {
            setArchiveListing(result);
            const auto selected = commentSelection; const auto focus = commentFocus; showArchive();
            for (int n = 0; n < files->topLevelItemCount(); ++n) { auto item = files->topLevelItem(n); const auto path = item->data(0, PathRole).toString();
                if (lastRequest.selectionDirectory >= 0) {
                    const auto index = item->data(0, Qt::UserRole + 110).toLongLong();
                    if (!item->data(0, ParentRole).toBool() && result.metadata.selectedIndices.contains(quint32(index))) files->setCurrentItem(item, 0, settings.alternativeSelection && commentBrowseSelection.focusedSelected ? QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows : QItemSelectionModel::NoUpdate);
                    bool select = false;
                    for (const auto &row : commentNativeSelection) if (!item->data(0, ParentRole).toBool() &&
                        (row.archiveIndex >= 0 ? index == row.archiveIndex : item->data(0, Qt::UserRole + 112).toInt() == row.identity.directory && item->data(0, Qt::UserRole + 113).toInt() == row.identity.item)) { select = true; break; }
                    files->setItemSelected(item, select);
                } else {
                    if (path == focus) files->setCurrentItem(item, 0, settings.alternativeSelection && commentBrowseSelection.focusedSelected ? QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows : QItemSelectionModel::NoUpdate);
                    files->setItemSelected(item, selected.contains(path));
                }
            }
        }
        if (result.success && (result.operation == ArchiveOperation::Rename || result.operation == ArchiveOperation::Delete) && result.target == archivePath) showUpdatedArchive(result);
        // Add may commit before source verification/Trash fails. Re-list the
        // real archive even on that failure rather than retaining stale rows.
        updateState(); if ((result.success && (result.operation == ArchiveOperation::Add || (result.operation == ArchiveOperation::Delete && result.target != archivePath))) ||
            (result.operation == ArchiveOperation::Add && !archivePath.isEmpty() && result.target == archivePath)) refresh();
    }
    if (result.operation != ArchiveOperation::List && !openWithBatch.isEmpty()) {
        if (result.success) { auto next = openWithBatch.takeFirst(); if (next.password.isEmpty()) next.password = lastRequest.password; QTimer::singleShot(0, this, [this, next] { run(next); }); }
        else openWithBatch.clear();
    }
    if (!hasBrowse && !filesystemRead && !backend.busy() && !closingRequested) showFilesystem(fsPath);
    if (archiveTransferSource && !backend.busy()) {
        auto source = archiveTransferSource; archiveTransferSource.clear(); source->refresh();
    }
    if (!backend.busy()) { lastRequest.password.fill(QChar::Null); lastRequest.password.clear(); } updateState();
    if (openCommandActive && !externalPending && !backend.busy()) QTimer::singleShot(0, this, [this] { continueOpenCommands(); });
}
void MainWindow::updateListInteraction(bool busy) {
    const bool enabled = !busy || files->property("renameEditing").toBool();
    const auto focused = QApplication::focusWidget();
    if (!enabled && (files->isEnabled() || icons->isEnabled()) && focused) {
        if (focused == files || files->isAncestorOf(focused)) browseFocus = files;
        else if (focused == icons || icons->isAncestorOf(focused)) browseFocus = icons;
    }
    const bool capture = browseFocus && !browseFocusDisplaced;
    pathCombo->setEnabled(!dataOperationBusy());
    files->setEnabled(enabled); icons->setEnabled(enabled);
    if (capture) browseFocusDisplaced = QApplication::focusWidget();
    if (!busy && browseFocus) {
        const auto view = browseFocus;
        const bool restore = !QApplication::activeModalWidget() &&
            QApplication::focusWidget() == browseFocusDisplaced && view->isVisible() && view->isEnabled();
        browseFocus.clear(); browseFocusDisplaced.clear();
        if (restore) view->setFocus(Qt::OtherFocusReason);
    }
    if (!busy && listFocusPending && pendingArchiveTail.isEmpty() && !QApplication::activeModalWidget()) {
        listFocusPending = false; focusList();
    }
}
void MainWindow::updateState() {
    updateListInteraction(operationBusy());
    if (secondPanel && activePanel == 1) { secondPanel->updateState(); for (auto it = actions.begin(); it != actions.end(); ++it) if (it.key() != "twoPanels" && !it.key().startsWith("toolbar") && secondPanel->actions.contains(it.key())) { it.value()->setEnabled(secondPanel->actions[it.key()]->isEnabled()); if (it.key().startsWith("ver") || it.key() == "diff") it.value()->setVisible(secondPanel->actions[it.key()]->isVisible()); if (it.value()->isCheckable()) it.value()->setChecked(secondPanel->actions[it.key()]->isChecked()); } actions["options"]->setEnabled(!operationBusy()); actions["twoPanels"]->setEnabled(!dataOperationBusy()); statusBar()->showMessage(secondPanel->statusBar()->currentMessage()); return; }
    bool busy = operationBusy(), selected = !selectedPaths().isEmpty(), archive = !archivePath.isEmpty();
    const bool one = selectedPaths().size() == 1; const QFileInfo single(selectedPaths().value(0));
    actions["split"]->setEnabled(!busy && !archive && one && single.isFile() && !single.isSymLink()); actions["combine"]->setEnabled(actions["split"]->isEnabled());
    for (const auto &key : {"open", "inside", "outside", "view", "edit", "rename", "copy", "delete"}) actions[key]->setEnabled(!busy && selected);
    if (archive && !canUpdateArchive()) for (const auto &key : {"rename", "delete"}) actions[key]->setEnabled(false);
    if (archive && !canUseNativeArchiveUpdate() && !archiveFolderIndex.uniquePaths(selectedPaths())) for (const auto &key : {"rename", "delete"}) actions[key]->setEnabled(false);
    auto focused = files->currentItem();
    actions["link"]->setEnabled(!busy && !archive && one);
    actions["alternateStreams"]->setEnabled(!busy && archive && alternateArchiveDirectory() >= 0);
    for (const auto &key : {"inside", "insideOne", "insideParser"}) actions[key]->setEnabled(!busy && focused);
    actions["comment"]->setEnabled(!busy && canComment());
    const bool focusedFile = focused && !focused->data(0, ParentRole).toBool() && !focused->data(0, DirRole).toBool();
    actions["view"]->setEnabled(!busy && (focusedFile || (!archive && !operatedFolders().isEmpty()))); actions["edit"]->setEnabled(!busy && focusedFile);
    actions["move"]->setEnabled(!busy && selected && (!archive || canUpdateArchive())); actions["info"]->setEnabled(!busy); actions["folder"]->setEnabled(!busy && (!archive || canCreateArchiveFolder())); actions["newfile"]->setEnabled(!busy && !archive);
    const bool readable = archive || (selectedPaths().size() == 1 && archiveExtension(selectedPaths().value(0)));
    actions["add"]->setEnabled(!busy && selected && !archive); actions["extract"]->setEnabled(!busy && readable); actions["test"]->setEnabled(!busy && readable);
    for (const auto &key : {"up", "root", "refresh", "history"}) actions[key]->setEnabled(!dataOperationBusy());
    for (const auto &key : {"flat", "unsorted", "sort0", "sort1", "sort2", "sort3", "time0", "time1", "time2", "time7", "time9", "timeUTC"}) actions[key]->setEnabled(!busy);
    actions["options"]->setEnabled(!busy); actions["diff"]->setEnabled(!busy && !archive && selectedPaths().size() == 2 && !settings.diff.isEmpty());
    actions["diff"]->setVisible(!settings.diff.isEmpty());
    FileMenuState versionState; versionState.filesystem = !archive; versionState.count = selectedPaths().size(); versionState.diff = settings.diff;
    versionState.versionStore = QSettings().value("7vc").toString(); versionState.filePath = selectedPaths().value(0);
    for (auto item : selectedItems()) if (item->data(0, DirRole).toBool()) versionState.allFiles = false;
    const auto visibleVersions = officialVersionMenuItems(versionState);
    for (const auto &name : {"verEdit", "verCommit", "verRevert", "verDiff"}) { actions[name]->setVisible(visibleVersions.contains(QString(name) + "Action")); actions[name]->setEnabled(!busy); }
    actions["benchmark"]->setEnabled(!busy);
    actions["temporaryFiles"]->setEnabled(!busy);
    actions["twoPanels"]->setEnabled(!dataOperationBusy());
    for (const auto &method : checksumMethods()) actions["hash" + method.second]->setEnabled(!busy && selected);
    statusBar()->showMessage(busy ? "Working..." : QString::number(selectedPaths().size()) + " / " + QString::number(qMax(0, files->topLevelItemCount() - (settings.showDots ? 1 : 0))) + " selected");
}
void MainWindow::closeEvent(QCloseEvent *e) {
    for (auto panel : {this, otherPanel()}) if (panel && panel->files->property("renameEditing").toBool()) {
        auto editor = panel->findChild<QLineEdit *>("renameEditor");
        if (editor) { QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier); QApplication::sendEvent(editor, &escape); QTimer::singleShot(0, this, [this] { close(); }); e->ignore(); return; }
    }
    if (dataOperationBusy()) { openCommands.clear(); backend.cancel(); fileOps.cancel(); versionControl.cancel(); folderStatistics.cancel(); cancelComment(); if (secondPanel) { secondPanel->openCommands.clear(); secondPanel->backend.cancel(); secondPanel->fileOps.cancel(); secondPanel->versionControl.cancel(); secondPanel->folderStatistics.cancel(); secondPanel->cancelComment(); } e->ignore(); return; }
    cancelFilesystemRead(); if (secondPanel) secondPanel->cancelFilesystemRead();
    // Keep the final queued close ahead of background List/editor decisions,
    // including the short interval after nested write-back clears its busy flag.
    closingRequested = true; watchDebounce.stop(); externalDecisions.stop();
    if (secondPanel) { secondPanel->closingRequested = true; secondPanel->watchDebounce.stop(); secondPanel->externalDecisions.stop(); }
    if (!parentArchives.isEmpty()) { e->ignore(); leaveNestedArchives({}, [this] { QTimer::singleShot(0, this, [this] { close(); }); }); return; }
    if (secondPanel && !secondPanel->parentArchives.isEmpty()) { e->ignore(); secondPanel->leaveNestedArchives({}, [this] { QTimer::singleShot(0, this, [this] { close(); }); }); return; }
    if (!embedded) { QSettings().setValue("View/Geometry", saveGeometry()); if (secondPanel) QSettings().setValue("View/Splitter", panels->saveState()); }
    closeExternalSessions(); if (secondPanel) secondPanel->closeExternalSessions();
    QMainWindow::closeEvent(e);
}
void MainWindow::syncPanelFonts() {
    // Resolve the list font after parenting/polishing, rather than copying
    // the temporary constructor font before the panel layout owns the list.
    auto font = files->font();
    // A widget's inherited QFont can have resolved values without the
    // corresponding explicit-property bits. Make these explicit so the
    // combo/editor cannot resolve them back to their smaller class fonts.
    font.setFamilies(font.families());
    if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF());
    else font.setPixelSize(font.pixelSize());
    pathCombo->setFont(font);
    pathBox->setFont(font);
    pathCombo->view()->setFont(font);
    toolbar->setFont(font);
    for (auto button : toolbar->findChildren<QToolButton *>()) button->setFont(font);
}
void MainWindow::showEvent(QShowEvent *e) {
    closingRequested = false;
    if (secondPanel) secondPanel->closingRequested = false;
    QMainWindow::showEvent(e);
    // Changing an embedded window's flags can deliver Show during its
    // constructor, before its address/list controls have been created.
    if (uiReady) syncPanelFonts();
    if (uiReady && !embedded && initialWindowPlacement) {
        initialWindowPlacement = false;
        centerPortWindow(this);
    }
    if (uiReady && !hasBrowse && !filesystemRead && !panelBusy()) showFilesystem(fsPath);
}
