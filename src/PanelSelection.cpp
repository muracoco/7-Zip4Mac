// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelSelection.h"
#include "MainWindow.h"
#include "PanelDisplay.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QSet>

namespace {
using UInt32 = quint32;
constexpr unsigned kParentIndex = unsigned(-1);
constexpr int ParentRole = Qt::UserRole + 2, LVKF_SHIFT = 1, LVKF_CONTROL = 2;
constexpr int RealIndexRole = Qt::UserRole + 121;
template<class T> struct CRecordVector {
    QList<T> data;
    void Clear() { data.clear(); }
    bool IsEmpty() const { return data.isEmpty(); }
    unsigned Size() const { return data.size(); }
    void Add(T value) { data.append(value); }
    const T *ConstData() const { return data.constData(); }
    T &operator[](unsigned index) { return data[index]; }
};
#define FOR_VECTOR(i, v) for (unsigned i = 0; i < (v).Size(); ++i)
template<class T> T MyMin(T a, T b) { return qMin(a, b); }
template<class T> T MyMax(T a, T b) { return qMax(a, b); }
using HWND = const void *;
struct MY_NMLISTVIEW_NMITEMACTIVATE { struct { HWND hwndFrom; } hdr; int iItem; unsigned uKeyFlags; };
struct ListAdapter {
    FileList *files;
    QAbstractItemView *active;
    operator HWND() const { return files; }
    int GetFocusedItem() const { return files->currentIndex().row(); }
    int GetItemCount() const { return files->topLevelItemCount(); }
    int GetSelectedCount() const { return files->QTreeWidget::selectedItems().size(); }
    bool IsItemSelected(int row) const { auto item = files->topLevelItem(row); return item && files->QTreeWidget::selectedItems().contains(item); }
    void SetItemState_Selected(int row, bool selected = true) { files->topLevelItem(row)->setSelected(selected); }
    void SetItemState_FocusedSelected(int row) { files->setCurrentItem(files->topLevelItem(row), 0, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows); }
    void EnsureVisible(int row, bool) { active->scrollTo(files->indexFromItem(files->topLevelItem(row))); }
    void RedrawItem(int) {}
    void RedrawAllItems() {}
};
class CPanel {
public:
    ListAdapter _listView;
    CRecordVector<bool> _selectedStatusVector;
    bool _mySelectMode = false, _selectionIsDefined = false, _selectMark = false, _enableItemChangeNotify = true;
    int _prevFocusedItem = -1, _startGroupSelect = 0;
    explicit CPanel(FileList *files) : _listView{files, files} {}
    unsigned GetRealItemIndex(int row) const { auto item = _listView.files->topLevelItem(row); return !item || item->data(0, ParentRole).toBool() ? kParentIndex : item->data(0, RealIndexRole).toUInt(); }
    void PostMsg(int) {} // Qt dispatches the post-navigation step after its key handler.
    void OnShiftSelectMessage(); void OnArrowWithShift(); void OnInsert(); void UpdateSelection();
    void SelectAll(bool); void InvertSelection(); void KillSelection(); void OnLeftClick(MY_NMLISTVIEW_NMITEMACTIVATE *);
    void Get_ItemIndices_Selected(CRecordVector<UInt32> &) const; void Get_ItemIndices_Operated(CRecordVector<UInt32> &) const;
};
constexpr bool g_CaseSensitive = false;
wchar_t MyCharUpper(wchar_t value) { return QChar(char16_t(value)).toUpper().unicode(); }
#include "upstream/WildcardMatch.inc"
std::wstring utf16Units(const QString &text) { std::wstring result; for (const auto c : text) result.push_back(wchar_t(c.unicode())); return result; }
constexpr int kShiftSelectMessage = 1;
#include "upstream/PanelSelection.inc"
}
struct PanelSelection::State {
    FileList *files; IconFileList *icons; CPanel panel; unsigned nextIndex = 0;
    State(FileList *f, IconFileList *i) : files(f), icons(i), panel(f) { assignIndices(); }
    void assignIndices(int first = 0, int last = -1) {
        if (last < 0) last = files->topLevelItemCount() - 1;
        for (int row = first; row <= last; ++row) { auto item = files->topLevelItem(row); if (!item->data(0, RealIndexRole).isValid()) item->setData(0, RealIndexRole, nextIndex++); else nextIndex = qMax(nextIndex, item->data(0, RealIndexRole).toUInt() + 1); }
    }
    void read(CPanel &target) const {
        target._mySelectMode = files->property("alternativeSelection").toBool();
        const auto selectedItems = files->QTreeWidget::selectedItems();
        const QSet<QTreeWidgetItem *> selected(selectedItems.cbegin(), selectedItems.cend());
        target._selectedStatusVector.Clear();
        for (unsigned index = 0; index < nextIndex; ++index) target._selectedStatusVector.Add(false);
        for (int row = 0; row < files->topLevelItemCount(); ++row) {
            const auto item = files->topLevelItem(row);
            target._selectedStatusVector[item->data(0, RealIndexRole).toUInt()] = !item->data(0, ParentRole).toBool() && (target._mySelectMode ? item->data(0, PanelMarkRole).toBool() : selected.contains(item));
        }
    }
    void read() { read(panel); }
    void write() {
        { QSignalBlocker block(files);
          for (int row = 0; row < files->topLevelItemCount(); ++row)
              files->setItemSelected(files->topLevelItem(row), panel._selectedStatusVector.data[files->topLevelItem(row)->data(0, RealIndexRole).toUInt()]); }
        files->viewport()->update(); icons->viewport()->update(); files->notifyMarksChanged();
    }
};
PanelSelection::PanelSelection(FileList *files, IconFileList *icons) : QObject(files), state(std::make_unique<State>(files, icons)) {
    files->selection = this; icons->fileList = files;
    for (auto view : {static_cast<QAbstractItemView *>(files), static_cast<QAbstractItemView *>(icons)}) { view->installEventFilter(this); view->viewport()->installEventFilter(this); }
    connect(files->model(), &QAbstractItemModel::modelReset, this, [this] { state->panel._selectionIsDefined = false; state->panel._startGroupSelect = 0; state->nextIndex = 0; state->assignIndices(); });
    connect(files->model(), &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex &parent, int first, int last) { if (!parent.isValid()) state->assignIndices(first, last); });
}
PanelSelection::~PanelSelection() = default;
void PanelSelection::selectAll(bool selected) { state->read(); { QSignalBlocker block(state->files); state->panel.SelectAll(selected); } state->write(); }
void PanelSelection::selectMask(const QString &mask, bool selected) {
    const auto pattern = utf16Units(mask); QSignalBlocker block(state->files);
    for (int row = 0; row < state->files->topLevelItemCount(); ++row) {
        auto item = state->files->topLevelItem(row); if (item->data(0, ParentRole).toBool()) continue;
        const auto name = utf16Units(panelItemName(item));
        if (EnhancedMaskTest(pattern.c_str(), name.c_str())) state->files->setItemSelected(item, selected);
    }
    state->files->viewport()->update(); state->icons->viewport()->update(); block.unblock(); state->files->notifyMarksChanged();
}
void PanelSelection::invert() { state->read(); { QSignalBlocker block(state->files); state->panel.InvertSelection(); } state->write(); }
QList<QTreeWidgetItem *> PanelSelection::operatedItems() const { return items(true); }
QList<QTreeWidgetItem *> PanelSelection::markedItems() const { return items(false); }
QList<QTreeWidgetItem *> PanelSelection::items(bool operated) const {
    CPanel panel(state->files); state->read(panel); CRecordVector<UInt32> indices;
    if (operated) panel.Get_ItemIndices_Operated(indices); else panel.Get_ItemIndices_Selected(indices);
    QList<QTreeWidgetItem *> byIndex(state->nextIndex, nullptr), result;
    for (int row = 0; row < state->files->topLevelItemCount(); ++row) { auto item = state->files->topLevelItem(row); byIndex[item->data(0, RealIndexRole).toUInt()] = item; }
    for (auto index : indices.data) if (byIndex[index]) result.append(byIndex[index]); return result;
}
bool PanelSelection::eventFilter(QObject *object, QEvent *event) {
    if (dispatching || !state->files->property("alternativeSelection").toBool()) return false;
    auto view = object == state->files || object == state->files->viewport() ? static_cast<QAbstractItemView *>(state->files) : static_cast<QAbstractItemView *>(state->icons);
    state->panel._listView.active = view;
    if (event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent *>(event); state->read();
        if (key->key() == Qt::Key_Shift) { state->panel._selectionIsDefined = false; state->panel._prevFocusedItem = state->panel._listView.GetFocusedItem(); }
        else if (key->key() == Qt::Key_Insert && key->modifiers() == Qt::NoModifier) { state->panel.OnInsert(); state->write(); event->accept(); return true; }
        else if (key->modifiers().testFlag(Qt::ShiftModifier) && !key->modifiers().testFlag(Qt::AltModifier) &&
                 (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down || key->key() == Qt::Key_Left || key->key() == Qt::Key_Right)) {
            state->panel.OnArrowWithShift();
            QKeyEvent movement(QEvent::KeyPress, key->key(), key->modifiers() & ~Qt::ShiftModifier, key->text(), key->isAutoRepeat(), key->count());
            { QScopedValueRollback<bool> guard(dispatching, true); QApplication::sendEvent(view, &movement); }
            state->panel.OnShiftSelectMessage(); state->write(); event->accept(); return true;
        }
    } else if (event->type() == QEvent::MouseButtonRelease) {
        const auto mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton) {
            const auto index = view->indexAt(mouse->position().toPoint());
            if (index.isValid()) { state->read(); MY_NMLISTVIEW_NMITEMACTIVATE click{{state->files}, index.row(), unsigned((mouse->modifiers().testFlag(Qt::ShiftModifier) ? LVKF_SHIFT : 0) | (mouse->modifiers().testFlag(Qt::ControlModifier) ? LVKF_CONTROL : 0))}; state->panel.OnLeftClick(&click); state->write(); }
        }
    }
    return false;
}
void PanelSelectionDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const {
    QStyledItemDelegate::initStyleOption(option, index);
    if (parent()->property("alternativeSelection").toBool() && index.siblingAtColumn(0).data(PanelMarkRole).toBool()) option->backgroundBrush = QColor(255, 192, 192);
}
