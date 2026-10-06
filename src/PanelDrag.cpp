// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelDrag.h"
#include "MainWindow.h"
#include "UiLanguage.h"
#include "ArchiveFormats.h"
#include <QApplication>
#include <QDrag>
#include <QDropEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <sys/stat.h>

namespace {
using DWORD = unsigned;
using UInt32 = unsigned;
struct POINTL {};
constexpr DWORD MK_CONTROL = 8, MK_SHIFT = 4, MK_ALT = 32;
constexpr DWORD DROPEFFECT_COPY = 1, DROPEFFECT_MOVE = 2, DROPEFFECT_LINK = 4;
struct CDropTarget {
    bool m_DropIsAllowed = true, m_PanelDropIsAllowed = true, m_TargetPath_WasSent_ToDataObject = true;
    bool sameDrive;
    bool IsFsFolderPath() const { return true; }
    bool IsItSameDrive() const { return sameDrive; }
    DWORD GetEffect(DWORD, POINTL, DWORD) const;
};
#include "upstream/PanelDrag.inc"
constexpr int PathRole = Qt::UserRole, DirRole = Qt::UserRole + 1, ParentRole = Qt::UserRole + 2;
QStringList paths(const QMimeData *mime) {
    QStringList result;
    for (const auto &url : mime->urls()) if (url.isLocalFile() && !url.toLocalFile().isEmpty()) result << url.toLocalFile();
    return result;
}
bool sameDrive(const QString &source, QString destination) {
    struct stat a{}, b{};
    while (!QFileInfo::exists(destination) && !destination.isEmpty() && destination != "/") destination = QFileInfo(destination).absolutePath();
    return ::lstat(QFile::encodeName(source).constData(), &a) == 0 && ::stat(QFile::encodeName(destination).constData(), &b) == 0 && a.st_dev == b.st_dev;
}
}

Qt::DropAction officialPanelDropEffect(Qt::KeyboardModifiers keys, Qt::DropActions allowed, bool same) {
    DWORD keyState = 0, effects = 0;
    if (keys.testFlag(Qt::ControlModifier)) keyState |= MK_CONTROL;
    if (keys.testFlag(Qt::ShiftModifier)) keyState |= MK_SHIFT;
    if (keys.testFlag(Qt::AltModifier)) keyState |= MK_ALT;
    if (allowed.testFlag(Qt::CopyAction)) effects |= DROPEFFECT_COPY;
    if (allowed.testFlag(Qt::MoveAction)) effects |= DROPEFFECT_MOVE;
    if (allowed.testFlag(Qt::LinkAction)) effects |= DROPEFFECT_LINK;
    const auto value = CDropTarget{true, true, true, same}.GetEffect(keyState, {}, effects);
    return value == DROPEFFECT_COPY ? Qt::CopyAction : value == DROPEFFECT_MOVE ? Qt::MoveAction : Qt::IgnoreAction;
}

