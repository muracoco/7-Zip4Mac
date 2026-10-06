// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStyledItemDelegate>
#include <memory>

class FileList;
class IconFileList;
class QTreeWidgetItem;
constexpr int PanelMarkRole = Qt::UserRole + 120;
class PanelSelection : public QObject {
public:
    PanelSelection(FileList *files, IconFileList *icons);
    ~PanelSelection() override;
    void selectAll(bool selected);
    void invert();
    void selectMask(const QString &mask, bool selected);
    QList<QTreeWidgetItem *> operatedItems() const;
    QList<QTreeWidgetItem *> markedItems() const;
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    struct State;
    std::unique_ptr<State> state;
    bool dispatching = false;
    QList<QTreeWidgetItem *> items(bool operated) const;
};
class PanelSelectionDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
protected:
    void initStyleOption(QStyleOptionViewItem *, const QModelIndex &) const override;
};
