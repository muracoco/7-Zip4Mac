// Copyright (C) 2026 7-Zip Mac Port contributors
// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QSettings>

struct FileManagerSettings {
    bool showDots = false, realIcons = false, fullRow = false, grid = false;
    bool singleClick = false, alternativeSelection = false;
    int workMode = 0, memoryLimitGB = 0;
    bool removableOnly = true;
    bool eliminateRoot = true;
    QString workPath, viewer, editor, diff, language = "en";
    static FileManagerSettings load();
    bool save() const;
    QString workingFolder(const QString &sourceFolder) const;
};

class QCheckBox;
class QRadioButton;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QPushButton;
class QTreeWidget;
class OptionsDialog : public QDialog {
    Q_OBJECT
public:
    explicit OptionsDialog(QWidget *parent = nullptr);
signals:
    void settingsApplied();
private:
    bool apply();
    QCheckBox *dots, *icons, *fullRow, *grid, *singleClick, *alternative;
    QCheckBox *removable, *limitEnabled;
    QCheckBox *eliminateRoot;
    QCheckBox *openWithIcons;
    QTreeWidget *openWithItems;
    QComboBox *language;
    QRadioButton *workSystem, *workCurrent, *workSpecified;
    QLineEdit *workPath, *viewer, *editor, *diff;
    QSpinBox *limit;
    QPushButton *applyButton;
};
