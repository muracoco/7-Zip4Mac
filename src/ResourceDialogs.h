// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QCheckBox>
#include <QObject>
#include <QMap>
#include <QPointer>
#include <QRect>
#include <QSize>
class QDialog;
class QDialogButtonBox;
struct ResourceDialog;
class ResourceCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &point) const override;
};
class ResourceDialogLayout : public QObject {
    Q_OBJECT
public:
    ResourceDialogLayout(QDialog *dialog, int resource);
    void bind(QString name, QWidget *widget);
    void bindButtons(QDialogButtonBox *box);
    void container(QWidget *widget, QStringList controls);
    void extra(QWidget *widget, QRect dialogUnits);
    void install();
    QRect controlRect(QString name) const;
    QSize dialogSize() const;
    QRect toPixels(QRect dialogUnits) const;
    void refresh();
    Q_INVOKABLE void refreshText();
protected:
    bool eventFilter(QObject *, QEvent *) override;
private:
    QDialog *dialog;
    const ResourceDialog *definition;
    QMap<int, QPointer<QWidget>> widgets;
    QMap<QWidget *, QRect> containers, extras;
    QPointer<QDialogButtonBox> buttons;
    qreal horizontal = 1, vertical = 1;
    bool installed = false, refreshing = false;
    void measure();
};
