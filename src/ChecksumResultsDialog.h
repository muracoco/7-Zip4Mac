// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ProgressPresentation.h"
#include <QDialog>
class QTreeWidget;
class ChecksumResultsDialog : public QDialog {
    Q_OBJECT
public:
    explicit ChecksumResultsDialog(const ChecksumPresentation &view, QWidget *parent = nullptr, bool folderHistory = false);
    QString copyText() const;
    QStringList strings() const;
    QString focusedString() const;
    bool stringsWereChanged() const { return changed; }
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    QTreeWidget *tree;
    bool history = false, changed = false;
    void itemInfo();
    void deleteItems();
};