PanelDragMimeData::PanelDragMimeData(MainWindow *origin, const QList<QUrl> &urls, bool isArchive) : source(origin), archive(isArchive) {
    setUrls(urls); setData("application/x-sevenzip-mac-port-origin", "1");
}
PanelDragController::PanelDragController(MainWindow *manager) : QObject(manager), window(manager) {
    setObjectName("panelDragController");
    window->files->viewport()->installEventFilter(this); window->icons->viewport()->installEventFilter(this);
    connect(window->files, &FileList::filesystemDragRequested, this, [this] { start(window->selectedPaths()); });
    connect(window->icons, &IconFileList::filesystemDragRequested, this, [this] { start(window->selectedPaths()); });
    connect(&window->fileOps, &FileOperations::finished, this, [this] {
        if (transferSource) { auto source = transferSource; transferSource.clear(); source->refresh(); }
    });
}
QString PanelDragController::target(QAbstractItemView *view, QPoint position) const {
    if (!window->archivePath.isEmpty()) return window->archiveLocation();
    const auto index = view->indexAt(position).siblingAtColumn(0);
    if (index.isValid() && index.data(DirRole).toBool() && !index.data(ParentRole).toBool()) return index.data(PathRole).toString();
    return window->fsPath;
}
Qt::DropAction PanelDragController::effect(QAbstractItemView *view, QDropEvent *event) const {
    const auto names = paths(event->mimeData());
    if (window->operationBusy() || names.isEmpty()) return Qt::IgnoreAction;
    const auto origin = qobject_cast<const PanelDragMimeData *>(event->mimeData());
    if (origin && origin->source == window) return Qt::IgnoreAction;
    if (!window->archivePath.isEmpty() && !window->canUpdateArchive()) return Qt::IgnoreAction;
    const auto action = officialPanelDropEffect(event->modifiers(), event->possibleActions(), sameDrive(names.first(), target(view, event->position().toPoint())));
    if (action == Qt::IgnoreAction) return action;
    // Ordinary external input creates a new archive. Archive targets and
    // archive sources use Copy by default; archive-source Move is unsupported
    // by the original Agent. Explicit right-drag choices are handled at Drop.
    if (!origin || origin->archive || !window->archivePath.isEmpty()) return Qt::CopyAction;
    return action;
}
bool PanelDragController::eventFilter(QObject *object, QEvent *event) {
    auto view = qobject_cast<QAbstractItemView *>(object->parent()); if (!view) return false;
    if (event->type() == QEvent::MouseButtonPress) {
        const auto mouse = static_cast<QMouseEvent *>(event);
        rightPressed = mouse->button() == Qt::RightButton; rightStarted = false; suppressContext = false; pressPosition = mouse->position().toPoint();
    } else if (event->type() == QEvent::MouseMove) {
        const auto mouse = static_cast<QMouseEvent *>(event);
        if (rightPressed && !rightStarted && mouse->buttons().testFlag(Qt::RightButton) && !window->operationBusy() &&
            (mouse->position().toPoint() - pressPosition).manhattanLength() >= QApplication::startDragDistance()) {
            rightStarted = true; suppressContext = true;
            if (window->files->archiveView) window->files->archiveDragRequested(); else start(window->selectedPaths());
            event->accept(); return true;
        }
    } else if (event->type() == QEvent::ContextMenu && suppressContext) { event->accept(); return true; }
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
        auto drop = static_cast<QDropEvent *>(event); const auto action = effect(view, drop);
        if (action == Qt::IgnoreAction) drop->ignore(); else { drop->setDropAction(action); drop->accept(); }
        return true;
    }
    if (event->type() == QEvent::Drop) { drop(view, static_cast<QDropEvent *>(event)); return true; }
    return false;
}
void PanelDragController::drop(QAbstractItemView *view, QDropEvent *event) {
    auto action = effect(view, event); if (action == Qt::IgnoreAction) { event->ignore(); return; }
    const auto names = paths(event->mimeData()); const auto origin = qobject_cast<const PanelDragMimeData *>(event->mimeData());
    const auto destination = target(view, event->position().toPoint());
    bool add = !origin && window->archivePath.isEmpty();
    const bool rightButton = event->buttons().testFlag(Qt::RightButton) || (origin && origin->rightButton);
    if (rightButton) {
        QMenu menu(window); menu.setObjectName("dragContextMenu");
        QAction *cancel = nullptr;
        for (const auto &pair : NDragMenu::g_Pairs) {
            const auto command = pair.CmdId_and_Flags & NDragMenu::k_MenuFlags_CmdMask;
            const bool filesystem = window->archivePath.isEmpty();
            if ((command == NDragMenu::k_Copy_Base || command == NDragMenu::k_AddToArc) && !filesystem) continue;
            if (command == NDragMenu::k_Copy_ToArc && filesystem) continue;
            const auto effect = pair.CmdId_and_Flags & NDragMenu::k_MenuFlag_Move ? Qt::MoveAction : Qt::CopyAction;
            QString label = UiLanguage::resource(pair.LangId);
            if (command == NDragMenu::k_Copy_ToArc) label += ' ' + UiLanguage::resource(2321);
            if (command == NDragMenu::k_Cancel) menu.addSeparator();
            auto item = menu.addAction(label);
            item->setObjectName(command == NDragMenu::k_Cancel ? "dragCancel" : command == NDragMenu::k_AddToArc ? "dragAdd" : effect == Qt::MoveAction ? "dragMove" : "dragCopy");
            if (command == NDragMenu::k_Cancel) cancel = item;
            else {
                if (effect == Qt::MoveAction && origin && origin->archive) item->setEnabled(false);
                connect(item, &QAction::triggered, &menu, [&add, &action, command, effect] { add = command == NDragMenu::k_AddToArc; action = effect; });
            }
        }
        const auto selected = menu.exec(view->viewport()->mapToGlobal(event->position().toPoint()));
        if (!selected || selected == cancel) { event->ignore(); return; }
    }
    if (!window->archivePath.isEmpty()) {
        // Match PanelDrag.cpp's explicit Copy_ToArc confirmation.
        QMessageBox confirmation(QMessageBox::Question, UiLanguage::resource(6010),
            UiLanguage::resource(action == Qt::MoveAction ? 6003 : 6002) + '\n' + destination + '\n' + UiLanguage::resource(6011) + " ?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, window);
        confirmation.setObjectName("archiveDropConfirmation"); confirmation.setTextFormat(Qt::PlainText); confirmation.setDefaultButton(QMessageBox::Yes);
        if (confirmation.exec() != QMessageBox::Yes) { event->ignore(); return; }
        window->archiveTransferSource = origin ? origin->source : nullptr;
        window->copyIntoArchive(names, action == Qt::MoveAction);
    } else if (add) {
        // Preserve the explicitly requested Finder archive-drop shortcut.
        if (!rightButton && names.size() == 1 && ArchiveFormats::recognizes(names.first())) window->openPath(names.first());
        else window->add(names, destination);
    } else {
        transferSource = origin ? origin->source : nullptr;
        window->fileProgress(action == Qt::MoveAction ? "Move" : "Copy", destination);
        window->fileOps.transfer(names, destination + '/', action == Qt::MoveAction); window->updateState();
    }
    event->setDropAction(action); event->accept();
}
void PanelDragController::start(QStringList names, std::shared_ptr<QTemporaryDir> temporary) {
    if (window->operationBusy() || names.isEmpty()) return;
    QList<QUrl> urls;
    for (const auto &name : names) if (QFileInfo::exists(name) || QFileInfo(name).isSymLink()) urls << QUrl::fromLocalFile(name);
    if (urls.isEmpty()) return;
    QDrag drag(window->files); auto mime = new PanelDragMimeData(window, urls, bool(temporary)); mime->rightButton = rightPressed; drag.setMimeData(mime);
    const auto accepted = executor ? executor(&drag) : drag.exec(temporary ? Qt::CopyAction : Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    rightPressed = false;
    if (temporary && accepted != Qt::IgnoreAction) {
        // Finder/file-URL consumers may read after the native drag returns.
        // Keep accepted files after Manager exit. Default temp roots appear in
        // Tools > Delete Temporary Files; custom working-folder roots can be
        // removed later through the File Manager/Finder.
        temporary->setAutoRemove(false); window->retainedTemps.append(temporary);
    }
}
