// SPDX-License-Identifier: LGPL-3.0-or-later
#include "TemporaryFilesDialog.h"
#include "Help.h"
#include "OptionsDialog.h"
#include "UiLanguage.h"
#include "ResourceDialogs.h"
#include "upstream/UiResourceIds.h"
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QShortcut>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QTreeWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int PathRole = Qt::UserRole, FolderRole = Qt::UserRole + 1, ParentRole = Qt::UserRole + 2,
    SortRole = Qt::UserRole + 3, LinkRole = Qt::UserRole + 4, OrderRole = Qt::UserRole + 5;
class TempItem final : public QTreeWidgetItem {
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem &other) const override {
        const bool asc = treeWidget()->header()->sortIndicatorOrder() == Qt::AscendingOrder;
        for (const int role : {ParentRole, FolderRole}) if (data(0, role).toBool() != other.data(0, role).toBool())
            return asc ? data(0, role).toBool() : other.data(0, role).toBool();
        const int column = treeWidget()->sortColumn();
        if (column >= 1 && column <= 4) {
            if (column == 1) { const auto a = data(column, SortRole).toLongLong(), b = other.data(column, SortRole).toLongLong(); if (a != b) return a < b; }
            else { const auto a = data(column, SortRole).toULongLong(), b = other.data(column, SortRole).toULongLong(); if (a != b) return a < b; }
        } else { const int compared = QString::localeAwareCompare(text(column), other.text(column)); if (compared) return compared < 0; }
        return data(0, OrderRole).toInt() < other.data(0, OrderRole).toInt();
    }
};
}
TemporaryFilesDialog::TemporaryFilesDialog(QString helper, QWidget *parent, QString root, std::function<QStringList()> protectedPaths)
    : QDialog(parent), executable(std::move(helper)), protection(std::move(protectedPaths)) {
    setObjectName("temporaryFilesDialog"); setWindowTitle("Delete Temporary Files"); resize(780, 600); setModal(true);
    const auto preferences = FileManagerSettings::load(); showDots = preferences.showDots;
    const QString requested = root.isEmpty() ? QDir::tempPath() : root;
    boundary = QFileInfo(requested).canonicalFilePath(); if (boundary.isEmpty()) boundary = QFileInfo(requested).absoluteFilePath(); rootSnapshot = FileInstall::capture(boundary);
    auto layout = new QVBoxLayout(this); auto tools = new QHBoxLayout;
    removeButton = new QPushButton("Delete", this); removeButton->setObjectName("deleteTemporary");
    refreshButton = new QPushButton("Refresh", this); refreshButton->setObjectName("refreshTemporary");
    for (auto button : {removeButton, refreshButton}) { button->setFixedWidth(112); tools->addWidget(button); }
    tools->addStretch(); layout->addLayout(tools);
    auto pathRow = new QHBoxLayout; upButton = new QPushButton("<--", this); upButton->setObjectName("temporaryParent"); upButton->setFixedWidth(42);
    pathBox = new QLineEdit(this); pathBox->setObjectName("temporaryPath"); pathBox->setReadOnly(true); pathRow->addWidget(upButton); pathRow->addWidget(pathBox); layout->addLayout(pathRow);
    files = new QTreeWidget(this); files->setObjectName("temporaryList"); files->setRootIsDecorated(false); files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    files->setSelectionBehavior(preferences.fullRow ? QAbstractItemView::SelectRows : QAbstractItemView::SelectItems);
    if (preferences.grid) files->setStyleSheet("QTreeView::item { border-right: 1px solid #d9d9d9; border-bottom: 1px solid #d9d9d9; }");
    files->setProperty("realIcons", preferences.realIcons);
    files->setHeaderLabels({"Name", "Modified", "Size", "Files", "Folders", "Name-2"}); files->setSortingEnabled(true); files->sortItems(sortedColumn, Qt::AscendingOrder);
    files->header()->setStretchLastSection(true); for (int column = 0; column < 6; ++column) files->setColumnWidth(column, column == 0 ? 160 : column == 1 ? 145 : 85);
    files->installEventFilter(this); files->setContextMenuPolicy(Qt::CustomContextMenu); layout->addWidget(files, 1);
    auto filter = new QComboBox(this); filter->setObjectName("temporaryFilter"); filter->addItem("7-Zip temp files (7z*)"); filter->setEnabled(false); layout->addWidget(filter);
    status = new QLabel(this); status->setObjectName("temporaryStatus"); status->setWordWrap(true); status->setTextFormat(Qt::PlainText); status->setProperty("uiLiteral", true); status->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(status);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close | QDialogButtonBox::Help, this); layout->addWidget(buttons);
    for (auto button : findChildren<QPushButton *>()) { button->setAutoDefault(false); button->setDefault(false); }
    connect(buttons, &QDialogButtonBox::rejected, this, &TemporaryFilesDialog::reject);
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this] { Help::show(this, "fm/temp.htm"); });
    auto help = new QShortcut(QKeySequence(Qt::Key_F1), this); connect(help, &QShortcut::activated, this, [this] { Help::show(this, "fm/temp.htm"); });
    connect(removeButton, &QPushButton::clicked, this, &TemporaryFilesDialog::removeSelected);
    connect(refreshButton, &QPushButton::clicked, this, &TemporaryFilesDialog::refresh);
    connect(upButton, &QPushButton::clicked, this, &TemporaryFilesDialog::up);
    connect(files, &QTreeWidget::itemDoubleClicked, this, [this, preferences] { if (!preferences.singleClick) enter(QApplication::keyboardModifiers()); });
    if (preferences.singleClick) connect(files, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        if (QApplication::keyboardModifiers() != Qt::NoModifier) return;
        QPersistentModelIndex index(files->indexFromItem(item)); QTimer::singleShot(0, this, [this, index] { if (index.isValid()) enter(); });
    });
    connect(files, &QTreeWidget::itemSelectionChanged, this, &TemporaryFilesDialog::updateButtons);
    connect(files, &QTreeWidget::customContextMenuRequested, this, &TemporaryFilesDialog::contextMenu);
    connect(files->header(), &QHeaderView::sectionClicked, this, [this](int column) {
        ascending = column == sortedColumn ? !ascending : column == 0 || column == 5; sortedColumn = column;
        files->sortItems(column, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
    });
    connect(&operations, &FileOperations::trashed, this, &TemporaryFilesDialog::trashed);
    connect(&operations, &FileOperations::finished, this, [this](QString error) {
        if (closing) { QDialog::reject(); return; }
        operationError = error; if (!error.isEmpty()) report(error);
        load(current);
    });
    UiLanguage::bind(removeButton, OfficialUi::IDS_BUTTON_DELETE, true); UiLanguage::bind(refreshButton, OfficialUi::IDM_VIEW_REFRESH, true);
    upButton->setProperty("uiLiteral", true); pathBox->setProperty("uiLiteral", true); filter->setProperty("uiLiteral", true);
    auto geometry = new ResourceDialogLayout(this, 93);
    geometry->bind("IDS_BUTTON_DELETE", removeButton); geometry->bind("IDM_VIEW_REFRESH", refreshButton);
    geometry->bind("IDB_BROWSE2_PARENT", upButton); geometry->bind("IDT_BROWSE2_FOLDER", pathBox);
    geometry->bind("IDL_BROWSE2", files); geometry->bind("IDC_BROWSE2_FILTER", filter); geometry->bindButtons(buttons); geometry->install();
    UiLanguage::apply(this); load(boundary);
}
TemporaryFilesDialog::~TemporaryFilesDialog() {
    for (auto child : findChildren<QObject *>()) child->disconnect(this);
    operations.disconnect(this); operations.cancel(); stopScanner();
}
void TemporaryFilesDialog::stopScanner() {
    reading = false; inspecting = false;
    for (auto holder : {&scanner, &propertiesReader}) {
        auto process = holder->data(); holder->clear(); if (!process) continue;
        process->disconnect(this); process->setParent(qApp);
        connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process, &QObject::deleteLater);
        connect(process, &QProcess::errorOccurred, process, [process](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) process->deleteLater(); });
        if (process->state() == QProcess::Starting) connect(process, &QProcess::started, process, &QProcess::kill);
        if (process->state() == QProcess::NotRunning) process->deleteLater(); else process->kill();
    }
}
void TemporaryFilesDialog::report(QString message) { status->setText(message); status->setToolTip(message); emit failed(message); }
bool TemporaryFilesDialog::protectedItem(QString path) const {
    if (!protection) return false;
    for (const auto &active : protection()) {
        const auto normalized = QDir::cleanPath(QFileInfo(active).absoluteFilePath());
        if (path == normalized || path.startsWith(normalized + '/') || normalized.startsWith(path + '/')) return true;
    }
    return false;
}
QStringList TemporaryFilesDialog::selectedPaths() const {
    QStringList paths; for (auto item : files->selectedItems()) if (!item->data(0, ParentRole).toBool()) paths << item->data(0, PathRole).toString(); return paths;
}
void TemporaryFilesDialog::updateButtons() {
    const bool busy = reading || inspecting || operations.busy(); const auto selected = selectedPaths();
    bool protectedSelection = false; for (const auto &path : selected) if (protectedItem(path)) protectedSelection = true;
    removeButton->setEnabled(!busy && !selected.isEmpty() && !protectedSelection);
    refreshButton->setEnabled(!busy); upButton->setEnabled(!busy && !current.isEmpty() && current != boundary);
}
void TemporaryFilesDialog::load(QString path, QStringList selected, QString focused) {
    if (operations.busy()) return;
    stopScanner();
    const auto root = FileInstall::capture(boundary), directory = FileInstall::capture(path);
    if (boundary.isEmpty() || !root.exists || !root.error.isEmpty() || root.stamp.st_dev != rootSnapshot.stamp.st_dev || root.stamp.st_ino != rootSnapshot.stamp.st_ino ||
        !directory.exists || !directory.error.isEmpty() || !S_ISDIR(directory.stamp.st_mode) || (path != boundary && !path.startsWith(boundary + '/'))) {
        reading = false; files->clear(); snapshots.clear(); report("Cannot read temporary folder: " + path + '\n' + directory.error); updateButtons(); emit listed(false); return;
    }
    current = path; pathBox->setText(current + '/'); reading = true; files->clear(); snapshots.clear(); status->setToolTip({}); status->setText(UiLanguage::text("Scanning...")); updateButtons();
    auto process = new QProcess(this); scanner = process;
    auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_TEMP_FOLDER", current); environment.insert("SEVENZIP_PORT_TEMP_ROOT", boundary); process->setProcessEnvironment(environment);
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (scanner != process || error != QProcess::FailedToStart) return;
        reading = false; scanner.clear(); report("List temporary files\n" + current + '\n' + process->errorString()); updateButtons(); emit listed(false); process->deleteLater();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, process, selected, focused](int code, QProcess::ExitStatus exit) {
        if (scanner != process) return;
        scanner.clear(); reading = false; files->setSortingEnabled(false); files->clear(); snapshots.clear();
        QJsonParseError parse; const auto data = QJsonDocument::fromJson(process->readAllStandardOutput(), &parse);
        QString error;
        if (exit != QProcess::NormalExit || code != 0 || parse.error != QJsonParseError::NoError || !data.isArray())
            error = "List temporary files\n" + current + "\nExit code: " + QString::number(code) + '\n' + QString::fromUtf8(process->readAllStandardError()) + (parse.error == QJsonParseError::NoError ? QString() : parse.errorString());
        QString captureError; const auto folder = error.isEmpty() ? FileInstall::openFolder(FileInstall::capture(current), &captureError) : nullptr;
        if (!captureError.isEmpty()) error = captureError;
        if (error.isEmpty()) {
            if (current != boundary && showDots) { auto parent = new TempItem(files, {".."}); parent->setData(0, ParentRole, true); }
            for (const auto &value : data.array()) {
                const auto row = value.toObject(); const auto name = row.value("name").toString();
                if (name.isEmpty() || name == "." || name == ".." || name.contains('/')) { error = "Invalid temporary item name."; break; }
                auto snapshot = FileInstall::captureAt(folder, QFile::encodeName(name)); if (!snapshot.exists || !snapshot.error.isEmpty()) continue;
                const auto path = current + '/' + name; snapshots.insert(path, snapshot);
                const bool interrupted = row.value("interrupted").toBool(); const QString suffix = interrupted ? "+" : "";
                const qint64 modified = row.value("modified").toString().toLongLong();
                auto item = new TempItem(files, {name, QDateTime::fromSecsSinceEpoch(modified).toString("yyyy-MM-dd HH:mm:ss"), row.value("sizeText").toString() + suffix,
                    row.value("link").toBool() ? "Link" : row.value("files").toInt() ? QString::number(row.value("files").toInt()) + suffix : QString(), row.value("folders").toInt() ? QString::number(row.value("folders").toInt()) + suffix : QString(), row.value("subName").toString()});
                if (files->property("realIcons").toBool()) item->setIcon(0, QFileIconProvider().icon(QFileInfo(path)));
                else item->setIcon(0, style()->standardIcon(row.value("directory").toBool() ? QStyle::SP_DirIcon : QStyle::SP_FileIcon));
                item->setData(0, PathRole, path); item->setData(0, FolderRole, row.value("directory").toBool()); item->setData(0, LinkRole, row.value("link").toBool()); item->setData(0, OrderRole, files->topLevelItemCount());
                item->setData(1, SortRole, modified); item->setData(2, SortRole, row.value("size").toString().toULongLong()); item->setData(3, SortRole, row.value("files").toInt()); item->setData(4, SortRole, row.value("folders").toInt());
                for (int column = 2; column <= 4; ++column) item->setTextAlignment(column, Qt::AlignRight | Qt::AlignVCenter);
                if (protectedItem(path)) item->setToolTip(0, "In use by 7-Zip");
                if (!row.value("error").toString().isEmpty()) item->setToolTip(2, row.value("error").toString());
                item->setSelected(selected.contains(path)); if (path == focused) files->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
            }
        }
        files->setSortingEnabled(true); files->sortItems(sortedColumn, ascending ? Qt::AscendingOrder : Qt::DescendingOrder);
        if (!files->currentItem() && files->topLevelItemCount()) files->setCurrentItem(files->topLevelItem(0), 0, QItemSelectionModel::NoUpdate);
        status->clear(); status->setToolTip({}); if (!error.isEmpty()) report(error); else if (!operationError.isEmpty()) { status->setText(operationError); status->setToolTip(operationError); operationError.clear(); }
        updateButtons(); emit listed(error.isEmpty()); process->deleteLater();
    });
    process->start(executable, {"i"});
}
void TemporaryFilesDialog::refresh() { const auto focused = files->currentItem(); load(current, selectedPaths(), focused ? focused->data(0, PathRole).toString() : QString()); }
void TemporaryFilesDialog::up() { if (current != boundary && !reading && !operations.busy()) { const auto old = current; load(QFileInfo(current).absolutePath(), {old}, old); } }
void TemporaryFilesDialog::enter(Qt::KeyboardModifiers modifiers) {
    if (reading || inspecting || operations.busy() || files->selectedItems().size() != 1) return;
    auto item = files->selectedItems().first(); if (item->data(0, ParentRole).toBool()) { up(); return; }
    if (modifiers & Qt::AltModifier) { showProperties(); return; }
    if (item->data(0, LinkRole).toBool()) { report("Link open operation was blocked."); return; }
    if (!item->data(0, FolderRole).toBool() || ((modifiers & Qt::ShiftModifier) && !(modifiers & Qt::ControlModifier))) { openOutside(false); return; }
    load(item->data(0, PathRole).toString());
}
void TemporaryFilesDialog::removeSelected() {
    if (reading || inspecting || operations.busy()) return;
    const auto paths = selectedPaths(); if (paths.isEmpty()) return;
    for (const auto &path : paths) if (protectedItem(path)) { report("Temporary files are still in use by 7-Zip: " + path); return; }
    if (QMessageBox::question(this, UiLanguage::text("Confirm Delete"), "Move the selected temporary files to the Trash?\n\n" + paths.join('\n'), QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No) != QMessageBox::Yes) return;
    QList<FileSnapshot> items;
    for (const auto &path : paths) {
        if (protectedItem(path) || !snapshots.contains(path) || !FileInstall::unchanged(snapshots[path])) { report("Temporary item changed or is in use: " + path); return; }
        items.append(snapshots[path]);
    }
    operations.trashSnapshots(std::move(items)); updateButtons();
}
void TemporaryFilesDialog::showProperties() {
    const auto paths = selectedPaths(); if (paths.size() != 1 || reading || inspecting || operations.busy()) return;
    const auto path = paths.first(); const auto fresh = FileInstall::inspect(path);
    if (!fresh.error.isEmpty() || !fresh.exists || S_ISLNK(fresh.stamp.st_mode)) {
        const auto error = "Temporary Properties refused: " + path + '\n' + (fresh.error.isEmpty() ? !fresh.exists ? "Item no longer exists." : "Link open operation was blocked." : fresh.error);
        report(error); if (!fresh.exists && fresh.error.isEmpty()) { operationError = error; load(current); } return;
    }
    auto process = new QProcess(this); process->setObjectName("temporaryPropertiesReader"); propertiesReader = process; inspecting = true; updateButtons(); status->setText(UiLanguage::text("Scanning..."));
    auto environment = QProcessEnvironment::systemEnvironment(); environment.insert("SEVENZIP_PORT_TEMP_FOLDER", current); environment.insert("SEVENZIP_PORT_TEMP_ROOT", boundary); environment.insert("SEVENZIP_PORT_TEMP_PROPERTIES", QFileInfo(path).fileName());
    const QMap<QString, QString> labels{{"SIZE", "Size"}, {"MTIME", "Modified"}, {"ATTRIBUTES", "Attributes"}, {"FOLDERS", "Folders"}, {"FILES", "Files"}, {"FILE_SIZE", "{0} bytes"}};
    for (auto label = labels.cbegin(); label != labels.cend(); ++label) environment.insert("SEVENZIP_PORT_TEMP_LABEL_" + label.key(), UiLanguage::text(label.value()));
    process->setProcessEnvironment(environment);
    connect(process, &QProcess::errorOccurred, this, [this, process, path](QProcess::ProcessError error) {
        if (propertiesReader != process || error != QProcess::FailedToStart) return;
        propertiesReader.clear(); inspecting = false; report("Temporary Properties\n" + path + '\n' + process->errorString()); updateButtons(); process->deleteLater();
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, process, path](int code, QProcess::ExitStatus exit) {
        if (propertiesReader != process) return;
        propertiesReader.clear(); inspecting = false; QJsonParseError parse; const auto result = QJsonDocument::fromJson(process->readAllStandardOutput(), &parse); updateButtons();
        if (exit != QProcess::NormalExit || code != 0 || parse.error != QJsonParseError::NoError || !result.isObject() || !result.object().value("text").isString()) {
            report("Temporary Properties\n" + path + "\nExit code: " + QString::number(code) + '\n' + QString::fromUtf8(process->readAllStandardError())); process->deleteLater(); return;
        }
        const auto error = result.object().value("error").toString(); if (!error.isEmpty()) report(error); else status->clear();
        // BrowseDialog2::Show_FileProps_Window uses a plain MB_OK message.
        // QMessageBox discards its title on macOS. A Widgets message dialog
        // retains the original Properties title and centered single OK button.
        auto dialog = new QDialog(this); dialog->setObjectName("temporaryProperties"); dialog->setWindowTitle(UiLanguage::text("Properties")); dialog->setWindowModality(Qt::ApplicationModal); dialog->setAttribute(Qt::WA_DeleteOnClose);
        auto layout = new QVBoxLayout(dialog); auto text = new QLabel(result.object().value("text").toString(), dialog); text->setObjectName("temporaryPropertyText"); text->setTextFormat(Qt::PlainText); text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard); text->setWordWrap(true); layout->addWidget(text);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dialog); buttons->setObjectName("temporaryPropertiesButtons"); buttons->setCenterButtons(true); layout->addWidget(buttons); connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
        dialog->resize(620, dialog->sizeHint().height()); dialog->open(); process->deleteLater();
    });
    process->start(executable, {"i"});
}
void TemporaryFilesDialog::openOutside(bool manager) {
    const auto paths = selectedPaths(); if (paths.size() > 1 || reading || inspecting || operations.busy()) return;
    const auto path = paths.isEmpty() ? current : paths.first();
    const auto source = paths.isEmpty() ? FileInstall::capture(current) : snapshots.value(path);
    if (!FileInstall::unchanged(source) || S_ISLNK(source.stamp.st_mode)) { report("Temporary item changed or is a link: " + path); return; }
    emit openRequested(path, manager);
}
void TemporaryFilesDialog::contextMenu(QPoint position) {
    if (reading || inspecting || operations.busy()) return;
    const auto selected = selectedPaths(); QMenu menu(this);
    if (!selected.isEmpty()) { auto remove = menu.addAction(UiLanguage::text("Delete") + "\tDelete", this, &TemporaryFilesDialog::removeSelected); remove->setEnabled(removeButton->isEnabled()); }
    if (selected.size() <= 1) {
        if (!selected.isEmpty()) menu.addSeparator();
        const bool one = selected.size() == 1;
        menu.addAction(UiLanguage::text("Open Outside") + "\tShift+Enter", this, [this] { openOutside(false); });
        menu.addAction(UiLanguage::text("Open Outside") + " : 7-Zip", this, [this] { openOutside(true); });
        menu.addSeparator(); menu.addAction(UiLanguage::text("Properties") + "\tAlt+Enter", this, [this] {
            if (files->selectedItems().size() == 1 && files->selectedItems().first()->data(0, LinkRole).toBool()) report("Link open operation was blocked."); else showProperties();
        })->setEnabled(one);
    }
    menu.exec(files->viewport()->mapToGlobal(position));
}
bool TemporaryFilesDialog::eventFilter(QObject *object, QEvent *event) {
    if (object == files && event->type() == QEvent::KeyPress) {
        if (inspecting) return true;
        auto key = static_cast<QKeyEvent *>(event); const auto modifiers = key->modifiers(); const bool ctrl = modifiers & Qt::ControlModifier;
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) { enter(modifiers); return true; }
        if (key->key() == Qt::Key_Backspace) { up(); return true; }
        if (key->key() == Qt::Key_Delete) { removeSelected(); return true; }
        if (ctrl && key->key() == Qt::Key_R) { refresh(); return true; }
        if (ctrl && key->key() == Qt::Key_A) { files->selectAll(); return true; }
        if (ctrl && (key->key() == Qt::Key_F3 || key->key() == Qt::Key_F5 || key->key() == Qt::Key_F6)) {
            const int column = key->key() == Qt::Key_F3 ? 0 : key->key() == Qt::Key_F5 ? 1 : 2;
            emit files->header()->sectionClicked(column); return true;
        }
    }
    return QDialog::eventFilter(object, event);
}
void TemporaryFilesDialog::reject() {
    if (operations.busy()) { closing = true; operations.cancel(); return; }
    stopScanner(); QDialog::reject();
}
