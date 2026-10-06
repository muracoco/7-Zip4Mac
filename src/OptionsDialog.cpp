// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "OptionsDialog.h"
#include "UiLanguage.h"
#include "upstream/UiResourceIds.h"
using namespace OfficialUi;
#include "Help.h"
#include "OpenWith.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QTreeWidget>
#include <QHeaderView>
#include <QComboBox>
#include <QMessageBox>
#include <QMenu>
#include <QStorageInfo>
#include <DiskArbitration/DiskArbitration.h>
#include <QPlainTextEdit>
#include <QTimer>

FileManagerSettings FileManagerSettings::load() {
    FileManagerSettings v; QSettings s; s.beginGroup("Options");
    v.showDots = s.value("ShowDots", false).toBool(); v.realIcons = s.value("ShowRealFileIcons", false).toBool();
    v.fullRow = s.value("FullRow", false).toBool(); v.grid = s.value("ShowGrid", false).toBool();
    v.singleClick = s.value("SingleClick", false).toBool(); v.alternativeSelection = s.value("AlternativeSelection", false).toBool();
    v.workMode = qBound(0, s.value("WorkMode", 0).toInt(), 2); v.workPath = s.value("WorkPath").toString();
    v.removableOnly = s.value("ForRemovableOnly", true).toBool();
    v.viewer = s.value("Viewer").toString(); v.editor = s.value("Editor").toString(); v.diff = s.value("Diff").toString();
    v.eliminateRoot = s.value("EliminateRoot", true).toBool(); v.language = s.value("Language").toString(); if (v.language.isEmpty()) v.language = UiLanguage::defaultLanguage(); if (v.language == "-") v.language = "en";
    v.memoryLimitGB = qBound(0, s.value("ExtractMemoryLimitGB", 0).toInt(), 16384); return v;
}
bool FileManagerSettings::save() const {
    QSettings s; s.beginGroup("Options");
    s.setValue("ShowDots", showDots); s.setValue("ShowRealFileIcons", realIcons); s.setValue("FullRow", fullRow); s.setValue("ShowGrid", grid);
    s.setValue("SingleClick", singleClick); s.setValue("AlternativeSelection", alternativeSelection);
    s.setValue("WorkMode", workMode); s.setValue("WorkPath", workPath); s.setValue("ForRemovableOnly", removableOnly);
    s.setValue("Viewer", viewer); s.setValue("Editor", editor); s.setValue("Diff", diff); s.setValue("ExtractMemoryLimitGB", memoryLimitGB);
    s.setValue("EliminateRoot", eliminateRoot); s.setValue("Language", language);
    s.sync(); return s.status() == QSettings::NoError;
}
QString FileManagerSettings::workingFolder(const QString &sourceFolder) const {
    bool useSetting = !removableOnly;
    if (removableOnly) {
        // Use the media property, rather than treating every /Volumes mount
        // (including SMB shares) as a removable drive.
        const auto bytes = QStorageInfo(sourceFolder).rootPath().toUtf8();
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8 *>(bytes.constData()), bytes.size(), true);
        DASessionRef session = DASessionCreate(nullptr);
        DADiskRef disk = url && session ? DADiskCreateFromVolumePath(nullptr, session, url) : nullptr;
        CFDictionaryRef description = disk ? DADiskCopyDescription(disk) : nullptr;
        if (description) useSetting = CFDictionaryGetValue(description, kDADiskDescriptionMediaRemovableKey) == kCFBooleanTrue;
        if (description) CFRelease(description); if (disk) CFRelease(disk); if (session) CFRelease(session); if (url) CFRelease(url);
    }
    if (!useSetting || workMode == 1) return QFileInfo(sourceFolder).absoluteFilePath();
    return workMode == 2 ? workPath : QDir::tempPath();
}

