// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QComboBox>
class ComboDialog : public QDialog {
    Q_OBJECT
public:
    ComboDialog(QString title, QString label, QString value, QWidget *parent = nullptr, QStringList choices = {});
    QString textValue() const { return combo->currentText(); }
    void setTextValue(const QString &text) { combo->setEditText(text); }
    static QString getText(QWidget *parent, unsigned titleId, unsigned labelId, QString value, bool *ok);
private:
    QComboBox *combo;
};
