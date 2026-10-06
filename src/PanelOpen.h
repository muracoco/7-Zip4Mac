// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QList>
class QTreeWidgetItem;
class FileList;
struct PanelOpenAction {
    enum Kind { BrowseFolder, ExternalFolder, OpenFile } kind;
    QTreeWidgetItem *item;
    bool tryInternal = false;
};
struct PanelOpenPlan {
    QList<PanelOpenAction> actions;
    bool tooManyItems = false;
};
// The retained official policy runs synchronously against a stable list. The
// caller snapshots its actions before starting asynchronous extraction.
PanelOpenPlan panelOpenPlan(FileList *files, bool tryInternal);
