// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#include "OpenWith.h"
#include "ArchiveFormats.h"
#include "MainWindow.h"
#include "UiLanguage.h"
#include "WindowPlacement.h"
#include <QApplication>
#include <QFileOpenEvent>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QLabel>
#include <QVBoxLayout>
#include <QPushButton>
#include <QMenu>
#include <QBitmap>
#include <QShowEvent>
#include <QFrame>
#include <QMessageBox>

QList<OpenWithItem> OpenWithSettings::items() {
    // FileManager/MenuPage.cpp kMenuItems and Explorer/ContextMenu.cpp.
    // Windows MAPI commands remain platform-specific.
    return {{"open", "Open archive"}, {"openAs", "Open archive >"}, {"extract", "Extract files..."}, {"here", "Extract Here"},
            {"to", "Extract to \"<name>/\""}, {"test", "Test archive"}, {"add", "Add to archive..."},
            {"7z", "Add to \"<name>.7z\""}, {"zip", "Add to \"<name>.zip\""}, {"checksum", "CRC SHA"}};
}
QString openWithItemLabel(const OpenWithItem &item, QString name) {
    if (item.id == "openAs") return UiLanguage::text("Open archive") + " >";
    if (item.id == "to") return UiLanguage::text("Extract to {0}").replace("{0}", '"' + name + "/\"");
    if (item.id == "7z" || item.id == "zip") return UiLanguage::text("Add to {0}").replace("{0}", '"' + name + '.' + item.id + '"');
    return UiLanguage::text(item.label);
}
OpenWithSettings OpenWithSettings::load() {
    QSettings s; OpenWithSettings value;
    if (s.contains("OpenWith/Items")) value.enabled = s.value("OpenWith/Items").toStringList();
    else for (const auto &item : items()) value.enabled << item.id;
    value.icons = s.value("OpenWith/Icons", true).toBool(); return value;
}
bool OpenWithSettings::save() const { QSettings s; s.setValue("OpenWith/Items", enabled); s.setValue("OpenWith/Icons", icons); s.sync(); return s.status() == QSettings::NoError; }
QList<QPair<QString, QString>> checksumMethods() {
    return {{"CRC-32", "CRC32"}, {"CRC-64", "CRC64"}, {"XXH64", "XXH64"}, {"MD5", "MD5"},
            {"SHA-1", "SHA1"}, {"SHA-256", "SHA256"}, {"SHA-384", "SHA384"}, {"SHA-512", "SHA512"},
            {"SHA3-256", "SHA3-256"}, {"BLAKE2sp", "BLAKE2sp"}, {"*", "*"}};
}
QString openWithCommonParent(const QStringList &paths) {
    if (paths.isEmpty()) return {};
    QString base = QFileInfo(paths.first()).absolutePath();
    for (const auto &path : paths) while (base != "/" && !QFileInfo(path).absoluteFilePath().startsWith(base + '/')) base = QFileInfo(base).absolutePath();
    return base;
}
QString openWithArchiveName(const QStringList &paths) {
    if (paths.isEmpty()) return "Archive";
    QFileInfo f(paths.first()); QString name = paths.size() == 1 ? f.fileName() : QFileInfo(f.absolutePath()).fileName();
    if (paths.size() == 1 && !f.isDir() && name.count('.') == 1 && name.indexOf('.') > 0) name.truncate(name.indexOf('.'));
    if (name.isEmpty() || name == "." || name == "..") name = "Archive";
    const auto base = name; int index = 2; bool conflict;
    do { conflict = false; for (const auto &path : paths) for (const auto &ext : {"7z", "zip", "tar", "wim"}) conflict |= QFileInfo(path).fileName().compare(name + '.' + ext, Qt::CaseInsensitive) == 0; if (conflict) name = base + '_' + QString::number(index++); } while (conflict);
    return name;
}
QString openWithExtractName(const QString &path) {
    QFileInfo f(path); QString name = f.fileName(); const int dot = name.lastIndexOf('.');
    if (dot < 0) return name + '~';
    const QString ext = name.mid(dot + 1).toLower(); name.truncate(dot);
    const auto second = QFileInfo(name).suffix().toLower();
    if ((ext == "001" && QStringList{"7z", "bz2", "gz", "rar", "zip"}.contains(second)) ||
        (ext == "rar" && QStringList{"part001", "part01", "part1"}.contains(second))) name = QFileInfo(name).completeBaseName();
    return name.isEmpty() || name == "." || name == ".." ? "Archive" : name;
}
bool openWithArchiveCandidate(const QString &path) {
    if (!QFileInfo(path).isFile()) return false;
    if (ArchiveFormats::recognizes(path)) return true;
    // Same negative extension heuristic as Explorer/ContextMenu.cpp.
    static const QStringList excluded = QString("3gp aac ans ape asc asm asp aspx avi awk bas bat bmp c cs cls clw cmd cpp csproj css ctl cxx def dep dlg dsp dsw eps f f77 f90 f95 fla flac frm gif h hpp hta htm html hxx ico idl inc ini inl java jpeg jpg js la lnk log mak manifest wmv mov mp3 mp4 mpe mpeg mpg m4a ofr ogg pac pas pdf php php3 php4 php5 phptml pl pm png ps py pyo ra rb rc reg rka rm rtf sed sh shn shtml sln sql srt swa tcl tex tiff tta txt vb vcproj vbs mkv wav webm wma wv xml xsd xsl xslt").split(' ');
    return !excluded.contains(QFileInfo(path).suffix().toLower());
}
struct OpenWithPresentation {
    OpenWithItem item;
    QString label;
    QString icon;
    bool archive;
};
static QList<OpenWithPresentation> visibleOpenWithItems(const QStringList &paths, const OpenWithSettings &settings) {
    bool archives = !paths.isEmpty();
    for (const auto &path : paths) archives &= openWithArchiveCandidate(path);
    const auto archiveName = openWithArchiveName(paths);
    const auto extractName = paths.size() == 1 ? openWithExtractName(paths.first()) : QString("<name>");
    QList<OpenWithPresentation> items;
    for (const auto &item : OpenWithSettings::items()) {
        if (!settings.enabled.contains(item.id)) continue;
        const bool archiveItem = QStringList{"open", "openAs", "extract", "here", "to", "test"}.contains(item.id);
        if (archiveItem && (!archives || ((item.id == "open" || item.id == "openAs") && paths.size() != 1))) continue;
        const auto name = item.id == "to" ? extractName : archiveName;
        const auto icon = archiveItem ? (item.id == "test" ? "Test" : "Extract") : item.id == "checksum" ? "Info" : "Add";
        items.append({item, openWithItemLabel(item, name.left(64).replace('&', "&&")), icon, archiveItem});
    }
    return items;
}
static bool openWithTargetsExist(const QStringList &paths) {
    if (paths.isEmpty()) return false;
    for (const auto &path : paths) if (!QFileInfo::exists(path)) return false;
    return true;
}
static void populateOpenAsMenu(QMenu *menu, const OpenWithSettings &settings, const std::function<void(QString)> &chosen) {
    menu->setObjectName("openAsMenu");
    for (const auto &type : ArchiveFormats::openTypes()) {
        if (type.isEmpty() && settings.enabled.contains("open")) continue;
        auto action = menu->addAction(type.isEmpty() ? UiLanguage::text("Open archive") : type);
        action->setObjectName("openAs_" + (type.isEmpty() ? QString("normal") : type)); action->setData(type);
        QObject::connect(action, &QAction::triggered, menu, [chosen, type] { chosen("openAs:" + type); });
    }
}
static void populateChecksumMenu(QMenu *menu, const std::function<void(QString)> &chosen) {
    for (const auto &method : checksumMethods())
        QObject::connect(menu->addAction(method.first), &QAction::triggered, menu,
                         [chosen, id = method.second] { chosen("hash:" + id); });
    menu->addSeparator();
    QObject::connect(menu->addAction("SHA-256 -> file.sha256"), &QAction::triggered, menu, [chosen] { chosen("hashFile"); });
    QObject::connect(menu->addAction("Checksum : Test"), &QAction::triggered, menu, [chosen] { chosen("hashTest"); });
}
void populateOpenWithContextMenu(QMenu *menu, const QStringList &paths, std::function<void(QString)> chosen) {
    const auto settings = OpenWithSettings::load();
    const bool exists = openWithTargetsExist(paths);
    bool previousArchive = false;
    for (const auto &presentation : visibleOpenWithItems(paths, settings)) {
        const auto &item = presentation.item;
        const bool archiveItem = presentation.archive;
        if (!archiveItem && previousArchive) { menu->addSeparator(); previousArchive = false; }
        previousArchive |= archiveItem;
        const auto label = item.id == "openAs" ? UiLanguage::text("Open archive") : presentation.label;
        QAction *action;
        if (item.id == "openAs") {
            auto types = menu->addMenu(label); action = types->menuAction(); populateOpenAsMenu(types, settings, chosen);
        } else if (item.id == "checksum") {
            auto hashes = menu->addMenu(label); action = hashes->menuAction();
            populateChecksumMenu(hashes, chosen);
        } else {
            action = menu->addAction(label); QObject::connect(action, &QAction::triggered, menu, [chosen, id = item.id] { chosen(id); });
        }
        action->setObjectName("context_" + item.id); action->setEnabled(exists);
        if (settings.icons) {
            QPixmap p(":/icons/" + presentation.icon + "2.bmp"); p.setMask(p.createMaskFromColor(QColor(255, 0, 255))); action->setIcon(QIcon(p));
        }
    }
}
OpenWithMenu::OpenWithMenu(QStringList paths, QWidget *parent) : QDialog(parent), targets(std::move(paths)) {
    setObjectName("openWithMenu"); setWindowTitle("7-Zip"); setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint); setModal(true);
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(7, 7, 7, 7); layout->setSpacing(1);
    auto title = new QLabel(targets.size() == 1 ? QFileInfo(targets.first()).fileName() : QString::number(targets.size()) + " files");
    title->setToolTip(targets.join('\n')); title->setWordWrap(true); title->setStyleSheet("font-weight: bold; padding: 5px;"); layout->addWidget(title);
    const auto settings = OpenWithSettings::load();
    const bool allExist = openWithTargetsExist(targets);
    auto choose = [this](const QString &id) { hide(); emit commandChosen(id, targets); accept(); };
    auto line = [layout] { auto frame = new QFrame; frame->setFrameShape(QFrame::HLine); layout->addWidget(frame); };
    auto button = [this, layout, &settings](QString id, QString label, QString icon = {}) {
        auto b = new QPushButton(label, this); b->setObjectName("openWith_" + id); b->setAutoDefault(false); b->setMinimumHeight(30);
        b->setStyleSheet("QPushButton { text-align: left; padding: 4px 10px; border: 1px solid transparent; } QPushButton:hover, QPushButton:focus { background: #dceafa; border-color: #99bbdf; }");
        if (settings.icons && !icon.isEmpty()) { QPixmap p(":/icons/" + icon + "2.bmp"); p.setMask(p.createMaskFromColor(QColor(255, 0, 255))); b->setIcon(QIcon(p)); b->setIconSize(QSize(24, 24)); }
        layout->addWidget(b); return b;
    };
    bool previousArchive = false;
    for (const auto &presentation : visibleOpenWithItems(targets, settings)) {
        const auto &item = presentation.item;
        const bool archiveItem = presentation.archive;
        if (!archiveItem && previousArchive) { line(); previousArchive = false; }
        previousArchive |= archiveItem;
        auto b = button(item.id, presentation.label, presentation.icon); b->setEnabled(allExist);
        if (item.id == "openAs") {
            auto types = new QMenu(b); b->setMenu(types); b->setStyleSheet(b->styleSheet() + " QPushButton::menu-indicator { image: none; }"); populateOpenAsMenu(types, settings, choose);
        } else if (item.id == "checksum") {
            auto hashes = new QMenu(b); b->setMenu(hashes);
            populateChecksumMenu(hashes, choose);
        } else connect(b, &QPushButton::clicked, this, [choose, id = item.id] { choose(id); });
    }
    line(); auto manager = button("manager", QString::fromUtf8("7-Zip ファイルマネージャーで開く"), "Info");
    connect(manager, &QPushButton::clicked, this, [choose] { choose("manager"); });
    setMinimumWidth(360); adjustSize();
}
void OpenWithMenu::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    centerPortWindow(this);
}
OpenWithController::OpenWithController(MainWindow *w, bool quit) : QObject(w), window(w), quitWhenIdle(quit) {
    qApp->installEventFilter(this); timer.setSingleShot(true); timer.setInterval(100); connect(&timer, &QTimer::timeout, this, &OpenWithController::flush);
    idleExitTimer.setSingleShot(true); idleExitTimer.setInterval(150);
    connect(&idleExitTimer, &QTimer::timeout, this, [this] {
        if (hasRequests()) return;
        // A hidden-manager operation can outlive the menu's closing event;
        // completion can also leave a filesystem refresh in progress.
        if (window->operationBusy()) { checkIdleExit(); return; }
        for (auto widget : QApplication::topLevelWidgets()) if (widget->isVisible()) return;
        qApp->quit();
    });
    if (quitWhenIdle) connect(qApp, &QApplication::lastWindowClosed, this, &OpenWithController::checkIdleExit);
}
void OpenWithController::checkIdleExit() {
    if (!quitWhenIdle) return;
    idleExitTimer.start();
}
bool OpenWithController::eventFilter(QObject *object, QEvent *e) {
    if (e->type() == QEvent::Close || e->type() == QEvent::Hide) {
        if (auto widget = qobject_cast<QWidget *>(object); widget && widget->isWindow()) checkIdleExit();
    }
    if (object == qApp && e->type() == QEvent::FileOpen) {
        auto open = static_cast<QFileOpenEvent *>(e); const auto url = open->url();
        if (url.isLocalFile()) enqueue({url.toLocalFile()}); else if (url.isEmpty() && !open->file().isEmpty()) enqueue({open->file()});
        return true;
    } return QObject::eventFilter(object, e);
}
void OpenWithController::enqueue(QStringList paths) {
    for (const auto &path : paths) { if (path.isEmpty()) continue; const auto absolute = QFileInfo(path).absoluteFilePath(); if (!pending.contains(absolute)) pending << absolute; }
    if (!pending.isEmpty()) {
        // Finder's operation menu does not need a hidden startup folder scan.
        // Leave real compression/editor jobs queued, but supersede read-only
        // startup work so a large remembered folder cannot delay this menu.
        if (!window->isVisible() && !window->dataOperationBusy()) {
            window->cancelFilesystemRead(); if (window->secondPanel) window->secondPanel->cancelFilesystemRead(); window->updateState();
        }
        received = true; timer.start();
    }
}
void OpenWithController::flush() {
    if (pending.isEmpty()) return;
    if (menu || window->operationBusy() || QApplication::activeModalWidget()) { timer.start(250); return; }
    const auto paths = pending; pending.clear(); menu = new OpenWithMenu(paths, window); menu->setAttribute(Qt::WA_DeleteOnClose);
    connect(menu, &OpenWithMenu::commandChosen, window, &MainWindow::executeOpenWith);
    connect(menu, &QDialog::finished, this, [this] { menu = nullptr; if (!pending.isEmpty()) timer.start(); checkIdleExit(); });
    menu->show(); menu->raise(); menu->activateWindow();
}

