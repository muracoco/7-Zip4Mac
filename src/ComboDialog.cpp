// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ComboDialog.h"
#include "ResourceDialogs.h"
#include "UiLanguage.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

ComboDialog::ComboDialog(QString title, QString labelText, QString value, QWidget *parent, QStringList choices) : QDialog(parent), combo(new QComboBox(this)) {
    setObjectName("comboDialog"); setWindowTitle(title);
    auto label = new QLabel(labelText, this); label->setObjectName("comboLabel");
    combo->setObjectName("comboValue"); combo->setEditable(true); combo->setInsertPolicy(QComboBox::NoInsert); combo->addItems(choices); combo->setEditText(value); label->setBuddy(combo);
    auto ok = new QPushButton(this), cancel = new QPushButton(this);
    ok->setObjectName("comboOk"); cancel->setObjectName("comboCancel"); UiLanguage::bind(ok, 401); UiLanguage::bind(cancel, 402);
    connect(ok, &QPushButton::clicked, this, &QDialog::accept); connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto layout = new ResourceDialogLayout(this, 98); layout->bind("IDT_COMBO", label); layout->bind("IDC_COMBO", combo); layout->bind("IDOK", ok); layout->bind("IDCANCEL", cancel); layout->install();
    ok->setDefault(true); combo->setFocus(); combo->lineEdit()->selectAll(); UiLanguage::apply(this);
}
QString ComboDialog::getText(QWidget *parent, unsigned titleId, unsigned labelId, QString value, bool *ok) {
    ComboDialog dialog(UiLanguage::resource(titleId), UiLanguage::resource(labelId), value, parent);
    UiLanguage::bindTitle(&dialog, titleId); UiLanguage::bind(dialog.findChild<QLabel *>("comboLabel"), labelId); UiLanguage::apply(&dialog);
    const bool accepted = dialog.exec() == QDialog::Accepted; if (ok) *ok = accepted; return accepted ? dialog.textValue() : QString();
}
