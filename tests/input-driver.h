// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "ComboDialog.h"
#include "CopyDialog.h"
#include <QApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QPointer>
struct TestInput {
    QPointer<QWidget> widget;
    explicit operator bool() const { return !widget.isNull(); }
    TestInput *operator->() { return this; }
    bool isVisible() const { return widget && widget->isVisible(); }
    QString textValue() const {
        if (auto input = qobject_cast<ComboDialog *>(widget)) return input->textValue();
        if (auto input = qobject_cast<CopyDialog *>(widget)) return input->textValue();
        if (auto input = qobject_cast<QInputDialog *>(widget)) return input->textValue();
        if (auto input = qobject_cast<QLineEdit *>(widget)) return input->text(); return {};
    }
    QLineEdit::EchoMode textEchoMode() const { if (auto input = qobject_cast<QInputDialog *>(widget)) return input->textEchoMode(); return QLineEdit::Normal; }
    void setTextValue(const QString &value) {
        if (auto input = qobject_cast<ComboDialog *>(widget)) input->setTextValue(value);
        else if (auto input = qobject_cast<CopyDialog *>(widget)) input->setTextValue(value);
        else if (auto input = qobject_cast<QInputDialog *>(widget)) input->setTextValue(value);
        else if (auto input = qobject_cast<QLineEdit *>(widget)) input->setText(value);
    }
    void finish(bool accepted) {
        if (auto dialog = qobject_cast<QDialog *>(widget)) { if (accepted) dialog->accept(); else dialog->reject(); }
        else if (widget) { QKeyEvent key(QEvent::KeyPress, accepted ? Qt::Key_Return : Qt::Key_Escape, Qt::NoModifier); QApplication::sendEvent(widget, &key); }
    }
    void accept() { finish(true); }
    void reject() { finish(false); }
};
inline QList<TestInput> testInputs(QObject *parent, const QString &name = {}) {
    QList<TestInput> result;
    for (auto input : parent->findChildren<ComboDialog *>(name)) result.append({input});
    for (auto input : parent->findChildren<CopyDialog *>(name)) result.append({input});
    for (auto input : parent->findChildren<QInputDialog *>(name)) result.append({input});
    if (name.isEmpty() || name == "renameEditor") for (auto input : parent->findChildren<QLineEdit *>("renameEditor")) result.append({input});
    return result;
}
inline TestInput testInput(QObject *parent, const QString &name = {}) { const auto inputs = testInputs(parent, name); for (auto input : inputs) if (input->isVisible()) return input; return inputs.isEmpty() ? TestInput{} : inputs.first(); }