OptionsDialog::OptionsDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Options"); setObjectName("optionsDialog"); resize(600, 465);
    auto outer = new QVBoxLayout(this); auto tabs = new QTabWidget(this); tabs->setObjectName("optionsTabs"); outer->addWidget(tabs);
    auto page = [tabs](QString title, unsigned id) { auto widget = new QWidget(tabs); widget->setProperty("uiTabId", id); tabs->addTab(widget, title); return new QVBoxLayout(widget); };
    auto note = [](QVBoxLayout *layout, QString text) { auto label = new QLabel(text); label->setWordWrap(true); layout->addWidget(label); };
    auto disabled = [](QVBoxLayout *layout, QString text, QString reason, unsigned id) { auto check = new QCheckBox(text); check->setEnabled(false); check->setToolTip(reason); UiLanguage::bind(check, id); layout->addWidget(check); };
    // Page order and control names follow FileManager/OptionsDialog.cpp and
    // the matching *Page*.rc resources from official 7-Zip 26.03.
    auto system = page("System", IDD_SYSTEM); auto associate = new QLabel("Associate 7-Zip with:"); UiLanguage::bind(associate, IDT_SYSTEM_ASSOCIATE); system->addWidget(associate);
    auto types = new QTreeWidget; types->setRootIsDecorated(false); types->setHeaderLabels({"Type", "Support"});
    for (const auto &type : {"7z", "zip", "rar", "tar", "gz", "xz", "bz2", "wim", "001", "sha256", "sha1"}) new QTreeWidgetItem(types, {type, "Declared in this app bundle"});
    types->header()->setStretchLastSection(true); types->setColumnWidth(0, 90); system->addWidget(types);
    note(system, "On macOS, select an archive in Finder → Get Info → Open with → 7-Zip Mac Port → Change All. This app does not change default applications automatically.");
    auto state = FileManagerSettings::load(); auto shell = page("7-Zip", IDD_MENU);
    note(shell, "Finder → Open With → 7-Zip Mac Port displays these commands. Choose which commands appear; the File Manager command always remains at the bottom.");
    const auto openSettings = OpenWithSettings::load();
    openWithIcons = new QCheckBox("Icons in context menu"); openWithIcons->setObjectName("openWithIcons"); openWithIcons->setChecked(openSettings.icons); shell->addWidget(openWithIcons);
    openWithItems = new QTreeWidget; openWithItems->setObjectName("openWithItems"); openWithItems->setHeaderHidden(true); openWithItems->setRootIsDecorated(false); openWithItems->setMinimumHeight(190);
    for (const auto &item : OpenWithSettings::items()) { auto row = new QTreeWidgetItem(openWithItems, {openWithItemLabel(item)}); row->setData(0, Qt::UserRole, item.id); row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable); row->setCheckState(0, openSettings.enabled.contains(item.id) ? Qt::Checked : Qt::Unchecked); }
    shell->addWidget(openWithItems);
    eliminateRoot = new QCheckBox("Eliminate duplication of root folder"); eliminateRoot->setObjectName("eliminateRoot"); eliminateRoot->setChecked(state.eliminateRoot); shell->addWidget(eliminateRoot);
    note(shell, "Open archive > offers the official *, #, #:e, 7z, zip, cab and rar choices. Explorer registration and Windows MAPI email commands are not provided by this macOS menu.");
    auto folders = page("Folders", IDD_FOLDERS); auto workLabel = new QLabel("&Working folder"); UiLanguage::bind(workLabel, IDT_FOLDERS_WORKING_FOLDER); folders->addWidget(workLabel);
    workSystem = new QRadioButton("&System temp folder"); workSystem->setObjectName("workSystem");
    workCurrent = new QRadioButton("&Current"); workCurrent->setObjectName("workCurrent");
    workSpecified = new QRadioButton("Specified:"); workSpecified->setObjectName("workSpecified");
    folders->addWidget(workSystem); folders->addWidget(workCurrent); folders->addWidget(workSpecified);
    (state.workMode == 0 ? workSystem : state.workMode == 1 ? workCurrent : workSpecified)->setChecked(true);
    auto workRow = new QHBoxLayout; workPath = new QLineEdit(state.workPath); workPath->setObjectName("workPath"); auto browseWork = new QPushButton("..."); browseWork->setFixedWidth(32); workRow->addWidget(workPath); workRow->addWidget(browseWork); folders->addLayout(workRow);
    removable = new QCheckBox("Use for removable drives only"); removable->setObjectName("removableOnly"); removable->setChecked(state.removableOnly); folders->addWidget(removable); folders->addStretch();
    auto enablePath = [this, browseWork] { workPath->setEnabled(workSpecified->isChecked()); browseWork->setEnabled(workSpecified->isChecked()); };
    connect(workSpecified, &QRadioButton::toggled, this, enablePath); enablePath();
    connect(browseWork, &QPushButton::clicked, this, [this] { auto path = QFileDialog::getExistingDirectory(this, "Working folder", workPath->text()); if (!path.isEmpty()) workPath->setText(path); });
    auto edit = page("Editor", IDD_EDIT);
    auto program = [this, edit](QString label, QString name, QString value, unsigned id) {
        auto text = new QLabel(label); UiLanguage::bind(text, id); text->setProperty("uiAppendColon", true); auto row = new QHBoxLayout; auto input = new QLineEdit(value); input->setObjectName(name); text->setBuddy(input);
        auto browse = new QPushButton("..."); browse->setFixedWidth(32); edit->addWidget(text); row->addWidget(input); row->addWidget(browse); edit->addLayout(row);
        auto menu = new QMenu(browse); browse->setMenu(menu);
        connect(menu->addAction("Application (.app)..."), &QAction::triggered, this, [this, input] {
            auto path = QFileDialog::getExistingDirectory(this, "Choose application (.app)", "/Applications");
            if (path.isEmpty()) return;
            if (!path.endsWith(".app", Qt::CaseInsensitive)) { QMessageBox::warning(this, "Options", "Choose an application bundle ending in .app."); return; }
            input->setText('"' + path + '"');
        });
        connect(menu->addAction("Executable..."), &QAction::triggered, this, [this, input] { auto path = QFileDialog::getOpenFileName(this, "Choose executable", "/usr/bin"); if (!path.isEmpty()) input->setText('"' + path + '"'); });
        return input;
    };
    viewer = program("&View:", "viewerCommand", state.viewer, IDT_EDIT_VIEWER); editor = program("&Editor:", "editorCommand", state.editor, IDT_EDIT_EDITOR); diff = program("&Diff:", "diffCommand", state.diff, IDT_EDIT_DIFF);
    note(edit, "Choose an application (.app) or an executable. Optional arguments may follow the executable; quote paths containing spaces. Selected file paths are appended as separate arguments. Empty View / Editor uses the default macOS application."); edit->addStretch();
    auto settings = page("Settings", IDD_SETTINGS);
    auto check = [settings](QString label, QString name, bool value) { auto widget = new QCheckBox(label); widget->setObjectName(name); widget->setChecked(value); settings->addWidget(widget); return widget; };
    dots = check("Show \"..\" item", "showDots", state.showDots); icons = check("Show real file &icons", "realIcons", state.realIcons);
    fullRow = check("&Full row select", "fullRow", state.fullRow); grid = check("Show &grid lines", "showGrid", state.grid);
    singleClick = check("&Single-click to open an item", "singleClick", state.singleClick); alternative = check("&Alternative selection mode", "alternativeSelection", state.alternativeSelection);
    disabled(settings, "Show system &menu", "Windows shell context menu is not available on macOS.", IDX_SETTINGS_SHOW_SYSTEM_MENU);
    disabled(settings, "Use &large memory pages", "Windows lock-memory privilege is not available in this port.", IDX_SETTINGS_LARGE_PAGES);
    auto memoryLabel = new QLabel("Maximum amount of RAM memory usage allowed to unpack archives:"); UiLanguage::bind(memoryLabel, IDT_MEM_USAGE_EXTRACT); settings->addWidget(memoryLabel);
    auto memory = new QHBoxLayout; limitEnabled = new QCheckBox; limitEnabled->setAccessibleName("Set extraction memory limit"); limitEnabled->setObjectName("memoryLimitEnabled"); limitEnabled->setChecked(state.memoryLimitGB > 0);
    limit = new QSpinBox; limit->setObjectName("memoryLimitGB"); limit->setRange(1, 16384); limit->setValue(state.memoryLimitGB > 0 ? state.memoryLimitGB : 4); limit->setEnabled(limitEnabled->isChecked());
    memory->addWidget(limitEnabled); memory->addWidget(limit); memory->addWidget(new QLabel("GB")); memory->addStretch(); settings->addLayout(memory); settings->addStretch(); connect(limitEnabled, &QCheckBox::toggled, limit, &QSpinBox::setEnabled);
    auto langPage = page("Language", IDD_LANG); auto languageLabel = new QLabel("Language:"); UiLanguage::bind(languageLabel, IDT_LANG_LANG); langPage->addWidget(languageLabel); language = new QComboBox; language->setObjectName("uiLanguage");
    for (const auto &entry : UiLanguage::available()) language->addItem(entry.second, entry.first);
    language->setCurrentIndex(qMax(0, language->findData(state.language))); langPage->addWidget(language);
    auto languageInfo = new QPlainTextEdit; languageInfo->setObjectName("languageInfo"); languageInfo->setReadOnly(true); langPage->addWidget(languageInfo, 1);
    auto showLanguageInfo = [this, languageInfo] { languageInfo->setPlainText(UiLanguage::information(language->currentData().toString())); };
    connect(language, &QComboBox::currentIndexChanged, this, showLanguageInfo); showLanguageInfo();
    const auto invalid = UiLanguage::invalidFiles();
    if (!invalid.isEmpty()) QTimer::singleShot(0, this, [this, invalid] { QMessageBox::warning(this, "Error in Lang file", invalid.join(' ')); });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply | QDialogButtonBox::Help); buttons->setObjectName("optionsButtons"); outer->addWidget(buttons); applyButton = buttons->button(QDialogButtonBox::Apply); applyButton->setEnabled(false);
    auto changed = [this] { applyButton->setEnabled(true); };
    for (auto control : findChildren<QCheckBox *>()) if (control->isEnabled()) connect(control, &QCheckBox::toggled, this, changed);
    for (auto control : findChildren<QRadioButton *>()) connect(control, &QRadioButton::toggled, this, changed);
    for (auto control : findChildren<QLineEdit *>()) connect(control, &QLineEdit::textChanged, this, changed);
    connect(limit, &QSpinBox::valueChanged, this, changed);
    connect(language, &QComboBox::currentIndexChanged, this, changed);
    connect(openWithItems, &QTreeWidget::itemChanged, this, changed);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] { if (apply()) accept(); }); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(applyButton, &QPushButton::clicked, this, [this] { apply(); });
    connect(buttons, &QDialogButtonBox::helpRequested, this, [this, tabs] { const QStringList anchors{"system", "sevenZip", "folders", "editor", "settings", "language"}; Help::show(this, "fm/options.htm#" + anchors.value(tabs->currentIndex())); });
    UiLanguage::bindTitle(this, IDS_OPTIONS);
    UiLanguage::bind(openWithIcons, IDX_SYSTEM_ICON_IN_MENU); UiLanguage::bind(eliminateRoot, IDX_EXTRACT_ELIM_DUP);
    UiLanguage::bind(workSystem, IDR_FOLDERS_WORK_SYSTEM); UiLanguage::bind(workCurrent, IDR_FOLDERS_WORK_CURRENT); UiLanguage::bind(workSpecified, IDR_FOLDERS_WORK_SPECIFIED); UiLanguage::bind(removable, IDX_FOLDERS_WORK_FOR_REMOVABLE);
    UiLanguage::bind(dots, IDX_SETTINGS_SHOW_DOTS); UiLanguage::bind(icons, IDX_SETTINGS_SHOW_REAL_FILE_ICONS); UiLanguage::bind(fullRow, IDX_SETTINGS_FULL_ROW); UiLanguage::bind(grid, IDX_SETTINGS_SHOW_GRID); UiLanguage::bind(singleClick, IDX_SETTINGS_SINGLE_CLICK); UiLanguage::bind(alternative, IDX_SETTINGS_ALTERNATIVE_SELECTION);
    UiLanguage::apply(this);
}
bool OptionsDialog::apply() {
    FileManagerSettings value;
    value.showDots = dots->isChecked(); value.realIcons = icons->isChecked(); value.fullRow = fullRow->isChecked(); value.grid = grid->isChecked(); value.singleClick = singleClick->isChecked(); value.alternativeSelection = alternative->isChecked();
    value.workMode = workSpecified->isChecked() ? 2 : workCurrent->isChecked() ? 1 : 0; value.workPath = workPath->text().trimmed(); value.removableOnly = removable->isChecked();
    if (value.workMode == 2 && (!QFileInfo(value.workPath).isDir() || !QFileInfo(value.workPath).isWritable() || !QFileInfo(value.workPath).isAbsolute())) {
        QMessageBox::warning(this, "Options", "Choose an existing, writable working folder using an absolute path."); return false;
    }
    value.viewer = viewer->text().trimmed(); value.editor = editor->text().trimmed(); value.diff = diff->text().trimmed(); value.memoryLimitGB = limitEnabled->isChecked() ? limit->value() : 0;
    value.eliminateRoot = eliminateRoot->isChecked(); value.language = language->currentData().toString();
    if (!value.save()) { QMessageBox::warning(this, "Options", "Cannot save settings. Check the application preferences directory permissions."); return false; }
    OpenWithSettings open; open.icons = openWithIcons->isChecked(); for (int n = 0; n < openWithItems->topLevelItemCount(); ++n) { auto row = openWithItems->topLevelItem(n); if (row->checkState(0) == Qt::Checked) open.enabled << row->data(0, Qt::UserRole).toString(); }
    if (!open.save()) { QMessageBox::warning(this, "Options", "Cannot save Open With menu settings."); return false; }
    applyButton->setEnabled(false); UiLanguage::set(value.language); emit settingsApplied(); UiLanguage::apply(this);
    { QSignalBlocker blocker(openWithItems); for (int n = 0; n < openWithItems->topLevelItemCount(); ++n) openWithItems->topLevelItem(n)->setText(0, openWithItemLabel(OpenWithSettings::items().at(n))); }
    return true;
}
