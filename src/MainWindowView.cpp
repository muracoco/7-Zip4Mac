// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "MainWindow.h"
#include "AddressCombo.h"
#include "ChecksumResultsDialog.h"
#include "UiLanguage.h"
#include "upstream/UiResourceIds.h"
#include "TimeText.h"
#include "PanelSelection.h"
#include "PanelSort.h"
#include <QApplication>
#include <QActionGroup>
#include <QDateTime>
#include <QDrag>
#include <QDropEvent>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
#include <QPersistentModelIndex>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QBitmap>

QItemSelectionModel::SelectionFlags IconFileList::selectionCommand(const QModelIndex &index, const QEvent *event) const {
    if (property("alternativeSelection").toBool() && index.isValid() && event) return QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows;
    return QListView::selectionCommand(index, event);
}
void IconFileList::dragEnterEvent(QDragEnterEvent *e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void IconFileList::dragMoveEvent(QDragMoveEvent *e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void IconFileList::dropEvent(QDropEvent *e) {
    QStringList paths; for (const auto &url : e->mimeData()->urls()) if (url.isLocalFile()) paths << url.toLocalFile();
    if (!paths.isEmpty()) { e->acceptProposedAction(); emit filesDropped(paths); }
}
void IconFileList::startDrag(Qt::DropActions) {
    if (property("archiveView").toBool()) { emit archiveDragRequested(); return; }
    emit filesystemDragRequested();
}
void MainWindow::buildViewMenus(QMenu *view) {
    auto modes = new QActionGroup(this); const QStringList labels{"Large Icons", "Small Icons", "List", "Details"};
    for (int n = 0; n < 4; ++n) { auto a = action("mode" + QString::number(n), labels[n], "Ctrl+" + QString::number(n + 1), [this, n] { setViewMode(n); }); a->setCheckable(true); modes->addAction(a); view->addAction(a); }
    view->addSeparator(); const QStringList sorts{"Name", "Type", "Date", "Size"};
    const QList<quint32> sortProperties{OfficialSort::kpidName,OfficialSort::kpidExtension,OfficialSort::kpidMTime,OfficialSort::kpidSize};
    for (int n = 0; n < sorts.size(); ++n) { auto item=action("sort" + QString::number(n), sorts[n], "Ctrl+F" + QString::number(n + 3), [this, property=sortProperties[n]] { sortByProperty(property); }); item->setCheckable(true); view->addAction(item); }
    auto unsorted=action("unsorted", "Unsorted", "Ctrl+F7", [this] { sortByProperty(OfficialSort::kpidNoProperty); }); unsorted->setCheckable(true); view->addAction(unsorted); view->addSeparator();
    auto flat = action("flat", "Flat View", "", [this] { flatView = !flatView; QSettings().setValue(embedded ? "View/Panel2Flat" : "View/Flat", flatView); if (archivePath.isEmpty()) showFilesystem(fsPath); else showArchive(); }); flat->setCheckable(true); view->addAction(flat);
    auto two = action("twoPanels", "2 Panels", "F9", [this] { setTwoPanels(!secondPanel); }); two->setCheckable(true); view->addAction(two);
    auto times = view->addMenu("Time"); times->setObjectName("timeMenu"); times->menuAction()->setProperty("uiLiteral", true); auto precision = new QActionGroup(this);
    const QStringList timeLabels{"Days", "Hours:Minutes", "Hours:Minutes:Seconds", "100 ns", "1 ns"}; const QList<int> timeValues{0, 1, 2, 7, 9};
    for (int n = 0; n < timeValues.size(); ++n) { int value = timeValues[n]; auto a = action("time" + QString::number(value), timeLabels[n], "", [this, value] { QSettings().setValue("View/TimePrecision", value); applySettings(); }); a->setCheckable(true); precision->addAction(a); times->addAction(a); }
    auto utc = action("timeUTC", "UTC", "", [this] { QSettings().setValue("View/UTC", !QSettings().value("View/UTC", false).toBool()); applySettings(); }); utc->setCheckable(true); times->addAction(utc);
    connect(view, &QMenu::aboutToShow, this, [this, times] {
        const auto now = QDateTime::currentDateTime(); const bool utc = QSettings().value("View/UTC", false).toBool();
        times->setTitle(officialTimeText(now, {}, 0, utc));
        const auto samples = officialTimeMenuSamples(now, utc); const QList<int> values{0, 1, 2, 7, 9};
        for (int n = 0; n < values.size(); ++n) { auto item = actions["time" + QString::number(values[n])]; item->setText(samples[n]); item->setProperty("uiLiteral", true); item->setChecked(QSettings().value("View/TimePrecision", 1).toInt() == values[n]); }
        actions["timeUTC"]->setChecked(utc);
    });
    auto bars = view->addMenu("Toolbars"); UiLanguage::bind(bars->menuAction(), OfficialUi::IDM_VIEW_TOOLBARS);
    for (auto pair : {qMakePair(QString("Archive"), QString("Archive Toolbar")), qMakePair(QString("Standard"), QString("Standard Toolbar")), qMakePair(QString("Large"), QString("Large Buttons")), qMakePair(QString("Text"), QString("Show Buttons Text"))}) {
        auto a = action("toolbar" + pair.first, pair.second, "", [this, key = pair.first] { QSettings s; s.setValue("View/Toolbar" + key, !s.value("View/Toolbar" + key, key != "Large").toBool()); updateToolbarSettings(); }); a->setCheckable(true); bars->addAction(a);
    }
    view->addSeparator(); view->addAction(action("root", "Open Root Folder", "\\", [this] { openPath("/"); })); view->addAction(action("up", "Up One Level", "Backspace", [this] { up(); }));
    view->addAction(action("history", "Folders History...", "Alt+F12", [this] {
        if (operationBusy()) return;
        ChecksumPresentation view; view.title = UiLanguage::resource(6601); view.columns = 1; view.selectFirst = true; view.deleteAllowed = true;
        for (const auto &path : QSettings().value("View/History").toStringList()) view.rows.append({path, {}});
        ChecksumResultsDialog dialog(view, this, true);
        if (dialog.exec() != QDialog::Accepted) return;
        if (dialog.stringsWereChanged()) QSettings().setValue("View/History", dialog.strings());
        const auto path = dialog.focusedString(); if (!path.isEmpty()) openPath(path);
    }));
    view->addSeparator(); view->addAction(action("refresh", "Refresh", "Ctrl+R", [this] { refresh(); }));
    auto automatic = action("autoRefresh", "Auto Refresh", "", [this] { QSettings().setValue("View/AutoRefresh", !autoRefresh); applyViewSettings(); if (secondPanel) secondPanel->applyViewSettings(); }); automatic->setCheckable(true); view->addAction(automatic);
}
void MainWindow::setupViews(QVBoxLayout *layout, QHBoxLayout *pathRow) {
    panels = new QSplitter(Qt::Horizontal, this); panels->setObjectName("filePanels"); auto left = new QWidget(panels); auto pane = new QVBoxLayout(left); pane->setContentsMargins(0, 0, 0, 0); pane->setSpacing(3); pane->addLayout(pathRow);
    viewStack = new QStackedWidget(left); viewStack->addWidget(files); icons = new IconFileList(left); icons->setObjectName("iconFileList"); icons->setModel(files->model()); icons->setSelectionModel(files->selectionModel()); icons->setModelColumn(0); icons->setDragEnabled(true); icons->setAcceptDrops(true); icons->setDragDropMode(QAbstractItemView::DragDrop); viewStack->addWidget(icons); pane->addWidget(viewStack); panels->addWidget(left); layout->addWidget(panels);
    icons->setItemDelegate(new PanelSelectionDelegate(icons)); new PanelSelection(files, icons);
    connect(icons, &QListView::doubleClicked, this, [this](QModelIndex) { if (!settings.singleClick) openSelectedItems(true); });
    connect(icons, &QListView::clicked, this, [this](QModelIndex index) { if (settings.singleClick && QApplication::keyboardModifiers() == Qt::NoModifier) { QPersistentModelIndex persistent(index); QTimer::singleShot(0, this, [this, persistent] { if (persistent.isValid()) openSelectedItems(true); }); } });
    connect(icons, &IconFileList::filesDropped, files, &FileList::filesDropped); connect(icons, &IconFileList::archiveDragRequested, files, &FileList::archiveDragRequested);
    for (const auto &key : {"open", "outside", "up"}) icons->addAction(actions[key]);
    icons->setContextMenuPolicy(Qt::CustomContextMenu); connect(icons, &QWidget::customContextMenuRequested, this, [this](QPoint point) { const auto index = icons->indexAt(point); if (index.isValid() && !icons->selectionModel()->isSelected(index)) icons->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows); showFileContextMenu(icons->viewport()->mapToGlobal(point)); });
    auto save = [this] { if (!loadingView) {
        const auto property=files->property("virtualSortProperty").isValid()?files->property("virtualSortProperty").toUInt():files->headerItem()->data(files->sortColumn(),ColumnPropertyRole).toUInt();
        files->setProperty("sortProperty",property);
        QSettings s; s.setValue(viewKey()+"/Header",files->header()->saveState()); s.setValue(viewKey()+"/Sorted",property!=OfficialSort::kpidNoProperty);
        s.setValue(viewKey()+"/SortProperty",property); s.setValue(viewKey()+"/SortRaw",files->property("sortRaw")); s.setValue(viewKey()+"/Ascending",files->header()->sortIndicatorOrder()==Qt::AscendingOrder);
        saveViewColumns();
    } };
    connect(files->header(), &QHeaderView::sectionMoved, this, save); connect(files->header(), &QHeaderView::sectionResized, this, [this, save](int logical, int oldSize, int size) { if (!loadingView) { const int width=size>0?size:oldSize; if(width>0) files->headerItem()->setData(logical,ColumnWidthRole,width); QSettings().setValue(viewKey() + "/Width" + QString::number(logical), width); save(); } }); connect(files->header(), &QHeaderView::sortIndicatorChanged, this, save);
    files->header()->setContextMenuPolicy(Qt::CustomContextMenu); connect(files->header(), &QWidget::customContextMenuRequested, this, [this, save](QPoint point) { QMenu menu(this); for (int n = 0; n < files->columnCount(); ++n) { auto a = menu.addAction(files->headerItem()->text(n)); a->setCheckable(true); a->setChecked(!files->isColumnHidden(n)); a->setEnabled(n != 0); connect(a, &QAction::toggled, this, [this, n, save](bool shown) { files->setColumnHidden(n, !shown); save(); }); } menu.exec(files->header()->mapToGlobal(point)); });
    watchDebounce.setParent(this); watchDebounce.setObjectName("archiveRefreshDebounce");
    watchDebounce.setSingleShot(true); watchDebounce.setInterval(250); connect(&watchDebounce, &QTimer::timeout, this, [this] {
        if (!autoRefresh || closingRequested) return;
        if (operationBusy() || QApplication::activeModalWidget() || QApplication::activePopupWidget()) { watchDebounce.start(); return; }
        refresh();
    });
    connect(&watcher, &QFileSystemWatcher::directoryChanged, &watchDebounce, qOverload<>(&QTimer::start)); connect(&watcher, &QFileSystemWatcher::fileChanged, &watchDebounce, qOverload<>(&QTimer::start));
    if (!embedded) connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) { if (!now) return; if (secondPanel && (now == secondPanel || secondPanel->isAncestorOf(now))) activePanel = 1; else if (now == pathBox || now == files || now == icons || files->isAncestorOf(now) || icons->isAncestorOf(now)) activePanel = 0; updateState(); });
}
QString MainWindow::viewKey() const { return QString(embedded ? "View/Panel2/" : "View/") + (archivePath.isEmpty() ? "FS" : archiveFolderIndex.native() ? "Archive/" + archiveType + (flatView ? "/NativeFlat" : "/Native") : "Archive"); }
void MainWindow::saveViewColumns() {
    QSettings s; QStringList order;
    for (int visual=0;visual<files->columnCount();++visual) {
        const int column=files->header()->logicalIndex(visual); const auto id=files->headerItem()->data(column,ColumnPropertyRole); if(!id.isValid()) continue;
        const auto key=QString::number(id.toUInt())+':'+QString::number(files->headerItem()->data(column,ColumnRawRole).toBool()); order<<key;
        const int width=files->isColumnHidden(column)?files->headerItem()->data(column,ColumnWidthRole).toInt():files->columnWidth(column);
        if(width>0) s.setValue(viewKey()+"/Column/"+key+"/Width",width);
        s.setValue(viewKey()+"/Column/"+key+"/Visible",!files->isColumnHidden(column));
    }
    // Preserve positions and settings of properties absent from this view,
    // such as Prefix while switching back from Flat to a normal directory.
    const auto previous=s.value(viewKey()+"/ColumnOrder").toStringList();
    for (int n=0;n<previous.size();++n) if (!order.contains(previous[n])) {
        int position=order.size();
        for (int following=n+1;following<previous.size();++following) if (order.contains(previous[following])) { position=order.indexOf(previous[following]); break; }
        order.insert(position,previous[n]);
    }
    s.setValue(viewKey()+"/ColumnOrder",order);
}
void MainWindow::finishBrowse() {
    QSettings s;
    const bool migrate=archivePath.isEmpty() && s.value(viewKey()+"/ColumnSchema",0).toInt()!=2;
    if (migrate && s.contains(viewKey()+"/Header")) {
        QTreeWidget old; old.setColumnCount(9); old.header()->restoreState(s.value(viewKey()+"/Header").toByteArray());
        const QList<quint32> ids{OfficialSort::kpidName,OfficialSort::kpidSize,OfficialSort::kpidMTime,OfficialSort::kpidCTime,OfficialSort::kpidComment,OfficialSort::kpidNumSubDirs,OfficialSort::kpidNumSubFiles,OfficialSort::kpidPrefix,OfficialSort::kpidExtension};
        int next=0;
        for (int visual=0;visual<ids.size();++visual) {
            const int index=old.header()->logicalIndex(visual), column=propertyColumn(files,ids[index]); if(column<0) continue;
            const bool hidden=old.isColumnHidden(index); old.setColumnHidden(index,false);
            const int width=s.value(viewKey()+"/Width"+QString::number(index),old.columnWidth(index)).toInt();
            files->setColumnWidth(column,width); files->headerItem()->setData(column,ColumnWidthRole,width); files->setColumnHidden(column,hidden);
            files->header()->moveSection(files->header()->visualIndex(column),next++);
        }
        s.setValue(viewKey()+"/SortProperty",ids.value(old.header()->sortIndicatorSection(),OfficialSort::kpidName));
        s.setValue(viewKey()+"/Ascending",old.header()->sortIndicatorOrder()==Qt::AscendingOrder);
    } else if (!migrate && s.contains(viewKey()+"/Header") && s.value(viewKey()+"/ColumnOrder").toStringList().isEmpty()) files->header()->restoreState(s.value(viewKey()+"/Header").toByteArray());
    if (!migrate && s.value(viewKey()+"/ColumnOrder").toStringList().isEmpty()) for (int n = 0; n < files->columnCount(); ++n) if (s.contains(viewKey() + "/Width" + QString::number(n))) files->setColumnWidth(n, s.value(viewKey() + "/Width" + QString::number(n)).toInt());
    if (migrate) { s.setValue(viewKey()+"/ColumnSchema",2); s.setValue(viewKey()+"/Header",files->header()->saveState()); }
    const auto columnOrder=s.value(viewKey()+"/ColumnOrder").toStringList(); int nextColumn=0;
    for(const auto &key:columnOrder) {
        const auto parts=key.split(':'); if(parts.size()!=2) continue;
        const int column=propertyColumn(files,parts[0].toUInt(),parts[1].toInt()!=0); if(column<0) continue;
        files->header()->moveSection(files->header()->visualIndex(column),nextColumn++);
    }
    for(int column=0;column<files->columnCount();++column) {
        const auto key=QString::number(files->headerItem()->data(column,ColumnPropertyRole).toUInt())+':'+QString::number(files->headerItem()->data(column,ColumnRawRole).toBool());
        if(s.contains(viewKey()+"/Column/"+key+"/Width")) { const int width=s.value(viewKey()+"/Column/"+key+"/Width").toInt(); files->setColumnWidth(column,width); files->headerItem()->setData(column,ColumnWidthRole,width); }
        if(s.contains(viewKey()+"/Column/"+key+"/Visible")) files->setColumnHidden(column,!s.value(viewKey()+"/Column/"+key+"/Visible").toBool());
    }
    const quint32 property=s.value(viewKey()+"/Sorted",true).toBool()?s.value(viewKey()+"/SortProperty",files->headerItem()->data(files->header()->sortIndicatorSection(),ColumnPropertyRole).isValid()?files->headerItem()->data(files->header()->sortIndicatorSection(),ColumnPropertyRole):QVariant(OfficialSort::kpidName)).toUInt():quint32(OfficialSort::kpidNoProperty);
    const bool raw=s.value(viewKey()+"/SortRaw",false).toBool(); const int column=propertyColumn(files,property,raw);
    files->setProperty("sortProperty",property); files->setProperty("sortRaw",raw); files->setProperty("virtualSortProperty",column<0?QVariant(property):QVariant());
    files->header()->setSortIndicator(column<0?0:column,s.value(viewKey()+"/Ascending",true).toBool()?Qt::AscendingOrder:Qt::DescendingOrder);
    files->header()->setSortIndicatorShown(column>=0); files->setSortingEnabled(true);
    saveViewColumns();
    // Enable one sort for the restored property. Large comparisons run on
    // copied metadata, then the GUI installs the completed order once.
    for (int n = 0; n < files->columnCount(); ++n) files->headerItem()->setData(n, Qt::UserRole + 100, files->headerItem()->text(n));
    icons->setProperty("archiveView", !archivePath.isEmpty()); loadingView = false; hasBrowse = true; actions["flat"]->setChecked(flatView);
    auto old = watcher.files() + watcher.directories(); if (!old.isEmpty()) watcher.removePaths(old); watcher.addPath(archivePath.isEmpty() ? fsPath : archivePath);
    if (archivePath.isEmpty()) { s.setValue(embedded ? "View/Panel2Path" : "View/LastPath", fsPath); auto history = s.value("View/History").toStringList(); history.removeAll(fsPath); history.prepend(fsPath); while (history.size() > 32) history.removeLast(); s.setValue("View/History", history); }
    UiLanguage::apply(this); updateState();
    const QList<quint32> sortProperties{OfficialSort::kpidName,OfficialSort::kpidExtension,OfficialSort::kpidMTime,OfficialSort::kpidSize};
    for(int n=0;n<sortProperties.size();++n) actions["sort"+QString::number(n)]->setChecked(property==sortProperties[n]&&!raw);
    actions["unsorted"]->setChecked(property==OfficialSort::kpidNoProperty);
}
void MainWindow::sortByProperty(quint32 property,bool raw) {
    if(operationBusy()) return;
    const PanelSortState current{files->property("sortProperty").isValid()?files->property("sortProperty").toUInt():quint32(OfficialSort::kpidName),files->header()->sortIndicatorOrder()==Qt::AscendingOrder,files->property("sortRaw").toBool()};
    const auto next=nextOfficialSort(current,property,raw); const int column=propertyColumn(files,property,raw);
    files->setProperty("sortProperty",property); files->setProperty("sortRaw",raw); files->setProperty("virtualSortProperty",column<0?QVariant(property):QVariant());
    { QSignalBlocker header(files->header()); files->sortItems(column<0?0:column,next.ascending?Qt::AscendingOrder:Qt::DescendingOrder); files->setSortingEnabled(true); files->header()->setSortIndicatorShown(column>=0); }
    QSettings s; s.setValue(viewKey()+"/SortProperty",property); s.setValue(viewKey()+"/SortRaw",raw); s.setValue(viewKey()+"/Ascending",next.ascending); s.setValue(viewKey()+"/Sorted",property!=OfficialSort::kpidNoProperty); s.setValue(viewKey()+"/Header",files->header()->saveState());
    const QList<quint32> ids{OfficialSort::kpidName,OfficialSort::kpidExtension,OfficialSort::kpidMTime,OfficialSort::kpidSize};
    for(int n=0;n<ids.size();++n) actions["sort"+QString::number(n)]->setChecked(property==ids[n]&&!raw); actions["unsorted"]->setChecked(property==OfficialSort::kpidNoProperty);
    if (files->currentItem()) { files->scrollToItem(files->currentItem()); icons->scrollTo(files->indexFromItem(files->currentItem())); }
}
void MainWindow::setViewMode(int mode) {
    viewMode = qBound(0, mode, 3); QSettings().setValue(embedded ? "View/Panel2Mode" : "View/Mode", viewMode);
    viewStack->setCurrentWidget(viewMode == 3 ? static_cast<QWidget *>(files) : icons);
    icons->setViewMode(viewMode == 2 ? QListView::ListMode : QListView::IconMode); icons->setFlow(viewMode == 2 ? QListView::TopToBottom : QListView::LeftToRight); icons->setWrapping(true); icons->setResizeMode(QListView::Adjust); icons->setMovement(QListView::Static);
    // QListView::setMovement(Static) disables drag/drop on its viewport. Keep
    // fixed icon positions while routing file transfers through our controller.
    icons->setDragEnabled(true); icons->setAcceptDrops(true); icons->viewport()->setAcceptDrops(true);
    icons->setIconSize(viewMode == 0 ? QSize(48, 48) : QSize(16, 16)); icons->setGridSize(viewMode == 0 ? QSize(120, 84) : QSize(210, 24));
    for (int n = 0; n < 4; ++n) actions["mode" + QString::number(n)]->setChecked(viewMode == n);
}
void MainWindow::applyViewSettings() {
    QSettings s; setViewMode(s.value(embedded ? "View/Panel2Mode" : "View/Mode", 3).toInt()); flatView = s.value(embedded ? "View/Panel2Flat" : "View/Flat", false).toBool(); autoRefresh = s.value("View/AutoRefresh", true).toBool();
    icons->setProperty("alternativeSelection", settings.alternativeSelection); icons->setSelectionMode(settings.alternativeSelection ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection); actions["autoRefresh"]->setChecked(autoRefresh); actions["flat"]->setChecked(flatView);
    for (int value : {0, 1, 2, 7, 9}) actions["time" + QString::number(value)]->setChecked(s.value("View/TimePrecision", 1).toInt() == value); actions["timeUTC"]->setChecked(s.value("View/UTC", false).toBool()); updateToolbarSettings();
}
void MainWindow::updateToolbarSettings() {
    QSettings s; bool archive = s.value("View/ToolbarArchive", true).toBool(), standard = s.value("View/ToolbarStandard", true).toBool(), large = s.value("View/ToolbarLarge", false).toBool(), text = s.value("View/ToolbarText", true).toBool();
    const QStringList keys{"add", "extract", "test", "copy", "move", "delete", "info"};
    for (int n = 0; n < keys.size(); ++n) { auto button = toolbar->findChild<QToolButton *>(keys[n] + "Button"); if (!button) continue; auto widgetAction = toolbar->actions().value(n); if (widgetAction) widgetAction->setVisible(n < 3 ? archive : standard); QString name = keys[n]; name[0] = name[0].toUpper(); QPixmap bitmap(":/icons/" + name + (large ? "" : "2") + ".bmp"); bitmap.setMask(bitmap.createMaskFromColor(QColor(255, 0, 255))); button->defaultAction()->setIcon(QIcon(bitmap)); button->setIconSize(large ? QSize(48, 36) : QSize(24, 24)); button->setToolButtonStyle(text ? Qt::ToolButtonTextUnderIcon : Qt::ToolButtonIconOnly); button->setMinimumSize(text ? QSize(56, large ? 62 : 50) : QSize(large ? 54 : 30, large ? 42 : 30)); }
    toolbar->setVisible(!embedded && (archive || standard)); for (const auto &key : {"Archive", "Standard", "Large", "Text"}) actions[QString("toolbar") + key]->setChecked(s.value(QString("View/Toolbar") + key, QString(key) != "Large").toBool());
}
QString MainWindow::timestamp(QDateTime value, QString fraction) const {
    QSettings s; return timestamp(value, fraction, s.value("View/TimePrecision", 1).toInt(), s.value("View/UTC", false).toBool());
}
QString MainWindow::timestamp(QDateTime value, QString fraction, int precision, bool utc) const {
    return officialTimeText(value, fraction, precision, utc);
}
MainWindow *MainWindow::otherPanel() const { if (!embedded) return secondPanel; auto widget = parentWidget(); while (widget) { if (auto root = qobject_cast<MainWindow *>(widget)) return root; widget = widget->parentWidget(); } return nullptr; }
void MainWindow::focusList() {
    // CApp retains LastFocusedPanel for menu dispatch. Cocoa can close a modal
    // progress window before Qt has an active window again; setFocus alone
    // then emits no focusChanged signal. Retain the requested panel explicitly.
    if (auto root = embedded ? otherPanel() : this) root->activePanel = embedded ? 1 : 0;
    (icons->isVisible() ? static_cast<QWidget *>(icons) : files)->setFocus(Qt::OtherFocusReason);
}
void MainWindow::focusAddress(int panelIndex) {
    auto root = embedded ? otherPanel() : this;
    if (!root) return;
    auto panel = root->secondPanel && panelIndex == 1 ? root->secondPanel : root;
    auto other = panel->otherPanel(); if (other) other->pathCombo->hidePopup();
    panel->pathBox->setFocus(); panel->pathCombo->showPopup(); panel->pathBox->selectAll();
}
void MainWindow::submitAddress(QString path) {
    if (dataOperationBusy() || path.isEmpty()) return;
    pathCombo->hidePopup(); listFocusPending = true; openPath(path);
    // Synchronous archive-folder navigation has already refreshed the list.
    updateState();
}
void MainWindow::setOtherPanelFolder(bool same) {
    auto destination = otherPanel();
    if (!destination || operationBusy()) return;
    auto focused = files->currentItem();
    if (!same && (!focused || !focused->data(0, Qt::UserRole + 1).toBool())) return;
    QString filesystem;
    ArchiveFrame location{archivePath, archiveVirtualPath, archivePrefix, archivePassword, archiveType, {},
        archiveEntries, archiveProperties, archiveTemporary, -1, {}, archiveReadMode, archiveLayers,
        archiveFolderIndex, archiveDirectory, {}};
    auto ancestors = parentArchives;
    if (archivePath.isEmpty()) {
        filesystem = fsPath;
        if (!same) {
            if (focused->data(0, Qt::UserRole + 2).toBool()) { QDir parent(fsPath); parent.cdUp(); filesystem = parent.absolutePath(); }
            else filesystem = focused->data(0, Qt::UserRole).toString();
        }
    } else if (!same) {
        if (focused->data(0, Qt::UserRole + 2).toBool()) {
            const auto directory = archiveFolderIndex.directory(archiveDirectory);
            if (directory && directory->parent >= 0) {
                location.directory = directory->parent; location.prefix = archiveFolderIndex.directory(directory->parent)->path;
            } else if (!ancestors.isEmpty()) location = ancestors.takeLast();
            else filesystem = QFileInfo(archivePath).absolutePath();
        } else {
            // BindToFolder uses the native graph, preserving duplicate folder
            // names and alternate-stream directories instead of reparsing a path.
            location.directory = focused->data(0, Qt::UserRole + 111).toInt();
            const auto directory = archiveFolderIndex.directory(location.directory);
            if (!directory) return;
            location.prefix = directory->path;
        }
    }
    const auto base = fsPath;
    QPointer<MainWindow> target = destination;
    auto bind = [target, filesystem, base, location, ancestors] {
        if (!target || target->dataOperationBusy() || target->closingRequested) return;
        if (!filesystem.isEmpty()) { target->showFilesystem(filesystem); return; }
        target->cancelFilesystemRead(); target->fsPath = base;
        target->archivePath = location.path; target->archiveVirtualPath = location.virtualPath;
        target->archivePrefix = location.prefix; target->archivePassword.fill(QChar::Null); target->archivePassword = location.password;
        target->archiveType = location.type; target->archiveEntries = location.entries; target->archiveProperties = location.properties;
        target->archiveTemporary = location.temporary; target->archiveReadMode = location.readMode;
        target->archiveLayers = location.layers; target->archiveFolderIndex = location.folderIndex; target->archiveDirectory = location.directory;
        target->parentArchives = ancestors; target->pendingArchiveTail.clear(); target->pendingDefaultOpen.clear();
        target->refreshSelection.reset(); target->showArchive(); target->updateState();
    };
    // Closing a changed nested destination retains the established write-back
    // confirmation. Shared temporary ownership keeps the source's view alive.
    if (destination->parentArchives.isEmpty()) bind(); else destination->leaveNestedArchives({}, std::move(bind));
}
void MainWindow::setTwoPanels(bool enabled) {
    if (embedded) return; if (dataOperationBusy()) return; QSettings s; s.setValue("View/TwoPanels", enabled); actions["twoPanels"]->setChecked(enabled);
    if (!enabled && secondPanel && !secondPanel->parentArchives.isEmpty()) { secondPanel->leaveNestedArchives({}, [this] { setTwoPanels(false); }); return; }
    if (enabled && !secondPanel) {
        secondPanel = new MainWindow(backendExecutable, panels, true); secondPanel->setObjectName("secondPanel"); panels->addWidget(secondPanel);
        for (auto a : secondPanel->actions) a->setShortcutContext(Qt::WidgetShortcut);
        for (auto a : actions) if (a->shortcutContext() == Qt::WindowShortcut) secondPanel->addAction(a);
        for (const auto &key : {"open", "outside", "up"}) { secondPanel->files->removeAction(secondPanel->actions[key]); secondPanel->icons->removeAction(secondPanel->actions[key]); secondPanel->files->addAction(actions[key]); secondPanel->icons->addAction(actions[key]); }
        connect(secondPanel->files, &QTreeWidget::itemSelectionChanged, this, &MainWindow::updateState); connect(&secondPanel->backend, &ArchiveBackend::finished, this, [this] { updateState(); });
        if (!panels->restoreState(s.value("View/Splitter").toByteArray())) panels->setSizes({width() / 2, width() / 2}); secondPanel->show();
    } else if (!enabled && secondPanel) { s.setValue("View/Splitter", panels->saveState()); auto child = secondPanel; secondPanel = nullptr; activePanel = 0; child->cancelFilesystemRead(); child->hide(); child->deleteLater(); }
    updateState();
}
