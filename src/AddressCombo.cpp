// SPDX-License-Identifier: LGPL-3.0-or-later
// Qt adaptation of official PanelFolderChange.cpp::OnComboBoxCommand.
#include "AddressCombo.h"
#include "UiLanguage.h"
#include <QDir>
#include <QFileInfo>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStyledItemDelegate>
#include <QStyle>

namespace {
class AddressDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto adjusted = option; adjusted.rect.setLeft(adjusted.rect.left() + 10 * index.data(Qt::UserRole + 1).toInt());
        QStyledItemDelegate::paint(painter, adjusted, index);
    }
};
}
AddressCombo::AddressCombo(QWidget *parent) : QComboBox(parent) {
    setObjectName("pathCombo"); setEditable(true); setInsertPolicy(QComboBox::NoInsert);
    setMaxVisibleItems(25); setIconSize({16,16}); setItemDelegate(new AddressDelegate(this));
}
void AddressCombo::showPopup() {
    if (!isEnabled()) return;
    const auto path = currentLocation ? currentLocation() : currentText();
    QSignalBlocker blocked(this); clear();
    auto add = [this](const QString &label, const QString &destination, unsigned indent, QStyle::StandardPixmap icon) {
        addItem(style()->standardIcon(icon), label, destination); setItemData(count()-1, indent, Qt::UserRole+1);
    };
    // Original order: root, indented path components (including archive
    // virtual components), Documents, Computer, drives, Network. macOS
    // supplies root and mounted volumes instead of Win32 shell namespaces.
    QString sum = "/"; unsigned indent = 0;
    add(sum, sum, indent++, QStyle::SP_DriveHDIcon);
    const auto parts = path.split('/', Qt::SkipEmptyParts);
    for (const auto &name : parts) {
        sum += name; const bool archive = QFileInfo(sum).isFile();
        add(name, sum + '/', indent++, archive ? QStyle::SP_FileIcon : QStyle::SP_DirIcon); sum += '/';
    }
    const auto documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (!documents.isEmpty()) add(UiLanguage::resource(7102), documents, 0, QStyle::SP_DirIcon);
    add(UiLanguage::resource(7100), "/", 0, QStyle::SP_ComputerIcon);
    for (const auto &volume : QStorageInfo::mountedVolumes()) {
        if (!volume.isReady() || !volume.isValid()) continue;
        const auto root = volume.rootPath();
        if (root == "/" || root.startsWith("/Volumes/")) add(root, root, 1, QStyle::SP_DriveHDIcon);
    }
    if (QFileInfo("/Network").isDir()) add(UiLanguage::resource(7101), "/Network", 0, QStyle::SP_DirIcon);
    setCurrentIndex(-1); setEditText(path); blocked.unblock();
    QComboBox::showPopup();
}
void AddressCombo::hidePopup() {
    QComboBox::hidePopup();
    if (currentLocation) setEditText(currentLocation());
}
