// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ArchiveBackend.h"
#include <QDialog>
class QTreeWidget;

// FileManager/ListViewDialog: read-only pairs, original values kept separately
// from the 1024-character, single-line display used by the table.
class PropertiesDialog final : public QDialog {
    Q_OBJECT
public:
    explicit PropertiesDialog(const ArchivePropertyList &rows, QWidget *parent = nullptr);
    QTreeWidget *table() const { return list; }
    QString selectedText() const;
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    void copySelection();
    void showValue();
    QTreeWidget *list;
};
