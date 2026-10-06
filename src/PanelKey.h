// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <Qt>
#include <QString>

enum class PanelKeyCommand {
    None, Rename, View, Edit, NewFile, Copy, Move, NewFolder, FocusPath,
    OtherSameFolder, OtherSubFolder, SwitchPanel, TogglePanels, Sort,
    Delete, ClipboardCopy, SelectAll, SelectMask, SelectType, Invert,
    Parent, Refresh, Close, Comment, ViewMode, History, StoreBookmark, Bookmark
};
struct PanelKeyPlan {
    PanelKeyCommand command = PanelKeyCommand::None;
    bool consumed = false, focusedOnly = false;
    int panel = 0;
    unsigned value = 0;
};
PanelKeyPlan officialPanelKey(int key, Qt::KeyboardModifiers modifiers, bool rightControl = false);
bool officialRenameName(const QString &name);
