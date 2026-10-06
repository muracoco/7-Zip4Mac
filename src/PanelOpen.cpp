// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PanelOpen.h"
#include "MainWindow.h"
#include <limits>

namespace {
using UInt32 = unsigned;
constexpr unsigned kParentIndex = std::numeric_limits<unsigned>::max();
constexpr int IDS_TOO_MANY_ITEMS = 3016;
template<class T> struct CRecordVector : QList<T> {
    unsigned Size() const { return unsigned(this->size()); }
    void Insert(unsigned index, const T &value) { this->insert(index, value); }
};
struct ListView {
    FileList *files;
    int GetFocusedItem() const { return files->indexOfTopLevelItem(files->currentItem()); }
    bool IsItemSelected(int index) const { return files->topLevelItem(index)->isSelected(); }
};
struct CPanel {
    FileList *files;
    ListView _listView;
    PanelOpenPlan plan;
    QTreeWidgetItem *item(unsigned index) const {
        if (index != kParentIndex) return files->topLevelItem(int(index));
        for (int row = 0; row < files->topLevelItemCount(); ++row) if (files->topLevelItem(row)->data(0, Qt::UserRole + 2).toBool()) return files->topLevelItem(row);
        return nullptr;
    }
    void Get_ItemIndices_Operated(CRecordVector<UInt32> &indices) const {
        for (auto selected : files->operatedItems()) if (!selected->data(0, Qt::UserRole + 2).toBool()) indices.append(UInt32(files->indexOfTopLevelItem(selected)));
    }
    unsigned GetRealItemIndex(int row) const { return files->topLevelItem(row)->data(0, Qt::UserRole + 2).toBool() ? kParentIndex : unsigned(row); }
    bool IsItem_Folder(unsigned index) const { return index == kParentIndex || item(index)->data(0, Qt::UserRole + 1).toBool(); }
    void MessageBox_Error_LangID(int) { plan.tooManyItems = true; }
    void OpenFolder(unsigned index) { plan.actions.append({PanelOpenAction::BrowseFolder, item(index)}); }
    void OpenFolderExternal(unsigned index) { plan.actions.append({PanelOpenAction::ExternalFolder, item(index)}); }
    void OpenItem(unsigned index, bool tryInternal, bool) { plan.actions.append({PanelOpenAction::OpenFile, item(index), tryInternal}); }
    void OpenSelectedItems(bool tryInternal);
};
#define FOR_VECTOR(i, vector) for (int i = 0; i < (vector).size(); ++i)
#include "upstream/PanelOpen.inc"
#undef FOR_VECTOR
}

PanelOpenPlan panelOpenPlan(FileList *files, bool tryInternal) {
    CPanel panel{files, {files}, {}};
    panel.OpenSelectedItems(tryInternal);
    return panel.plan;
}
