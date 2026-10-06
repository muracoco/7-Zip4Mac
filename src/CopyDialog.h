// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDialog>
#include <QComboBox>
#include <optional>
struct CopySummaryItem { QString name; bool directory = false; std::optional<quint64> size; };
QString copyItemsInfo(const QList<CopySummaryItem> &, const QString &folder);
QString combineItemsInfo(const QStringList &volumes, quint64 size, const QString &folder);
class CopyDialog : public QDialog {
    Q_OBJECT
public:
    CopyDialog(QString title, QString label, QString value, QString info, QStringList history, QWidget *parent = nullptr, QString base = {});
    QString textValue() const { return path->currentText(); }
    void setTextValue(const QString &value) { path->setEditText(value); }
    QString destination() const { return textValue(); }
    static void remember(QString normalizedDestination);
protected:
    QComboBox *path;
};
