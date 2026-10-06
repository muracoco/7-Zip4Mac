// SPDX-License-Identifier: LGPL-3.0-or-later
#include "PropertiesDialog.h"
#include "UiLanguage.h"
#include "OptionsDialog.h"
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

PropertiesDialog::PropertiesDialog(const ArchivePropertyList &rows, QWidget *parent) : QDialog(parent), list(new QTreeWidget(this)) {
    setObjectName("propertiesDialog"); setWindowTitle(UiLanguage::text("Properties")); resize(620, 430); setWindowModality(Qt::ApplicationModal);
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(8, 8, 8, 8); layout->addWidget(list);
    list->setObjectName("propertyList"); list->setColumnCount(2); list->setHeaderLabels({{}, {}}); list->setRootIsDecorated(false); list->setUniformRowHeights(true); list->setSortingEnabled(false);
    list->setSelectionMode(QAbstractItemView::ExtendedSelection); list->setSelectionBehavior(QAbstractItemView::SelectRows); list->setEditTriggers(QAbstractItemView::NoEditTriggers); list->setAllColumnsShowFocus(true);
    // Labels are localized explicitly; values, including "Cancel", stay literal.
    list->setProperty("uiLiteral", true);
    for (const auto &property : rows) {
        auto name = property.native ? UiLanguage::propertyName(property.id, property.name) : UiLanguage::text(property.name); auto value = property.value;
        if (property.name.isEmpty() && value.endsWith(" object(s) selected")) value = UiLanguage::text("{0} object(s) selected").replace("{0}", value.section(' ', 0, 0));
        auto display = value.size() > 1024 ? value.left(1024) + " ..." : value; display.replace("\r\n", " "); display.replace('\n', ' ');
        auto row = new QTreeWidgetItem(list, {name, display}); row->setData(0, Qt::UserRole, name); row->setData(1, Qt::UserRole, value);
        row->setTextAlignment(0, Qt::AlignLeft | Qt::AlignVCenter); row->setTextAlignment(1, Qt::AlignLeft | Qt::AlignVCenter);
    }
    list->resizeColumnToContents(0); list->resizeColumnToContents(1); list->header()->setStretchLastSection(false);
    list->installEventFilter(this); list->viewport()->installEventFilter(this); connect(list, &QTreeWidget::itemDoubleClicked, this, [this] { showValue(); });
    if (FileManagerSettings::load().singleClick) connect(list, &QTreeWidget::itemClicked, this, [this] { if (QApplication::keyboardModifiers() == Qt::NoModifier) showValue(); });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this); buttons->setObjectName("propertiesButtons"); layout->addWidget(buttons); buttons->button(QDialogButtonBox::Ok)->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept); connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject); UiLanguage::apply(this);
    list->clearSelection(); list->setCurrentItem(nullptr); list->setFocus();
}
QString PropertiesDialog::selectedText() const {
    QString result;
    for (int n = 0; n < list->topLevelItemCount(); ++n) { auto item = list->topLevelItem(n); if (item->isSelected()) result += item->data(0, Qt::UserRole).toString() + ": " + item->data(1, Qt::UserRole).toString() + "\r\n"; }
    return result;
}
void PropertiesDialog::copySelection() { QApplication::clipboard()->setText(selectedText()); }
void PropertiesDialog::showValue() {
    const auto selected = list->selectedItems(); if (selected.size() != 1) return;
    if (findChild<QDialog *>("propertyValueDialog")) return;
    auto viewer = new QDialog(this); viewer->setObjectName("propertyValueDialog"); viewer->setAttribute(Qt::WA_DeleteOnClose); viewer->setWindowModality(Qt::ApplicationModal); viewer->setWindowTitle(selected.first()->data(0, Qt::UserRole).toString()); viewer->resize(560, 360);
    auto layout = new QVBoxLayout(viewer); auto body = new QPlainTextEdit(viewer); body->setObjectName("propertyValueText"); body->setReadOnly(true); body->setPlainText(selected.first()->data(1, Qt::UserRole).toString()); layout->addWidget(body);
    auto close = new QPushButton(UiLanguage::text("Close"), viewer); close->setObjectName("propertyValueClose"); close->setDefault(true); auto row = new QHBoxLayout; row->addStretch(); row->addWidget(close); layout->addLayout(row); connect(close, &QPushButton::clicked, viewer, &QDialog::accept); viewer->show();
}
bool PropertiesDialog::eventFilter(QObject *object, QEvent *event) {
    if ((object == list || object == list->viewport()) && event->type() == QEvent::KeyPress) {
        auto key = static_cast<QKeyEvent *>(event);
        if (key->modifiers() == Qt::ControlModifier && key->key() == Qt::Key_A) { list->selectAll(); return true; }
        if (key->modifiers() == Qt::ControlModifier && (key->key() == Qt::Key_C || key->key() == Qt::Key_Insert)) { copySelection(); return true; }
        if (key->modifiers() == Qt::NoModifier && (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)) { showValue(); return true; }
        if (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) return true;
    }
    return QDialog::eventFilter(object, event);
}
