// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelMenu.h"
#include <QAction>
#include <QHash>
#include <QMenu>
#include <QFileInfo>
#include <QFile>
#include <sys/stat.h>

namespace {
using HMENU = QMenu *;
struct UString { QString value; UString() = default; UString(const char *text) : value(QString::fromUtf8(text)) {} bool IsEmpty() const { return value.isEmpty(); } };
using UInt32 = quint32;
#define Z7_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
constexpr unsigned IDCLOSE = 8, MIIM_SUBMENU = 0, MIIM_STATE = 0, MIIM_ID = 0;
constexpr unsigned MFT_STRING = 0, MF_BYPOSITION = 0, MF_GRAYED = 0;
constexpr unsigned MF_STRING = 0;
namespace NFile::NFind {
struct CFileInfo {
    quint64 Size = 0;
    struct stat stamp{};
    bool Find(const QString &path) { if (::stat(QFile::encodeName(path).constData(), &stamp) != 0) return false; Size = quint64(stamp.st_size); return true; }
    bool IsDir() const { return S_ISDIR(stamp.st_mode); }
    bool IsReadOnly() const { return (stamp.st_mode & 0222) == 0; }
};
}
unsigned Get_fMask_for_FType_and_String() { return 0; }
struct CMenuItem {
    unsigned fMask = 0, fType = 0, wID = 0;
    QAction *action = nullptr;
    bool IsSeparator() const { return action->isSeparator(); }
};
QAction *copyAction(QAction *source, QMenu *destination) {
    if (source->isSeparator()) return destination->addSeparator();
    QAction *copy;
    if (source->menu()) {
        auto submenu = destination->addMenu(source->icon(), source->text());
        submenu->setObjectName(source->menu()->objectName());
        for (auto child : source->menu()->actions()) copyAction(child, submenu);
        copy = submenu->menuAction();
    } else {
        copy = destination->addAction(source->icon(), source->text());
        copy->setShortcut(source->shortcut()); copy->setShortcutContext(Qt::WidgetShortcut);
        QObject::connect(copy, &QAction::triggered, source, &QAction::trigger);
    }
    copy->setObjectName(source->objectName()); copy->setEnabled(source->isEnabled());
    copy->setCheckable(source->isCheckable()); copy->setChecked(source->isChecked());
    return copy;
}
struct CMenu {
    QMenu *menu = nullptr;
    QHash<QString, unsigned> ids;
    QHash<unsigned, QAction *> versionCommands;
    void Attach(QMenu *value) { menu = value; }
    bool GetItem(unsigned index, bool, CMenuItem &item) const {
        auto list = menu->actions(); for (auto action : menu->actions()) if (action->property("versionCommand").toBool()) list.removeOne(action);
        if (index >= unsigned(list.size())) return false;
        item.action = list[index]; item.wID = ids.value(item.action->objectName()); return true;
    }
    bool AppendItem(unsigned, unsigned id, const UString &label) {
        auto source = versionCommands.value(id);
        auto action = source ? copyAction(source, menu) : menu->addAction(label.value);
        action->setText(label.value); action->setData(id); action->setVisible(true);
        action->setProperty("versionCommand", true); return true;
    }
    bool InsertItem(unsigned index, bool, CMenuItem &item) {
        const auto before = menu->actions().value(index); auto copy = copyAction(item.action, menu);
        if (before) { menu->removeAction(copy); menu->insertAction(before, copy); }
        return true;
    }
    void EnableItem(unsigned index, unsigned) { menu->actions().at(index)->setEnabled(false); }
    void RemoveAllItemsFrom(unsigned index) {
        const auto list = menu->actions(); for (unsigned row = index; row < unsigned(list.size()); ++row) menu->removeAction(list[row]);
    }
};
struct CFileMenu {
    const FileMenuState &state;
    CMenu g_FileMenu;
    bool programMenu, readOnly, isHashFolder, isFsFolder, allAreFiles, isAltStreamsSupported;
    unsigned numItems;
    int g_HWND;
    QString FilePath;
    explicit CFileMenu(QMenu *source, const FileMenuState &value) : state(value), programMenu(value.programMenu), readOnly(value.readOnly), isHashFolder(value.hashFolder), isFsFolder(value.filesystem), allAreFiles(value.allFiles), isAltStreamsSupported(value.alternateStreams), numItems(value.count), g_HWND(value.largeScreen), FilePath(value.filePath) { g_FileMenu.Attach(source); }
    void ReadRegDiff(UString &path) const { path.value = state.diff; }
    void ReadReg_VerCtrlPath(UString &path) const { path.value = state.versionStore; }
    void Load(HMENU, unsigned);
};
namespace NControl { bool IsDialogSizeOK(int, int, int screen) { return screen != 0; } }
void CopyPopMenu_IfRequired(CMenuItem &) {} // copyAction creates owned Qt submenus.
#include "upstream/PanelMenu.inc"
#undef Z7_ARRAY_SIZE
const QStringList versionNames{"verEditAction", "verCommitAction", "verRevertAction", "verDiffAction"};
}

void appendOfficialFileMenu(QMenu *destination, QMenu *source, const FileMenuState &state) {
    CFileMenu menu(source, state);
    menu.g_FileMenu.ids = {{"exitAction", IDCLOSE}, {"diffAction", IDM_DIFF},
        {"openAction", IDM_OPEN}, {"insideAction", IDM_OPEN_INSIDE}, {"insideOneAction", IDM_OPEN_INSIDE_ONE}, {"insideParserAction", IDM_OPEN_INSIDE_PARSER},
        {"outsideAction", IDM_OPEN_OUTSIDE}, {"viewAction", IDM_FILE_VIEW}, {"editAction", IDM_FILE_EDIT},
        {"renameAction", IDM_RENAME}, {"copyAction", IDM_COPY_TO}, {"moveAction", IDM_MOVE_TO}, {"deleteAction", IDM_DELETE},
        {"splitAction", IDM_SPLIT}, {"combineAction", IDM_COMBINE}, {"commentAction", IDM_COMMENT},
        {"folderAction", IDM_CREATE_FOLDER}, {"newfileAction", IDM_CREATE_FILE}, {"linkAction", IDM_LINK}, {"alternateStreamsAction", IDM_ALT_STREAMS}};
    for (auto action : source->actions()) for (unsigned n = 0; n < 4; ++n) if (action->objectName() == versionNames[n]) menu.g_FileMenu.versionCommands.insert(g_Zvc_IDs[n], action);
    menu.Load(destination, unsigned(destination->actions().size()));
}
QStringList officialVersionMenuItems(const FileMenuState &state) {
    QMenu source, destination; CFileMenu menu(&source, state); menu.Load(&destination, 0);
    QStringList names; for (auto action : destination.actions()) for (unsigned n = 0; n < 4; ++n) if (action->data().toUInt() == g_Zvc_IDs[n]) names << versionNames[n]; return names;
}
