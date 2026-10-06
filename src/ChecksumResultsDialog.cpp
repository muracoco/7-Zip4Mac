// SPDX-License-Identifier: LGPL-3.0-or-later
#include "ChecksumResultsDialog.h"
#include "ResourceDialogs.h"
#include "UiLanguage.h"
#include "OptionsDialog.h"
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

ChecksumResultsDialog::ChecksumResultsDialog(const ChecksumPresentation &view, QWidget *parent, bool folderHistory) : QDialog(parent), tree(new QTreeWidget(this)), history(folderHistory) {
    setObjectName(history ? "folderHistoryDialog" : "checksumResultsDialog"); setWindowTitle(view.title); setProperty("uiTitleId", history ? 6601 : 7501);
    tree->setObjectName(history ? "folderHistoryList" : "checksumResults"); tree->setColumnCount(int(view.columns)); tree->setHeaderLabels({"", ""}); tree->setHeaderHidden(view.columns == 1); tree->setRootIsDecorated(false);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection); tree->setAllColumnsShowFocus(true); tree->installEventFilter(this);
    for (const auto &row : view.rows) {
        auto value = row.second;
        if (value.size() > 1024) value = value.left(1024) + " ...";
        value.replace("\r\n", " "); value.replace('\n', ' ');
        auto item = new QTreeWidgetItem(tree, {row.first, value}); item->setData(1, Qt::UserRole, row.second);
    }
    if (view.selectFirst && tree->topLevelItemCount()) tree->setCurrentItem(tree->topLevelItem(0));
    tree->resizeColumnToContents(0); if (view.columns > 1) tree->resizeColumnToContents(1);
    auto copy = new QAction(this); copy->setObjectName("copyChecksumResults"); copy->setShortcuts({QKeySequence("Ctrl+C"), QKeySequence("Ctrl+Insert")}); copy->setShortcutContext(Qt::WidgetWithChildrenShortcut); tree->addAction(copy);
    connect(copy, &QAction::triggered, this, [this] { QApplication::clipboard()->setText(copyText()); });
    auto select = new QAction(this); select->setShortcut(QKeySequence("Ctrl+A")); select->setShortcutContext(Qt::WidgetWithChildrenShortcut); tree->addAction(select); connect(select, &QAction::triggered, tree, &QTreeWidget::selectAll);
    if (view.deleteAllowed) { auto remove = new QAction(this); remove->setObjectName("deleteChecksumRows"); remove->setShortcut(QKeySequence(Qt::Key_Delete)); remove->setShortcutContext(Qt::WidgetWithChildrenShortcut); tree->addAction(remove); connect(remove, &QAction::triggered, this, &ChecksumResultsDialog::deleteItems); }
    if (FileManagerSettings::load().singleClick) connect(tree, &QTreeWidget::itemClicked, this, [this] { if (history && !QApplication::keyboardModifiers().testFlag(Qt::AltModifier)) accept(); else itemInfo(); });
    else connect(tree, &QTreeWidget::itemActivated, this, [this] { if (history && !QApplication::keyboardModifiers().testFlag(Qt::AltModifier)) accept(); else itemInfo(); });
    auto ok = new QPushButton(this), cancel = new QPushButton(this); ok->setObjectName("checksumOk"); cancel->setObjectName("checksumCancel"); UiLanguage::bind(ok, 401); UiLanguage::bind(cancel, 402); connect(ok, &QPushButton::clicked, this, &QDialog::accept); connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    auto layout = new ResourceDialogLayout(this, 99); layout->bind("IDL_LISTVIEW", tree); layout->bind("IDOK", ok); layout->bind("IDCANCEL", cancel); layout->install(); UiLanguage::apply(this);
}
QString ChecksumResultsDialog::copyText() const {
    QVector<QPair<QString, QString>> rows; QVector<unsigned> selected;
    for (int i = 0; i < tree->topLevelItemCount(); ++i) { const auto item = tree->topLevelItem(i); rows.append({item->text(0), item->data(1, Qt::UserRole).toString()}); if (item->isSelected()) selected.append(unsigned(i)); }
    return officialChecksumCopy(rows, selected);
}
void ChecksumResultsDialog::deleteItems() {
    const int focused = tree->indexOfTopLevelItem(tree->currentItem());
    if (!tree->selectedItems().isEmpty()) changed = true;
    for (auto item : tree->selectedItems()) delete tree->takeTopLevelItem(tree->indexOfTopLevelItem(item));
    if (tree->topLevelItemCount()) tree->setCurrentItem(tree->topLevelItem(qBound(0, focused, tree->topLevelItemCount() - 1)));
    tree->resizeColumnToContents(0);
}
void ChecksumResultsDialog::itemInfo() {
    const auto selected = tree->selectedItems(); if (selected.size() != 1) return;
    QDialog detail(this); detail.setObjectName("checksumItemInfo"); detail.setWindowTitle(selected[0]->text(0)); auto text = new QPlainTextEdit(tree->columnCount() == 1 ? selected[0]->text(0) : selected[0]->data(1, Qt::UserRole).toString(), &detail); text->setObjectName("checksumItemText"); text->setReadOnly(true); text->setLineWrapMode(QPlainTextEdit::NoWrap);
    auto close = new QPushButton(&detail); close->setObjectName("checksumItemClose"); UiLanguage::bind(close, 408); connect(close, &QPushButton::clicked, &detail, &QDialog::accept);
    auto geometry = new ResourceDialogLayout(&detail, 94); geometry->bind("IDE_EDIT", text); geometry->bind("IDCLOSE", close); geometry->install(); UiLanguage::apply(&detail); detail.exec();
}
bool ChecksumResultsDialog::eventFilter(QObject *object, QEvent *event) {
    if (object == tree && event->type() == QEvent::KeyPress) { const auto key = static_cast<QKeyEvent *>(event); if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) { if (history && !key->modifiers().testFlag(Qt::AltModifier)) accept(); else itemInfo(); return true; } }
    return QDialog::eventFilter(object, event);
}

QStringList ChecksumResultsDialog::strings() const { QStringList result; for (int row = 0; row < tree->topLevelItemCount(); ++row) result << tree->topLevelItem(row)->text(0); return result; }
QString ChecksumResultsDialog::focusedString() const { return tree->currentItem() ? tree->currentItem()->text(0) : QString(); }
