// SPDX-License-Identifier: LGPL-3.0-or-later
#include "OverwriteDialog.h"
#include "UiLanguage.h"
#include <QFileInfo>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QString reduce(QString value) {
    if (value.size() > 72) value = value.left(36) + " ... " + value.right(36);
    if (value.endsWith(' ')) value = '"' + value + '"'; return value;
}
QString fileText(const OverwriteFileInfo &file) {
    const auto slash = file.path.lastIndexOf('/'); QString text = reduce(file.path.left(slash + 1)) + '\n' + reduce(file.path.mid(slash + 1)) + '\n';
    if (file.sizeDefined) {
        text += UiLanguage::text("{0} bytes").replace("{0}", QString::number(file.size));
        if (file.size >= 1024) {
            const int shift = file.size >= (quint64(10) << 30) ? 30 : file.size >= (quint64(10) << 20) ? 20 : 10;
            text += " : " + QString::number(file.size >> shift) + (shift == 30 ? " GiB" : shift == 20 ? " MiB" : " KiB");
        }
    }
    text += '\n'; if (file.modified.isValid()) text += file.modified.toLocalTime().toString("yyyy-MM-dd HH:mm:ss"); return text;
}
}
OverwriteDialog::OverwriteDialog(const OverwriteConflict &conflict, QWidget *parent, bool extraButtons, bool defaultNo) : QDialog(parent) {
    setObjectName("overwriteDialog"); setWindowTitle("Confirm File Replace"); setWindowModality(Qt::ApplicationModal); setMinimumWidth(520);
    auto layout = new QVBoxLayout(this); layout->setContentsMargins(12, 10, 12, 10); layout->setSpacing(10);
    layout->addWidget(new QLabel("Destination folder already contains processed file.", this)); layout->addWidget(new QLabel("Would you like to replace the existing file", this));
    auto information = [&](const OverwriteFileInfo &file, QString name) {
        auto row = new QHBoxLayout; auto icon = new QLabel(this); icon->setPixmap(style()->standardIcon(file.directory ? QStyle::SP_DirIcon : QStyle::SP_FileIcon).pixmap(32, 32)); icon->setAlignment(Qt::AlignTop); row->addWidget(icon);
        auto label = new QLabel(fileText(file), this); label->setObjectName(name); label->setTextFormat(Qt::PlainText); label->setProperty("uiLiteral", true); label->setTextInteractionFlags(Qt::TextSelectableByMouse); row->addWidget(label, 1); layout->addLayout(row);
    };
    information(conflict.existing, "overwriteExisting"); layout->addWidget(new QLabel("with this one?", this)); information(conflict.incoming, "overwriteIncoming");
    auto grid = new QGridLayout; grid->setHorizontalSpacing(6); grid->setVerticalSpacing(6); layout->addLayout(grid);
    struct Choice { const char *text, *name; OverwriteAnswer answer; int row, column; };
    for (const auto &choice : {Choice{"&Yes", "overwriteYes", OverwriteAnswer::Yes, 0, 0}, Choice{"Yes to &All", "overwriteYesAll", OverwriteAnswer::YesToAll, 0, 1}, Choice{"A&uto Rename", "overwriteRename", OverwriteAnswer::AutoRename, 0, 2}, Choice{"&No", "overwriteNo", OverwriteAnswer::No, 1, 0}, Choice{"No to A&ll", "overwriteNoAll", OverwriteAnswer::NoToAll, 1, 1}, Choice{"&Cancel", "overwriteCancel", OverwriteAnswer::Cancel, 1, 2}}) {
        if (!extraButtons && (choice.answer == OverwriteAnswer::YesToAll || choice.answer == OverwriteAnswer::NoToAll || choice.answer == OverwriteAnswer::AutoRename)) continue;
        auto button = new QPushButton(choice.text, this); button->setObjectName(choice.name); button->setMinimumWidth(choice.answer == OverwriteAnswer::AutoRename ? 135 : 110); grid->addWidget(button, choice.row, choice.column);
        if (choice.answer == (defaultNo ? OverwriteAnswer::No : OverwriteAnswer::Yes)) { button->setDefault(true); button->setFocus(); }
        connect(button, &QPushButton::clicked, this, [this, answer = choice.answer] { selected = answer; done(answer == OverwriteAnswer::Cancel ? QDialog::Rejected : QDialog::Accepted); });
    }
    UiLanguage::apply(this);
}
void OverwriteDialog::reject() { selected = OverwriteAnswer::Cancel; QDialog::reject(); }
