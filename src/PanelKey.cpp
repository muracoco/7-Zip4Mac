// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelKey.h"
#include "PanelSort.h"

namespace {
constexpr int VK_F1 = Qt::Key_F1, VK_F2 = Qt::Key_F2, VK_F3 = Qt::Key_F3,
    VK_F4 = Qt::Key_F4, VK_F5 = Qt::Key_F5, VK_F6 = Qt::Key_F6, VK_F7 = Qt::Key_F7,
    VK_F9 = Qt::Key_F9, VK_F12 = Qt::Key_F12;
constexpr int VK_UP = Qt::Key_Up, VK_DOWN = Qt::Key_Down,
    VK_RIGHT = Qt::Key_Right, VK_LEFT = Qt::Key_Left, VK_DELETE = Qt::Key_Delete,
    VK_INSERT = Qt::Key_Insert, VK_BACK = Qt::Key_Backspace, VK_TAB = Qt::Key_Tab,
    VK_NEXT = Qt::Key_PageDown, VK_ADD = Qt::Key_Plus, VK_SUBTRACT = Qt::Key_Minus,
    VK_MULTIPLY = Qt::Key_Asterisk, VK_MENU = 1, VK_CONTROL = 2, VK_SHIFT = 3,
    VK_RCONTROL = 4, WM_COMMAND = 5, IDCLOSE = 6, g_HWND = 7;
using WORD = unsigned;
using PROPID = unsigned;
using LRESULT = int;
using namespace OfficialSort;
struct KeyDown { unsigned wVKey; struct { int hwndFrom = 0; } hdr; };
using LPNMLVKEYDOWN = const KeyDown *;
struct CPanel;
thread_local CPanel *dispatchPanel = nullptr;
struct Application { void SwitchOnOffOnePanel(); } g_App;
void PostMessage(int, int, int, int);
struct CPanel {
    PanelKeyPlan plan;
    Qt::KeyboardModifiers modifiers;
    bool rightControl = false;
    CPanel *_panelCallback = this;
    // Selection keeps the already imported PanelSelection callback bodies.
    // False leaves plain Insert to that adapter instead of running it twice.
    bool _mySelectMode = false, _selectionIsDefined = false;
    int _prevFocusedItem = 0;
    struct ListView { operator int() const { return 0; } int GetFocusedItem() const { return 0; } } _listView;
    bool IsKeyDown(int key) const {
        return key == VK_RCONTROL ? rightControl : modifiers.testFlag(key == VK_MENU ? Qt::AltModifier : key == VK_CONTROL ? Qt::ControlModifier : Qt::ShiftModifier);
    }
    void RenameFile() { plan.command = PanelKeyCommand::Rename; }
    void EditItem(bool editor) { plan.command = editor ? PanelKeyCommand::Edit : PanelKeyCommand::View; }
    void CreateFile() { plan.command = PanelKeyCommand::NewFile; }
    void CreateFolder() { plan.command = PanelKeyCommand::NewFolder; }
    void SetFocusToPath(int panel) { plan.command = PanelKeyCommand::FocusPath; plan.panel = panel; }
    void OnCopy(bool move, bool focused) { plan.command = move ? PanelKeyCommand::Move : PanelKeyCommand::Copy; plan.focusedOnly = focused; }
    void OnSetSameFolder() { plan.command = PanelKeyCommand::OtherSameFolder; }
    void OnSetSubFolder() { plan.command = PanelKeyCommand::OtherSubFolder; }
    void OnTab() { plan.command = PanelKeyCommand::SwitchPanel; }
    void SetBookmark(unsigned index) { plan.command = PanelKeyCommand::StoreBookmark; plan.value = index; }
    void OpenBookmark(unsigned index) { plan.command = PanelKeyCommand::Bookmark; plan.value = index; }
    void SortItemsWithPropID(unsigned property) { plan.command = PanelKeyCommand::Sort; plan.value = property; }
    void DeleteItems(bool trash) { plan.command = PanelKeyCommand::Delete; plan.focusedOnly = trash; }
    void EditCopy() { plan.command = PanelKeyCommand::ClipboardCopy; }
    void EditCut() {} // The original PanelMenu bodies are intentionally empty.
    void EditPaste() {}
    void OnInsert() {} // Selection adapter handles this independently.
    void SelectAll(bool select) { plan.command = PanelKeyCommand::SelectAll; plan.focusedOnly = select; }
    void SelectByType(bool select) { plan.command = PanelKeyCommand::SelectType; plan.focusedOnly = select; }
    void SelectSpec(bool select) { plan.command = PanelKeyCommand::SelectMask; plan.focusedOnly = select; }
    void InvertSelection() { plan.command = PanelKeyCommand::Invert; }
    void OpenParentFolder() { plan.command = PanelKeyCommand::Parent; }
    void OnReload() { plan.command = PanelKeyCommand::Refresh; }
    void ChangeComment() { plan.command = PanelKeyCommand::Comment; }
    void SetListViewMode(unsigned mode) { plan.command = PanelKeyCommand::ViewMode; plan.value = mode; }
    void FoldersHistory() { plan.command = PanelKeyCommand::History; }
    void OnArrowWithShift() {} // PanelSelection already imports this body.
    bool OnKeyDown(LPNMLVKEYDOWN keyDownInfo, LRESULT &result);
};
void Application::SwitchOnOffOnePanel() { dispatchPanel->plan.command = PanelKeyCommand::TogglePanels; }
void PostMessage(int, int, int, int) { dispatchPanel->plan.command = PanelKeyCommand::Close; }
#define Z7_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#include "upstream/PanelKeyCommands.inc"
#undef Z7_ARRAY_SIZE
struct UString {
    QString value;
    int ReverseFind_PathSepar() const { return value.lastIndexOf('/'); }
    UString Ptr(unsigned index) const { return {value.mid(index)}; }
    bool IsEqualTo(const char *other) const { return value == QLatin1String(other); }
};
#include "upstream/RenameName.inc"
}
PanelKeyPlan officialPanelKey(int key, Qt::KeyboardModifiers modifiers, bool rightControl) {
    if (modifiers.testFlag(Qt::MetaModifier)) return {};
    // Qt reports Shift+Tab as Backtab; the original callback receives Tab.
    CPanel panel; panel.modifiers = modifiers; panel.rightControl = rightControl;
    const KeyDown event{unsigned(key == Qt::Key_Backtab ? Qt::Key_Tab : key), {}};
    dispatchPanel = &panel; LRESULT result = 0;
    panel.plan.consumed = panel.OnKeyDown(&event, result); dispatchPanel = nullptr;
    return panel.plan;
}
bool officialRenameName(const QString &name) {
    UString corrected;
    return !name.isEmpty() && !name.contains(QChar::Null) && IsCorrectFsName({name}) && CorrectFsPath({}, {name}, corrected);
}
