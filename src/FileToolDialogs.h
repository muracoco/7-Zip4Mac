// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "FileOperations.h"
#include "CopyDialog.h"
#include <QDialog>
class QComboBox;
class QLineEdit;
class QButtonGroup;
class SplitDialog : public QDialog {
    Q_OBJECT
public:
    SplitDialog(QString source, QString destination, QWidget *parent = nullptr);
    QString destination() const;
    QList<quint64> volumeSizes() const;
    void accept() override;
private:
    QString source;
    QComboBox *path, *sizes;
};
class CombineDialog : public CopyDialog {
    Q_OBJECT
public:
    CombineDialog(QString firstVolume, QString destination, QWidget *parent = nullptr);
    QString destination() const;
};
class LinkDialog : public QDialog {
    Q_OBJECT
public:
    LinkDialog(QString source, QString directory, QWidget *parent = nullptr, QString currentDirectory = {});
    void accept() override;
private:
    QString base;
    QLineEdit *from, *to;
    QButtonGroup *types;
    SymbolicLinkSnapshot original;
};