void MainWindow::executeOpenWith(QString command, QStringList paths) {
    if (dataOperationBusy() || paths.isEmpty()) return;
    for (auto &path : paths) path = QFileInfo(path).absoluteFilePath(); paths.removeDuplicates();
    for (const auto &path : paths) if (!QFileInfo::exists(path)) { QMessageBox::warning(this, "7-Zip", "Cannot open: " + path); return; }
    const bool openAs = command.startsWith("openAs:");
    const auto readMode = openAs ? command.mid(7) : QString();
    if (openAs && (paths.size() != 1 || !QFileInfo(paths.first()).isFile() || !ArchiveFormats::openTypes().contains(readMode))) { QMessageBox::warning(this, "7-Zip", "Select one file and a supported Open archive type."); return; }
    if (command == "manager" || command == "open" || openAs) {
        show(); raise(); activateWindow();
        if (command == "open" || openAs) openPath(paths.first(), readMode, true);
        else if (paths.size() == 1 && openWithArchiveCandidate(paths.first())) openPath(paths.first());
        else { showFilesystem(QFileInfo(paths.first()).isDir() && paths.size() == 1 ? paths.first() : QFileInfo(paths.first()).absolutePath(), [this, paths] { for (int n = 0; n < files->topLevelItemCount(); ++n) if (paths.contains(files->topLevelItem(n)->data(0, Qt::UserRole).toString())) files->topLevelItem(n)->setSelected(true); }); }
        return;
    }
    cancelFilesystemRead();
    const auto base = openWithCommonParent(paths); const auto outputParent = QFileInfo(paths.first()).absolutePath();
    if (command == "add" || command == "7z" || command == "zip") {
        const auto format = command == "zip" ? "zip" : "7z"; AddDialog dialog(outputParent + '/' + openWithArchiveName(paths) + '.' + format, this);
        if (command != "add") dialog.findChild<QComboBox *>("archiveFormat")->setCurrentText(format);
        if (command == "add" && dialog.exec() != QDialog::Accepted) return;
        auto r = dialog.options(); r.workingDirectory = base; if (QFileInfo(r.archive).isRelative()) r.archive = QDir(outputParent).absoluteFilePath(r.archive);
        if (command != "add") { r.updateMode = "add"; r.pathMode = "relative"; r.deleteAfter = false; r.volume.clear(); r.password.clear(); r.encryptNames = false; }
        for (const auto &path : paths) { if (QFileInfo(path).absoluteFilePath() == QFileInfo(r.archive).absoluteFilePath()) { QMessageBox::warning(this, "7-Zip", "The output archive must differ from every input file."); return; } r.files << QDir(base).relativeFilePath(path); }
        if (command != "add" && QFileInfo::exists(r.archive) && QMessageBox::question(this, "7-Zip", "Update existing archive?\n" + r.archive, QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
        run(r); return;
    }
    if (command.startsWith("hash:") || command == "hashFile") {
        ArchiveRequest r; r.operation = command == "hashFile" ? ArchiveOperation::HashFile : ArchiveOperation::Hash; r.workingDirectory = base;
        r.archive = command == "hashFile" ? base + '/' + (paths.size() == 1 ? QFileInfo(paths.first()).fileName() : openWithArchiveName(paths)) + ".sha256" : paths.first();
        r.hashMethod = command.mid(5); for (const auto &path : paths) r.files << QDir(base).relativeFilePath(path); run(r); return;
    }
    if (!QStringList{"extract", "here", "to", "test", "hashTest"}.contains(command)) return;
    ArchiveRequest common;
    if (command == "extract") { ExtractDialog dialog(outputParent + '/' + openWithExtractName(paths.first()), false, this); if (dialog.exec() != QDialog::Accepted) return; common = dialog.options(); }
    openWithBatch.clear();
    for (const auto &path : paths) {
        if (!QFileInfo(path).isFile()) { QMessageBox::warning(this, "7-Zip", "Select archive files for this command: " + path); openWithBatch.clear(); return; }
        auto r = common; r.archive = path;
        if (command == "test" || command == "hashTest") { r.operation = ArchiveOperation::Test; r.hashTest = command == "hashTest"; }
        else { r.operation = ArchiveOperation::Extract; if (command != "extract") { r.outputDirectory = QFileInfo(path).absolutePath() + (command == "to" ? '/' + openWithExtractName(path) : QString()); r.pathMode = "full"; r.overwriteMode = "ask"; r.eliminateRoot = command == "to" && settings.eliminateRoot; } }
        openWithBatch << r;
    }
    auto first = openWithBatch.takeFirst(); run(first);
}
